/**
 * @file nimcp_hypo_epistemic_fep_bridge.c
 * @brief Implementation of Hypothalamus Epistemic - FEP bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for drive-modulated epistemic processing
 * WHY:  Enable drives to shape confidence bounds; uncertainty as free energy
 * HOW:  Map drive state to epistemic parameters, use FEP for uncertainty
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_epistemic_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_epistemic_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void extract_drive_features(
    hypo_epi_fep_bridge_t* bridge,
    float* features
);

static hypo_epi_state_t classify_epistemic_state(
    float free_energy,
    const hypo_epi_fep_config_t* config
);

static hypo_epi_action_t determine_action(
    hypo_epi_state_t state,
    const float* drive_features,
    const hypo_epi_fep_config_t* config
);

static void update_running_averages(
    hypo_epi_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_epi_fep_default_config(hypo_epi_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Drive-FEP coupling weights */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.0f;
    config->precision_modulation = 1.0f;

    /* Epistemic parameters */
    config->certainty_threshold = HYPO_EPI_FEP_CERTAIN_THRESHOLD;
    config->ignorance_threshold = HYPO_EPI_FEP_IGNORANT_THRESHOLD;
    config->info_value_weight = 1.0f;

    /* Drive influence weights */
    config->curiosity_weight = 1.0f;
    config->safety_weight = 0.8f;
    config->competence_weight = 0.7f;

    /* Confidence calibration */
    config->overconfidence_penalty = 0.1f;
    config->underconfidence_penalty = 0.05f;
    config->calibration_target = 0.8f;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = HYPO_EPI_FEP_DEFAULT_LEARNING_RATE;
    config->precision_learning_rate = HYPO_EPI_FEP_PRECISION_LEARNING_RATE;

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

