//=============================================================================
// nimcp_basal_ganglia_enhanced.c - Enhanced Basal Ganglia Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_enhanced, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURE
 * ============================================================================ */

struct bg_enhanced {
    /* Core basal ganglia */
    basal_ganglia_t* core;

    /* Enhancement modules */
    bg_beta_system_t* beta;
    bg_neuromod_system_t* neuromod;
    bg_hrl_system_t* hrl;
    bg_model_based_t* model_based;
    nucleus_accumbens_t* nac;
    superior_colliculus_t* sc;
    striatal_interneurons_t* interneurons;
    bg_cerebellar_coord_t* cerebellar;
    bg_outcome_deval_t* outcome_deval;
    bg_temporal_credit_t* temporal_credit;

    /* Training integration */
    bgtr_bridge_t* training_bridge;

    /* Configuration */
    bg_enhanced_config_t config;

    /* State */
    uint32_t current_option;
    bool in_option;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

void bg_enhanced_default_config(bg_enhanced_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(bg_enhanced_config_t));

    /* Core config */
    basal_ganglia_default_config(&config->core_config);

    /* Enable all features by default */
    config->features.enable_beta_oscillations = true;
    config->features.enable_multi_neuromod = true;
    config->features.enable_hierarchical_rl = true;
    config->features.enable_model_based = true;
    config->features.enable_nucleus_accumbens = true;
    config->features.enable_superior_colliculus = true;
    config->features.enable_interneurons = true;
    config->features.enable_cerebellar_coord = true;
    config->features.enable_outcome_deval = true;
    config->features.enable_temporal_credit = true;
    config->features.enable_training_plasticity = true;

    /* Module configs */
    bg_beta_default_config(&config->beta_config);
    bg_neuromod_default_config(&config->neuromod_config);
    bg_hrl_default_config(&config->hrl_config);
    bg_mb_default_config(&config->model_based_config);
    nac_default_config(&config->nac_config);
    sc_default_config(&config->sc_config);
    sint_default_config(&config->interneuron_config);
    bgcb_default_config(&config->cerebellar_config);
    bgod_default_config(&config->outcome_deval_config);
    bgtc_default_config(&config->temporal_credit_config);
    bgtr_bridge_default_config(&config->training_config);
}

