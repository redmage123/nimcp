/**
 * @file nimcp_security_epistemic_bridge.c
 * @brief Security - Epistemic Filter Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implementation of security-epistemic bridge for protecting reasoning
 * WHY:  Enforce confidence validation, belief verification, and attack detection
 * HOW:  Validation checks, evidence chain analysis, pattern detection
 */

#include "security/epistemic/nimcp_security_epistemic_bridge.h"
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
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_epistemic_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Maximum source entries */
#define MAX_SOURCES 256

/** @brief Internal source tracking */
typedef struct {
    uint64_t source_id;
    float reliability;
    uint32_t correct_count;
    uint32_t incorrect_count;
    uint64_t last_update;
} source_entry_t;

/** @brief Internal bridge state */
typedef struct {
    /* Source tracking */
    source_entry_t sources[MAX_SOURCES];
    uint32_t source_count;

    /* Attack pattern tracking */
    struct {
        security_epist_attack_t type;
        uint64_t timestamp;
        float severity;
    } attack_history[32];
    uint32_t attack_history_head;
    uint32_t attack_history_count;

    /* Rate limiting for confidence changes */
    float last_confidence;
    uint64_t last_confidence_time;
    bool has_prior_confidence;  /* True after first confidence validation */
} security_epist_internal_t;

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
 * @brief Find belief index
 */
static int find_belief_index(
    const security_epist_bridge_t* bridge,
    uint64_t belief_id
)
{
    if (!bridge || !bridge->beliefs) {
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_beliefs; i++) {
        if (bridge->beliefs[i].belief_id == belief_id) {
            return (int)i;
        }
    }
    /* Belief not found - normal case during registration checks, not an error */
    return -1;
}

/**
 * @brief Find source index
 */
static int find_source_index(
    const security_epist_internal_t* internal,
    uint64_t source_id
)
{
    if (!internal) {
        return -1;
    }

    for (uint32_t i = 0; i < internal->source_count; i++) {
        if (internal->sources[i].source_id == source_id) {
            return (int)i;
        }
    }
    /* Source not found - normal case during registration checks, not an error */
    return -1;
}

/**
 * @brief Calculate confidence rate of change
 */
static float calculate_confidence_rate(
    security_epist_bridge_t* bridge,
    float new_confidence
)
{
    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (!internal) {
        return 0.0f;
    }

    /* First confidence check has no prior to compare against */
    if (!internal->has_prior_confidence) {
        internal->last_confidence = new_confidence;
        internal->last_confidence_time = get_timestamp_us();
        internal->has_prior_confidence = true;
        return 0.0f;
    }

    uint64_t now = get_timestamp_us();
    uint64_t elapsed = now - internal->last_confidence_time;

    /* Minimum elapsed time (1ms) before rate checking is meaningful.
     * Rapid changes during initialization or batch processing are normal. */
    const uint64_t MIN_RATE_CHECK_INTERVAL_US = 1000;
    if (elapsed < MIN_RATE_CHECK_INTERVAL_US) {
        internal->last_confidence = new_confidence;
        internal->last_confidence_time = now;
        return 0.0f;  /* Not enough time has passed to measure rate */
    }

    float diff = fabsf(new_confidence - internal->last_confidence);
    float rate = diff / ((float)elapsed / 1000000.0f);

    internal->last_confidence = new_confidence;
    internal->last_confidence_time = now;

    return rate;
}

/**
 * @brief Record confidence in history
 */
static void record_confidence_history(
    security_epist_bridge_t* bridge,
    float confidence
)
{
    if (!bridge->confidence_history) {
        return;
    }

    bridge->confidence_history[bridge->history_head] = confidence;
    bridge->history_head = (bridge->history_head + 1) % SEC_EPIST_CONFIDENCE_HISTORY;
    if (bridge->history_count < SEC_EPIST_CONFIDENCE_HISTORY) {
        bridge->history_count++;
    }
}

/**
 * @brief Calculate confidence variance from history
 */
