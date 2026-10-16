/**
 * @file nimcp_quantum_mcts.h
 * @brief Quantum Monte Carlo Tree Search for Mathematical Planning
 *
 * Implements a hybrid classical-quantum MCTS algorithm that leverages
 * quantum Monte Carlo methods for enhanced rollouts, amplitude estimation
 * and exploration. Designed for mathematical theorem proving and planning.
 *
 * Key Features:
 * - Quantum-enhanced rollouts using QMC importance sampling
 * - Amplitude-based value estimation
 * - Quantum exploration bonus via superposition
 * - Hybrid classical/quantum simulation modes
 * - Integration with FEP planning for active inference
 *
 * Biological Basis:
 * - Prefrontal cortex planning with uncertainty
 * - Hippocampal replay for value estimation
 * - Dopaminergic reward prediction error
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_QUANTUM_MCTS_H
#define NIMCP_QUANTUM_MCTS_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_QUANTUM_MCTS         0x0395

/** Default fraction of rollouts using quantum methods */
#define QMCTS_DEFAULT_QUANTUM_FRACTION  0.3f

/** Default number of QMC shots */
#define QMCTS_DEFAULT_SHOTS             1000

/** Maximum cached quantum states */
#define QMCTS_MAX_CACHED_STATES         10000

/** Default exploration constant */
#define QMCTS_DEFAULT_EXPLORATION       1.414f  /* sqrt(2) */

/** Maximum planning horizon */
#define QMCTS_MAX_HORIZON               64

/** Maximum simulations per planning call */
#define QMCTS_MAX_SIMULATIONS           100000

/** Default number of simulations */
#define QMCTS_DEFAULT_SIMULATIONS       1000

/* ============================================================================
 * Quantum Enhancement Types
 * ============================================================================ */

/**
 * @brief Types of quantum enhancement for MCTS
 */
typedef enum {
    QMCTS_ENHANCE_NONE = 0,          /**< Pure classical MCTS */
    QMCTS_ENHANCE_ROLLOUT,           /**< Quantum-enhanced rollouts */
    QMCTS_ENHANCE_SELECTION,         /**< Quantum amplitude for UCB */
    QMCTS_ENHANCE_EXPANSION,         /**< Quantum superposition of children */
    QMCTS_ENHANCE_BACKPROP,          /**< Quantum averaging for backprop */
    QMCTS_ENHANCE_FULL               /**< All quantum enhancements */
} qmcts_enhancement_t;

/**
 * @brief State encoding methods for quantum representation
 */
typedef enum {
    QMCTS_ENCODE_DIRECT = 0,         /**< Direct state-to-amplitude mapping */
    QMCTS_ENCODE_FEATURE,            /**< Feature vector encoding */
    QMCTS_ENCODE_POSITIONAL,         /**< Positional encoding */
    QMCTS_ENCODE_GRAPH               /**< Graph structure encoding */
} qmcts_state_encoding_t;

/* ============================================================================
 * QMCTS Node Structure
 * ============================================================================ */

/**
 * @brief Quantum-enhanced MCTS node
 */
typedef struct qmcts_node {
    uint32_t node_id;                /**< Unique node identifier */
    uint32_t parent_id;              /**< Parent node ID (0 = root) */
    uint32_t* children;              /**< Array of child node IDs */
    uint32_t num_children;           /**< Number of children */
    uint32_t children_capacity;      /**< Capacity of children array */

    /* Classical MCTS statistics */
    uint32_t visit_count;            /**< Number of visits */
    float q_value;                   /**< Cumulative Q value */
    float mean_value;                /**< Mean value = q_value / visit_count */

    /* Quantum-enhanced statistics */
    qmc_amplitude_t amplitude;       /**< Quantum amplitude for this node */
    float quantum_value;             /**< Value from quantum estimation */
    float quantum_uncertainty;       /**< Uncertainty from quantum methods */
    float exploration_bonus;         /**< Quantum exploration bonus */

    /* State information */
    float* state;                    /**< State vector */
    uint32_t state_dim;              /**< State dimensionality */
    int action_id;                   /**< Action that led to this node */
    bool is_terminal;                /**< Whether this is a terminal state */
    bool is_expanded;                /**< Whether node has been expanded */

    /* Proof/mathematical context */
    uint32_t proof_depth;            /**< Depth in proof tree */
    float proof_confidence;          /**< Confidence in proof path */

    /* Timing */
    uint64_t created_time_us;        /**< Creation timestamp */
    uint64_t last_visit_us;          /**< Last visit timestamp */
} qmcts_node_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for Quantum MCTS
 */
