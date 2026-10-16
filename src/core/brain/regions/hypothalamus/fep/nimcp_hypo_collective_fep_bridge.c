/**
 * @file nimcp_hypo_collective_fep_bridge.c
 * @brief Implementation of Hypothalamus Collective - FEP bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for drive-modulated collective alignment
 * WHY:  Enable collective goals to translate to individual drives; consensus as FE
 * HOW:  Map collective state to drive priorities, use FEP for alignment
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_collective_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_collective_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void extract_drive_features(
    hypo_col_fep_bridge_t* bridge,
    float* features
);

static hypo_col_state_t classify_alignment_state(
    float free_energy,
    float alignment_score,
    const hypo_col_fep_config_t* config
);

static hypo_col_action_t determine_action(
    hypo_col_state_t state,
    const float* drive_features,
    const hypo_col_fep_config_t* config
);

static float compute_consensus_distance(
    const float* collective_features,
    uint32_t collective_len,
    const float* individual_features,
    uint32_t individual_len
);

static void update_running_averages(
    hypo_col_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_col_fep_default_config(hypo_col_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Drive-FEP coupling weights */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.0f;
    config->precision_modulation = 1.0f;

    /* Alignment parameters */
    config->alignment_threshold = HYPO_COL_FEP_ALIGNED_THRESHOLD;
    config->deviance_threshold = HYPO_COL_FEP_DEVIANT_THRESHOLD;
    config->consensus_weight = 1.0f;

    /* Drive influence weights */
    config->social_weight = 1.0f;
    config->autonomy_weight = 0.8f;
    config->safety_weight = 0.7f;

    /* Balance parameters */
    config->conformity_pressure = 0.5f;
    config->independence_value = 0.5f;
    config->leadership_threshold = 0.8f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = HYPO_COL_FEP_DEFAULT_LEARNING_RATE;
    config->precision_learning_rate = HYPO_COL_FEP_PRECISION_LEARNING_RATE;

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

