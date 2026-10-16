/**
 * @file nimcp_hypo_imagination_fep_bridge.c
 * @brief Implementation of Hypothalamus Imagination - FEP bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for drive-modulated imagination
 * WHY:  Enable drives to shape scenario priority; fantasy vs reality as FE
 * HOW:  Map drive state to imagination mode, use FEP for reality grounding
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_imagination_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_imagination_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void extract_drive_features(
    hypo_img_fep_bridge_t* bridge,
    float* features
);

static hypo_img_mode_t determine_imagination_mode(
    const float* drive_features,
    float free_energy,
    const hypo_img_fep_config_t* config
);

static hypo_img_plausibility_t classify_plausibility(
    float free_energy,
    const hypo_img_fep_config_t* config
);

static void update_running_averages(
    hypo_img_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_img_fep_default_config(hypo_img_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Drive-FEP coupling weights */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.0f;
    config->precision_modulation = 1.0f;

    /* Imagination parameters */
    config->realistic_threshold = HYPO_IMG_FEP_REALISTIC_THRESHOLD;
    config->fantasy_threshold = HYPO_IMG_FEP_FANTASY_THRESHOLD;
    config->creativity_factor = 0.5f;

    /* Drive influence weights */
    config->curiosity_weight = 1.0f;
    config->safety_weight = 0.8f;
    config->social_weight = 0.7f;

    /* Reality grounding */
    config->reality_anchor = 0.5f;
    config->plausibility_threshold = 0.3f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = HYPO_IMG_FEP_DEFAULT_LEARNING_RATE;
    config->precision_learning_rate = HYPO_IMG_FEP_PRECISION_LEARNING_RATE;

    /* Active inference */
    config->enable_active_inference = true;
    config->action_temperature = 1.0f;

    /* Bio-async */
    config->enable_bio_async = true;

    /* Sensitivity factors */
    config->fep_sensitivity = 1.0f;
    config->drive_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

