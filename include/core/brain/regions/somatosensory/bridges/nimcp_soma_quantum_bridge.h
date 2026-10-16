/**
 * @file nimcp_soma_quantum_bridge.h
 * @brief Somatosensory Cortex Quantum Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Quantum algorithm integration for somatosensory cortex that leverages
 *       quantum Monte Carlo simulation, quantum walks, and quantum annealing
 *       for optimized sensory processing.
 *
 * WHY: Quantum algorithms can enhance somatosensory processing:
 *      - QMC for receptor threshold optimization (finding optimal sensitivity)
 *      - Quantum walks for body map exploration and pattern matching
 *      - Quantum annealing for pain/pleasure tradeoff optimization
 *      - MCTS with quantum guidance for tactile exploration strategies
 *
 * HOW: Provides bridge APIs that connect somatosensory processing requests
 *      to quantum backend implementations, managing problem encoding and
 *      result interpretation.
 *
 * QUANTUM APPLICATIONS FOR SOMATOSENSORY:
 * =======================================
 *
 * 1. RECEPTOR THRESHOLD OPTIMIZATION (QMC):
 *    - Sample receptor response curves
 *    - Optimize sensitivity thresholds
 *    - Balance noise vs signal detection
 *
 * 2. BODY MAP SEARCH (QUANTUM WALK):
 *    - Fast localization of active regions
 *    - Pattern propagation through homunculus
 *    - Cross-region correlation detection
 *
 * 3. SENSORY INTEGRATION (QUANTUM ANNEALING):
 *    - Multi-modal sensory fusion
 *    - Optimal feature binding
 *    - Attention allocation optimization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SOMA_QUANTUM_BRIDGE_H
#define NIMCP_SOMA_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum qubits for somatosensory problems */
#define SOMA_QUANTUM_MAX_QUBITS         64

/** Default QMC samples */
#define SOMA_QUANTUM_DEFAULT_QMC_SAMPLES    1000

/** Default quantum walk steps */
#define SOMA_QUANTUM_DEFAULT_WALK_STEPS     100

/** Default annealing schedule steps */
#define SOMA_QUANTUM_DEFAULT_ANNEAL_STEPS   500

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Quantum algorithm types for somatosensory
 */
typedef enum {
    SOMA_QUANTUM_ALG_QMC = 0,           /**< Quantum Monte Carlo */
    SOMA_QUANTUM_ALG_WALK,              /**< Quantum Walk */
    SOMA_QUANTUM_ALG_ANNEAL,            /**< Quantum Annealing */
    SOMA_QUANTUM_ALG_MCTS,              /**< Quantum-guided MCTS */
    SOMA_QUANTUM_ALG_COUNT
} soma_quantum_algorithm_t;

/**
 * @brief Problem types for quantum optimization
 */
typedef enum {
    SOMA_QUANTUM_PROB_THRESHOLD_OPT = 0,  /**< Receptor threshold optimization */
    SOMA_QUANTUM_PROB_BODY_MAP_SEARCH,    /**< Body map region search */
    SOMA_QUANTUM_PROB_PATTERN_MATCH,      /**< Tactile pattern matching */
    SOMA_QUANTUM_PROB_ATTENTION_ALLOC,    /**< Attention allocation */
    SOMA_QUANTUM_PROB_PAIN_MODULATION,    /**< Pain modulation optimization */
    SOMA_QUANTUM_PROB_COUNT
} soma_quantum_problem_t;

/**
 * @brief Quantum computation status
 */
typedef enum {
    SOMA_QUANTUM_STATUS_IDLE = 0,
    SOMA_QUANTUM_STATUS_ENCODING,
    SOMA_QUANTUM_STATUS_COMPUTING,
    SOMA_QUANTUM_STATUS_DECODING,
    SOMA_QUANTUM_STATUS_COMPLETE,
    SOMA_QUANTUM_STATUS_ERROR
} soma_quantum_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief QMC optimization result
 */
typedef struct {
    float* optimal_thresholds;          /**< Optimized threshold values */
    uint32_t num_thresholds;            /**< Number of thresholds */
    float energy;                        /**< Solution energy */
    float variance;                      /**< Sampling variance */
    uint32_t samples_used;              /**< Samples actually used */
    float convergence_rate;             /**< Convergence metric */
} soma_qmc_result_t;

/**
 * @brief Quantum walk result
 */
typedef struct {
    uint32_t* visited_regions;          /**< Visited body regions */
    uint32_t num_visited;               /**< Number visited */
    float* region_probabilities;        /**< Final probability distribution */
    uint32_t target_region;             /**< Target if found */
    bool target_found;                   /**< Whether target was found */
    uint32_t steps_taken;               /**< Steps executed */
} soma_quantum_walk_result_t;

/**
 * @brief Quantum annealing result
 */
typedef struct {
    float* solution_vector;             /**< Optimal solution */
    uint32_t solution_dim;              /**< Solution dimensionality */
    float final_energy;                 /**< Final energy */
    float initial_energy;               /**< Starting energy */
    float temperature_final;            /**< Final temperature */
    bool converged;                      /**< Converged to minimum */
    uint32_t iterations;                /**< Iterations performed */
} soma_quantum_anneal_result_t;

/**
 * @brief Problem specification for threshold optimization
 */
typedef struct {
    float* current_thresholds;          /**< Current threshold values */
    uint32_t num_thresholds;            /**< Number of thresholds */
    float* signal_samples;              /**< Signal samples for evaluation */
    uint32_t num_samples;               /**< Number of signal samples */
    float target_sensitivity;           /**< Target sensitivity [0, 1] */
    float noise_tolerance;              /**< Noise tolerance [0, 1] */
} soma_threshold_opt_spec_t;

/**
 * @brief Problem specification for body map search
 */