hypo_col_fep_bridge_t* hypo_col_fep_create(
    const hypo_col_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo COL FEP bridge: FEP system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    hypo_col_fep_bridge_t* bridge = (hypo_col_fep_bridge_t*)nimcp_malloc(sizeof(hypo_col_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo COL FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_col_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(hypo_col_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_col_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->fep_system = fep_system;
    bridge->drive_system = drive_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hypo_collective_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo COL FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_col_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate feature buffers */
    bridge->drive_features = (float*)nimcp_malloc(HYPO_COL_FEP_DRIVE_DIM * sizeof(float));
    bridge->collective_features = (float*)nimcp_malloc(HYPO_COL_FEP_COLLECTIVE_DIM * sizeof(float));
    bridge->individual_features = (float*)nimcp_malloc(HYPO_COL_FEP_INDIVIDUAL_DIM * sizeof(float));

    if (!bridge->drive_features || !bridge->collective_features || !bridge->individual_features) {
        NIMCP_LOGGING_ERROR("Hypo COL FEP bridge: feature buffer allocation failed");
        hypo_col_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_col_fep_create: required parameter is NULL (bridge->drive_features, bridge->collective_features, bridge->individual_features)");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_COL_FEP_DEFAULT_PRECISION;
    bridge->state.current_state = HYPO_COL_STATE_MARGINAL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Hypo COL FEP bridge created successfully");
    return bridge;
}

void hypo_col_fep_destroy(hypo_col_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        hypo_col_fep_disconnect_bio_async(bridge);
    }

    /* Free feature buffers */
    if (bridge->drive_features) {
        nimcp_free(bridge->drive_features);
    }
    if (bridge->collective_features) {
        nimcp_free(bridge->collective_features);
    }
    if (bridge->individual_features) {
        nimcp_free(bridge->individual_features);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo COL FEP bridge destroyed");
}

int hypo_col_fep_reset(hypo_col_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(hypo_col_fep_state_t));
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_COL_FEP_DEFAULT_PRECISION;
    bridge->state.current_state = HYPO_COL_STATE_MARGINAL;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_col_fep_stats_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_col_effects_t));
    memset(&bridge->col_effects, 0, sizeof(col_to_fep_effects_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo COL FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int hypo_col_fep_update(hypo_col_fep_bridge_t* bridge) {
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
    fep_process_observation(bridge->fep_system, bridge->drive_features, HYPO_COL_FEP_DRIVE_DIM);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Store FEP metrics */
    bridge->fep_effects.free_energy = fe.total;
    bridge->fep_effects.prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Compute alignment vs independence scores based on drives */
    float social = bridge->drive_features[0] * bridge->config.social_weight;
    float autonomy = bridge->drive_features[1] * bridge->config.autonomy_weight;

    /* Alignment score: social drive weighted by conformity pressure */
    bridge->fep_effects.alignment_score = social * bridge->config.conformity_pressure;

    /* Independence score: autonomy drive weighted by independence value */
    bridge->fep_effects.independence_score = autonomy * bridge->config.independence_value;

    /* Normalize */
    float total = bridge->fep_effects.alignment_score + bridge->fep_effects.independence_score;
    if (total > 0.01f) {
        bridge->fep_effects.alignment_score /= total;
        bridge->fep_effects.independence_score /= total;
    } else {
        bridge->fep_effects.alignment_score = 0.5f;
        bridge->fep_effects.independence_score = 0.5f;
    }

    /* Classify alignment state */
    bridge->fep_effects.current_state = classify_alignment_state(
        fe.total, bridge->fep_effects.alignment_score, &bridge->config);
    bridge->state.current_state = bridge->fep_effects.current_state;

    /* Compute consensus metrics */
    bridge->fep_effects.consensus_distance = bridge->col_effects.avg_consensus_distance;
    bridge->fep_effects.consensus_confidence = bridge->state.current_precision / HYPO_COL_FEP_MAX_PRECISION;

    /* Estimate group coherence from recent history */
    if (bridge->col_effects.alignments + bridge->col_effects.deviations > 0) {
        bridge->fep_effects.group_coherence =
            (float)bridge->col_effects.alignments /
            (float)(bridge->col_effects.alignments + bridge->col_effects.deviations);
    } else {
        bridge->fep_effects.group_coherence = 0.5f;
    }

    /* Determine recommended action */
    bridge->fep_effects.recommended_action = determine_action(
        bridge->fep_effects.current_state, bridge->drive_features, &bridge->config);
    bridge->state.last_action = bridge->fep_effects.recommended_action;

    /* Compute action strength */
    float drive_urgency = 0.0f;
    for (int i = 0; i < HYPO_COL_FEP_DRIVE_DIM; i++) {
        if (bridge->drive_features[i] > drive_urgency) {
            drive_urgency = bridge->drive_features[i];
        }
    }
    bridge->fep_effects.action_strength = drive_urgency;

    /* Compute drive influences */
    bridge->fep_effects.social_influence = bridge->drive_features[0] * bridge->config.social_weight;
    bridge->fep_effects.autonomy_influence = bridge->drive_features[1] * bridge->config.autonomy_weight;
    bridge->fep_effects.safety_influence = bridge->drive_features[2] * bridge->config.safety_weight;

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

    /* Track state distribution */
    switch (bridge->state.current_state) {
        case HYPO_COL_STATE_ALIGNED:
            bridge->stats.state_aligned++;
            break;
        case HYPO_COL_STATE_MARGINAL:
            bridge->stats.state_marginal++;
            break;
        case HYPO_COL_STATE_INDEPENDENT:
            bridge->stats.state_independent++;
            break;
        case HYPO_COL_STATE_DEVIANT:
            bridge->stats.state_deviant++;
            break;
        case HYPO_COL_STATE_LEADER:
            bridge->stats.state_leader++;
            break;
    }

    /* Track action distribution */
    switch (bridge->state.last_action) {
        case HYPO_COL_ACTION_ALIGN:
            bridge->stats.action_align++;
            break;
        case HYPO_COL_ACTION_MAINTAIN:
            bridge->stats.action_maintain++;
            break;
        case HYPO_COL_ACTION_ASSERT:
            bridge->stats.action_assert++;
            break;
        case HYPO_COL_ACTION_NEGOTIATE:
            bridge->stats.action_negotiate++;
            break;
        case HYPO_COL_ACTION_LEAD:
            bridge->stats.action_lead++;
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_col_fep_compute_fe(
    hypo_col_fep_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_compute_fe: bridge or free_energy is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);
    *free_energy = fe.total * bridge->config.drive_fe_weight;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_col_fep_modulate_precision(hypo_col_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute collective success rate */
    float success_rate = 0.5f;
    uint64_t total = bridge->col_effects.collective_successes +
                     bridge->col_effects.collective_failures;
    if (total > 0) {
        success_rate = (float)bridge->col_effects.collective_successes / (float)total;
    }

    /* Adjust precision based on success rate */
    float target_precision = HYPO_COL_FEP_DEFAULT_PRECISION;

    if (success_rate > 0.7f) {
        /* High success: increase precision (trust collective) */
        target_precision = HYPO_COL_FEP_DEFAULT_PRECISION * (1.0f + success_rate);
    } else if (success_rate < 0.3f) {
        /* Low success: decrease precision (less trust) */
        target_precision = HYPO_COL_FEP_DEFAULT_PRECISION * (0.5f + success_rate);
    }

    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    if (bridge->state.current_precision < HYPO_COL_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_COL_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_COL_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_COL_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.avg_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_col_fep_get_effects(
    const hypo_col_fep_bridge_t* bridge,
    fep_to_col_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/* ============================================================================
 * Collective Alignment API Implementation
 * ============================================================================ */

int hypo_col_fep_compute_alignment(
    hypo_col_fep_bridge_t* bridge,
    const float* collective_features,
    uint32_t collective_len,
    const float* individual_features,
    uint32_t individual_len,
    float* alignment_score
) {
    if (!bridge || !collective_features || !individual_features || !alignment_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_compute_alignment: required parameter is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Copy features */
    uint32_t col_copy = collective_len < HYPO_COL_FEP_COLLECTIVE_DIM ?
                        collective_len : HYPO_COL_FEP_COLLECTIVE_DIM;
    uint32_t ind_copy = individual_len < HYPO_COL_FEP_INDIVIDUAL_DIM ?
                        individual_len : HYPO_COL_FEP_INDIVIDUAL_DIM;

    memcpy(bridge->collective_features, collective_features, col_copy * sizeof(float));
    memcpy(bridge->individual_features, individual_features, ind_copy * sizeof(float));

    /* Compute distance from consensus */
    float distance = compute_consensus_distance(
        bridge->collective_features, col_copy,
        bridge->individual_features, ind_copy);

    /* Convert distance to alignment score */
    *alignment_score = 1.0f - distance;
    if (*alignment_score < 0.0f) *alignment_score = 0.0f;
    if (*alignment_score > 1.0f) *alignment_score = 1.0f;

    /* Update effect metrics */
    bridge->fep_effects.consensus_distance = distance;

    /* Update running average */
    float alpha = 0.1f;
    bridge->col_effects.avg_consensus_distance =
        (1.0f - alpha) * bridge->col_effects.avg_consensus_distance + alpha * distance;
    bridge->col_effects.avg_alignment =
        (1.0f - alpha) * bridge->col_effects.avg_alignment + alpha * (*alignment_score);

    /* Track event */
    bridge->state.collective_events++;
    if (*alignment_score > 0.7f) {
        bridge->col_effects.alignments++;
    } else if (*alignment_score < 0.3f) {
        bridge->col_effects.deviations++;
    }

    bridge->stats.avg_alignment = bridge->col_effects.avg_alignment;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_col_fep_get_recommended_action(
    hypo_col_fep_bridge_t* bridge,
    hypo_col_action_t* action,
    float* strength
) {
    if (!bridge || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_get_recommended_action: bridge or action is NULL");
        return -1;
    }

    /* Update first to get latest state (update handles its own locking) */
    hypo_col_fep_update(bridge);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *action = bridge->fep_effects.recommended_action;
    if (strength) {
        *strength = bridge->fep_effects.action_strength;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_col_fep_update_from_outcome(
    hypo_col_fep_bridge_t* bridge,
    bool collective_success,
    float individual_contribution
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Track outcome */
    if (collective_success) {
        bridge->col_effects.collective_successes++;
    } else {
        bridge->col_effects.collective_failures++;
    }

    /* Track individual contribution */
    if (individual_contribution > 0.5f && collective_success) {
        bridge->col_effects.individual_successes++;
    } else if (individual_contribution > 0.5f && !collective_success) {
        bridge->col_effects.individual_failures++;
    }

    /* Compute utility */
    float utility = collective_success ? 1.0f : 0.0f;
    utility *= individual_contribution;

    float alpha = 0.1f;
    bridge->col_effects.collective_utility =
        (1.0f - alpha) * bridge->col_effects.collective_utility + alpha * utility;

    /* Suggest drive updates based on outcome */
    if (collective_success) {
        /* Collective success: reinforce social drive */
        bridge->col_effects.social_drive_update = 0.05f * individual_contribution;
    } else {
        /* Collective failure: may want more independence or safety */
        bridge->col_effects.autonomy_drive_update = 0.05f;
        bridge->col_effects.safety_drive_update = 0.1f;
    }

    /* Update precision (inline to avoid deadlock) */
    {
        float sr = 0.5f;
        uint64_t sr_total = bridge->col_effects.collective_successes +
                            bridge->col_effects.collective_failures;
        if (sr_total > 0) {
            sr = (float)bridge->col_effects.collective_successes / (float)sr_total;
        }
        float target_prec = HYPO_COL_FEP_DEFAULT_PRECISION;
        if (sr > 0.7f) {
            target_prec = HYPO_COL_FEP_DEFAULT_PRECISION * (1.0f + sr);
        } else if (sr < 0.3f) {
            target_prec = HYPO_COL_FEP_DEFAULT_PRECISION * (0.5f + sr);
        }
        float prec_alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - prec_alpha) * bridge->state.current_precision + prec_alpha * target_prec;
        if (bridge->state.current_precision < HYPO_COL_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_COL_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_COL_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_COL_FEP_MAX_PRECISION;
        }
        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.avg_precision = bridge->state.current_precision;
    }

    /* Update success rates */
    uint64_t col_total = bridge->col_effects.collective_successes +
                         bridge->col_effects.collective_failures;
    if (col_total > 0) {
        bridge->stats.collective_success_rate =
            (float)bridge->col_effects.collective_successes / (float)col_total;
    }

    uint64_t ind_total = bridge->col_effects.individual_successes +
                         bridge->col_effects.individual_failures;
    if (ind_total > 0) {
        bridge->stats.individual_success_rate =
            (float)bridge->col_effects.individual_successes / (float)ind_total;
    }

    /* Process through FEP if learning enabled */
    if (bridge->config.enable_online_learning) {
        float outcome_features[4] = {
            collective_success ? 1.0f : 0.0f,
            individual_contribution,
            bridge->col_effects.avg_alignment,
            bridge->col_effects.collective_utility
        };
        fep_process_observation(bridge->fep_system, outcome_features, 4);
        fep_update_beliefs(bridge->fep_system);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int hypo_col_fep_get_state(
    const hypo_col_fep_bridge_t* bridge,
    hypo_col_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_get_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int hypo_col_fep_get_stats(
    const hypo_col_fep_bridge_t* bridge,
    hypo_col_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_col_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_col_fep_connect_bio_async(hypo_col_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_COL_FEP,
        .module_name = "hypo_col_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo COL FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_col_fep_disconnect_bio_async(hypo_col_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo COL FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_col_fep_process_messages(
    hypo_col_fep_bridge_t* bridge,
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

void hypo_col_fep_print_summary(const hypo_col_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo COL FEP Bridge: NULL\n");
        return;
    }

    printf("\n========== Hypo COL FEP Bridge Summary ==========\n");
    printf("State:\n");
    printf("  Active:            %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update Count:      %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Collective Events: %lu\n", (unsigned long)bridge->state.collective_events);
    printf("  Alignment State:   %s\n", hypo_col_state_to_string(bridge->state.current_state));
    printf("  Last Action:       %s\n", hypo_col_action_to_string(bridge->state.last_action));
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy:   %.3f\n", bridge->state.avg_free_energy);

    printf("\nFEP Effects:\n");
    printf("  Alignment Score:   %.3f\n", bridge->fep_effects.alignment_score);
    printf("  Independence Score: %.3f\n", bridge->fep_effects.independence_score);
    printf("  Consensus Distance: %.3f\n", bridge->fep_effects.consensus_distance);
    printf("  Consensus Confidence: %.3f\n", bridge->fep_effects.consensus_confidence);
    printf("  Group Coherence:   %.3f\n", bridge->fep_effects.group_coherence);
    printf("  Action Strength:   %.3f\n", bridge->fep_effects.action_strength);
    printf("  Free Energy:       %.3f\n", bridge->fep_effects.free_energy);

    printf("\nStatistics:\n");
    printf("  Collective Success Rate: %.3f\n", bridge->stats.collective_success_rate);
    printf("  Individual Success Rate: %.3f\n", bridge->stats.individual_success_rate);
    printf("  Avg Alignment:     %.3f\n", bridge->stats.avg_alignment);
    printf("  State Distribution:\n");
    printf("    Aligned:         %lu\n", (unsigned long)bridge->stats.state_aligned);
    printf("    Marginal:        %lu\n", (unsigned long)bridge->stats.state_marginal);
    printf("    Independent:     %lu\n", (unsigned long)bridge->stats.state_independent);
    printf("    Deviant:         %lu\n", (unsigned long)bridge->stats.state_deviant);
    printf("    Leader:          %lu\n", (unsigned long)bridge->stats.state_leader);

    printf("\nBio-Async: %s\n", bridge->base.bio_async_enabled ? "connected" : "disconnected");
    printf("==============================================\n\n");
}

const char* hypo_col_state_to_string(hypo_col_state_t state) {
    switch (state) {
        case HYPO_COL_STATE_ALIGNED:
            return "aligned";
        case HYPO_COL_STATE_MARGINAL:
            return "marginal";
        case HYPO_COL_STATE_INDEPENDENT:
            return "independent";
        case HYPO_COL_STATE_DEVIANT:
            return "deviant";
        case HYPO_COL_STATE_LEADER:
            return "leader";
        default:
            return "unknown";
    }
}

const char* hypo_col_action_to_string(hypo_col_action_t action) {
    switch (action) {
        case HYPO_COL_ACTION_ALIGN:
            return "align";
        case HYPO_COL_ACTION_MAINTAIN:
            return "maintain";
        case HYPO_COL_ACTION_ASSERT:
            return "assert";
        case HYPO_COL_ACTION_NEGOTIATE:
            return "negotiate";
        case HYPO_COL_ACTION_LEAD:
            return "lead";
        default:
            return "unknown";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static void extract_drive_features(
    hypo_col_fep_bridge_t* bridge,
    float* features
) {
    if (!bridge || !features) {
        return;
    }

    /* Initialize with defaults if no drive system */
    if (!bridge->drive_system) {
        features[0] = 0.5f;  /* SOCIAL */
        features[1] = 0.5f;  /* AUTONOMY */
        features[2] = 0.5f;  /* SAFETY */
        features[3] = 0.5f;  /* Reserved */
        return;
    }

    /* Extract drive states */
    hypo_drive_state_t drive_state;

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SOCIAL, &drive_state)) {
        features[0] = drive_state.level * drive_state.urgency;
    } else {
        features[0] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_AUTONOMY, &drive_state)) {
        features[1] = drive_state.level * drive_state.urgency;
    } else {
        features[1] = 0.5f;
    }

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_SAFETY, &drive_state)) {
        features[2] = drive_state.level * drive_state.urgency;
    } else {
        features[2] = 0.5f;
    }

    features[3] = 0.5f;  /* Reserved */
}

static hypo_col_state_t classify_alignment_state(
    float free_energy,
    float alignment_score,
    const hypo_col_fep_config_t* config
) {
    if (!config) {
        return HYPO_COL_STATE_MARGINAL;
    }

    /* Check for leadership (high alignment + high autonomy) */
    if (alignment_score > 0.8f && free_energy > config->alignment_threshold) {
        return HYPO_COL_STATE_LEADER;
    }

    if (free_energy < config->alignment_threshold) {
        return HYPO_COL_STATE_ALIGNED;
    }
    if (free_energy < HYPO_COL_FEP_MARGINAL_THRESHOLD) {
        return HYPO_COL_STATE_MARGINAL;
    }
    if (free_energy < config->deviance_threshold) {
        return HYPO_COL_STATE_INDEPENDENT;
    }

    return HYPO_COL_STATE_DEVIANT;
}

static hypo_col_action_t determine_action(
    hypo_col_state_t state,
    const float* drive_features,
    const hypo_col_fep_config_t* config
) {
    if (!drive_features || !config) {
        return HYPO_COL_ACTION_MAINTAIN;
    }

    float social = drive_features[0] * config->social_weight;
    float autonomy = drive_features[1] * config->autonomy_weight;
    float safety = drive_features[2] * config->safety_weight;

    /* Check for leadership potential */
    if (autonomy > config->leadership_threshold && social > 0.5f) {
        return HYPO_COL_ACTION_LEAD;
    }

    switch (state) {
        case HYPO_COL_STATE_ALIGNED:
            return HYPO_COL_ACTION_MAINTAIN;

        case HYPO_COL_STATE_MARGINAL:
            if (social > autonomy) {
                return HYPO_COL_ACTION_ALIGN;
            }
            return HYPO_COL_ACTION_NEGOTIATE;

        case HYPO_COL_STATE_INDEPENDENT:
            if (autonomy > social) {
                return HYPO_COL_ACTION_ASSERT;
            }
            if (safety > 0.5f) {
                return HYPO_COL_ACTION_ALIGN;  /* Safety: follow collective */
            }
            return HYPO_COL_ACTION_NEGOTIATE;

        case HYPO_COL_STATE_DEVIANT:
            if (safety > 0.5f) {
                return HYPO_COL_ACTION_ALIGN;  /* High risk: align */
            }
            if (autonomy > 0.7f) {
                return HYPO_COL_ACTION_ASSERT;  /* Strong autonomy: hold position */
            }
            return HYPO_COL_ACTION_NEGOTIATE;

        case HYPO_COL_STATE_LEADER:
            return HYPO_COL_ACTION_LEAD;

        default:
            return HYPO_COL_ACTION_MAINTAIN;
    }
}

static float compute_consensus_distance(
    const float* collective_features,
    uint32_t collective_len,
    const float* individual_features,
    uint32_t individual_len
) {
    if (!collective_features || !individual_features) {
        return 0.5f;
    }

    /* Compute L2 distance between collective and individual */
    uint32_t min_len = collective_len < individual_len ? collective_len : individual_len;
    if (min_len == 0) {
        return 0.5f;
    }

    float distance = 0.0f;
    for (uint32_t i = 0; i < min_len; i++) {
        float diff = collective_features[i] - individual_features[i];
        distance += diff * diff;
    }

    distance = sqrtf(distance / min_len);

    /* Normalize to [0, 1] */
    if (distance > 1.0f) distance = 1.0f;

    return distance;
}

static void update_running_averages(
    hypo_col_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * free_energy;
    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * prediction_error;
}
