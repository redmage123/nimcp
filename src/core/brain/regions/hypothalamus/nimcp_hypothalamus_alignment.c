/**
 * @file nimcp_hypothalamus_alignment.c
 * @brief Alignment Introspection, Verification, and Audit Implementation
 *
 * WHAT: Implementation of alignment safety APIs
 * WHY:  Critical for AGI safety verification
 * HOW:  State introspection, audit logging, verification callbacks
 *
 * @version Phase 19: Alignment Hardening (Full Implementation)
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_alignment, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

#define ALIGNMENT_MAX_SYSTEMS       16
#define ALIGNMENT_AUDIT_CAPACITY    1024
#define ALIGNMENT_MAX_ALERTS        64
#define ALIGNMENT_MAX_CALLBACKS     HYPO_MAX_ALIGNMENT_CALLBACKS

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Alert entry
 */
typedef struct {
    uint32_t id;
    uint32_t severity;
    uint64_t timestamp_us;
    uint32_t reporter_id;
    bool acknowledged;
    uint32_t acknowledger_id;
    char message[NIMCP_ERROR_BUFFER_SIZE];
} alignment_alert_t;

/**
 * @brief Callback registration entry
 */
typedef struct {
    uint32_t id;
    bool active;
    enum {
        CB_TYPE_ALIGNMENT,
        CB_TYPE_ALERT,
        CB_TYPE_VERIFIER
    } type;
    union {
        hypo_alignment_callback_t alignment_cb;
        hypo_alert_callback_t alert_cb;
        hypo_integrity_verifier_t verifier;
    } callback;
    void* user_data;
    uint32_t min_severity;  /* For alert callbacks */
} callback_entry_t;

/**
 * @brief Per-system alignment state
 */
typedef struct {
    const hypo_drive_system_handle_t* system;
    bool in_use;

    /* Audit log (circular buffer) */
    hypo_audit_entry_t audit_log[ALIGNMENT_AUDIT_CAPACITY];
    size_t audit_count;
    size_t audit_head;      /* Next write position */
    size_t audit_tail;      /* Oldest entry position */
    bool audit_enabled;

    /* Callbacks */
    callback_entry_t callbacks[ALIGNMENT_MAX_CALLBACKS];
    size_t callback_count;
    uint32_t next_callback_id;

    /* Alerts */
    alignment_alert_t alerts[ALIGNMENT_MAX_ALERTS];
    size_t alert_count;
    uint32_t next_alert_id;
    uint32_t active_alert_count;

    /* Integrity tracking */
    uint32_t stored_checksum;
    uint64_t last_integrity_check_us;

    /* Statistics */
    uint64_t verifications_performed;
    uint64_t modifications_denied;
    uint64_t callbacks_invoked;
} alignment_state_t;

/* Global state storage for all systems */
static alignment_state_t g_alignment_states[ALIGNMENT_MAX_SYSTEMS];
static bool g_states_initialized = false;

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Initialize global state storage
 */
static void init_states_if_needed(void) {
    if (!g_states_initialized) {
        memset(g_alignment_states, 0, sizeof(g_alignment_states));
        g_states_initialized = true;
    }
}

/**
 * @brief Get or create state for a system
 */
