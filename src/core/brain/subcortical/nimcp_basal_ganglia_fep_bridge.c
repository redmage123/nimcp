/**
 * @file nimcp_basal_ganglia_fep_bridge.c
 * @brief Implementation of Basal Ganglia-to-FEP Integration Bridge
 *
 * Implements action selection as expected free energy minimization.
 * Maps dopamine reward prediction error to FEP surprise signals.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_fep_bridge.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>
#include <float.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_fep_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


#define LOG_MODULE "BASAL_GANGLIA_FEP_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define BG_FEP_MIN_PRECISION 0.01f
#define BG_FEP_MAX_PRECISION 100.0f
#include "constants/nimcp_constants.h"
#define BG_FEP_EPSILON NIMCP_EPSILON_NUMERICAL
#define BG_FEP_ENTROPY_SCALE 0.1f

//=============================================================================
// Internal Structure
//=============================================================================

struct bg_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    bg_fep_config_t config;

    /* Current model */
    bg_fep_model_t current_model;
    float model_evidence[BG_FEP_NUM_POLICY_MODELS];

    /* Action evaluation */
    uint32_t num_actions;
    bg_fep_action_eval_t evaluations[BG_FEP_MAX_ACTIONS];
    float action_beliefs[BG_FEP_MAX_ACTIONS];
    float action_precision[BG_FEP_MAX_ACTIONS];

    /* Habit system */
    float habit_strength[BG_FEP_MAX_ACTIONS];
    float habit_prior[BG_FEP_MAX_ACTIONS];

    /* Outcome models */
    bg_fep_outcome_model_t outcome_models[BG_FEP_MAX_ACTIONS];

    /* Current state */
    float current_free_energy;
    float current_surprise;
    float policy_entropy;
    uint32_t selected_action;
    bg_fep_confidence_t confidence;
    bg_fep_action_state_t action_state;

    /* Dopamine state */
    float dopamine_level;
    float dopamine_precision;
    float recent_rpe;

    /* Prediction errors */
    bg_fep_errors_t last_errors;

    /* Statistics */
    bg_fep_stats_t stats;

    /* Connections */
    basal_ganglia_t* connected_bg;
    void* connected_orchestrator;
};

//=============================================================================
// Helper Functions
//=============================================================================

static float safe_log(float x) {
    if (x < BG_FEP_EPSILON) x = BG_FEP_EPSILON;
    return logf(x);
}

static float softmax_temperature(float value, float temperature) {
    if (temperature < BG_FEP_EPSILON) temperature = BG_FEP_EPSILON;
    return expf(value / temperature);
}

/**
 * @brief Compute entropy of action distribution
 */
static float compute_entropy(const float* probs, uint32_t n) {
    float entropy = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probs[i] > BG_FEP_EPSILON) {
            entropy -= probs[i] * safe_log(probs[i]);
        }
    }
    return entropy;
}

/**
 * @brief Compute softmax probabilities from expected free energies
 */
static void compute_action_posteriors(
    bg_fep_bridge_t* bridge,
    const bg_fep_action_eval_t* evals,
    uint32_t n
) {
    float temp = bridge->config.selection_temperature;
    float sum = 0.0f;

    /* Compute exp(-G/temp) for each action */
    for (uint32_t i = 0; i < n; i++) {
        /* Lower expected FE = higher probability (negate for softmax) */
        float neg_efe = -evals[i].expected_free_energy;
        bridge->evaluations[i].posterior_probability = softmax_temperature(neg_efe, temp);
        sum += bridge->evaluations[i].posterior_probability;
    }

    /* Normalize */
    if (sum > BG_FEP_EPSILON) {
        for (uint32_t i = 0; i < n; i++) {
            bridge->evaluations[i].posterior_probability /= sum;
        }
    }
}

/**
 * @brief Map entropy to confidence level
 */
static bg_fep_confidence_t entropy_to_confidence(float entropy, uint32_t n) {
    float max_entropy = safe_log((float)n);  /* Uniform distribution */
    float normalized = entropy / (max_entropy + BG_FEP_EPSILON);

    if (normalized < 0.3f) return BG_FEP_CONFIDENCE_HIGH;
    if (normalized < 0.7f) return BG_FEP_CONFIDENCE_MEDIUM;
    return BG_FEP_CONFIDENCE_LOW;
}

