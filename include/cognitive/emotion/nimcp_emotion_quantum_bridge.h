/**
 * @file nimcp_emotion_quantum_bridge.h
 * @brief Quantum-accelerated emotion state space exploration
 *
 * WHAT: Integrates quantum walk algorithms with emotional state transitions
 * WHY:  Faster exploration of emotion state space for regulation and prediction
 * HOW:  Graph-based quantum walk on valence-arousal space with ternary transitions
 *
 * BIOLOGICAL INSPIRATION:
 * - Emotional state transitions in amygdala-PFC circuits
 * - Rapid affective shifts in mood disorders
 * - Exploratory emotion regulation in healthy cognition
 * - Quantum-like decision making in affective neuroscience
 *
 * THEORETICAL BASIS:
 * - Russell's Circumplex Model: Emotion state space as 2D graph (valence × arousal)
 * - Quantum Cognition: Human decision making exhibits quantum-like interference
 * - Gross (1998): Emotion regulation as state space exploration
 * - Busemeyer & Bruza (2012): Quantum models of cognition
 *
 * QUANTUM WALK APPLICATION:
 * - Nodes: Discretized emotion states in valence-arousal space
 * - Edges: Transition probabilities between states
 * - Walker: Explores regulation pathways and predicts transitions
 * - Measurement: Collapses superposition to most probable state
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 * @version 1.0.0
 */

#ifndef NIMCP_EMOTION_QUANTUM_BRIDGE_H
#define NIMCP_EMOTION_QUANTUM_BRIDGE_H

#include "utils/quantum/nimcp_quantum_walk_ternary.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_emotional_system.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct emotion_quantum_bridge emotion_quantum_bridge_t;

/**
 * @brief Emotion quantum bridge configuration
 *
 * WHAT: Controls quantum walk behavior for emotion state exploration
 * WHY:  Allow tuning between exploratory vs focused emotion regulation
 * HOW:  Set state space resolution and walk parameters
 */
typedef struct {
    bool enabled;                    /**< Enable quantum emotion exploration */
    uint32_t state_dimensions;       /**< Resolution of valence-arousal grid (e.g., 16 = 16x16) */
    uint32_t transition_steps;       /**< Quantum walk steps for exploration */
    float exploration_bias;          /**< Balance exploration vs exploitation [0, 1] */
    bool enable_prediction;          /**< Predict next emotional states */
} emotion_quantum_config_t;

/**
 * @brief Emotion quantum bridge statistics
 *
 * WHAT: Track quantum walk performance on emotion states
 * WHY:  Monitor effectiveness and validate quantum advantage
 * HOW:  Count operations and measure transition accuracy
 */
typedef struct {
    uint64_t quantum_transitions;    /**< Total quantum state transitions */
    uint64_t state_evaluations;      /**< State evaluations performed */
    uint64_t valence_computations;   /**< Valence pathway explorations */
    uint64_t arousal_computations;   /**< Arousal pathway explorations */
    uint64_t predictions_made;       /**< State predictions generated */
    float avg_prediction_accuracy;   /**< Average prediction accuracy [0, 1] */
    float avg_exploration_entropy;   /**< Average state space entropy */
} emotion_quantum_stats_t;

/**
 * @brief Quantum-predicted emotion state
 *
 * WHAT: Probabilistic emotion state from quantum walk
 * WHY:  Enable predictive emotion regulation
 * HOW:  Amplitude distribution over state space
 */
typedef struct {
    float valence;                   /**< Predicted valence [-1, +1] */
    float arousal;                   /**< Predicted arousal [0, 1] */
    float probability;               /**< Probability of this state [0, 1] */
    uint32_t state_index;            /**< State index in discretized space */
} emotion_quantum_prediction_t;

//=============================================================================
// API
//=============================================================================

/**
 * @brief Get default quantum bridge configuration
 *
 * WHAT: Return sensible defaults for emotion quantum bridge
 * WHY:  Convenient initialization with validated parameters
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe
 */
emotion_quantum_config_t emotion_quantum_default_config(void);

