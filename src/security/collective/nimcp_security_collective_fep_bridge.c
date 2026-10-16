/**
 * @file nimcp_security_collective_fep_bridge.c
 * @brief Implementation of Security-Collective Cognition FEP Bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Implements FEP integration for collective cognition security
 * WHY:  Enables predictive processing approach to Byzantine/Sybil detection
 * HOW:  Maps collective behavior deviations to free energy, uses active inference
 */

#include "security/collective/nimcp_security_collective_fep_bridge.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_collective_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Running average smoothing factor */
#define FEP_SMOOTHING_ALPHA  0.1f

/** Minimum update interval (ms) */
#define MIN_UPDATE_INTERVAL_MS  10

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_consensus_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float deviation
);

static float compute_byzantine_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float confidence
);

static float compute_sybil_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float probability
);

static collective_fep_threat_t classify_threat_level(
    const security_collective_fep_bridge_t* bridge,
    float total_fe
);

static collective_fep_response_t determine_response(
    const security_collective_fep_bridge_t* bridge,
    float total_fe,
    collective_fep_threat_t threat
);

static void update_running_averages(
    security_collective_fep_bridge_t* bridge,
    float fe,
    float surprise
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int security_collective_fep_default_config(security_collective_fep_config_t* config) {
    /* WHAT: Initialize config with sensible defaults */
    /* WHY:  Provide biologically-plausible starting values */
    /* HOW:  Set thresholds based on expected variance in legitimate behavior */

    if (!config) {
        return -1;
    }

    /* FEP thresholds */
    config->byzantine_fe_threshold = COLLECTIVE_FEP_BYZANTINE_THRESHOLD;
    config->sybil_fe_threshold = COLLECTIVE_FEP_ATTACK_THRESHOLD;
    config->consensus_fe_threshold = COLLECTIVE_FEP_SUSPICIOUS_THRESHOLD;
    config->surprise_threshold = 10.0f;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->consensus_deviation_weight = COLLECTIVE_FEP_CONSENSUS_WEIGHT;
    config->byzantine_behavior_weight = COLLECTIVE_FEP_BYZANTINE_WEIGHT;
    config->sybil_indicator_weight = COLLECTIVE_FEP_SYBIL_WEIGHT;

    /* Active inference */
    config->enable_active_inference = true;
    config->quarantine_threshold = COLLECTIVE_FEP_QUARANTINE_THRESHOLD;
    config->restore_threshold = COLLECTIVE_FEP_RESTORE_THRESHOLD;
    config->inference_learning_rate = 0.1f;

    /* Precision learning */
    config->precision_learning_rate = 0.05f;
    config->learn_from_false_positives = true;
    config->learn_from_true_positives = true;

    /* Sensitivity */
    config->fep_sensitivity = 1.0f;
    config->security_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_collective_fep_bridge_t* security_collective_fep_create(
    const security_collective_fep_config_t* config,
    security_collective_bridge_t* security_bridge,
    fep_system_t* fep_system
) {
    /* WHAT: Create and initialize the FEP bridge */
    /* WHY:  Enable predictive processing for collective security */
    /* HOW:  Allocate, initialize base, connect systems */

    if (!security_bridge || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_collective_fep_create: security_bridge or fep_system is NULL");
        NIMCP_LOGGING_ERROR("Security collective FEP: NULL system pointers");
        return NULL;
    }

    security_collective_fep_bridge_t* bridge =
        (security_collective_fep_bridge_t*)nimcp_malloc(sizeof(security_collective_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_collective_fep_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Security collective FEP: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_collective_fep_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_COLLECTIVE_FEP,
                         "security_collective_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_collective_fep_create: bridge_base_init failed");
        NIMCP_LOGGING_ERROR("Security collective FEP: base init failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_collective_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->security_bridge = security_bridge;
    bridge->fep_system = fep_system;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.consensus_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;
    bridge->state.byzantine_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;
    bridge->state.sybil_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;
    bridge->state.last_response = COLLECTIVE_FEP_RESPONSE_NONE;

    NIMCP_LOGGING_INFO("Security collective FEP bridge created");
    return bridge;
}

void security_collective_fep_destroy(security_collective_fep_bridge_t* bridge) {
    /* WHAT: Clean up bridge resources */
    /* WHY:  Prevent memory leaks */
    /* HOW:  Disconnect bio-async, cleanup base, free */

    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        security_collective_fep_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security collective FEP bridge destroyed");
}

int security_collective_fep_reset(security_collective_fep_bridge_t* bridge) {
    /* WHAT: Reset state while preserving connections */
    /* WHY:  Allow bridge reuse */
    /* HOW:  Zero state/stats, reset precision */

    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.inference_count = 0;
    bridge->state.current_free_energy = 0.0f;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.max_free_energy = 0.0f;
    bridge->state.current_surprise = 0.0f;
    bridge->state.avg_surprise = 0.0f;

    /* Reset precision to defaults */
    bridge->state.consensus_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;
    bridge->state.byzantine_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;
    bridge->state.sybil_precision = COLLECTIVE_FEP_DEFAULT_PRECISION;

    bridge->state.last_response = COLLECTIVE_FEP_RESPONSE_NONE;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_security_effects_t));
    memset(&bridge->security_effects, 0, sizeof(security_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_collective_fep_stats_t));

    /* Reset base */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Security collective FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int security_collective_fep_get_config(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int security_collective_fep_set_config(
    security_collective_fep_bridge_t* bridge,
    const security_collective_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Security collective FEP config updated");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int security_collective_fep_compute_effects(security_collective_fep_bridge_t* bridge) {
    /* WHAT: Compute FEP effects from current collective state */
    /* WHY:  Core FEP computation - quantify surprise */
    /* HOW:  Process security state through generative model */

    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->fep_system) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise);

    /* Compute component-wise free energy */
    float consensus_fe = compute_consensus_free_energy(bridge,
        bridge->security_effects.avg_consensus_deviation);
    float byzantine_fe = compute_byzantine_free_energy(bridge,
        (float)bridge->security_effects.byzantine_detections /
        (bridge->state.update_count + 1));
    float sybil_fe = compute_sybil_free_energy(bridge,
        (float)bridge->security_effects.sybil_detections /
        (bridge->state.update_count + 1));

    /* Combine with sensitivity scaling */
    float total_fe = (consensus_fe + byzantine_fe + sybil_fe) *
                     bridge->config.fep_sensitivity;

    /* Update effects */
    bridge->fep_effects.total_free_energy = total_fe;
    bridge->fep_effects.consensus_surprise = consensus_fe;
    bridge->fep_effects.byzantine_surprise = byzantine_fe;
    bridge->fep_effects.sybil_surprise = sybil_fe;

    /* Compute normalized threat severity */
    float max_threshold = bridge->config.sybil_fe_threshold;
    bridge->fep_effects.threat_severity = fminf(total_fe / max_threshold, 1.0f);

    /* Compute probabilities from component FE */
    bridge->fep_effects.byzantine_probability =
        fminf(byzantine_fe / bridge->config.byzantine_fe_threshold, 1.0f);
    bridge->fep_effects.sybil_probability =
        fminf(sybil_fe / bridge->config.sybil_fe_threshold, 1.0f);
    bridge->fep_effects.consensus_confidence =
        1.0f - fminf(consensus_fe / bridge->config.consensus_fe_threshold, 1.0f);

    /* Update precision effects */
    float avg_precision = (bridge->state.consensus_precision +
                           bridge->state.byzantine_precision +
                           bridge->state.sybil_precision) / 3.0f;
    bridge->fep_effects.detection_sensitivity = avg_precision;
    bridge->fep_effects.current_precision = avg_precision;

    /* Classify threat level */
    bridge->fep_effects.threat_level = classify_threat_level(bridge, total_fe);

    /* Determine recommended action via active inference */
    bridge->fep_effects.recommended_action = determine_response(bridge, total_fe,
        bridge->fep_effects.threat_level);

    /* Update state */
    bridge->state.current_free_energy = total_fe;
    if (total_fe > bridge->state.max_free_energy) {
        bridge->state.max_free_energy = total_fe;
    }
    bridge->state.current_surprise = surprise;
    bridge->state.update_count++;

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.max_free_energy = bridge->state.max_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.current_precision = avg_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_collective_fep_update_from_detection(
    security_collective_fep_bridge_t* bridge,
    collective_fep_threat_t detection_type,
    uint32_t agent_id,
    float severity
) {
    /* WHAT: Feed detection result to FEP */
    /* WHY:  Detections are high-surprise observations */
    /* HOW:  Convert to FEP observation, update beliefs */

    if (!bridge) {
        return -1;
    }

    (void)agent_id;  /* Used for targeted precision updates in future */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map detection to prediction error */
    float weight = 1.0f;
    switch (detection_type) {
        case COLLECTIVE_FEP_THREAT_BYZANTINE:
            weight = bridge->config.byzantine_behavior_weight;
            bridge->security_effects.byzantine_detections++;
            bridge->stats.byzantine_detections++;
            break;
        case COLLECTIVE_FEP_THREAT_SYBIL:
            weight = bridge->config.sybil_indicator_weight;
            bridge->security_effects.sybil_detections++;
            bridge->stats.sybil_detections++;
            break;
        case COLLECTIVE_FEP_THREAT_MONITOR:
        case COLLECTIVE_FEP_THREAT_CRITICAL:
            weight = bridge->config.consensus_deviation_weight;
            bridge->security_effects.consensus_violations++;
            bridge->stats.consensus_violations++;
            break;
        default:
            break;
    }

    /* Create observation for FEP */
    float observation[16] = {0};
    observation[0] = severity * weight;
    observation[1] = (float)detection_type / 5.0f;  /* Normalized type */

    fep_process_observation(bridge->fep_system, observation, 2);

    /* Update beliefs based on detection */
    if (bridge->config.learn_from_true_positives) {
        fep_update_precision(bridge->fep_system);
    }
    fep_update_beliefs(bridge->fep_system);

    bridge->stats.threats_detected++;
    bridge->stats.fep_based_decisions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_collective_fep_active_inference(
    security_collective_fep_bridge_t* bridge,
    collective_fep_response_t* response
) {
    /* WHAT: Use active inference for security response */
    /* WHY:  Actions minimize expected free energy */
    /* HOW:  Evaluate policies, select EFE-minimizing action */

    if (!bridge || !response) {
        return -1;
    }

    if (!bridge->config.enable_active_inference) {
        *response = COLLECTIVE_FEP_RESPONSE_NONE;
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy ratio */
    float fe_ratio = bridge->state.current_free_energy /
                     bridge->config.sybil_fe_threshold;

    /* Determine response based on EFE minimization */
    if (fe_ratio >= bridge->config.quarantine_threshold) {
        *response = COLLECTIVE_FEP_RESPONSE_QUARANTINE;
        bridge->stats.quarantine_actions++;
    } else if (fe_ratio >= 0.5f) {
        *response = COLLECTIVE_FEP_RESPONSE_ISOLATE;
    } else if (fe_ratio >= 0.3f) {
        *response = COLLECTIVE_FEP_RESPONSE_WARN;
        bridge->stats.warning_actions++;
    } else if (fe_ratio >= 0.1f) {
        *response = COLLECTIVE_FEP_RESPONSE_OBSERVE;
    } else if (fe_ratio <= bridge->config.restore_threshold &&
               bridge->state.last_response == COLLECTIVE_FEP_RESPONSE_QUARANTINE) {
        *response = COLLECTIVE_FEP_RESPONSE_RESTORE;
        bridge->stats.restore_actions++;
    } else {
        *response = COLLECTIVE_FEP_RESPONSE_NONE;
    }

    bridge->state.last_response = *response;
    bridge->state.inference_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_collective_fep_modulate_precision(
    security_collective_fep_bridge_t* bridge
) {
    /* WHAT: Adapt detection precision */
    /* WHY:  Precision is attention - focus on likely threats */
    /* HOW:  Update based on detection history and FP rate */

    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float lr = bridge->config.precision_learning_rate;
    float total_detections = (float)(bridge->stats.threats_detected + 1);
    float fp_rate = (float)bridge->security_effects.false_positives / total_detections;

    /* Adapt consensus precision based on deviation patterns */
    float consensus_target = COLLECTIVE_FEP_DEFAULT_PRECISION;
    if (bridge->security_effects.avg_consensus_deviation > 0.3f) {
        consensus_target = COLLECTIVE_FEP_MAX_PRECISION * 0.8f;
    } else if (bridge->security_effects.avg_consensus_deviation < 0.1f) {
        consensus_target = COLLECTIVE_FEP_MIN_PRECISION * 2.0f;
    }
    bridge->state.consensus_precision =
        (1.0f - lr) * bridge->state.consensus_precision + lr * consensus_target;

    /* Adapt Byzantine precision based on detection rate */
    float byzantine_rate = (float)bridge->stats.byzantine_detections / total_detections;
    float byzantine_target = COLLECTIVE_FEP_DEFAULT_PRECISION;
    if (byzantine_rate > 0.1f) {
        byzantine_target = COLLECTIVE_FEP_MAX_PRECISION;
    } else if (byzantine_rate < 0.01f && fp_rate > 0.2f) {
        byzantine_target = COLLECTIVE_FEP_MIN_PRECISION;
    }
    bridge->state.byzantine_precision =
        (1.0f - lr) * bridge->state.byzantine_precision + lr * byzantine_target;

    /* Adapt Sybil precision based on detection rate */
    float sybil_rate = (float)bridge->stats.sybil_detections / total_detections;
    float sybil_target = COLLECTIVE_FEP_DEFAULT_PRECISION;
    if (sybil_rate > 0.05f) {
        sybil_target = COLLECTIVE_FEP_MAX_PRECISION;
    } else if (sybil_rate < 0.005f && fp_rate > 0.2f) {
        sybil_target = COLLECTIVE_FEP_MIN_PRECISION;
    }
    bridge->state.sybil_precision =
        (1.0f - lr) * bridge->state.sybil_precision + lr * sybil_target;

    /* Clamp all precisions to valid range */
    bridge->state.consensus_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        fminf(COLLECTIVE_FEP_MAX_PRECISION, bridge->state.consensus_precision));
    bridge->state.byzantine_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        fminf(COLLECTIVE_FEP_MAX_PRECISION, bridge->state.byzantine_precision));
    bridge->state.sybil_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        fminf(COLLECTIVE_FEP_MAX_PRECISION, bridge->state.sybil_precision));

    bridge->stats.precision_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Detection Feedback Implementation
 * ============================================================================ */

int security_collective_fep_report_consensus(
    security_collective_fep_bridge_t* bridge,
    float deviation,
    uint32_t participant_count
) {
    /* WHAT: Feed consensus deviation to FEP */
    /* WHY:  Deviation from expected consensus is surprise */
    /* HOW:  Map to prediction error, update beliefs */

    if (!bridge) {
        return -1;
    }

    (void)participant_count;  /* Future: weight by participant count */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update running average */
    float alpha = FEP_SMOOTHING_ALPHA;
    bridge->security_effects.avg_consensus_deviation =
        (1.0f - alpha) * bridge->security_effects.avg_consensus_deviation +
        alpha * deviation;

    if (deviation > bridge->security_effects.max_consensus_deviation) {
        bridge->security_effects.max_consensus_deviation = deviation;
    }

    /* Update consensus health (inverse of deviation) */
    bridge->security_effects.consensus_health = 1.0f - deviation;

    /* If deviation is significant, record as violation */
    if (deviation > 0.3f) {
        bridge->security_effects.consensus_violations++;
        bridge->stats.consensus_violations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_collective_fep_report_byzantine(
    security_collective_fep_bridge_t* bridge,
    uint32_t agent_id,
    float confidence
) {
    /* WHAT: Feed Byzantine detection to FEP */
    /* WHY:  Byzantine agents generate high-surprise */
    /* HOW:  Map to prediction error with high weight */

    if (!bridge) {
        return -1;
    }

    return security_collective_fep_update_from_detection(
        bridge, COLLECTIVE_FEP_THREAT_BYZANTINE, agent_id, confidence);
}

int security_collective_fep_report_sybil(
    security_collective_fep_bridge_t* bridge,
    uint32_t suspected_count,
    float probability
) {
    /* WHAT: Feed Sybil indicators to FEP */
    /* WHY:  Fake identities violate expected distribution */
    /* HOW:  Map to prediction error with highest weight */

    if (!bridge) {
        return -1;
    }

    (void)suspected_count;  /* Future: scale probability by count */

    return security_collective_fep_update_from_detection(
        bridge, COLLECTIVE_FEP_THREAT_SYBIL, 0, probability);
}

int security_collective_fep_report_false_positive(
    security_collective_fep_bridge_t* bridge,
    collective_fep_threat_t detection_type
) {
    /* WHAT: Indicate false positive detection */
    /* WHY:  Reduce precision to prevent similar FPs */
    /* HOW:  Lower type-specific precision */

    if (!bridge) {
        return -1;
    }

    if (!bridge->config.learn_from_false_positives) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->security_effects.false_positives++;
    bridge->stats.false_positives++;

    /* Reduce precision for the specific detection type */
    float reduction = 0.9f;
    switch (detection_type) {
        case COLLECTIVE_FEP_THREAT_BYZANTINE:
            bridge->state.byzantine_precision *= reduction;
            break;
        case COLLECTIVE_FEP_THREAT_SYBIL:
            bridge->state.sybil_precision *= reduction;
            break;
        default:
            bridge->state.consensus_precision *= reduction;
            break;
    }

    /* Clamp to minimum */
    bridge->state.consensus_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        bridge->state.consensus_precision);
    bridge->state.byzantine_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        bridge->state.byzantine_precision);
    bridge->state.sybil_precision = fmaxf(COLLECTIVE_FEP_MIN_PRECISION,
        bridge->state.sybil_precision);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_collective_fep_get_fep_effects(
    const security_collective_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int security_collective_fep_get_security_effects(
    const security_collective_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->security_effects;
    return 0;
}

int security_collective_fep_get_state(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_state_t* state
) {
    if (!bridge || !state) {
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int security_collective_fep_get_stats(
    const security_collective_fep_bridge_t* bridge,
    security_collective_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float security_collective_fep_get_free_energy(
    const security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->state.current_free_energy;
}

collective_fep_threat_t security_collective_fep_get_threat_level(
    const security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        return COLLECTIVE_FEP_THREAT_NONE;
    }

    return bridge->fep_effects.threat_level;
}

/* ============================================================================
 * Debug/Diagnostic Implementation
 * ============================================================================ */

void security_collective_fep_print_summary(
    const security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        printf("Security Collective FEP Bridge: NULL\n");
        return;
    }

    printf("=== Security Collective FEP Bridge Summary ===\n");
    printf("State: %s\n", bridge->state.active ? "ACTIVE" : "INACTIVE");
    printf("Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("\n");

    printf("Free Energy:\n");
    printf("  Current: %.3f\n", bridge->state.current_free_energy);
    printf("  Average: %.3f\n", bridge->state.avg_free_energy);
    printf("  Maximum: %.3f\n", bridge->state.max_free_energy);
    printf("\n");

    printf("Precision:\n");
    printf("  Consensus:  %.3f\n", bridge->state.consensus_precision);
    printf("  Byzantine:  %.3f\n", bridge->state.byzantine_precision);
    printf("  Sybil:      %.3f\n", bridge->state.sybil_precision);
    printf("\n");

    printf("Threat Assessment:\n");
    printf("  Level: %s\n", collective_fep_threat_to_string(bridge->fep_effects.threat_level));
    printf("  Severity: %.1f%%\n", bridge->fep_effects.threat_severity * 100.0f);
    printf("  Byzantine P: %.1f%%\n", bridge->fep_effects.byzantine_probability * 100.0f);
    printf("  Sybil P: %.1f%%\n", bridge->fep_effects.sybil_probability * 100.0f);
    printf("\n");

    printf("Statistics:\n");
    printf("  Threats detected: %lu\n", (unsigned long)bridge->stats.threats_detected);
    printf("  Byzantine: %lu\n", (unsigned long)bridge->stats.byzantine_detections);
    printf("  Sybil: %lu\n", (unsigned long)bridge->stats.sybil_detections);
    printf("  Consensus violations: %lu\n", (unsigned long)bridge->stats.consensus_violations);
    printf("  False positives: %lu\n", (unsigned long)bridge->stats.false_positives);
    printf("  Quarantine actions: %lu\n", (unsigned long)bridge->stats.quarantine_actions);
    printf("===============================================\n");
}

int security_collective_fep_reset_stats(
    security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(security_collective_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int security_collective_fep_connect_bio_async(
    security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_COLLECTIVE_FEP,
        .module_name = "security_collective_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security collective FEP connected to bio-async");
        return 0;
    }

    NIMCP_LOGGING_WARN("Security collective FEP: bio-async connection failed");
    return -1;
}

int security_collective_fep_disconnect_bio_async(
    security_collective_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security collective FEP disconnected from bio-async");
    return 0;
}

bool security_collective_fep_is_bio_async_connected(
    const security_collective_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int security_collective_fep_process_messages(
    security_collective_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_collective_fep_process_messages: bridge is NULL");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Process up to 16 messages per call using bio_router_process_inbox */
    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, 16);

    return (int)processed;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* collective_fep_threat_to_string(collective_fep_threat_t threat) {
    switch (threat) {
        case COLLECTIVE_FEP_THREAT_NONE:     return "NONE";
        case COLLECTIVE_FEP_THREAT_MONITOR:  return "MONITOR";
        case COLLECTIVE_FEP_THREAT_BYZANTINE: return "BYZANTINE";
        case COLLECTIVE_FEP_THREAT_SYBIL:    return "SYBIL";
        case COLLECTIVE_FEP_THREAT_CRITICAL: return "CRITICAL";
        default:                              return "UNKNOWN";
    }
}

const char* collective_fep_response_to_string(collective_fep_response_t response) {
    switch (response) {
        case COLLECTIVE_FEP_RESPONSE_NONE:      return "NONE";
        case COLLECTIVE_FEP_RESPONSE_OBSERVE:   return "OBSERVE";
        case COLLECTIVE_FEP_RESPONSE_WARN:      return "WARN";
        case COLLECTIVE_FEP_RESPONSE_ISOLATE:   return "ISOLATE";
        case COLLECTIVE_FEP_RESPONSE_QUARANTINE: return "QUARANTINE";
        case COLLECTIVE_FEP_RESPONSE_RESTORE:   return "RESTORE";
        default:                                 return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static float compute_consensus_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float deviation
) {
    /* WHAT: Compute FE from consensus deviation */
    /* WHY:  Deviation is prediction error in consensus space */
    /* HOW:  Scale by precision and weight */

    float precision = bridge->state.consensus_precision;
    float weight = bridge->config.consensus_deviation_weight;

    /* FE = precision * (deviation)^2 * weight */
    return precision * deviation * deviation * weight;
}

static float compute_byzantine_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float detection_rate
) {
    /* WHAT: Compute FE from Byzantine detection rate */
    /* WHY:  Byzantine behavior is high-surprise */
    /* HOW:  Scale by precision and weight */

    float precision = bridge->state.byzantine_precision;
    float weight = bridge->config.byzantine_behavior_weight;

    /* FE = precision * rate * weight (linear for rate-based) */
    return precision * detection_rate * weight *
           bridge->config.byzantine_fe_threshold;
}

static float compute_sybil_free_energy(
    const security_collective_fep_bridge_t* bridge,
    float detection_rate
) {
    /* WHAT: Compute FE from Sybil detection rate */
    /* WHY:  Sybil attacks are extremely surprising */
    /* HOW:  Scale by precision and highest weight */

    float precision = bridge->state.sybil_precision;
    float weight = bridge->config.sybil_indicator_weight;

    /* FE = precision * rate * weight (linear for rate-based) */
    return precision * detection_rate * weight *
           bridge->config.sybil_fe_threshold;
}

static collective_fep_threat_t classify_threat_level(
    const security_collective_fep_bridge_t* bridge,
    float total_fe
) {
    /* WHAT: Classify threat based on total FE */
    /* WHY:  Categorical output for decision making */
    /* HOW:  Compare against thresholds */

    if (total_fe >= bridge->config.sybil_fe_threshold) {
        return COLLECTIVE_FEP_THREAT_CRITICAL;
    } else if (total_fe >= bridge->config.byzantine_fe_threshold) {
        return COLLECTIVE_FEP_THREAT_SYBIL;
    } else if (total_fe >= bridge->config.consensus_fe_threshold) {
        return COLLECTIVE_FEP_THREAT_BYZANTINE;
    } else if (total_fe >= COLLECTIVE_FEP_NORMAL_THRESHOLD) {
        return COLLECTIVE_FEP_THREAT_MONITOR;
    }

    return COLLECTIVE_FEP_THREAT_NONE;
}

static collective_fep_response_t determine_response(
    const security_collective_fep_bridge_t* bridge,
    float total_fe,
    collective_fep_threat_t threat
) {
    /* WHAT: Map threat level to response action */
    /* WHY:  Active inference for security decisions */
    /* HOW:  Use threat classification and FE ratio */

    (void)total_fe;  /* Used in active_inference function */

    switch (threat) {
        case COLLECTIVE_FEP_THREAT_CRITICAL:
            return COLLECTIVE_FEP_RESPONSE_QUARANTINE;
        case COLLECTIVE_FEP_THREAT_SYBIL:
            return COLLECTIVE_FEP_RESPONSE_QUARANTINE;
        case COLLECTIVE_FEP_THREAT_BYZANTINE:
            return COLLECTIVE_FEP_RESPONSE_ISOLATE;
        case COLLECTIVE_FEP_THREAT_MONITOR:
            return COLLECTIVE_FEP_RESPONSE_OBSERVE;
        default:
            return COLLECTIVE_FEP_RESPONSE_NONE;
    }
}

static void update_running_averages(
    security_collective_fep_bridge_t* bridge,
    float fe,
    float surprise
) {
    /* WHAT: Update exponential moving averages */
    /* WHY:  Smooth noisy measurements */
    /* HOW:  EMA with configurable alpha */

    float alpha = FEP_SMOOTHING_ALPHA;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * fe;

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;
}
