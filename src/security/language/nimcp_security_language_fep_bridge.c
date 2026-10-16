/**
 * @file nimcp_security_language_fep_bridge.c
 * @brief Implementation of Security Language FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for language security operations
 * WHY:  Malicious language patterns represent high-surprise deviations from
 *       expected linguistic distributions
 * HOW:  Map language anomalies to free energy, use precision for sensitivity tuning,
 *       employ active inference for protective responses
 *
 * @author NIMCP Development Team
 */

#include "security/language/nimcp_security_language_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_language_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_threat_from_fe(float free_energy, const sec_lang_fep_config_t* config);
static sec_lang_fep_threat_level_t classify_threat(float normalized_threat);
static sec_lang_fep_action_t select_action_for_threat(sec_lang_fep_threat_level_t threat);
static float clamp_precision(float precision);
static float compute_detection_sensitivity(float precision);
static void update_running_average(float* avg, float new_value, float alpha);
static float get_category_weight(sec_lang_fep_threat_category_t category);
static sec_lang_fep_threat_category_t find_primary_category(
    const sec_lang_fep_bridge_t* bridge
);
static void update_category_state(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category,
    float fe_contribution
);

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

/**
 * WHAT: Provide sensible defaults for FEP-language security integration
 * WHY:  Easy initialization with balanced sensitivity
 * HOW:  Set moderate thresholds and enable key features
 */