typedef struct {
    float* activation_map;              /**< Current body activation */
    uint32_t map_dim;                   /**< Map dimensionality */
    uint32_t start_region;              /**< Starting region */
    uint32_t target_region;             /**< Target region (or -1 for max) */
    float activation_threshold;         /**< Activation threshold */
} soma_body_map_search_spec_t;

/**
 * @brief Problem specification for attention allocation
 */
typedef struct {
    float* region_salience;             /**< Salience per region */
    float* region_importance;           /**< Importance weights */
    uint32_t num_regions;               /**< Number of regions */
    float total_attention_budget;       /**< Total attention budget */
    float* current_allocation;          /**< Current allocation */
} soma_attention_alloc_spec_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    /* Algorithm parameters */
    uint32_t qmc_samples;               /**< QMC sample count */
    uint32_t walk_steps;                /**< Quantum walk steps */
    uint32_t anneal_steps;              /**< Annealing steps */
    float anneal_initial_temp;          /**< Initial temperature */
    float anneal_final_temp;            /**< Final temperature */

    /* Precision settings */
    uint32_t max_qubits;                /**< Maximum qubits to use */
    float convergence_threshold;        /**< Convergence threshold */
    uint32_t max_iterations;            /**< Maximum iterations */

    /* Feature flags */
    bool enable_qmc;                    /**< Enable QMC */
    bool enable_walks;                  /**< Enable quantum walks */
    bool enable_annealing;              /**< Enable annealing */
    bool enable_mcts_guidance;          /**< Enable MCTS guidance */
    bool use_classical_fallback;        /**< Fallback to classical */

    /* Performance */
    bool async_computation;             /**< Async quantum computation */
    uint32_t timeout_ms;                /**< Computation timeout */

    /* Logging */
    bool enable_logging;                /**< Enable logging */
} soma_quantum_config_t;

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    /* Computation counts */
    uint64_t qmc_computations;
    uint64_t walk_computations;
    uint64_t anneal_computations;
    uint64_t mcts_computations;

    /* Success rates */
    uint64_t successful_computations;
    uint64_t failed_computations;
    uint64_t timeouts;

    /* Timing */
    float avg_qmc_time_ms;
    float avg_walk_time_ms;
    float avg_anneal_time_ms;

    /* Quality metrics */
    float avg_solution_quality;
    float best_solution_quality;

    /* Classical fallbacks */
    uint64_t classical_fallbacks;
} soma_quantum_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct soma_quantum_bridge_struct soma_quantum_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int soma_quantum_default_config(soma_quantum_config_t* config);
soma_quantum_bridge_t* soma_quantum_bridge_create(const soma_quantum_config_t* config);
void soma_quantum_bridge_destroy(soma_quantum_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int soma_quantum_connect(soma_quantum_bridge_t* bridge, nimcp_somatosensory_t* soma);
int soma_quantum_disconnect(soma_quantum_bridge_t* bridge);
bool soma_quantum_is_connected(const soma_quantum_bridge_t* bridge);
soma_quantum_status_t soma_quantum_get_status(const soma_quantum_bridge_t* bridge);

/* ============================================================================
 * QMC API
 * ============================================================================ */

int soma_quantum_optimize_thresholds(soma_quantum_bridge_t* bridge, const soma_threshold_opt_spec_t* spec, soma_qmc_result_t* result);
int soma_quantum_sample_receptor_response(soma_quantum_bridge_t* bridge, body_segment_t region, uint32_t num_samples, float* samples);
int soma_quantum_estimate_sensitivity(soma_quantum_bridge_t* bridge, body_segment_t region, float* sensitivity);

/* ============================================================================
 * Quantum Walk API
 * ============================================================================ */

int soma_quantum_search_body_map(soma_quantum_bridge_t* bridge, const soma_body_map_search_spec_t* spec, soma_quantum_walk_result_t* result);
int soma_quantum_localize_stimulus(soma_quantum_bridge_t* bridge, const float* activation, uint32_t dim, uint32_t* region);
int soma_quantum_propagate_activation(soma_quantum_bridge_t* bridge, uint32_t source_region, float* propagated);

/* ============================================================================
 * Quantum Annealing API
 * ============================================================================ */

int soma_quantum_optimize_attention(soma_quantum_bridge_t* bridge, const soma_attention_alloc_spec_t* spec, soma_quantum_anneal_result_t* result);
int soma_quantum_optimize_pain_modulation(soma_quantum_bridge_t* bridge, float pain_level, float* modulation);
int soma_quantum_bind_features(soma_quantum_bridge_t* bridge, const float* features, uint32_t num_features, float* binding_weights);

/* ============================================================================
 * MCTS Guidance API
 * ============================================================================ */

int soma_quantum_mcts_explore(soma_quantum_bridge_t* bridge, const float* state, uint32_t state_dim, uint32_t* action);
int soma_quantum_mcts_evaluate(soma_quantum_bridge_t* bridge, const float* state, uint32_t state_dim, float* value);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void soma_qmc_result_free(soma_qmc_result_t* result);
void soma_quantum_walk_result_free(soma_quantum_walk_result_t* result);
void soma_quantum_anneal_result_free(soma_quantum_anneal_result_t* result);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int soma_quantum_get_stats(const soma_quantum_bridge_t* bridge, soma_quantum_stats_t* stats);
int soma_quantum_reset_stats(soma_quantum_bridge_t* bridge);
const char* soma_quantum_algorithm_name(soma_quantum_algorithm_t alg);
const char* soma_quantum_problem_name(soma_quantum_problem_t prob);
const char* soma_quantum_status_name(soma_quantum_status_t status);
void soma_quantum_print_summary(const soma_quantum_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMA_QUANTUM_BRIDGE_H */
