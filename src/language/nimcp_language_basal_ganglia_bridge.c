/**
 * @file nimcp_language_basal_ganglia_bridge.c
 * @brief Basal ganglia ↔ grounded language for action selection and habits
 *
 * WHAT: Integrates action selection, habit formation, reward with language
 * WHY:  Enables goal-directed language, habitual phrases, action-oriented semantics
 * HOW:  BG state (action values, habits, rewards) → action/goal language
 *
 * Basal ganglia language functions:
 *   - Action selection: "I will do X" from striatal action values
 *   - Habit expression: learned phrase patterns from habitual loops
 *   - Goal-directed language: GPi-modulated goal semantics
 *   - Reward-based emphasis: dopamine-weighted semantic salience
 *   - Procedural knowledge: motor sequences → procedural language
 *
 * Biological substrate:
 *   - Striatum (caudate/putamen): action selection via competitive inhibition
 *   - GPe/GPi: direct/indirect pathway modulation
 *   - SNc: dopaminergic reward prediction error
 *   - STN: response inhibition, conflict resolution
 *   - Ventral striatum (NAcc): motivation, reward processing
 *
 * Integration with language:
 *   - Direct pathway: "go" signals → action-oriented language
 *   - Indirect pathway: "no-go" signals → inhibitory language
 *   - Habitual striatum: overlearned phrases, formulaic language
 *   - Goal-directed striatum: flexible, novel constructions
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief Action representation in basal ganglia
 */
typedef struct {
    char action_label[64];    /**< Action name */
    float action_value;       /**< Q-value or utility */
    float habit_strength;     /**< Habitual tendency [0,1] */
    float goal_directed;      /**< Goal-directed component [0,1] */
} bg_action_t;

/**
 * @brief Basal ganglia state for action selection
 */
typedef struct {
    bg_action_t* candidate_actions;  /**< Array of possible actions */
    uint32_t num_actions;            /**< Number of candidate actions */
    uint32_t selected_action_idx;    /**< Index of selected action */
    float selection_confidence;      /**< Confidence in selection */
    float dopamine_level;            /**< Current dopamine (reward signal) */
    float direct_pathway_activity;   /**< "Go" pathway strength */
    float indirect_pathway_activity; /**< "No-go" pathway strength */
} basal_ganglia_state_t;

/**
 * @brief Reward learning state
 */
typedef struct {
    float reward_prediction_error;   /**< RPE: δ = r - V(s) */
    float value_estimate;            /**< State value V(s) */
    float expected_reward;           /**< Expected reward */
    float actual_reward;             /**< Received reward */
} reward_learning_state_t;

/**
 * @brief Extract basal ganglia action selection state
 *
 * @param brain     Brain with basal ganglia
 * @param out_bg    Output BG state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int basal_ganglia_extract_state(brain_t brain,
                                  basal_ganglia_state_t* out_bg) {
    if (!brain || !out_bg) return -1;

    /* Query basal ganglia adapter if available.
     * Actual implementation: brain->basal_ganglia->get_action_values().
     * For now, simulate action selection. */

    /* Create candidate actions */
    const uint32_t num_actions = 4;
    out_bg->candidate_actions = nimcp_malloc(num_actions * sizeof(bg_action_t));
    if (!out_bg->candidate_actions) return -1;
    out_bg->num_actions = num_actions;

    /* Populate action values (placeholder) */
    const char* action_names[] = {"speak", "move", "observe", "wait"};
    for (uint32_t i = 0; i < num_actions; i++) {
        strncpy(out_bg->candidate_actions[i].action_label, action_names[i], 63);
        out_bg->candidate_actions[i].action_value = ((float)rand() / RAND_MAX);
        out_bg->candidate_actions[i].habit_strength = 0.2f + 0.3f * ((float)rand() / RAND_MAX);
        out_bg->candidate_actions[i].goal_directed = 1.0f - out_bg->candidate_actions[i].habit_strength;
    }

    /* Select action via winner-take-all (argmax Q-value) */
    float max_value = -INFINITY;
    out_bg->selected_action_idx = 0;
    for (uint32_t i = 0; i < num_actions; i++) {
        if (out_bg->candidate_actions[i].action_value > max_value) {
            max_value = out_bg->candidate_actions[i].action_value;
            out_bg->selected_action_idx = i;
        }
    }
    out_bg->selection_confidence = max_value;

    /* Dopamine level from recent reward */
    out_bg->dopamine_level = 0.5f + 0.3f * ((float)rand() / RAND_MAX - 0.5f);

    /* Direct/indirect pathway balance */
    out_bg->direct_pathway_activity = 0.6f;   /* "Go" signal */
    out_bg->indirect_pathway_activity = 0.4f; /* "No-go" signal */

    NIMCP_LOGGING_DEBUG("Basal ganglia: selected='%s' confidence=%.3f "
                        "dopamine=%.3f direct=%.3f indirect=%.3f",
                        out_bg->candidate_actions[out_bg->selected_action_idx].action_label,
                        out_bg->selection_confidence, out_bg->dopamine_level,
                        out_bg->direct_pathway_activity,
                        out_bg->indirect_pathway_activity);

    return 0;
}