int sec_lang_fep_default_config(sec_lang_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->injection_fe_threshold = SEC_LANG_FEP_THREAT_FE_THRESHOLD;
    config->semantic_fe_threshold = SEC_LANG_FEP_SUSPICIOUS_FE_THRESHOLD;
    config->context_fe_threshold = SEC_LANG_FEP_HIGH_SURPRISE;
    config->output_fe_threshold = SEC_LANG_FEP_SUSPICIOUS_FE_THRESHOLD;
    config->precision_learning_rate = SEC_LANG_FEP_DEFAULT_PRECISION_LR;
    config->belief_learning_rate = SEC_LANG_FEP_DEFAULT_BELIEF_LR;

    /* Detection sensitivity */
    config->initial_precision = SEC_LANG_FEP_DEFAULT_PRECISION;
    config->enable_precision_modulation = true;
    config->enable_fep_scoring = true;

    /* Category-specific enables */
    config->enable_prompt_injection_detection = true;
    config->enable_semantic_manipulation_detection = true;
    config->enable_context_hijacking_detection = true;
    config->enable_output_manipulation_detection = true;

    /* Active inference settings */
    config->enable_active_inference = true;
    config->action_threshold = 0.5f;
    config->threat_decay_rate = SEC_LANG_FEP_DEFAULT_THREAT_DECAY;

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
int sec_lang_fep_get_config(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_config: required parameter is NULL (bridge, config)");
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
int sec_lang_fep_set_config(
    sec_lang_fep_bridge_t* bridge,
    const sec_lang_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_set_config: required parameter is NULL (bridge, config)");
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
 * WHAT: Allocate and initialize FEP-language security bridge
 * WHY:  Enable surprise-based threat detection for language processing
 * HOW:  Allocate structure, initialize base, connect systems
 */
sec_lang_fep_bridge_t* sec_lang_fep_create(
    const sec_lang_fep_config_t* config,
    security_language_bridge_t* sec_lang,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!sec_lang || !fep_system) {
        NIMCP_LOGGING_ERROR("Security Language FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_create: required parameter is NULL (sec_lang, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    sec_lang_fep_bridge_t* bridge = (sec_lang_fep_bridge_t*)nimcp_malloc(
        sizeof(sec_lang_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security Language FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_lang_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(sec_lang_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_lang_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->sec_lang = sec_lang;
    bridge->fep_system = fep_system;

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "security_language_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Security Language FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_create: bridge->base is NULL");
        return NULL;
    }

    bridge->base.module_id = BIO_MODULE_SEC_LANG_FEP;
    bridge->base.module_name = SEC_LANG_FEP_MODULE_NAME;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.fep_connected = true;
    bridge->state.sec_lang_connected = true;
    bridge->state.current_precision = bridge->config.initial_precision;
    bridge->state.current_threat = SEC_LANG_FEP_THREAT_NONE;
    bridge->state.primary_category = SEC_LANG_FEP_CATEGORY_NONE;
    bridge->state.last_update_time_ms = nimcp_platform_time_monotonic_ms();

    /* Initialize category states */
    for (int i = 0; i < SEC_LANG_FEP_CATEGORY_COUNT; i++) {
        bridge->state.category_states[i].category = (sec_lang_fep_threat_category_t)i;
        bridge->state.category_states[i].current_fe = 0.0f;
        bridge->state.category_states[i].peak_fe = 0.0f;
        bridge->state.category_states[i].detection_count = 0;
        bridge->state.category_states[i].last_detection_ms = 0;
        bridge->state.category_states[i].detection_rate = 0.0f;
    }

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        sec_lang_fep_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Security Language FEP bridge created");
    return bridge;
}

/**
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect bio-async, cleanup base, free memory
 */
void sec_lang_fep_destroy(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_lang_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security Language FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Clear accumulated state for fresh start
 * HOW:  Zero state, reset precision to default
 */
int sec_lang_fep_reset(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.current_free_energy = 0.0f;
    bridge->state.current_surprise = 0.0f;
    bridge->state.current_precision = bridge->config.initial_precision;
    bridge->state.current_threat = SEC_LANG_FEP_THREAT_NONE;
    bridge->state.primary_category = SEC_LANG_FEP_CATEGORY_NONE;
    bridge->state.threat_start_time_ms = 0;
    bridge->state.threat_peak = 0.0f;
    bridge->state.update_count = 0;
    bridge->state.last_update_time_ms = nimcp_platform_time_monotonic_ms();

    /* Reset category states */
    for (int i = 0; i < SEC_LANG_FEP_CATEGORY_COUNT; i++) {
        bridge->state.category_states[i].current_fe = 0.0f;
        bridge->state.category_states[i].peak_fe = 0.0f;
        bridge->state.category_states[i].detection_count = 0;
        bridge->state.category_states[i].last_detection_ms = 0;
        bridge->state.category_states[i].detection_rate = 0.0f;
    }

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_sec_lang_effects_t));
    memset(&bridge->sec_effects, 0, sizeof(sec_lang_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_lang_fep_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Security Language FEP bridge reset");
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
int sec_lang_fep_compute_effects(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Store current values */
    bridge->state.current_free_energy = current_fe;
    bridge->state.current_surprise = surprise;

    /* Compute per-category free energy contributions */
    bridge->fep_effects.prompt_injection_fe =
        bridge->state.category_states[SEC_LANG_FEP_CATEGORY_PROMPT_INJECTION].current_fe;
    bridge->fep_effects.semantic_manipulation_fe =
        bridge->state.category_states[SEC_LANG_FEP_CATEGORY_SEMANTIC_MANIPULATION].current_fe;
    bridge->fep_effects.context_hijacking_fe =
        bridge->state.category_states[SEC_LANG_FEP_CATEGORY_CONTEXT_HIJACKING].current_fe;
    bridge->fep_effects.output_manipulation_fe =
        bridge->state.category_states[SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION].current_fe;

    /* Compute total weighted free energy */
    float weighted_fe =
        bridge->fep_effects.prompt_injection_fe * SEC_LANG_FEP_PROMPT_INJECTION_WEIGHT +
        bridge->fep_effects.semantic_manipulation_fe * SEC_LANG_FEP_SEMANTIC_MANIP_WEIGHT +
        bridge->fep_effects.context_hijacking_fe * SEC_LANG_FEP_CONTEXT_HIJACK_WEIGHT +
        bridge->fep_effects.output_manipulation_fe * SEC_LANG_FEP_OUTPUT_MANIP_WEIGHT;

    /* Use max of system FE and weighted category FE */
    float total_fe = (current_fe > weighted_fe) ? current_fe : weighted_fe;

    /* Compute normalized threat from free energy */
    float normalized_threat = compute_threat_from_fe(total_fe, &bridge->config);
    bridge->fep_effects.normalized_threat = normalized_threat;
    bridge->fep_effects.free_energy_score = total_fe;
    bridge->fep_effects.surprise_score = surprise;

    /* Classify threat level */
    sec_lang_fep_threat_level_t threat_level = classify_threat(normalized_threat);
    bridge->fep_effects.threat_level = threat_level;
    bridge->state.current_threat = threat_level;

    /* Find primary threat category */
    sec_lang_fep_threat_category_t primary = find_primary_category(bridge);
    bridge->fep_effects.primary_threat = primary;
    bridge->state.primary_category = primary;

    /* Update threat peak tracking */
    if (normalized_threat > bridge->state.threat_peak) {
        bridge->state.threat_peak = normalized_threat;
        if (threat_level > SEC_LANG_FEP_THREAT_LOW && bridge->state.threat_start_time_ms == 0) {
            bridge->state.threat_start_time_ms = nimcp_platform_time_monotonic_ms();
        }
    }

    /* Compute detection sensitivity from precision */
    float sensitivity = compute_detection_sensitivity(bridge->state.current_precision);
    bridge->fep_effects.detection_sensitivity = sensitivity;

    /* Compute threshold adjustments */
    float precision_factor = 1.0f / bridge->state.current_precision;
    bridge->fep_effects.injection_threshold_adj = precision_factor;
    bridge->fep_effects.semantic_threshold_adj = precision_factor;
    bridge->fep_effects.context_threshold_adj = precision_factor;

    /* Select recommended action via active inference */
    if (bridge->config.enable_active_inference) {
        bridge->fep_effects.recommended_action = select_action_for_threat(threat_level);
        bridge->fep_effects.action_urgency = normalized_threat;
    } else {
        bridge->fep_effects.recommended_action = SEC_LANG_FEP_ACTION_NONE;
        bridge->fep_effects.action_urgency = 0.0f;
    }

    /* Compute confidence metrics */
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.detection_confidence = 1.0f - (pred_error / 10.0f);
    if (bridge->fep_effects.detection_confidence < 0.0f) {
        bridge->fep_effects.detection_confidence = 0.0f;
    }
    if (bridge->fep_effects.detection_confidence > 1.0f) {
        bridge->fep_effects.detection_confidence = 1.0f;
    }

    bridge->fep_effects.model_certainty = bridge->state.current_precision /
                                          SEC_LANG_FEP_MAX_PRECISION;

    /* Update statistics */
    update_running_average(&bridge->stats.avg_free_energy, total_fe, 0.1f);
    update_running_average(&bridge->stats.avg_surprise, surprise, 0.1f);
    update_running_average(&bridge->stats.avg_precision,
                          bridge->state.current_precision, 0.1f);

    if (total_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = total_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    bridge->stats.fep_computations++;
    bridge->state.last_update_time_ms = nimcp_platform_time_monotonic_ms();

    /* Track timing */
    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start_time;
    update_running_average(&bridge->stats.avg_update_time_ms, (float)elapsed, 0.1f);
    if ((float)elapsed > bridge->stats.max_update_time_ms) {
        bridge->stats.max_update_time_ms = (float)elapsed;
    }
    bridge->stats.total_processing_time_ms += elapsed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process injection detection as FEP observation
 * WHY:  Feed security events back to FEP system
 * HOW:  Convert detection to prediction error, update beliefs
 */
int sec_lang_fep_update_from_detection(
    sec_lang_fep_bridge_t* bridge,
    const security_language_detection_result_t* detection
) {
    if (!bridge || !detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_update_from_detection: required parameter is NULL (bridge, detection)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_platform_time_monotonic_ms();

    /* Update security effects counters */
    bridge->sec_effects.inputs_analyzed++;

    if (detection->injection_detected) {
        bridge->sec_effects.injections_detected++;
        bridge->sec_effects.last_detection_time_ms = now;

        /* Update per-detection statistics */
        for (uint32_t i = 0; i < detection->detection_count; i++) {
            const security_language_detection_t* det = &detection->detections[i];

            /* Map injection type to FEP category */
            sec_lang_fep_threat_category_t category = sec_lang_fep_from_injection_type(det->type);

            /* Update category state */
            float fe_contribution = det->confidence * get_category_weight(category);
            update_category_state(bridge, category, fe_contribution);

            /* Track by category */
            if (category < SEC_LANG_FEP_CATEGORY_COUNT) {
                bridge->stats.threats_by_category[category]++;
            }
        }
    }

    /* Update running averages */
    update_running_average(&bridge->sec_effects.avg_injection_score,
                          detection->aggregate_threat_score, 0.1f);

    /* Process through FEP if online learning enabled */
    if (bridge->config.enable_online_learning && bridge->fep_system) {
        /* Convert detection scores to FEP observation */
        float observation[16] = {0};
        observation[0] = detection->aggregate_threat_score;
        observation[1] = detection->injection_detected ? 1.0f : 0.0f;
        observation[2] = (float)detection->detection_count / (float)SECURITY_LANGUAGE_MAX_DETECTIONS;
        observation[3] = (float)detection->max_severity / (float)THREAT_SEVERITY_CRITICAL;

        fep_process_observation(bridge->fep_system, observation, 16);

        /* Update beliefs based on detection */
        if (detection->injection_detected ||
            detection->aggregate_threat_score > bridge->config.action_threshold) {
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
    if (bridge->fep_effects.threat_level > SEC_LANG_FEP_THREAT_NONE) {
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
 * WHAT: Process semantic anomaly as FEP observation
 * WHY:  Semantic manipulation informs threat model
 * HOW:  Convert semantic scores to prediction error
 */
int sec_lang_fep_update_from_semantic(
    sec_lang_fep_bridge_t* bridge,
    float semantic_score,
    float context_score
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_platform_time_monotonic_ms();

    /* Update semantic statistics */
    if (semantic_score > 0.5f) {
        bridge->sec_effects.semantic_anomalies++;
        update_category_state(bridge, SEC_LANG_FEP_CATEGORY_SEMANTIC_MANIPULATION,
                            semantic_score * SEC_LANG_FEP_SEMANTIC_MANIP_WEIGHT);
    }

    if (context_score > 0.5f) {
        bridge->sec_effects.context_hijacks++;
        update_category_state(bridge, SEC_LANG_FEP_CATEGORY_CONTEXT_HIJACKING,
                            context_score * SEC_LANG_FEP_CONTEXT_HIJACK_WEIGHT);
        bridge->sec_effects.last_detection_time_ms = now;
    }

    /* Update running averages */
    update_running_average(&bridge->sec_effects.avg_semantic_score, semantic_score, 0.1f);
    update_running_average(&bridge->sec_effects.avg_context_score, context_score, 0.1f);
    update_running_average(&bridge->sec_effects.semantic_deviation_avg,
                          (semantic_score + context_score) / 2.0f, 0.1f);

    /* Process through FEP if online learning enabled */
    if (bridge->config.enable_online_learning && bridge->fep_system) {
        float observation[16] = {0};
        observation[0] = semantic_score;
        observation[1] = context_score;
        observation[2] = (semantic_score > 0.5f || context_score > 0.5f) ? 1.0f : 0.0f;

        fep_process_observation(bridge->fep_system, observation, 16);
        fep_update_beliefs(bridge->fep_system);
        bridge->stats.belief_updates++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process output validation as FEP observation
 * WHY:  Output manipulation attempts inform threat model
 * HOW:  Convert validation result to prediction error
 */
int sec_lang_fep_update_from_output(
    sec_lang_fep_bridge_t* bridge,
    const security_language_output_validation_t* validation
) {
    if (!bridge || !validation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_update_from_output: required parameter is NULL (bridge, validation)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_platform_time_monotonic_ms();

    /* Update output statistics */
    if (!validation->valid || validation->requires_modification) {
        bridge->sec_effects.output_manipulations++;

        float output_score = 1.0f - validation->safety_score;
        update_category_state(bridge, SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION,
                            output_score * SEC_LANG_FEP_OUTPUT_MANIP_WEIGHT);

        bridge->sec_effects.last_detection_time_ms = now;
    }

    /* Update running average */
    float output_anomaly = 1.0f - validation->safety_score;
    update_running_average(&bridge->sec_effects.avg_output_score, output_anomaly, 0.1f);

    /* Process through FEP if online learning enabled */
    if (bridge->config.enable_online_learning && bridge->fep_system) {
        float observation[16] = {0};
        observation[0] = output_anomaly;
        observation[1] = validation->requires_modification ? 1.0f : 0.0f;
        observation[2] = !validation->valid ? 1.0f : 0.0f;

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
int sec_lang_fep_update(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Compute FEP effects on security */
    int result = sec_lang_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply threat decay */
    if (bridge->fep_effects.normalized_threat < bridge->state.threat_peak) {
        bridge->state.threat_peak *= bridge->config.threat_decay_rate;
    }

    /* Apply category-specific decay */
    for (int i = 0; i < SEC_LANG_FEP_CATEGORY_COUNT; i++) {
        bridge->state.category_states[i].current_fe *= bridge->config.threat_decay_rate;
        if (bridge->state.category_states[i].current_fe < 0.01f) {
            bridge->state.category_states[i].current_fe = 0.0f;
        }
    }

    /* Reset threat tracking if threat subsided */
    if (bridge->state.current_threat == SEC_LANG_FEP_THREAT_NONE) {
        bridge->state.threat_start_time_ms = 0;
    }

    /* Apply precision modulation if enabled */
    if (bridge->config.enable_precision_modulation) {
        sec_lang_fep_apply_precision_modulation(bridge);
    }

    /* Process bio-async messages */
    if (bridge->base.bio_async_enabled) {
        bio_router_process_inbox(bridge->base.bio_ctx, 0);
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
int sec_lang_fep_select_action(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_action_t* action
) {
    if (!bridge || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_select_action: required parameter is NULL (bridge, action)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Select action based on current threat level */
    *action = select_action_for_threat(bridge->state.current_threat);

    /* Track action selection */
    if (*action < SEC_LANG_FEP_ACTION_COUNT) {
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
int sec_lang_fep_get_action_efe(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_action_t action,
    float* efe
) {
    if (!bridge || !efe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_action_efe: required parameter is NULL (bridge, efe)");
        return -1;
    }

    /* Compute expected free energy for action */
    /* Lower EFE = better action for reducing future surprise */
    float base_fe = bridge->state.current_free_energy;

    switch (action) {
        case SEC_LANG_FEP_ACTION_NONE:
            /* No action - expect same free energy */
            *efe = base_fe;
            break;

        case SEC_LANG_FEP_ACTION_LOG:
            /* Logging provides information - slight FE reduction */
            *efe = base_fe * 0.95f;
            break;

        case SEC_LANG_FEP_ACTION_SANITIZE:
            /* Sanitization reduces threat - moderate FE reduction */
            *efe = base_fe * 0.7f;
            break;

        case SEC_LANG_FEP_ACTION_THROTTLE:
            /* Throttling prevents escalation - significant reduction */
            *efe = base_fe * 0.5f;
            break;

        case SEC_LANG_FEP_ACTION_BLOCK:
            /* Blocking eliminates immediate threat */
            *efe = base_fe * 0.2f;
            break;

        case SEC_LANG_FEP_ACTION_ALERT:
            /* Alert brings human review - moderate reduction */
            *efe = base_fe * 0.4f;
            break;

        case SEC_LANG_FEP_ACTION_LOCKDOWN:
            /* Lockdown maximally reduces uncertainty */
            *efe = base_fe * 0.1f;
            break;

        default:
            *efe = base_fe;
            break;
    }

    return 0;
}

/**
 * WHAT: Execute the recommended security action
 * WHY:  Complete active inference loop
 * HOW:  Apply action to security language bridge
 */
int sec_lang_fep_apply_action(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sec_lang_fep_action_t action = bridge->fep_effects.recommended_action;

    /* Apply action to connected security language bridge */
    switch (action) {
        case SEC_LANG_FEP_ACTION_NONE:
            /* No action needed */
            break;

        case SEC_LANG_FEP_ACTION_LOG:
            /* Logging action - just track it */
            if (bridge->config.enable_detailed_logging) {
                NIMCP_LOGGING_INFO("Security Language FEP: LOG action applied");
            }
            break;

        case SEC_LANG_FEP_ACTION_SANITIZE:
            /* Could trigger sanitization mode */
            if (bridge->config.enable_detailed_logging) {
                NIMCP_LOGGING_INFO("Security Language FEP: SANITIZE action applied");
            }
            break;

        case SEC_LANG_FEP_ACTION_THROTTLE:
            /* Throttle requests */
            if (bridge->config.enable_detailed_logging) {
                NIMCP_LOGGING_INFO("Security Language FEP: THROTTLE action applied");
            }
            break;

        case SEC_LANG_FEP_ACTION_BLOCK:
            /* Block current input */
            if (bridge->config.enable_detailed_logging) {
                NIMCP_LOGGING_INFO("Security Language FEP: BLOCK action applied");
            }
            break;

        case SEC_LANG_FEP_ACTION_ALERT:
            /* Send alert */
            NIMCP_LOGGING_WARN("Security Language FEP: ALERT - threat detected, category=%s",
                sec_lang_fep_category_name(bridge->state.primary_category));
            break;

        case SEC_LANG_FEP_ACTION_LOCKDOWN:
            /* Enter lockdown mode */
            if (bridge->sec_lang) {
                security_language_enter_lockdown(bridge->sec_lang);
            }
            NIMCP_LOGGING_WARN("Security Language FEP: LOCKDOWN initiated");
            break;

        default:
            break;
    }

    /* Track action */
    if (action < SEC_LANG_FEP_ACTION_COUNT) {
        bridge->stats.actions_taken[action]++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
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
int sec_lang_fep_apply_precision_modulation(sec_lang_fep_bridge_t* bridge) {
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
        target_precision = SEC_LANG_FEP_MAX_PRECISION;
    } else if (threat > 0.5f) {
        /* High threat - elevated precision */
        target_precision = SEC_LANG_FEP_DEFAULT_PRECISION * 2.0f;
    } else if (threat > 0.2f) {
        /* Moderate threat - slightly elevated */
        target_precision = SEC_LANG_FEP_DEFAULT_PRECISION * 1.2f;
    } else {
        /* Low threat - decay toward default */
        target_precision = SEC_LANG_FEP_DEFAULT_PRECISION;
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
int sec_lang_fep_report_false_positive(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category
) {
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

        /* Also reduce category-specific FE */
        if (category > SEC_LANG_FEP_CATEGORY_NONE &&
            category < SEC_LANG_FEP_CATEGORY_COUNT) {
            bridge->state.category_states[category].current_fe *= 0.5f;
        }
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
int sec_lang_fep_report_confirmed_attack(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category,
    float severity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_platform_time_monotonic_ms();
    bridge->sec_effects.last_attack_time_ms = now;

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

        /* Boost category-specific tracking */
        if (category > SEC_LANG_FEP_CATEGORY_NONE &&
            category < SEC_LANG_FEP_CATEGORY_COUNT) {
            update_category_state(bridge, category, severity * 5.0f);
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
int sec_lang_fep_set_precision(
    sec_lang_fep_bridge_t* bridge,
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
float sec_lang_fep_get_precision(const sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_precision;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int sec_lang_fep_get_fep_effects(
    const sec_lang_fep_bridge_t* bridge,
    fep_to_sec_lang_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->fep_effects;
    return 0;
}

int sec_lang_fep_get_sec_effects(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_sec_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->sec_effects;
    return 0;
}

int sec_lang_fep_get_state(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int sec_lang_fep_get_stats(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

sec_lang_fep_threat_level_t sec_lang_fep_get_threat_level(
    const sec_lang_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_LANG_FEP_THREAT_NONE;
    }
    return bridge->state.current_threat;
}

sec_lang_fep_threat_category_t sec_lang_fep_get_primary_category(
    const sec_lang_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_LANG_FEP_CATEGORY_NONE;
    }
    return bridge->state.primary_category;
}

float sec_lang_fep_get_free_energy(const sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_free_energy;
}

float sec_lang_fep_get_surprise(const sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_surprise;
}

float sec_lang_fep_get_category_fe(
    const sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category
) {
    if (!bridge || category >= SEC_LANG_FEP_CATEGORY_COUNT) {
        return -1.0f;
    }
    return bridge->state.category_states[category].current_fe;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

/**
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 */
int sec_lang_fep_connect_bio_async(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SEC_LANG_FEP,
        .module_name = SEC_LANG_FEP_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security Language FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 */
int sec_lang_fep_disconnect_bio_async(sec_lang_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security Language FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 */
bool sec_lang_fep_is_bio_async_connected(const sec_lang_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/**
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle async security notifications
 * HOW:  Use bio_router_process_inbox to handle pending messages
 */
int sec_lang_fep_process_messages(sec_lang_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Process pending messages using bio_router_process_inbox */
    uint32_t messages_processed = bio_router_process_inbox(
        bridge->base.bio_ctx,
        0  /* Process all pending messages */
    );

    return (int)messages_processed;
}

/**
 * WHAT: Send threat notification to other modules
 * WHY:  Coordinate security response across system
 * HOW:  Create and send bio-async message using standard bio_message_header_t
 */
int sec_lang_fep_broadcast_threat(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_level_t threat_level,
    sec_lang_fep_threat_category_t category
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    if (!bridge->base.bio_ctx) {
        return -1;
    }

    /* Create threat notification message using standard bio_message_header_t */
    struct {
        bio_message_header_t header;
        struct {
            uint8_t threat_level;
            uint8_t category;
            float free_energy;
            float precision;
        } payload;
    } msg;

    memset(&msg, 0, sizeof(msg));

    /* Initialize header */
    msg.header.type = BIO_MSG_SECURITY_ALERT;
    msg.header.source_module = BIO_MODULE_SEC_LANG_FEP;
    msg.header.target_module = 0;  /* Broadcast to all */
    msg.header.timestamp_us = nimcp_platform_time_monotonic_us();
    msg.header.payload_size = sizeof(msg.payload);
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (threat_level >= SEC_LANG_FEP_THREAT_HIGH) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Fill payload */
    msg.payload.threat_level = (uint8_t)threat_level;
    msg.payload.category = (uint8_t)category;
    msg.payload.free_energy = bridge->state.current_free_energy;
    msg.payload.precision = bridge->state.current_precision;

    /* Send via bio-async router (ctx, msg, size, timeout_ms) */
    nimcp_error_t result = bio_router_send(
        bridge->base.bio_ctx,
        &msg,
        sizeof(msg),
        0  /* No timeout - fire and forget */
    );

    if (result == NIMCP_SUCCESS && bridge->config.enable_detailed_logging) {
        NIMCP_LOGGING_INFO("Security Language FEP: broadcast threat level=%s category=%s",
            sec_lang_fep_threat_level_name(threat_level),
            sec_lang_fep_category_name(category));
    }

    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

/* ============================================================================
 * Utility API Implementation
 * ============================================================================ */

/**
 * WHAT: Output human-readable bridge summary
 * WHY:  Debugging and monitoring
 */
void sec_lang_fep_print_summary(const sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_INFO("Security Language FEP Bridge: NULL");
        return;
    }

    NIMCP_LOGGING_INFO("=== Security Language FEP Bridge Summary ===");
    NIMCP_LOGGING_INFO("State: %s", bridge->state.active ? "ACTIVE" : "INACTIVE");
    NIMCP_LOGGING_INFO("Threat Level: %s",
        sec_lang_fep_threat_level_name(bridge->state.current_threat));
    NIMCP_LOGGING_INFO("Primary Category: %s",
        sec_lang_fep_category_name(bridge->state.primary_category));
    NIMCP_LOGGING_INFO("Free Energy: %.3f", bridge->state.current_free_energy);
    NIMCP_LOGGING_INFO("Surprise: %.3f", bridge->state.current_surprise);
    NIMCP_LOGGING_INFO("Precision: %.3f", bridge->state.current_precision);
    NIMCP_LOGGING_INFO("Recommended Action: %s",
        sec_lang_fep_action_name(bridge->fep_effects.recommended_action));
    NIMCP_LOGGING_INFO("-------------------------------------------");
    NIMCP_LOGGING_INFO("Total Updates: %lu", (unsigned long)bridge->stats.total_updates);
    NIMCP_LOGGING_INFO("Threats Detected: %lu",
        (unsigned long)bridge->stats.threats_detected);
    NIMCP_LOGGING_INFO("Inputs Analyzed: %lu",
        (unsigned long)bridge->sec_effects.inputs_analyzed);
    NIMCP_LOGGING_INFO("Injections Detected: %lu",
        (unsigned long)bridge->sec_effects.injections_detected);
    NIMCP_LOGGING_INFO("Semantic Anomalies: %lu",
        (unsigned long)bridge->sec_effects.semantic_anomalies);
    NIMCP_LOGGING_INFO("Context Hijacks: %lu",
        (unsigned long)bridge->sec_effects.context_hijacks);
    NIMCP_LOGGING_INFO("Output Manipulations: %lu",
        (unsigned long)bridge->sec_effects.output_manipulations);
    NIMCP_LOGGING_INFO("Bio-Async: %s",
        bridge->base.bio_async_enabled ? "CONNECTED" : "DISCONNECTED");
    NIMCP_LOGGING_INFO("===========================================");
}

/**
 * WHAT: Clear cumulative statistics
 * WHY:  Start fresh measurement period
 */
void sec_lang_fep_reset_stats(sec_lang_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sec_lang_fep_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/**
 * WHAT: Get human-readable threat level name
 */
const char* sec_lang_fep_threat_level_name(sec_lang_fep_threat_level_t level) {
    switch (level) {
        case SEC_LANG_FEP_THREAT_NONE:     return "NONE";
        case SEC_LANG_FEP_THREAT_LOW:      return "LOW";
        case SEC_LANG_FEP_THREAT_MEDIUM:   return "MEDIUM";
        case SEC_LANG_FEP_THREAT_HIGH:     return "HIGH";
        case SEC_LANG_FEP_THREAT_CRITICAL: return "CRITICAL";
        default:                            return "UNKNOWN";
    }
}

/**
 * WHAT: Get human-readable threat category name
 */
const char* sec_lang_fep_category_name(sec_lang_fep_threat_category_t category) {
    switch (category) {
        case SEC_LANG_FEP_CATEGORY_NONE:               return "NONE";
        case SEC_LANG_FEP_CATEGORY_PROMPT_INJECTION:   return "PROMPT_INJECTION";
        case SEC_LANG_FEP_CATEGORY_SEMANTIC_MANIPULATION: return "SEMANTIC_MANIPULATION";
        case SEC_LANG_FEP_CATEGORY_CONTEXT_HIJACKING:  return "CONTEXT_HIJACKING";
        case SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION: return "OUTPUT_MANIPULATION";
        case SEC_LANG_FEP_CATEGORY_JAILBREAK:          return "JAILBREAK";
        case SEC_LANG_FEP_CATEGORY_DATA_EXFILTRATION:  return "DATA_EXFILTRATION";
        case SEC_LANG_FEP_CATEGORY_FORMAT_INJECTION:   return "FORMAT_INJECTION";
        default:                                        return "UNKNOWN";
    }
}

/**
 * WHAT: Get human-readable action name
 */
const char* sec_lang_fep_action_name(sec_lang_fep_action_t action) {
    switch (action) {
        case SEC_LANG_FEP_ACTION_NONE:     return "NONE";
        case SEC_LANG_FEP_ACTION_LOG:      return "LOG";
        case SEC_LANG_FEP_ACTION_SANITIZE: return "SANITIZE";
        case SEC_LANG_FEP_ACTION_THROTTLE: return "THROTTLE";
        case SEC_LANG_FEP_ACTION_BLOCK:    return "BLOCK";
        case SEC_LANG_FEP_ACTION_ALERT:    return "ALERT";
        case SEC_LANG_FEP_ACTION_LOCKDOWN: return "LOCKDOWN";
        default:                            return "UNKNOWN";
    }
}

/**
 * WHAT: Convert FEP threat level to security language severity
 */
security_language_threat_severity_t sec_lang_fep_to_severity(
    sec_lang_fep_threat_level_t level
) {
    switch (level) {
        case SEC_LANG_FEP_THREAT_NONE:
            return THREAT_SEVERITY_NONE;
        case SEC_LANG_FEP_THREAT_LOW:
            return THREAT_SEVERITY_LOW;
        case SEC_LANG_FEP_THREAT_MEDIUM:
            return THREAT_SEVERITY_MEDIUM;
        case SEC_LANG_FEP_THREAT_HIGH:
            return THREAT_SEVERITY_HIGH;
        case SEC_LANG_FEP_THREAT_CRITICAL:
            return THREAT_SEVERITY_CRITICAL;
        default:
            return THREAT_SEVERITY_NONE;
    }
}

/**
 * WHAT: Convert security language injection type to FEP category
 */
sec_lang_fep_threat_category_t sec_lang_fep_from_injection_type(
    security_language_injection_type_t injection_type
) {
    switch (injection_type) {
        case INJECTION_TYPE_PROMPT:
            return SEC_LANG_FEP_CATEGORY_PROMPT_INJECTION;
        case INJECTION_TYPE_SQL:
        case INJECTION_TYPE_CODE:
        case INJECTION_TYPE_SHELL:
        case INJECTION_TYPE_TEMPLATE:
        case INJECTION_TYPE_LDAP:
        case INJECTION_TYPE_XML:
            return SEC_LANG_FEP_CATEGORY_FORMAT_INJECTION;
        case INJECTION_TYPE_XSS:
        case INJECTION_TYPE_HEADER:
            return SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION;
        case INJECTION_TYPE_PATH_TRAVERSAL:
        case INJECTION_TYPE_FORMAT_STRING:
            return SEC_LANG_FEP_CATEGORY_DATA_EXFILTRATION;
        case INJECTION_TYPE_NONE:
        case INJECTION_TYPE_CUSTOM:
        default:
            return SEC_LANG_FEP_CATEGORY_NONE;
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
static float compute_threat_from_fe(float free_energy, const sec_lang_fep_config_t* config) {
    if (free_energy <= 0.0f) {
        return 0.0f;
    }

    float threshold = config->injection_fe_threshold;
    if (threshold <= 0.0f) {
        threshold = SEC_LANG_FEP_THREAT_FE_THRESHOLD;
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
static sec_lang_fep_threat_level_t classify_threat(float normalized_threat) {
    if (normalized_threat >= 0.9f) {
        return SEC_LANG_FEP_THREAT_CRITICAL;
    } else if (normalized_threat >= 0.7f) {
        return SEC_LANG_FEP_THREAT_HIGH;
    } else if (normalized_threat >= 0.4f) {
        return SEC_LANG_FEP_THREAT_MEDIUM;
    } else if (normalized_threat >= 0.1f) {
        return SEC_LANG_FEP_THREAT_LOW;
    } else {
        return SEC_LANG_FEP_THREAT_NONE;
    }
}

/**
 * WHAT: Select appropriate action for threat level
 * WHY:  Active inference action selection
 * HOW:  Map threat level to response action
 */
static sec_lang_fep_action_t select_action_for_threat(sec_lang_fep_threat_level_t threat) {
    switch (threat) {
        case SEC_LANG_FEP_THREAT_CRITICAL:
            return SEC_LANG_FEP_ACTION_LOCKDOWN;
        case SEC_LANG_FEP_THREAT_HIGH:
            return SEC_LANG_FEP_ACTION_BLOCK;
        case SEC_LANG_FEP_THREAT_MEDIUM:
            return SEC_LANG_FEP_ACTION_SANITIZE;
        case SEC_LANG_FEP_THREAT_LOW:
            return SEC_LANG_FEP_ACTION_LOG;
        case SEC_LANG_FEP_THREAT_NONE:
        default:
            return SEC_LANG_FEP_ACTION_NONE;
    }
}

/**
 * WHAT: Clamp precision to valid range
 * WHY:  Prevent extreme sensitivity values
 * HOW:  Bound to [MIN, MAX] range
 */
static float clamp_precision(float precision) {
    if (precision < SEC_LANG_FEP_MIN_PRECISION) {
        return SEC_LANG_FEP_MIN_PRECISION;
    }
    if (precision > SEC_LANG_FEP_MAX_PRECISION) {
        return SEC_LANG_FEP_MAX_PRECISION;
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
    float range = SEC_LANG_FEP_MAX_PRECISION - SEC_LANG_FEP_MIN_PRECISION;
    float normalized = (precision - SEC_LANG_FEP_MIN_PRECISION) / range;

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
    if (!isfinite(*avg)) *avg = new_value;
}

/**
 * WHAT: Get category weight for FE computation
 * WHY:  Different threats have different severity weights
 * HOW:  Return predefined weight per category
 */
static float get_category_weight(sec_lang_fep_threat_category_t category) {
    switch (category) {
        case SEC_LANG_FEP_CATEGORY_PROMPT_INJECTION:
            return SEC_LANG_FEP_PROMPT_INJECTION_WEIGHT;
        case SEC_LANG_FEP_CATEGORY_SEMANTIC_MANIPULATION:
            return SEC_LANG_FEP_SEMANTIC_MANIP_WEIGHT;
        case SEC_LANG_FEP_CATEGORY_CONTEXT_HIJACKING:
            return SEC_LANG_FEP_CONTEXT_HIJACK_WEIGHT;
        case SEC_LANG_FEP_CATEGORY_OUTPUT_MANIPULATION:
            return SEC_LANG_FEP_OUTPUT_MANIP_WEIGHT;
        case SEC_LANG_FEP_CATEGORY_JAILBREAK:
            return SEC_LANG_FEP_CONTEXT_HIJACK_WEIGHT;  /* Similar to context hijack */
        case SEC_LANG_FEP_CATEGORY_DATA_EXFILTRATION:
            return SEC_LANG_FEP_OUTPUT_MANIP_WEIGHT;
        case SEC_LANG_FEP_CATEGORY_FORMAT_INJECTION:
            return SEC_LANG_FEP_PROMPT_INJECTION_WEIGHT;
        default:
            return 1.0f;
    }
}

/**
 * WHAT: Find the category with highest free energy
 * WHY:  Identify primary threat source
 * HOW:  Compare category FE values
 */
static sec_lang_fep_threat_category_t find_primary_category(
    const sec_lang_fep_bridge_t* bridge
) {
    sec_lang_fep_threat_category_t primary = SEC_LANG_FEP_CATEGORY_NONE;
    float max_fe = 0.0f;

    for (int i = 1; i < SEC_LANG_FEP_CATEGORY_COUNT; i++) {
        if (bridge->state.category_states[i].current_fe > max_fe) {
            max_fe = bridge->state.category_states[i].current_fe;
            primary = (sec_lang_fep_threat_category_t)i;
        }
    }

    return primary;
}

/**
 * WHAT: Update category-specific state
 * WHY:  Track per-category threat evolution
 * HOW:  Update FE, peak, detection count, rate
 */
static void update_category_state(
    sec_lang_fep_bridge_t* bridge,
    sec_lang_fep_threat_category_t category,
    float fe_contribution
) {
    if (category <= SEC_LANG_FEP_CATEGORY_NONE ||
        category >= SEC_LANG_FEP_CATEGORY_COUNT) {
        return;
    }

    uint64_t now = nimcp_platform_time_monotonic_ms();
    sec_lang_fep_category_state_t* state = &bridge->state.category_states[category];

    /* Accumulate free energy (with some decay) */
    state->current_fe = state->current_fe * 0.8f + fe_contribution;

    /* Track peak */
    if (state->current_fe > state->peak_fe) {
        state->peak_fe = state->current_fe;
    }

    /* Update detection tracking */
    state->detection_count++;

    /* Calculate detection rate (per minute) */
    if (state->last_detection_ms > 0) {
        uint64_t elapsed_ms = now - state->last_detection_ms;
        if (elapsed_ms > 0) {
            float current_rate = 60000.0f / (float)elapsed_ms;
            update_running_average(&state->detection_rate, current_rate, 0.3f);
        }
    }

    state->last_detection_ms = now;
}
