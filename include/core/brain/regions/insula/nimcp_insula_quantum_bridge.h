/**
 * @file nimcp_insula_quantum_bridge.h
 * @brief Quantum-inspired interoceptive and emotional processing
 *
 * WHAT: Integrates quantum algorithms with Insula region
 * WHY: Enable superposition-based body signal integration and emotional state evaluation
 * HOW: Quantum superposition for multi-channel interoception, Grover search for somatic markers
 *
 * BIOLOGICAL INSPIRATION:
 * - Insula integrates multiple body signals simultaneously
 * - Emotional states exist in superposition until conscious awareness
 * - Gut feelings emerge from parallel evaluation of somatic markers
 * - Social emotion assessment evaluates multiple hypotheses in parallel
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Multiple body states evaluated simultaneously
 * - Interference: Body signals reinforce or cancel emotional hypotheses
 * - Grover search: O(sqrt(N)) somatic marker retrieval
 * - Amplitude amplification: Boost high-confidence emotional states
 * - Quantum error correction: Handle noisy interoceptive signals
 *
 * APPLICATIONS:
 * - Interoceptive integration: Fuse multi-channel body signals
 * - Emotional state evaluation: Superposition of emotion hypotheses
 * - Somatic marker search: Fast gut-feeling retrieval
 * - Social emotion disambiguation: Parallel social cue evaluation
 * - Homeostatic optimization: Find optimal body state
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_INSULA_QUANTUM_BRIDGE_H
#define NIMCP_INSULA_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * TYPES
 *===========================================================================*/

typedef struct insula_quantum_bridge insula_quantum_bridge_t;

/**
 * @brief Quantum Insula configuration
 */
typedef struct {
    bool enabled;                        /**< Enable quantum optimization */
    uint32_t intero_channels;            /**< Number of interoceptive channels (default: 16) */
    uint32_t emotion_superposition_size; /**< Max emotional states in superposition (default: 8) */
    uint32_t max_grover_iterations;      /**< Max Grover iterations (default: 10) */
    float min_confidence_threshold;      /**< Min confidence for collapse (default: 0.5) */
    bool enable_interference;            /**< Enable quantum interference (default: true) */
    bool use_superposition;              /**< Use superposition for evaluation (default: true) */
    float noise_tolerance;               /**< Tolerance for noisy signals (default: 0.1) */
    uint32_t seed;                       /**< Random seed (default: 42) */
} insula_quantum_config_t;

/**
 * @brief Interoceptive channel quantum state
 */
typedef struct {
    uint32_t channel_id;                 /**< Channel identifier */
    float amplitude_real;                /**< Real component of amplitude */
    float amplitude_imag;                /**< Imaginary component */
    float signal_value;                  /**< Classical signal value */
    float confidence;                    /**< Measurement confidence */
    bool collapsed;                      /**< Has state collapsed? */
} quantum_intero_state_t;

/**
 * @brief Quantum interoceptive integration result
 */
typedef struct {
    float* integrated_state;             /**< Integrated body state vector */
    uint32_t state_dim;                  /**< Dimension of state vector */
    float integration_quality;           /**< Quality of integration [0, 1] */
    float coherence;                     /**< Quantum coherence measure */
    uint32_t channels_fused;             /**< Number of channels fused */
    float speedup;                       /**< Speedup vs classical */
} quantum_intero_result_t;

/**
 * @brief Emotional state hypothesis
 */
typedef struct {
    uint32_t hypothesis_id;              /**< Hypothesis identifier */
    char emotion_label[32];              /**< Emotion name */
    float valence;                       /**< Valence [-1, 1] */
    float arousal;                       /**< Arousal [-1, 1] */
    float amplitude;                     /**< Quantum amplitude [0, 1] */
    float probability;                   /**< Measurement probability */
} quantum_emotion_hypothesis_t;

/**
 * @brief Quantum emotional evaluation result
 */
typedef struct {
    quantum_emotion_hypothesis_t* best_emotion; /**< Most likely emotion */
    uint32_t hypotheses_evaluated;              /**< Total hypotheses */
    float satisfaction_probability;              /**< Evaluation success */
    float emotional_clarity;                     /**< Clarity of result */
    uint32_t grover_iterations_used;            /**< Grover iterations */
} quantum_emotion_result_t;