/**
 * @brief Extract reward learning signals
 *
 * @param brain        Brain with reward system
 * @param out_reward   Output reward learning state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int basal_ganglia_extract_reward_signals(brain_t brain,
                                           reward_learning_state_t* out_reward) {
    if (!brain || !out_reward) return -1;

    /* Query reward learning state from brain.
     * Actual implementation: brain->reward_system->get_rpe().
     * For now, simulate reward prediction error. */

    out_reward->value_estimate = 0.5f;
    out_reward->expected_reward = 0.4f;
    out_reward->actual_reward = 0.6f;
    out_reward->reward_prediction_error = out_reward->actual_reward -
                                           out_reward->expected_reward;

    NIMCP_LOGGING_DEBUG("Reward learning: RPE=%.3f V(s)=%.3f expected=%.3f actual=%.3f",
                        out_reward->reward_prediction_error,
                        out_reward->value_estimate,
                        out_reward->expected_reward,
                        out_reward->actual_reward);

    return 0;
}

/**
 * @brief Convert action selection to goal-directed language
 *
 * Generates language expressing selected action and goals.
 *
 * @param bg_state    Basal ganglia state with selected action
 * @param out_text    Output action-oriented utterance (caller allocates buffer)
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int basal_ganglia_action_to_language(const basal_ganglia_state_t* bg_state,
                                       char* out_text,
                                       size_t buffer_size) {
    if (!bg_state || !out_text) return -1;

    const bg_action_t* selected = &bg_state->candidate_actions[bg_state->selected_action_idx];

    /* Generate language based on action type and confidence */
    if (bg_state->selection_confidence > 0.7f) {
        snprintf(out_text, buffer_size, "I will %s", selected->action_label);
    } else if (bg_state->selection_confidence > 0.4f) {
        snprintf(out_text, buffer_size, "I might %s", selected->action_label);
    } else {
        snprintf(out_text, buffer_size, "I am uncertain about %s", selected->action_label);
    }
    out_text[buffer_size - 1] = '\0';

    /* Habit vs goal-directed markers */
    if (selected->habit_strength > 0.7f) {
        /* Habitual action: formulaic language */
        strncat(out_text, " (habitual)", buffer_size - strlen(out_text) - 1);
    } else if (selected->goal_directed > 0.7f) {
        /* Goal-directed: deliberate language */
        strncat(out_text, " (deliberate)", buffer_size - strlen(out_text) - 1);
    }

    return 0;
}

/**
 * @brief Integrate basal ganglia action/reward signals into grounded language
 *
 * Modulates language production based on action selection and reward learning.
 *
 * @param brain  Brain with basal ganglia
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_basal_ganglia(brain_t brain,
                                                grounded_language_t* gl,
                                                void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    /* Check if basal ganglia coupling is enabled */
    extern bool grounded_language_basal_ganglia_enabled(const grounded_language_t* gl);
    if (!grounded_language_basal_ganglia_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Extract basal ganglia state */
    basal_ganglia_state_t bg_state = {0};
    if (basal_ganglia_extract_state(brain, &bg_state) != 0) {
        return -1;
    }

    /* Extract reward signals */
    reward_learning_state_t reward;
    if (basal_ganglia_extract_reward_signals(brain, &reward) != 0) {
        if (bg_state.candidate_actions) nimcp_free(bg_state.candidate_actions);
        return -1;
    }

    /* Blend action values into content_intent (dims 420-440) */
    const float w_action = 0.21f;
    /* C7 FIX: Move selected declaration outside conditional so it's in scope
     * for NIMCP_LOGGING_DEBUG below. */
    bg_action_t* selected = &bg_state.candidate_actions[bg_state.selected_action_idx];

    if (cs->dim > 430) {
        /* Selected action value */
        cs->content_intent[420] += w_action * bg_state.selection_confidence;

        /* Direct/indirect pathway balance */
        cs->content_intent[421] += w_action * bg_state.direct_pathway_activity;
        cs->content_intent[422] += w_action * bg_state.indirect_pathway_activity;

        /* Dopamine-weighted salience */
        cs->content_intent[423] += w_action * bg_state.dopamine_level;

        /* Reward prediction error (RPE) */
        cs->content_intent[424] += w_action * reward.reward_prediction_error;

        /* Habit vs goal-directed */
        cs->content_intent[425] += w_action * selected->habit_strength;
        cs->content_intent[426] += w_action * selected->goal_directed;
    }

    /* Dopamine modulation: positive RPE amplifies semantic salience */
    if (reward.reward_prediction_error > 0.1f) {
        float dopamine_boost = 1.0f + 0.5f * reward.reward_prediction_error;
        for (uint32_t i = 0; i < cs->dim; i++) {
            cs->content_intent[i] *= dopamine_boost;
        }
    }

    NIMCP_LOGGING_DEBUG("Basal ganglia integration: action_value=%.3f dopamine=%.3f "
                        "RPE=%.3f habit=%.3f goal_directed=%.3f",
                        bg_state.selection_confidence, bg_state.dopamine_level,
                        reward.reward_prediction_error, selected->habit_strength,
                        selected->goal_directed);

    /* Free allocated actions */
    if (bg_state.candidate_actions) nimcp_free(bg_state.candidate_actions);

    return 0;
}

/**
 * @brief Query whether basal ganglia coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if basal ganglia integration is active
 */
bool grounded_language_basal_ganglia_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    /* For now, always enabled when basal ganglia is present.
     * Future: add runtime flag to grounded_language struct. */
    return true;
}
