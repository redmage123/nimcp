/**
 * @file nimcp_security_epistemic_fep_bridge.c
 * @brief Implementation of Security Epistemic FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implementation of FEP integration for epistemic security
 * WHY:  Map belief corruption, evidence tampering, and confidence manipulation
 *       to free energy for unified detection and active inference restoration
 * HOW:  Compute FE from epistemic state, use prediction errors for detection,
 *       apply active inference for restoration actions
 */

#include "security/epistemic/nimcp_security_epistemic_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_epistemic_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Minimum time between restorations (ms) */
#define MIN_RESTORATION_INTERVAL_MS 1000

/** @brief Smoothing factor for running averages */
#define RUNNING_AVG_ALPHA 0.1f

#include "constants/nimcp_constants.h"
/** @brief Small epsilon for division safety */
#define FEP_EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * String Tables
 * ============================================================================ */

static const char* g_integrity_names[SEC_EPIST_FEP_INTEGRITY_COUNT] = {
    "HEALTHY",
    "SUSPICIOUS",
    "TAMPERED",
    "COMPROMISED"
};

static const char* g_action_names[SEC_EPIST_FEP_ACTION_COUNT] = {
    "NONE",
    "MONITOR",
    "QUARANTINE_BELIEF",
    "QUARANTINE_SOURCE",
    "ROLLBACK",
    "REVERIFY",
    "RESET_CONFIDENCE",
    "REJECT_INPUT"
};

static const char* g_detection_names[SEC_EPIST_FEP_DETECT_COUNT] = {
    "NONE",
    "BELIEF_CORRUPT",
    "EVIDENCE_TAMPER",
    "CONFIDENCE_MANIP",
    "SOURCE_POISON",
    "CIRCULAR_EVIDENCE"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Retrieve monotonic time for timing operations
 * WHY:  Track update intervals and restoration timing
 * HOW:  Use platform time API
 */
static uint64_t get_timestamp_ms(void)
{
    return nimcp_time_monotonic_us() / 1000;
}

/**
 * @brief Compute softmax over action values
 *
 * WHAT: Convert action values to probabilities via softmax
 * WHY:  Stochastic action selection for active inference
 * HOW:  exp(v/T) / sum(exp(v/T))
 */
static void softmax_actions(
    const float* values,
    float* probs,
    uint32_t count,
    float temperature
)
{
    if (!values || !probs || count == 0) {
        return;
    }

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < count; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        probs[i] = expf((values[i] - max_val) / (temperature + FEP_EPSILON));
        sum += probs[i];
    }

    /* Normalize */
    if (sum > FEP_EPSILON) {
        for (uint32_t i = 0; i < count; i++) {
            probs[i] /= sum;
        }
    } else {
        /* Uniform if sum is too small */
        for (uint32_t i = 0; i < count; i++) {
            probs[i] = 1.0f / (float)count;
        }
    }
}

/**
 * @brief Classify integrity level from free energy
 *
 * WHAT: Map FE value to integrity classification
 * WHY:  Determine appropriate security response
 * HOW:  Threshold-based classification
 */
static sec_epist_fep_integrity_t classify_integrity(
    float free_energy,
    const sec_epist_fep_config_t* config
)
{
    if (free_energy < config->healthy_fe_threshold) {
        return SEC_EPIST_FEP_INTEGRITY_HEALTHY;
    } else if (free_energy < config->corruption_fe_threshold) {
        return SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS;
    } else if (free_energy < config->attack_fe_threshold) {
        return SEC_EPIST_FEP_INTEGRITY_TAMPERED;
    } else {
        return SEC_EPIST_FEP_INTEGRITY_COMPROMISED;
    }
}

/**
 * @brief Update running average
 *
 * WHAT: Exponential moving average update
 * WHY:  Smooth metrics over time
 * HOW:  new_avg = alpha * new_val + (1 - alpha) * old_avg
 */
static float update_running_avg(float old_avg, float new_val, float alpha)
{
    return alpha * new_val + (1.0f - alpha) * old_avg;
}

/**
 * @brief Record value in history buffer
 *
 * WHAT: Add value to circular history buffer
 * WHY:  Track recent values for analysis
 * HOW:  Circular buffer with head tracking
 */
static void record_history(
    float* history,
    uint32_t* head,
    uint32_t* count,
    uint32_t max_size,
    float value
)
{
    if (!history || !head || !count) {
        return;
    }

    history[*head] = value;
    *head = (*head + 1) % max_size;
    if (*count < max_size) {
        (*count)++;
    }
}

/**
 * @brief Compute expected free energy for restoration action
 *
 * WHAT: Estimate EFE for a potential restoration action
 * WHY:  Active inference selects actions minimizing EFE
 * HOW:  Model expected FE reduction for each action type
 */