/**
 * @brief Somatic marker quantum entry
 */
typedef struct {
    uint32_t context_id;                 /**< Decision context */
    float amplitude;                     /**< Quantum amplitude */
    float valence;                       /**< Good/bad association */
    float confidence;                    /**< Marker confidence */
    float retrieval_time;                /**< Time to retrieve */
} quantum_somatic_marker_t;

/**
 * @brief Quantum somatic marker search result
 */
typedef struct {
    quantum_somatic_marker_t* best_marker; /**< Best matching marker */
    uint32_t markers_evaluated;            /**< Total markers searched */
    float search_speedup;                  /**< Speedup vs linear */
    float satisfaction_probability;         /**< Search success */
} quantum_somatic_result_t;

/**
 * @brief Statistics for quantum Insula operations
 */
typedef struct {
    uint64_t intero_integrations;        /**< Total interoceptive integrations */
    uint64_t emotion_evaluations;        /**< Total emotion evaluations */
    uint64_t somatic_searches;           /**< Total somatic marker searches */
    float avg_intero_speedup;            /**< Average interoceptive speedup */
    float avg_emotion_clarity;           /**< Average emotional clarity */
    float avg_somatic_speedup;           /**< Average somatic search speedup */
    uint64_t successful_collapses;       /**< Successful state collapses */
    uint64_t decoherence_events;         /**< Decoherence (noise) events */
} insula_quantum_stats_t;

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get default quantum Insula configuration
 * @return Default configuration
 */
insula_quantum_config_t insula_quantum_default_config(void);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Create quantum Insula bridge
 * @param insula Insula adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
insula_quantum_bridge_t* insula_quantum_bridge_create(
    void* insula,
    const insula_quantum_config_t* config
);

/**
 * @brief Destroy quantum Insula bridge
 * @param bridge Bridge to destroy
 */
