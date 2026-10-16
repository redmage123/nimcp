/**
 * @file nimcp_olfact_quantum_bridge.h
 * @brief Olfactory Cortex Quantum Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Quantum algorithm integration for olfactory cortex that leverages
 *       quantum Monte Carlo simulation, quantum walks, and quantum annealing
 *       for optimized odor processing and recognition.
 *
 * WHY: Quantum algorithms can enhance olfactory processing:
 *      - QMC for receptor binding affinity estimation
 *      - Quantum walks for odor space exploration and similarity search
 *      - Quantum annealing for optimal odor classification
 *      - MCTS with quantum guidance for active sniffing strategies
 *
 * HOW: Provides bridge APIs that connect olfactory processing requests
 *      to quantum backend implementations, managing problem encoding and
 *      result interpretation.
 *
 * QUANTUM APPLICATIONS FOR OLFACTORY:
 * ====================================
 *
 * 1. RECEPTOR BINDING ESTIMATION (QMC):
 *    - Sample odorant-receptor interactions
 *    - Estimate binding affinities
 *    - Predict mixture responses
 *
 * 2. ODOR SPACE SEARCH (QUANTUM WALK):
 *    - Fast similarity search in odor space
 *    - Pattern completion for partial odors
 *    - Memory association traversal
 *
 * 3. ODOR CLASSIFICATION (QUANTUM ANNEALING):
 *    - Optimal category assignment
 *    - Mixture decomposition
 *    - Hedonic value optimization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OLFACT_QUANTUM_BRIDGE_H
#define NIMCP_OLFACT_QUANTUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum qubits for olfactory problems */
#define OLFACT_QUANTUM_MAX_QUBITS        64

/** Default QMC samples */
#define OLFACT_QUANTUM_DEFAULT_QMC_SAMPLES   1000

/** Default quantum walk steps */
#define OLFACT_QUANTUM_DEFAULT_WALK_STEPS    100

/** Maximum odor dimensions */
#define OLFACT_QUANTUM_MAX_ODOR_DIM          128

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Quantum algorithm types for olfactory
 */
typedef enum {
    OLFACT_QUANTUM_ALG_QMC = 0,         /**< Quantum Monte Carlo */
    OLFACT_QUANTUM_ALG_WALK,            /**< Quantum Walk */
    OLFACT_QUANTUM_ALG_ANNEAL,          /**< Quantum Annealing */
    OLFACT_QUANTUM_ALG_GROVER,          /**< Grover-style search */
    OLFACT_QUANTUM_ALG_COUNT
} olfact_quantum_algorithm_t;

/**
 * @brief Problem types for quantum optimization
 */
typedef enum {
    OLFACT_QUANTUM_PROB_BINDING_EST = 0,  /**< Receptor binding estimation */
    OLFACT_QUANTUM_PROB_SIMILARITY,       /**< Odor similarity search */
    OLFACT_QUANTUM_PROB_CLASSIFICATION,   /**< Odor classification */
    OLFACT_QUANTUM_PROB_MIXTURE,          /**< Mixture decomposition */
    OLFACT_QUANTUM_PROB_MEMORY_SEARCH,    /**< Memory association search */
    OLFACT_QUANTUM_PROB_COUNT
} olfact_quantum_problem_t;

/**
 * @brief Quantum computation status
 */
typedef enum {
    OLFACT_QUANTUM_STATUS_IDLE = 0,
    OLFACT_QUANTUM_STATUS_ENCODING,
    OLFACT_QUANTUM_STATUS_COMPUTING,
    OLFACT_QUANTUM_STATUS_DECODING,
    OLFACT_QUANTUM_STATUS_COMPLETE,
    OLFACT_QUANTUM_STATUS_ERROR
} olfact_quantum_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief QMC binding estimation result
 */
typedef struct {
    float* binding_affinities;          /**< Receptor binding affinities */
    uint32_t num_receptors;             /**< Number of receptors */
    float avg_affinity;                 /**< Average binding affinity */
    float variance;                      /**< Estimation variance */
    uint32_t samples_used;              /**< Samples actually used */
} olfact_qmc_binding_result_t;

/**
 * @brief Quantum walk similarity result
 */
typedef struct {
    uint32_t* similar_odors;            /**< Similar odor IDs */
    float* similarity_scores;           /**< Similarity scores */
    uint32_t num_similar;               /**< Number of similar odors */
    uint32_t best_match;                /**< Best matching odor */
    float best_score;                   /**< Best similarity score */
    uint32_t steps_taken;               /**< Steps executed */
} olfact_quantum_similarity_result_t;

/**
 * @brief Quantum annealing classification result
 */
typedef struct {
    odor_category_t category;           /**< Assigned category */
    float confidence;                   /**< Classification confidence */
    float* category_probabilities;      /**< Probabilities per category */
    uint32_t num_categories;            /**< Number of categories */
    float final_energy;                 /**< Final optimization energy */
    bool converged;                      /**< Whether converged */
} olfact_quantum_classification_result_t;

/**
 * @brief Mixture decomposition result
 */
