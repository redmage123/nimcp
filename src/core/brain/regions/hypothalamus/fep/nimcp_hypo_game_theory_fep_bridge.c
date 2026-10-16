/**
 * @file nimcp_hypo_game_theory_fep_bridge.c
 * @brief Implementation of Hypothalamus Game Theory - FEP bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for drive-modulated game-theoretic strategy selection
 * WHY:  Enable SOCIAL drive to influence cooperation vs competition via FEP
 * HOW:  Map drive state to strategy priors, use FEP for learning
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_game_theory_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_game_theory_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void extract_drive_features(
    hypo_gt_fep_bridge_t* bridge,
    float* features
);

static float compute_cooperation_score(
    const float* drive_features,
    const hypo_gt_fep_config_t* config
);

static float compute_competition_score(
    const float* drive_features,
    const hypo_gt_fep_config_t* config
);

static void update_running_averages(
    hypo_gt_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
);

static hypo_gt_partner_type_t classify_partner_from_history(
    const hypo_gt_fep_bridge_t* bridge
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_gt_fep_default_config(hypo_gt_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Drive-FEP coupling weights */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.0f;
    config->precision_modulation = 1.0f;

    /* Strategy parameters */
    config->cooperation_threshold = HYPO_GT_FEP_COOPERATIVE_THRESHOLD;
    config->competition_threshold = HYPO_GT_FEP_COMPETITIVE_THRESHOLD;
    config->exploration_rate = 0.1f;

    /* Drive influence weights */
    config->social_weight = 1.0f;
    config->safety_weight = 0.8f;
    config->autonomy_weight = 0.6f;
    config->competence_weight = 0.4f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = HYPO_GT_FEP_DEFAULT_LEARNING_RATE;
    config->precision_learning_rate = HYPO_GT_FEP_PRECISION_LEARNING_RATE;

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

