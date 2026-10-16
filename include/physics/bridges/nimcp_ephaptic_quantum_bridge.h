//=============================================================================
// nimcp_ephaptic_quantum_bridge.h - Ephaptic Quantum Monte Carlo Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_quantum_bridge.h
 * @brief QMC integration for Ephaptic coupling module
 *
 * WHAT: Quantum Monte Carlo methods for ephaptic field analysis
 *
 * WHY:  QMC provides:
 *       - Phase coherence optimization via annealing
 *       - Quantum walk for field propagation patterns
 *       - Entropy estimation for collective neural dynamics
 *       - Synchronization analysis using MCTS
 *
 * HOW:  - Uses qmc_adaptive_anneal() for Kuramoto coupling optimization
 *       - Uses qmc_walk_mcts() for field propagation modeling
 *       - Uses qmc_estimate_entropy() for collective entropy
 *
 * BIOLOGICAL: Ephaptic fields exhibit emergent coherent dynamics similar
 * to quantum systems. QMC methods help optimize synchronization parameters
 * and understand information flow through extracellular fields.
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_QUANTUM_BRIDGE_H
#define NIMCP_EPHAPTIC_QUANTUM_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default number of annealing iterations */
#define EPHAPTIC_QMC_DEFAULT_ITERATIONS     1000

/** Default MCTS iterations for quantum walk */
#define EPHAPTIC_QMC_DEFAULT_MCTS_ITERS     500

/** Default number of samples for entropy estimation */
#define EPHAPTIC_QMC_DEFAULT_SAMPLES        5000

//=============================================================================
// Phase Coherence Optimization Types
//=============================================================================

/**
 * @brief Target coherence for optimization
 */
typedef struct {
    float target_coherence;         /**< Desired order parameter r [0,1] */
    float target_frequency;         /**< Desired oscillation frequency (Hz) */
    float frequency_tolerance;      /**< Acceptable frequency deviation */
    uint32_t min_synchronized;      /**< Minimum neurons to synchronize */
} ephaptic_coherence_target_t;

/**
 * @brief Configuration for coherence optimization
 */
typedef struct {
    /** Annealing parameters */
    float initial_temp;
    float final_temp;
    uint32_t num_iterations;
    float quantum_strength;

    /** Optimization bounds */
    float coupling_min, coupling_max;       /**< Kuramoto coupling bounds */
    float sync_threshold_min, sync_threshold_max;
    float field_decay_min, field_decay_max;

    /** Simulation parameters */
    float sim_duration_ms;
    float sim_dt;

    uint32_t seed;
} ephaptic_coherence_config_t;

/**
 * @brief Result of coherence optimization
 */
typedef struct {
    /** Optimized parameters */
    float opt_coupling;                 /**< Optimal Kuramoto coupling K */
    float opt_sync_threshold;           /**< Optimal sync threshold */
    float opt_field_decay;              /**< Optimal field decay constant */

    /** Achieved behavior */
    float achieved_coherence;           /**< Achieved order parameter */
    float achieved_frequency;           /**< Achieved oscillation frequency */
    uint32_t synced_neurons;            /**< Number of synchronized neurons */

    /** Optimization stats */
    float final_energy;
    float acceptance_rate;
    uint32_t iterations_run;
    bool converged;
} ephaptic_coherence_result_t;

//=============================================================================
// Quantum Walk Types
//=============================================================================

/**
 * @brief Configuration for field propagation quantum walk
 */
typedef struct {
    uint32_t max_steps;             /**< Maximum walk steps */
    uint32_t mcts_iterations;       /**< MCTS iterations per step */
    float exploration_constant;     /**< UCB exploration parameter */
    bool adaptive_coin;             /**< Use adaptive coin selection */
    uint32_t seed;
} ephaptic_walk_config_t;

/**
 * @brief Result of field propagation quantum walk
 */
typedef struct {
    uint32_t steps_taken;           /**< Steps to reach target */
    float target_probability;       /**< Probability at target */
    float mean_hitting_time;        /**< Mean hitting time */
    float propagation_speed;        /**< Effective propagation speed */
    float* amplitude_distribution;  /**< Final amplitude distribution */
    uint32_t num_nodes;             /**< Number of nodes */
    bool target_reached;
} ephaptic_walk_result_t;

//=============================================================================
// Collective Entropy Types
//=============================================================================

/**
 * @brief Configuration for collective entropy analysis
 */
