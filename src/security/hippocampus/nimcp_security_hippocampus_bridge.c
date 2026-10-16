/**
 * @file nimcp_security_hippocampus_bridge.c
 * @brief Security - Hippocampus Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implementation of security-hippocampus bridge for memory protection
 * WHY:  Protect hippocampal consolidation from attacks during sleep
 * HOW:  Sleep protection, consolidation verification, injection detection
 */

#include "security/hippocampus/nimcp_security_hippocampus_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_hippocampus_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Maximum registered encodings for replay validation */
#define MAX_ENCODINGS 1024

/** @brief Maximum registered patterns for injection detection */
#define MAX_PATTERNS 512

/** @brief Maximum place cell history */
#define MAX_PLACE_CELL_HISTORY 256

/** @brief Maximum time cell history */
#define MAX_TIME_CELL_HISTORY 256

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Registered encoding for replay validation
 */
typedef struct {
    uint64_t encoding_id;
    uint64_t content_hash;
    uint64_t timestamp;
    bool valid;
} registered_encoding_t;

/**
 * @brief Registered pattern for injection detection
 */
typedef struct {
    uint64_t pattern_id;
    uint64_t pattern_hash;
    bool valid;
} registered_pattern_t;

/**
 * @brief Place cell activity record
 */
typedef struct {
    uint32_t cell_id;
    float position_x;
    float position_y;
    float firing_rate;
    uint64_t timestamp;
} place_cell_record_t;

/**
 * @brief Time cell activity record
 */
typedef struct {
    uint32_t cell_id;
    uint64_t timestamp;
    float firing_rate;
} time_cell_record_t;

/**
 * @brief Internal bridge state
 */
typedef struct {
    /* Registered encodings */
    registered_encoding_t encodings[MAX_ENCODINGS];
    uint32_t num_encodings;

    /* Registered patterns */
    registered_pattern_t patterns[MAX_PATTERNS];
    uint32_t num_patterns;

    /* Place cell history */
    place_cell_record_t place_cells[MAX_PLACE_CELL_HISTORY];
    uint32_t place_cell_head;
    uint32_t place_cell_count;

    /* Time cell history */
    time_cell_record_t time_cells[MAX_TIME_CELL_HISTORY];
    uint32_t time_cell_head;
    uint32_t time_cell_count;

    /* Sleep protection state */
    bool protection_active;
    uint64_t protection_start_time;

    /* Consolidation pause state */
    bool consolidation_paused;
} sec_hippo_internal_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    return nimcp_time_monotonic_us();
}

/**
 * @brief Add audit entry
 */
static void add_audit_entry(
    sec_hippo_bridge_t* bridge,
    sec_hippo_audit_type_t type,
    bool success,
    float severity,
    const char* details
)
{
    if (!bridge || !bridge->audit_log || !bridge->config.enable_audit) {
        return;
    }

    sec_hippo_audit_entry_t* entry = &bridge->audit_log[bridge->audit_log_head];
    entry->timestamp = get_timestamp_us();
    entry->type = type;
    entry->sleep_phase = bridge->hippo_effects.current_sleep_phase;
    entry->success = success;
    entry->severity = severity;

    if (details) {
        strncpy(entry->details, details, sizeof(entry->details) - 1);
        entry->details[sizeof(entry->details) - 1] = '\0';
    } else {
        entry->details[0] = '\0';
    }

    bridge->audit_log_head = (bridge->audit_log_head + 1) % SEC_HIPPO_MAX_AUDIT_ENTRIES;
    if (bridge->audit_log_count < SEC_HIPPO_MAX_AUDIT_ENTRIES) {
        bridge->audit_log_count++;
    }

    bridge->stats.audit_entries++;
    if (!success || severity > 0.5f) {
        bridge->stats.audit_alerts++;
    }
}

/**
 * @brief Find encoding by ID
 */
static registered_encoding_t* find_encoding(
    sec_hippo_internal_t* internal,
    uint64_t encoding_id
)
{
    if (!internal) {
        return NULL;
    }

    for (uint32_t i = 0; i < internal->num_encodings; i++) {
        if (internal->encodings[i].valid &&
            internal->encodings[i].encoding_id == encoding_id) {
            return &internal->encodings[i];
        }
    }
    return NULL;
}

/**
 * @brief Compute spatial coherence from place cell history
 */
static float compute_spatial_coherence(sec_hippo_internal_t* internal)
{
    if (!internal || internal->place_cell_count < 2) {
        return 1.0f;  /* Not enough data - assume coherent */
    }

    /* Simple coherence: check for consistent place cell firing */
    float total_consistency = 0.0f;
    uint32_t pairs = 0;

    for (uint32_t i = 1; i < internal->place_cell_count && i < MAX_PLACE_CELL_HISTORY; i++) {
        uint32_t curr_idx = (internal->place_cell_head + MAX_PLACE_CELL_HISTORY - i) %
                           MAX_PLACE_CELL_HISTORY;
        uint32_t prev_idx = (internal->place_cell_head + MAX_PLACE_CELL_HISTORY - i - 1) %
                           MAX_PLACE_CELL_HISTORY;

        float dx = internal->place_cells[curr_idx].position_x -
                   internal->place_cells[prev_idx].position_x;
        float dy = internal->place_cells[curr_idx].position_y -
                   internal->place_cells[prev_idx].position_y;
        float dist = sqrtf(dx * dx + dy * dy);

        /* Consistent if distance is small (< 0.5 normalized) */
        float consistency = 1.0f - fminf(dist, 1.0f);
        total_consistency += consistency;
        pairs++;
    }

    return (pairs > 0) ? (total_consistency / pairs) : 1.0f;
}

/**
 * @brief Compute temporal coherence from time cell history
 */
