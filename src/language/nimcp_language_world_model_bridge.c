/**
 * @file nimcp_language_world_model_bridge.c
 * @brief World model ↔ grounded language for counterfactual reasoning
 *
 * WHAT: Integrates world model latent dynamics with language for what-if scenarios
 * WHY:  Enables counterfactual generation, causal reasoning, grounded verification
 * HOW:  World model predicts state transitions → language describes outcomes
 *
 * World model language integration:
 *   - Counterfactual generation: "If X then Y" from predicted state transitions
 *   - Causal reasoning: action → state change → linguistic description
 *   - Grounded verification: symbolic logic facts verified against sensory evidence
 *   - Planning language: multi-step action sequences → narrative plans
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief World state representation for counterfactual reasoning
 */
typedef struct {
    float* latent_state;       /**< Latent world state vector */
    float* sensory_features;   /**< Observable sensory features */
    uint32_t latent_dim;       /**< Latent state dimensionality */
    uint32_t sensory_dim;      /**< Sensory feature dimensionality */
    float confidence;          /**< State estimate confidence */
} world_state_t;

/**
 * @brief Action representation for world model transitions
 */
typedef struct {
    char action_name[64];      /**< Action label (e.g., "move_left") */
    float* action_vector;      /**< Action embedding/parameters */
    uint32_t action_dim;       /**< Action dimensionality */
} world_action_t;

/**
 * @brief Counterfactual prediction result
 */
typedef struct {
    world_state_t* predicted_state;  /**< Predicted outcome state */
    float prediction_confidence;     /**< Prediction certainty */
    char* causal_explanation;        /**< Natural language explanation */
} counterfactual_result_t;

/**
 * @brief Get current world state from brain's world model
 *
 * @param brain      Brain instance with world model
 * @param out_state  Output world state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int world_model_get_current_state(brain_t brain, world_state_t* out_state) {
    if (!brain || !out_state) return -1;

    /* Placeholder: In actual implementation, query brain->world_model.
     * For now, use brain's last decision as proxy for world state. */

    if (!brain->last_decision || !brain->last_decision->output_vector) {
        return -1;
    }

    /* Allocate latent state from decision output */
    uint32_t dim = brain->last_decision->output_size;
    out_state->latent_state = nimcp_malloc(dim * sizeof(float));
    if (!out_state->latent_state) return -1;

    memcpy(out_state->latent_state, brain->last_decision->output_vector,
           dim * sizeof(float));
    out_state->latent_dim = dim;

    /* Sensory features from input (if available) */
    out_state->sensory_features = NULL;
    out_state->sensory_dim = 0;
    out_state->confidence = brain->last_decision->confidence;

    return 0;
}

/**
 * @brief Predict world state after action using world model
 *
 * Computes s' = T(s, a) where T is the learned transition function.
 *
 * @param brain          Brain with world model
 * @param current_state  Current world state
 * @param action         Action to apply
 * @param out_predicted  Output predicted state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int world_model_predict_transition(brain_t brain,
                                     const world_state_t* current_state,
                                     const world_action_t* action,
                                     world_state_t* out_predicted) {
    if (!brain || !current_state || !action || !out_predicted) return -1;

    /* Placeholder: Actual implementation queries brain->world_model->predict().
     * For now, simulate state transition with simple dynamics. */

    uint32_t dim = current_state->latent_dim;
    out_predicted->latent_state = nimcp_malloc(dim * sizeof(float));
    if (!out_predicted->latent_state) return -1;
    out_predicted->latent_dim = dim;

    /* Simple transition: s' = s + α*action + noise */
    const float alpha = 0.2f;
    const float noise_scale = 0.05f;

    for (uint32_t i = 0; i < dim; i++) {
        float action_effect = 0.0f;
        if (action->action_vector && i < action->action_dim) {
            action_effect = alpha * action->action_vector[i];
        }
        float noise = noise_scale * ((float)rand() / RAND_MAX - 0.5f);
        out_predicted->latent_state[i] = current_state->latent_state[i] +
                                          action_effect + noise;
    }

    out_predicted->sensory_features = NULL;
    out_predicted->sensory_dim = 0;
    out_predicted->confidence = current_state->confidence * 0.9f; /* Uncertainty increases */

    return 0;
}