typedef struct {
    uint32_t num_samples;           /**< MC samples */
    float time_window_ms;           /**< Analysis time window */
    bool compute_mutual_info;       /**< Compute MI between regions */
    uint32_t num_regions;           /**< Number of spatial regions */
    uint32_t seed;
} ephaptic_entropy_config_t;

/**
 * @brief Result of collective entropy analysis
 */
typedef struct {
    float phase_entropy;            /**< Entropy of phase distribution */
    float field_entropy;            /**< Entropy of field distribution */
    float spatial_entropy;          /**< Spatial organization entropy */
    float collective_info;          /**< Total collective information */
    float* region_entropies;        /**< Per-region entropies */
    float* mutual_info_matrix;      /**< Pairwise MI (if requested) */
    uint32_t num_regions;
} ephaptic_entropy_result_t;

//=============================================================================
// Synchronization Pattern Types
//=============================================================================

/**
 * @brief Configuration for sync pattern discovery
 */
typedef struct {
    uint32_t mcts_iterations;
    float exploration_constant;
    uint32_t max_patterns;          /**< Max patterns to discover */
    float min_coherence;            /**< Min coherence for pattern */
    uint32_t seed;
} ephaptic_pattern_config_t;

/**
 * @brief Discovered synchronization pattern
 */
typedef struct {
    uint32_t* neuron_ids;           /**< Neurons in pattern */
    uint32_t neuron_count;
    float coherence;                /**< Pattern coherence */
    float frequency;                /**< Pattern frequency */
    float phase_offset;             /**< Phase offset from mean */
} ephaptic_sync_pattern_t;

/**
 * @brief Result of pattern discovery
 */
typedef struct {
    ephaptic_sync_pattern_t* patterns;
    uint32_t num_patterns;
    float total_coherence;          /**< Overall system coherence */
    float pattern_coverage;         /**< Fraction of neurons in patterns */
} ephaptic_pattern_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default coherence optimization configuration
 */
NIMCP_EXPORT int ephaptic_qmc_coherence_default_config(
    ephaptic_coherence_config_t* config
);

/**
 * @brief Get default coherence target
 */
NIMCP_EXPORT int ephaptic_qmc_coherence_default_target(
    ephaptic_coherence_target_t* target
);

/**
 * @brief Get default quantum walk configuration
 */
NIMCP_EXPORT int ephaptic_qmc_walk_default_config(
    ephaptic_walk_config_t* config
);

/**
 * @brief Get default entropy analysis configuration
 */
NIMCP_EXPORT int ephaptic_qmc_entropy_default_config(
    ephaptic_entropy_config_t* config
);

/**
 * @brief Get default pattern discovery configuration
 */
NIMCP_EXPORT int ephaptic_qmc_pattern_default_config(
    ephaptic_pattern_config_t* config
);

//=============================================================================
// Phase Coherence Optimization API
//=============================================================================

/**
 * @brief Optimize Kuramoto coupling for target coherence
 *
 * WHAT: Find optimal coupling parameters for desired synchronization
 * WHY:  Tune ephaptic system for specific coherence/frequency
 * HOW:  QMC annealing with Kuramoto model simulation
 *
 * @param system    Ephaptic system (modified in place)
 * @param target    Target coherence behavior
 * @param config    Optimization configuration
 * @param result    Output optimization result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_qmc_optimize_coherence(
    nimcp_ephaptic_system_t* system,
    const ephaptic_coherence_target_t* target,
    const ephaptic_coherence_config_t* config,
    ephaptic_coherence_result_t* result
);

/**
 * @brief Apply optimized parameters to system
 */
NIMCP_EXPORT int ephaptic_qmc_apply_coherence_result(
    nimcp_ephaptic_system_t* system,
    const ephaptic_coherence_result_t* result
);

/**
 * @brief Compute current phase coherence (order parameter)
 */
NIMCP_EXPORT int ephaptic_qmc_current_coherence(
    const nimcp_ephaptic_system_t* system,
    float* coherence
);

//=============================================================================
// Quantum Walk API
//=============================================================================

