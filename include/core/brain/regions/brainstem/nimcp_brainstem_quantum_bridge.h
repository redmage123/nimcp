/**
 * @file nimcp_brainstem_quantum_bridge.h
 * @brief Quantum-accelerated processing for brainstem functions
 *
 * WHAT: Quantum bridge for brainstem computational acceleration
 * WHY:  Enable parallel reflex pathway evaluation and arousal state optimization
 * HOW:  Uses quantum algorithms for combinatorial optimization in brainstem
 *
 * QUANTUM APPLICATIONS:
 * - Grover Search: Fast reflex pathway selection
 * - Quantum Annealing: Optimal arousal state finding
 * - Amplitude Amplification: Parallel sensory processing
 * - QAOA: Multi-pathway optimization
 *
 * BIOLOGICAL MAPPING:
 * - Reflex pathway selection -> Grover search (O(sqrt(N)) speedup)
 * - Arousal optimization -> Quantum annealing (find global minima)
 * - Sensory integration -> Amplitude amplification
 * - Motor planning -> QAOA for trajectory optimization
 *
 * @version Phase BS-2: Quantum Brainstem Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAINSTEM_QUANTUM_BRIDGE_H
#define NIMCP_BRAINSTEM_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"

/* Forward declaration for quantum reasoner */
typedef struct brain_qreason_ctx brain_qreason_ctx_t;

/* Forward declaration for quantum annealer */
typedef struct quantum_annealer_struct* quantum_annealer_t;

/**
 * @brief Opaque quantum bridge handle
 */
typedef struct brainstem_quantum_bridge brainstem_quantum_bridge_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Quantum algorithm selection
 */
typedef enum {
    BRAINSTEM_QUANTUM_ALG_NONE = 0,      /**< No quantum (classical fallback) */
    BRAINSTEM_QUANTUM_ALG_GROVER,        /**< Grover search for pathway selection */
    BRAINSTEM_QUANTUM_ALG_ANNEALING,     /**< Quantum annealing for optimization */
    BRAINSTEM_QUANTUM_ALG_AMPLITUDE,     /**< Amplitude amplification */
    BRAINSTEM_QUANTUM_ALG_QAOA,          /**< QAOA for combinatorial optimization */
    BRAINSTEM_QUANTUM_ALG_VQE            /**< VQE for energy minimization */
} brainstem_quantum_algorithm_t;

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    /* Algorithm selection */
    brainstem_quantum_algorithm_t reflex_algorithm;   /**< Algorithm for reflex selection */
    brainstem_quantum_algorithm_t arousal_algorithm;  /**< Algorithm for arousal optimization */
    brainstem_quantum_algorithm_t sensory_algorithm;  /**< Algorithm for sensory processing */

    /* Quantum parameters */
    uint32_t max_qubits;             /**< Maximum qubits available */
    uint32_t grover_iterations;      /**< Grover iteration count (O(sqrt(N))) */
    uint32_t annealing_steps;        /**< Annealing schedule steps */
    float annealing_temperature;     /**< Initial temperature */
    uint32_t qaoa_layers;            /**< QAOA circuit depth */

    /* Performance tuning */
    float quantum_classical_mix;     /**< Mix ratio [0=quantum, 1=classical] */
    bool enable_noise_mitigation;    /**< Error mitigation techniques */
    bool enable_hybrid_execution;    /**< Hybrid quantum-classical */

    /* Thresholds */
    float quantum_advantage_threshold; /**< Min problem size for quantum */
    uint32_t min_pathways_for_quantum; /**< Min pathways for Grover benefit */
} brainstem_quantum_config_t;

/*=============================================================================
 * RESULT STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum reflex search result
 */
typedef struct {
    uint32_t selected_reflex_id;     /**< Best reflex pathway ID */
    float selection_confidence;       /**< Confidence in selection [0-1] */
    uint32_t pathways_evaluated;      /**< Number of pathways checked */
    float quantum_speedup;            /**< Estimated speedup over classical */
    double execution_time_us;         /**< Execution time */
    bool used_quantum;                /**< Whether quantum was actually used */
} quantum_reflex_result_t;

/**
 * @brief Quantum arousal optimization result
 */
typedef struct {
    float optimal_arousal;           /**< Optimal arousal level [0-1] */
    float energy_minimum;            /**< Free energy at optimal state */
    uint32_t iterations;             /**< Optimization iterations */
    float convergence_metric;        /**< How well it converged */
    double execution_time_us;        /**< Execution time */
    bool used_quantum;               /**< Whether quantum was used */
} quantum_arousal_result_t;

/**
 * @brief Quantum sensory integration result
 */
typedef struct {
    float integrated_salience;       /**< Combined sensory salience */
    float visual_weight;             /**< Visual contribution */
    float auditory_weight;           /**< Auditory contribution */
    float attention_direction_x;     /**< Optimal attention X */
    float attention_direction_y;     /**< Optimal attention Y */
    double execution_time_us;        /**< Execution time */
    bool used_quantum;               /**< Whether quantum was used */
} quantum_sensory_result_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Usage counts */
    uint64_t reflex_queries;         /**< Reflex search queries */
    uint64_t arousal_optimizations;  /**< Arousal optimization calls */
    uint64_t sensory_integrations;   /**< Sensory integration calls */

    /* Quantum usage */
    uint64_t quantum_executions;     /**< Times quantum was used */
    uint64_t classical_fallbacks;    /**< Times classical was used */
    float avg_quantum_speedup;       /**< Average speedup achieved */

    /* Timing */
    double total_quantum_time_us;    /**< Total quantum execution time */
    double total_classical_time_us;  /**< Total classical execution time */
    float avg_execution_time_us;     /**< Average per-call time */
} brainstem_quantum_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 *
 * @return Default configuration
 */