static alignment_state_t* get_state(const hypo_drive_system_handle_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    init_states_if_needed();

    /* Look for existing state */
    for (int i = 0; i < ALIGNMENT_MAX_SYSTEMS; i++) {
        if (g_alignment_states[i].in_use &&
            g_alignment_states[i].system == system) {
            return &g_alignment_states[i];
        }
    }

    /* Allocate new state */
    for (int i = 0; i < ALIGNMENT_MAX_SYSTEMS; i++) {
        if (!g_alignment_states[i].in_use) {
            alignment_state_t* state = &g_alignment_states[i];
            memset(state, 0, sizeof(alignment_state_t));
            state->system = system;
            state->in_use = true;
            state->audit_enabled = false;  /* Off by default */
            state->next_callback_id = 1;
            state->next_alert_id = 1;
            return state;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_state: no available alignment state slots");
    return NULL;  /* No slots available */
}

/**
 * @brief Simple CRC32 for integrity checking
 */
static uint32_t compute_crc32(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

/**
 * @brief Add entry to circular audit log
 */
static void add_audit_entry(
    alignment_state_t* state,
    hypo_audit_event_t event,
    hypo_param_type_t param_type,
    uint32_t param_index,
    uint32_t accessor_id,
    float old_value,
    float new_value,
    hypo_alignment_status_t result)
{
    if (!state || !state->audit_enabled) {
        return;
    }

    hypo_audit_entry_t* entry = &state->audit_log[state->audit_head];

    entry->timestamp_us = nimcp_time_get_us();
    entry->event_type = event;
    entry->param_type = param_type;
    entry->param_index = param_index;
    entry->accessor_id = accessor_id;
    entry->old_value = old_value;
    entry->new_value = new_value;
    entry->result = result;
    entry->result_message = hypo_alignment_status_string(result);

    state->audit_head = (state->audit_head + 1) % ALIGNMENT_AUDIT_CAPACITY;

    if (state->audit_count < ALIGNMENT_AUDIT_CAPACITY) {
        state->audit_count++;
    } else {
        /* Buffer wrapped - move tail */
        state->audit_tail = (state->audit_tail + 1) % ALIGNMENT_AUDIT_CAPACITY;
    }
}

/**
 * @brief Invoke alignment callbacks
 */
static void invoke_alignment_callbacks(
    alignment_state_t* state,
    const hypo_alignment_snapshot_t* snapshot,
    hypo_audit_event_t event)
{
    if (!state) return;

    for (size_t i = 0; i < state->callback_count; i++) {
        if (state->callbacks[i].active &&
            state->callbacks[i].type == CB_TYPE_ALIGNMENT &&
            state->callbacks[i].callback.alignment_cb) {
            state->callbacks[i].callback.alignment_cb(
                snapshot,
                event,
                state->callbacks[i].user_data
            );
            state->callbacks_invoked++;
        }
    }
}

/**
 * @brief Invoke alert callbacks
 */
static void invoke_alert_callbacks(
    alignment_state_t* state,
    const hypo_verification_report_t* report,
    uint32_t severity)
{
    if (!state) return;

    for (size_t i = 0; i < state->callback_count; i++) {
        if (state->callbacks[i].active &&
            state->callbacks[i].type == CB_TYPE_ALERT &&
            severity >= state->callbacks[i].min_severity &&
            state->callbacks[i].callback.alert_cb) {
            state->callbacks[i].callback.alert_cb(
                report,
                severity,
                state->callbacks[i].user_data
            );
            state->callbacks_invoked++;
        }
    }
}

/**
 * @brief Invoke verifier callbacks
 */
static bool invoke_verifiers(
    alignment_state_t* state,
    const hypo_alignment_snapshot_t* snapshot)
{
    if (!state) return true;

    for (size_t i = 0; i < state->callback_count; i++) {
        if (state->callbacks[i].active &&
            state->callbacks[i].type == CB_TYPE_VERIFIER &&
            state->callbacks[i].callback.verifier) {
            if (!state->callbacks[i].callback.verifier(
                    snapshot,
                    state->callbacks[i].user_data)) {
                return false;  /* Verifier failed — legitimate verification failure, not an error */
            }
        }
    }
    return true;
}

/*=============================================================================
 * STRING CONVERSION FUNCTIONS
 *===========================================================================*/

/* Note: hypo_alignment_mode_string() and hypo_lock_state_string() are
 * defined in nimcp_hypothalamus_drives.c to avoid duplication */

const char* hypo_alignment_status_string(hypo_alignment_status_t status) {
    switch (status) {
        case HYPO_ALIGN_OK:
            return "OK - Alignment verified";
        case HYPO_ALIGN_WARN_DRIFT:
            return "WARNING - Alignment weights drifting";
        case HYPO_ALIGN_WARN_IMBALANCE:
            return "WARNING - Alignment weight imbalance";
        case HYPO_ALIGN_ERROR_UNLOCKED:
            return "ERROR - Critical parameters unlocked";
        case HYPO_ALIGN_ERROR_MODIFIED:
            return "ERROR - Unauthorized modification detected";
        case HYPO_ALIGN_ERROR_CORRUPTED:
            return "ERROR - Alignment state corrupted";
        case HYPO_ALIGN_ERROR_VIOLATED:
            return "ERROR - Alignment constraint violated";
        default:
            return "UNKNOWN";
    }
}

static const char* audit_event_string(hypo_audit_event_t event) {
    switch (event) {
        case HYPO_AUDIT_READ:
            return "READ";
        case HYPO_AUDIT_WRITE_SUCCESS:
            return "WRITE_SUCCESS";
        case HYPO_AUDIT_WRITE_DENIED:
            return "WRITE_DENIED";
        case HYPO_AUDIT_LOCK_CHANGED:
            return "LOCK_CHANGED";
        case HYPO_AUDIT_VERIFICATION:
            return "VERIFICATION";
        case HYPO_AUDIT_ALERT_TRIGGERED:
            return "ALERT_TRIGGERED";
        case HYPO_AUDIT_INTEGRITY_CHECK:
            return "INTEGRITY_CHECK";
        default:
            return "UNKNOWN";
    }
}

/*=============================================================================
 * INTROSPECTION API IMPLEMENTATION
 *===========================================================================*/

hypo_alignment_status_t hypo_alignment_get_snapshot(
    const hypo_drive_system_handle_t* system,
    hypo_alignment_snapshot_t* snapshot)
{
    if (!system || !snapshot) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    alignment_state_t* state = get_state(system);

    memset(snapshot, 0, sizeof(hypo_alignment_snapshot_t));

    /* Get drive system state */
    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(system, &drive_state)) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    /* Get setpoint configuration */
    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    /* Get alignment mode via public API */
    hypo_alignment_mode_t alignment_mode = hypo_drive_get_alignment_mode(system);

    /* Fill snapshot */
    snapshot->mode = alignment_mode;
    snapshot->mode_string = hypo_alignment_mode_string(alignment_mode);

    snapshot->setpoints_lock = setpoints.setpoints_lock;
    snapshot->alignment_lock = setpoints.alignment_lock;
    snapshot->all_critical_locked =
        (setpoints.setpoints_lock == HYPO_LOCK_HARD) &&
        (setpoints.alignment_lock == HYPO_LOCK_HARD);

    /* Alignment weights */
    snapshot->human_wellbeing_weight = setpoints.human_wellbeing_weight;
    snapshot->harm_avoidance_weight = setpoints.harm_avoidance_weight;
    snapshot->honesty_weight = setpoints.honesty_weight;
    snapshot->helpfulness_weight = setpoints.helpfulness_weight;

    /* Weight analysis */
    snapshot->weight_sum =
        setpoints.human_wellbeing_weight +
        setpoints.harm_avoidance_weight +
        setpoints.honesty_weight +
        setpoints.helpfulness_weight;

    float min_w = setpoints.human_wellbeing_weight;
    float max_w = setpoints.human_wellbeing_weight;
    if (setpoints.harm_avoidance_weight < min_w) min_w = setpoints.harm_avoidance_weight;
    if (setpoints.harm_avoidance_weight > max_w) max_w = setpoints.harm_avoidance_weight;
    if (setpoints.honesty_weight < min_w) min_w = setpoints.honesty_weight;
    if (setpoints.honesty_weight > max_w) max_w = setpoints.honesty_weight;
    if (setpoints.helpfulness_weight < min_w) min_w = setpoints.helpfulness_weight;
    if (setpoints.helpfulness_weight > max_w) max_w = setpoints.helpfulness_weight;

    snapshot->weight_balance = (max_w > 0.0f) ? (min_w / max_w) : 0.0f;

    snapshot->weights_valid =
        (snapshot->weight_sum > 0.5f) &&
        (snapshot->weight_sum < 4.0f) &&
        (snapshot->weight_balance >= HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD / HYPO_ALIGN_MAX_WEIGHT_RATIO);

    /* Reward configuration */
    snapshot->reward_gain = setpoints.reward_gain;
    snapshot->punishment_gain = setpoints.punishment_gain;
    snapshot->temporal_discount = setpoints.temporal_discount;

    /* Audit info */
    snapshot->modification_count = setpoints.modification_count;
    snapshot->last_modified_us = setpoints.last_modified_us;
    snapshot->last_modifier_id = setpoints.modifier_id;

    /* Compute checksum */
    snapshot->checksum = hypo_alignment_compute_checksum(system);
    snapshot->integrity_valid = hypo_alignment_verify_integrity(system);

    snapshot->snapshot_time_us = nimcp_time_get_us();

    /* Log the read operation */
    if (state) {
        add_audit_entry(state, HYPO_AUDIT_READ, HYPO_PARAM_ALIGNMENT_WEIGHT,
                        0, 0, 0.0f, 0.0f, HYPO_ALIGN_OK);
    }

    return HYPO_ALIGN_OK;
}

bool hypo_alignment_all_locked(const hypo_drive_system_handle_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_all_locked: system is NULL");
        return false;
    }

    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_all_locked: hypo_drive_get_setpoints is NULL");
        return false;
    }

    return (setpoints.setpoints_lock == HYPO_LOCK_HARD) &&
           (setpoints.alignment_lock == HYPO_LOCK_HARD);
}