static float calculate_confidence_variance(const security_epist_bridge_t* bridge)
{
    if (!bridge->confidence_history || bridge->history_count < 2) {
        return 0.0f;
    }

    float mean = 0.0f;
    for (uint32_t i = 0; i < bridge->history_count; i++) {
        mean += bridge->confidence_history[i];
    }
    mean /= (float)bridge->history_count;

    float variance = 0.0f;
    for (uint32_t i = 0; i < bridge->history_count; i++) {
        float diff = bridge->confidence_history[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)bridge->history_count;

    return variance;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int security_epist_default_config(security_epist_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(security_epist_config_t));

    /* Feature enable flags */
    config->enable_confidence_validation = true;
    config->enable_belief_verification = true;
    config->enable_uncertainty_enforcement = true;
    config->enable_evidence_validation = true;
    config->enable_attack_detection = true;
    config->enable_audit = true;
    config->enable_auto_correction = true;

    /* Confidence validation settings */
    config->min_confidence = SEC_EPIST_DEFAULT_MIN_CONFIDENCE;
    config->max_confidence = SEC_EPIST_DEFAULT_MAX_CONFIDENCE;
    config->max_confidence_rate = 1.0f;
    config->require_calibrated_confidence = false;

    /* Uncertainty enforcement settings */
    config->min_uncertainty = SEC_EPIST_DEFAULT_UNCERTAINTY_MIN;
    config->max_uncertainty = 1.0f;
    config->enforce_irreducible_uncertainty = true;

    /* Evidence validation settings */
    config->max_evidence_age_s = 86400;
    config->min_independent_sources = 1;
    config->min_source_reliability = 0.3f;
    config->detect_circular_evidence = true;

    /* Attack detection settings */
    config->attack_threshold = 0.7f;
    config->attack_window_ms = SEC_EPIST_ATTACK_WINDOW_MS;
    config->block_on_attack = false;

    /* Sensitivity parameters */
    config->security_sensitivity = 1.0f;
    config->epistemic_sensitivity = 1.0f;

    /* Bio-async integration */
    config->enable_bio_async = true;

    return 0;
}

security_epist_bridge_t* security_epist_bridge_create(
    const security_epist_config_t* config
)
{
    security_epist_bridge_t* bridge = nimcp_malloc(sizeof(security_epist_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_epist_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate security-epistemic bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(security_epist_bridge_t));

    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_EPISTEMIC,
                         "security_epistemic_bridge") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_epist_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        security_epist_default_config(&bridge->config);
    }

    bridge->beliefs = nimcp_malloc(SEC_EPIST_MAX_BELIEFS * sizeof(security_epist_belief_t));
    if (!bridge->beliefs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_epist_bridge_create: failed to allocate beliefs array");
        NIMCP_LOGGING_ERROR("Failed to allocate beliefs array");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->beliefs, 0, SEC_EPIST_MAX_BELIEFS * sizeof(security_epist_belief_t));

    bridge->confidence_history = nimcp_malloc(
        SEC_EPIST_CONFIDENCE_HISTORY * sizeof(float));
    if (!bridge->confidence_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_epist_bridge_create: failed to allocate confidence_history");
        NIMCP_LOGGING_ERROR("Failed to allocate confidence history");
        nimcp_free(bridge->beliefs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->confidence_history, 0, SEC_EPIST_CONFIDENCE_HISTORY * sizeof(float));

    if (bridge->config.enable_audit) {
        bridge->audit_log = nimcp_malloc(
            SEC_EPIST_MAX_AUDIT_ENTRIES * sizeof(security_epist_audit_entry_t));
        if (!bridge->audit_log) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_epist_bridge_create: failed to allocate audit_log");
            NIMCP_LOGGING_ERROR("Failed to allocate audit log");
            nimcp_free(bridge->confidence_history);
            nimcp_free(bridge->beliefs);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            return NULL;
        }
        memset(bridge->audit_log, 0,
               SEC_EPIST_MAX_AUDIT_ENTRIES * sizeof(security_epist_audit_entry_t));
    }

    security_epist_internal_t* internal = nimcp_malloc(sizeof(security_epist_internal_t));
    if (!internal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_epist_bridge_create: failed to allocate internal state");
        NIMCP_LOGGING_ERROR("Failed to allocate internal state");
        if (bridge->audit_log) nimcp_free(bridge->audit_log);
        nimcp_free(bridge->confidence_history);
        nimcp_free(bridge->beliefs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(internal, 0, sizeof(security_epist_internal_t));
    internal->last_confidence = 0.5f;
    internal->last_confidence_time = get_timestamp_us();

    bridge->base.system_b = internal;
    bridge->state.state = SEC_EPIST_STATE_IDLE;

    bridge->security_effects.enforced_min_confidence = bridge->config.min_confidence;
    bridge->security_effects.enforced_max_confidence = bridge->config.max_confidence;
    bridge->security_effects.enforced_uncertainty_floor = bridge->config.min_uncertainty;
    bridge->security_effects.accepting_new_beliefs = true;

    NIMCP_LOGGING_INFO("Created security-epistemic bridge");
    return bridge;
}

void security_epist_bridge_destroy(security_epist_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        security_epist_disconnect_bio_async(bridge);
    }

    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (internal) {
        nimcp_free(internal);
    }

    if (bridge->audit_log) {
        nimcp_free(bridge->audit_log);
    }

    if (bridge->confidence_history) {
        nimcp_free(bridge->confidence_history);
    }

    if (bridge->beliefs) {
        nimcp_free(bridge->beliefs);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed security-epistemic bridge");
}

int security_epist_bridge_reset(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    memset(&bridge->stats, 0, sizeof(security_epist_stats_t));
    memset(&bridge->security_effects, 0, sizeof(security_to_epist_effects_t));
    memset(&bridge->epist_effects, 0, sizeof(epist_to_security_effects_t));

    bridge->state.state = SEC_EPIST_STATE_IDLE;
    bridge->state.last_validation = 0;
    bridge->state.last_verification = 0;
    bridge->state.last_attack_check = 0;
    bridge->state.active_sessions = 0;
    bridge->state.pending_validations = 0;

    bridge->num_beliefs = 0;
    bridge->history_head = 0;
    bridge->history_count = 0;
    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    bridge->security_effects.enforced_min_confidence = bridge->config.min_confidence;
    bridge->security_effects.enforced_max_confidence = bridge->config.max_confidence;
    bridge->security_effects.enforced_uncertainty_floor = bridge->config.min_uncertainty;
    bridge->security_effects.accepting_new_beliefs = true;

    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (internal) {
        internal->source_count = 0;
        internal->attack_history_head = 0;
        internal->attack_history_count = 0;
        internal->last_confidence = 0.5f;
        internal->last_confidence_time = get_timestamp_us();
    }

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Reset security-epistemic bridge");
    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int security_epist_connect_filter(
    security_epist_bridge_t* bridge,
    epistemic_filter_t epistemic_filter
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(epistemic_filter, NIMCP_ERROR_NULL_POINTER, "epistemic_filter is NULL");

    BRIDGE_LOCK(bridge);
    bridge->epistemic_filter = epistemic_filter;
    bridge->epistemic_connected = true;
    bridge->state.epistemic_connected = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected epistemic filter to security bridge");
    return 0;
}

int security_epist_connect_bbb(
    security_epist_bridge_t* bridge,
    bbb_system_t bbb
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb, NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

    BRIDGE_LOCK(bridge);
    bridge->bbb = bbb;
    bridge->bbb_connected = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected BBB to security-epistemic bridge");
    return 0;
}

int security_epist_disconnect_all(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->epistemic_filter = NULL;
    bridge->epistemic_connected = false;
    bridge->state.epistemic_connected = false;
    bridge->bbb = NULL;
    bridge->bbb_connected = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Disconnected all from security-epistemic bridge");
    return 0;
}

bool security_epist_is_connected(const security_epist_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->epistemic_connected;
}

/* ============================================================================
 * Confidence Validation Functions
 * ============================================================================ */

bool security_epist_validate_confidence(
    security_epist_bridge_t* bridge,
    float confidence,
    uint64_t belief_id,
    security_epist_conf_status_t* status_out
)
{
    if (!bridge) {
        if (status_out) *status_out = SEC_EPIST_CONF_VALID;
        return false;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_EPIST_STATE_VALIDATING;
    bridge->stats.total_confidence_checks++;

    security_epist_conf_status_t status = SEC_EPIST_CONF_VALID;
    bool is_valid = true;

    if (confidence < bridge->config.min_confidence) {
        status = SEC_EPIST_CONF_TOO_LOW;
        is_valid = false;
    } else if (confidence > bridge->config.max_confidence) {
        status = SEC_EPIST_CONF_TOO_HIGH;
        is_valid = false;
    }

    if (is_valid && bridge->config.max_confidence_rate > 0.0f) {
        float rate = calculate_confidence_rate(bridge, confidence);
        if (rate > bridge->config.max_confidence_rate) {
            status = SEC_EPIST_CONF_RATE_ANOMALY;
            is_valid = false;
        }
    }

    if (is_valid && belief_id != 0) {
        int idx = find_belief_index(bridge, belief_id);
        if (idx >= 0) {
            security_epist_belief_t* belief = &bridge->beliefs[idx];
            float expected = belief->current_confidence;
            float diff = fabsf(confidence - expected);
            if (diff > 0.5f) {
                status = SEC_EPIST_CONF_SOURCE_MISMATCH;
                is_valid = false;
            }
        }
    }

    record_confidence_history(bridge, confidence);

    if (is_valid) {
        bridge->stats.confidence_valid++;
        bridge->stats.mean_confidence =
            (bridge->stats.mean_confidence * (bridge->stats.total_confidence_checks - 1) +
             confidence) / bridge->stats.total_confidence_checks;
    } else {
        bridge->stats.confidence_rejected++;
        bridge->security_effects.confidence_rejections++;
    }

    if (status_out) *status_out = status;

    bridge->state.state = SEC_EPIST_STATE_IDLE;
    bridge->state.last_validation = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return is_valid;
}

int security_epist_correct_confidence(
    security_epist_bridge_t* bridge,
    float confidence,
    float* corrected_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(corrected_out, NIMCP_ERROR_NULL_POINTER, "corrected_out is NULL");

    BRIDGE_LOCK(bridge);

    float corrected = nimcp_clampf(
        confidence,
        bridge->config.min_confidence,
        bridge->config.max_confidence
    );

    if (fabsf(corrected - confidence) > 0.001f) {
        bridge->stats.confidence_corrected++;
        bridge->security_effects.confidence_corrections++;

        if (bridge->config.enable_audit) {
            BRIDGE_UNLOCK(bridge);
            security_epist_audit_event(bridge, SEC_EPIST_AUDIT_CORRECTION,
                                       0, confidence, corrected, true,
                                       "Confidence auto-corrected");
            BRIDGE_LOCK(bridge);
        }
    }

    *corrected_out = corrected;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_epist_set_confidence_bounds(
    security_epist_bridge_t* bridge,
    float min_confidence,
    float max_confidence
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    NIMCP_CHECK_THROW(min_confidence >= 0.0f && max_confidence <= 1.0f &&
                     min_confidence < max_confidence,
                     NIMCP_ERROR_INVALID_PARAMETER, "invalid confidence bounds");

    BRIDGE_LOCK(bridge);

    bridge->config.min_confidence = min_confidence;
    bridge->config.max_confidence = max_confidence;
    bridge->security_effects.enforced_min_confidence = min_confidence;
    bridge->security_effects.enforced_max_confidence = max_confidence;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Belief Verification Functions
 * ============================================================================ */

bool security_epist_verify_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    security_epist_belief_status_t* status_out
)
{
    if (!bridge) {
        if (status_out) *status_out = SEC_EPIST_BELIEF_VALID;
        return false;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_EPIST_STATE_VERIFYING;
    bridge->stats.total_belief_checks++;

    security_epist_belief_status_t status = SEC_EPIST_BELIEF_VALID;
    bool is_valid = true;

    int idx = find_belief_index(bridge, belief_id);
    if (idx < 0) {
        is_valid = true;
        goto verify_done;
    }

    security_epist_belief_t* belief = &bridge->beliefs[idx];

    if (content_hash != 0 && content_hash != belief->hash) {
        status = SEC_EPIST_BELIEF_CORRUPTED;
        is_valid = false;
        bridge->stats.beliefs_corrupted++;
        goto verify_done;
    }

    if (belief->is_locked) {
        uint64_t now = get_timestamp_us();
        uint64_t age = now - belief->last_verified;
        uint64_t max_age_us = 3600ULL * 1000000ULL;
        if (age > max_age_us) {
            status = SEC_EPIST_BELIEF_EXPIRED;
            is_valid = false;
            goto verify_done;
        }
    }

    belief->last_verified = get_timestamp_us();
    belief->verification_count++;

verify_done:
    if (is_valid) {
        bridge->stats.beliefs_verified++;
        bridge->security_effects.beliefs_verified++;
    } else {
        bridge->stats.beliefs_rejected++;
        bridge->security_effects.beliefs_rejected++;
    }

    if (status_out) *status_out = status;

    bridge->state.state = SEC_EPIST_STATE_IDLE;
    bridge->state.last_verification = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return is_valid;
}

int security_epist_register_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    float initial_confidence
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    if (!bridge->security_effects.accepting_new_beliefs) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_PERMISSION_DENIED, "not accepting new beliefs");
    }

    int idx = find_belief_index(bridge, belief_id);
    if (idx >= 0) {
        bridge->beliefs[idx].hash = content_hash;
        bridge->beliefs[idx].current_confidence = initial_confidence;
        bridge->beliefs[idx].last_verified = get_timestamp_us();
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    if (bridge->num_beliefs >= SEC_EPIST_MAX_BELIEFS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "max beliefs reached");
    }

    security_epist_belief_t* belief = &bridge->beliefs[bridge->num_beliefs++];
    belief->belief_id = belief_id;
    belief->creation_time = get_timestamp_us();
    belief->last_verified = belief->creation_time;
    belief->verification_count = 0;
    belief->initial_confidence = initial_confidence;
    belief->current_confidence = initial_confidence;
    belief->hash = content_hash;
    belief->evidence_count = 0;
    belief->is_locked = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_update_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id,
    float new_confidence,
    uint64_t new_hash
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    int idx = find_belief_index(bridge, belief_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "belief_id not found");
    }

    security_epist_belief_t* belief = &bridge->beliefs[idx];

    if (belief->is_locked) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_PERMISSION_DENIED, "belief is locked");
    }

    belief->current_confidence = nimcp_clampf(
        new_confidence,
        bridge->config.min_confidence,
        bridge->config.max_confidence
    );

    if (new_hash != 0) {
        belief->hash = new_hash;
    }

    belief->last_verified = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_lock_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    int idx = find_belief_index(bridge, belief_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "belief_id not found");
    }

    bridge->beliefs[idx].is_locked = true;
    bridge->security_effects.beliefs_locked++;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_revoke_belief(
    security_epist_bridge_t* bridge,
    uint64_t belief_id
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    int idx = find_belief_index(bridge, belief_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "belief_id not found");
    }

    for (uint32_t i = (uint32_t)idx; i < bridge->num_beliefs - 1; i++) {
        bridge->beliefs[i] = bridge->beliefs[i + 1];
    }
    bridge->num_beliefs--;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Uncertainty Enforcement Functions
 * ============================================================================ */