brainstem_quantum_config_t brainstem_quantum_default_config(void);

/**
 * @brief Create quantum bridge for brainstem
 *
 * @param adapter Brainstem adapter to accelerate
 * @param config Configuration (NULL for defaults)
 * @return New quantum bridge, or NULL on failure
 */
brainstem_quantum_bridge_t* brainstem_quantum_bridge_create(
    brainstem_adapter_t* adapter,
    const brainstem_quantum_config_t* config);

/**
 * @brief Destroy quantum bridge
 *
 * @param bridge Bridge to destroy
 */
void brainstem_quantum_bridge_destroy(brainstem_quantum_bridge_t* bridge);

/**
 * @brief Connect external quantum reasoner
 *
 * @param bridge Quantum bridge
 * @param qreason External quantum reasoner context
 * @return true on success
 */
bool brainstem_quantum_bridge_connect_reasoner(
    brainstem_quantum_bridge_t* bridge,
    brain_qreason_ctx_t* qreason);

/**
 * @brief Connect external quantum annealer
 *
 * @param bridge Quantum bridge
 * @param annealer External quantum annealer
 * @return true on success
 */
bool brainstem_quantum_bridge_connect_annealer(
    brainstem_quantum_bridge_t* bridge,
    quantum_annealer_t annealer);

/*=============================================================================
 * QUANTUM OPERATIONS
 *===========================================================================*/

/**
 * @brief Quantum-accelerated reflex pathway selection
 *
 * Uses Grover search to find optimal reflex pathway among registered reflexes.
 * Falls back to classical if quantum is not available or problem is too small.
 *
 * @param bridge Quantum bridge
 * @param stimulus_pattern Stimulus pattern to match
 * @param pattern_size Size of stimulus pattern
 * @param urgency Urgency level for selection [0-1]
 * @param result Output result structure
 * @return true on success
 */
bool brainstem_quantum_select_reflex(
    brainstem_quantum_bridge_t* bridge,
    const float* stimulus_pattern,
    uint32_t pattern_size,
    float urgency,
    quantum_reflex_result_t* result);

/**
 * @brief Quantum-accelerated arousal optimization
 *
 * Uses quantum annealing to find optimal arousal state that minimizes
 * free energy given current sensory and internal states.
 *
 * @param bridge Quantum bridge
 * @param current_arousal Current arousal level
 * @param sensory_load Current sensory processing load [0-1]
 * @param metabolic_state Metabolic energy state [0-1]
 * @param threat_level Current threat assessment [0-1]
 * @param result Output result structure
 * @return true on success
 */
bool brainstem_quantum_optimize_arousal(
    brainstem_quantum_bridge_t* bridge,
    float current_arousal,
    float sensory_load,
    float metabolic_state,
    float threat_level,
    quantum_arousal_result_t* result);

/**
 * @brief Quantum-accelerated multi-sensory integration
 *
 * Uses amplitude amplification to optimally combine visual and auditory
 * inputs for attention allocation.
 *
 * @param bridge Quantum bridge
 * @param visual_input Visual sensory data
 * @param auditory_input Auditory sensory data
 * @param result Output result structure
 * @return true on success
 */
bool brainstem_quantum_integrate_sensory(
    brainstem_quantum_bridge_t* bridge,
    const brainstem_sensory_input_t* visual_input,
    const brainstem_sensory_input_t* auditory_input,
    quantum_sensory_result_t* result);

/**
 * @brief Parallel reflex pathway evaluation
 *
 * Evaluates multiple reflex pathways in parallel using quantum superposition.
 * Returns ranked list of activated pathways.
 *
 * @param bridge Quantum bridge
 * @param stimulus Stimulus intensity
 * @param pathway_ids Array to receive activated pathway IDs
 * @param activations Array to receive activation levels
 * @param max_pathways Maximum pathways to return
 * @return Number of activated pathways
 */
uint32_t brainstem_quantum_evaluate_pathways(
    brainstem_quantum_bridge_t* bridge,
    float stimulus,
    uint32_t* pathway_ids,
    float* activations,
    uint32_t max_pathways);

/*=============================================================================
 * STATE AND STATISTICS
 *===========================================================================*/

/**
 * @brief Update quantum bridge state
 *
 * @param bridge Quantum bridge
 * @param dt Time step in seconds
 * @return true on success
 */
bool brainstem_quantum_bridge_update(brainstem_quantum_bridge_t* bridge, float dt);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Quantum bridge
 * @param stats Output statistics structure
 * @return true on success
 */
bool brainstem_quantum_bridge_get_stats(
    const brainstem_quantum_bridge_t* bridge,
    brainstem_quantum_stats_t* stats);

/**
 * @brief Check if quantum execution is available
 *
 * @param bridge Quantum bridge
 * @return true if quantum resources are available
 */
bool brainstem_quantum_bridge_is_quantum_available(
    const brainstem_quantum_bridge_t* bridge);

/**
 * @brief Get quantum speedup estimate for given problem size
 *
 * @param bridge Quantum bridge
 * @param problem_size Size of the problem
 * @param algorithm Algorithm to estimate for
 * @return Estimated speedup factor (1.0 = no speedup)
 */
float brainstem_quantum_bridge_estimate_speedup(
    const brainstem_quantum_bridge_t* bridge,
    uint32_t problem_size,
    brainstem_quantum_algorithm_t algorithm);

/**
 * @brief Set quantum-classical mixing ratio
 *
 * @param bridge Quantum bridge
 * @param mix Mixing ratio [0=pure quantum, 1=pure classical]
 * @return true on success
 */
bool brainstem_quantum_bridge_set_mix(
    brainstem_quantum_bridge_t* bridge,
    float mix);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_QUANTUM_BRIDGE_H */