static float compute_temporal_coherence(sec_hippo_internal_t* internal)
{
    if (!internal || internal->time_cell_count < 2) {
        return 1.0f;  /* Not enough data - assume coherent */
    }

    /* Check for monotonic time progression */
    float total_coherence = 0.0f;
    uint32_t pairs = 0;

    for (uint32_t i = 1; i < internal->time_cell_count && i < MAX_TIME_CELL_HISTORY; i++) {
        uint32_t curr_idx = (internal->time_cell_head + MAX_TIME_CELL_HISTORY - i) %
                           MAX_TIME_CELL_HISTORY;
        uint32_t prev_idx = (internal->time_cell_head + MAX_TIME_CELL_HISTORY - i - 1) %
                           MAX_TIME_CELL_HISTORY;

        /* Time should progress forward */
        int64_t dt = (int64_t)internal->time_cells[curr_idx].timestamp -
                     (int64_t)internal->time_cells[prev_idx].timestamp;

        /* Coherent if time progresses forward reasonably */
        float coherence = (dt >= 0 && dt < 10000000) ? 1.0f : 0.0f;
        total_coherence += coherence;
        pairs++;
    }

    return (pairs > 0) ? (total_coherence / pairs) : 1.0f;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int security_hippocampus_default_config(sec_hippo_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(sec_hippo_config_t));

    /* Feature enable flags */
    config->enable_sleep_protection = true;
    config->enable_consolidation_verify = true;
    config->enable_injection_detection = true;
    config->enable_replay_validation = true;
    config->enable_coherence_checking = true;
    config->enable_audit = true;

    /* Sleep protection settings */
    config->protect_nrem_deep = true;
    config->protect_rem = true;
    config->ripple_filter_threshold = 0.7f;
    config->spindle_gate_threshold = 0.8f;

    /* Consolidation settings */
    config->consolidation_min_strength = 0.3f;
    config->consolidation_check_interval_ms = 1000;
    config->degradation_threshold = 0.2f;

    /* Injection detection settings */
    config->injection_sensitivity = 1.0f;
    config->pattern_match_window = 50;
    config->false_positive_tolerance = 0.05f;

    /* Replay validation settings */
    config->replay_timing_tolerance_ms = 100.0f;
    config->replay_content_match_threshold = 0.8f;
    config->replay_sequence_max_gap = 5;

    /* Coherence settings */
    config->spatial_coherence_threshold = 0.7f;
    config->temporal_coherence_threshold = 0.7f;
    config->context_coherence_threshold = 0.6f;

    /* Sensitivity parameters */
    config->security_sensitivity = 1.0f;
    config->hippocampus_sensitivity = 1.0f;

    /* Bio-async integration */
    config->enable_bio_async = true;

    return 0;
}

sec_hippo_bridge_t* security_hippocampus_bridge_create(const sec_hippo_config_t* config)
{
    /* Allocate bridge */
    sec_hippo_bridge_t* bridge = nimcp_malloc(sizeof(sec_hippo_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_hippocampus_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate security-hippocampus bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(sec_hippo_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_HIPPOCAMPUS,
                         "security_hippocampus_bridge") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_hippocampus_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_hippocampus_default_config(&bridge->config);
    }

    /* Allocate replay sequences */
    bridge->replay_sequences = nimcp_malloc(
        SEC_HIPPO_MAX_REPLAY_SEQUENCES * sizeof(sec_hippo_replay_sequence_t));
    if (!bridge->replay_sequences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_hippocampus_bridge_create: failed to allocate replay_sequences");
        NIMCP_LOGGING_ERROR("Failed to allocate replay sequences");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->replay_sequences, 0,
           SEC_HIPPO_MAX_REPLAY_SEQUENCES * sizeof(sec_hippo_replay_sequence_t));

    /* Allocate consolidation events */
    bridge->consolidation_events = nimcp_malloc(
        SEC_HIPPO_MAX_CONSOLIDATION_EVENTS * sizeof(sec_hippo_consolidation_event_t));
    if (!bridge->consolidation_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_hippocampus_bridge_create: failed to allocate consolidation_events");
        NIMCP_LOGGING_ERROR("Failed to allocate consolidation events");
        nimcp_free(bridge->replay_sequences);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->consolidation_events, 0,
           SEC_HIPPO_MAX_CONSOLIDATION_EVENTS * sizeof(sec_hippo_consolidation_event_t));

    /* Allocate audit log */
    if (bridge->config.enable_audit) {
        bridge->audit_log = nimcp_malloc(
            SEC_HIPPO_MAX_AUDIT_ENTRIES * sizeof(sec_hippo_audit_entry_t));
        if (!bridge->audit_log) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_hippocampus_bridge_create: failed to allocate audit_log");
            NIMCP_LOGGING_ERROR("Failed to allocate audit log");
            nimcp_free(bridge->consolidation_events);
            nimcp_free(bridge->replay_sequences);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            return NULL;
        }
        memset(bridge->audit_log, 0,
               SEC_HIPPO_MAX_AUDIT_ENTRIES * sizeof(sec_hippo_audit_entry_t));
    }

    /* Allocate internal state */
    sec_hippo_internal_t* internal = nimcp_malloc(sizeof(sec_hippo_internal_t));
    if (!internal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_hippocampus_bridge_create: failed to allocate internal state");
        NIMCP_LOGGING_ERROR("Failed to allocate internal state");
        if (bridge->audit_log) nimcp_free(bridge->audit_log);
        nimcp_free(bridge->consolidation_events);
        nimcp_free(bridge->replay_sequences);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(internal, 0, sizeof(sec_hippo_internal_t));
    bridge->base.system_b = internal;

    /* Initialize state */
    bridge->state.state = SEC_HIPPO_STATE_IDLE;
    bridge->hippo_effects.current_sleep_phase = SEC_HIPPO_SLEEP_AWAKE;

    NIMCP_LOGGING_INFO("Created security-hippocampus bridge");
    return bridge;
}

void security_hippocampus_bridge_destroy(sec_hippo_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_hippocampus_disconnect_bio_async(bridge);
    }

    /* Free internal state */
    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (internal) {
        nimcp_free(internal);
    }

    /* Free audit log */
    if (bridge->audit_log) {
        nimcp_free(bridge->audit_log);
    }

    /* Free consolidation events */
    if (bridge->consolidation_events) {
        nimcp_free(bridge->consolidation_events);
    }

    /* Free replay sequences */
    if (bridge->replay_sequences) {
        nimcp_free(bridge->replay_sequences);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed security-hippocampus bridge");
}

int security_hippocampus_bridge_reset(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_hippo_stats_t));

    /* Reset effects */
    memset(&bridge->security_effects, 0, sizeof(security_to_hippo_effects_t));
    memset(&bridge->hippo_effects, 0, sizeof(hippo_to_security_effects_t));

    /* Reset state */
    bridge->state.state = SEC_HIPPO_STATE_IDLE;
    bridge->state.last_sleep_check = 0;
    bridge->state.last_consol_verify = 0;
    bridge->state.last_inject_scan = 0;
    bridge->state.last_replay_validate = 0;
    bridge->state.last_coherence_check = 0;
    bridge->state.active_protections = 0;

    /* Reset counters */
    bridge->num_replay_sequences = 0;
    bridge->num_consolidation_events = 0;
    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    /* Reset internal state */
    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (internal) {
        memset(internal, 0, sizeof(sec_hippo_internal_t));
    }

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Reset security-hippocampus bridge");
    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int security_hippocampus_connect_hippo(
    sec_hippo_bridge_t* bridge,
    hippocampus_system_t hippocampus
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(hippocampus, NIMCP_ERROR_NULL_POINTER, "hippocampus is NULL");

    BRIDGE_LOCK(bridge);
    bridge->hippocampus = hippocampus;
    bridge->hippocampus_connected = true;
    bridge->state.hippocampus_connected = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected hippocampus to security bridge");
    return 0;
}