hypo_gt_fep_bridge_t* hypo_gt_fep_create(
    const hypo_gt_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo GT FEP bridge: FEP system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    hypo_gt_fep_bridge_t* bridge = (hypo_gt_fep_bridge_t*)nimcp_malloc(sizeof(hypo_gt_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo GT FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_gt_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(hypo_gt_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_gt_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->fep_system = fep_system;
    bridge->drive_system = drive_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hypo_game_theory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo GT FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_gt_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate feature buffers */
    bridge->drive_features = (float*)nimcp_malloc(HYPO_GT_FEP_DRIVE_DIM * sizeof(float));
    bridge->strategy_features = (float*)nimcp_malloc(HYPO_GT_FEP_STRATEGY_DIM * sizeof(float));
    bridge->partner_features = (float*)nimcp_malloc(HYPO_GT_FEP_PARTNER_DIM * sizeof(float));

    if (!bridge->drive_features || !bridge->strategy_features || !bridge->partner_features) {
        NIMCP_LOGGING_ERROR("Hypo GT FEP bridge: feature buffer allocation failed");
        hypo_gt_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_gt_fep_create: required parameter is NULL (bridge->drive_features, bridge->strategy_features, bridge->partner_features)");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_GT_FEP_DEFAULT_PRECISION;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Hypo GT FEP bridge created successfully");
    return bridge;
}

void hypo_gt_fep_destroy(hypo_gt_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        hypo_gt_fep_disconnect_bio_async(bridge);
    }

    /* Free feature buffers */
    if (bridge->drive_features) {
        nimcp_free(bridge->drive_features);
    }
    if (bridge->strategy_features) {
        nimcp_free(bridge->strategy_features);
    }
    if (bridge->partner_features) {
        nimcp_free(bridge->partner_features);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo GT FEP bridge destroyed");
}

int hypo_gt_fep_reset(hypo_gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(hypo_gt_fep_state_t));
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_GT_FEP_DEFAULT_PRECISION;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_gt_fep_stats_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_gt_effects_t));
    memset(&bridge->gt_effects, 0, sizeof(gt_to_fep_effects_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo GT FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int hypo_gt_fep_update(hypo_gt_fep_bridge_t* bridge) {
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
    fep_process_observation(bridge->fep_system, bridge->drive_features, HYPO_GT_FEP_DRIVE_DIM);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Compute cooperation and competition scores */
    bridge->fep_effects.cooperation_score = compute_cooperation_score(
        bridge->drive_features, &bridge->config);
    bridge->fep_effects.competition_score = compute_competition_score(
        bridge->drive_features, &bridge->config);

    /* Compute exploration score based on uncertainty */
    float uncertainty = fe.total / bridge->config.competition_threshold;
    if (uncertainty > 1.0f) uncertainty = 1.0f;
    bridge->fep_effects.exploration_score = uncertainty * bridge->config.exploration_rate;

    /* Store FEP metrics */
    bridge->fep_effects.free_energy = fe.total;
    bridge->fep_effects.prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Determine recommended strategy */
    if (fe.total < bridge->config.cooperation_threshold) {
        bridge->fep_effects.recommended_strategy = HYPO_GT_STRATEGY_COOPERATIVE;
    } else if (fe.total > bridge->config.competition_threshold) {
        bridge->fep_effects.recommended_strategy = HYPO_GT_STRATEGY_COMPETITIVE;
    } else if (bridge->fep_effects.cooperation_score > bridge->fep_effects.competition_score) {
        bridge->fep_effects.recommended_strategy = HYPO_GT_STRATEGY_RECIPROCAL;
    } else {
        bridge->fep_effects.recommended_strategy = HYPO_GT_STRATEGY_MIXED;
    }

    /* Compute strategy confidence */
    float score_diff = fabsf(bridge->fep_effects.cooperation_score -
                             bridge->fep_effects.competition_score);
    bridge->fep_effects.strategy_confidence = score_diff /
        (bridge->fep_effects.cooperation_score + bridge->fep_effects.competition_score + 0.01f);

    /* Active inference */
    if (bridge->config.enable_active_inference) {
        fep_efe_t efe;
        if (bridge->fep_system->policies && bridge->fep_system->num_policies > 0) {
            fep_compute_efe(bridge->fep_system, &bridge->fep_system->policies[0], &efe);
            bridge->fep_effects.active_inference_strength = efe.total;
        }
    }

    /* Infer partner type */
    bridge->fep_effects.partner_type = classify_partner_from_history(bridge);

    /* Update running averages */
    update_running_averages(bridge, fe.total, bridge->fep_effects.prediction_error);

    /* Update state */
    bridge->state.update_count++;
    bridge->state.last_strategy = bridge->fep_effects.recommended_strategy;
    bridge->state.last_partner = bridge->fep_effects.partner_type;
    bridge->state.last_update_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    /* Update statistics */
    bridge->stats.bridge_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
    if (fe.total > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = fe.total;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gt_fep_compute_fe(
    hypo_gt_fep_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_compute_fe: bridge or free_energy is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);
    *free_energy = fe.total * bridge->config.drive_fe_weight;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gt_fep_modulate_precision(hypo_gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute prediction accuracy */
    float accuracy = 1.0f - (bridge->state.avg_prediction_error / 10.0f);
    if (accuracy < 0.0f) accuracy = 0.0f;
    if (accuracy > 1.0f) accuracy = 1.0f;

    /* Adjust precision based on accuracy */
    float target_precision = HYPO_GT_FEP_DEFAULT_PRECISION * (1.0f + accuracy);

    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    if (bridge->state.current_precision < HYPO_GT_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_GT_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_GT_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_GT_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.avg_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gt_fep_get_effects(
    const hypo_gt_fep_bridge_t* bridge,
    fep_to_gt_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/* ============================================================================
 * Strategy Selection Implementation
 * ============================================================================ */

int hypo_gt_fep_select_strategy(
    hypo_gt_fep_bridge_t* bridge,
    hypo_gt_strategy_type_t* strategy,
    float* confidence
) {
    if (!bridge || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_select_strategy: bridge or strategy is NULL");
        return -1;
    }

    /* Update first to get latest state (unlocked - update handles its own locking) */
    hypo_gt_fep_update(bridge);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *strategy = bridge->fep_effects.recommended_strategy;
    if (confidence) {
        *confidence = bridge->fep_effects.strategy_confidence;
    }

    /* Update statistics */
    switch (*strategy) {
        case HYPO_GT_STRATEGY_COOPERATIVE:
            bridge->stats.strategy_cooperative++;
            break;
        case HYPO_GT_STRATEGY_COMPETITIVE:
            bridge->stats.strategy_competitive++;
            break;
        case HYPO_GT_STRATEGY_CAUTIOUS:
            bridge->stats.strategy_cautious++;
            break;
        case HYPO_GT_STRATEGY_MIXED:
            bridge->stats.strategy_mixed++;
            break;
        case HYPO_GT_STRATEGY_RECIPROCAL:
            bridge->stats.strategy_reciprocal++;
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gt_fep_update_from_outcome(
    hypo_gt_fep_bridge_t* bridge,
    float own_payoff,
    float partner_payoff,
    bool partner_cooperated
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update game counts */
    bridge->state.game_count++;
    bridge->stats.total_games++;

    /* Track outcome type */
    if (partner_cooperated && own_payoff > 0) {
        bridge->gt_effects.cooperative_outcomes++;
        bridge->stats.cooperative_games++;
    } else if (!partner_cooperated) {
        bridge->gt_effects.competitive_outcomes++;
        bridge->stats.competitive_games++;
        if (own_payoff < partner_payoff) {
            bridge->gt_effects.exploited_count++;
        }
    }

    /* Update average payoffs */
    float alpha = 0.1f;
    bridge->gt_effects.avg_payoff =
        (1.0f - alpha) * bridge->gt_effects.avg_payoff + alpha * own_payoff;
    bridge->gt_effects.avg_partner_payoff =
        (1.0f - alpha) * bridge->gt_effects.avg_partner_payoff + alpha * partner_payoff;

    /* Compute reciprocity score */
    if (bridge->stats.total_games > 0) {
        bridge->gt_effects.reciprocity_score =
            (float)bridge->gt_effects.cooperative_outcomes /
            (float)bridge->stats.total_games;
    }

    /* Suggest drive updates based on outcome */
    if (partner_cooperated) {
        bridge->gt_effects.social_drive_update = 0.1f;  /* Reinforce social */
    } else {
        bridge->gt_effects.safety_drive_update = 0.1f;  /* Increase caution */
        bridge->gt_effects.autonomy_drive_update = 0.05f;  /* Encourage independence */
    }

    /* Update FEP with new observation */
    if (bridge->config.enable_online_learning) {
        float outcome_features[4] = {
            own_payoff,
            partner_payoff,
            partner_cooperated ? 1.0f : 0.0f,
            bridge->gt_effects.reciprocity_score
        };
        fep_process_observation(bridge->fep_system, outcome_features, 4);
        fep_update_beliefs(bridge->fep_system);
    }

    /* Update precision based on prediction accuracy (inline to avoid deadlock) */
    {
        float acc = 1.0f - (bridge->state.avg_prediction_error / 10.0f);
        if (acc < 0.0f) acc = 0.0f;
        if (acc > 1.0f) acc = 1.0f;
        float target_prec = HYPO_GT_FEP_DEFAULT_PRECISION * (1.0f + acc);
        float prec_alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - prec_alpha) * bridge->state.current_precision + prec_alpha * target_prec;
        if (bridge->state.current_precision < HYPO_GT_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_GT_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_GT_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_GT_FEP_MAX_PRECISION;
        }
        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.avg_precision = bridge->state.current_precision;
    }

    bridge->stats.avg_payoff = bridge->gt_effects.avg_payoff;
    bridge->stats.cooperation_rate = bridge->gt_effects.reciprocity_score;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gt_fep_infer_partner(
    hypo_gt_fep_bridge_t* bridge,
    hypo_gt_partner_type_t* partner_type,
    float* confidence
) {
    if (!bridge || !partner_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_infer_partner: bridge or partner_type is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *partner_type = classify_partner_from_history(bridge);

    if (confidence) {
        /* Confidence based on number of observations */
        float obs_factor = (float)bridge->stats.total_games / 10.0f;
        if (obs_factor > 1.0f) obs_factor = 1.0f;
        *confidence = obs_factor * bridge->fep_effects.partner_model_confidence;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int hypo_gt_fep_get_state(
    const hypo_gt_fep_bridge_t* bridge,
    hypo_gt_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_get_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int hypo_gt_fep_get_stats(
    const hypo_gt_fep_bridge_t* bridge,
    hypo_gt_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gt_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_gt_fep_connect_bio_async(hypo_gt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_GT_FEP,
        .module_name = "hypo_gt_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo GT FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_gt_fep_disconnect_bio_async(hypo_gt_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo GT FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_gt_fep_process_messages(
    hypo_gt_fep_bridge_t* bridge,
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

void hypo_gt_fep_print_summary(const hypo_gt_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo GT FEP Bridge: NULL\n");
        return;
    }

    printf("\n========== Hypo GT FEP Bridge Summary ==========\n");
    printf("State:\n");
    printf("  Active:            %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update Count:      %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Game Count:        %lu\n", (unsigned long)bridge->state.game_count);
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy:   %.3f\n", bridge->state.avg_free_energy);
    printf("  Last Strategy:     %s\n", hypo_gt_strategy_to_string(bridge->state.last_strategy));
    printf("  Last Partner:      %s\n", hypo_gt_partner_to_string(bridge->state.last_partner));

    printf("\nFEP Effects:\n");
    printf("  Cooperation Score: %.3f\n", bridge->fep_effects.cooperation_score);
    printf("  Competition Score: %.3f\n", bridge->fep_effects.competition_score);
    printf("  Exploration Score: %.3f\n", bridge->fep_effects.exploration_score);
    printf("  Strategy Confidence: %.3f\n", bridge->fep_effects.strategy_confidence);
    printf("  Free Energy:       %.3f\n", bridge->fep_effects.free_energy);
    printf("  Prediction Error:  %.3f\n", bridge->fep_effects.prediction_error);

    printf("\nStatistics:\n");
    printf("  Total Games:       %lu\n", (unsigned long)bridge->stats.total_games);
    printf("  Cooperative:       %lu\n", (unsigned long)bridge->stats.cooperative_games);
    printf("  Competitive:       %lu\n", (unsigned long)bridge->stats.competitive_games);
    printf("  Avg Payoff:        %.3f\n", bridge->stats.avg_payoff);
    printf("  Cooperation Rate:  %.3f\n", bridge->stats.cooperation_rate);

    printf("\nBio-Async: %s\n", bridge->base.bio_async_enabled ? "connected" : "disconnected");
    printf("==============================================\n\n");
}

const char* hypo_gt_strategy_to_string(hypo_gt_strategy_type_t strategy) {
    switch (strategy) {
        case HYPO_GT_STRATEGY_COOPERATIVE:
            return "cooperative";
        case HYPO_GT_STRATEGY_COMPETITIVE:
            return "competitive";
        case HYPO_GT_STRATEGY_CAUTIOUS:
            return "cautious";
        case HYPO_GT_STRATEGY_MIXED:
            return "mixed";
        case HYPO_GT_STRATEGY_RECIPROCAL:
            return "reciprocal";
        default:
            return "unknown";
    }
}

const char* hypo_gt_partner_to_string(hypo_gt_partner_type_t partner) {
    switch (partner) {
        case HYPO_GT_PARTNER_UNKNOWN:
            return "unknown";
        case HYPO_GT_PARTNER_COOPERATIVE:
            return "cooperative";
        case HYPO_GT_PARTNER_COMPETITIVE:
            return "competitive";
        case HYPO_GT_PARTNER_MIXED:
            return "mixed";
        case HYPO_GT_PARTNER_EXPLOITATIVE:
            return "exploitative";
        default:
            return "unknown";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static void extract_drive_features(
    hypo_gt_fep_bridge_t* bridge,
    float* features
) {
    if (!bridge || !features) {
        return;
    }

    /* Initialize with defaults if no drive system */
    if (!bridge->drive_system) {
        features[0] = 0.5f;  /* SOCIAL */
        features[1] = 0.5f;  /* SAFETY */
        features[2] = 0.5f;  /* AUTONOMY */
        features[3] = 0.5f;  /* COMPETENCE */
        return;
    }

    /* Extract drive states */
    hypo_drive_state_t drive_state;

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SOCIAL, &drive_state)) {
        features[0] = drive_state.level * drive_state.urgency;
    } else {
        features[0] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SAFETY, &drive_state)) {
        features[1] = drive_state.level * drive_state.urgency;
    } else {
        features[1] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_AUTONOMY, &drive_state)) {
        features[2] = drive_state.level * drive_state.urgency;
    } else {
        features[2] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_COMPETENCE, &drive_state)) {
        features[3] = drive_state.level * drive_state.urgency;
    } else {
        features[3] = 0.5f;
    }
}

static float compute_cooperation_score(
    const float* drive_features,
    const hypo_gt_fep_config_t* config
) {
    if (!drive_features || !config) {
        return 0.5f;
    }

    /* SOCIAL drive promotes cooperation */
    float coop_score = drive_features[0] * config->social_weight;

    /* SAFETY drive adds caution (moderate cooperation) */
    coop_score += drive_features[1] * config->safety_weight * 0.5f;

    /* Normalize */
    float total_weight = config->social_weight + config->safety_weight * 0.5f;
    if (total_weight > 0.0f) {
        coop_score /= total_weight;
    }

    return coop_score > 1.0f ? 1.0f : coop_score;
}

static float compute_competition_score(
    const float* drive_features,
    const hypo_gt_fep_config_t* config
) {
    if (!drive_features || !config) {
        return 0.5f;
    }

    /* AUTONOMY drive promotes competition */
    float comp_score = drive_features[2] * config->autonomy_weight;

    /* COMPETENCE drive adds achievement motivation */
    comp_score += drive_features[3] * config->competence_weight;

    /* Normalize */
    float total_weight = config->autonomy_weight + config->competence_weight;
    if (total_weight > 0.0f) {
        comp_score /= total_weight;
    }

    return comp_score > 1.0f ? 1.0f : comp_score;
}

static void update_running_averages(
    hypo_gt_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * free_energy;
    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * prediction_error;
}

static hypo_gt_partner_type_t classify_partner_from_history(
    const hypo_gt_fep_bridge_t* bridge
) {
    if (!bridge || bridge->stats.total_games < 3) {
        return HYPO_GT_PARTNER_UNKNOWN;
    }

    float coop_rate = (float)bridge->gt_effects.cooperative_outcomes /
                      (float)bridge->stats.total_games;
    float exploit_rate = (float)bridge->gt_effects.exploited_count /
                         (float)bridge->stats.total_games;

    if (coop_rate > 0.7f) {
        return HYPO_GT_PARTNER_COOPERATIVE;
    } else if (exploit_rate > 0.5f) {
        return HYPO_GT_PARTNER_EXPLOITATIVE;
    } else if (coop_rate < 0.3f) {
        return HYPO_GT_PARTNER_COMPETITIVE;
    } else {
        return HYPO_GT_PARTNER_MIXED;
    }
}