static float compute_action_efe(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t action,
    float current_fe
)
{
    /* Base EFE is current free energy (doing nothing) */
    float efe = current_fe;

    /* Estimate FE reduction based on action type */
    /* More aggressive actions have higher expected reduction */
    /* but also higher ambiguity (uncertainty about outcome) */

    float expected_reduction = 0.0f;
    float ambiguity = 0.0f;

    switch (action) {
        case SEC_EPIST_FEP_ACTION_NONE:
            /* No action = no change */
            expected_reduction = 0.0f;
            ambiguity = 0.0f;
            break;

        case SEC_EPIST_FEP_ACTION_MONITOR:
            /* Monitoring has small benefit, low risk */
            expected_reduction = current_fe * 0.05f;
            ambiguity = 0.1f;
            break;

        case SEC_EPIST_FEP_ACTION_QUARANTINE_BELIEF:
            /* Quarantine has moderate benefit if belief is corrupted */
            expected_reduction = current_fe * 0.3f;
            ambiguity = 0.4f;
            break;

        case SEC_EPIST_FEP_ACTION_QUARANTINE_SOURCE:
            /* Source quarantine has broader impact */
            expected_reduction = current_fe * 0.4f;
            ambiguity = 0.5f;
            break;

        case SEC_EPIST_FEP_ACTION_ROLLBACK:
            /* Rollback has high potential reduction but high uncertainty */
            expected_reduction = current_fe * 0.6f;
            ambiguity = 0.7f;
            break;

        case SEC_EPIST_FEP_ACTION_REVERIFY:
            /* Re-verification clarifies state */
            expected_reduction = current_fe * 0.2f;
            ambiguity = 0.2f;
            break;

        case SEC_EPIST_FEP_ACTION_RESET_CONFIDENCE:
            /* Confidence reset helps with manipulation */
            expected_reduction = current_fe * 0.25f;
            ambiguity = 0.3f;
            break;

        case SEC_EPIST_FEP_ACTION_REJECT_INPUT:
            /* Rejection is aggressive, high impact */
            expected_reduction = current_fe * 0.7f;
            ambiguity = 0.6f;
            break;

        default:
            expected_reduction = 0.0f;
            ambiguity = 1.0f;
            break;
    }

    /* Scale by precision (higher precision = more confident in predictions) */
    float precision_factor = bridge->state.current_precision / SEC_EPIST_FEP_DEFAULT_PRECISION;
    expected_reduction *= precision_factor;

    /* EFE = expected FE after action + ambiguity */
    /* Lower is better (we want low FE and low uncertainty) */
    efe = (current_fe - expected_reduction) + ambiguity * bridge->config.surprise_threshold;

    return efe;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int sec_epist_fep_default_config(sec_epist_fep_config_t* config)
{
    if (!config) {
        return -1;
    }

    /* FEP parameters */
    config->corruption_fe_threshold = SEC_EPIST_FEP_SUSPICIOUS_THRESHOLD;
    config->surprise_threshold = SEC_EPIST_FEP_SURPRISE_MEDIUM;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->healthy_fe_threshold = SEC_EPIST_FEP_HEALTHY_THRESHOLD;
    config->attack_fe_threshold = SEC_EPIST_FEP_ATTACK_THRESHOLD;

    /* Active inference restoration */
    config->enable_active_restoration = true;
    config->action_temperature = 1.0f;
    config->max_restoration_actions = 3;

    /* Belief integrity weights */
    config->belief_corruption_weight = 0.4f;
    config->evidence_tamper_weight = 0.35f;
    config->confidence_manip_weight = 0.25f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    /* Bio-async integration */
    config->enable_bio_async = false;

    return 0;
}

sec_epist_fep_bridge_t* sec_epist_fep_create(
    const sec_epist_fep_config_t* config,
    security_epist_bridge_t* epist_bridge,
    fep_system_t* fep_system
)
{
    /*
     * WHAT: Create and initialize the FEP-epistemic security bridge
     * WHY:  Enable free energy-based epistemic integrity detection
     * HOW:  Allocate bridge, initialize base, connect systems, allocate history
     */

    if (!epist_bridge || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_epist_fep_create: epist_bridge or fep_system is NULL");
        NIMCP_LOGGING_ERROR("sec_epist_fep_create: NULL pointer for required system");
        return NULL;
    }

    /* Allocate bridge */
    sec_epist_fep_bridge_t* bridge = nimcp_malloc(sizeof(sec_epist_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_epist_fep_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("sec_epist_fep_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(sec_epist_fep_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_EPISTEMIC_FEP,
                         "security_epistemic_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "sec_epist_fep_create: bridge_base_init failed");
        NIMCP_LOGGING_ERROR("sec_epist_fep_create: failed to initialize base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_epist_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->fep_system = fep_system;
    bridge->epist_bridge = epist_bridge;
    bridge->base.system_a = fep_system;
    bridge->base.system_a_connected = true;
    bridge->base.system_b = epist_bridge;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = true;

    /* Allocate history buffers */
    bridge->fe_history = nimcp_malloc(SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));
    bridge->surprise_history = nimcp_malloc(SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));

    if (!bridge->fe_history || !bridge->surprise_history) {
        NIMCP_LOGGING_ERROR("sec_epist_fep_create: failed to allocate history buffers");
        if (bridge->fe_history) nimcp_free(bridge->fe_history);
        if (bridge->surprise_history) nimcp_free(bridge->surprise_history);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_epist_fep_create: validation failed");
        return NULL;
    }

    memset(bridge->fe_history, 0, SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));
    memset(bridge->surprise_history, 0, SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SEC_EPIST_FEP_DEFAULT_PRECISION;
    bridge->state.last_integrity = SEC_EPIST_FEP_INTEGRITY_HEALTHY;

    /* Initialize effects */
    bridge->fep_effects.integrity_level = SEC_EPIST_FEP_INTEGRITY_HEALTHY;
    bridge->fep_effects.belief_integrity_score = 1.0f;
    bridge->fep_effects.evidence_chain_score = 1.0f;

    NIMCP_LOGGING_INFO("Security epistemic FEP bridge created successfully");
    return bridge;
}

void sec_epist_fep_destroy(sec_epist_fep_bridge_t* bridge)
{
    /*
     * WHAT: Clean up and destroy the bridge
     * WHY:  Prevent memory leaks and ensure clean shutdown
     * HOW:  Disconnect bio-async, free buffers, cleanup base, free bridge
     */

    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_epist_fep_disconnect_bio_async(bridge);
    }

    /* Free history buffers */
    if (bridge->fe_history) {
        nimcp_free(bridge->fe_history);
        bridge->fe_history = NULL;
    }

    if (bridge->surprise_history) {
        nimcp_free(bridge->surprise_history);
        bridge->surprise_history = NULL;
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security epistemic FEP bridge destroyed");
}

int sec_epist_fep_reset(sec_epist_fep_bridge_t* bridge)
{
    /*
     * WHAT: Reset bridge state while preserving connections
     * WHY:  Allow fresh start without reconnection
     * HOW:  Zero effects, statistics, history; reset precision
     */

    if (!bridge) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(sec_epist_fep_effects_t));
    memset(&bridge->sec_effects, 0, sizeof(fep_security_epist_effects_t));

    /* Reset state (preserve active flag) */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.restoration_count = 0;
    bridge->state.current_precision = SEC_EPIST_FEP_DEFAULT_PRECISION;
    bridge->state.precision_velocity = 0.0f;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_integrity = SEC_EPIST_FEP_INTEGRITY_HEALTHY;
    bridge->state.integrity_transitions = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_epist_fep_stats_t));

    /* Reset history */
    if (bridge->fe_history) {
        memset(bridge->fe_history, 0, SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));
    }
    if (bridge->surprise_history) {
        memset(bridge->surprise_history, 0, SEC_EPIST_FEP_HISTORY_SIZE * sizeof(float));
    }
    bridge->history_head = 0;
    bridge->history_count = 0;

    /* Reset restoration state */
    bridge->pending_action = SEC_EPIST_FEP_ACTION_NONE;
    bridge->last_restoration_time = 0;

    /* Reset base */
    bridge_base_reset_unlocked(&bridge->base);

    /* Restore integrity defaults */
    bridge->fep_effects.integrity_level = SEC_EPIST_FEP_INTEGRITY_HEALTHY;
    bridge->fep_effects.belief_integrity_score = 1.0f;
    bridge->fep_effects.evidence_chain_score = 1.0f;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Security epistemic FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int sec_epist_fep_get_config(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_config_t* config
)
{
    if (!bridge || !config) {
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int sec_epist_fep_set_config(
    sec_epist_fep_bridge_t* bridge,
    const sec_epist_fep_config_t* config
)
{
    if (!bridge || !config) {
        return -1;
    }

    BRIDGE_LOCK(bridge);
    bridge->config = *config;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Security epistemic FEP bridge config updated");
    return 0;
}

/* ============================================================================
 * Compute and Update Implementation
 * ============================================================================ */

int sec_epist_fep_compute_effects(sec_epist_fep_bridge_t* bridge)
{
    /*
     * WHAT: Compute FEP-derived effects on epistemic security
     * WHY:  Free energy provides corruption and integrity indicators
     * HOW:  Get FEP state, compute metrics, classify integrity
     */

    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Record in history */
    record_history(bridge->fe_history, &bridge->history_head, &bridge->history_count,
                   SEC_EPIST_FEP_HISTORY_SIZE, current_fe);
    record_history(bridge->surprise_history, &bridge->history_head, &bridge->history_count,
                   SEC_EPIST_FEP_HISTORY_SIZE, surprise);

    /* Update running averages */
    bridge->state.avg_free_energy = update_running_avg(
        bridge->state.avg_free_energy, current_fe, RUNNING_AVG_ALPHA);
    bridge->state.avg_surprise = update_running_avg(
        bridge->state.avg_surprise, surprise, RUNNING_AVG_ALPHA);
    bridge->state.avg_prediction_error = update_running_avg(
        bridge->state.avg_prediction_error, pred_error, RUNNING_AVG_ALPHA);

    /* Compute corruption score (normalized FE) */
    float corruption_score = current_fe / (bridge->config.attack_fe_threshold + FEP_EPSILON);
    corruption_score = nimcp_clampf(corruption_score, 0.0f, 1.0f);
    bridge->fep_effects.fep_corruption_score = corruption_score;

    /* Compute belief integrity (inverse of corruption) */
    bridge->fep_effects.belief_integrity_score = 1.0f - corruption_score * bridge->config.belief_corruption_weight;

    /* Compute evidence chain score from prediction error */
    float evidence_score = 1.0f - (pred_error / (bridge->config.surprise_threshold + FEP_EPSILON));
    evidence_score = nimcp_clampf(evidence_score, 0.0f, 1.0f);
    bridge->fep_effects.evidence_chain_score = evidence_score;

    /* Compute surprise score */
    bridge->fep_effects.surprise_score = surprise / (bridge->config.surprise_threshold + FEP_EPSILON);
    bridge->fep_effects.surprise_score = nimcp_clampf(bridge->fep_effects.surprise_score, 0.0f, 1.0f);

    /* Compute confidence surprise (how unexpected are confidence changes) */
    /* This would integrate with epistemic bridge confidence history */
    bridge->fep_effects.confidence_surprise = bridge->fep_effects.surprise_score *
                                               bridge->config.confidence_manip_weight;

    /* Precision-adjusted detection threshold */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;
    bridge->fep_effects.adjusted_corruption_threshold =
        bridge->config.corruption_fe_threshold / (bridge->state.current_precision + FEP_EPSILON);

    /* Classify integrity level */
    sec_epist_fep_integrity_t new_integrity = classify_integrity(current_fe, &bridge->config);

    /* Track integrity transitions */
    if (new_integrity != bridge->state.last_integrity) {
        bridge->state.integrity_transitions++;
        bridge->state.last_integrity = new_integrity;
    }
    bridge->fep_effects.integrity_level = new_integrity;

    /* Update integrity statistics */
    switch (new_integrity) {
        case SEC_EPIST_FEP_INTEGRITY_HEALTHY:
            bridge->stats.healthy_states++;
            break;
        case SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS:
            bridge->stats.suspicious_states++;
            break;
        case SEC_EPIST_FEP_INTEGRITY_TAMPERED:
            bridge->stats.tampered_states++;
            break;
        case SEC_EPIST_FEP_INTEGRITY_COMPROMISED:
            bridge->stats.compromised_states++;
            break;
        default:
            break;
    }

    /* Select recommended action via active inference (if enabled) */
    if (bridge->config.enable_active_restoration &&
        new_integrity >= SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS) {

        sec_epist_fep_action_t best_action = SEC_EPIST_FEP_ACTION_NONE;
        float best_efe = current_fe;
        float action_confidence = 0.0f;

        /* Evaluate each action */
        for (int action = SEC_EPIST_FEP_ACTION_NONE; action < SEC_EPIST_FEP_ACTION_COUNT; action++) {
            float efe = compute_action_efe(bridge, (sec_epist_fep_action_t)action, current_fe);
            if (efe < best_efe) {
                best_efe = efe;
                best_action = (sec_epist_fep_action_t)action;
            }
        }

        /* Compute confidence in selected action */
        if (best_action != SEC_EPIST_FEP_ACTION_NONE) {
            action_confidence = (current_fe - best_efe) / (current_fe + FEP_EPSILON);
            action_confidence = nimcp_clampf(action_confidence, 0.0f, 1.0f);
        }

        bridge->fep_effects.recommended_action = best_action;
        bridge->fep_effects.action_confidence = action_confidence;
    } else {
        bridge->fep_effects.recommended_action = SEC_EPIST_FEP_ACTION_NONE;
        bridge->fep_effects.action_confidence = 0.0f;
    }

    /* Update statistics */
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    bridge->state.update_count++;
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_epist_fep_update_from_detection(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_detection_t detection,
    float severity,
    uint64_t belief_id
)
{
    /*
     * WHAT: Update FEP state from security detection event
     * WHY:  Security events inform FEP generative model
     * HOW:  Convert detection to observation, update beliefs/precision
     */

    if (!bridge) {
        return -1;
    }

    (void)belief_id; /* May be used in future for belief-specific updates */

    BRIDGE_LOCK(bridge);

    /* Update detection statistics */
    bridge->state.detection_count++;
    bridge->stats.total_detections++;

    /* Update security effects based on detection type */
    switch (detection) {
        case SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT:
            bridge->sec_effects.beliefs_corrupted++;
            bridge->stats.corruptions_found++;
            break;

        case SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER:
            bridge->sec_effects.evidence_chains_tampered++;
            bridge->stats.tamperings_found++;
            break;

        case SEC_EPIST_FEP_DETECT_CONFIDENCE_MANIP:
            bridge->sec_effects.confidence_manipulations++;
            break;

        case SEC_EPIST_FEP_DETECT_SOURCE_POISON:
            /* Tracked elsewhere */
            break;

        case SEC_EPIST_FEP_DETECT_CIRCULAR_EVIDENCE:
            bridge->sec_effects.evidence_chains_tampered++;
            break;

        case SEC_EPIST_FEP_DETECT_NONE:
            /* Verified successfully */
            bridge->sec_effects.beliefs_verified++;
            break;

        default:
            break;
    }

    /* Update running averages */
    bridge->sec_effects.avg_belief_corruption = update_running_avg(
        bridge->sec_effects.avg_belief_corruption, severity, RUNNING_AVG_ALPHA);

    /* Update FEP if online learning is enabled */
    if (bridge->config.enable_online_learning && detection != SEC_EPIST_FEP_DETECT_NONE) {
        /* Detection events are high-surprise observations */
        /* Increase precision for this type of detection */
        if (severity > 0.5f) {
            fep_update_precision(bridge->fep_system);
        }
    }

    /* Track attack state */
    if (severity > 0.7f) {
        bridge->sec_effects.attack_in_progress = true;
        bridge->sec_effects.attack_severity = severity;
        /* Map detection type to attack type */
        switch (detection) {
            case SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT:
                bridge->sec_effects.current_attack = SEC_EPIST_ATTACK_BELIEF_CORRUPT;
                break;
            case SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER:
                bridge->sec_effects.current_attack = SEC_EPIST_ATTACK_EVIDENCE_TAMPER;
                break;
            case SEC_EPIST_FEP_DETECT_CONFIDENCE_MANIP:
                bridge->sec_effects.current_attack = SEC_EPIST_ATTACK_CONFIDENCE_INFLATE;
                break;
            default:
                bridge->sec_effects.current_attack = SEC_EPIST_ATTACK_NONE;
                break;
        }
    } else if (severity < 0.2f) {
        bridge->sec_effects.attack_in_progress = false;
        bridge->sec_effects.attack_severity = 0.0f;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_epist_fep_apply_precision_modulation(sec_epist_fep_bridge_t* bridge)
{
    /*
     * WHAT: Adapt detection precision based on FEP state
     * WHY:  Higher precision = more sensitive detection when needed
     * HOW:  Adjust precision based on integrity level and detection rate
     */

    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Compute target precision based on integrity level */
    float target_precision = SEC_EPIST_FEP_DEFAULT_PRECISION;

    switch (bridge->fep_effects.integrity_level) {
        case SEC_EPIST_FEP_INTEGRITY_HEALTHY:
            /* Normal precision when healthy */
            target_precision = SEC_EPIST_FEP_DEFAULT_PRECISION;
            break;

        case SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS:
            /* Slightly elevated precision */
            target_precision = SEC_EPIST_FEP_DEFAULT_PRECISION * 1.5f;
            break;

        case SEC_EPIST_FEP_INTEGRITY_TAMPERED:
            /* High precision for active monitoring */
            target_precision = SEC_EPIST_FEP_DEFAULT_PRECISION * 3.0f;
            break;

        case SEC_EPIST_FEP_INTEGRITY_COMPROMISED:
            /* Maximum precision during attack */
            target_precision = SEC_EPIST_FEP_MAX_PRECISION;
            break;

        default:
            break;
    }

    /* Clamp target precision */
    target_precision = nimcp_clampf(target_precision,
                                    SEC_EPIST_FEP_MIN_PRECISION,
                                    SEC_EPIST_FEP_MAX_PRECISION);

    /* Smooth adaptation */
    float old_precision = bridge->state.current_precision;
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * old_precision + alpha * target_precision;

    /* Track precision velocity */
    bridge->state.precision_velocity = bridge->state.current_precision - old_precision;

    /* Update statistics */
    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_epist_fep_verify_belief(
    sec_epist_fep_bridge_t* bridge,
    uint64_t belief_id,
    uint64_t content_hash,
    sec_epist_fep_result_t* result
)
{
    /*
     * WHAT: Verify belief integrity using FEP-enhanced detection
     * WHY:  Combine epistemic verification with FEP analysis
     * HOW:  Run both methods, fuse scores
     */

    if (!bridge || !result) {
        return -1;
    }

    memset(result, 0, sizeof(sec_epist_fep_result_t));

    BRIDGE_LOCK(bridge);

    /* Run standard epistemic verification */
    security_epist_belief_status_t belief_status;
    bool verified = security_epist_verify_belief(
        bridge->epist_bridge, belief_id, content_hash, &belief_status);

    /* Get current FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Fill result structure */
    result->free_energy = current_fe;
    result->surprise = surprise;
    result->prediction_error = pred_error;
    result->affected_belief_id = belief_id;

    /* Compute combined corruption score */
    float fep_score = current_fe / (bridge->config.attack_fe_threshold + FEP_EPSILON);
    fep_score = nimcp_clampf(fep_score, 0.0f, 1.0f);

    float epist_score = 0.0f;
    if (!verified) {
        switch (belief_status) {
            case SEC_EPIST_BELIEF_CORRUPTED:
                epist_score = 0.9f;
                result->detection_type = SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT;
                break;
            case SEC_EPIST_BELIEF_UNAUTHORIZED:
                epist_score = 0.8f;
                result->detection_type = SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT;
                break;
            case SEC_EPIST_BELIEF_CIRCULAR:
                epist_score = 0.6f;
                result->detection_type = SEC_EPIST_FEP_DETECT_CIRCULAR_EVIDENCE;
                break;
            case SEC_EPIST_BELIEF_INCONSISTENT:
                epist_score = 0.7f;
                result->detection_type = SEC_EPIST_FEP_DETECT_BELIEF_CORRUPT;
                break;
            case SEC_EPIST_BELIEF_EXPIRED:
                epist_score = 0.3f;
                result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
                break;
            default:
                epist_score = 0.0f;
                result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
                break;
        }
    } else {
        result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
    }

    /* Fuse scores (FEP-weighted) */
    if (bridge->config.use_fep_scoring) {
        result->corruption_score = 0.4f * fep_score + 0.6f * epist_score;
    } else {
        result->corruption_score = epist_score;
    }

    /* Compute confidence */
    result->confidence = 1.0f - (pred_error / (bridge->config.surprise_threshold + FEP_EPSILON));
    result->confidence = nimcp_clampf(result->confidence, 0.0f, 1.0f);

    /* Classify integrity */
    result->integrity = classify_integrity(current_fe, &bridge->config);

    /* Determine if action is needed */
    result->requires_action = (result->integrity >= SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS) ||
                              (result->corruption_score > 0.5f);

    if (result->requires_action) {
        result->recommended_action = bridge->fep_effects.recommended_action;
    } else {
        result->recommended_action = SEC_EPIST_FEP_ACTION_NONE;
    }

    /* Generate explanation */
    snprintf(result->explanation, sizeof(result->explanation),
             "Belief %lu: integrity=%s, FE=%.2f, corruption=%.2f",
             (unsigned long)belief_id,
             sec_epist_fep_integrity_name(result->integrity),
             current_fe,
             result->corruption_score);

    /* Update statistics */
    bridge->stats.fep_based_detections++;
    if (result->corruption_score > 0.5f) {
        bridge->stats.corruptions_found++;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_epist_fep_validate_evidence(
    sec_epist_fep_bridge_t* bridge,
    const security_epist_evidence_chain_t* chain,
    sec_epist_fep_result_t* result
)
{
    /*
     * WHAT: Validate evidence chain using FEP analysis
     * WHY:  Prediction error reveals chain tampering
     * HOW:  Model expected chain structure, detect deviations
     */

    if (!bridge || !chain || !result) {
        return -1;
    }

    memset(result, 0, sizeof(sec_epist_fep_result_t));

    BRIDGE_LOCK(bridge);

    /* Run standard evidence validation */
    security_epist_evidence_status_t evidence_status;
    bool valid = security_epist_validate_evidence(
        bridge->epist_bridge, chain, &evidence_status);

    /* Get FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    result->free_energy = current_fe;
    result->surprise = surprise;
    result->prediction_error = pred_error;

    /* Map evidence status to detection type */
    float epist_score = 0.0f;
    if (!valid) {
        switch (evidence_status) {
            case SEC_EPIST_EVIDENCE_BROKEN:
                epist_score = 0.7f;
                result->detection_type = SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER;
                break;
            case SEC_EPIST_EVIDENCE_CIRCULAR:
                epist_score = 0.6f;
                result->detection_type = SEC_EPIST_FEP_DETECT_CIRCULAR_EVIDENCE;
                break;
            case SEC_EPIST_EVIDENCE_FORGED:
                epist_score = 0.95f;
                result->detection_type = SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER;
                break;
            case SEC_EPIST_EVIDENCE_TAMPERED:
                epist_score = 0.85f;
                result->detection_type = SEC_EPIST_FEP_DETECT_EVIDENCE_TAMPER;
                break;
            case SEC_EPIST_EVIDENCE_EXPIRED:
                epist_score = 0.3f;
                result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
                break;
            case SEC_EPIST_EVIDENCE_SOURCE_UNTRUSTED:
                epist_score = 0.6f;
                result->detection_type = SEC_EPIST_FEP_DETECT_SOURCE_POISON;
                break;
            default:
                epist_score = 0.0f;
                result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
                break;
        }
    } else {
        result->detection_type = SEC_EPIST_FEP_DETECT_NONE;
    }

    /* FEP-enhanced scoring */
    float fep_score = pred_error / (bridge->config.surprise_threshold + FEP_EPSILON);
    fep_score = nimcp_clampf(fep_score, 0.0f, 1.0f);

    if (bridge->config.use_fep_scoring) {
        result->corruption_score = 0.35f * fep_score + 0.65f * epist_score;
    } else {
        result->corruption_score = epist_score;
    }

    result->confidence = 1.0f - fep_score * 0.5f;
    result->confidence = nimcp_clampf(result->confidence, 0.0f, 1.0f);

    result->integrity = classify_integrity(current_fe, &bridge->config);
    result->requires_action = (result->integrity >= SEC_EPIST_FEP_INTEGRITY_SUSPICIOUS) ||
                              (result->corruption_score > 0.5f);
    result->recommended_action = result->requires_action ?
                                 bridge->fep_effects.recommended_action : SEC_EPIST_FEP_ACTION_NONE;

    snprintf(result->explanation, sizeof(result->explanation),
             "Evidence chain %lu: links=%u, reliability=%.2f, corruption=%.2f",
             (unsigned long)chain->chain_id,
             chain->link_count,
             chain->overall_reliability,
             result->corruption_score);

    if (result->corruption_score > 0.5f) {
        bridge->stats.tamperings_found++;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Active Inference Restoration Implementation
 * ============================================================================ */

int sec_epist_fep_select_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t* action_out,
    float* confidence_out
)
{
    /*
     * WHAT: Select restoration action via active inference
     * WHY:  Choose action minimizing expected free energy
     * HOW:  Evaluate each action's EFE, select via softmax
     */

    if (!bridge || !action_out) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    float current_fe = bridge->state.avg_free_energy;

    /* If integrity is healthy, no action needed */
    if (bridge->fep_effects.integrity_level == SEC_EPIST_FEP_INTEGRITY_HEALTHY) {
        *action_out = SEC_EPIST_FEP_ACTION_NONE;
        if (confidence_out) *confidence_out = 1.0f;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Compute EFE for each action */
    float efe_values[SEC_EPIST_FEP_ACTION_COUNT];
    float action_probs[SEC_EPIST_FEP_ACTION_COUNT];

    for (int i = 0; i < SEC_EPIST_FEP_ACTION_COUNT; i++) {
        efe_values[i] = -compute_action_efe(bridge, (sec_epist_fep_action_t)i, current_fe);
        /* Negate because softmax expects higher = better */
    }

    /* Apply softmax to get action probabilities */
    softmax_actions(efe_values, action_probs, SEC_EPIST_FEP_ACTION_COUNT,
                    bridge->config.action_temperature);

    /* Select action with highest probability (or sample stochastically) */
    float max_prob = 0.0f;
    sec_epist_fep_action_t best_action = SEC_EPIST_FEP_ACTION_NONE;

    for (int i = 0; i < SEC_EPIST_FEP_ACTION_COUNT; i++) {
        if (action_probs[i] > max_prob) {
            max_prob = action_probs[i];
            best_action = (sec_epist_fep_action_t)i;
        }
    }

    *action_out = best_action;
    if (confidence_out) {
        *confidence_out = max_prob;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int sec_epist_fep_execute_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t action,
    uint64_t target_belief_id
)
{
    /*
     * WHAT: Execute the selected restoration action
     * WHY:  Reduce free energy by restoring epistemic integrity
     * HOW:  Dispatch action to epistemic bridge
     */

    if (!bridge) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    uint64_t now = get_timestamp_ms();

    /* Rate limit restorations */
    if (now - bridge->last_restoration_time < MIN_RESTORATION_INTERVAL_MS) {
        BRIDGE_UNLOCK(bridge);
        return -1;  /* Rate limited - too many restorations */
    }

    int result = 0;

    switch (action) {
        case SEC_EPIST_FEP_ACTION_NONE:
            /* Nothing to do */
            break;

        case SEC_EPIST_FEP_ACTION_MONITOR:
            /* Just flag for increased monitoring (no direct action) */
            bridge->pending_action = action;
            break;

        case SEC_EPIST_FEP_ACTION_QUARANTINE_BELIEF:
            if (target_belief_id != 0) {
                result = security_epist_lock_belief(bridge->epist_bridge, target_belief_id);
            }
            break;

        case SEC_EPIST_FEP_ACTION_QUARANTINE_SOURCE:
            /* Would need source ID - for now just track */
            bridge->pending_action = action;
            break;

        case SEC_EPIST_FEP_ACTION_ROLLBACK:
            /* Reset the epistemic bridge */
            result = security_epist_bridge_reset(bridge->epist_bridge);
            break;

        case SEC_EPIST_FEP_ACTION_REVERIFY:
            /* Trigger re-verification of target belief */
            if (target_belief_id != 0) {
                security_epist_belief_status_t status;
                security_epist_verify_belief(bridge->epist_bridge, target_belief_id, 0, &status);
            }
            break;

        case SEC_EPIST_FEP_ACTION_RESET_CONFIDENCE:
            /* Reset confidence bounds to defaults */
            result = security_epist_set_confidence_bounds(
                bridge->epist_bridge,
                SEC_EPIST_DEFAULT_MIN_CONFIDENCE,
                SEC_EPIST_DEFAULT_MAX_CONFIDENCE);
            break;

        case SEC_EPIST_FEP_ACTION_REJECT_INPUT:
            /* Would need to interface with input gate - track for now */
            bridge->pending_action = action;
            break;

        default:
            result = NIMCP_ERROR_INVALID_PARAMETER;
            break;
    }

    if (result == 0) {
        bridge->state.restoration_count++;
        bridge->stats.restorations_attempted++;
        bridge->last_restoration_time = now;
    }

    BRIDGE_UNLOCK(bridge);
    return result;
}

int sec_epist_fep_report_restoration(
    sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_action_t action,
    bool success,
    float fe_reduction
)
{
    /*
     * WHAT: Report restoration outcome for learning
     * WHY:  Update FEP from action outcomes
     * HOW:  Adjust precision based on success, track FE reduction
     */

    if (!bridge) {
        return -1;
    }

    (void)action; /* May use for action-specific learning */

    BRIDGE_LOCK(bridge);

    if (success) {
        bridge->stats.restorations_successful++;

        /* Track FE reduction */
        bridge->stats.avg_restoration_fe_reduction = update_running_avg(
            bridge->stats.avg_restoration_fe_reduction, fe_reduction, RUNNING_AVG_ALPHA);

        /* Increase precision slightly on success */
        if (bridge->config.enable_online_learning) {
            bridge->state.current_precision *= 1.02f;
            bridge->state.current_precision = nimcp_clampf(
                bridge->state.current_precision,
                SEC_EPIST_FEP_MIN_PRECISION,
                SEC_EPIST_FEP_MAX_PRECISION);
        }
    } else {
        /* Decrease precision on failure */
        if (bridge->config.enable_online_learning) {
            bridge->state.current_precision *= 0.95f;
            bridge->state.current_precision = nimcp_clampf(
                bridge->state.current_precision,
                SEC_EPIST_FEP_MIN_PRECISION,
                SEC_EPIST_FEP_MAX_PRECISION);
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sec_epist_fep_get_effects(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_effects_t* effects
)
{
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int sec_epist_fep_get_security_effects(
    const sec_epist_fep_bridge_t* bridge,
    fep_security_epist_effects_t* effects
)
{
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->sec_effects;
    return 0;
}

int sec_epist_fep_get_stats(
    const sec_epist_fep_bridge_t* bridge,
    sec_epist_fep_stats_t* stats
)
{
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float sec_epist_fep_get_corruption_score(const sec_epist_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.fep_corruption_score;
}

sec_epist_fep_integrity_t sec_epist_fep_get_integrity(
    const sec_epist_fep_bridge_t* bridge
)
{
    if (!bridge) {
        return (sec_epist_fep_integrity_t)-1;
    }

    return bridge->fep_effects.integrity_level;
}

float sec_epist_fep_get_free_energy(const sec_epist_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }

    return bridge->state.avg_free_energy;
}

int sec_epist_fep_reset_stats(sec_epist_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1;
    }

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(sec_epist_fep_stats_t));
    bridge->stats.current_precision = bridge->state.current_precision;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int sec_epist_fep_connect_bio_async(sec_epist_fep_bridge_t* bridge)
{
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_EPISTEMIC_FEP,
        .module_name = "security_epistemic_fep",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security epistemic FEP bridge connected to bio-async");
    }

    return 0;
}

int sec_epist_fep_disconnect_bio_async(sec_epist_fep_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security epistemic FEP bridge disconnected from bio-async");
    return 0;
}

bool sec_epist_fep_is_bio_async_connected(const sec_epist_fep_bridge_t* bridge)
{
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* sec_epist_fep_integrity_name(sec_epist_fep_integrity_t level)
{
    if (level >= 0 && level < SEC_EPIST_FEP_INTEGRITY_COUNT) {
        return g_integrity_names[level];
    }
    return "UNKNOWN";
}

const char* sec_epist_fep_action_name(sec_epist_fep_action_t action)
{
    if (action >= 0 && action < SEC_EPIST_FEP_ACTION_COUNT) {
        return g_action_names[action];
    }
    return "UNKNOWN";
}

const char* sec_epist_fep_detection_name(sec_epist_fep_detection_t detection)
{
    if (detection >= 0 && detection < SEC_EPIST_FEP_DETECT_COUNT) {
        return g_detection_names[detection];
    }
    return "UNKNOWN";
}

void sec_epist_fep_print_summary(const sec_epist_fep_bridge_t* bridge)
{
    if (!bridge) {
        printf("Security Epistemic FEP Bridge: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("Security Epistemic FEP Bridge Summary\n");
    printf("========================================\n");
    printf("Active: %s\n", bridge->state.active ? "Yes" : "No");
    printf("Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("Restorations: %lu\n", (unsigned long)bridge->state.restoration_count);
    printf("\nFEP State:\n");
    printf("  Free Energy (avg): %.4f\n", bridge->state.avg_free_energy);
    printf("  Surprise (avg): %.4f\n", bridge->state.avg_surprise);
    printf("  Precision: %.4f\n", bridge->state.current_precision);
    printf("\nIntegrity:\n");
    printf("  Level: %s\n", sec_epist_fep_integrity_name(bridge->fep_effects.integrity_level));
    printf("  Corruption Score: %.4f\n", bridge->fep_effects.fep_corruption_score);
    printf("  Belief Integrity: %.4f\n", bridge->fep_effects.belief_integrity_score);
    printf("  Evidence Chain: %.4f\n", bridge->fep_effects.evidence_chain_score);
    printf("\nRecommended Action: %s (conf: %.2f)\n",
           sec_epist_fep_action_name(bridge->fep_effects.recommended_action),
           bridge->fep_effects.action_confidence);
    printf("Bio-Async: %s\n", bridge->base.bio_async_enabled ? "Connected" : "Not connected");
    printf("========================================\n\n");
}

void sec_epist_fep_print_stats(const sec_epist_fep_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("Security Epistemic FEP Statistics\n");
    printf("========================================\n");
    printf("Detection Stats:\n");
    printf("  Total Detections: %lu\n", (unsigned long)stats->total_detections);
    printf("  FEP-based: %lu\n", (unsigned long)stats->fep_based_detections);
    printf("  Corruptions Found: %lu\n", (unsigned long)stats->corruptions_found);
    printf("  Tamperings Found: %lu\n", (unsigned long)stats->tamperings_found);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positives);
    printf("\nFEP Metrics:\n");
    printf("  Avg Free Energy: %.4f\n", stats->avg_free_energy);
    printf("  Max Free Energy: %.4f\n", stats->max_free_energy);
    printf("  Avg Surprise: %.4f\n", stats->avg_surprise);
    printf("  Max Surprise: %.4f\n", stats->max_surprise);
    printf("\nPrecision:\n");
    printf("  Current: %.4f\n", stats->current_precision);
    printf("  Adaptations: %lu\n", (unsigned long)stats->precision_adaptations);
    printf("\nRestoration Stats:\n");
    printf("  Attempted: %lu\n", (unsigned long)stats->restorations_attempted);
    printf("  Successful: %lu\n", (unsigned long)stats->restorations_successful);
    printf("  Avg FE Reduction: %.4f\n", stats->avg_restoration_fe_reduction);
    printf("\nIntegrity State Distribution:\n");
    printf("  Healthy: %lu\n", (unsigned long)stats->healthy_states);
    printf("  Suspicious: %lu\n", (unsigned long)stats->suspicious_states);
    printf("  Tampered: %lu\n", (unsigned long)stats->tampered_states);
    printf("  Compromised: %lu\n", (unsigned long)stats->compromised_states);
    printf("========================================\n\n");
}
