/**
 * @file nimcp_security_kg_fep_bridge.c
 * @brief Implementation of Security Knowledge Graph FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for knowledge graph security operations
 * WHY:  Malicious queries represent high-surprise deviations from expected patterns
 * HOW:  Map injection detection to free energy, use precision for sensitivity tuning
 *
 * @author NIMCP Development Team
 */

#include "security/knowledge_graph/nimcp_security_kg_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_kg_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_threat_from_fe(float free_energy, const sec_kg_fep_config_t* config);
static sec_kg_fep_threat_level_t classify_threat(float normalized_threat);
static sec_kg_fep_action_t select_action_for_threat(sec_kg_fep_threat_level_t threat);
static float clamp_precision(float precision);
static float compute_detection_sensitivity(float precision);
static void update_running_average(float* avg, float new_value, float alpha);

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

/**
 * WHAT: Provide sensible defaults for FEP-security integration
 * WHY:  Easy initialization with balanced sensitivity
 * HOW:  Set moderate thresholds and enable key features
 */
int sec_kg_fep_default_config(sec_kg_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->injection_fe_threshold = SEC_KG_FEP_THREAT_FE_THRESHOLD;
    config->traversal_fe_threshold = SEC_KG_FEP_SUSPICIOUS_FE_THRESHOLD;
    config->schema_surprise_threshold = SEC_KG_FEP_HIGH_SURPRISE;
    config->precision_learning_rate = SEC_KG_FEP_DEFAULT_PRECISION_LR;
    config->belief_learning_rate = SEC_KG_FEP_DEFAULT_BELIEF_LR;

    /* Detection sensitivity */
    config->initial_precision = SEC_KG_FEP_DEFAULT_PRECISION;
    config->enable_precision_modulation = true;
    config->enable_fep_scoring = true;

    /* Active inference settings */
    config->enable_active_inference = true;
    config->action_threshold = 0.5f;
    config->threat_decay_rate = SEC_KG_FEP_DEFAULT_THREAT_DECAY;

    /* Learning settings */
    config->enable_online_learning = true;
    config->learn_from_false_positives = true;
    config->learn_from_confirmed_attacks = true;

    /* Integration settings */
    config->enable_bio_async = true;
    config->enable_detailed_logging = false;

    return 0;
}

/**
 * WHAT: Retrieve current bridge configuration
 * WHY:  Allow inspection of active settings
 * HOW:  Copy current config to output
 */