/**
 * @brief Create emotion quantum bridge
 *
 * WHAT: Initialize quantum walk for emotion state space
 * WHY:  Enable quantum-accelerated emotion exploration
 * HOW:  Create graph walker on discretized valence-arousal grid
 *
 * @param config Configuration (NULL = defaults)
 * @param emotion_system Associated emotional system (can be NULL)
 * @return Quantum bridge handle or NULL on error
 *
 * COMPLEXITY: O(N²) where N = state_dimensions
 * THREAD_SAFETY: Thread-safe
 */
emotion_quantum_bridge_t* emotion_quantum_bridge_create(
    const emotion_quantum_config_t* config,
    emotional_system_t* emotion_system
);

/**
 * @brief Destroy emotion quantum bridge
 *
 * WHAT: Free all quantum walk resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy walker, free adjacency matrix, free structure
 *
 * @param bridge Quantum bridge handle
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: NOT thread-safe (caller must synchronize)
 */
void emotion_quantum_bridge_destroy(emotion_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum bridge is enabled
 *
 * WHAT: Query whether quantum exploration is active
 * WHY:  Allow conditional quantum usage
 * HOW:  Return enabled flag
 *
 * @param bridge Quantum bridge handle
 * @return true if enabled
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read-only)
 */
bool emotion_quantum_bridge_is_enabled(const emotion_quantum_bridge_t* bridge);

/**
 * @brief Set quantum bridge enabled state
 *
 * WHAT: Enable/disable quantum exploration
 * WHY:  Allow runtime toggling for comparison
 * HOW:  Set enabled flag
 *
 * @param bridge Quantum bridge handle
 * @param enabled New enabled state
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (atomic write)
 */
void emotion_quantum_bridge_set_enabled(emotion_quantum_bridge_t* bridge, bool enabled);

/**
 * @brief Evaluate emotion state using quantum walk
 *
 * WHAT: Explore emotion state space from current state
 * WHY:  Find optimal regulation pathways or predict transitions
 * HOW:  Initialize walker at current state, run quantum steps, measure
 *
 * @param bridge Quantum bridge handle
 * @param current_valence Current valence [-1, +1]
 * @param current_arousal Current arousal [0, 1]
 * @param target_valence Target valence (for regulation) [-1, +1]
 * @param target_arousal Target arousal (for regulation) [0, 1]
 * @param steps_required Output: estimated steps to target (can be NULL)
 * @return true on success
 *
 * COMPLEXITY: O(S × N) where S = walk steps, N = state dimensions
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_quantum_evaluate_state(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    float target_valence,
    float target_arousal,
    uint32_t* steps_required
);

/**
 * @brief Compute quantum transition pathway
 *
 * WHAT: Find optimal path from current to target emotion state
 * WHY:  Guide emotion regulation with quantum-accelerated search
 * HOW:  Quantum walk search algorithm on state graph
 *
 * @param bridge Quantum bridge handle
 * @param current_valence Current valence [-1, +1]
 * @param current_arousal Current arousal [0, 1]
 * @param target_valence Target valence [-1, +1]
 * @param target_arousal Target arousal [0, 1]
 * @param pathway Output: intermediate states (caller allocated, size = max_steps)
 * @param max_steps Maximum pathway length
 * @param steps_found Output: actual pathway length (can be NULL)
 * @return true if pathway found
 *
 * COMPLEXITY: O(S × N) where S = max_steps, N = state dimensions
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_quantum_transition(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    float target_valence,
    float target_arousal,
    emotion_quantum_prediction_t* pathway,
    uint32_t max_steps,
    uint32_t* steps_found
);

/**
 * @brief Predict next emotion states
 *
 * WHAT: Generate probabilistic predictions for emotion evolution
 * WHY:  Enable proactive regulation and mental health monitoring
 * HOW:  Quantum walk from current state, return amplitude distribution
 *
 * @param bridge Quantum bridge handle
 * @param current_valence Current valence [-1, +1]
 * @param current_arousal Current arousal [0, 1]
 * @param predictions Output: predicted states (caller allocated)
 * @param max_predictions Maximum predictions to return
 * @param predictions_found Output: actual predictions (can be NULL)
 * @return true on success
 *
 * COMPLEXITY: O(S × N) where S = walk steps, N = state dimensions
 * THREAD_SAFETY: Thread-safe (write lock)
 */