/**
 * @brief Compute expected free energy for action
 *
 * G = E_Q[log Q(s|a) - log P(o|s) - log P(s)]
 *   = -E_Q[log P(o)] + E_Q[log Q(s|a) - log P(s)]
 *   = -pragmatic - epistemic
 *
 * Pragmatic: Expected reward (lower FE = higher reward)
 * Epistemic: Information gain (lower FE = more learning)
 */
static float compute_action_efe(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float action_value
) {
    const bg_fep_config_t* cfg = &bridge->config;
    bg_fep_outcome_model_t* outcome = &bridge->outcome_models[action_id];

    /* Pragmatic value: expected reward */
    float pragmatic = cfg->pragmatic_weight * outcome->reward_mean;

    /* Epistemic value: information gain (based on outcome variance) */
    float epistemic = cfg->epistemic_weight * outcome->reward_variance;

    /* Cost term */
    float cost = cfg->cost_weight * outcome->cost_mean;

    /* Risk term (variance penalty) */
    float risk = cfg->risk_sensitivity * outcome->reward_variance;

    /* Habit prior boost */
    float habit_boost = 0.0f;
    if (bridge->habit_strength[action_id] > 0.0f) {
        habit_boost = cfg->habit_prior_boost * bridge->habit_strength[action_id];
    }

    /* Expected free energy (lower = better) */
    /* Negate pragmatic and epistemic because we want to maximize them */
    float efe = -pragmatic - epistemic + cost + risk - habit_boost;

    return efe;
}

//=============================================================================
// Configuration Functions
//=============================================================================

void bg_fep_default_config(bg_fep_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(bg_fep_config_t));

    /* Model settings */
    config->default_model = BG_FEP_MODEL_EXPLOIT;
    config->auto_model_selection = true;
    config->model_switch_threshold = 0.5f;

    /* Precision settings */
    config->precision_mode = BG_FEP_PRECISION_DOPAMINE;
    config->action_precision = 1.0f;
    config->outcome_precision = 1.0f;
    config->prior_precision = 0.5f;

    /* Free energy computation */
    config->epistemic_weight = 0.3f;    /* Exploration bonus */
    config->pragmatic_weight = 1.0f;    /* Reward seeking */
    config->cost_weight = 0.5f;         /* Cost sensitivity */
    config->risk_sensitivity = 0.2f;    /* Risk aversion */

    /* Habit parameters */
    config->habit_prior_boost = 0.5f;
    config->habit_precision_boost = 2.0f;
    config->habit_threshold = 0.7f;

    /* Softmax temperature */
    config->selection_temperature = 1.0f;

    /* Learning rates */
    config->reward_learning_rate = 0.1f;
    config->transition_learning_rate = 0.05f;
    config->precision_learning_rate = 0.02f;
}