/**
 * @brief Simulate field propagation via quantum walk
 *
 * WHAT: Model how ephaptic fields propagate through neural tissue
 * WHY:  Understand field-mediated information flow
 * HOW:  MCTS-guided quantum walk on neural topology
 *
 * @param system        Ephaptic system
 * @param start_idx     Starting neuron index
 * @param target_idx    Target neuron index
 * @param config        Walk configuration
 * @param result        Output walk result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_qmc_field_walk(
    const nimcp_ephaptic_system_t* system,
    uint32_t start_idx,
    uint32_t target_idx,
    const ephaptic_walk_config_t* config,
    ephaptic_walk_result_t* result
);

/**
 * @brief Free walk result resources
 */
NIMCP_EXPORT void ephaptic_qmc_walk_result_free(
    ephaptic_walk_result_t* result
);

/**
 * @brief Compute field propagation speed
 *
 * @param system    Ephaptic system
 * @param speed     Output: propagation speed (mm/ms)
 * @return 0 on success
 */
NIMCP_EXPORT int ephaptic_qmc_propagation_speed(
    const nimcp_ephaptic_system_t* system,
    float* speed
);

//=============================================================================
// Collective Entropy API
//=============================================================================

/**
 * @brief Analyze collective entropy of ephaptic system
 *
 * WHAT: Compute information-theoretic measures of field dynamics
 * WHY:  Quantify collective organization and information capacity
 * HOW:  MC sampling of phase and field distributions
 *
 * @param system    Ephaptic system
 * @param config    Entropy analysis configuration
 * @param result    Output entropy result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_qmc_collective_entropy(
    const nimcp_ephaptic_system_t* system,
    const ephaptic_entropy_config_t* config,
    ephaptic_entropy_result_t* result
);

/**
 * @brief Free entropy result resources
 */
NIMCP_EXPORT void ephaptic_qmc_entropy_result_free(
    ephaptic_entropy_result_t* result
);

/**
 * @brief Compute mutual information between field regions
 *
 * @param system    Ephaptic system
 * @param region1   First region neuron indices
 * @param count1    Count of neurons in region 1
 * @param region2   Second region neuron indices
 * @param count2    Count of neurons in region 2
 * @param mi        Output: mutual information (bits)
 * @return 0 on success
 */
NIMCP_EXPORT int ephaptic_qmc_region_mutual_info(
    const nimcp_ephaptic_system_t* system,
    const uint32_t* region1,
    uint32_t count1,
    const uint32_t* region2,
    uint32_t count2,
    float* mi
);

//=============================================================================
// Synchronization Pattern API
//=============================================================================

/**
 * @brief Discover synchronization patterns via MCTS
 *
 * WHAT: Find groups of neurons that synchronize together
 * WHY:  Identify functional clusters in ephaptic field
 * HOW:  MCTS search over neuron groupings
 *
 * @param system    Ephaptic system
 * @param config    Pattern discovery configuration
 * @param result    Output pattern result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_qmc_discover_patterns(
    const nimcp_ephaptic_system_t* system,
    const ephaptic_pattern_config_t* config,
    ephaptic_pattern_result_t* result
);

/**
 * @brief Free pattern result resources
 */
NIMCP_EXPORT void ephaptic_qmc_pattern_result_free(
    ephaptic_pattern_result_t* result
);

/**
 * @brief Classify neuron into existing patterns
 *
 * @param system        Ephaptic system
 * @param neuron_idx    Neuron to classify
 * @param patterns      Existing patterns
 * @param num_patterns  Number of patterns
 * @param best_pattern  Output: best matching pattern index (-1 if none)
 * @param match_score   Output: match score [0,1]
 * @return 0 on success
 */
NIMCP_EXPORT int ephaptic_qmc_classify_neuron(
    const nimcp_ephaptic_system_t* system,
    uint32_t neuron_idx,
    const ephaptic_sync_pattern_t* patterns,
    uint32_t num_patterns,
    int32_t* best_pattern,
    float* match_score
);

//=============================================================================
// Field Analysis API
//=============================================================================

/**
 * @brief Compute field correlation length
 *
 * @param system    Ephaptic system
 * @param corr_length Output: correlation length (mm)
 * @return 0 on success
 */
NIMCP_EXPORT int ephaptic_qmc_correlation_length(
    const nimcp_ephaptic_system_t* system,
    float* corr_length
);

/**
 * @brief Estimate field information capacity
 *
 * @param system    Ephaptic system
 * @param capacity  Output: information capacity (bits)
 * @return 0 on success
 */
NIMCP_EXPORT int ephaptic_qmc_field_capacity(
    const nimcp_ephaptic_system_t* system,
    float* capacity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_QUANTUM_BRIDGE_H */