typedef struct quantum_mcts_config {
    /* Classical MCTS parameters */
    uint32_t num_simulations;        /**< Number of simulations per plan */
    float exploration_constant;      /**< UCB exploration parameter */
    uint32_t planning_horizon;       /**< Maximum depth to explore */
    float discount_factor;           /**< Gamma for future rewards */

    /* Quantum enhancement parameters */
    qmcts_enhancement_t enhancement; /**< Type of quantum enhancement */
    bool enable_quantum_rollouts;    /**< Use QMC for rollouts */
    bool enable_amplitude_estimation; /**< Use quantum amplitude estimation */
    bool enable_quantum_sampling;    /**< Use quantum Boltzmann sampling */
    uint32_t qmc_shots;              /**< Measurement shots for QMC */
    float quantum_exploration_boost; /**< Extra exploration via quantum */

    /* Hybrid classical-quantum */
    float quantum_fraction;          /**< Fraction of quantum rollouts [0,1] */
    uint32_t classical_simulations;  /**< Classical simulations */
    uint32_t quantum_simulations;    /**< Quantum simulations */

    /* State encoding */
    qmcts_state_encoding_t encoding; /**< How to encode states for quantum */
    uint32_t max_state_dim;          /**< Maximum state dimensionality */

    /* Caching */
    bool enable_caching;             /**< Cache quantum computations */
    uint32_t max_cached_states;      /**< Maximum states to cache */

    /* Integration */
    bool enable_fep_integration;     /**< Integrate with FEP planning */
    bool enable_bio_async;           /**< Enable async messaging */

    /* Modulation */
    float temperature;               /**< Softmax temperature for action selection */
    float atp_sensitivity;           /**< Sensitivity to metabolic state */
} quantum_mcts_config_t;

/* ============================================================================
 * Plan Structure
 * ============================================================================ */

/**
 * @brief Plan output from Quantum MCTS
 */
typedef struct qmcts_plan {
    int* actions;                    /**< Action sequence */
    uint32_t num_actions;            /**< Number of actions */
    float expected_value;            /**< Expected cumulative reward */
    float uncertainty;               /**< Uncertainty in value estimate */
    float* step_values;              /**< Value at each step */
    float* step_uncertainties;       /**< Uncertainty at each step */
    float quantum_confidence;        /**< Confidence from quantum estimation */
} qmcts_plan_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for Quantum MCTS
 */
typedef struct quantum_mcts_stats {
    /* Tree statistics */
    uint32_t total_nodes;            /**< Total nodes created */
    uint32_t max_depth_reached;      /**< Maximum depth explored */
    uint32_t terminal_nodes;         /**< Number of terminal nodes */

    /* Simulation statistics */
    uint64_t total_simulations;      /**< Total simulations run */
    uint64_t classical_simulations;  /**< Classical simulations */
    uint64_t quantum_simulations;    /**< Quantum-enhanced simulations */

    /* Quantum statistics */
    uint64_t qmc_shots_used;         /**< Total QMC shots */
    float avg_quantum_value;         /**< Average quantum value estimate */
    float avg_quantum_uncertainty;   /**< Average quantum uncertainty */
    uint32_t cache_hits;             /**< Quantum state cache hits */
    uint32_t cache_misses;           /**< Quantum state cache misses */

    /* Performance */
    uint64_t total_planning_time_us; /**< Total time in planning */
    float avg_planning_time_us;      /**< Average planning time */
    uint64_t total_rollout_time_us;  /**< Total rollout time */
    float avg_rollout_time_us;       /**< Average rollout time */

    /* Value statistics */
    float best_value_found;          /**< Best value discovered */
    float avg_root_value;            /**< Average value at root */
    float value_variance;            /**< Variance in value estimates */
} quantum_mcts_stats_t;

/* ============================================================================
 * Quantum State Cache Entry
 * ============================================================================ */

/**
 * @brief Cache entry for quantum state computations
 */