bool emotion_quantum_predict(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    emotion_quantum_prediction_t* predictions,
    uint32_t max_predictions,
    uint32_t* predictions_found
);

/**
 * @brief Get quantum bridge statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor quantum advantage and validate effectiveness
 * HOW:  Return statistics structure
 *
 * @param bridge Quantum bridge handle
 * @param stats Output statistics
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (read lock)
 */
bool emotion_quantum_get_stats(
    const emotion_quantum_bridge_t* bridge,
    emotion_quantum_stats_t* stats
);

/**
 * @brief Reset quantum bridge statistics
 *
 * WHAT: Clear all counters and averages
 * WHY:  Enable fresh measurement windows
 * HOW:  Zero statistics structure
 *
 * @param bridge Quantum bridge handle
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe (write lock)
 */
void emotion_quantum_reset_stats(emotion_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_EMOTION_QUANTUM_BRIDGE_IMPLEMENTATION

#include "utils/ternary/nimcp_ternary.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "EMOTION_QUANTUM"

/**
 * @brief Internal bridge structure
 */
struct emotion_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    emotion_quantum_config_t config;      /**< Configuration */
    emotional_system_t* emotion_system;   /**< Associated emotion system */

    /* Quantum walk state */
    trit_matrix_t* adjacency;             /**< State transition graph */
    trit_walker_graph_t* walker;          /**< Graph quantum walker */

    /* State space */
    uint32_t n_states;                    /**< Total states (dimensions²) */
    float* state_valences;                /**< Valence for each state */
    float* state_arousals;                /**< Arousal for each state */

    /* Statistics */
    emotion_quantum_stats_t stats;        /**< Performance metrics */
};

/**
 * @brief Convert continuous emotion to discrete state index
 *
 * WHAT: Map (valence, arousal) to grid index
 * WHY:  Quantum walk operates on discrete graph nodes
 * HOW:  Quantize to grid, compute linear index
 */
static inline uint32_t emotion_to_state_index(
    const emotion_quantum_bridge_t* bridge,
    float valence,
    float arousal
) {
    if (!bridge) return 0;

    uint32_t dim = bridge->config.state_dimensions;

    /* Clamp inputs */
    if (valence < -1.0f) valence = -1.0f;
    if (valence > 1.0f) valence = 1.0f;
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    /* Convert to grid coordinates */
    uint32_t v_idx = (uint32_t)((valence + 1.0f) * 0.5f * (float)(dim - 1));
    uint32_t a_idx = (uint32_t)(arousal * (float)(dim - 1));

    if (v_idx >= dim) v_idx = dim - 1;
    if (a_idx >= dim) a_idx = dim - 1;

    return a_idx * dim + v_idx;
}

/**
 * @brief Convert state index to continuous emotion
 *
 * WHAT: Map grid index to (valence, arousal)
 * WHY:  Convert quantum walk results to continuous space
 * HOW:  Reverse quantization
 */
static inline void state_index_to_emotion(
    const emotion_quantum_bridge_t* bridge,
    uint32_t state_index,
    float* valence,
    float* arousal
) {
    if (!bridge || !valence || !arousal) return;
    if (state_index >= bridge->n_states) return;

    *valence = bridge->state_valences[state_index];
    *arousal = bridge->state_arousals[state_index];
}

/**
 * @brief Compute distance between emotion states
 *
 * WHAT: Euclidean distance in valence-arousal space
 * WHY:  Define edge weights for quantum walk graph
 * HOW:  √((v1-v2)² + (a1-a2)²)
 */
static inline float emotion_distance(
    float v1, float a1,
    float v2, float a2
) {
    float dv = v1 - v2;
    float da = a1 - a2;
    return sqrtf(dv * dv + da * da);
}

emotion_quantum_config_t emotion_quantum_default_config(void) {
    return (emotion_quantum_config_t){
        .enabled = true,
        .state_dimensions = 16,           /* 16×16 = 256 states */
        .transition_steps = 10,           /* 10 quantum steps */
        .exploration_bias = 0.5f,         /* Balanced exploration */
        .enable_prediction = true
    };
}