int sec_kg_fep_get_config(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

/**
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Apply new config with validation
 */
int sec_kg_fep_set_config(
    sec_kg_fep_bridge_t* bridge,
    const sec_kg_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * WHAT: Allocate and initialize FEP-security bridge
 * WHY:  Enable surprise-based threat detection for KG
 * HOW:  Allocate structure, initialize base, connect systems
 */
sec_kg_fep_bridge_t* sec_kg_fep_create(
    const sec_kg_fep_config_t* config,
    security_kg_bridge_t* sec_kg,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!sec_kg || !fep_system) {
        NIMCP_LOGGING_ERROR("Security KG FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_create: required parameter is NULL (sec_kg, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    sec_kg_fep_bridge_t* bridge = (sec_kg_fep_bridge_t*)nimcp_malloc(
        sizeof(sec_kg_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security KG FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_kg_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(sec_kg_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_kg_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->sec_kg = sec_kg;
    bridge->fep_system = fep_system;

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "security_kg_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Security KG FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "sec_kg_fep_create: mutex creation failed");
        return NULL;
    }

    bridge->base.module_id = BIO_MODULE_SEC_KG_FEP;
    bridge->base.module_name = SEC_KG_FEP_MODULE_NAME;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.fep_connected = true;
    bridge->state.sec_kg_connected = true;
    bridge->state.current_precision = bridge->config.initial_precision;
    bridge->state.current_threat = SEC_KG_FEP_THREAT_NONE;

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        sec_kg_fep_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Security KG FEP bridge created");
    return bridge;
}

/**
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect bio-async, cleanup base, free memory
 */
void sec_kg_fep_destroy(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_kg_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security KG FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Clear accumulated state for fresh start
 * HOW:  Zero state, reset precision to default
 */
int sec_kg_fep_reset(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.current_free_energy = 0.0f;
    bridge->state.current_surprise = 0.0f;
    bridge->state.current_precision = bridge->config.initial_precision;
    bridge->state.current_threat = SEC_KG_FEP_THREAT_NONE;
    bridge->state.threat_start_time = 0;
    bridge->state.threat_peak = 0.0f;
    bridge->state.update_count = 0;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_sec_kg_effects_t));
    memset(&bridge->sec_effects, 0, sizeof(sec_kg_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_kg_fep_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Security KG FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Core Update API Implementation
 * ============================================================================ */

/**
 * WHAT: Calculate FEP-derived security modulation
 * WHY:  Map free energy to threat assessment
 * HOW:  Get FEP state, compute threat level, set sensitivity
 */
int sec_kg_fep_compute_effects(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Store current values */
    bridge->state.current_free_energy = current_fe;
    bridge->state.current_surprise = surprise;

    /* Compute normalized threat from free energy */
    float normalized_threat = compute_threat_from_fe(current_fe, &bridge->config);
    bridge->fep_effects.normalized_threat = normalized_threat;
    bridge->fep_effects.free_energy_score = current_fe;
    bridge->fep_effects.surprise_score = surprise;

    /* Classify threat level */
    sec_kg_fep_threat_level_t threat_level = classify_threat(normalized_threat);
    bridge->fep_effects.threat_level = threat_level;
    bridge->state.current_threat = threat_level;

    /* Update threat peak tracking */
    if (normalized_threat > bridge->state.threat_peak) {
        bridge->state.threat_peak = normalized_threat;
    }

    /* Compute detection sensitivity from precision */
    float sensitivity = compute_detection_sensitivity(bridge->state.current_precision);
    bridge->fep_effects.detection_sensitivity = sensitivity;

    /* Compute threshold adjustments */
    bridge->fep_effects.injection_threshold_adj = 1.0f / bridge->state.current_precision;
    bridge->fep_effects.traversal_threshold_adj = 1.0f / bridge->state.current_precision;

    /* Select recommended action via active inference */
    if (bridge->config.enable_active_inference) {
        bridge->fep_effects.recommended_action = select_action_for_threat(threat_level);
        bridge->fep_effects.action_urgency = normalized_threat;
    } else {
        bridge->fep_effects.recommended_action = SEC_KG_FEP_ACTION_NONE;
        bridge->fep_effects.action_urgency = 0.0f;
    }

    /* Compute confidence metrics */
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.detection_confidence = 1.0f - (pred_error / 10.0f);
    if (bridge->fep_effects.detection_confidence < 0.0f) {
        bridge->fep_effects.detection_confidence = 0.0f;
    }

    bridge->fep_effects.model_certainty = bridge->state.current_precision /
                                          SEC_KG_FEP_MAX_PRECISION;

    /* Update statistics */
    update_running_average(&bridge->stats.avg_free_energy, current_fe, 0.1f);
    update_running_average(&bridge->stats.avg_surprise, surprise, 0.1f);
    update_running_average(&bridge->stats.avg_precision,
                          bridge->state.current_precision, 0.1f);

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    bridge->stats.fep_computations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process security detection as FEP observation
 * WHY:  Feed security events back to FEP system
 * HOW:  Convert detection to prediction error, update beliefs
 */
int sec_kg_fep_update_from_detection(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_query_result_t query_result,
    float injection_score,
    float schema_deviation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update security effects counters */
    bridge->sec_effects.queries_analyzed++;

    if (query_result == SEC_KG_QUERY_INJECTION_DETECTED) {
        bridge->sec_effects.injections_detected++;
    }

    if (schema_deviation > 0.5f) {
        bridge->sec_effects.schema_violations++;
    }

    /* Update running averages */
    update_running_average(&bridge->sec_effects.avg_injection_score,
                          injection_score, 0.1f);
    update_running_average(&bridge->sec_effects.avg_schema_deviation,
                          schema_deviation, 0.1f);

    /* Process through FEP if online learning enabled */
    if (bridge->config.enable_online_learning && bridge->fep_system) {
        /* Convert detection scores to FEP observation */
        float observation[16] = {0};
        observation[0] = injection_score;
        observation[1] = schema_deviation;
        observation[2] = (query_result != SEC_KG_QUERY_VALID) ? 1.0f : 0.0f;

        fep_process_observation(bridge->fep_system, observation, 16);

        /* Update beliefs based on detection */
        if (query_result == SEC_KG_QUERY_INJECTION_DETECTED ||
            schema_deviation > bridge->config.schema_surprise_threshold / 20.0f) {
            /* High-surprise observation - update precision */
            fep_update_precision(bridge->fep_system);
            bridge->stats.belief_updates++;
        } else {
            /* Normal observation - update generative model */
            fep_update_beliefs(bridge->fep_system);
            bridge->stats.belief_updates++;
        }
    }

    /* Track threat by level */
    if (bridge->fep_effects.threat_level > SEC_KG_FEP_THREAT_NONE) {
        bridge->stats.threats_detected++;
        if (bridge->fep_effects.threat_level < 5) {
            bridge->stats.threats_by_level[bridge->fep_effects.threat_level]++;
        }
    }

    bridge->state.update_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process traversal access check as FEP observation
 * WHY:  Traversal patterns inform threat model
 * HOW:  Convert traversal metrics to prediction error
 */
int sec_kg_fep_update_from_traversal(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_traversal_result_t traversal_result,
    uint32_t depth_reached,
    float anomaly_score
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update traversal statistics */
    if (traversal_result != SEC_KG_TRAVERSAL_ALLOWED) {
        bridge->sec_effects.traversal_violations++;
    }

    update_running_average(&bridge->sec_effects.avg_traversal_score,
                          anomaly_score, 0.1f);
    update_running_average(&bridge->sec_effects.traversal_depth_avg,
                          (float)depth_reached, 0.1f);

    /* Process through FEP if online learning enabled */
    if (bridge->config.enable_online_learning && bridge->fep_system) {
        float observation[16] = {0};
        observation[0] = anomaly_score;
        observation[1] = (float)depth_reached / (float)SEC_KG_MAX_TRAVERSAL_DEPTH;
        observation[2] = (traversal_result != SEC_KG_TRAVERSAL_ALLOWED) ? 1.0f : 0.0f;

        fep_process_observation(bridge->fep_system, observation, 16);
        fep_update_beliefs(bridge->fep_system);
        bridge->stats.belief_updates++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Perform complete update in both directions
 * WHY:  Convenience for regular update cycles
 * HOW:  Compute effects, apply decay, update model
 */
int sec_kg_fep_update(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Compute FEP effects on security */
    int result = sec_kg_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply threat decay */
    if (bridge->fep_effects.normalized_threat < bridge->state.threat_peak) {
        bridge->state.threat_peak *= bridge->config.threat_decay_rate;
    }

    /* Apply precision modulation if enabled */
    if (bridge->config.enable_precision_modulation) {
        sec_kg_fep_apply_precision_modulation(bridge);
    }

    /* Record update */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Active Inference API Implementation
 * ============================================================================ */

/**
 * WHAT: Choose action to minimize expected free energy
 * WHY:  Active inference for security response
 * HOW:  Evaluate EFE for each action, select minimum
 */
int sec_kg_fep_select_action(
    sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_action_t* action
) {
    if (!bridge || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_select_action: required parameter is NULL (bridge, action)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Select action based on current threat level */
    *action = select_action_for_threat(bridge->state.current_threat);

    /* Track action selection */
    if (*action < 6) {
        bridge->stats.actions_taken[*action]++;
    }
    bridge->stats.action_selections++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Compute EFE for specific action
 * WHY:  Evaluate action before selection
 * HOW:  Project future states, compute expected surprise
 */
int sec_kg_fep_get_action_efe(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_action_t action,
    float* efe
) {
    if (!bridge || !efe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_action_efe: required parameter is NULL (bridge, efe)");
        return -1;
    }

    /* Compute expected free energy for action */
    /* Lower EFE = better action for reducing future surprise */
    float base_fe = bridge->state.current_free_energy;

    switch (action) {
        case SEC_KG_FEP_ACTION_NONE:
            /* No action - expect same free energy */
            *efe = base_fe;
            break;

        case SEC_KG_FEP_ACTION_LOG:
            /* Logging provides information - slight FE reduction */
            *efe = base_fe * 0.95f;
            break;

        case SEC_KG_FEP_ACTION_SANITIZE:
            /* Sanitization reduces threat - moderate FE reduction */
            *efe = base_fe * 0.7f;
            break;

        case SEC_KG_FEP_ACTION_THROTTLE:
            /* Throttling prevents escalation - significant reduction */
            *efe = base_fe * 0.5f;
            break;

        case SEC_KG_FEP_ACTION_BLOCK:
            /* Blocking eliminates immediate threat */
            *efe = base_fe * 0.2f;
            break;

        case SEC_KG_FEP_ACTION_LOCKDOWN:
            /* Lockdown maximally reduces uncertainty */
            *efe = base_fe * 0.1f;
            break;

        default:
            *efe = base_fe;
            break;
    }

    return 0;
}

/* ============================================================================
 * Precision Modulation API Implementation
 * ============================================================================ */

/**
 * WHAT: Adjust detection sensitivity via precision
 * WHY:  Adapt to current threat environment
 * HOW:  Scale thresholds based on precision level
 */
int sec_kg_fep_apply_precision_modulation(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Already locked by caller or lock here */
    float threat = bridge->fep_effects.normalized_threat;

    /* Adapt precision based on threat level */
    float target_precision = bridge->state.current_precision;

    if (threat > 0.8f) {
        /* Critical threat - maximum precision */
        target_precision = SEC_KG_FEP_MAX_PRECISION;
    } else if (threat > 0.5f) {
        /* High threat - elevated precision */
        target_precision = SEC_KG_FEP_DEFAULT_PRECISION * 2.0f;
    } else if (threat > 0.2f) {
        /* Moderate threat - slightly elevated */
        target_precision = SEC_KG_FEP_DEFAULT_PRECISION * 1.2f;
    } else {
        /* Low threat - decay toward default */
        target_precision = SEC_KG_FEP_DEFAULT_PRECISION;
    }

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    bridge->state.current_precision = clamp_precision(bridge->state.current_precision);

    bridge->stats.precision_adaptations++;

    return 0;
}

/**
 * WHAT: Report detection as false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Decrease precision proportionally
 */
int sec_kg_fep_report_false_positive(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->sec_effects.false_positives++;

    if (bridge->config.learn_from_false_positives) {
        /* Reduce precision to lower sensitivity */
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;
        bridge->state.current_precision = clamp_precision(
            bridge->state.current_precision
        );
        bridge->stats.false_positive_corrections++;
    }

    /* Update estimated precision metric */
    uint64_t total = bridge->sec_effects.injections_detected +
                     bridge->sec_effects.false_positives;
    if (total > 0) {
        bridge->sec_effects.estimated_precision =
            (float)bridge->sec_effects.injections_detected / (float)total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report detection as confirmed true positive
 * WHY:  Increase precision for heightened alertness
 * HOW:  Increase precision, update generative model
 */
int sec_kg_fep_report_confirmed_attack(
    sec_kg_fep_bridge_t* bridge,
    float severity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.learn_from_confirmed_attacks) {
        /* Increase precision based on severity */
        float increase = 1.0f + (0.2f * severity);
        bridge->state.current_precision *= increase;
        bridge->state.current_precision = clamp_precision(
            bridge->state.current_precision
        );

        /* Update FEP precision if available */
        if (bridge->fep_system) {
            fep_update_precision(bridge->fep_system);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Override precision to specific value
 * WHY:  Manual sensitivity tuning
 * HOW:  Clamp to valid range and apply
 */
int sec_kg_fep_set_precision(
    sec_kg_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_precision = clamp_precision(precision);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get current precision level
 */
float sec_kg_fep_get_precision(const sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_precision;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int sec_kg_fep_get_fep_effects(
    const sec_kg_fep_bridge_t* bridge,
    fep_to_sec_kg_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->fep_effects;
    return 0;
}

int sec_kg_fep_get_sec_effects(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_sec_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->sec_effects;
    return 0;
}

int sec_kg_fep_get_state(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int sec_kg_fep_get_stats(
    const sec_kg_fep_bridge_t* bridge,
    sec_kg_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

sec_kg_fep_threat_level_t sec_kg_fep_get_threat_level(
    const sec_kg_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_KG_FEP_THREAT_NONE;
    }
    return bridge->state.current_threat;
}

float sec_kg_fep_get_free_energy(const sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_free_energy;
}

float sec_kg_fep_get_surprise(const sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_surprise;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

/**
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 */
int sec_kg_fep_connect_bio_async(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SEC_KG_FEP,
        .module_name = SEC_KG_FEP_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security KG FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 */
int sec_kg_fep_disconnect_bio_async(sec_kg_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security KG FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 */
bool sec_kg_fep_is_bio_async_connected(const sec_kg_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/**
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle async security notifications
 * HOW:  Use bio_router_process_inbox to invoke registered handlers
 */
int sec_kg_fep_process_messages(sec_kg_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Process pending messages using registered handlers */
    uint32_t messages_processed = bio_router_process_inbox(
        bridge->base.bio_ctx,
        0  /* Process all pending messages */
    );

    return (int)messages_processed;
}

/* ============================================================================
 * Utility API Implementation
 * ============================================================================ */

/**
 * WHAT: Output human-readable bridge summary
 * WHY:  Debugging and monitoring
 */
void sec_kg_fep_print_summary(const sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_INFO("Security KG FEP Bridge: NULL");
        return;
    }

    NIMCP_LOGGING_INFO("=== Security KG FEP Bridge Summary ===");
    NIMCP_LOGGING_INFO("State: %s", bridge->state.active ? "ACTIVE" : "INACTIVE");
    NIMCP_LOGGING_INFO("Threat Level: %s",
        sec_kg_fep_threat_level_name(bridge->state.current_threat));
    NIMCP_LOGGING_INFO("Free Energy: %.3f", bridge->state.current_free_energy);
    NIMCP_LOGGING_INFO("Surprise: %.3f", bridge->state.current_surprise);
    NIMCP_LOGGING_INFO("Precision: %.3f", bridge->state.current_precision);
    NIMCP_LOGGING_INFO("Total Updates: %lu", (unsigned long)bridge->stats.total_updates);
    NIMCP_LOGGING_INFO("Threats Detected: %lu",
        (unsigned long)bridge->stats.threats_detected);
    NIMCP_LOGGING_INFO("Queries Analyzed: %lu",
        (unsigned long)bridge->sec_effects.queries_analyzed);
    NIMCP_LOGGING_INFO("Injections Detected: %lu",
        (unsigned long)bridge->sec_effects.injections_detected);
    NIMCP_LOGGING_INFO("Bio-Async: %s",
        bridge->base.bio_async_enabled ? "CONNECTED" : "DISCONNECTED");
    NIMCP_LOGGING_INFO("======================================");
}

/**
 * WHAT: Clear cumulative statistics
 * WHY:  Start fresh measurement period
 */
void sec_kg_fep_reset_stats(sec_kg_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sec_kg_fep_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/**
 * WHAT: Get human-readable threat level name
 */
const char* sec_kg_fep_threat_level_name(sec_kg_fep_threat_level_t level) {
    switch (level) {
        case SEC_KG_FEP_THREAT_NONE:     return "NONE";
        case SEC_KG_FEP_THREAT_LOW:      return "LOW";
        case SEC_KG_FEP_THREAT_MEDIUM:   return "MEDIUM";
        case SEC_KG_FEP_THREAT_HIGH:     return "HIGH";
        case SEC_KG_FEP_THREAT_CRITICAL: return "CRITICAL";
        default:                          return "UNKNOWN";
    }
}

/**
 * WHAT: Get human-readable action name
 */
const char* sec_kg_fep_action_name(sec_kg_fep_action_t action) {
    switch (action) {
        case SEC_KG_FEP_ACTION_NONE:     return "NONE";
        case SEC_KG_FEP_ACTION_LOG:      return "LOG";
        case SEC_KG_FEP_ACTION_SANITIZE: return "SANITIZE";
        case SEC_KG_FEP_ACTION_THROTTLE: return "THROTTLE";
        case SEC_KG_FEP_ACTION_BLOCK:    return "BLOCK";
        case SEC_KG_FEP_ACTION_LOCKDOWN: return "LOCKDOWN";
        default:                          return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute normalized threat from free energy
 * WHY:  Map unbounded FE to [0,1] range
 * HOW:  Sigmoid-like scaling based on threshold
 */
static float compute_threat_from_fe(float free_energy, const sec_kg_fep_config_t* config) {
    if (free_energy <= 0.0f) {
        return 0.0f;
    }

    float threshold = config->injection_fe_threshold;
    if (threshold <= 0.0f) {
        threshold = SEC_KG_FEP_THREAT_FE_THRESHOLD;
    }

    /* Normalize to [0,1] with saturation */
    float normalized = free_energy / threshold;
    if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    return normalized;
}

/**
 * WHAT: Classify normalized threat into categorical level
 * WHY:  Discrete levels for action selection
 * HOW:  Threshold-based classification
 */
static sec_kg_fep_threat_level_t classify_threat(float normalized_threat) {
    if (normalized_threat >= 0.9f) {
        return SEC_KG_FEP_THREAT_CRITICAL;
    } else if (normalized_threat >= 0.7f) {
        return SEC_KG_FEP_THREAT_HIGH;
    } else if (normalized_threat >= 0.4f) {
        return SEC_KG_FEP_THREAT_MEDIUM;
    } else if (normalized_threat >= 0.1f) {
        return SEC_KG_FEP_THREAT_LOW;
    } else {
        return SEC_KG_FEP_THREAT_NONE;
    }
}

/**
 * WHAT: Select appropriate action for threat level
 * WHY:  Active inference action selection
 * HOW:  Map threat level to response action
 */
static sec_kg_fep_action_t select_action_for_threat(sec_kg_fep_threat_level_t threat) {
    switch (threat) {
        case SEC_KG_FEP_THREAT_CRITICAL:
            return SEC_KG_FEP_ACTION_LOCKDOWN;
        case SEC_KG_FEP_THREAT_HIGH:
            return SEC_KG_FEP_ACTION_BLOCK;
        case SEC_KG_FEP_THREAT_MEDIUM:
            return SEC_KG_FEP_ACTION_THROTTLE;
        case SEC_KG_FEP_THREAT_LOW:
            return SEC_KG_FEP_ACTION_LOG;
        case SEC_KG_FEP_THREAT_NONE:
        default:
            return SEC_KG_FEP_ACTION_NONE;
    }
}

/**
 * WHAT: Clamp precision to valid range
 * WHY:  Prevent extreme sensitivity values
 * HOW:  Bound to [MIN, MAX] range
 */
static float clamp_precision(float precision) {
    if (precision < SEC_KG_FEP_MIN_PRECISION) {
        return SEC_KG_FEP_MIN_PRECISION;
    }
    if (precision > SEC_KG_FEP_MAX_PRECISION) {
        return SEC_KG_FEP_MAX_PRECISION;
    }
    return precision;
}

/**
 * WHAT: Compute detection sensitivity from precision
 * WHY:  Precision modulates detection threshold
 * HOW:  Normalize precision to [0,1] sensitivity
 */
static float compute_detection_sensitivity(float precision) {
    /* Map precision [MIN, MAX] to sensitivity [0, 1] */
    float range = SEC_KG_FEP_MAX_PRECISION - SEC_KG_FEP_MIN_PRECISION;
    float normalized = (precision - SEC_KG_FEP_MIN_PRECISION) / range;

    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized;
}

/**
 * WHAT: Update exponential moving average
 * WHY:  Smooth statistics over time
 * HOW:  new_avg = (1-alpha)*old + alpha*new
 */
static void update_running_average(float* avg, float new_value, float alpha) {
    if (!avg) return;
    *avg = (1.0f - alpha) * (*avg) + alpha * new_value;
}