typedef struct qmcts_cache_entry {
    uint64_t state_hash;             /**< Hash of state */
    qmc_amplitude_result_t amplitude; /**< Cached amplitude result */
    float value_estimate;            /**< Cached value estimate */
    uint64_t timestamp_us;           /**< When cached */
    uint32_t access_count;           /**< Number of accesses */
} qmcts_cache_entry_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to Quantum MCTS system
 */
typedef struct quantum_mcts quantum_mcts_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief State transition function type
 */
typedef nimcp_error_t (*qmcts_transition_fn)(
    const float* state,
    uint32_t state_dim,
    int action,
    float* next_state,
    float* reward,
    bool* terminal,
    void* user_data);

/**
 * @brief Reward/value function type
 */
typedef float (*qmcts_value_fn)(
    const float* state,
    uint32_t state_dim,
    void* user_data);

/**
 * @brief Action enumeration function type
 */
typedef uint32_t (*qmcts_action_fn)(
    const float* state,
    uint32_t state_dim,
    int* actions,
    uint32_t max_actions,
    void* user_data);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create Quantum MCTS system with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return QMCTS handle or NULL on failure
 */
NIMCP_API quantum_mcts_t* quantum_mcts_create(
    const quantum_mcts_config_t* config);

/**
 * @brief Destroy Quantum MCTS system
 *
 * @param qmcts System to destroy
 */
NIMCP_API void quantum_mcts_destroy(quantum_mcts_t* qmcts);

/**
 * @brief Reset tree state, keeping configuration
 *
 * @param qmcts The QMCTS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_reset(quantum_mcts_t* qmcts);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_get_default_config(
    quantum_mcts_config_t* config);

/* ============================================================================
 * Setup Functions
 * ============================================================================ */

/**
 * @brief Set environment callbacks
 *
 * @param qmcts The QMCTS system
 * @param transition State transition function
 * @param value Value/reward function
 * @param actions Action enumeration function
 * @param user_data Passed to callbacks
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_set_environment(
    quantum_mcts_t* qmcts,
    qmcts_transition_fn transition,
    qmcts_value_fn value,
    qmcts_action_fn actions,
    void* user_data);

/**
 * @brief Link to existing FEP planning system
 *
 * @param qmcts The QMCTS system
 * @param fep_planner FEP planning system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_link_fep(
    quantum_mcts_t* qmcts,
    fep_planning_system_t* fep_planner);

/**
 * @brief Link to quantum annealer for optimization
 *
 * @param qmcts The QMCTS system
 * @param annealer Quantum annealer
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_link_annealer(
    quantum_mcts_t* qmcts,
    quantum_annealer_t* annealer);

/* ============================================================================
 * Planning Functions
 * ============================================================================ */

/**
 * @brief Generate plan from current state
 *
 * Main planning function that runs MCTS with quantum enhancements.
 *
 * @param qmcts The QMCTS system
 * @param state Current state
 * @param state_dim State dimensionality
 * @param plan Output plan
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_plan(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    qmcts_plan_t* plan);

/**
 * @brief Select best action from current state
 *
 * @param qmcts The QMCTS system
 * @param state Current state
 * @param state_dim State dimensionality
 * @param action Output best action
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_select_action(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    int* action);

/**
 * @brief Run specified number of simulations
 *
 * @param qmcts The QMCTS system
 * @param num_simulations Number of simulations
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_simulate(
    quantum_mcts_t* qmcts,
    uint32_t num_simulations);

/* ============================================================================
 * Quantum-Enhanced Functions
 * ============================================================================ */

/**
 * @brief Perform quantum-enhanced rollout
 *
 * Uses QMC methods for more efficient value estimation.
 *
 * @param qmcts The QMCTS system
 * @param node_id Node to rollout from
 * @param fep FEP system for value estimation (optional)
 * @return Estimated value
 */
NIMCP_API float quantum_mcts_rollout(
    quantum_mcts_t* qmcts,
    uint32_t node_id,
    void* fep);

/**
 * @brief Estimate node value using quantum amplitude estimation
 *
 * @param qmcts The QMCTS system
 * @param state State to evaluate
 * @param state_dim State dimensionality
 * @param result Output amplitude result
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_estimate_value(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    qmc_amplitude_result_t* result);

/**
 * @brief Compute quantum exploration bonus for UCB
 *
 * @param qmcts The QMCTS system
 * @param node Node to compute bonus for
 * @return Exploration bonus
 */