hypo_epi_fep_bridge_t* hypo_epi_fep_create(
    const hypo_epi_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo EPI FEP bridge: FEP system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    hypo_epi_fep_bridge_t* bridge = (hypo_epi_fep_bridge_t*)nimcp_malloc(sizeof(hypo_epi_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo EPI FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_epi_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(hypo_epi_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_epi_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->fep_system = fep_system;
    bridge->drive_system = drive_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "hypo_epistemic_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo EPI FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_epi_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate feature buffers */
    bridge->drive_features = (float*)nimcp_malloc(HYPO_EPI_FEP_DRIVE_DIM * sizeof(float));
    bridge->belief_features = (float*)nimcp_malloc(HYPO_EPI_FEP_BELIEF_DIM * sizeof(float));
    bridge->evidence_features = (float*)nimcp_malloc(HYPO_EPI_FEP_EVIDENCE_DIM * sizeof(float));

    if (!bridge->drive_features || !bridge->belief_features || !bridge->evidence_features) {
        NIMCP_LOGGING_ERROR("Hypo EPI FEP bridge: feature buffer allocation failed");
        hypo_epi_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_epi_fep_create: required parameter is NULL (bridge->drive_features, bridge->belief_features, bridge->evidence_features)");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_EPI_FEP_DEFAULT_PRECISION;
    bridge->state.current_state = HYPO_EPI_STATE_PROBABLE;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Hypo EPI FEP bridge created successfully");
    return bridge;
}

void hypo_epi_fep_destroy(hypo_epi_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        hypo_epi_fep_disconnect_bio_async(bridge);
    }

    /* Free feature buffers */
    if (bridge->drive_features) {
        nimcp_free(bridge->drive_features);
    }
    if (bridge->belief_features) {
        nimcp_free(bridge->belief_features);
    }
    if (bridge->evidence_features) {
        nimcp_free(bridge->evidence_features);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo EPI FEP bridge destroyed");
}

int hypo_epi_fep_reset(hypo_epi_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(hypo_epi_fep_state_t));
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_EPI_FEP_DEFAULT_PRECISION;
    bridge->state.current_state = HYPO_EPI_STATE_PROBABLE;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_epi_fep_stats_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_epi_effects_t));
    memset(&bridge->epi_effects, 0, sizeof(epi_to_fep_effects_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo EPI FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int hypo_epi_fep_update(hypo_epi_fep_bridge_t* bridge) {
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
    fep_process_observation(bridge->fep_system, bridge->drive_features, HYPO_EPI_FEP_DRIVE_DIM);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Store FEP metrics */
    bridge->fep_effects.free_energy = fe.total;
    bridge->fep_effects.prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify epistemic state */
    bridge->fep_effects.current_state = classify_epistemic_state(fe.total, &bridge->config);
    bridge->state.current_state = bridge->fep_effects.current_state;

    /* Compute uncertainty level (normalized FE) */
    float normalized_fe = fe.total / bridge->config.ignorance_threshold;
    if (normalized_fe > 1.0f) normalized_fe = 1.0f;
    bridge->fep_effects.uncertainty_level = normalized_fe;
    bridge->fep_effects.confidence_level = 1.0f - normalized_fe;

    /* Compute confidence bounds */
    float base_width = normalized_fe * 0.5f;  /* Max 50% interval at max uncertainty */

    /* Safety drive widens bounds (more cautious) */
    float safety_factor = 1.0f + bridge->drive_features[1] * bridge->config.safety_weight * 0.5f;
    base_width *= safety_factor;

    bridge->fep_effects.bound_width = base_width;
    bridge->fep_effects.lower_bound = 0.5f - base_width / 2.0f;
    bridge->fep_effects.upper_bound = 0.5f + base_width / 2.0f;

    /* Clamp bounds */
    if (bridge->fep_effects.lower_bound < 0.0f) bridge->fep_effects.lower_bound = 0.0f;
    if (bridge->fep_effects.upper_bound > 1.0f) bridge->fep_effects.upper_bound = 1.0f;

    /* Compute information value (higher when uncertain and curious) */
    float curiosity = bridge->drive_features[0] * bridge->config.curiosity_weight;
    bridge->fep_effects.info_value = normalized_fe * curiosity * bridge->config.info_value_weight;
    bridge->fep_effects.info_urgency = bridge->fep_effects.info_value *
                                        (1.0f + bridge->drive_features[1]);  /* Safety adds urgency */

    /* Determine recommended action */
    bridge->fep_effects.recommended_action = determine_action(
        bridge->fep_effects.current_state, bridge->drive_features, &bridge->config);
    bridge->state.last_action = bridge->fep_effects.recommended_action;

    /* Compute drive influences */
    bridge->fep_effects.curiosity_influence = bridge->drive_features[0] * bridge->config.curiosity_weight;
    bridge->fep_effects.safety_influence = bridge->drive_features[1] * bridge->config.safety_weight;
    bridge->fep_effects.competence_influence = bridge->drive_features[2] * bridge->config.competence_weight;

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
        case HYPO_EPI_STATE_CERTAIN:
            bridge->stats.state_certain++;
            break;
        case HYPO_EPI_STATE_CONFIDENT:
            bridge->stats.state_confident++;
            break;
        case HYPO_EPI_STATE_PROBABLE:
            bridge->stats.state_probable++;
            break;
        case HYPO_EPI_STATE_UNCERTAIN:
            bridge->stats.state_uncertain++;
            break;
        case HYPO_EPI_STATE_IGNORANT:
            bridge->stats.state_ignorant++;
            break;
    }

    /* Track action distribution */
    switch (bridge->state.last_action) {
        case HYPO_EPI_ACTION_ACT:
            bridge->stats.action_act++;
            break;
        case HYPO_EPI_ACTION_SEEK_INFO:
            bridge->stats.action_seek_info++;
            break;
        case HYPO_EPI_ACTION_WAIT:
            bridge->stats.action_wait++;
            break;
        case HYPO_EPI_ACTION_HEDGE:
            bridge->stats.action_hedge++;
            break;
        case HYPO_EPI_ACTION_EXPLORE:
            bridge->stats.action_explore++;
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_compute_fe(
    hypo_epi_fep_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_compute_fe: bridge or free_energy is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);
    *free_energy = fe.total * bridge->config.drive_fe_weight;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_modulate_precision(hypo_epi_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute calibration accuracy */
    float calibration = bridge->epi_effects.calibration_score;

    /* Adjust precision based on calibration */
    float target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION;

    if (calibration > bridge->config.calibration_target) {
        /* Well-calibrated: increase precision */
        target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION * (1.0f + calibration);
    } else {
        /* Poorly calibrated: decrease precision */
        target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION * calibration;
    }

    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    if (bridge->state.current_precision < HYPO_EPI_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_EPI_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_EPI_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_EPI_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.avg_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_get_effects(
    const hypo_epi_fep_bridge_t* bridge,
    fep_to_epi_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/* ============================================================================
 * Epistemic API Implementation
 * ============================================================================ */

int hypo_epi_fep_compute_bounds(
    hypo_epi_fep_bridge_t* bridge,
    const float* belief_features,
    uint32_t belief_len,
    float* lower_bound,
    float* upper_bound
) {
    if (!bridge || !belief_features || !lower_bound || !upper_bound) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_compute_bounds: required parameter is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Copy belief features */
    uint32_t copy_len = belief_len < HYPO_EPI_FEP_BELIEF_DIM ?
                        belief_len : HYPO_EPI_FEP_BELIEF_DIM;
    memcpy(bridge->belief_features, belief_features, copy_len * sizeof(float));

    /* Process through FEP */
    fep_process_observation(bridge->fep_system, bridge->belief_features, copy_len);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Compute bounds based on FE and drives */
    float normalized_fe = fe.total / bridge->config.ignorance_threshold;
    if (normalized_fe > 1.0f) normalized_fe = 1.0f;

    float base_width = normalized_fe * 0.5f;

    /* Modulate by drive state */
    extract_drive_features(bridge, bridge->drive_features);
    float safety_factor = 1.0f + bridge->drive_features[1] * bridge->config.safety_weight * 0.5f;
    base_width *= safety_factor;

    /* Compute mean estimate (could be derived from belief features) */
    float mean = 0.5f;  /* Default to 0.5 */
    for (uint32_t i = 0; i < copy_len; i++) {
        mean += belief_features[i];
    }
    mean /= (copy_len + 1.0f);
    if (mean > 1.0f) mean = 1.0f;
    if (mean < 0.0f) mean = 0.0f;

    *lower_bound = mean - base_width / 2.0f;
    *upper_bound = mean + base_width / 2.0f;

    /* Clamp */
    if (*lower_bound < 0.0f) *lower_bound = 0.0f;
    if (*upper_bound > 1.0f) *upper_bound = 1.0f;

    /* Update statistics */
    bridge->state.belief_count++;
    float alpha = 0.1f;
    bridge->stats.avg_bound_width =
        (1.0f - alpha) * bridge->stats.avg_bound_width + alpha * (*upper_bound - *lower_bound);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_compute_info_value(
    hypo_epi_fep_bridge_t* bridge,
    float* info_value
) {
    if (!bridge || !info_value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_compute_info_value: bridge or info_value is NULL");
        return -1;
    }

    /* Update first to get latest state (update handles its own locking) */
    hypo_epi_fep_update(bridge);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *info_value = bridge->fep_effects.info_value;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_update_from_evidence(
    hypo_epi_fep_bridge_t* bridge,
    const float* evidence_features,
    uint32_t evidence_len,
    float evidence_weight
) {
    if (!bridge || !evidence_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_update_from_evidence: bridge or evidence_features is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Copy evidence features */
    uint32_t copy_len = evidence_len < HYPO_EPI_FEP_EVIDENCE_DIM ?
                        evidence_len : HYPO_EPI_FEP_EVIDENCE_DIM;
    memcpy(bridge->evidence_features, evidence_features, copy_len * sizeof(float));

    /* Weight the evidence */
    for (uint32_t i = 0; i < copy_len; i++) {
        bridge->evidence_features[i] *= evidence_weight;
    }

    /* Process through FEP */
    if (bridge->config.enable_online_learning) {
        fep_process_observation(bridge->fep_system, bridge->evidence_features, copy_len);
        fep_update_beliefs(bridge->fep_system);
    }

    /* Update counters */
    bridge->epi_effects.evidence_received++;
    bridge->epi_effects.beliefs_updated++;

    /* Update average metrics */
    float alpha = 0.1f;
    bridge->epi_effects.avg_uncertainty =
        (1.0f - alpha) * bridge->epi_effects.avg_uncertainty +
        alpha * bridge->fep_effects.uncertainty_level;

    /* Modulate precision based on evidence quality (inline to avoid deadlock) */
    {
        float calibration = bridge->epi_effects.calibration_score;
        float target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION;
        if (calibration > bridge->config.calibration_target) {
            target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION * (1.0f + calibration);
        } else {
            target_precision = HYPO_EPI_FEP_DEFAULT_PRECISION * calibration;
        }
        float prec_alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - prec_alpha) * bridge->state.current_precision + prec_alpha * target_precision;
        if (bridge->state.current_precision < HYPO_EPI_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_EPI_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_EPI_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_EPI_FEP_MAX_PRECISION;
        }
        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.avg_precision = bridge->state.current_precision;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_epi_fep_check_calibration(
    hypo_epi_fep_bridge_t* bridge,
    float predicted_confidence,
    bool outcome_correct
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->epi_effects.calibration_checks++;

    if (outcome_correct) {
        bridge->epi_effects.correct_predictions++;
    } else {
        bridge->epi_effects.incorrect_predictions++;
    }

    /* Compute calibration score */
    /* Perfect calibration: if you predict 70% confidence, you're right 70% of time */
    float expected = predicted_confidence;
    float actual = outcome_correct ? 1.0f : 0.0f;
    float calibration_error = fabsf(expected - actual);

    /* Running average calibration */
    float alpha = 0.1f;
    bridge->epi_effects.calibration_score =
        (1.0f - alpha) * bridge->epi_effects.calibration_score +
        alpha * (1.0f - calibration_error);

    bridge->stats.calibration_score = bridge->epi_effects.calibration_score;

    /* Suggest drive updates */
    if (outcome_correct && predicted_confidence > 0.7f) {
        /* Good confident prediction: reinforce competence */
        bridge->epi_effects.competence_drive_update = 0.05f;
    } else if (!outcome_correct && predicted_confidence > 0.7f) {
        /* Overconfident wrong prediction: reduce competence, increase curiosity */
        bridge->epi_effects.competence_drive_update = -0.1f;
        bridge->epi_effects.curiosity_drive_update = 0.1f;
    }

    /* Update prediction accuracy stat */
    if (bridge->epi_effects.calibration_checks > 0) {
        bridge->stats.prediction_accuracy =
            (float)bridge->epi_effects.correct_predictions /
            (float)bridge->epi_effects.calibration_checks;
    }

    /* Modulate precision based on calibration (inline to avoid deadlock) */
    {
        float cal = bridge->epi_effects.calibration_score;
        float target_prec = HYPO_EPI_FEP_DEFAULT_PRECISION;
        if (cal > bridge->config.calibration_target) {
            target_prec = HYPO_EPI_FEP_DEFAULT_PRECISION * (1.0f + cal);
        } else {
            target_prec = HYPO_EPI_FEP_DEFAULT_PRECISION * cal;
        }
        float prec_alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - prec_alpha) * bridge->state.current_precision + prec_alpha * target_prec;
        if (bridge->state.current_precision < HYPO_EPI_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_EPI_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_EPI_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_EPI_FEP_MAX_PRECISION;
        }
        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.avg_precision = bridge->state.current_precision;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int hypo_epi_fep_get_state(
    const hypo_epi_fep_bridge_t* bridge,
    hypo_epi_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_get_state: bridge or state is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int hypo_epi_fep_get_stats(
    const hypo_epi_fep_bridge_t* bridge,
    hypo_epi_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_epi_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_epi_fep_connect_bio_async(hypo_epi_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_EPI_FEP,
        .module_name = "hypo_epi_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo EPI FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_epi_fep_disconnect_bio_async(hypo_epi_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo EPI FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_epi_fep_process_messages(
    hypo_epi_fep_bridge_t* bridge,
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

void hypo_epi_fep_print_summary(const hypo_epi_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo EPI FEP Bridge: NULL\n");
        return;
    }

    printf("\n========== Hypo EPI FEP Bridge Summary ==========\n");
    printf("State:\n");
    printf("  Active:            %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update Count:      %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Belief Count:      %lu\n", (unsigned long)bridge->state.belief_count);
    printf("  Epistemic State:   %s\n", hypo_epi_state_to_string(bridge->state.current_state));
    printf("  Last Action:       %s\n", hypo_epi_action_to_string(bridge->state.last_action));
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy:   %.3f\n", bridge->state.avg_free_energy);

    printf("\nFEP Effects:\n");
    printf("  Uncertainty Level: %.3f\n", bridge->fep_effects.uncertainty_level);
    printf("  Confidence Level:  %.3f\n", bridge->fep_effects.confidence_level);
    printf("  Bounds:            [%.3f, %.3f] (width: %.3f)\n",
           bridge->fep_effects.lower_bound,
           bridge->fep_effects.upper_bound,
           bridge->fep_effects.bound_width);
    printf("  Info Value:        %.3f\n", bridge->fep_effects.info_value);
    printf("  Info Urgency:      %.3f\n", bridge->fep_effects.info_urgency);
    printf("  Free Energy:       %.3f\n", bridge->fep_effects.free_energy);

    printf("\nStatistics:\n");
    printf("  Calibration Score: %.3f\n", bridge->stats.calibration_score);
    printf("  Prediction Accuracy: %.3f\n", bridge->stats.prediction_accuracy);
    printf("  Avg Bound Width:   %.3f\n", bridge->stats.avg_bound_width);
    printf("  State Distribution:\n");
    printf("    Certain:         %lu\n", (unsigned long)bridge->stats.state_certain);
    printf("    Confident:       %lu\n", (unsigned long)bridge->stats.state_confident);
    printf("    Probable:        %lu\n", (unsigned long)bridge->stats.state_probable);
    printf("    Uncertain:       %lu\n", (unsigned long)bridge->stats.state_uncertain);
    printf("    Ignorant:        %lu\n", (unsigned long)bridge->stats.state_ignorant);

    printf("\nBio-Async: %s\n", bridge->base.bio_async_enabled ? "connected" : "disconnected");
    printf("==============================================\n\n");
}

const char* hypo_epi_state_to_string(hypo_epi_state_t state) {
    switch (state) {
        case HYPO_EPI_STATE_CERTAIN:
            return "certain";
        case HYPO_EPI_STATE_CONFIDENT:
            return "confident";
        case HYPO_EPI_STATE_PROBABLE:
            return "probable";
        case HYPO_EPI_STATE_UNCERTAIN:
            return "uncertain";
        case HYPO_EPI_STATE_IGNORANT:
            return "ignorant";
        default:
            return "unknown";
    }
}

const char* hypo_epi_action_to_string(hypo_epi_action_t action) {
    switch (action) {
        case HYPO_EPI_ACTION_ACT:
            return "act";
        case HYPO_EPI_ACTION_SEEK_INFO:
            return "seek_info";
        case HYPO_EPI_ACTION_WAIT:
            return "wait";
        case HYPO_EPI_ACTION_HEDGE:
            return "hedge";
        case HYPO_EPI_ACTION_EXPLORE:
            return "explore";
        default:
            return "unknown";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static void extract_drive_features(
    hypo_epi_fep_bridge_t* bridge,
    float* features
) {
    if (!bridge || !features) {
        return;
    }

    /* Initialize with defaults if no drive system */
    if (!bridge->drive_system) {
        features[0] = 0.5f;  /* CURIOSITY */
        features[1] = 0.5f;  /* SAFETY */
        features[2] = 0.5f;  /* COMPETENCE */
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

    if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_COMPETENCE, &drive_state)) {
        features[2] = drive_state.level * drive_state.urgency;
    } else {
        features[2] = 0.5f;
    }

    features[3] = 0.5f;  /* Reserved */
}

static hypo_epi_state_t classify_epistemic_state(
    float free_energy,
    const hypo_epi_fep_config_t* config
) {
    if (!config) {
        return HYPO_EPI_STATE_PROBABLE;
    }

    if (free_energy < config->certainty_threshold) {
        return HYPO_EPI_STATE_CERTAIN;
    }
    if (free_energy < HYPO_EPI_FEP_CONFIDENT_THRESHOLD) {
        return HYPO_EPI_STATE_CONFIDENT;
    }
    if (free_energy < HYPO_EPI_FEP_UNCERTAIN_THRESHOLD) {
        return HYPO_EPI_STATE_PROBABLE;
    }
    if (free_energy < config->ignorance_threshold) {
        return HYPO_EPI_STATE_UNCERTAIN;
    }

    return HYPO_EPI_STATE_IGNORANT;
}

static hypo_epi_action_t determine_action(
    hypo_epi_state_t state,
    const float* drive_features,
    const hypo_epi_fep_config_t* config
) {
    if (!drive_features || !config) {
        return HYPO_EPI_ACTION_WAIT;
    }

    float curiosity = drive_features[0] * config->curiosity_weight;
    float safety = drive_features[1] * config->safety_weight;

    /* High curiosity always encourages exploration */
    if (curiosity > 0.7f && state != HYPO_EPI_STATE_CERTAIN) {
        return HYPO_EPI_ACTION_EXPLORE;
    }

    switch (state) {
        case HYPO_EPI_STATE_CERTAIN:
            return HYPO_EPI_ACTION_ACT;

        case HYPO_EPI_STATE_CONFIDENT:
            if (safety > 0.5f) {
                return HYPO_EPI_ACTION_SEEK_INFO;  /* Cautious: verify first */
            }
            return HYPO_EPI_ACTION_ACT;

        case HYPO_EPI_STATE_PROBABLE:
            if (curiosity > 0.5f) {
                return HYPO_EPI_ACTION_SEEK_INFO;
            }
            if (safety > 0.5f) {
                return HYPO_EPI_ACTION_HEDGE;
            }
            return HYPO_EPI_ACTION_ACT;

        case HYPO_EPI_STATE_UNCERTAIN:
            if (safety > 0.5f) {
                return HYPO_EPI_ACTION_WAIT;
            }
            return HYPO_EPI_ACTION_SEEK_INFO;

        case HYPO_EPI_STATE_IGNORANT:
            return HYPO_EPI_ACTION_WAIT;

        default:
            return HYPO_EPI_ACTION_WAIT;
    }
}

static void update_running_averages(
    hypo_epi_fep_bridge_t* bridge,
    float free_energy,
    float prediction_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * free_energy;
    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * prediction_error;
}