bg_enhanced_t* bg_enhanced_create(const bg_enhanced_config_t* config) {
    bg_enhanced_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        bg_enhanced_default_config(&cfg);
    }

    bg_enhanced_t* bge = nimcp_calloc(1, sizeof(bg_enhanced_t));
    if (!bge) {
        NIMCP_LOGGING_ERROR("Failed to allocate enhanced BG");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bge is NULL");

        return NULL;
    }

    bge->config = cfg;

    /* Create core BG */
    bge->core = basal_ganglia_create(&cfg.core_config);
    if (!bge->core) {
        NIMCP_LOGGING_ERROR("Failed to create core BG");
        nimcp_free(bge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_enhanced_create: bge->core is NULL");
        return NULL;
    }

    /* Create enhancement modules based on feature flags */
    if (cfg.features.enable_beta_oscillations) {
        bge->beta = bg_beta_create(&cfg.beta_config);
        if (!bge->beta) NIMCP_LOGGING_WARN("Failed to create beta system");
    }

    if (cfg.features.enable_multi_neuromod) {
        bge->neuromod = bg_neuromod_create(&cfg.neuromod_config);
        if (!bge->neuromod) NIMCP_LOGGING_WARN("Failed to create neuromod system");
    }

    if (cfg.features.enable_hierarchical_rl) {
        bge->hrl = bg_hrl_create(&cfg.hrl_config);
        if (!bge->hrl) NIMCP_LOGGING_WARN("Failed to create HRL system");
    }

    if (cfg.features.enable_model_based) {
        bge->model_based = bg_mb_create(&cfg.model_based_config);
        if (!bge->model_based) NIMCP_LOGGING_WARN("Failed to create model-based system");
    }

    if (cfg.features.enable_nucleus_accumbens) {
        bge->nac = nac_create(&cfg.nac_config);
        if (!bge->nac) NIMCP_LOGGING_WARN("Failed to create NAc");
    }

    if (cfg.features.enable_superior_colliculus) {
        bge->sc = sc_create(&cfg.sc_config);
        if (!bge->sc) NIMCP_LOGGING_WARN("Failed to create SC");
    }

    if (cfg.features.enable_interneurons) {
        bge->interneurons = sint_create(&cfg.interneuron_config);
        if (!bge->interneurons) NIMCP_LOGGING_WARN("Failed to create interneurons");
    }

    if (cfg.features.enable_cerebellar_coord) {
        bge->cerebellar = bgcb_create(&cfg.cerebellar_config);
        if (!bge->cerebellar) NIMCP_LOGGING_WARN("Failed to create cerebellar coord");
    }

    if (cfg.features.enable_outcome_deval) {
        bge->outcome_deval = bgod_create(&cfg.outcome_deval_config);
        if (!bge->outcome_deval) NIMCP_LOGGING_WARN("Failed to create outcome deval");
    }

    if (cfg.features.enable_temporal_credit) {
        bge->temporal_credit = bgtc_create(&cfg.temporal_credit_config);
        if (!bge->temporal_credit) NIMCP_LOGGING_WARN("Failed to create temporal credit");
    }

    if (cfg.features.enable_training_plasticity) {
        uint32_t num_actions = cfg.core_config.num_actions > 0 ? cfg.core_config.num_actions : 16;
        bge->training_bridge = bgtr_bridge_create(&cfg.training_config, num_actions);
        if (bge->training_bridge) {
            /* Connect training bridge to core BG */
            bgtr_bridge_connect_bg(bge->training_bridge, bge->core);
        } else {
            NIMCP_LOGGING_WARN("Failed to create training bridge");
        }
    }

    /* Create mutex */
    bge->mutex = nimcp_mutex_create(NULL);
    if (!bge->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create enhanced BG mutex");
        bg_enhanced_destroy(bge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_enhanced_create: bge->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created enhanced BG with features: beta=%d neuro=%d hrl=%d mb=%d nac=%d sc=%d intern=%d cb=%d od=%d tc=%d",
        cfg.features.enable_beta_oscillations,
        cfg.features.enable_multi_neuromod,
        cfg.features.enable_hierarchical_rl,
        cfg.features.enable_model_based,
        cfg.features.enable_nucleus_accumbens,
        cfg.features.enable_superior_colliculus,
        cfg.features.enable_interneurons,
        cfg.features.enable_cerebellar_coord,
        cfg.features.enable_outcome_deval,
        cfg.features.enable_temporal_credit);

    return bge;
}

void bg_enhanced_destroy(bg_enhanced_t* bge) {
    if (!bge) return;

    /* Destroy enhancement modules */
    if (bge->beta) bg_beta_destroy(bge->beta);
    if (bge->neuromod) bg_neuromod_destroy(bge->neuromod);
    if (bge->hrl) bg_hrl_destroy(bge->hrl);
    if (bge->model_based) bg_mb_destroy(bge->model_based);
    if (bge->nac) nac_destroy(bge->nac);
    if (bge->sc) sc_destroy(bge->sc);
    if (bge->interneurons) sint_destroy(bge->interneurons);
    if (bge->cerebellar) bgcb_destroy(bge->cerebellar);
    if (bge->outcome_deval) bgod_destroy(bge->outcome_deval);
    if (bge->temporal_credit) bgtc_destroy(bge->temporal_credit);
    if (bge->training_bridge) bgtr_bridge_destroy(bge->training_bridge);

    /* Destroy core */
    if (bge->core) basal_ganglia_destroy(bge->core);

    /* Destroy mutex */
    if (bge->mutex) {
        nimcp_mutex_free(bge->mutex);
    }

    nimcp_free(bge);
}

int bg_enhanced_reset(bg_enhanced_t* bge) {
    if (!bge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bge->mutex);

    basal_ganglia_reset(bge->core);

    if (bge->beta) bg_beta_reset(bge->beta);
    if (bge->neuromod) bg_neuromod_reset(bge->neuromod);
    if (bge->hrl) bg_hrl_reset(bge->hrl);
    if (bge->model_based) bg_mb_reset(bge->model_based);
    if (bge->nac) nac_reset(bge->nac);
    if (bge->sc) sc_reset(bge->sc);
    if (bge->interneurons) sint_reset(bge->interneurons);
    if (bge->cerebellar) bgcb_reset(bge->cerebellar);
    if (bge->outcome_deval) bgod_reset(bge->outcome_deval);
    if (bge->temporal_credit) bgtc_reset(bge->temporal_credit);
    if (bge->training_bridge) bgtr_bridge_reset(bge->training_bridge);

    bge->in_option = false;
    bge->current_option = 0;

    nimcp_mutex_unlock(bge->mutex);
    return 0;
}

/* ============================================================================
 * CORE ACCESS
 * ============================================================================ */

basal_ganglia_t* bg_enhanced_get_core(bg_enhanced_t* bge) {
    return bge ? bge->core : NULL;
}

bg_beta_system_t* bg_enhanced_get_beta(bg_enhanced_t* bge) {
    return bge ? bge->beta : NULL;
}

bg_neuromod_system_t* bg_enhanced_get_neuromod(bg_enhanced_t* bge) {
    return bge ? bge->neuromod : NULL;
}

bg_hrl_system_t* bg_enhanced_get_hrl(bg_enhanced_t* bge) {
    return bge ? bge->hrl : NULL;
}

bg_model_based_t* bg_enhanced_get_model_based(bg_enhanced_t* bge) {
    return bge ? bge->model_based : NULL;
}

nucleus_accumbens_t* bg_enhanced_get_nac(bg_enhanced_t* bge) {
    return bge ? bge->nac : NULL;
}

superior_colliculus_t* bg_enhanced_get_sc(bg_enhanced_t* bge) {
    return bge ? bge->sc : NULL;
}

striatal_interneurons_t* bg_enhanced_get_interneurons(bg_enhanced_t* bge) {
    return bge ? bge->interneurons : NULL;
}

bg_cerebellar_coord_t* bg_enhanced_get_cerebellar(bg_enhanced_t* bge) {
    return bge ? bge->cerebellar : NULL;
}

bg_outcome_deval_t* bg_enhanced_get_outcome_deval(bg_enhanced_t* bge) {
    return bge ? bge->outcome_deval : NULL;
}

bg_temporal_credit_t* bg_enhanced_get_temporal_credit(bg_enhanced_t* bge) {
    return bge ? bge->temporal_credit : NULL;
}

bgtr_bridge_t* bg_enhanced_get_training_bridge(bg_enhanced_t* bge) {
    return bge ? bge->training_bridge : NULL;
}

/* ============================================================================
 * ACTION SELECTION
 * ============================================================================ */

int bg_enhanced_select_action(bg_enhanced_t* bge,
                               const float* cortical_input,
                               uint32_t* selected_action) {
    if (!bge || !cortical_input || !selected_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_select_action: required parameter is NULL (bge, cortical_input, selected_action)");
        return -1;
    }

    nimcp_mutex_lock(bge->mutex);

    uint32_t num_actions = bge->config.core_config.num_actions;

    /* Guard against stack buffer overflow */
    if (num_actions > BG_MAX_ACTIONS) {
        nimcp_mutex_unlock(bge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_enhanced_select_action: num_actions exceeds BG_MAX_ACTIONS");
        return -1;
    }

    float modified_input[BG_MAX_ACTIONS];

    /* Start with cortical input */
    memcpy(modified_input, cortical_input, num_actions * sizeof(float));

    /* 1. Apply beta oscillation gating */
    if (bge->beta) {
        bg_beta_state_t beta_state = bg_beta_get_state(bge->beta);
        if (beta_state == BG_BETA_STATE_LOCKED || beta_state == BG_BETA_STATE_REBOUNDING) {
            /* High beta = movement suppression */
            float suppression = bg_beta_get_power(bge->beta, BG_BETA_BAND_FULL);
            for (uint32_t i = 0; i < num_actions; i++) {
                modified_input[i] *= (1.0f - suppression * 0.5f);
            }
        }
    }

    /* 2. Apply interneuron modulation */
    /* Note: Interneuron modulation is applied internally by striatum */
    (void)bge->interneurons;

    /* 3. Apply neuromodulator effects */
    if (bge->neuromod) {
        bg_neuromod_effects_t effects;
        bg_neuromod_compute_effects(bge->neuromod, &effects);

        for (uint32_t i = 0; i < num_actions; i++) {
            /* Exploration bonus for high norepinephrine */
            modified_input[i] += effects.exploration_bonus * 0.1f;

            /* Attention gate from acetylcholine */
            modified_input[i] *= effects.attention_gate;
        }
    }

    /* 4. Apply NAc motivation influence */
    if (bge->nac) {
        float wanting = nac_get_wanting(bge->nac, 0);
        for (uint32_t i = 0; i < num_actions; i++) {
            modified_input[i] *= (1.0f + wanting * 0.3f);
        }
    }

    /* 5. Apply outcome devaluation effects */
    if (bge->outcome_deval) {
        bgod_modulate_action_values(bge->outcome_deval, modified_input, num_actions);
    }

    /* 6. Check if in HRL option */
    if (bge->hrl && bge->in_option) {
        /* HRL option execution - defer to core BG */
        /* Options are handled through normal action selection with biased values */
    }

    /* 7. Model-based vs model-free arbitration */
    if (bge->model_based) {
        /* Model-based planning influences action selection through value function */
        uint32_t planned_action = bg_mb_get_planned_action(bge->model_based, 0);
        if (planned_action < num_actions) {
            /* Boost planned action value */
            modified_input[planned_action] += 0.2f;
        }
    }

    /* 8. Core BG action selection */
    int result = basal_ganglia_select_action(bge->core, modified_input, selected_action);

    /* 9. Update temporal credit traces */
    if (bge->temporal_credit) {
        bgtc_update_trace(bge->temporal_credit, 0, *selected_action);
    }

    /* 10. Record action for training plasticity */
    if (bge->training_bridge) {
        bgtr_bridge_record_action(bge->training_bridge, *selected_action, 0, 0);
    }

    nimcp_mutex_unlock(bge->mutex);
    return result;
}

int bg_enhanced_select_action_for_goal(bg_enhanced_t* bge,
                                        const float* cortical_input,
                                        uint32_t goal_id,
                                        uint32_t* selected_action) {
    if (!bge || !cortical_input || !selected_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_select_action_for_goal: required parameter is NULL (bge, cortical_input, selected_action)");
        return -1;
    }

    /* If model-based is enabled, use it for goal-directed selection */
    if (bge->model_based) {
        *selected_action = bg_mb_get_planned_action(bge->model_based, 0);
        return 0;
    }

    /* Otherwise use standard selection */
    return bg_enhanced_select_action(bge, cortical_input, selected_action);
}

float bg_enhanced_get_action_value(const bg_enhanced_t* bge, uint32_t action_id) {
    if (!bge || !bge->core) return 0.0f;
    if (action_id >= BG_MAX_ACTIONS) return 0.0f;

    float value = 0.0f;

    /* Get core value */
    float thalamic_out[BG_MAX_ACTIONS];
    basal_ganglia_get_thalamic_output(bge->core, thalamic_out);
    value = thalamic_out[action_id];

    /* Adjust for outcome devaluation */
    if (bge->outcome_deval) {
        value *= bgod_get_action_value(bge->outcome_deval, action_id);
    }

    return value;
}

/* ============================================================================
 * HIERARCHICAL RL
 * ============================================================================ */

int bg_enhanced_start_option(bg_enhanced_t* bge, uint32_t option_id) {
    if (!bge || !bge->hrl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_start_option: required parameter is NULL (bge, bge->hrl)");
        return -1;
    }

    nimcp_mutex_lock(bge->mutex);
    bge->current_option = option_id;
    bge->in_option = true;
    /* Option execution managed by select_option in action selection */
    nimcp_mutex_unlock(bge->mutex);

    return 0;
}

bool bg_enhanced_in_option(const bg_enhanced_t* bge) {
    return bge ? bge->in_option : false;
}

int bg_enhanced_get_option_action(bg_enhanced_t* bge, uint32_t state, uint32_t* action) {
    if (!bge || !bge->hrl || !bge->in_option || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_get_option_action: required parameter is NULL (bge, bge->hrl, bge->in_option, action)");
        return -1;
    }
    /* Get active option and its primitive action */
    uint32_t opt_id = bg_hrl_get_active_option(bge->hrl);
    const bg_option_t* opt = bg_hrl_get_option(bge->hrl, opt_id);
    if (opt && opt->num_init_states > 0 && opt->initiation_states) {
        /* For simplicity, use the stored option's current action */
        *action = bge->current_option;
        return 0;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "bg_enhanced_get_option_action: validation failed");
    return -1;
}

/* ============================================================================
 * MOTIVATION
 * ============================================================================ */

int bg_enhanced_process_reward(bg_enhanced_t* bge, float reward, float predicted_reward) {
    if (!bge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bge->mutex);

    /* Update core BG dopamine */
    basal_ganglia_update_dopamine(bge->core, reward, predicted_reward);

    /* Update neuromodulators */
    if (bge->neuromod) {
        bg_neuromod_process_reward(bge->neuromod, reward, predicted_reward);
    }

    /* Update NAc */
    if (bge->nac) {
        float dopamine = basal_ganglia_get_dopamine(bge->core);
        nac_receive_vta_input(bge->nac, dopamine, reward - predicted_reward);
    }

    /* Update temporal credit */
    if (bge->temporal_credit) {
        float td_error = bgtc_compute_td_error(bge->temporal_credit, reward, predicted_reward, 0.0f, false);
        float updates[BGTC_MAX_STATES];
        uint32_t num_updates;
        bgtc_apply_credit(bge->temporal_credit, td_error, 0.1f, updates, &num_updates);
    }

    /* Process reward through training bridge for plasticity */
    if (bge->training_bridge) {
        bgtr_bridge_process_reward(bge->training_bridge, reward, predicted_reward);
    }

    nimcp_mutex_unlock(bge->mutex);
    return 0;
}

float bg_enhanced_get_motivation(const bg_enhanced_t* bge) {
    if (!bge || !bge->nac) return 0.5f;

    float core_out, shell_out;
    nac_get_output(bge->nac, &core_out, &shell_out);
    return (core_out + shell_out) / 2.0f;
}

float bg_enhanced_get_wanting(const bg_enhanced_t* bge, uint32_t reward_id) {
    if (!bge || !bge->nac) return 0.0f;
    return nac_get_wanting(bge->nac, reward_id);
}

float bg_enhanced_get_liking(const bg_enhanced_t* bge, uint32_t reward_id) {
    if (!bge || !bge->nac) return 0.0f;
    return nac_get_liking(bge->nac, reward_id);
}

/* ============================================================================
 * MODEL-BASED PLANNING
 * ============================================================================ */

int bg_enhanced_plan_to_goal(bg_enhanced_t* bge, uint32_t current_state,
                              uint32_t goal_state, uint32_t* action_sequence,
                              uint32_t* sequence_length) {
    if (!bge || !bge->model_based || !action_sequence || !sequence_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_plan_to_goal: required parameter is NULL (bge, bge->model_based, action_sequence, sequence_length)");
        return -1;
    }
    /* Use simulate_trajectory and extract action sequence */
    bg_mb_trajectory_t trajectory;
    if (bg_mb_simulate_trajectory(bge->model_based, current_state, 10, &trajectory) == 0) {
        *sequence_length = trajectory.length;
        for (uint32_t i = 0; i < trajectory.length && i < *sequence_length; i++) {
            action_sequence[i] = trajectory.actions[i];
        }
        return 0;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_enhanced_get_liking: validation failed");
    return -1;
}

float bg_enhanced_get_mb_weight(const bg_enhanced_t* bge) {
    if (!bge || !bge->model_based) return 0.0f;
    /* Return model accuracy as proxy for MB weight */
    return bg_mb_get_model_accuracy(bge->model_based);
}

int bg_enhanced_set_mb_weight(bg_enhanced_t* bge, float weight) {
    if (!bge || !bge->model_based) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_set_mb_weight: required parameter is NULL (bge, bge->model_based)");
        return -1;
    }
    /* Use arbitration mode to influence weight */
    /* Available modes: FIXED, UNCERTAINTY, RELIABILITY, SPEED_ACCURACY */
    bg_mb_arbitration_mode_t mode = (weight > 0.7f) ? BG_MB_ARBIT_RELIABILITY :
                                    (weight < 0.3f) ? BG_MB_ARBIT_SPEED_ACCURACY :
                                    BG_MB_ARBIT_UNCERTAINTY;
    return bg_mb_set_arbitration_mode(bge->model_based, mode);
}

/* ============================================================================
 * GAZE/ORIENTING
 * ============================================================================ */

int bg_enhanced_generate_saccade(bg_enhanced_t* bge, float target_x, float target_y,
                                  float* saccade_x, float* saccade_y) {
    if (!bge || !bge->sc || !saccade_x || !saccade_y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_set_mb_weight: required parameter is NULL (bge, bge->sc, saccade_x, saccade_y)");
        return -1;
    }

    sc_target_t target = {
        .position = {.x = target_x, .y = target_y},
        .salience = 1.0f,
        .priority = 1.0f,
        .id = 0,
        .is_tracked = true
    };

    sc_add_target(bge->sc, &target);
    sc_saccade_t saccade;
    int result = sc_get_saccade(bge->sc, &saccade);
    if (result == 0) {
        *saccade_x = saccade.target.x;
        *saccade_y = saccade.target.y;
    }
    return result;
}

int bg_enhanced_orient_to_target(bg_enhanced_t* bge, float target_x, float target_y) {
    float sx, sy;
    return bg_enhanced_generate_saccade(bge, target_x, target_y, &sx, &sy);
}

/* ============================================================================
 * CEREBELLAR COORDINATION
 * ============================================================================ */

int bg_enhanced_coordinate_timing(bg_enhanced_t* bge, uint32_t action_id,
                                   float* timing_adjustment) {
    if (!bge || !bge->cerebellar || !timing_adjustment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_orient_to_target: required parameter is NULL (bge, bge->cerebellar, timing_adjustment)");
        return -1;
    }
    /* Get timing state and compute adjustment */
    bgcb_timing_state_t timing_state;
    if (bgcb_get_timing_state(bge->cerebellar, &timing_state) == 0) {
        /* Compute adjustment from timing accuracy and sync phase */
        *timing_adjustment = (1.0f - timing_state.timing_accuracy) * timing_state.sync_phase;
        return 0;
    }
    *timing_adjustment = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_enhanced_orient_to_target: validation failed");
    return -1;
}

int bg_enhanced_receive_cerebellar_prediction(bg_enhanced_t* bge,
                                               const float* prediction,
                                               uint32_t prediction_dim) {
    if (!bge || !bge->cerebellar || !prediction || prediction_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_orient_to_target: required parameter is NULL (bge, bge->cerebellar, prediction)");
        return -1;
    }
    /* Set timing prediction from first element of prediction array */
    return bgcb_set_timing_prediction(bge->cerebellar, prediction[0]);
}

/* ============================================================================
 * OUTCOME DEVALUATION
 * ============================================================================ */

int bg_enhanced_devalue_outcome(bg_enhanced_t* bge, uint32_t outcome_id,
                                 bgod_method_t method, float magnitude) {
    if (!bge || !bge->outcome_deval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_orient_to_target: required parameter is NULL (bge, bge->outcome_deval)");
        return -1;
    }

    switch (method) {
        case BGOD_METHOD_SATIETY:
            return bgod_devalue_by_satiety(bge->outcome_deval, outcome_id, magnitude);
        case BGOD_METHOD_AVERSION:
            return bgod_devalue_by_aversion(bge->outcome_deval, outcome_id, magnitude);
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_orient_to_target: required parameter is NULL (bge, bge->outcome_deval)");
            return -1;
    }
}

bgod_behavior_type_t bg_enhanced_test_behavior(bg_enhanced_t* bge, uint32_t action_id) {
    if (!bge || !bge->outcome_deval) return BGOD_BEHAVIOR_UNKNOWN;

    bgod_test_result_t result;
    if (bgod_run_test(bge->outcome_deval, action_id, 0, BGOD_METHOD_SATIETY, &result) != 0) {
        return BGOD_BEHAVIOR_UNKNOWN;
    }
    return bgod_classify_behavior(&result);
}

/* ============================================================================
 * PROCESSING
 * ============================================================================ */

int bg_enhanced_step(bg_enhanced_t* bge, float dt_ms) {
    if (!bge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bge->mutex);

    /* Step all subsystems */
    basal_ganglia_step(bge->core, dt_ms);

    if (bge->beta) bg_beta_step(bge->beta, dt_ms);
    if (bge->neuromod) bg_neuromod_step(bge->neuromod, dt_ms);
    /* Note: HRL step requires state/reward, skip time-only updates */
    /* if (bge->hrl) bg_hrl_step(bge->hrl, state, reward, dt_ms); */
    /* Note: Model-based has no simple step function */
    if (bge->nac) nac_step(bge->nac, dt_ms);
    if (bge->sc) sc_step(bge->sc, dt_ms);
    if (bge->interneurons) sint_step(bge->interneurons, dt_ms);
    if (bge->cerebellar) bgcb_step(bge->cerebellar, dt_ms);
    if (bge->outcome_deval) bgod_step(bge->outcome_deval, dt_ms);
    if (bge->temporal_credit) bgtc_step(bge->temporal_credit, dt_ms);
    if (bge->training_bridge) bgtr_bridge_decay_traces(bge->training_bridge, dt_ms);

    nimcp_mutex_unlock(bge->mutex);
    return 0;
}

int bg_enhanced_process_input(bg_enhanced_t* bge, const float* cortical_input) {
    uint32_t selected;
    return bg_enhanced_select_action(bge, cortical_input, &selected);
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int bg_enhanced_get_stats(const bg_enhanced_t* bge, bg_enhanced_stats_t* stats) {
    if (!bge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_get_stats: required parameter is NULL (bge, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bge->mutex);

    memset(stats, 0, sizeof(bg_enhanced_stats_t));

    /* Core stats */
    basal_ganglia_get_stats(bge->core, &stats->core_stats);

    /* Beta stats */
    if (bge->beta) {
        stats->current_beta_power = bg_beta_get_power(bge->beta, BG_BETA_BAND_FULL);
        stats->beta_state = bg_beta_get_state(bge->beta);
    }

    /* Neuromod stats */
    if (bge->neuromod) {
        bg_neuromod_get_all_levels(bge->neuromod, &stats->neuromod_levels);
        stats->motive_state = bg_neuromod_get_state(bge->neuromod);
    }

    /* HRL stats */
    if (bge->hrl) {
        bg_hrl_stats_t hrl_stats;
        bg_hrl_get_stats(bge->hrl, &hrl_stats);
        stats->active_options = hrl_stats.active_options;
        stats->option_completions = hrl_stats.goals_achieved;
    }

    /* Model-based stats */
    if (bge->model_based) {
        bg_mb_arbitration_t arbit;
        if (bg_mb_get_arbitration(bge->model_based, &arbit) == 0) {
            stats->model_based_weight = arbit.model_based_weight;
        }
        bg_mb_stats_t mb_stats;
        bg_mb_get_stats(bge->model_based, &mb_stats);
        stats->planning_steps = mb_stats.planning_episodes;
    }

    /* NAc stats */
    if (bge->nac) {
        nac_stats_t nac_stats;
        nac_get_stats(bge->nac, &nac_stats);
        stats->wanting_level = nac_stats.wanting_level;
        stats->liking_level = nac_stats.liking_level;
        stats->motivation_state = nac_stats.state;
    }

    /* Interneuron stats */
    if (bge->interneurons) {
        sint_get_stats(bge->interneurons, &stats->interneuron_state);
    }

    /* Outcome devaluation stats */
    if (bge->outcome_deval) {
        stats->behavior_type = bgod_get_overall_behavior(bge->outcome_deval);
        stats->goal_weight = bgod_get_goal_weight(bge->outcome_deval);
        stats->habit_weight = bgod_get_habit_weight(bge->outcome_deval);
    }

    /* Training plasticity stats */
    if (bge->training_bridge) {
        bgtr_bridge_get_stats(bge->training_bridge, &stats->training_stats);
        stats->last_rpe = bge->training_bridge->last_rpe;
        stats->active_traces = bgtr_bridge_get_trace_count(bge->training_bridge);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bge->mutex);
    return 0;
}

bool bg_enhanced_feature_enabled(const bg_enhanced_t* bge, const char* feature_name) {
    if (!bge || !feature_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_enhanced_feature_enabled: required parameter is NULL (bge, feature_name)");
        return false;
    }

    if (strcmp(feature_name, "beta") == 0) return bge->beta != NULL;
    if (strcmp(feature_name, "neuromod") == 0) return bge->neuromod != NULL;
    if (strcmp(feature_name, "hrl") == 0) return bge->hrl != NULL;
    if (strcmp(feature_name, "model_based") == 0) return bge->model_based != NULL;
    if (strcmp(feature_name, "nac") == 0) return bge->nac != NULL;
    if (strcmp(feature_name, "sc") == 0) return bge->sc != NULL;
    if (strcmp(feature_name, "interneurons") == 0) return bge->interneurons != NULL;
    if (strcmp(feature_name, "cerebellar") == 0) return bge->cerebellar != NULL;
    if (strcmp(feature_name, "outcome_deval") == 0) return bge->outcome_deval != NULL;
    if (strcmp(feature_name, "temporal_credit") == 0) return bge->temporal_credit != NULL;
    if (strcmp(feature_name, "training") == 0 || strcmp(feature_name, "training_plasticity") == 0)
        return bge->training_bridge != NULL;

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_enhanced_feature_enabled: validation failed");
    return false;
}

float bg_enhanced_get_beta_power(const bg_enhanced_t* bge) {
    if (!bge || !bge->beta) return 0.0f;
    return bg_beta_get_power(bge->beta, BG_BETA_BAND_FULL);
}

bg_beta_state_t bg_enhanced_get_beta_state(const bg_enhanced_t* bge) {
    if (!bge || !bge->beta) return BG_BETA_STATE_BASELINE;
    return bg_beta_get_state(bge->beta);
}

float bg_enhanced_get_neuromod_level(const bg_enhanced_t* bge, bg_neuromod_type_t type) {
    if (!bge || !bge->neuromod) return 0.0f;
    return bg_neuromod_get_level(bge->neuromod, type);
}