NIMCP_API float quantum_mcts_exploration_bonus(
    quantum_mcts_t* qmcts,
    const qmcts_node_t* node);

/**
 * @brief Select child using quantum-enhanced UCB
 *
 * Uses amplitude-weighted selection instead of pure UCB1.
 *
 * @param qmcts The QMCTS system
 * @param node_id Parent node
 * @return Selected child node ID
 */
NIMCP_API uint32_t quantum_mcts_select_child(
    quantum_mcts_t* qmcts,
    uint32_t node_id);

/* ============================================================================
 * Tree Management
 * ============================================================================ */

/**
 * @brief Get node by ID
 *
 * @param qmcts The QMCTS system
 * @param node_id Node ID
 * @return Pointer to node or NULL
 */
NIMCP_API const qmcts_node_t* quantum_mcts_get_node(
    const quantum_mcts_t* qmcts,
    uint32_t node_id);

/**
 * @brief Get root node
 *
 * @param qmcts The QMCTS system
 * @return Pointer to root node or NULL
 */
NIMCP_API const qmcts_node_t* quantum_mcts_get_root(
    const quantum_mcts_t* qmcts);

/**
 * @brief Get best child of a node
 *
 * @param qmcts The QMCTS system
 * @param node_id Parent node
 * @return Best child node ID or UINT32_MAX
 */
NIMCP_API uint32_t quantum_mcts_get_best_child(
    const quantum_mcts_t* qmcts,
    uint32_t node_id);

/**
 * @brief Get tree depth
 *
 * @param qmcts The QMCTS system
 * @return Maximum tree depth
 */
NIMCP_API uint32_t quantum_mcts_get_depth(const quantum_mcts_t* qmcts);

/* ============================================================================
 * FEP Integration
 * ============================================================================ */

/**
 * @brief Use FEP expected free energy as MCTS value
 *
 * Integrates active inference with tree search.
 *
 * @param qmcts The QMCTS system
 * @param state Current state
 * @param state_dim State dimensionality
 * @return Expected free energy (lower is better)
 */
NIMCP_API float quantum_mcts_fep_value(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim);

/**
 * @brief Select action using quantum-enhanced active inference
 *
 * @param qmcts The QMCTS system
 * @param belief_state Current belief state
 * @param belief_dim Belief state dimensionality
 * @param action Output selected action
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_active_inference_action(
    quantum_mcts_t* qmcts,
    const float* belief_state,
    uint32_t belief_dim,
    int* action);

/* ============================================================================
 * Plan Management
 * ============================================================================ */

/**
 * @brief Initialize plan structure
 *
 * @param plan Plan to initialize
 * @param max_actions Maximum actions
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_plan_init(
    qmcts_plan_t* plan,
    uint32_t max_actions);

/**
 * @brief Clean up plan structure
 *
 * @param plan Plan to clean up
 */
NIMCP_API void quantum_mcts_plan_cleanup(qmcts_plan_t* plan);

/* ============================================================================
 * Modulation
 * ============================================================================ */

/**
 * @brief Apply ATP level modulation
 *
 * Low ATP reduces quantum simulation budget.
 *
 * @param qmcts The QMCTS system
 * @param atp_level ATP level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_modulate_atp(
    quantum_mcts_t* qmcts,
    float atp_level);

/**
 * @brief Set exploration temperature
 *
 * Higher temperature = more exploration.
 *
 * @param qmcts The QMCTS system
 * @param temperature Temperature
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_set_temperature(
    quantum_mcts_t* qmcts,
    float temperature);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param qmcts The QMCTS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_register_bio_async(
    quantum_mcts_t* qmcts);

/**
 * @brief Unregister from bio-async router
 *
 * @param qmcts The QMCTS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_unregister_bio_async(
    quantum_mcts_t* qmcts);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param qmcts The QMCTS system
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t quantum_mcts_get_stats(
    const quantum_mcts_t* qmcts,
    quantum_mcts_stats_t* stats);

/**
 * @brief Print diagnostic information
 *
 * @param qmcts The QMCTS system
 */
NIMCP_API void quantum_mcts_print_diagnostics(const quantum_mcts_t* qmcts);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_MCTS_H */