hypo_img_fep_bridge_t* hypo_img_fep_create(
    const hypo_img_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo IMG FEP bridge: FEP system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    hypo_img_fep_bridge_t* bridge = (hypo_img_fep_bridge_t*)nimcp_malloc(sizeof(hypo_img_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo IMG FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_img_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(hypo_img_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_img_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->fep_system = fep_system;
    bridge->drive_system = drive_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hypo_imagination_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo IMG FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_img_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate feature buffers */
    bridge->drive_features = (float*)nimcp_malloc(HYPO_IMG_FEP_DRIVE_DIM * sizeof(float));
    bridge->scenario_features = (float*)nimcp_malloc(HYPO_IMG_FEP_SCENARIO_DIM * sizeof(float));
    bridge->reality_features = (float*)nimcp_malloc(HYPO_IMG_FEP_REALITY_DIM * sizeof(float));

    if (!bridge->drive_features || !bridge->scenario_features || !bridge->reality_features) {
        NIMCP_LOGGING_ERROR("Hypo IMG FEP bridge: feature buffer allocation failed");
        hypo_img_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_img_fep_create: required parameter is NULL (bridge->drive_features, bridge->scenario_features, bridge->reality_features)");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_IMG_FEP_DEFAULT_PRECISION;
    bridge->state.current_mode = HYPO_IMG_MODE_REALISTIC;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Hypo IMG FEP bridge created successfully");
    return bridge;
}

void hypo_img_fep_destroy(hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        hypo_img_fep_disconnect_bio_async(bridge);
    }

    /* Free feature buffers */
    if (bridge->drive_features) {
        nimcp_free(bridge->drive_features);
    }
    if (bridge->scenario_features) {
        nimcp_free(bridge->scenario_features);
    }
    if (bridge->reality_features) {
        nimcp_free(bridge->reality_features);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo IMG FEP bridge destroyed");
}

int hypo_img_fep_reset(hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(hypo_img_fep_state_t));
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_IMG_FEP_DEFAULT_PRECISION;
    bridge->state.current_mode = HYPO_IMG_MODE_REALISTIC;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_img_fep_stats_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_img_effects_t));
    memset(&bridge->img_effects, 0, sizeof(img_to_fep_effects_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo IMG FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int hypo_img_fep_update(hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Extract current drive features */
    extract_drive_features(bridge, bridge->drive_features);

    /* Process drives through FEP */
    fep_process_observation(bridge->fep_system, bridge->drive_features, HYPO_IMG_FEP_DRIVE_DIM);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Store FEP metrics */
    bridge->fep_effects.free_energy = fe.total;
    bridge->fep_effects.prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Determine imagination mode */
    bridge->fep_effects.current_mode = determine_imagination_mode(
        bridge->drive_features, fe.total, &bridge->config);
    bridge->state.current_mode = bridge->fep_effects.current_mode;

    /* Compute creativity level (inverse of reality anchoring) */
    float normalized_fe = fe.total / bridge->config.fantasy_threshold;
    if (normalized_fe > 1.0f) normalized_fe = 1.0f;
    bridge->fep_effects.creativity_level = normalized_fe * bridge->config.creativity_factor;

    /* Compute reality anchoring (inverse of free energy) */
    bridge->fep_effects.reality_anchoring =
        bridge->config.reality_anchor * (1.0f - normalized_fe);

    /* Determine plausibility */
    bridge->fep_effects.plausibility = classify_plausibility(fe.total, &bridge->config);
    bridge->fep_effects.plausibility_score = 1.0f - normalized_fe;
    bridge->state.last_plausibility = bridge->fep_effects.plausibility;

    /* Compute novelty (based on surprise) */
    float surprise = fep_compute_surprise(bridge->fep_system);
    bridge->fep_effects.novelty_score = surprise / 10.0f;
    if (bridge->fep_effects.novelty_score > 1.0f) {
        bridge->fep_effects.novelty_score = 1.0f;
    }

    /* Compute utility (high for moderate FE, low for extremes) */
    float mid_fe = (bridge->config.realistic_threshold + bridge->config.fantasy_threshold) / 2.0f;
    float utility = 1.0f - fabsf(fe.total - mid_fe) / mid_fe;
    if (utility < 0.0f) utility = 0.0f;
    bridge->fep_effects.utility_score = utility;

    /* Compute drive influences */
    bridge->fep_effects.curiosity_influence = bridge->drive_features[0] * bridge->config.curiosity_weight;
    bridge->fep_effects.safety_influence = bridge->drive_features[1] * bridge->config.safety_weight;
    bridge->fep_effects.social_influence = bridge->drive_features[2] * bridge->config.social_weight;

    /* Active inference */
    if (bridge->config.enable_active_inference) {
        fep_efe_t efe;
        if (bridge->fep_system->policies && bridge->fep_system->num_policies > 0) {
            fep_compute_efe(bridge->fep_system, &bridge->fep_system->policies[0], &efe);
            bridge->fep_effects.active_inference_strength = efe.total;
        }
    }

    /* Update running averages */
    update_running_averages(bridge, fe.total, bridge->fep_effects.prediction_error);

    /* Update state */
    bridge->state.update_count++;
    bridge->state.last_update_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    /* Update statistics */
    bridge->stats.bridge_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
    if (fe.total > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = fe.total;
    }

    /* Track mode usage */
    switch (bridge->state.current_mode) {
        case HYPO_IMG_MODE_REALISTIC:
            bridge->stats.realistic_count++;
            break;
        case HYPO_IMG_MODE_CREATIVE:
            bridge->stats.creative_count++;
            break;
        case HYPO_IMG_MODE_EXPLORATORY:
            bridge->stats.exploratory_count++;
            break;
        case HYPO_IMG_MODE_THREAT_SIM:
            bridge->stats.threat_sim_count++;
            break;
        case HYPO_IMG_MODE_SOCIAL_SIM:
            bridge->stats.social_sim_count++;
            break;
        case HYPO_IMG_MODE_FANTASY:
            bridge->stats.fantasy_count++;
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_img_fep_compute_fe(
    hypo_img_fep_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_compute_fe: bridge or free_energy is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);
    *free_energy = fe.total * bridge->config.drive_fe_weight;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_img_fep_modulate_precision(hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute prediction accuracy from validation history */
    float accuracy = 0.5f;
    if (bridge->img_effects.scenarios_generated > 0) {
        accuracy = (float)bridge->img_effects.scenarios_validated /
                   (float)bridge->img_effects.scenarios_generated;
    }

    /* Adjust precision based on accuracy */
    float target_precision = HYPO_IMG_FEP_DEFAULT_PRECISION * (1.0f + accuracy);

    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    if (bridge->state.current_precision < HYPO_IMG_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_IMG_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_IMG_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_IMG_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.avg_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_img_fep_get_effects(
    const hypo_img_fep_bridge_t* bridge,
    fep_to_img_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/* ============================================================================
 * Imagination API Implementation
 * ============================================================================ */

int hypo_img_fep_evaluate_scenario(
    hypo_img_fep_bridge_t* bridge,
    const float* scenario_features,
    uint32_t scenario_len,
    hypo_img_plausibility_t* plausibility,
    float* score
) {
    if (!bridge || !scenario_features || !plausibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_evaluate_scenario: required parameter is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Copy scenario features */
    uint32_t copy_len = scenario_len < HYPO_IMG_FEP_SCENARIO_DIM ?
                        scenario_len : HYPO_IMG_FEP_SCENARIO_DIM;
    memcpy(bridge->scenario_features, scenario_features, copy_len * sizeof(float));

    /* Process through FEP */
    fep_process_observation(bridge->fep_system, bridge->scenario_features, copy_len);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Classify plausibility */
    *plausibility = classify_plausibility(fe.total, &bridge->config);

    /* Compute numeric score */
    if (score) {
        float normalized = fe.total / bridge->config.fantasy_threshold;
        if (normalized > 1.0f) normalized = 1.0f;
        *score = 1.0f - normalized;
    }

    /* Update statistics */
    bridge->state.scenario_count++;
    bridge->stats.total_scenarios++;
    bridge->img_effects.scenarios_generated++;

    /* Track by mode */
    if (fe.total < bridge->config.realistic_threshold) {
        bridge->img_effects.realistic_scenarios++;
    } else if (fe.total < bridge->config.fantasy_threshold) {
        bridge->img_effects.creative_scenarios++;
    } else {
        bridge->img_effects.fantasy_scenarios++;
    }

    /* Update averages */
    float alpha = 0.1f;
    float plaus_score = score ? *score : (1.0f - fe.total / bridge->config.fantasy_threshold);
    bridge->img_effects.avg_plausibility =
        (1.0f - alpha) * bridge->img_effects.avg_plausibility + alpha * plaus_score;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_img_fep_get_mode(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_mode_t* mode
) {
    if (!bridge || !mode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_get_mode: bridge or mode is NULL");
        return -1;
    }

    *mode = bridge->state.current_mode;
    return 0;
}

int hypo_img_fep_update_from_validation(
    hypo_img_fep_bridge_t* bridge,
    bool was_validated,
    const float* actual_features,
    uint32_t actual_len
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Track validation outcome */
    if (was_validated) {
        bridge->img_effects.scenarios_validated++;
    } else {
        bridge->img_effects.scenarios_invalidated++;
    }

    /* Process actual outcome through FEP if provided */
    if (actual_features && actual_len > 0 && bridge->config.enable_online_learning) {
        uint32_t copy_len = actual_len < HYPO_IMG_FEP_REALITY_DIM ?
                            actual_len : HYPO_IMG_FEP_REALITY_DIM;
        memcpy(bridge->reality_features, actual_features, copy_len * sizeof(float));

        fep_process_observation(bridge->fep_system, bridge->reality_features, copy_len);
        fep_update_beliefs(bridge->fep_system);
    }

    /* Suggest drive updates */
    if (was_validated) {
        /* Good prediction: reinforce curiosity */
        bridge->img_effects.curiosity_drive_update = 0.05f;
    } else {
        /* Bad prediction: increase safety/caution */
        bridge->img_effects.safety_drive_update = 0.1f;
        bridge->img_effects.curiosity_drive_update = -0.05f;
    }

    /* Update precision (inline to avoid deadlock) */
    {
        float acc = 0.5f;
        if (bridge->img_effects.scenarios_generated > 0) {
            acc = (float)bridge->img_effects.scenarios_validated /
                  (float)bridge->img_effects.scenarios_generated;
        }
        float target_prec = HYPO_IMG_FEP_DEFAULT_PRECISION * (1.0f + acc);
        float prec_alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - prec_alpha) * bridge->state.current_precision + prec_alpha * target_prec;
        if (bridge->state.current_precision < HYPO_IMG_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_IMG_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_IMG_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_IMG_FEP_MAX_PRECISION;
        }
        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.avg_precision = bridge->state.current_precision;
    }

    /* Update prediction accuracy stat */
    if (bridge->img_effects.scenarios_generated > 0) {
        bridge->stats.prediction_accuracy =
            (float)bridge->img_effects.scenarios_validated /
            (float)bridge->img_effects.scenarios_generated;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int hypo_img_fep_get_state(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_get_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int hypo_img_fep_get_stats(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_img_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_img_fep_connect_bio_async(hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_IMG_FEP,
        .module_name = "hypo_img_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo IMG FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_img_fep_disconnect_bio_async(hypo_img_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo IMG FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_img_fep_process_messages(
    hypo_img_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Bio-async uses handler-based callbacks, not message polling */
    (void)max_messages;
    return 0;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

void hypo_img_fep_print_summary(const hypo_img_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo IMG FEP Bridge: NULL\n");
        return;
    }

    printf("\n========== Hypo IMG FEP Bridge Summary ==========\n");
    printf("State:\n");
    printf("  Active:            %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update Count:      %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Scenario Count:    %lu\n", (unsigned long)bridge->state.scenario_count);
    printf("  Current Mode:      %s\n", hypo_img_mode_to_string(bridge->state.current_mode));
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy:   %.3f\n", bridge->state.avg_free_energy);

    printf("\nFEP Effects:\n");
    printf("  Creativity Level:  %.3f\n", bridge->fep_effects.creativity_level);
    printf("  Reality Anchoring: %.3f\n", bridge->fep_effects.reality_anchoring);
    printf("  Plausibility:      %s (%.3f)\n",
           hypo_img_plausibility_to_string(bridge->fep_effects.plausibility),
           bridge->fep_effects.plausibility_score);
    printf("  Novelty Score:     %.3f\n", bridge->fep_effects.novelty_score);
    printf("  Utility Score:     %.3f\n", bridge->fep_effects.utility_score);
    printf("  Free Energy:       %.3f\n", bridge->fep_effects.free_energy);

    printf("\nStatistics:\n");
    printf("  Total Scenarios:   %lu\n", (unsigned long)bridge->stats.total_scenarios);
    printf("  Realistic:         %lu\n", (unsigned long)bridge->stats.realistic_count);
    printf("  Creative:          %lu\n", (unsigned long)bridge->stats.creative_count);
    printf("  Exploratory:       %lu\n", (unsigned long)bridge->stats.exploratory_count);
    printf("  Threat Sim:        %lu\n", (unsigned long)bridge->stats.threat_sim_count);
    printf("  Fantasy:           %lu\n", (unsigned long)bridge->stats.fantasy_count);
    printf("  Prediction Accuracy: %.3f\n", bridge->stats.prediction_accuracy);

    printf("\nBio-Async: %s\n", bridge->base.bio_async_enabled ? "connected" : "disconnected");
    printf("==============================================\n\n");
}

const char* hypo_img_mode_to_string(hypo_img_mode_t mode) {
    switch (mode) {
        case HYPO_IMG_MODE_REALISTIC:
            return "realistic";
        case HYPO_IMG_MODE_CREATIVE:
            return "creative";
        case HYPO_IMG_MODE_EXPLORATORY:
            return "exploratory";
        case HYPO_IMG_MODE_THREAT_SIM:
            return "threat_simulation";
        case HYPO_IMG_MODE_SOCIAL_SIM:
            return "social_simulation";
        case HYPO_IMG_MODE_FANTASY:
            return "fantasy";
        default:
            return "unknown";
    }
}

const char* hypo_img_plausibility_to_string(hypo_img_plausibility_t plausibility) {
    switch (plausibility) {
        case HYPO_IMG_PLAUS_CERTAIN:
            return "certain";
        case HYPO_IMG_PLAUS_PROBABLE:
            return "probable";
        case HYPO_IMG_PLAUS_POSSIBLE:
            return "possible";
        case HYPO_IMG_PLAUS_UNLIKELY:
            return "unlikely";
        case HYPO_IMG_PLAUS_FANTASY:
            return "fantasy";
        default:
            return "unknown";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static void extract_drive_features(
    hypo_img_fep_bridge_t* bridge,
    float* features
) {
    if (!bridge || !features) {
        return;
    }

    /* Initialize with defaults if no drive system */
    if (!bridge->drive_system) {
        features[0] = 0.5f;  /* CURIOSITY */
        features[1] = 0.5f;  /* SAFETY */
        features[2] = 0.5f;  /* SOCIAL */
        features[3] = 0.5f;  /* Reserved */
        return;
    }

    /* Extract drive states */
    hypo_drive_state_t drive_state;

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_CURIOSITY, &drive_state)) {
        features[0] = drive_state.level * drive_state.urgency;
    } else {
        features[0] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SAFETY, &drive_state)) {
        features[1] = drive_state.level * drive_state.urgency;
    } else {
        features[1] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SOCIAL, &drive_state)) {
        features[2] = drive_state.level * drive_state.urgency;
    } else {
        features[2] = 0.5f;
    }

    features[3] = 0.5f;  /* Reserved for future use */
}

static hypo_img_mode_t determine_imagination_mode(
    const float* drive_features,
    float free_energy,
    const hypo_img_fep_config_t* config
) {
    if (!drive_features || !config) {
        return HYPO_IMG_MODE_REALISTIC;
    }

    /* Check for fantasy mode first (very high FE) */
    if (free_energy > config->fantasy_threshold) {
        return HYPO_IMG_MODE_FANTASY;
    }

    /* Find dominant drive */
    float curiosity = drive_features[0] * config->curiosity_weight;
    float safety = drive_features[1] * config->safety_weight;
    float social = drive_features[2] * config->social_weight;

    /* Determine mode based on dominant drive and FE level */
    if (safety > curiosity && safety > social) {
        return HYPO_IMG_MODE_THREAT_SIM;
    }

    if (social > curiosity && social > safety) {
        return HYPO_IMG_MODE_SOCIAL_SIM;
    }

    if (curiosity > 0.6f) {
        return HYPO_IMG_MODE_EXPLORATORY;
    }

    if (free_energy < config->realistic_threshold) {
        return HYPO_IMG_MODE_REALISTIC;
    }

    return HYPO_IMG_MODE_CREATIVE;
}

static hypo_img_plausibility_t classify_plausibility(
    float free_energy,
    const hypo_img_fep_config_t* config
) {
    if (!config) {
        return HYPO_IMG_PLAUS_POSSIBLE;
    }

    float range = config->fantasy_threshold - config->realistic_threshold;
    if (range <= 0.0f) {
        range = 8.0f;  /* Default range */
    }

    float quarter = range / 4.0f;

    if (free_energy < config->realistic_threshold) {
        return HYPO_IMG_PLAUS_CERTAIN;
    }
    if (free_energy < config->realistic_threshold + quarter) {
        return HYPO_IMG_PLAUS_PROBABLE;
    }
    if (free_energy < config->realistic_threshold + 2 * quarter) {
        return HYPO_IMG_PLAUS_POSSIBLE;
    }
    if (free_energy < config->fantasy_threshold) {
        return HYPO_IMG_PLAUS_UNLIKELY;
    }

    return HYPO_IMG_PLAUS_FANTASY;
}

static void update_running_averages(
    hypo_img_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * free_energy;
    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * prediction_error;
}