bool hypo_alignment_get_weight(
    const hypo_drive_system_handle_t* system,
    const char* name,
    float* value)
{
    if (!system || !name || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_get_weight: required parameter is NULL (system, name, value)");
        return false;
    }

    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_get_weight: hypo_drive_get_setpoints is NULL");
        return false;
    }

    if (strcmp(name, "human_wellbeing") == 0) {
        *value = setpoints.human_wellbeing_weight;
    } else if (strcmp(name, "harm_avoidance") == 0) {
        *value = setpoints.harm_avoidance_weight;
    } else if (strcmp(name, "honesty") == 0) {
        *value = setpoints.honesty_weight;
    } else if (strcmp(name, "helpfulness") == 0) {
        *value = setpoints.helpfulness_weight;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_get_weight: validation failed");
        return false;
    }

    return true;
}

/*=============================================================================
 * VERIFICATION API IMPLEMENTATION
 *===========================================================================*/

/**
 * @brief Helper to add recommendation and update worst status
 */
static void add_verification_issue(
    hypo_verification_report_t* report,
    uint32_t* rec_count,
    hypo_alignment_status_t* worst_status,
    const char* recommendation,
    hypo_alignment_status_t issue_status)
{
    if (*rec_count < 8) {
        report->recommendations[(*rec_count)++] = recommendation;
    }
    if (*worst_status < issue_status) {
        *worst_status = issue_status;
    }
}