bool bg_fep_validate_config(const bg_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_validate_config: config is NULL");
        return false;
    }

    if (config->default_model >= BG_FEP_NUM_POLICY_MODELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_fep_validate_config: capacity exceeded");
        return false;
    }
    if (config->precision_mode > BG_FEP_PRECISION_HIERARCHICAL) {
        return false;
    }

    if (config->action_precision < 0.0f) {
        return false;
    }
    if (config->outcome_precision < 0.0f) {
        return false;
    }
    if (config->selection_temperature <= 0.0f) {
        return false;
    }

    if (config->reward_learning_rate < 0.0f || config->reward_learning_rate > 1.0f) {
        return false;
    }
    if (config->transition_learning_rate < 0.0f || config->transition_learning_rate > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

bg_fep_bridge_t* bg_fep_create(const bg_fep_config_t* config) {
    bg_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(bg_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply config */
    if (config) {
        if (!bg_fep_validate_config(config)) {
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_create: bg_fep_validate_config is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bg_fep_default_config(&bridge->config);
    }

    bridge->current_model = bridge->config.default_model;

    /* Initialize model evidence uniformly */
    for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
        bridge->model_evidence[i] = 1.0f / BG_FEP_NUM_POLICY_MODELS;
    }

    /* Initialize action precision */
    for (uint32_t i = 0; i < BG_FEP_MAX_ACTIONS; i++) {
        bridge->action_precision[i] = bridge->config.action_precision;
        bridge->action_beliefs[i] = 0.0f;
        bridge->habit_strength[i] = 0.0f;
        bridge->habit_prior[i] = 1.0f;

        /* Initialize outcome models with priors */
        bridge->outcome_models[i].reward_mean = 0.0f;
        bridge->outcome_models[i].reward_variance = 1.0f;  /* High initial uncertainty */
        bridge->outcome_models[i].cost_mean = 0.5f;
        bridge->outcome_models[i].success_probability = 0.5f;
    }

    /* Initialize dopamine state */
    bridge->dopamine_level = 0.5f;  /* Baseline */
    bridge->dopamine_precision = bridge->config.action_precision;

    bridge->action_state = BG_FEP_ACTION_PENDING;
    bridge->confidence = BG_FEP_CONFIDENCE_MEDIUM;

    bridge_base_init(&bridge->base, 0, "basal_ganglia_fep_bridge");

    NIMCP_LOGGING_INFO("Created %s bridge", "basal_ganglia_fep");
    return bridge;
}

void bg_fep_destroy(bg_fep_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "basal_ganglia_fep");
    nimcp_free(bridge);
}

int bg_fep_reset(bg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_reset: bridge is NULL");
        return -1;
    }

    /* Reset to initial state while preserving config */
    bg_fep_config_t saved_config = bridge->config;
    basal_ganglia_t* saved_bg = bridge->connected_bg;
    void* saved_orch = bridge->connected_orchestrator;

    memset(bridge, 0, sizeof(bg_fep_bridge_t));
    bridge->config = saved_config;
    bridge->connected_bg = saved_bg;
    bridge->connected_orchestrator = saved_orch;

    /* Reinitialize */
    bridge->current_model = bridge->config.default_model;

    for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
        bridge->model_evidence[i] = 1.0f / BG_FEP_NUM_POLICY_MODELS;
    }

    for (uint32_t i = 0; i < BG_FEP_MAX_ACTIONS; i++) {
        bridge->action_precision[i] = bridge->config.action_precision;
        bridge->habit_prior[i] = 1.0f;
        bridge->outcome_models[i].reward_variance = 1.0f;
        bridge->outcome_models[i].success_probability = 0.5f;
    }

    bridge->dopamine_level = 0.5f;
    bridge->dopamine_precision = bridge->config.action_precision;
    bridge->action_state = BG_FEP_ACTION_PENDING;
    bridge->confidence = BG_FEP_CONFIDENCE_MEDIUM;

    return 0;
}

//=============================================================================
// Prediction Error Functions
//=============================================================================

int bg_fep_compute_errors(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float actual_reward,
    float expected_reward,
    bg_fep_errors_t* errors
) {
    if (!bridge || !errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_compute_errors: required parameter is NULL (bridge, errors)");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_compute_errors: capacity exceeded");
        return -1;
    }

    memset(errors, 0, sizeof(bg_fep_errors_t));

    /* Reward prediction error (TD error) */
    errors->reward_error = actual_reward - expected_reward;

    /* Outcome prediction error */
    bg_fep_outcome_model_t* model = &bridge->outcome_models[action_id];
    errors->outcome_error = actual_reward - model->reward_mean;

    /* State transition error (placeholder) */
    errors->state_error = 0.0f;

    /* Cost prediction error */
    errors->cost_error = 0.0f;

    /* Precision-weighted error */
    float precision = bridge->action_precision[action_id];
    errors->precision_weighted_error = precision * fabsf(errors->outcome_error);

    /* Total free energy (approximate) */
    /* F = E_Q[log Q - log P] ~= prediction error^2 * precision / 2 */
    errors->total_free_energy = 0.5f * precision * errors->outcome_error * errors->outcome_error;

    /* Store for later access */
    bridge->last_errors = *errors;
    bridge->current_free_energy = errors->total_free_energy;
    bridge->current_surprise = fabsf(errors->outcome_error);

    /* Update statistics */
    bridge->stats.avg_prediction_error =
        0.9f * bridge->stats.avg_prediction_error +
        0.1f * fabsf(errors->outcome_error);

    return 0;
}