int security_hippocampus_connect_sleep(
    sec_hippo_bridge_t* bridge,
    sleep_system_t sleep_system
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(sleep_system, NIMCP_ERROR_NULL_POINTER, "sleep_system is NULL");

    BRIDGE_LOCK(bridge);
    bridge->sleep_system = sleep_system;
    bridge->sleep_connected = true;
    bridge->state.sleep_system_connected = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected sleep system to security bridge");
    return 0;
}

int security_hippocampus_disconnect_all(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->hippocampus = NULL;
    bridge->hippocampus_connected = false;
    bridge->sleep_system = NULL;
    bridge->sleep_connected = false;
    bridge->state.hippocampus_connected = false;
    bridge->state.sleep_system_connected = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Disconnected all systems from security-hippocampus bridge");
    return 0;
}

bool security_hippocampus_is_fully_connected(const sec_hippo_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->hippocampus_connected && bridge->sleep_connected;
}

/* ============================================================================
 * Sleep Protection Functions
 * ============================================================================ */

int security_hippocampus_protect_sleep(
    sec_hippo_bridge_t* bridge,
    sec_hippo_sleep_phase_t phase
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_HIPPO_STATE_PROTECTING;
    bridge->hippo_effects.current_sleep_phase = phase;

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;

    /* Determine protection level based on phase */
    float protection_level = 0.0f;
    bool should_protect = false;

    switch (phase) {
        case SEC_HIPPO_SLEEP_AWAKE:
            protection_level = 0.1f;
            should_protect = false;
            break;
        case SEC_HIPPO_SLEEP_DROWSY:
            protection_level = 0.3f;
            should_protect = false;
            break;
        case SEC_HIPPO_SLEEP_LIGHT_NREM:
            protection_level = 0.5f;
            should_protect = true;
            break;
        case SEC_HIPPO_SLEEP_DEEP_NREM:
            protection_level = 1.0f;
            should_protect = bridge->config.protect_nrem_deep;
            break;
        case SEC_HIPPO_SLEEP_REM:
            protection_level = 0.8f;
            should_protect = bridge->config.protect_rem;
            break;
    }

    /* Apply protection */
    if (should_protect && bridge->config.enable_sleep_protection) {
        bridge->security_effects.sleep_protection_active = true;
        bridge->security_effects.ripple_filter_level =
            protection_level * bridge->config.ripple_filter_threshold;
        bridge->security_effects.spindle_gate_level =
            protection_level * bridge->config.spindle_gate_threshold;

        if (internal && !internal->protection_active) {
            internal->protection_active = true;
            internal->protection_start_time = get_timestamp_us();
        }

        bridge->stats.sleep_protection_activations++;
        bridge->state.active_protections++;

        add_audit_entry(bridge, SEC_HIPPO_AUDIT_SLEEP_PROTECT, true,
                       protection_level, "Sleep protection activated");
    } else {
        bridge->security_effects.sleep_protection_active = false;
        bridge->security_effects.ripple_filter_level = 0.0f;
        bridge->security_effects.spindle_gate_level = 0.0f;

        if (internal) {
            internal->protection_active = false;
        }
    }

    bridge->stats.sleep_phases_protected++;
    bridge->state.last_sleep_check = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_MONITORING;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_set_sleep_phase(
    sec_hippo_bridge_t* bridge,
    sec_hippo_sleep_phase_t phase
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->hippo_effects.current_sleep_phase = phase;
    BRIDGE_UNLOCK(bridge);

    /* Trigger protection update */
    return security_hippocampus_protect_sleep(bridge, phase);
}

sec_hippo_sleep_phase_t security_hippocampus_get_sleep_phase(
    const sec_hippo_bridge_t* bridge
)
{
    if (!bridge) {
        return SEC_HIPPO_SLEEP_AWAKE;
    }
    return bridge->hippo_effects.current_sleep_phase;
}

bool security_hippocampus_is_sleep_protected(const sec_hippo_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->security_effects.sleep_protection_active;
}

/* ============================================================================
 * Consolidation Verification Functions
 * ============================================================================ */

int security_hippocampus_verify_consolidation(
    sec_hippo_bridge_t* bridge,
    uint64_t memory_id,
    sec_hippo_consolidation_status_t* status_out,
    float* confidence_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(status_out, NIMCP_ERROR_NULL_POINTER, "status_out is NULL");
    NIMCP_CHECK_THROW(confidence_out, NIMCP_ERROR_NULL_POINTER, "confidence_out is NULL");

    BRIDGE_LOCK(bridge);

    uint64_t start_time = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_VERIFYING;
    bridge->stats.consolidation_checks++;

    /* Find consolidation event for this memory */
    sec_hippo_consolidation_event_t* event = NULL;
    for (uint32_t i = 0; i < bridge->num_consolidation_events; i++) {
        if (bridge->consolidation_events[i].memory_id == memory_id) {
            event = &bridge->consolidation_events[i];
            break;
        }
    }

    sec_hippo_consolidation_status_t status;
    float confidence;

    if (!event) {
        /* No consolidation event found - assume incomplete */
        status = SEC_HIPPO_CONSOL_INCOMPLETE;
        confidence = 0.5f;
    } else {
        /* Verify consolidation based on strength change */
        float strength_change = event->strength_after - event->strength_before;
        float min_strength = bridge->config.consolidation_min_strength;

        if (event->strength_after >= min_strength && strength_change >= 0.0f) {
            status = SEC_HIPPO_CONSOL_OK;
            confidence = event->strength_after;
            bridge->stats.consolidations_verified++;
        } else if (strength_change < -bridge->config.degradation_threshold) {
            status = SEC_HIPPO_CONSOL_CORRUPTED;
            confidence = fabsf(strength_change);
            bridge->stats.consolidations_corrupted++;
        } else if (strength_change < 0.0f) {
            status = SEC_HIPPO_CONSOL_DEGRADED;
            confidence = 1.0f + strength_change;
            bridge->stats.consolidations_degraded++;
        } else {
            status = SEC_HIPPO_CONSOL_OK;
            confidence = 0.8f;
            bridge->stats.consolidations_verified++;
        }

        event->status = status;
        event->verified = true;
    }

    *status_out = status;
    *confidence_out = confidence;

    /* Update latency stats */
    uint64_t elapsed = get_timestamp_us() - start_time;
    bridge->stats.mean_consol_verify_latency_us =
        (bridge->stats.mean_consol_verify_latency_us *
         (bridge->stats.consolidation_checks - 1) + (float)elapsed) /
        bridge->stats.consolidation_checks;

    bridge->state.last_consol_verify = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_MONITORING;

    add_audit_entry(bridge, SEC_HIPPO_AUDIT_CONSOL_VERIFY,
                   status == SEC_HIPPO_CONSOL_OK,
                   1.0f - confidence, "Consolidation verification");

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_verify_all_consolidations(
    sec_hippo_bridge_t* bridge,
    uint32_t* verified_count_out,
    uint32_t* failed_count_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(verified_count_out, NIMCP_ERROR_NULL_POINTER, "verified_count_out is NULL");
    NIMCP_CHECK_THROW(failed_count_out, NIMCP_ERROR_NULL_POINTER, "failed_count_out is NULL");

    uint32_t verified = 0;
    uint32_t failed = 0;

    BRIDGE_LOCK(bridge);

    for (uint32_t i = 0; i < bridge->num_consolidation_events; i++) {
        if (!bridge->consolidation_events[i].verified) {
            sec_hippo_consolidation_status_t status;
            float confidence;

            /* Copy memory_id under lock to avoid TOCTOU race after unlock */
            uint64_t memory_id = bridge->consolidation_events[i].memory_id;

            BRIDGE_UNLOCK(bridge);
            int result = security_hippocampus_verify_consolidation(
                bridge, memory_id,
                &status, &confidence);
            BRIDGE_LOCK(bridge);

            if (result == 0) {
                if (status == SEC_HIPPO_CONSOL_OK) {
                    verified++;
                } else {
                    failed++;
                }
            }
        }
    }

    BRIDGE_UNLOCK(bridge);

    *verified_count_out = verified;
    *failed_count_out = failed;

    return 0;
}

int security_hippocampus_pause_consolidation(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (internal) {
        internal->consolidation_paused = true;
    }
    bridge->security_effects.consolidation_paused = true;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Consolidation paused for verification");
    return 0;
}

int security_hippocampus_resume_consolidation(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (internal) {
        internal->consolidation_paused = false;
    }
    bridge->security_effects.consolidation_paused = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Consolidation resumed");
    return 0;
}

/* ============================================================================
 * Injection Detection Functions
 * ============================================================================ */

bool security_hippocampus_detect_injection(
    sec_hippo_bridge_t* bridge,
    sec_hippo_injection_type_t* injection_out,
    float* confidence_out,
    char* details_out,
    size_t details_size
)
{
    if (!bridge) {
        return false;
    }

    BRIDGE_LOCK(bridge);

    uint64_t start_time = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_DETECTING;
    bridge->stats.injection_scans++;

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;

    sec_hippo_injection_type_t injection = SEC_HIPPO_INJECT_NONE;
    float confidence = 0.0f;
    bool detected = false;

    if (internal && bridge->config.enable_injection_detection) {
        /* Check for anomalies in consolidation patterns */
        /* Simple heuristic: rapid consecutive consolidation events */
        uint32_t rapid_events = 0;
        uint64_t threshold_us = 100000;  /* 100ms */

        for (uint32_t i = 1; i < bridge->num_consolidation_events && i < 10; i++) {
            uint64_t t1 = bridge->consolidation_events[i - 1].timestamp;
            uint64_t t2 = bridge->consolidation_events[i].timestamp;
            if (t2 > t1 && (t2 - t1) < threshold_us) {
                rapid_events++;
            }
        }

        /* Anomaly detection based on rapid events */
        if (rapid_events >= 5) {
            injection = SEC_HIPPO_INJECT_PATTERN_POISON;
            confidence = 0.7f * bridge->config.injection_sensitivity;
            detected = true;
        }

        /* Check for spatial coherence anomalies */
        float spatial = compute_spatial_coherence(internal);
        if (spatial < 0.3f) {
            injection = SEC_HIPPO_INJECT_SPATIAL_FAKE;
            confidence = fmaxf(confidence, (0.3f - spatial) * 2.0f);
            detected = true;
        }

        /* Check for temporal coherence anomalies */
        float temporal = compute_temporal_coherence(internal);
        if (temporal < 0.3f) {
            injection = SEC_HIPPO_INJECT_TEMPORAL_SPLICE;
            confidence = fmaxf(confidence, (0.3f - temporal) * 2.0f);
            detected = true;
        }
    }

    /* Update effects */
    bridge->hippo_effects.injection_type = injection;
    bridge->hippo_effects.injection_confidence = confidence;

    if (detected) {
        bridge->stats.injections_detected++;
        bridge->security_effects.injection_guard_active = true;
        bridge->security_effects.current_threat_level = confidence;

        add_audit_entry(bridge, SEC_HIPPO_AUDIT_INJECT_DETECT, false,
                       confidence, "Injection attempt detected");
    }

    /* Update latency stats */
    uint64_t elapsed = get_timestamp_us() - start_time;
    bridge->stats.mean_inject_scan_latency_us =
        (bridge->stats.mean_inject_scan_latency_us *
         (bridge->stats.injection_scans - 1) + (float)elapsed) /
        bridge->stats.injection_scans;

    bridge->state.last_inject_scan = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_MONITORING;

    BRIDGE_UNLOCK(bridge);

    /* Output results */
    if (injection_out) {
        *injection_out = injection;
    }
    if (confidence_out) {
        *confidence_out = confidence;
    }
    if (details_out && details_size > 0) {
        if (detected) {
            snprintf(details_out, details_size, "Detected %s with %.1f%% confidence",
                    security_hippocampus_injection_name(injection), confidence * 100.0f);
        } else {
            details_out[0] = '\0';
        }
    }

    return detected;
}

int security_hippocampus_block_injection(
    sec_hippo_bridge_t* bridge,
    sec_hippo_injection_type_t injection_type
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->security_effects.injection_guard_active = true;
    bridge->security_effects.injections_blocked++;
    bridge->stats.injections_blocked++;

    add_audit_entry(bridge, SEC_HIPPO_AUDIT_THREAT_RESPONSE, true,
                   0.8f, "Injection blocked");

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Blocked injection type: %s",
                      security_hippocampus_injection_name(injection_type));
    return 0;
}

int security_hippocampus_register_pattern(
    sec_hippo_bridge_t* bridge,
    uint64_t pattern_id,
    uint64_t pattern_hash
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    if (internal->num_patterns >= MAX_PATTERNS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "pattern limit exceeded");
    }

    registered_pattern_t* pattern = &internal->patterns[internal->num_patterns++];
    pattern->pattern_id = pattern_id;
    pattern->pattern_hash = pattern_hash;
    pattern->valid = true;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Replay Validation Functions
 * ============================================================================ */

int security_hippocampus_validate_replay(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id,
    sec_hippo_replay_status_t* status_out,
    float* match_score_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(status_out, NIMCP_ERROR_NULL_POINTER, "status_out is NULL");
    NIMCP_CHECK_THROW(match_score_out, NIMCP_ERROR_NULL_POINTER, "match_score_out is NULL");

    BRIDGE_LOCK(bridge);

    uint64_t start_time = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_VALIDATING;
    bridge->stats.replays_validated++;

    /* Find replay sequence */
    sec_hippo_replay_sequence_t* seq = NULL;
    for (uint32_t i = 0; i < bridge->num_replay_sequences; i++) {
        if (bridge->replay_sequences[i].sequence_id == sequence_id) {
            seq = &bridge->replay_sequences[i];
            break;
        }
    }

    sec_hippo_replay_status_t status;
    float match_score;

    if (!seq) {
        /* Sequence not found - create a new one */
        if (bridge->num_replay_sequences < SEC_HIPPO_MAX_REPLAY_SEQUENCES) {
            seq = &bridge->replay_sequences[bridge->num_replay_sequences++];
            seq->sequence_id = sequence_id;
            seq->start_time = get_timestamp_us();
            seq->event_count = 0;
            seq->replay_strength = 0.5f;
            seq->content_match_score = 0.5f;
            seq->status = SEC_HIPPO_REPLAY_VALID;
            seq->during_sleep = bridge->hippo_effects.current_sleep_phase != SEC_HIPPO_SLEEP_AWAKE;
            seq->sleep_phase = bridge->hippo_effects.current_sleep_phase;
        }
        status = SEC_HIPPO_REPLAY_VALID;
        match_score = 0.5f;  /* Unknown - neutral score */
    } else {
        /* Validate existing sequence */
        sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;

        /* Check if encoding exists */
        registered_encoding_t* encoding = internal ?
            find_encoding(internal, seq->encoding_id) : NULL;

        if (encoding) {
            /* Compare content hash (simplified) */
            match_score = seq->content_match_score;

            if (match_score >= bridge->config.replay_content_match_threshold) {
                status = SEC_HIPPO_REPLAY_VALID;
                bridge->stats.replays_valid++;
            } else if (match_score < 0.3f) {
                status = SEC_HIPPO_REPLAY_FORGED;
                bridge->stats.replays_hijacked++;
            } else {
                status = SEC_HIPPO_REPLAY_CONTENT_MISMATCH;
                bridge->stats.replays_invalid++;
            }
        } else {
            /* No encoding to compare - check timing */
            if (seq->event_count > 0 && seq->replay_strength > 0.5f) {
                status = SEC_HIPPO_REPLAY_VALID;
                match_score = seq->replay_strength;
                bridge->stats.replays_valid++;
            } else {
                status = SEC_HIPPO_REPLAY_OUT_OF_ORDER;
                match_score = 0.3f;
                bridge->stats.replays_invalid++;
            }
        }

        seq->status = status;
    }

    *status_out = status;
    *match_score_out = match_score;

    /* Update security effects */
    bridge->security_effects.replay_validation_active = true;
    bridge->security_effects.replays_validated++;
    if (status != SEC_HIPPO_REPLAY_VALID) {
        bridge->security_effects.replays_rejected++;
    }

    /* Update latency stats */
    uint64_t elapsed = get_timestamp_us() - start_time;
    bridge->stats.mean_replay_validate_latency_us =
        (bridge->stats.mean_replay_validate_latency_us *
         (bridge->stats.replays_validated - 1) + (float)elapsed) /
        bridge->stats.replays_validated;

    bridge->state.last_replay_validate = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_MONITORING;

    add_audit_entry(bridge, SEC_HIPPO_AUDIT_REPLAY_VALIDATE,
                   status == SEC_HIPPO_REPLAY_VALID,
                   1.0f - match_score, "Replay validation");

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_register_encoding(
    sec_hippo_bridge_t* bridge,
    uint64_t encoding_id,
    uint64_t content_hash,
    uint64_t timestamp
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    if (internal->num_encodings >= MAX_ENCODINGS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "encoding limit exceeded");
    }

    registered_encoding_t* enc = &internal->encodings[internal->num_encodings++];
    enc->encoding_id = encoding_id;
    enc->content_hash = content_hash;
    enc->timestamp = timestamp;
    enc->valid = true;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_reject_replay(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    for (uint32_t i = 0; i < bridge->num_replay_sequences; i++) {
        if (bridge->replay_sequences[i].sequence_id == sequence_id) {
            bridge->replay_sequences[i].status = SEC_HIPPO_REPLAY_FORGED;
            bridge->security_effects.replays_rejected++;
            bridge->stats.replays_invalid++;
            break;
        }
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_get_replay_info(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id,
    sec_hippo_replay_sequence_t* sequence_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(sequence_out, NIMCP_ERROR_NULL_POINTER, "sequence_out is NULL");

    BRIDGE_LOCK(bridge);

    for (uint32_t i = 0; i < bridge->num_replay_sequences; i++) {
        if (bridge->replay_sequences[i].sequence_id == sequence_id) {
            *sequence_out = bridge->replay_sequences[i];
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    BRIDGE_UNLOCK(bridge);

    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "replay sequence not found");
    return 0; /* unreachable */
}

/* ============================================================================
 * Coherence Checking Functions
 * ============================================================================ */

int security_hippocampus_check_coherence(
    sec_hippo_bridge_t* bridge,
    sec_hippo_coherence_status_t* status_out,
    float* spatial_score_out,
    float* temporal_score_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(status_out, NIMCP_ERROR_NULL_POINTER, "status_out is NULL");
    NIMCP_CHECK_THROW(spatial_score_out, NIMCP_ERROR_NULL_POINTER, "spatial_score_out is NULL");
    NIMCP_CHECK_THROW(temporal_score_out, NIMCP_ERROR_NULL_POINTER, "temporal_score_out is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_HIPPO_STATE_VERIFYING;
    bridge->stats.coherence_checks++;

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;

    float spatial = 1.0f;
    float temporal = 1.0f;

    if (internal) {
        spatial = compute_spatial_coherence(internal);
        temporal = compute_temporal_coherence(internal);
    }

    /* Determine overall status */
    sec_hippo_coherence_status_t status;
    float spatial_thresh = bridge->config.spatial_coherence_threshold;
    float temporal_thresh = bridge->config.temporal_coherence_threshold;

    if (spatial >= spatial_thresh && temporal >= temporal_thresh) {
        status = SEC_HIPPO_COHERENCE_OK;
        bridge->stats.coherence_ok++;
    } else if (spatial < 0.3f && temporal < 0.3f) {
        status = SEC_HIPPO_COHERENCE_SCRAMBLED;
        bridge->stats.coherence_failures++;
    } else if (spatial < spatial_thresh) {
        status = SEC_HIPPO_COHERENCE_SPATIAL_DRIFT;
        bridge->stats.coherence_failures++;
    } else {
        status = SEC_HIPPO_COHERENCE_TEMPORAL_GAP;
        bridge->stats.coherence_failures++;
    }

    *status_out = status;
    *spatial_score_out = spatial;
    *temporal_score_out = temporal;

    /* Update effects */
    bridge->hippo_effects.coherence_status = status;
    bridge->hippo_effects.spatial_coherence = spatial;
    bridge->hippo_effects.temporal_coherence = temporal;

    /* Update mean scores */
    bridge->stats.mean_spatial_coherence =
        (bridge->stats.mean_spatial_coherence *
         (bridge->stats.coherence_checks - 1) + spatial) /
        bridge->stats.coherence_checks;
    bridge->stats.mean_temporal_coherence =
        (bridge->stats.mean_temporal_coherence *
         (bridge->stats.coherence_checks - 1) + temporal) /
        bridge->stats.coherence_checks;

    bridge->state.last_coherence_check = get_timestamp_us();
    bridge->state.state = SEC_HIPPO_STATE_MONITORING;

    if (status != SEC_HIPPO_COHERENCE_OK) {
        add_audit_entry(bridge, SEC_HIPPO_AUDIT_COHERENCE_CHECK, false,
                       1.0f - fminf(spatial, temporal), "Coherence check failed");
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_check_spatial_coherence(
    sec_hippo_bridge_t* bridge,
    float* score_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(score_out, NIMCP_ERROR_NULL_POINTER, "score_out is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    *score_out = internal ? compute_spatial_coherence(internal) : 1.0f;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_check_temporal_coherence(
    sec_hippo_bridge_t* bridge,
    float* score_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(score_out, NIMCP_ERROR_NULL_POINTER, "score_out is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    *score_out = internal ? compute_temporal_coherence(internal) : 1.0f;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_report_place_cell(
    sec_hippo_bridge_t* bridge,
    uint32_t cell_id,
    float position_x,
    float position_y,
    float firing_rate
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    place_cell_record_t* record = &internal->place_cells[internal->place_cell_head];
    record->cell_id = cell_id;
    record->position_x = position_x;
    record->position_y = position_y;
    record->firing_rate = firing_rate;
    record->timestamp = get_timestamp_us();

    internal->place_cell_head = (internal->place_cell_head + 1) % MAX_PLACE_CELL_HISTORY;
    if (internal->place_cell_count < MAX_PLACE_CELL_HISTORY) {
        internal->place_cell_count++;
    }

    bridge->hippo_effects.active_place_cells++;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_report_time_cell(
    sec_hippo_bridge_t* bridge,
    uint32_t cell_id,
    uint64_t timestamp,
    float firing_rate
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    time_cell_record_t* record = &internal->time_cells[internal->time_cell_head];
    record->cell_id = cell_id;
    record->timestamp = timestamp;
    record->firing_rate = firing_rate;

    internal->time_cell_head = (internal->time_cell_head + 1) % MAX_TIME_CELL_HISTORY;
    if (internal->time_cell_count < MAX_TIME_CELL_HISTORY) {
        internal->time_cell_count++;
    }

    bridge->hippo_effects.active_time_cells++;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update Functions
 * ============================================================================ */

int security_hippocampus_bridge_update(
    sec_hippo_bridge_t* bridge,
    uint64_t delta_ms
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Record update */
    bridge_base_record_update(&bridge->base);

    uint64_t now = get_timestamp_us();

    /* Update sleep-phase stats */
    switch (bridge->hippo_effects.current_sleep_phase) {
        case SEC_HIPPO_SLEEP_DEEP_NREM:
            bridge->stats.nrem_deep_events++;
            break;
        case SEC_HIPPO_SLEEP_REM:
            bridge->stats.rem_events++;
            break;
        case SEC_HIPPO_SLEEP_AWAKE:
            bridge->stats.awake_events++;
            break;
        default:
            break;
    }

    BRIDGE_UNLOCK(bridge);

    /* Gather hippo effects */
    security_hippocampus_gather_hippo_effects(bridge);

    /* Apply security effects */
    security_hippocampus_apply_security_effects(bridge);

    BRIDGE_LOCK(bridge);

    /* Periodic sleep protection check */
    if (bridge->config.enable_sleep_protection) {
        if (now - bridge->state.last_sleep_check > SEC_HIPPO_SLEEP_CHECK_INTERVAL_MS * 1000) {
            BRIDGE_UNLOCK(bridge);
            security_hippocampus_protect_sleep(bridge, bridge->hippo_effects.current_sleep_phase);
            BRIDGE_LOCK(bridge);
        }
    }

    /* Periodic injection detection */
    if (bridge->config.enable_injection_detection) {
        if (now - bridge->state.last_inject_scan > (uint64_t)delta_ms * 10000) {
            sec_hippo_injection_type_t injection;
            float confidence;
            BRIDGE_UNLOCK(bridge);
            security_hippocampus_detect_injection(bridge, &injection, &confidence, NULL, 0);
            BRIDGE_LOCK(bridge);
        }
    }

    /* Periodic coherence check */
    if (bridge->config.enable_coherence_checking) {
        if (now - bridge->state.last_coherence_check > (uint64_t)delta_ms * 5000) {
            sec_hippo_coherence_status_t status;
            float spatial, temporal;
            BRIDGE_UNLOCK(bridge);
            security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
            BRIDGE_LOCK(bridge);
        }
    }

    /* Update security latency */
    bridge->security_effects.security_latency_ms =
        (float)delta_ms * bridge->config.security_sensitivity * 0.1f;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_apply_security_effects(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Calculate throughput reduction based on active protections */
    float overhead = 0.0f;
    if (bridge->security_effects.sleep_protection_active) overhead += 0.05f;
    if (bridge->security_effects.injection_guard_active) overhead += 0.03f;
    if (bridge->security_effects.replay_validation_active) overhead += 0.02f;
    if (bridge->security_effects.consolidation_paused) overhead += 0.1f;

    bridge->security_effects.throughput_reduction =
        overhead * bridge->config.security_sensitivity;
    if (bridge->security_effects.throughput_reduction > 0.5f) {
        bridge->security_effects.throughput_reduction = 0.5f;
    }

    /* Calculate consolidation throttle */
    if (bridge->security_effects.current_threat_level > 0.5f) {
        bridge->security_effects.consolidation_throttle =
            1.0f - bridge->security_effects.current_threat_level;
    } else {
        bridge->security_effects.consolidation_throttle = 1.0f;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_hippocampus_gather_hippo_effects(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    sec_hippo_internal_t* internal = (sec_hippo_internal_t*)bridge->base.system_b;

    /* Update coherence scores */
    if (internal) {
        bridge->hippo_effects.spatial_coherence = compute_spatial_coherence(internal);
        bridge->hippo_effects.temporal_coherence = compute_temporal_coherence(internal);
    }

    /* Calculate pattern regularity */
    if (bridge->num_consolidation_events > 1) {
        float total_interval = 0.0f;
        uint32_t intervals = 0;

        for (uint32_t i = 1; i < bridge->num_consolidation_events && i < 10; i++) {
            uint64_t t1 = bridge->consolidation_events[i - 1].timestamp;
            uint64_t t2 = bridge->consolidation_events[i].timestamp;
            if (t2 > t1) {
                total_interval += (float)(t2 - t1);
                intervals++;
            }
        }

        if (intervals > 0) {
            float avg_interval = total_interval / intervals;
            /* Regularity based on interval consistency */
            float variance = 0.0f;
            for (uint32_t i = 1; i < bridge->num_consolidation_events && i < 10; i++) {
                uint64_t t1 = bridge->consolidation_events[i - 1].timestamp;
                uint64_t t2 = bridge->consolidation_events[i].timestamp;
                if (t2 > t1) {
                    float diff = (float)(t2 - t1) - avg_interval;
                    variance += diff * diff;
                }
            }
            variance /= intervals;
            bridge->hippo_effects.pattern_regularity =
                1.0f / (1.0f + sqrtf(variance) / avg_interval);
        }
    }

    /* Update oscillation power estimates (stub - would query real hippocampus) */
    bridge->hippo_effects.theta_power = 0.5f;  /* Placeholder */
    bridge->hippo_effects.gamma_power = 0.3f;  /* Placeholder */

    /* Update consolidation rate */
    if (bridge->num_consolidation_events > 0) {
        uint64_t window = 60000000;  /* 60 seconds in us */
        uint32_t recent_count = 0;
        uint64_t now = get_timestamp_us();

        for (uint32_t i = 0; i < bridge->num_consolidation_events; i++) {
            if (now - bridge->consolidation_events[i].timestamp < window) {
                recent_count++;
            }
        }

        bridge->hippo_effects.current_consolidation_rate =
            (float)recent_count / 60.0f;  /* Events per second */
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int security_hippocampus_get_security_effects(
    const sec_hippo_bridge_t* bridge,
    security_to_hippo_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->security_effects;
    return 0;
}

int security_hippocampus_get_hippo_effects(
    const sec_hippo_bridge_t* bridge,
    hippo_to_security_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->hippo_effects;
    return 0;
}

int security_hippocampus_get_state(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_state_info_t* state_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state_out, NIMCP_ERROR_NULL_POINTER, "state_out is NULL");

    *state_out = bridge->state;
    return 0;
}

int security_hippocampus_get_stats(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_stats_t* stats_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats_out, NIMCP_ERROR_NULL_POINTER, "stats_out is NULL");

    *stats_out = bridge->stats;
    return 0;
}

int security_hippocampus_reset_stats(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(sec_hippo_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Audit Functions
 * ============================================================================ */

int security_hippocampus_get_audit_log(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entries_out, NIMCP_ERROR_NULL_POINTER, "entries_out is NULL");
    NIMCP_CHECK_THROW(count_out, NIMCP_ERROR_NULL_POINTER, "count_out is NULL");
    NIMCP_CHECK_THROW(bridge->audit_log, NIMCP_ERROR_INVALID_STATE, "audit log not initialized");

    size_t to_copy = (bridge->audit_log_count < max_entries) ?
                     bridge->audit_log_count : max_entries;

    /* Copy from circular buffer, newest first */
    for (size_t i = 0; i < to_copy; i++) {
        uint32_t idx = (bridge->audit_log_head + SEC_HIPPO_MAX_AUDIT_ENTRIES - 1 - i) %
                       SEC_HIPPO_MAX_AUDIT_ENTRIES;
        entries_out[i] = bridge->audit_log[idx];
    }

    *count_out = to_copy;
    return 0;
}

int security_hippocampus_clear_audit_log(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Cleared security-hippocampus audit log");
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_hippocampus_connect_bio_async(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_connect_bio_async(&bridge->base);
}

int security_hippocampus_disconnect_bio_async(sec_hippo_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_hippocampus_is_bio_async_connected(const sec_hippo_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge_base_is_bio_async_connected(&bridge->base);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_hippocampus_sleep_phase_name(sec_hippo_sleep_phase_t phase)
{
    switch (phase) {
        case SEC_HIPPO_SLEEP_AWAKE:      return "AWAKE";
        case SEC_HIPPO_SLEEP_DROWSY:     return "DROWSY";
        case SEC_HIPPO_SLEEP_LIGHT_NREM: return "LIGHT_NREM";
        case SEC_HIPPO_SLEEP_DEEP_NREM:  return "DEEP_NREM";
        case SEC_HIPPO_SLEEP_REM:        return "REM";
        default:                         return "UNKNOWN";
    }
}

const char* security_hippocampus_injection_name(sec_hippo_injection_type_t type)
{
    switch (type) {
        case SEC_HIPPO_INJECT_NONE:           return "NONE";
        case SEC_HIPPO_INJECT_FALSE_MEMORY:   return "FALSE_MEMORY";
        case SEC_HIPPO_INJECT_PATTERN_POISON: return "PATTERN_POISON";
        case SEC_HIPPO_INJECT_TEMPORAL_SPLICE:return "TEMPORAL_SPLICE";
        case SEC_HIPPO_INJECT_SPATIAL_FAKE:   return "SPATIAL_FAKE";
        case SEC_HIPPO_INJECT_CONTEXT_CORRUPT:return "CONTEXT_CORRUPT";
        case SEC_HIPPO_INJECT_RIPPLE_FORGE:   return "RIPPLE_FORGE";
        default:                              return "UNKNOWN";
    }
}

const char* security_hippocampus_consolidation_name(sec_hippo_consolidation_status_t status)
{
    switch (status) {
        case SEC_HIPPO_CONSOL_OK:         return "OK";
        case SEC_HIPPO_CONSOL_DEGRADED:   return "DEGRADED";
        case SEC_HIPPO_CONSOL_CORRUPTED:  return "CORRUPTED";
        case SEC_HIPPO_CONSOL_TAMPERED:   return "TAMPERED";
        case SEC_HIPPO_CONSOL_INCOMPLETE: return "INCOMPLETE";
        default:                          return "UNKNOWN";
    }
}

const char* security_hippocampus_replay_name(sec_hippo_replay_status_t status)
{
    switch (status) {
        case SEC_HIPPO_REPLAY_VALID:           return "VALID";
        case SEC_HIPPO_REPLAY_OUT_OF_ORDER:    return "OUT_OF_ORDER";
        case SEC_HIPPO_REPLAY_TIMING_ANOMALY:  return "TIMING_ANOMALY";
        case SEC_HIPPO_REPLAY_CONTENT_MISMATCH:return "CONTENT_MISMATCH";
        case SEC_HIPPO_REPLAY_FORGED:          return "FORGED";
        case SEC_HIPPO_REPLAY_HIJACKED:        return "HIJACKED";
        default:                               return "UNKNOWN";
    }
}

const char* security_hippocampus_coherence_name(sec_hippo_coherence_status_t status)
{
    switch (status) {
        case SEC_HIPPO_COHERENCE_OK:              return "OK";
        case SEC_HIPPO_COHERENCE_SPATIAL_DRIFT:   return "SPATIAL_DRIFT";
        case SEC_HIPPO_COHERENCE_TEMPORAL_GAP:    return "TEMPORAL_GAP";
        case SEC_HIPPO_COHERENCE_CONTEXT_MISMATCH:return "CONTEXT_MISMATCH";
        case SEC_HIPPO_COHERENCE_SCRAMBLED:       return "SCRAMBLED";
        default:                                  return "UNKNOWN";
    }
}

const char* security_hippocampus_state_name(sec_hippo_state_t state)
{
    switch (state) {
        case SEC_HIPPO_STATE_IDLE:       return "IDLE";
        case SEC_HIPPO_STATE_MONITORING: return "MONITORING";
        case SEC_HIPPO_STATE_PROTECTING: return "PROTECTING";
        case SEC_HIPPO_STATE_VERIFYING:  return "VERIFYING";
        case SEC_HIPPO_STATE_DETECTING:  return "DETECTING";
        case SEC_HIPPO_STATE_VALIDATING: return "VALIDATING";
        case SEC_HIPPO_STATE_RESPONDING: return "RESPONDING";
        case SEC_HIPPO_STATE_ERROR:      return "ERROR";
        default:                         return "UNKNOWN";
    }
}

const char* security_hippocampus_audit_type_name(sec_hippo_audit_type_t type)
{
    switch (type) {
        case SEC_HIPPO_AUDIT_SLEEP_PROTECT:  return "SLEEP_PROTECT";
        case SEC_HIPPO_AUDIT_CONSOL_VERIFY:  return "CONSOL_VERIFY";
        case SEC_HIPPO_AUDIT_INJECT_DETECT:  return "INJECT_DETECT";
        case SEC_HIPPO_AUDIT_REPLAY_VALIDATE:return "REPLAY_VALIDATE";
        case SEC_HIPPO_AUDIT_COHERENCE_CHECK:return "COHERENCE_CHECK";
        case SEC_HIPPO_AUDIT_THREAT_RESPONSE:return "THREAT_RESPONSE";
        case SEC_HIPPO_AUDIT_ANOMALY:        return "ANOMALY";
        default:                             return "UNKNOWN";
    }
}

void security_hippocampus_print_summary(const sec_hippo_bridge_t* bridge)
{
    if (!bridge) {
        printf("Security-Hippocampus Bridge: NULL\n");
        return;
    }

    printf("\n=== Security-Hippocampus Bridge Summary ===\n");
    printf("State: %s\n", security_hippocampus_state_name(bridge->state.state));
    printf("Sleep Phase: %s\n",
           security_hippocampus_sleep_phase_name(bridge->hippo_effects.current_sleep_phase));
    printf("Connections:\n");
    printf("  Hippocampus:  %s\n", bridge->hippocampus_connected ? "Connected" : "Disconnected");
    printf("  Sleep System: %s\n", bridge->sleep_connected ? "Connected" : "Disconnected");
    printf("  Bio-Async:    %s\n", bridge->base.bio_async_enabled ? "Connected" : "Disconnected");
    printf("Protection:\n");
    printf("  Sleep Protection: %s\n",
           bridge->security_effects.sleep_protection_active ? "Active" : "Inactive");
    printf("  Injection Guard:  %s\n",
           bridge->security_effects.injection_guard_active ? "Active" : "Inactive");
    printf("  Threat Level:     %.1f%%\n",
           bridge->security_effects.current_threat_level * 100.0f);
    printf("Coherence:\n");
    printf("  Spatial:  %.1f%%\n", bridge->hippo_effects.spatial_coherence * 100.0f);
    printf("  Temporal: %.1f%%\n", bridge->hippo_effects.temporal_coherence * 100.0f);
    printf("Tracking:\n");
    printf("  Replay Sequences:     %u\n", bridge->num_replay_sequences);
    printf("  Consolidation Events: %u\n", bridge->num_consolidation_events);
    printf("  Audit Log Entries:    %u\n", bridge->audit_log_count);
    printf("\n");
}

void security_hippocampus_print_stats(const sec_hippo_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n=== Security-Hippocampus Bridge Statistics ===\n");
    printf("Sleep Protection:\n");
    printf("  Activations:     %lu\n", (unsigned long)stats->sleep_protection_activations);
    printf("  Phases Protected: %lu\n", (unsigned long)stats->sleep_phases_protected);
    printf("  Ripples Analyzed: %lu\n", (unsigned long)stats->ripples_analyzed);
    printf("  Ripples Blocked:  %lu\n", (unsigned long)stats->ripples_blocked);
    printf("\nConsolidation Verification:\n");
    printf("  Checks:     %lu\n", (unsigned long)stats->consolidation_checks);
    printf("  Verified:   %lu\n", (unsigned long)stats->consolidations_verified);
    printf("  Degraded:   %lu\n", (unsigned long)stats->consolidations_degraded);
    printf("  Corrupted:  %lu\n", (unsigned long)stats->consolidations_corrupted);
    printf("\nInjection Detection:\n");
    printf("  Scans:      %lu\n", (unsigned long)stats->injection_scans);
    printf("  Detected:   %lu\n", (unsigned long)stats->injections_detected);
    printf("  Blocked:    %lu\n", (unsigned long)stats->injections_blocked);
    printf("\nReplay Validation:\n");
    printf("  Validated:  %lu\n", (unsigned long)stats->replays_validated);
    printf("  Valid:      %lu\n", (unsigned long)stats->replays_valid);
    printf("  Invalid:    %lu\n", (unsigned long)stats->replays_invalid);
    printf("  Hijacked:   %lu\n", (unsigned long)stats->replays_hijacked);
    printf("\nCoherence Checks:\n");
    printf("  Checks:     %lu\n", (unsigned long)stats->coherence_checks);
    printf("  OK:         %lu\n", (unsigned long)stats->coherence_ok);
    printf("  Failures:   %lu\n", (unsigned long)stats->coherence_failures);
    printf("  Mean Spatial:  %.1f%%\n", stats->mean_spatial_coherence * 100.0f);
    printf("  Mean Temporal: %.1f%%\n", stats->mean_temporal_coherence * 100.0f);
    printf("\nPer-Phase Events:\n");
    printf("  Deep NREM:  %lu\n", (unsigned long)stats->nrem_deep_events);
    printf("  REM:        %lu\n", (unsigned long)stats->rem_events);
    printf("  Awake:      %lu\n", (unsigned long)stats->awake_events);
    printf("\n");
}