typedef struct {
    uint32_t* component_odors;          /**< Component odor IDs */
    float* component_concentrations;    /**< Component concentrations */
    uint32_t num_components;            /**< Number of components */
    float reconstruction_error;         /**< Reconstruction error */
    float confidence;                   /**< Decomposition confidence */
} olfact_quantum_mixture_result_t;

/**
 * @brief Problem specification for binding estimation
 */
typedef struct {
    float* odorant_features;            /**< Odorant feature vector */
    uint32_t feature_dim;               /**< Feature dimensionality */
    uint32_t num_receptor_types;        /**< Number of receptor types */
    float temperature;                  /**< Simulation temperature */
} olfact_binding_est_spec_t;

/**
 * @brief Problem specification for similarity search
 */
typedef struct {
    float* query_odor;                  /**< Query odor pattern */
    uint32_t odor_dim;                  /**< Odor dimensionality */
    uint32_t max_results;               /**< Maximum results to return */
    float similarity_threshold;         /**< Minimum similarity */
} olfact_similarity_spec_t;

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
    uint32_t max_qubits;                /**< Maximum qubits */
    float convergence_threshold;        /**< Convergence threshold */
    uint32_t max_iterations;            /**< Maximum iterations */

    /* Feature flags */
    bool enable_qmc;                    /**< Enable QMC */
    bool enable_walks;                  /**< Enable quantum walks */
    bool enable_annealing;              /**< Enable annealing */
    bool enable_grover;                 /**< Enable Grover search */
    bool use_classical_fallback;        /**< Fallback to classical */

    /* Performance */
    bool async_computation;             /**< Async computation */
    uint32_t timeout_ms;                /**< Timeout */

    /* Logging */
    bool enable_logging;                /**< Enable logging */
} olfact_quantum_config_t;

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    uint64_t qmc_computations;
    uint64_t walk_computations;
    uint64_t anneal_computations;
    uint64_t grover_computations;

    uint64_t successful_computations;
    uint64_t failed_computations;
    uint64_t timeouts;

    float avg_qmc_time_ms;
    float avg_walk_time_ms;
    float avg_anneal_time_ms;

    float avg_solution_quality;
    float best_solution_quality;

    uint64_t classical_fallbacks;
} olfact_quantum_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct olfact_quantum_bridge_struct olfact_quantum_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int olfact_quantum_default_config(olfact_quantum_config_t* config);
olfact_quantum_bridge_t* olfact_quantum_bridge_create(const olfact_quantum_config_t* config);
void olfact_quantum_bridge_destroy(olfact_quantum_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int olfact_quantum_connect(olfact_quantum_bridge_t* bridge, nimcp_olfactory_t* olfact);
int olfact_quantum_disconnect(olfact_quantum_bridge_t* bridge);
bool olfact_quantum_is_connected(const olfact_quantum_bridge_t* bridge);
olfact_quantum_status_t olfact_quantum_get_status(const olfact_quantum_bridge_t* bridge);

/* ============================================================================
 * QMC API
 * ============================================================================ */

int olfact_quantum_estimate_binding(olfact_quantum_bridge_t* bridge, const olfact_binding_est_spec_t* spec, olfact_qmc_binding_result_t* result);
int olfact_quantum_sample_receptor_response(olfact_quantum_bridge_t* bridge, const float* odorant, uint32_t dim, uint32_t num_samples, float* responses);

/* ============================================================================
 * Quantum Walk API
 * ============================================================================ */

int olfact_quantum_search_similar(olfact_quantum_bridge_t* bridge, const olfact_similarity_spec_t* spec, olfact_quantum_similarity_result_t* result);
int olfact_quantum_search_memory(olfact_quantum_bridge_t* bridge, const float* odor, uint32_t dim, uint32_t* memory_id);
int olfact_quantum_complete_pattern(olfact_quantum_bridge_t* bridge, const float* partial_odor, uint32_t dim, float* completed);

/* ============================================================================
 * Quantum Annealing API
 * ============================================================================ */

int olfact_quantum_classify_odor(olfact_quantum_bridge_t* bridge, const float* odor, uint32_t dim, olfact_quantum_classification_result_t* result);
int olfact_quantum_decompose_mixture(olfact_quantum_bridge_t* bridge, const float* mixture, uint32_t dim, olfact_quantum_mixture_result_t* result);
int olfact_quantum_optimize_hedonic(olfact_quantum_bridge_t* bridge, const float* odor, uint32_t dim, float* hedonic_value);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void olfact_qmc_binding_result_free(olfact_qmc_binding_result_t* result);
void olfact_quantum_similarity_result_free(olfact_quantum_similarity_result_t* result);
void olfact_quantum_classification_result_free(olfact_quantum_classification_result_t* result);
void olfact_quantum_mixture_result_free(olfact_quantum_mixture_result_t* result);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int olfact_quantum_get_stats(const olfact_quantum_bridge_t* bridge, olfact_quantum_stats_t* stats);
int olfact_quantum_reset_stats(olfact_quantum_bridge_t* bridge);
const char* olfact_quantum_algorithm_name(olfact_quantum_algorithm_t alg);
const char* olfact_quantum_problem_name(olfact_quantum_problem_t prob);
void olfact_quantum_print_summary(const olfact_quantum_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLFACT_QUANTUM_BRIDGE_H */