float bg_fep_get_free_energy(const bg_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->current_free_energy;
}

float bg_fep_get_surprise(const bg_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->current_surprise;
}

int bg_fep_update_precision(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float prediction_error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_update_precision: bridge is NULL");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_update_precision: capacity exceeded");
        return -1;
    }

    float lr = bridge->config.precision_learning_rate;
    float pe_sq = prediction_error * prediction_error;

    /* Precision decreases with large prediction errors
     * (high variance = low precision) */
    float current = bridge->action_precision[action_id];
    float target = 1.0f / (pe_sq + BG_FEP_EPSILON);

    bridge->action_precision[action_id] = (1.0f - lr) * current + lr * target;
    bridge->action_precision[action_id] = nimcp_clampf(
        bridge->action_precision[action_id],
        BG_FEP_MIN_PRECISION,
        BG_FEP_MAX_PRECISION
    );

    return 0;
}

//=============================================================================
// Active Inference - Action Selection
//=============================================================================

int bg_fep_evaluate_actions(
    bg_fep_bridge_t* bridge,
    const float* action_values,
    uint32_t num_actions,
    bg_fep_action_eval_t* evaluations
) {
    if (!bridge || !action_values || !evaluations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_evaluate_actions: required parameter is NULL (bridge, action_values, evaluations)");
        return -1;
    }
    if (num_actions > BG_FEP_MAX_ACTIONS) num_actions = BG_FEP_MAX_ACTIONS;

    bridge->num_actions = num_actions;

    for (uint32_t i = 0; i < num_actions; i++) {
        bg_fep_action_eval_t* eval = &evaluations[i];
        bg_fep_outcome_model_t* model = &bridge->outcome_models[i];

        eval->action_id = i;

        /* Use provided action value and outcome model */
        eval->pragmatic_value = action_values[i];
        eval->epistemic_value = model->reward_variance;  /* Higher variance = more to learn */
        eval->cost = model->cost_mean;
        eval->risk = model->reward_variance;

        /* Compute expected free energy */
        eval->expected_free_energy = compute_action_efe(bridge, i, action_values[i]);

        /* Store in bridge */
        bridge->evaluations[i] = *eval;
        bridge->action_beliefs[i] = action_values[i];
    }

    /* Compute posterior probabilities via softmax */
    compute_action_posteriors(bridge, evaluations, num_actions);

    /* Copy posteriors back to output */
    for (uint32_t i = 0; i < num_actions; i++) {
        evaluations[i].posterior_probability = bridge->evaluations[i].posterior_probability;
    }

    /* Compute policy entropy */
    float probs[BG_FEP_MAX_ACTIONS];
    for (uint32_t i = 0; i < num_actions; i++) {
        probs[i] = bridge->evaluations[i].posterior_probability;
    }
    bridge->policy_entropy = compute_entropy(probs, num_actions);
    bridge->confidence = entropy_to_confidence(bridge->policy_entropy, num_actions);

    /* Update average entropy stat */
    bridge->stats.avg_entropy =
        0.9f * bridge->stats.avg_entropy + 0.1f * bridge->policy_entropy;

    return 0;
}

int bg_fep_select_action(
    bg_fep_bridge_t* bridge,
    const bg_fep_action_eval_t* evaluations,
    uint32_t num_actions,
    uint32_t* selected
) {
    if (!bridge || !evaluations || !selected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_select_action: required parameter is NULL (bridge, evaluations, selected)");
        return -1;
    }
    if (num_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_fep_select_action: num_actions is zero");
        return -1;
    }

    /* Find action with highest posterior probability */
    uint32_t best = 0;
    float best_prob = -FLT_MAX;

    for (uint32_t i = 0; i < num_actions; i++) {
        if (evaluations[i].posterior_probability > best_prob) {
            best_prob = evaluations[i].posterior_probability;
            best = i;
        }
    }

    *selected = best;
    bridge->selected_action = best;
    bridge->action_state = BG_FEP_ACTION_SELECTED;

    /* Update statistics */
    bridge->stats.action_selections++;
    bridge->stats.total_inferences++;

    /* Track if habitual or exploratory */
    if (bridge->habit_strength[best] > bridge->config.habit_threshold) {
        bridge->stats.habit_selections++;
    }

    if (bridge->current_model == BG_FEP_MODEL_EXPLORE) {
        bridge->stats.explore_selections++;
    }

    /* Update exploration ratio */
    bridge->stats.exploration_ratio =
        (float)bridge->stats.explore_selections /
        (float)(bridge->stats.action_selections + 1);

    return 0;
}