hypo_alignment_status_t hypo_alignment_verify(
    const hypo_drive_system_handle_t* system,
    hypo_verification_report_t* report)
{
    if (!system || !report) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    alignment_state_t* state = get_state(system);

    memset(report, 0, sizeof(hypo_verification_report_t));
    report->verification_time_us = nimcp_time_get_us();

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_status_t snap_status = hypo_alignment_get_snapshot(system, &snapshot);
    if (snap_status != HYPO_ALIGN_OK) {
        report->status = snap_status;
        report->status_message = "Failed to get alignment snapshot";
        return snap_status;
    }

    hypo_alignment_status_t worst_status = HYPO_ALIGN_OK;
    uint32_t rec_count = 0;

    /* Check lock states */
    report->setpoints_properly_locked = (snapshot.setpoints_lock >= HYPO_LOCK_SOFT);
    report->alignment_weights_locked = (snapshot.alignment_lock >= HYPO_LOCK_SOFT);

    if (!report->setpoints_properly_locked) {
        report->unlocked_critical_count++;
        add_verification_issue(report, &rec_count, &worst_status,
            "Lock setpoints to prevent modification", HYPO_ALIGN_ERROR_UNLOCKED);
    }

    if (!report->alignment_weights_locked) {
        report->unlocked_critical_count++;
        add_verification_issue(report, &rec_count, &worst_status,
            "Lock alignment weights for safety", HYPO_ALIGN_ERROR_UNLOCKED);
    }

    /* Check weight bounds */
    report->min_weight = snapshot.human_wellbeing_weight;
    report->max_weight = snapshot.human_wellbeing_weight;

    const float weights[] = {
        snapshot.harm_avoidance_weight,
        snapshot.honesty_weight,
        snapshot.helpfulness_weight
    };

    for (int i = 0; i < 3; i++) {
        if (weights[i] < report->min_weight) report->min_weight = weights[i];
        if (weights[i] > report->max_weight) report->max_weight = weights[i];
    }

    report->weights_in_range =
        (report->min_weight >= 0.0f) &&
        (report->max_weight <= 1.0f);

    if (!report->weights_in_range) {
        add_verification_issue(report, &rec_count, &worst_status,
            "Weights should be in [0, 1] range", HYPO_ALIGN_WARN_IMBALANCE);
    }

    /* Check weight balance */
    float ratio = (report->min_weight > 0.01f) ?
        (report->max_weight / report->min_weight) : 100.0f;
    report->weights_balanced = (ratio <= HYPO_ALIGN_MAX_WEIGHT_RATIO);

    if (!report->weights_balanced) {
        add_verification_issue(report, &rec_count, &worst_status,
            "Reduce weight imbalance for safety", HYPO_ALIGN_WARN_IMBALANCE);
    }

    /* Check specific weight minimums (Byrnes recommendations) */
    if (snapshot.human_wellbeing_weight < HYPO_ALIGN_MIN_WELLBEING_WEIGHT) {
        add_verification_issue(report, &rec_count, &worst_status,
            "human_wellbeing_weight below recommended minimum", HYPO_ALIGN_WARN_DRIFT);
    }

    if (snapshot.harm_avoidance_weight < HYPO_ALIGN_MIN_HARM_AVOIDANCE) {
        add_verification_issue(report, &rec_count, &worst_status,
            "harm_avoidance_weight below recommended minimum", HYPO_ALIGN_WARN_DRIFT);
    }

    /* Integrity check - also run custom verifiers */
    report->checksum_valid = snapshot.integrity_valid;

    if (state) {
        bool verifiers_passed = invoke_verifiers(state, &snapshot);
        if (!verifiers_passed) {
            report->checksum_valid = false;
            report->suspicious_events++;
        }
    }

    report->no_unauthorized_modifications = (snapshot.modification_count == 0) ||
        snapshot.all_critical_locked;

    if (!report->checksum_valid) {
        add_verification_issue(report, &rec_count, &worst_status,
            "Integrity check failed - investigate", HYPO_ALIGN_ERROR_CORRUPTED);
    }

    /* Compute alignment score */
    float score = 1.0f;
    if (!report->setpoints_properly_locked) score -= 0.2f;
    if (!report->alignment_weights_locked) score -= 0.2f;
    if (!report->weights_in_range) score -= 0.15f;
    if (!report->weights_balanced) score -= 0.15f;
    if (!report->checksum_valid) score -= 0.3f;
    if (score < 0.0f) score = 0.0f;
    report->alignment_score = score;

    report->recommendation_count = rec_count;
    report->status = worst_status;
    report->status_message = hypo_alignment_status_string(worst_status);
    report->verification_duration_us = nimcp_time_get_us() - report->verification_time_us;

    /* Log verification */
    if (state) {
        state->verifications_performed++;
        add_audit_entry(state, HYPO_AUDIT_VERIFICATION, HYPO_PARAM_ALIGNMENT_WEIGHT,
                        0, 0, score, 0.0f, worst_status);

        /* Invoke alignment callbacks to notify of verification */
        invoke_alignment_callbacks(state, &snapshot, HYPO_AUDIT_VERIFICATION);
    }

    return worst_status;
}