/**
 * @brief Generate natural language description of state difference
 *
 * Describes how the world changed from state A to state B in natural language.
 *
 * @param gl          Grounded language instance
 * @param state_from  Initial state
 * @param state_to    Final state
 * @param action      Action that caused transition
 * @param out_text    Output text buffer (caller allocates)
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int world_model_describe_transition(grounded_language_t* gl,
                                      const world_state_t* state_from,
                                      const world_state_t* state_to,
                                      const world_action_t* action,
                                      char* out_text,
                                      size_t buffer_size) {
    if (!gl || !state_from || !state_to || !action || !out_text) return -1;

    /* Compute state difference vector */
    uint32_t dim = fminf(state_from->latent_dim, state_to->latent_dim);
    float* state_diff = nimcp_malloc(dim * sizeof(float));
    if (!state_diff) return -1;

    for (uint32_t i = 0; i < dim; i++) {
        state_diff[i] = state_to->latent_state[i] - state_from->latent_state[i];
    }

    /* Produce text from state difference using grounded language.
     * The difference vector represents semantic change caused by action. */
    gl_produce_result_t result;
    extern int grounded_language_produce_from_state(grounded_language_t* gl,
                                                      const float* state,
                                                      uint32_t dim,
                                                      gl_produce_result_t* out);

    int ret = grounded_language_produce_from_state(gl, state_diff, dim, &result);
    nimcp_free(state_diff);

    if (ret != 0) {
        snprintf(out_text, buffer_size, "If %s then [unknown outcome]",
                 action->action_name);
        return -1;
    }

    /* Format as counterfactual: "If <action> then <outcome>" */
    snprintf(out_text, buffer_size, "If %s then %s",
             action->action_name, result.text);

    NIMCP_LOGGING_DEBUG("World model transition: '%s'", out_text);
    return 0;
}

/**
 * @brief Verify symbolic logic fact against world model sensory evidence
 *
 * Grounds abstract logical facts in concrete sensory observations.
 * Example: is_a(ball, red) verified by checking visual cortex activations.
 *
 * @param brain  Brain with world model and symbolic logic
 * @param fact   Logical fact to verify
 * @param out_confidence  Output verification confidence [0,1]
 * @return 0 on success, -1 on error
 */
int world_model_verify_logical_fact(brain_t brain,
                                      logic_clause_t* fact,
                                      float* out_confidence) {
    if (!brain || !fact || !out_confidence) return -1;

    /* Get current sensory evidence from world model */
    world_state_t current_state;
    if (world_model_get_current_state(brain, &current_state) != 0) {
        *out_confidence = 0.0f;
        return -1;
    }

    /* Extract predicate from fact.
     * Example: is_a(ball, red) → predicate="is_a", subject="ball", object="red" */
    if (fact->num_literals == 0 || !fact->literals[0]) {
        *out_confidence = 0.0f;
        return -1;
    }

    atomic_formula_t* atom = fact->literals[0];
    const char* predicate = atom->name;

    /* Verification strategy depends on predicate type:
     *   - is_a(X, Y): Check if X's sensory features match Y's prototype
     *   - has_property(X, Y): Check if Y is active in X's feature vector
     *   - spatial relation: Check geometry in world model
     */

    /* Placeholder: Actual implementation queries visual cortex, etc.
     * For now, use simple heuristic based on state confidence. */

    float base_confidence = current_state.confidence;
    float predicate_match = 0.5f; /* Assume partial match */

    *out_confidence = base_confidence * predicate_match;

    NIMCP_LOGGING_DEBUG("World model verification: predicate='%s' confidence=%.3f",
                        predicate, *out_confidence);

    /* Free allocated state */
    if (current_state.latent_state) nimcp_free(current_state.latent_state);
    if (current_state.sensory_features) nimcp_free(current_state.sensory_features);

    return 0;
}