float bg_fep_expected_free_energy(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) return 0.0f;
    if (action_id >= BG_FEP_MAX_ACTIONS) return 0.0f;

    return bridge->evaluations[action_id].expected_free_energy;
}

float bg_fep_get_policy_entropy(const bg_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->policy_entropy;
}

int bg_fep_get_inference_state(
    const bg_fep_bridge_t* bridge,
    bg_fep_inference_t* inference
) {
    if (!bridge || !inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_get_inference_state: required parameter is NULL (bridge, inference)");
        return -1;
    }

    memset(inference, 0, sizeof(bg_fep_inference_t));

    inference->num_actions = bridge->num_actions;

    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        inference->beliefs[i] = bridge->action_beliefs[i];
        inference->precision[i] = bridge->action_precision[i];
    }

    inference->evaluations = (bg_fep_action_eval_t*)bridge->evaluations;
    inference->policy_entropy = bridge->policy_entropy;
    inference->selected_action = bridge->selected_action;
    inference->confidence = bridge->confidence;

    return 0;
}

//=============================================================================
// Generative Model Functions
//=============================================================================

int bg_fep_set_model(bg_fep_bridge_t* bridge, bg_fep_model_t model) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_set_model: bridge is NULL");
        return -1;
    }
    if (model >= BG_FEP_NUM_POLICY_MODELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_fep_set_model: capacity exceeded");
        return -1;
    }

    if (bridge->current_model != model) {
        bridge->stats.model_switches++;
    }

    bridge->current_model = model;
    return 0;
}

bg_fep_model_t bg_fep_get_best_model(const bg_fep_bridge_t* bridge) {
    if (!bridge) return BG_FEP_MODEL_EXPLOIT;

    bg_fep_model_t best = BG_FEP_MODEL_EXPLOIT;
    float best_evidence = -FLT_MAX;

    for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
        if (bridge->model_evidence[i] > best_evidence) {
            best_evidence = bridge->model_evidence[i];
            best = (bg_fep_model_t)i;
        }
    }

    return best;
}

float bg_fep_get_model_evidence(
    const bg_fep_bridge_t* bridge,
    bg_fep_model_t model
) {
    if (!bridge) return 0.0f;
    if (model >= BG_FEP_NUM_POLICY_MODELS) return 0.0f;
    return bridge->model_evidence[model];
}

int bg_fep_update_outcome_model(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float outcome_reward,
    float outcome_cost,
    bool success
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_update_outcome_model: bridge is NULL");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_update_outcome_model: capacity exceeded");
        return -1;
    }

    bg_fep_outcome_model_t* model = &bridge->outcome_models[action_id];
    float lr = bridge->config.reward_learning_rate;

    /* Update reward mean (exponential moving average) */
    float old_mean = model->reward_mean;
    model->reward_mean = (1.0f - lr) * old_mean + lr * outcome_reward;

    /* Update reward variance */
    float delta = outcome_reward - old_mean;
    float delta2 = outcome_reward - model->reward_mean;
    model->reward_variance = (1.0f - lr) * model->reward_variance + lr * delta * delta2;
    if (model->reward_variance < BG_FEP_EPSILON) {
        model->reward_variance = BG_FEP_EPSILON;
    }

    /* Update cost */
    model->cost_mean = (1.0f - lr) * model->cost_mean + lr * outcome_cost;

    /* Update success probability */
    float success_f = success ? 1.0f : 0.0f;
    model->success_probability =
        (1.0f - lr) * model->success_probability + lr * success_f;

    return 0;
}

int bg_fep_predict_outcome(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id,
    bg_fep_outcome_model_t* outcome
) {
    if (!bridge || !outcome) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_predict_outcome: required parameter is NULL (bridge, outcome)");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_predict_outcome: capacity exceeded");
        return -1;
    }

    *outcome = bridge->outcome_models[action_id];
    return 0;
}