hypo_alignment_status_t hypo_alignment_health_check(
    const hypo_drive_system_handle_t* system,
    float* score)
{
    if (!system || !score) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    hypo_verification_report_t report;
    hypo_alignment_status_t status = hypo_alignment_verify(system, &report);
    *score = report.alignment_score;
    return status;
}

bool hypo_alignment_verify_weight_bounds(
    const hypo_drive_system_handle_t* system,
    float min_weight,
    float max_weight)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_verify_weight_bounds: system is NULL");
        return false;
    }

    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_verify_weight_bounds: hypo_drive_get_setpoints is NULL");
        return false;
    }

    const float weights[] = {
        setpoints.human_wellbeing_weight,
        setpoints.harm_avoidance_weight,
        setpoints.honesty_weight,
        setpoints.helpfulness_weight
    };

    for (int i = 0; i < 4; i++) {
        if (weights[i] < min_weight || weights[i] > max_weight) {
            return false;
        }
    }

    return true;
}

uint32_t hypo_alignment_compute_checksum(
    const hypo_drive_system_handle_t* system)
{
    if (!system) return 0;

    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        return 0;
    }

    /* Compute checksum over critical alignment data */
    uint8_t buffer[64];
    size_t offset = 0;

    memcpy(buffer + offset, &setpoints.human_wellbeing_weight, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &setpoints.harm_avoidance_weight, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &setpoints.honesty_weight, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &setpoints.helpfulness_weight, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &setpoints.setpoints_lock, sizeof(hypo_lock_state_t));
    offset += sizeof(hypo_lock_state_t);
    memcpy(buffer + offset, &setpoints.alignment_lock, sizeof(hypo_lock_state_t));
    offset += sizeof(hypo_lock_state_t);

    return compute_crc32(buffer, offset);
}

bool hypo_alignment_verify_integrity(
    const hypo_drive_system_handle_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_verify_integrity: system is NULL");
        return false;
    }

    alignment_state_t* state = get_state(system);
    uint32_t checksum = hypo_alignment_compute_checksum(system);

    if (checksum == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_verify_integrity: checksum is zero");
        return false;
    }

    /* If we have stored checksum, verify against it */
    if (state && state->stored_checksum != 0) {
        bool valid = (checksum == state->stored_checksum);
        state->last_integrity_check_us = nimcp_time_get_us();

        if (state->audit_enabled) {
            add_audit_entry(state, HYPO_AUDIT_INTEGRITY_CHECK, HYPO_PARAM_NONE,
                            0, 0, (float)state->stored_checksum, (float)checksum,
                            valid ? HYPO_ALIGN_OK : HYPO_ALIGN_ERROR_CORRUPTED);
        }
        return valid;
    }

    /* No stored checksum yet - store current and return valid */
    if (state) {
        state->stored_checksum = checksum;
        state->last_integrity_check_us = nimcp_time_get_us();
    }

    return true;
}

/*=============================================================================
 * AUDIT LOGGING API IMPLEMENTATION
 *===========================================================================*/

bool hypo_alignment_set_audit_enabled(
    hypo_drive_system_handle_t* system,
    bool enable)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_set_audit_enabled: system is NULL");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_set_audit_enabled: state is NULL");
        return false;
    }

    state->audit_enabled = enable;
    return true;
}

size_t hypo_alignment_get_audit_count(
    const hypo_drive_system_handle_t* system)
{
    if (!system) return 0;

    alignment_state_t* state = get_state(system);
    if (!state) return 0;

    return state->audit_count;
}

bool hypo_alignment_get_audit_entry(
    const hypo_drive_system_handle_t* system,
    size_t index,
    hypo_audit_entry_t* entry)
{
    if (!system || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_get_audit_entry: required parameter is NULL (system, entry)");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state || index >= state->audit_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_alignment_get_audit_entry: state is NULL");
        return false;
    }

    /* Calculate actual index in circular buffer */
    size_t actual_index = (state->audit_tail + index) % ALIGNMENT_AUDIT_CAPACITY;
    *entry = state->audit_log[actual_index];

    return true;
}