void insula_quantum_bridge_destroy(insula_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool insula_quantum_bridge_is_enabled(const insula_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void insula_quantum_bridge_set_enabled(insula_quantum_bridge_t* bridge, bool enabled);

/*=============================================================================
 * INTEROCEPTIVE INTEGRATION API
 *===========================================================================*/

/**
 * @brief Initialize interoceptive quantum state
 *
 * WHAT: Prepare quantum superposition of body signal channels
 * WHY:  Enable parallel integration of multiple interoceptive signals
 * HOW:  Create superposition with equal amplitudes for all channels
 *
 * @param bridge Quantum bridge
 * @param channels Array of channel signal values
 * @param num_channels Number of channels
 * @return 0 on success, -1 on error
 */
int insula_quantum_init_interoception(
    insula_quantum_bridge_t* bridge,
    const float* channels,
    uint32_t num_channels
);

/**
 * @brief Integrate interoceptive signals using quantum fusion
 *
 * WHAT: Fuse multiple body signal channels into unified state
 * WHY:  Leverage quantum parallelism for multi-channel integration
 * HOW:  Apply quantum gates to superposition, then measure
 *
 * @param bridge Quantum bridge
 * @param result Output: integration result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) classical fusion
 */
int insula_quantum_integrate_interoception(
    insula_quantum_bridge_t* bridge,
    quantum_intero_result_t* result
);

/**
 * @brief Apply noise correction to interoceptive signals
 *
 * WHAT: Remove noise from body signals using quantum error correction
 * WHY:  Interoceptive signals are inherently noisy
 * HOW:  Apply quantum error correction codes
 *
 * @param bridge Quantum bridge
 * @param noisy_signals Input noisy signals
 * @param corrected_signals Output corrected signals
 * @param num_signals Number of signals
 * @return 0 on success, -1 on error
 */
int insula_quantum_correct_intero_noise(
    insula_quantum_bridge_t* bridge,
    const float* noisy_signals,
    float* corrected_signals,
    uint32_t num_signals
);

/*=============================================================================
 * EMOTIONAL EVALUATION API
 *===========================================================================*/

/**
 * @brief Evaluate emotional state using quantum superposition
 *
 * WHAT: Evaluate multiple emotion hypotheses simultaneously
 * WHY:  Emotional states often ambiguous, benefit from parallel evaluation
 * HOW:  Create superposition of emotion hypotheses, apply constraints, measure
 *
 * @param bridge Quantum bridge
 * @param body_state Current body state vector
 * @param body_state_dim Body state dimension
 * @param result Output: evaluation result
 * @return 0 on success, -1 on error
 */
int insula_quantum_evaluate_emotion(
    insula_quantum_bridge_t* bridge,
    const float* body_state,
    uint32_t body_state_dim,
    quantum_emotion_result_t* result
);

/**
 * @brief Apply quantum interference to emotional hypotheses
 *
 * WHAT: Use interference to amplify consistent and cancel inconsistent emotions
 * WHY:  Emotional clarity emerges from constructive interference
 * HOW:  Apply phase shifts based on body-emotion consistency
 *
 * @param bridge Quantum bridge
 * @param hypotheses Array of emotion hypotheses
 * @param num_hypotheses Number of hypotheses
 * @param consistency_matrix NxN consistency between hypotheses
 * @return 0 on success, -1 on error
 */
int insula_quantum_apply_emotional_interference(
    insula_quantum_bridge_t* bridge,
    quantum_emotion_hypothesis_t* hypotheses,
    uint32_t num_hypotheses,
    const float* consistency_matrix
);

/*=============================================================================
 * SOMATIC MARKER SEARCH API
 *===========================================================================*/

/**
 * @brief Search somatic markers using Grover algorithm
 *
 * WHAT: Find relevant somatic marker for decision context
 * WHY:  Large marker databases benefit from quantum speedup
 * HOW:  Use Grover search with context-matching oracle
 *
 * @param bridge Quantum bridge
 * @param context_features Feature vector describing decision context
 * @param context_dim Context feature dimension
 * @param marker_database Marker database size
 * @param result Output: search result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 */
int insula_quantum_search_somatic_marker(
    insula_quantum_bridge_t* bridge,
    const float* context_features,
    uint32_t context_dim,
    uint32_t marker_database,
    quantum_somatic_result_t* result
);

/*=============================================================================
 * SOCIAL EMOTION API
 *===========================================================================*/

/**
 * @brief Evaluate social cues using quantum parallel processing
 *
 * WHAT: Process multiple social cues simultaneously
 * WHY:  Social emotions depend on multiple ambiguous cues
 * HOW:  Superposition of social hypotheses with interference
 *
 * @param bridge Quantum bridge
 * @param social_cues Array of social cue values
 * @param num_cues Number of cues
 * @param trust_estimate Output: trust evaluation
 * @param fairness_estimate Output: fairness evaluation
 * @return 0 on success, -1 on error
 */
int insula_quantum_evaluate_social(
    insula_quantum_bridge_t* bridge,
    const float* social_cues,
    uint32_t num_cues,
    float* trust_estimate,
    float* fairness_estimate
);

/*=============================================================================
 * HOMEOSTATIC OPTIMIZATION API
 *===========================================================================*/

/**
 * @brief Find optimal homeostatic adjustment using quantum optimization
 *
 * WHAT: Search for best action to restore homeostasis
 * WHY:  Multiple possible adjustments, need fast search
 * HOW:  Quantum optimization over action space
 *
 * @param bridge Quantum bridge
 * @param current_state Current body state
 * @param target_state Target homeostatic state
 * @param state_dim State dimension
 * @param optimal_action Output: best action
 * @param action_dim Action dimension
 * @return 0 on success, -1 on error
 */
int insula_quantum_optimize_homeostasis(
    insula_quantum_bridge_t* bridge,
    const float* current_state,
    const float* target_state,
    uint32_t state_dim,
    float* optimal_action,
    uint32_t action_dim
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get quantum Insula statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int insula_quantum_get_stats(
    const insula_quantum_bridge_t* bridge,
    insula_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void insula_quantum_reset_stats(insula_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int insula_quantum_get_config(
    const insula_quantum_bridge_t* bridge,
    insula_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSULA_QUANTUM_BRIDGE_H */