//=============================================================================
// Habit-Related Functions
//=============================================================================

int bg_fep_set_habit(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_set_habit: bridge is NULL");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_set_habit: capacity exceeded");
        return -1;
    }

    bridge->habit_strength[action_id] = nimcp_clampf(strength, 0.0f, 1.0f);

    /* Boost prior for habitual actions */
    bridge->habit_prior[action_id] =
        1.0f + bridge->config.habit_prior_boost * bridge->habit_strength[action_id];

    /* Boost precision for habits (more confident about value) */
    if (strength > bridge->config.habit_threshold) {
        bridge->action_precision[action_id] *= bridge->config.habit_precision_boost;
        bridge->action_precision[action_id] = nimcp_clampf(
            bridge->action_precision[action_id],
            BG_FEP_MIN_PRECISION,
            BG_FEP_MAX_PRECISION
        );
    }

    return 0;
}

float bg_fep_get_habit_prior(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) return 1.0f;
    if (action_id >= BG_FEP_MAX_ACTIONS) return 1.0f;
    return bridge->habit_prior[action_id];
}

int bg_fep_clear_habit(bg_fep_bridge_t* bridge, uint32_t action_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_clear_habit: bridge is NULL");
        return -1;
    }
    if (action_id >= BG_FEP_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bg_fep_clear_habit: capacity exceeded");
        return -1;
    }

    bridge->habit_strength[action_id] = 0.0f;
    bridge->habit_prior[action_id] = 1.0f;

    return 0;
}

bool bg_fep_is_habit_mode(const bg_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->current_model == BG_FEP_MODEL_HABIT;
}

//=============================================================================
// Dopamine Integration
//=============================================================================

int bg_fep_set_dopamine(bg_fep_bridge_t* bridge, float dopamine_level) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_set_dopamine: bridge is NULL");
        return -1;
    }

    bridge->dopamine_level = nimcp_clampf(dopamine_level, 0.0f, 1.0f);

    /* Dopamine modulates precision
     * Higher dopamine = higher precision on GO signals (D1)
     * Lower dopamine = higher precision on NO-GO signals (D2)
     */
    if (bridge->config.precision_mode == BG_FEP_PRECISION_DOPAMINE) {
        /* Scale precision around baseline (0.5) */
        float da_factor = 0.5f + dopamine_level;  /* Range: 0.5 to 1.5 */
        bridge->dopamine_precision = bridge->config.action_precision * da_factor;
    }

    return 0;
}

float bg_fep_get_dopamine_precision(const bg_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->dopamine_precision;
}

int bg_fep_process_rpe(bg_fep_bridge_t* bridge, float rpe) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_process_rpe: bridge is NULL");
        return -1;
    }

    bridge->recent_rpe = rpe;

    /* RPE updates model evidence */
    /* Positive RPE increases evidence for current model */
    int model_idx = (int)bridge->current_model;
    float lr = NIMCP_LEARNING_RATE_COARSE;

    if (rpe > 0) {
        bridge->model_evidence[model_idx] += lr * rpe;
    } else {
        /* Negative RPE decreases evidence, increases evidence for alternatives */
        bridge->model_evidence[model_idx] += lr * rpe;  /* Negative */
        for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
            if (i != model_idx) {
                bridge->model_evidence[i] -= lr * rpe * 0.25f;  /* Distribute */
            }
        }
    }

    /* Normalize model evidence */
    float sum = 0.0f;
    for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
        if (bridge->model_evidence[i] < BG_FEP_EPSILON) {
            bridge->model_evidence[i] = BG_FEP_EPSILON;
        }
        sum += bridge->model_evidence[i];
    }
    for (int i = 0; i < BG_FEP_NUM_POLICY_MODELS; i++) {
        bridge->model_evidence[i] /= sum;
    }

    /* Auto model selection if enabled */
    if (bridge->config.auto_model_selection) {
        bg_fep_model_t best = bg_fep_get_best_model(bridge);
        if (best != bridge->current_model) {
            float evidence_diff = bridge->model_evidence[best] -
                                  bridge->model_evidence[bridge->current_model];
            if (evidence_diff > bridge->config.model_switch_threshold) {
                bg_fep_set_model(bridge, best);
            }
        }
    }

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int bg_fep_connect_basal_ganglia(
    bg_fep_bridge_t* bridge,
    basal_ganglia_t* bg
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_connect_basal_ganglia: bridge is NULL");
        return -1;
    }
    bridge->connected_bg = bg;
    return 0;
}