int security_epist_enforce_uncertainty(
    security_epist_bridge_t* bridge,
    float uncertainty,
    bool* is_valid_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(is_valid_out, NIMCP_ERROR_NULL_POINTER, "is_valid_out is NULL");

    BRIDGE_LOCK(bridge);

    bridge->stats.uncertainty_checks++;

    bool is_valid = true;

    if (uncertainty < bridge->config.min_uncertainty) {
        is_valid = false;
        bridge->stats.uncertainty_violations++;
        bridge->security_effects.uncertainty_violations++;
    }

    if (uncertainty > bridge->config.max_uncertainty) {
        is_valid = false;
        bridge->stats.uncertainty_violations++;
    }

    *is_valid_out = is_valid;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_adjust_uncertainty(
    security_epist_bridge_t* bridge,
    float uncertainty,
    float* adjusted_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(adjusted_out, NIMCP_ERROR_NULL_POINTER, "adjusted_out is NULL");

    BRIDGE_LOCK(bridge);

    float adjusted = nimcp_clampf(
        uncertainty,
        bridge->config.min_uncertainty,
        bridge->config.max_uncertainty
    );

    if (fabsf(adjusted - uncertainty) > 0.001f) {
        bridge->stats.uncertainty_corrections++;
    }

    *adjusted_out = adjusted;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_set_uncertainty_bounds(
    security_epist_bridge_t* bridge,
    float min_uncertainty,
    float max_uncertainty
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    NIMCP_CHECK_THROW(min_uncertainty >= 0.0f && max_uncertainty <= 1.0f &&
                     min_uncertainty <= max_uncertainty,
                     NIMCP_ERROR_INVALID_PARAMETER, "invalid uncertainty bounds");

    BRIDGE_LOCK(bridge);

    bridge->config.min_uncertainty = min_uncertainty;
    bridge->config.max_uncertainty = max_uncertainty;
    bridge->security_effects.enforced_uncertainty_floor = min_uncertainty;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Evidence Validation Functions
 * ============================================================================ */

bool security_epist_validate_evidence(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    security_epist_evidence_status_t* status_out
)
{
    if (!bridge || !chain) {
        if (status_out) *status_out = SEC_EPIST_EVIDENCE_VALID;
        return false;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_EPIST_STATE_VERIFYING;
    bridge->stats.evidence_chains_checked++;

    security_epist_evidence_status_t status = SEC_EPIST_EVIDENCE_VALID;
    bool is_valid = true;

    if (chain->link_count == 0) {
        status = SEC_EPIST_EVIDENCE_BROKEN;
        is_valid = false;
        goto evidence_done;
    }

    if (bridge->config.detect_circular_evidence) {
        for (uint32_t i = 0; i < chain->link_count; i++) {
            for (uint32_t j = i + 1; j < chain->link_count; j++) {
                if (chain->links[i].evidence_id == chain->links[j].evidence_id) {
                    status = SEC_EPIST_EVIDENCE_CIRCULAR;
                    is_valid = false;
                    bridge->stats.circular_evidence_detected++;
                    goto evidence_done;
                }
            }
        }
    }

    uint64_t now = get_timestamp_us();
    uint64_t max_age_us = (uint64_t)bridge->config.max_evidence_age_s * 1000000ULL;
    for (uint32_t i = 0; i < chain->link_count; i++) {
        if (now - chain->links[i].timestamp > max_age_us) {
            status = SEC_EPIST_EVIDENCE_EXPIRED;
            is_valid = false;
            goto evidence_done;
        }
    }

    for (uint32_t i = 0; i < chain->link_count; i++) {
        if (chain->links[i].source_reliability < bridge->config.min_source_reliability) {
            status = SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED;
            is_valid = false;
            goto evidence_done;
        }
    }

    if (chain->independent_paths < bridge->config.min_independent_sources) {
        is_valid = false;
    }

evidence_done:
    if (is_valid) {
        bridge->stats.evidence_chains_valid++;
        bridge->security_effects.evidence_chains_validated++;
    } else {
        bridge->stats.evidence_chains_rejected++;
        bridge->security_effects.evidence_chains_rejected++;
    }

    if (status_out) *status_out = status;

    bridge->state.state = SEC_EPIST_STATE_IDLE;

    BRIDGE_UNLOCK(bridge);

    return is_valid;
}

bool security_epist_check_circular_evidence(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain
)
{
    if (!bridge || !chain) {
        return false;
    }

    for (uint32_t i = 0; i < chain->link_count; i++) {
        for (uint32_t j = i + 1; j < chain->link_count; j++) {
            if (chain->links[i].evidence_id == chain->links[j].evidence_id) {
                return true;
            }
        }
    }

    return false;
}

int security_epist_calculate_chain_reliability(
    security_epist_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    float* reliability_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(chain, NIMCP_ERROR_NULL_POINTER, "chain is NULL");
    NIMCP_CHECK_THROW(reliability_out, NIMCP_ERROR_NULL_POINTER, "reliability_out is NULL");

    if (chain->link_count == 0) {
        *reliability_out = 0.0f;
        return 0;
    }

    float product = 1.0f;
    for (uint32_t i = 0; i < chain->link_count; i++) {
        product *= chain->links[i].source_reliability;
    }

    *reliability_out = product;

    return 0;
}

int security_epist_register_source(
    security_epist_bridge_t* bridge,
    uint64_t source_id,
    float reliability
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    int idx = find_source_index(internal, source_id);
    if (idx >= 0) {
        internal->sources[idx].reliability = reliability;
        internal->sources[idx].last_update = get_timestamp_us();
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    if (internal->source_count >= MAX_SOURCES) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "max sources reached");
    }

    source_entry_t* source = &internal->sources[internal->source_count++];
    source->source_id = source_id;
    source->reliability = nimcp_clampf(reliability, 0.0f, 1.0f);
    source->correct_count = 0;
    source->incorrect_count = 0;
    source->last_update = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_update_source(
    security_epist_bridge_t* bridge,
    uint64_t source_id,
    bool correct
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    int idx = find_source_index(internal, source_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "source_id not found");
    }

    source_entry_t* source = &internal->sources[idx];
    if (correct) {
        source->correct_count++;
    } else {
        source->incorrect_count++;
    }

    uint32_t total = source->correct_count + source->incorrect_count;
    if (total > 0) {
        source->reliability = (float)source->correct_count / (float)total;
    }
    source->last_update = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Attack Detection Functions
 * ============================================================================ */

bool security_epist_detect_attack(
    security_epist_bridge_t* bridge,
    security_epist_attack_t* attack_out,
    float* severity_out,
    char* details_out,
    size_t details_size
)
{
    if (!bridge) {
        return false;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_EPIST_STATE_DETECTING;
    bridge->stats.attack_checks++;

    security_epist_attack_t attack = SEC_EPIST_ATTACK_NONE;
    float severity = 0.0f;

    float variance = calculate_confidence_variance(bridge);
    if (variance > 0.1f) {
        if (bridge->stats.mean_confidence > 0.8f) {
            attack = SEC_EPIST_ATTACK_CONFIDENCE_INFLATE;
            severity = variance;
        } else if (bridge->stats.mean_confidence < 0.2f) {
            attack = SEC_EPIST_ATTACK_CONFIDENCE_DEFLATE;
            severity = variance;
        }
    }

    if (attack == SEC_EPIST_ATTACK_NONE &&
        bridge->stats.beliefs_corrupted > 0) {
        attack = SEC_EPIST_ATTACK_BELIEF_CORRUPT;
        severity = 0.5f + 0.1f * (float)bridge->stats.beliefs_corrupted;
        if (severity > 1.0f) severity = 1.0f;
    }

    if (attack == SEC_EPIST_ATTACK_NONE &&
        bridge->stats.circular_evidence_detected > 0) {
        attack = SEC_EPIST_ATTACK_CIRCULAR_EVIDENCE;
        severity = 0.6f;
    }

    bool detected = (attack != SEC_EPIST_ATTACK_NONE &&
                     severity >= bridge->config.attack_threshold);

    if (detected) {
        bridge->stats.attacks_detected++;
        bridge->security_effects.attack_mode_active = true;
        bridge->security_effects.active_attack = attack;
        bridge->security_effects.attack_severity = severity;

        bridge->stats.mean_attack_severity =
            (bridge->stats.mean_attack_severity * (bridge->stats.attacks_detected - 1) +
             severity) / bridge->stats.attacks_detected;

        if (bridge->config.enable_audit) {
            BRIDGE_UNLOCK(bridge);
            security_epist_audit_event(bridge, SEC_EPIST_AUDIT_ATTACK,
                                       0, 0.0f, 0.0f, false,
                                       security_epist_attack_name(attack));
            BRIDGE_LOCK(bridge);
        }
    }

    if (attack_out) *attack_out = attack;
    if (severity_out) *severity_out = severity;
    if (details_out && details_size > 0) {
        if (attack != SEC_EPIST_ATTACK_NONE) {
            snprintf(details_out, details_size,
                     "Detected %s with severity %.2f",
                     security_epist_attack_name(attack), severity);
        } else {
            details_out[0] = '\0';
        }
    }

    bridge->state.state = SEC_EPIST_STATE_IDLE;
    bridge->state.last_attack_check = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return detected;
}

const char* security_epist_get_attack_signature(security_epist_attack_t attack_type)
{
    switch (attack_type) {
        case SEC_EPIST_ATTACK_CONFIDENCE_INFLATE:
            return "Pattern: Rapid confidence increases without evidence";
        case SEC_EPIST_ATTACK_CONFIDENCE_DEFLATE:
            return "Pattern: Systematic confidence reduction";
        case SEC_EPIST_ATTACK_BELIEF_INJECT:
            return "Pattern: New beliefs from untrusted sources";
        case SEC_EPIST_ATTACK_BELIEF_CORRUPT:
            return "Pattern: Hash mismatch on verified beliefs";
        case SEC_EPIST_ATTACK_UNCERTAINTY_EXPLOIT:
            return "Pattern: Uncertainty values at edge cases";
        case SEC_EPIST_ATTACK_EVIDENCE_TAMPER:
            return "Pattern: Evidence chain modifications";
        case SEC_EPIST_ATTACK_EVIDENCE_FORGE:
            return "Pattern: Evidence with unknown sources";
        case SEC_EPIST_ATTACK_BIAS_AMPLIFY:
            return "Pattern: Confirmation bias reinforcement";
        case SEC_EPIST_ATTACK_SOURCE_POISON:
            return "Pattern: Coordinated source reliability changes";
        case SEC_EPIST_ATTACK_CIRCULAR_EVIDENCE:
            return "Pattern: Self-referential evidence chains";
        case SEC_EPIST_ATTACK_SYBIL:
            return "Pattern: Multiple sources with same origin";
        default:
            return "No attack signature";
    }
}

int security_epist_report_false_positive(
    security_epist_bridge_t* bridge,
    security_epist_attack_t attack_type
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->stats.false_positives++;

    if (bridge->security_effects.active_attack == attack_type) {
        bridge->security_effects.attack_mode_active = false;
        bridge->security_effects.active_attack = SEC_EPIST_ATTACK_NONE;
        bridge->security_effects.attack_severity = 0.0f;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update Functions
 * ============================================================================ */

int security_epist_bridge_update(
    security_epist_bridge_t* bridge,
    uint64_t delta_ms
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge_base_record_update(&bridge->base);

    security_epist_gather_epist_effects(bridge);
    security_epist_apply_security_effects(bridge);

    uint64_t now = get_timestamp_us();
    if (bridge->config.enable_attack_detection) {
        uint64_t check_interval = (uint64_t)bridge->config.attack_window_ms * 1000ULL;
        if (now - bridge->state.last_attack_check > check_interval) {
            security_epist_attack_t attack;
            float severity;
            BRIDGE_UNLOCK(bridge);
            security_epist_detect_attack(bridge, &attack, &severity, NULL, 0);
            BRIDGE_LOCK(bridge);
        }
    }

    bridge->security_effects.validation_latency_ms = (float)delta_ms * 0.1f;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_apply_security_effects(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bridge->security_effects.enforced_min_confidence = bridge->config.min_confidence;
    bridge->security_effects.enforced_max_confidence = bridge->config.max_confidence;
    bridge->security_effects.enforced_uncertainty_floor = bridge->config.min_uncertainty;

    bridge->security_effects.min_chain_reliability = bridge->config.min_source_reliability;

    float overhead = 0.0f;
    if (bridge->config.enable_confidence_validation) overhead += 0.01f;
    if (bridge->config.enable_belief_verification) overhead += 0.02f;
    if (bridge->config.enable_evidence_validation) overhead += 0.02f;
    if (bridge->config.enable_attack_detection) overhead += 0.03f;

    bridge->security_effects.throughput_reduction =
        overhead * bridge->config.security_sensitivity;
    if (bridge->security_effects.throughput_reduction > 0.5f) {
        bridge->security_effects.throughput_reduction = 0.5f;
    }

    return 0;
}

int security_epist_gather_epist_effects(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->history_count > 0) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < bridge->history_count; i++) {
            sum += bridge->confidence_history[i];
        }
        bridge->epist_effects.average_confidence = sum / (float)bridge->history_count;
        bridge->epist_effects.confidence_variance = calculate_confidence_variance(bridge);
    }

    bridge->epist_effects.active_beliefs = bridge->num_beliefs;

    security_epist_internal_t* internal = (security_epist_internal_t*)bridge->base.system_b;
    if (internal && internal->source_count > 0) {
        float sum = 0.0f;
        uint32_t trusted = 0;
        uint32_t untrusted = 0;
        for (uint32_t i = 0; i < internal->source_count; i++) {
            sum += internal->sources[i].reliability;
            if (internal->sources[i].reliability >= 0.5f) {
                trusted++;
            } else {
                untrusted++;
            }
        }
        bridge->epist_effects.source_trust_average = sum / (float)internal->source_count;
        bridge->epist_effects.trusted_sources = trusted;
        bridge->epist_effects.untrusted_sources = untrusted;
    }

    bridge->epist_effects.epistemic_health = 1.0f -
        bridge->security_effects.throughput_reduction;
    if (bridge->security_effects.attack_mode_active) {
        bridge->epist_effects.epistemic_health *= 0.5f;
    }

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int security_epist_get_security_effects(
    const security_epist_bridge_t* bridge,
    security_to_epist_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->security_effects;
    return 0;
}

int security_epist_get_epist_effects(
    const security_epist_bridge_t* bridge,
    epist_to_security_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->epist_effects;
    return 0;
}

int security_epist_get_state(
    const security_epist_bridge_t* bridge,
    security_epist_state_info_t* state_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state_out, NIMCP_ERROR_NULL_POINTER, "state_out is NULL");

    *state_out = bridge->state;
    return 0;
}

int security_epist_get_stats(
    const security_epist_bridge_t* bridge,
    security_epist_stats_t* stats_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats_out, NIMCP_ERROR_NULL_POINTER, "stats_out is NULL");

    *stats_out = bridge->stats;
    return 0;
}

int security_epist_reset_stats(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_epist_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Audit Functions
 * ============================================================================ */

int security_epist_audit_event(
    security_epist_bridge_t* bridge,
    security_epist_audit_type_t type,
    uint64_t belief_id,
    float original_confidence,
    float corrected_confidence,
    bool success,
    const char* details
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->config.enable_audit && bridge->audit_log,
                     NIMCP_ERROR_INVALID_STATE, "audit not enabled or audit_log is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_EPIST_STATE_AUDITING;

    security_epist_audit_entry_t* entry = &bridge->audit_log[bridge->audit_log_head];

    entry->timestamp = get_timestamp_us();
    entry->type = type;
    entry->belief_id = belief_id;
    entry->original_confidence = original_confidence;
    entry->corrected_confidence = corrected_confidence;
    entry->attack_type = SEC_EPIST_ATTACK_NONE;
    entry->success = success;

    if (details) {
        strncpy(entry->details, details, sizeof(entry->details) - 1);
        entry->details[sizeof(entry->details) - 1] = '\0';
    } else {
        entry->details[0] = '\0';
    }

    bridge->audit_log_head = (bridge->audit_log_head + 1) % SEC_EPIST_MAX_AUDIT_ENTRIES;
    if (bridge->audit_log_count < SEC_EPIST_MAX_AUDIT_ENTRIES) {
        bridge->audit_log_count++;
    }

    bridge->stats.audit_entries++;
    if (!success) {
        bridge->stats.audit_alerts++;
    }

    bridge->state.state = SEC_EPIST_STATE_IDLE;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_epist_get_audit_log(
    const security_epist_bridge_t* bridge,
    security_epist_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entries_out, NIMCP_ERROR_NULL_POINTER, "entries_out is NULL");
    NIMCP_CHECK_THROW(count_out, NIMCP_ERROR_NULL_POINTER, "count_out is NULL");
    NIMCP_CHECK_THROW(bridge->audit_log, NIMCP_ERROR_INVALID_STATE, "audit_log is NULL");

    size_t to_copy = (bridge->audit_log_count < max_entries) ?
                     bridge->audit_log_count : max_entries;

    for (size_t i = 0; i < to_copy; i++) {
        uint32_t idx = (bridge->audit_log_head + SEC_EPIST_MAX_AUDIT_ENTRIES - 1 - i) %
                       SEC_EPIST_MAX_AUDIT_ENTRIES;
        entries_out[i] = bridge->audit_log[idx];
    }

    *count_out = to_copy;
    return 0;
}

int security_epist_clear_audit_log(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Cleared security-epistemic audit log");
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_epist_connect_bio_async(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_connect_bio_async(&bridge->base);
}

int security_epist_disconnect_bio_async(security_epist_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_epist_is_bio_async_connected(const security_epist_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge_base_is_bio_async_connected(&bridge->base);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_epist_attack_name(security_epist_attack_t attack)
{
    switch (attack) {
        case SEC_EPIST_ATTACK_NONE:              return "NONE";
        case SEC_EPIST_ATTACK_CONFIDENCE_INFLATE: return "CONFIDENCE_INFLATE";
        case SEC_EPIST_ATTACK_CONFIDENCE_DEFLATE: return "CONFIDENCE_DEFLATE";
        case SEC_EPIST_ATTACK_BELIEF_INJECT:     return "BELIEF_INJECT";
        case SEC_EPIST_ATTACK_BELIEF_CORRUPT:    return "BELIEF_CORRUPT";
        case SEC_EPIST_ATTACK_UNCERTAINTY_EXPLOIT: return "UNCERTAINTY_EXPLOIT";
        case SEC_EPIST_ATTACK_EVIDENCE_TAMPER:   return "EVIDENCE_TAMPER";
        case SEC_EPIST_ATTACK_EVIDENCE_FORGE:    return "EVIDENCE_FORGE";
        case SEC_EPIST_ATTACK_BIAS_AMPLIFY:      return "BIAS_AMPLIFY";
        case SEC_EPIST_ATTACK_SOURCE_POISON:     return "SOURCE_POISON";
        case SEC_EPIST_ATTACK_CIRCULAR_EVIDENCE: return "CIRCULAR_EVIDENCE";
        case SEC_EPIST_ATTACK_SYBIL:             return "SYBIL";
        default:                                  return "UNKNOWN";
    }
}

const char* security_epist_conf_status_name(security_epist_conf_status_t status)
{
    switch (status) {
        case SEC_EPIST_CONF_VALID:          return "VALID";
        case SEC_EPIST_CONF_TOO_LOW:        return "TOO_LOW";
        case SEC_EPIST_CONF_TOO_HIGH:       return "TOO_HIGH";
        case SEC_EPIST_CONF_RATE_ANOMALY:   return "RATE_ANOMALY";
        case SEC_EPIST_CONF_SOURCE_MISMATCH: return "SOURCE_MISMATCH";
        case SEC_EPIST_CONF_CALIBRATION_FAIL: return "CALIBRATION_FAIL";
        default:                             return "UNKNOWN";
    }
}

const char* security_epist_belief_status_name(security_epist_belief_status_t status)
{
    switch (status) {
        case SEC_EPIST_BELIEF_VALID:       return "VALID";
        case SEC_EPIST_BELIEF_CORRUPTED:   return "CORRUPTED";
        case SEC_EPIST_BELIEF_UNAUTHORIZED: return "UNAUTHORIZED";
        case SEC_EPIST_BELIEF_CIRCULAR:    return "CIRCULAR";
        case SEC_EPIST_BELIEF_INCONSISTENT: return "INCONSISTENT";
        case SEC_EPIST_BELIEF_EXPIRED:     return "EXPIRED";
        default:                            return "UNKNOWN";
    }
}

const char* security_epist_evidence_status_name(security_epist_evidence_status_t status)
{
    switch (status) {
        case SEC_EPIST_EVIDENCE_VALID:           return "VALID";
        case SEC_EPIST_EVIDENCE_BROKEN:          return "BROKEN";
        case SEC_EPIST_EVIDENCE_CIRCULAR:        return "CIRCULAR";
        case SEC_EPIST_EVIDENCE_FORGED:          return "FORGED";
        case SEC_EPIST_EVIDENCE_TAMPERED:        return "TAMPERED";
        case SEC_EPIST_EVIDENCE_EXPIRED:         return "EXPIRED";
        case SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED: return "SOURCE_UNTRUSTED";
        default:                                  return "UNKNOWN";
    }
}

const char* security_epist_state_name(security_epist_state_t state)
{
    switch (state) {
        case SEC_EPIST_STATE_IDLE:       return "IDLE";
        case SEC_EPIST_STATE_VALIDATING: return "VALIDATING";
        case SEC_EPIST_STATE_VERIFYING:  return "VERIFYING";
        case SEC_EPIST_STATE_DETECTING:  return "DETECTING";
        case SEC_EPIST_STATE_ENFORCING:  return "ENFORCING";
        case SEC_EPIST_STATE_AUDITING:   return "AUDITING";
        case SEC_EPIST_STATE_ERROR:      return "ERROR";
        default:                          return "UNKNOWN";
    }
}

const char* security_epist_audit_type_name(security_epist_audit_type_t audit_type)
{
    switch (audit_type) {
        case SEC_EPIST_AUDIT_CONFIDENCE:  return "CONFIDENCE";
        case SEC_EPIST_AUDIT_BELIEF:      return "BELIEF";
        case SEC_EPIST_AUDIT_UNCERTAINTY: return "UNCERTAINTY";
        case SEC_EPIST_AUDIT_EVIDENCE:    return "EVIDENCE";
        case SEC_EPIST_AUDIT_ATTACK:      return "ATTACK";
        case SEC_EPIST_AUDIT_CORRECTION:  return "CORRECTION";
        case SEC_EPIST_AUDIT_REJECTION:   return "REJECTION";
        default:                           return "UNKNOWN";
    }
}

void security_epist_print_summary(const security_epist_bridge_t* bridge)
{
    if (!bridge) {
        printf("Security-Epistemic Bridge: NULL\n");
        return;
    }

    printf("\n=== Security-Epistemic Bridge Summary ===\n");
    printf("State: %s\n", security_epist_state_name(bridge->state.state));
    printf("Connections:\n");
    printf("  Epistemic Filter: %s\n",
           bridge->epistemic_connected ? "Connected" : "Disconnected");
    printf("  BBB:              %s\n",
           bridge->bbb_connected ? "Connected" : "Disconnected");
    printf("  Bio-Async:        %s\n",
           bridge->base.bio_async_enabled ? "Connected" : "Disconnected");
    printf("Tracked Beliefs:    %u\n", bridge->num_beliefs);
    printf("Audit Log Entries:  %u\n", bridge->audit_log_count);
    printf("Confidence Bounds:  [%.2f, %.2f]\n",
           bridge->config.min_confidence, bridge->config.max_confidence);
    printf("Uncertainty Floor:  %.3f\n", bridge->config.min_uncertainty);
    printf("Attack Mode:        %s\n",
           bridge->security_effects.attack_mode_active ? "ACTIVE" : "Inactive");
    printf("\n");
}

void security_epist_print_stats(const security_epist_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n=== Security-Epistemic Bridge Statistics ===\n");
    printf("Confidence Validation:\n");
    printf("  Total Checks:     %lu\n", (unsigned long)stats->total_confidence_checks);
    printf("  Valid:            %lu\n", (unsigned long)stats->confidence_valid);
    printf("  Rejected:         %lu\n", (unsigned long)stats->confidence_rejected);
    printf("  Corrected:        %lu\n", (unsigned long)stats->confidence_corrected);
    printf("  Mean Confidence:  %.3f\n", stats->mean_confidence);
    printf("\nBelief Verification:\n");
    printf("  Total Checks:     %lu\n", (unsigned long)stats->total_belief_checks);
    printf("  Verified:         %lu\n", (unsigned long)stats->beliefs_verified);
    printf("  Rejected:         %lu\n", (unsigned long)stats->beliefs_rejected);
    printf("  Corrupted:        %lu\n", (unsigned long)stats->beliefs_corrupted);
    printf("\nUncertainty Enforcement:\n");
    printf("  Checks:           %lu\n", (unsigned long)stats->uncertainty_checks);
    printf("  Violations:       %lu\n", (unsigned long)stats->uncertainty_violations);
    printf("  Corrections:      %lu\n", (unsigned long)stats->uncertainty_corrections);
    printf("\nEvidence Validation:\n");
    printf("  Chains Checked:   %lu\n", (unsigned long)stats->evidence_chains_checked);
    printf("  Valid:            %lu\n", (unsigned long)stats->evidence_chains_valid);
    printf("  Rejected:         %lu\n", (unsigned long)stats->evidence_chains_rejected);
    printf("  Circular Found:   %lu\n", (unsigned long)stats->circular_evidence_detected);
    printf("\nAttack Detection:\n");
    printf("  Checks:           %lu\n", (unsigned long)stats->attack_checks);
    printf("  Attacks Found:    %lu\n", (unsigned long)stats->attacks_detected);
    printf("  Blocked:          %lu\n", (unsigned long)stats->attacks_blocked);
    printf("  False Positives:  %lu\n", (unsigned long)stats->false_positives);
    printf("\n");
}