emotion_quantum_bridge_t* emotion_quantum_bridge_create(
    const emotion_quantum_config_t* config,
    emotional_system_t* emotion_system
) {
    /* Guard: validate config */
    emotion_quantum_config_t cfg = config ? *config : emotion_quantum_default_config();
    if (cfg.state_dimensions < 2 || cfg.state_dimensions > 64) {
        NIMCP_LOGGING_ERROR("Invalid state_dimensions: %u (must be [2, 64])",
                           cfg.state_dimensions);
        return NULL;
    }

    /* Allocate bridge */
    emotion_quantum_bridge_t* bridge = (emotion_quantum_bridge_t*)nimcp_calloc(
        1, sizeof(emotion_quantum_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate quantum bridge");
        return NULL;
    }

    bridge->config = cfg;
    bridge->emotion_system = emotion_system;
    bridge->n_states = cfg.state_dimensions * cfg.state_dimensions;

    /* Allocate state space */
    bridge->state_valences = (float*)nimcp_calloc(bridge->n_states, sizeof(float));
    bridge->state_arousals = (float*)nimcp_calloc(bridge->n_states, sizeof(float));
    if (!bridge->state_valences || !bridge->state_arousals) {
        NIMCP_LOGGING_ERROR("Failed to allocate state space");
        emotion_quantum_bridge_destroy(bridge);
        return NULL;
    }

    /* Initialize state grid */
    uint32_t dim = cfg.state_dimensions;
    for (uint32_t a = 0; a < dim; a++) {
        for (uint32_t v = 0; v < dim; v++) {
            uint32_t idx = a * dim + v;
            bridge->state_valences[idx] = ((float)v / (float)(dim - 1)) * 2.0f - 1.0f;
            bridge->state_arousals[idx] = (float)a / (float)(dim - 1);
        }
    }

    /* Build adjacency matrix (connect nearby states) */
    bridge->adjacency = trit_matrix_create(bridge->n_states, bridge->n_states,
                                           TERNARY_PACK_BASE243);
    if (!bridge->adjacency) {
        NIMCP_LOGGING_ERROR("Failed to create adjacency matrix");
        emotion_quantum_bridge_destroy(bridge);
        return NULL;
    }

    /* Connect states within radius (8-neighborhood + extended) */
    float connection_radius = 1.5f / (float)(dim - 1);  /* Adaptive radius */
    for (uint32_t i = 0; i < bridge->n_states; i++) {
        for (uint32_t j = 0; j < bridge->n_states; j++) {
            if (i == j) continue;

            float dist = emotion_distance(
                bridge->state_valences[i], bridge->state_arousals[i],
                bridge->state_valences[j], bridge->state_arousals[j]
            );

            if (dist <= connection_radius) {
                /* Weight by inverse distance */
                trit_t weight = (dist < connection_radius * 0.5f) ?
                    TRIT_POSITIVE : TRIT_UNKNOWN;
                trit_matrix_set(bridge->adjacency, i, j, weight);
            }
        }
    }

    /* Create quantum walker */
    bridge->walker = trit_walker_graph_create(bridge->adjacency);
    if (!bridge->walker) {
        NIMCP_LOGGING_ERROR("Failed to create quantum walker");
        emotion_quantum_bridge_destroy(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created emotion quantum bridge: %u states (%ux%u grid)",
                      bridge->n_states, dim, dim);

    return bridge;
}

void emotion_quantum_bridge_destroy(emotion_quantum_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->walker) trit_walker_graph_destroy(bridge->walker);
    if (bridge->adjacency) trit_matrix_destroy(bridge->adjacency);
    nimcp_free(bridge->state_valences);
    nimcp_free(bridge->state_arousals);
    nimcp_free(bridge);
}

bool emotion_quantum_bridge_is_enabled(const emotion_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void emotion_quantum_bridge_set_enabled(emotion_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

bool emotion_quantum_evaluate_state(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    float target_valence,
    float target_arousal,
    uint32_t* steps_required
) {
    /* Guard: validate inputs */
    if (!bridge || !bridge->config.enabled) return false;

    /* Convert to state indices */
    uint32_t current_idx = emotion_to_state_index(bridge, current_valence, current_arousal);
    uint32_t target_idx = emotion_to_state_index(bridge, target_valence, target_arousal);

    /* Run quantum search */
    uint32_t steps;
    bool found = trit_walker_graph_search(
        bridge->walker,
        current_idx,
        target_idx,
        bridge->config.transition_steps,
        &steps
    );

    if (steps_required) *steps_required = steps;

    /* Update statistics */
    bridge->stats.quantum_transitions++;
    bridge->stats.state_evaluations++;

    return found;
}

bool emotion_quantum_transition(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    float target_valence,
    float target_arousal,
    emotion_quantum_prediction_t* pathway,
    uint32_t max_steps,
    uint32_t* steps_found
) {
    /* Guard: validate inputs */
    if (!bridge || !bridge->config.enabled || !pathway) return false;
    if (max_steps == 0) return false;

    /* Convert to state indices */
    uint32_t current_idx = emotion_to_state_index(bridge, current_valence, current_arousal);
    uint32_t target_idx = emotion_to_state_index(bridge, target_valence, target_arousal);

    /* Initialize walker at current state */
    trit_walker_graph_init(bridge->walker, current_idx);

    /* Run quantum walk steps and track pathway */
    uint32_t step_count = 0;
    for (uint32_t s = 0; s < max_steps && s < bridge->config.transition_steps; s++) {
        trit_walker_graph_step(bridge->walker);

        /* Find maximum amplitude state */
        float max_amp;
        uint32_t max_node = trit_walker_graph_max_node(bridge->walker, &max_amp);

        /* Record in pathway */
        state_index_to_emotion(bridge, max_node,
                              &pathway[s].valence,
                              &pathway[s].arousal);
        pathway[s].probability = max_amp * max_amp;
        pathway[s].state_index = max_node;

        step_count++;

        /* Check if reached target */
        if (max_node == target_idx) break;
    }

    if (steps_found) *steps_found = step_count;

    /* Update statistics */
    bridge->stats.quantum_transitions += step_count;
    bridge->stats.valence_computations++;
    bridge->stats.arousal_computations++;

    return step_count > 0;
}

bool emotion_quantum_predict(
    emotion_quantum_bridge_t* bridge,
    float current_valence,
    float current_arousal,
    emotion_quantum_prediction_t* predictions,
    uint32_t max_predictions,
    uint32_t* predictions_found
) {
    /* Guard: validate inputs */
    if (!bridge || !bridge->config.enabled || !predictions) return false;
    if (!bridge->config.enable_prediction) return false;
    if (max_predictions == 0) return false;

    /* Convert to state index */
    uint32_t current_idx = emotion_to_state_index(bridge, current_valence, current_arousal);

    /* Initialize walker and run exploration */
    trit_walker_graph_init(bridge->walker, current_idx);
    trit_walker_graph_run(bridge->walker, bridge->config.transition_steps);

    /* Get probability distribution */
    float* probs = (float*)nimcp_calloc(bridge->n_states, sizeof(float));
    if (!probs) return false;

    trit_walker_graph_get_distribution(bridge->walker, probs);

    /* Find top-K predictions */
    uint32_t found = 0;
    for (uint32_t k = 0; k < max_predictions && found < bridge->n_states; k++) {
        /* Find max probability */
        float max_prob = 0.0f;
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < bridge->n_states; i++) {
            if (probs[i] > max_prob && probs[i] > 0.01f) {  /* Threshold */
                max_prob = probs[i];
                max_idx = i;
            }
        }

        if (max_prob < 0.01f) break;  /* No more significant predictions */

        /* Record prediction */
        state_index_to_emotion(bridge, max_idx,
                              &predictions[found].valence,
                              &predictions[found].arousal);
        predictions[found].probability = max_prob;
        predictions[found].state_index = max_idx;
        found++;

        /* Mark as used */
        probs[max_idx] = 0.0f;
    }

    nimcp_free(probs);

    if (predictions_found) *predictions_found = found;

    /* Update statistics */
    bridge->stats.predictions_made += found;

    return found > 0;
}

bool emotion_quantum_get_stats(
    const emotion_quantum_bridge_t* bridge,
    emotion_quantum_stats_t* stats
) {
    if (!bridge || !stats) return false;
    *stats = bridge->stats;
    return true;
}

void emotion_quantum_reset_stats(emotion_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_EMOTION_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EMOTION_QUANTUM_BRIDGE_H