int bg_fep_connect_orchestrator(
    bg_fep_bridge_t* bridge,
    void* orchestrator
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_connect_orchestrator: bridge is NULL");
        return -1;
    }
    bridge->connected_orchestrator = orchestrator;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int bg_fep_update(bg_fep_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_update: bridge is NULL");
        return -1;
    }

    (void)dt;  /* Time-based updates could be added here */

    /* Sync with basal ganglia if connected */
    if (bridge->connected_bg) {
        bg_fep_sync_with_bg(bridge);
    }

    /* Update average free energy */
    bridge->stats.avg_free_energy =
        0.95f * bridge->stats.avg_free_energy +
        0.05f * bridge->current_free_energy;

    /* Update average precision */
    float avg_prec = 0.0f;
    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        avg_prec += bridge->action_precision[i];
    }
    if (bridge->num_actions > 0) {
        avg_prec /= (float)bridge->num_actions;
    }
    bridge->stats.avg_precision =
        0.95f * bridge->stats.avg_precision + 0.05f * avg_prec;

    return 0;
}

int bg_fep_sync_with_bg(bg_fep_bridge_t* bridge) {
    if (!bridge || !bridge->connected_bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_sync_with_bg: required parameter is NULL (bridge, bridge->connected_bg)");
        return -1;
    }

    basal_ganglia_t* bg = bridge->connected_bg;

    /* Sync dopamine level */
    float da = basal_ganglia_get_dopamine(bg);
    bg_fep_set_dopamine(bridge, da);

    /* Sync RPE */
    float rpe = basal_ganglia_get_rpe(bg);
    bg_fep_process_rpe(bridge, rpe);

    /* Sync habit mode */
    if (basal_ganglia_is_habit_mode(bg)) {
        if (bridge->current_model != BG_FEP_MODEL_HABIT) {
            bg_fep_set_model(bridge, BG_FEP_MODEL_HABIT);
        }
    }

    return 0;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

int bg_fep_get_stats(
    const bg_fep_bridge_t* bridge,
    bg_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int bg_fep_reset_stats(bg_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_fep_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bg_fep_stats_t));
    return 0;
}

const char* bg_fep_model_name(bg_fep_model_t model) {
    switch (model) {
        case BG_FEP_MODEL_EXPLORE:  return "Explore";
        case BG_FEP_MODEL_EXPLOIT:  return "Exploit";
        case BG_FEP_MODEL_HABIT:    return "Habit";
        case BG_FEP_MODEL_CAUTIOUS: return "Cautious";
        default: return "Unknown";
    }
}

const char* bg_fep_action_state_name(bg_fep_action_state_t state) {
    switch (state) {
        case BG_FEP_ACTION_PENDING:    return "Pending";
        case BG_FEP_ACTION_EVALUATING: return "Evaluating";
        case BG_FEP_ACTION_SELECTED:   return "Selected";
        case BG_FEP_ACTION_EXECUTING:  return "Executing";
        case BG_FEP_ACTION_FEEDBACK:   return "Feedback";
        default: return "Unknown";
    }
}

const char* bg_fep_precision_mode_name(bg_fep_precision_mode_t mode) {
    switch (mode) {
        case BG_FEP_PRECISION_FIXED:        return "Fixed";
        case BG_FEP_PRECISION_DOPAMINE:     return "Dopamine";
        case BG_FEP_PRECISION_ADAPTIVE:     return "Adaptive";
        case BG_FEP_PRECISION_HIERARCHICAL: return "Hierarchical";
        default: return "Unknown";
    }
}

const char* bg_fep_confidence_name(bg_fep_confidence_t confidence) {
    switch (confidence) {
        case BG_FEP_CONFIDENCE_LOW:    return "Low";
        case BG_FEP_CONFIDENCE_MEDIUM: return "Medium";
        case BG_FEP_CONFIDENCE_HIGH:   return "High";
        default: return "Unknown";
    }
}