bool hypo_alignment_get_recent_audits(
    const hypo_drive_system_handle_t* system,
    hypo_audit_entry_t* entries,
    size_t max_entries,
    size_t* count)
{
    if (!system || !entries || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_get_recent_audits: required parameter is NULL (system, entries, count)");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state) {
        *count = 0;
        return true;
    }

    size_t to_copy = (max_entries < state->audit_count) ? max_entries : state->audit_count;

    /* Copy most recent entries (from head going backwards) */
    for (size_t i = 0; i < to_copy; i++) {
        size_t src_index;
        if (state->audit_head >= i + 1) {
            src_index = state->audit_head - i - 1;
        } else {
            src_index = ALIGNMENT_AUDIT_CAPACITY - (i + 1 - state->audit_head);
        }
        entries[i] = state->audit_log[src_index];
    }

    *count = to_copy;
    return true;
}

bool hypo_alignment_clear_audit_log(
    hypo_drive_system_handle_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_clear_audit_log: system is NULL");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_clear_audit_log: state is NULL");
        return false;
    }

    state->audit_count = 0;
    state->audit_head = 0;
    state->audit_tail = 0;

    return true;
}

bool hypo_alignment_export_audit_log(
    const hypo_drive_system_handle_t* system,
    const char* filepath)
{
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_export_audit_log: required parameter is NULL (system, filepath)");
        return false;
    }

    alignment_state_t* state = get_state(system);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_export_audit_log: f is NULL");
        return false;
    }

    fprintf(f, "# Hypothalamus Alignment Audit Log\n");
    fprintf(f, "# Exported: %lu us\n", (unsigned long)nimcp_time_get_us());
    fprintf(f, "# Format: timestamp_us,event,param_type,param_index,accessor,old,new,result\n");
    fprintf(f, "# Entries: %zu\n", state ? state->audit_count : 0);
    fprintf(f, "\n");

    if (state && state->audit_count > 0) {
        for (size_t i = 0; i < state->audit_count; i++) {
            size_t idx = (state->audit_tail + i) % ALIGNMENT_AUDIT_CAPACITY;
            hypo_audit_entry_t* e = &state->audit_log[idx];

            fprintf(f, "%lu,%s,%d,%u,%u,%.6f,%.6f,%s\n",
                    (unsigned long)e->timestamp_us,
                    audit_event_string(e->event_type),
                    (int)e->param_type,
                    e->param_index,
                    e->accessor_id,
                    e->old_value,
                    e->new_value,
                    hypo_alignment_status_string(e->result));
        }
    }

    fclose(f);
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION API IMPLEMENTATION
 *===========================================================================*/

uint32_t hypo_alignment_register_callback(
    hypo_drive_system_handle_t* system,
    hypo_alignment_callback_t callback,
    void* user_data)
{
    if (!system || !callback) return 0;

    alignment_state_t* state = get_state(system);
    if (!state || state->callback_count >= ALIGNMENT_MAX_CALLBACKS) return 0;

    callback_entry_t* entry = &state->callbacks[state->callback_count++];
    entry->id = state->next_callback_id++;
    entry->active = true;
    entry->type = CB_TYPE_ALIGNMENT;
    entry->callback.alignment_cb = callback;
    entry->user_data = user_data;
    entry->min_severity = 0;

    return entry->id;
}

uint32_t hypo_alignment_register_alert_callback(
    hypo_drive_system_handle_t* system,
    hypo_alert_callback_t callback,
    uint32_t min_severity,
    void* user_data)
{
    if (!system || !callback) return 0;

    alignment_state_t* state = get_state(system);
    if (!state || state->callback_count >= ALIGNMENT_MAX_CALLBACKS) return 0;

    callback_entry_t* entry = &state->callbacks[state->callback_count++];
    entry->id = state->next_callback_id++;
    entry->active = true;
    entry->type = CB_TYPE_ALERT;
    entry->callback.alert_cb = callback;
    entry->user_data = user_data;
    entry->min_severity = min_severity;

    return entry->id;
}

uint32_t hypo_alignment_register_verifier(
    hypo_drive_system_handle_t* system,
    hypo_integrity_verifier_t verifier,
    void* user_data)
{
    if (!system || !verifier) return 0;

    alignment_state_t* state = get_state(system);
    if (!state || state->callback_count >= ALIGNMENT_MAX_CALLBACKS) return 0;

    callback_entry_t* entry = &state->callbacks[state->callback_count++];
    entry->id = state->next_callback_id++;
    entry->active = true;
    entry->type = CB_TYPE_VERIFIER;
    entry->callback.verifier = verifier;
    entry->user_data = user_data;
    entry->min_severity = 0;

    return entry->id;
}