/**
 * @brief Generate counterfactual language for multiple hypothetical actions
 *
 * Produces "what-if" scenarios for planning and reasoning.
 *
 * @param brain         Brain with world model
 * @param gl            Grounded language instance
 * @param actions       Array of hypothetical actions
 * @param num_actions   Number of actions
 * @param out_scenarios Output array of counterfactual texts (caller allocates)
 * @param scenario_size Size of each scenario buffer
 * @return 0 on success, -1 on error
 */
int world_model_generate_counterfactuals(brain_t brain,
                                           grounded_language_t* gl,
                                           const world_action_t* actions,
                                           uint32_t num_actions,
                                           char** out_scenarios,
                                           size_t scenario_size) {
    if (!brain || !gl || !actions || !out_scenarios) return -1;

    /* Get current world state */
    world_state_t current_state;
    if (world_model_get_current_state(brain, &current_state) != 0) {
        return -1;
    }

    /* For each action, predict outcome and describe it */
    for (uint32_t i = 0; i < num_actions; i++) {
        world_state_t predicted_state;

        /* Predict state after action */
        if (world_model_predict_transition(brain, &current_state,
                                             &actions[i],
                                             &predicted_state) != 0) {
            snprintf(out_scenarios[i], scenario_size,
                     "If %s then [prediction failed]", actions[i].action_name);
            continue;
        }

        /* Describe transition in natural language */
        world_model_describe_transition(gl, &current_state, &predicted_state,
                                         &actions[i], out_scenarios[i],
                                         scenario_size);

        /* Free predicted state */
        if (predicted_state.latent_state) nimcp_free(predicted_state.latent_state);
        if (predicted_state.sensory_features) nimcp_free(predicted_state.sensory_features);
    }

    /* Free current state */
    if (current_state.latent_state) nimcp_free(current_state.latent_state);
    if (current_state.sensory_features) nimcp_free(current_state.sensory_features);

    return 0;
}

/**
 * @brief Integrate world model counterfactual reasoning with grounded language
 *
 * Enriches language production with world model predictions and causal reasoning.
 *
 * @param brain  Brain with world model
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_world_model(brain_t brain,
                                              grounded_language_t* gl,
                                              void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    /* Check if world model coupling is enabled */
    extern bool grounded_language_world_model_enabled(const grounded_language_t* gl);
    if (!grounded_language_world_model_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get current world state for grounding */
    world_state_t current_state;
    if (world_model_get_current_state(brain, &current_state) != 0) {
        return 0; /* World model not available */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Blend world model latent state into content_intent for grounding */
    const float w_world_model = 0.18f;
    uint32_t blend_dim = fminf(cs->dim, current_state.latent_dim);

    for (uint32_t i = 0; i < blend_dim; i++) {
        float world_feature = current_state.latent_state[i];
        if (isfinite(world_feature)) {
            cs->content_intent[i] += w_world_model * world_feature;
        }
    }

    /* Log world model integration */
    NIMCP_LOGGING_DEBUG("World model grounding: state_confidence=%.3f "
                        "blend_dim=%u weight=%.2f",
                        current_state.confidence, blend_dim, w_world_model);

    /* Free allocated state */
    if (current_state.latent_state) nimcp_free(current_state.latent_state);
    if (current_state.sensory_features) nimcp_free(current_state.sensory_features);

    return 0;
}

/**
 * @brief Query whether world model coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if world model integration is active
 */
bool grounded_language_world_model_enabled(const grounded_language_t* gl) {
    extern bool grounded_language_is_bridge_enabled(const grounded_language_t* gl,
                                                      uint32_t mask);
    return grounded_language_is_bridge_enabled(gl, (1u << 19)); /* GL_BRIDGE_WORLD_MODEL */
}