bool hypo_alignment_unregister_callback(
    hypo_drive_system_handle_t* system,
    uint32_t callback_id)
{
    if (!system || callback_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_unregister_callback: system is NULL");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_unregister_callback: state is NULL");
        return false;
    }

    for (size_t i = 0; i < state->callback_count; i++) {
        if (state->callbacks[i].id == callback_id && state->callbacks[i].active) {
            /* Mark as inactive and compact */
            state->callbacks[i].active = false;

            /* Shift remaining entries */
            for (size_t j = i; j < state->callback_count - 1; j++) {
                state->callbacks[j] = state->callbacks[j + 1];
            }
            state->callback_count--;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_unregister_callback: operation failed");
    return false;
}

/*=============================================================================
 * SETPOINT ACCESS CONTROL IMPLEMENTATION
 *===========================================================================*/

hypo_alignment_status_t hypo_alignment_request_modification(
    hypo_drive_system_handle_t* system,
    hypo_param_type_t param_type,
    uint32_t param_index,
    float new_value,
    uint32_t modifier_id,
    const char* reason)
{
    if (!system) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    alignment_state_t* state = get_state(system);

    /* Check lock state */
    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    hypo_lock_state_t lock = HYPO_LOCK_UNLOCKED;
    float old_value = 0.0f;

    if (param_type == HYPO_PARAM_SETPOINT) {
        lock = setpoints.setpoints_lock;
    } else if (param_type == HYPO_PARAM_ALIGNMENT_WEIGHT) {
        lock = setpoints.alignment_lock;
        /* Get current value for audit */
        switch (param_index) {
            case 0: old_value = setpoints.human_wellbeing_weight; break;
            case 1: old_value = setpoints.harm_avoidance_weight; break;
            case 2: old_value = setpoints.honesty_weight; break;
            case 3: old_value = setpoints.helpfulness_weight; break;
            default: old_value = 0.0f; break;
        }
    }

    if (lock == HYPO_LOCK_HARD) {
        /* Hard locked - deny modification */
        if (state) {
            state->modifications_denied++;
            add_audit_entry(state, HYPO_AUDIT_WRITE_DENIED, param_type,
                            param_index, modifier_id, old_value, new_value,
                            HYPO_ALIGN_ERROR_MODIFIED);
        }
        return HYPO_ALIGN_ERROR_MODIFIED;
    }

    if (lock == HYPO_LOCK_SOFT) {
        /* Soft locked - would require explicit unlock first */
        if (state) {
            state->modifications_denied++;
            add_audit_entry(state, HYPO_AUDIT_WRITE_DENIED, param_type,
                            param_index, modifier_id, old_value, new_value,
                            HYPO_ALIGN_ERROR_UNLOCKED);
        }
        return HYPO_ALIGN_ERROR_UNLOCKED;
    }

    (void)reason;

    /* Log successful modification */
    if (state) {
        add_audit_entry(state, HYPO_AUDIT_WRITE_SUCCESS, param_type,
                        param_index, modifier_id, old_value, new_value,
                        HYPO_ALIGN_OK);

        /* Update stored checksum */
        state->stored_checksum = hypo_alignment_compute_checksum(system);

        /* Invoke alignment callbacks to notify of state change */
        hypo_alignment_snapshot_t snapshot;
        if (hypo_alignment_get_snapshot(system, &snapshot) == HYPO_ALIGN_OK) {
            invoke_alignment_callbacks(state, &snapshot, HYPO_AUDIT_WRITE_SUCCESS);
        }
    }

    return HYPO_ALIGN_OK;
}

hypo_alignment_status_t hypo_alignment_request_lock_change(
    hypo_drive_system_handle_t* system,
    hypo_param_type_t param_type,
    hypo_lock_state_t new_lock,
    uint32_t modifier_id,
    uint64_t authorization)
{
    if (!system) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    alignment_state_t* state = get_state(system);

    /* Get current lock state */
    hypo_setpoint_config_t setpoints;
    if (!hypo_drive_get_setpoints(system, &setpoints)) {
        return HYPO_ALIGN_ERROR_CORRUPTED;
    }

    hypo_lock_state_t current_lock = HYPO_LOCK_UNLOCKED;
    if (param_type == HYPO_PARAM_SETPOINT) {
        current_lock = setpoints.setpoints_lock;
    } else if (param_type == HYPO_PARAM_ALIGNMENT_WEIGHT) {
        current_lock = setpoints.alignment_lock;
    }

    /* Cannot unlock from HARD lock */
    if (current_lock == HYPO_LOCK_HARD && new_lock != HYPO_LOCK_HARD) {
        if (state) {
            add_audit_entry(state, HYPO_AUDIT_LOCK_CHANGED, param_type,
                            0, modifier_id, (float)current_lock, (float)new_lock,
                            HYPO_ALIGN_ERROR_MODIFIED);
        }
        return HYPO_ALIGN_ERROR_MODIFIED;
    }

    /* Verify authorization for unlock operations */
    if (new_lock < current_lock) {
        /* Unlocking requires authorization */
        if (authorization == 0) {
            if (state) {
                add_audit_entry(state, HYPO_AUDIT_LOCK_CHANGED, param_type,
                                0, modifier_id, (float)current_lock, (float)new_lock,
                                HYPO_ALIGN_ERROR_UNLOCKED);
            }
            return HYPO_ALIGN_ERROR_UNLOCKED;
        }
        /* Would verify authorization token in production */
    }

    /* Log lock change */
    if (state) {
        add_audit_entry(state, HYPO_AUDIT_LOCK_CHANGED, param_type,
                        0, modifier_id, (float)current_lock, (float)new_lock,
                        HYPO_ALIGN_OK);

        /* Invoke alignment callbacks to notify of lock state change */
        hypo_alignment_snapshot_t snapshot;
        if (hypo_alignment_get_snapshot(system, &snapshot) == HYPO_ALIGN_OK) {
            invoke_alignment_callbacks(state, &snapshot, HYPO_AUDIT_LOCK_CHANGED);
        }
    }

    return HYPO_ALIGN_OK;
}

/*=============================================================================
 * ALERT MANAGEMENT IMPLEMENTATION
 *===========================================================================*/

void hypo_alignment_trigger_alert(
    hypo_drive_system_handle_t* system,
    uint32_t severity,
    const char* message,
    uint32_t reporter_id)
{
    if (!system) return;

    alignment_state_t* state = get_state(system);
    if (!state) return;

    /* Add alert to list */
    if (state->alert_count < ALIGNMENT_MAX_ALERTS) {
        alignment_alert_t* alert = &state->alerts[state->alert_count++];
        alert->id = state->next_alert_id++;
        alert->severity = severity;
        alert->timestamp_us = nimcp_time_get_us();
        alert->reporter_id = reporter_id;
        alert->acknowledged = false;
        alert->acknowledger_id = 0;

        if (message) {
            strncpy(alert->message, message, sizeof(alert->message) - 1);
            alert->message[sizeof(alert->message) - 1] = '\0';
        } else {
            alert->message[0] = '\0';
        }

        state->active_alert_count++;
    }

    /* Log alert */
    add_audit_entry(state, HYPO_AUDIT_ALERT_TRIGGERED, HYPO_PARAM_NONE,
                    0, reporter_id, (float)severity, 0.0f, HYPO_ALIGN_OK);

    /* Create verification report for callbacks */
    hypo_verification_report_t report;
    memset(&report, 0, sizeof(report));
    report.status = HYPO_ALIGN_ERROR_VIOLATED;
    report.status_message = message;
    report.verification_time_us = nimcp_time_get_us();

    /* Invoke alert callbacks */
    invoke_alert_callbacks(state, &report, severity);
}

uint32_t hypo_alignment_get_alert_count(
    const hypo_drive_system_handle_t* system)
{
    if (!system) return 0;

    alignment_state_t* state = get_state(system);
    if (!state) return 0;

    return state->active_alert_count;
}

bool hypo_alignment_acknowledge_alert(
    hypo_drive_system_handle_t* system,
    uint32_t alert_id,
    uint32_t acknowledger_id)
{
    if (!system || alert_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_acknowledge_alert: system is NULL");
        return false;
    }

    alignment_state_t* state = get_state(system);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_acknowledge_alert: state is NULL");
        return false;
    }

    for (size_t i = 0; i < state->alert_count; i++) {
        if (state->alerts[i].id == alert_id && !state->alerts[i].acknowledged) {
            state->alerts[i].acknowledged = true;
            state->alerts[i].acknowledger_id = acknowledger_id;
            if (state->active_alert_count > 0) {
                state->active_alert_count--;
            }
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_alignment_acknowledge_alert: validation failed");
    return false;
}

/*=============================================================================
 * STATE MANAGEMENT
 *===========================================================================*/

void hypo_alignment_release_state(hypo_drive_system_handle_t* system) {
    if (!system) return;

    init_states_if_needed();

    for (int i = 0; i < ALIGNMENT_MAX_SYSTEMS; i++) {
        if (g_alignment_states[i].in_use &&
            g_alignment_states[i].system == system) {
            memset(&g_alignment_states[i], 0, sizeof(alignment_state_t));
            return;
        }
    }
}

void hypo_alignment_reset_all_state(void) {
    memset(g_alignment_states, 0, sizeof(g_alignment_states));
    g_states_initialized = false;
}
