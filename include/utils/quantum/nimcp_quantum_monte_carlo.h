/**
 * @file nimcp_quantum_monte_carlo.h
 * @brief Quantum Monte Carlo Integration Module
 * @version 1.0.0
 * @date 2026-01-04
 *
 * WHAT: Monte Carlo methods specialized for quantum algorithms
 * WHY:  Efficient sampling, variance reduction, adaptive optimization for quantum systems
 * HOW:  Builds on nimcp_monte_carlo.h with quantum-specific callbacks and utilities
 *
 * ALGORITHMS PROVIDED:
 * - Quantum amplitude estimation via importance sampling
 * - Finite-shot measurement simulation
 * - Adaptive quantum annealing with M-H
 * - MCTS-guided quantum walk
 * - Partition function estimation
 * - Quantum state tomography sampling
 *
 * INTEGRATION TARGETS:
 * - Quantum Walk (nimcp_quantum_walk.h)
 * - Quantum Shannon (nimcp_quantum_shannon.h)
 * - Quantum Annealing (nimcp_quantum_annealing.h)
 * - Quantum Reasoning (nimcp_quantum_reasoning.h)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_QUANTUM_MONTE_CARLO_H
#define NIMCP_QUANTUM_MONTE_CARLO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/algorithms/nimcp_monte_carlo.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default number of shots for finite measurement */
#define QMC_DEFAULT_SHOTS           1000

/** Default samples for amplitude estimation */
#define QMC_DEFAULT_AMPLITUDE_SAMPLES  10000

/** Default burn-in for quantum MCMC */
#define QMC_DEFAULT_BURNIN          500

/** Default acceptance rate target for adaptive M-H */
#define QMC_TARGET_ACCEPTANCE_RATE  0.234f

/** Maximum states for direct amplitude computation */
#define QMC_MAX_DIRECT_STATES       65536

/* ============================================================================
 * Return Codes (extends nimcp_mc_result_t)
 * ============================================================================ */

typedef enum qmc_result {
    QMC_OK                  =  0,   /**< Success */
    QMC_ERROR_NULL          = -1,   /**< NULL parameter */
    QMC_ERROR_MEMORY        = -2,   /**< Memory allocation failed */
    QMC_ERROR_INVALID       = -3,   /**< Invalid parameter */
    QMC_ERROR_CONVERGENCE   = -4,   /**< Failed to converge */
    QMC_ERROR_NO_AMPLITUDE  = -5,   /**< Zero amplitude in target state */
    QMC_ERROR_TOO_MANY_STATES = -6  /**< State space too large */
} qmc_result_t;

/* ============================================================================
 * Quantum State Representation
 * ============================================================================ */

/**
 * @brief Complex amplitude for quantum states
 */
typedef struct qmc_amplitude {
    float real;
    float imag;
} qmc_amplitude_t;

/**
 * @brief Quantum state for MC operations
 */
typedef struct qmc_state {
    float* amplitudes;          /**< Real amplitudes (|psi|, not complex) */
    float* probabilities;       /**< Cached |amplitude|^2 */
    uint32_t num_states;        /**< Number of basis states */
    bool probs_cached;          /**< Whether probabilities are up to date */
} qmc_state_t;

/* ============================================================================
 * Amplitude Estimation API
 * ============================================================================ */

/**
 * @brief Configuration for amplitude estimation
 */
typedef struct qmc_amplitude_config {
    uint32_t num_samples;       /**< Number of MC samples */
    bool use_importance;        /**< Use importance sampling */
    float* proposal_dist;       /**< Proposal distribution (NULL = uniform) */
    uint32_t seed;              /**< RNG seed (0 = time-based) */
} qmc_amplitude_config_t;

/**
 * @brief Result of amplitude estimation
 */
typedef struct qmc_amplitude_result {
    float probability;          /**< Estimated |amplitude|^2 */
    float amplitude;            /**< Estimated |amplitude| */
    float variance;             /**< Estimation variance */
    float std_error;            /**< Standard error */
    float effective_samples;    /**< ESS for importance sampling */
    uint32_t samples_used;      /**< Actual samples taken */
} qmc_amplitude_result_t;

/**
 * @brief Estimate quantum amplitude via Monte Carlo
 *
 * WHAT: Estimate |<target|psi>|^2 using sampling
 * WHY:  Efficient for large state spaces where direct computation is expensive
 * HOW:  Importance sampling with proposal distribution
 *
 * @param amplitudes Array of state amplitudes
 * @param num_states Number of basis states
 * @param target_state Target state index
 * @param config Estimation configuration
 * @param result Output estimation result
 * @return QMC_OK on success
 */
qmc_result_t qmc_estimate_amplitude(
    const float* amplitudes,
    uint32_t num_states,
    uint32_t target_state,
    const qmc_amplitude_config_t* config,
    qmc_amplitude_result_t* result
);

/**
 * @brief Estimate multiple amplitudes simultaneously
 *
 * @param amplitudes Array of state amplitudes
 * @param num_states Number of basis states
 * @param target_states Array of target state indices
 * @param num_targets Number of target states
 * @param config Estimation configuration
 * @param results Output array of results
 * @return QMC_OK on success
 */
qmc_result_t qmc_estimate_amplitudes_batch(
    const float* amplitudes,
    uint32_t num_states,
    const uint32_t* target_states,
    uint32_t num_targets,
    const qmc_amplitude_config_t* config,
    qmc_amplitude_result_t* results
);

/* ============================================================================
 * Quantum Measurement API
 * ============================================================================ */

/**
 * @brief Configuration for finite-shot measurement
 */
typedef struct qmc_measurement_config {
    uint32_t num_shots;         /**< Number of measurement shots */
    bool compute_uncertainty;   /**< Compute statistical uncertainties */
    uint32_t seed;              /**< RNG seed */
} qmc_measurement_config_t;

/**
 * @brief Result of finite-shot measurement
 */
typedef struct qmc_measurement_result {
    uint32_t* counts;           /**< Measurement counts per state */
    float* frequencies;         /**< Empirical frequencies */
    float* uncertainties;       /**< Statistical uncertainties (if requested) */
    uint32_t num_states;        /**< Number of states */
    uint32_t total_shots;       /**< Total measurements taken */
    uint32_t most_frequent;     /**< Most frequently measured state */
} qmc_measurement_result_t;

/**
 * @brief Simulate finite-shot quantum measurement
 *
 * WHAT: Simulate N measurements on quantum state
 * WHY:  Model realistic quantum hardware with shot noise
 * HOW:  Sample from multinomial distribution with p_i = |amplitude_i|^2
 *
 * @param amplitudes Array of state amplitudes
 * @param num_states Number of basis states
 * @param config Measurement configuration
 * @param result Output measurement result
 * @return QMC_OK on success
 */
qmc_result_t qmc_finite_shots(
    const float* amplitudes,
    uint32_t num_states,
    const qmc_measurement_config_t* config,
    qmc_measurement_result_t* result
);

/**
 * @brief Free measurement result resources
 */
void qmc_measurement_result_free(qmc_measurement_result_t* result);

/**
 * @brief Single measurement with importance sampling
 *
 * More efficient than cumulative search for large state spaces.
 *
 * @param amplitudes Array of state amplitudes
 * @param num_states Number of basis states
 * @param proposal Proposal distribution (NULL = use |amplitude|^2)
 * @param seed RNG seed pointer (modified in place)
 * @return Measured state index
 */
uint32_t qmc_measure_importance(
    const float* amplitudes,
    uint32_t num_states,
    const float* proposal,
    uint32_t* seed
);

/* ============================================================================
 * Adaptive Quantum Annealing API
 * ============================================================================ */

/**
 * @brief Proposal adaptation strategy
 */
typedef enum qmc_proposal_strategy {
    QMC_PROPOSAL_FIXED,         /**< Fixed Gaussian proposal */
    QMC_PROPOSAL_ADAPTIVE,      /**< Adapt step size to target acceptance */
    QMC_PROPOSAL_COVARIANCE,    /**< Learn covariance from samples */
    QMC_PROPOSAL_DIFFERENTIAL   /**< Differential evolution proposal */
} qmc_proposal_strategy_t;

/**
 * @brief Configuration for adaptive annealing
 */
typedef struct qmc_anneal_config {
    float initial_temp;         /**< Starting temperature */
    float final_temp;           /**< Ending temperature */
    uint32_t num_iterations;    /**< Number of annealing steps */
    float quantum_strength;     /**< Tunneling probability [0,1] */
    qmc_proposal_strategy_t strategy;  /**< Proposal adaptation strategy */
    float target_acceptance;    /**< Target acceptance rate (default 0.234) */
    uint32_t adaptation_interval; /**< Steps between adaptation updates */
    uint32_t seed;              /**< RNG seed */
} qmc_anneal_config_t;

/**
 * @brief Result of adaptive annealing
 */
typedef struct qmc_anneal_result {
    float final_energy;         /**< Best energy found */
    float* best_state;          /**< Best state configuration */
    uint32_t dim;               /**< State dimensionality */
    float acceptance_rate;      /**< Overall acceptance rate */
    float* step_sizes;          /**< Adapted step sizes per dimension */
    uint32_t iterations_run;    /**< Actual iterations completed */
    uint32_t tunneling_events;  /**< Number of quantum tunneling events */
} qmc_anneal_result_t;

/**
 * @brief Energy function type for annealing
 */
typedef float (*qmc_energy_fn)(const float* state, uint32_t dim, void* user_data);

/**
 * @brief Run adaptive quantum annealing
 *
 * WHAT: Quantum annealing with adaptive Metropolis-Hastings
 * WHY:  Automatic tuning of proposal distribution for efficient exploration
 * HOW:  Adapt step sizes to maintain target acceptance rate
 *
 * @param energy_fn Energy function to minimize
 * @param initial_state Starting configuration
 * @param dim State dimensionality
 * @param config Annealing configuration
 * @param user_data Passed to energy function
 * @param result Output annealing result
 * @return QMC_OK on success
 */
qmc_result_t qmc_adaptive_anneal(
    qmc_energy_fn energy_fn,
    const float* initial_state,
    uint32_t dim,
    const qmc_anneal_config_t* config,
    void* user_data,
    qmc_anneal_result_t* result
);

/**
 * @brief Free annealing result resources
 */
void qmc_anneal_result_free(qmc_anneal_result_t* result);

/**
 * @brief Get default annealing configuration
 */
qmc_anneal_config_t qmc_anneal_default_config(void);

/* ============================================================================
 * Partition Function Estimation API
 * ============================================================================ */

/**
 * @brief Configuration for partition function estimation
 */
typedef struct qmc_partition_config {
    uint32_t num_samples;       /**< MCMC samples */
    uint32_t burnin;            /**< Burn-in period */
    uint32_t thinning;          /**< Thinning interval */
    float temperature;          /**< System temperature */
    uint32_t seed;              /**< RNG seed */
} qmc_partition_config_t;

/**
 * @brief Result of partition function estimation
 */
typedef struct qmc_partition_result {
    float log_Z;                /**< log(partition function) estimate */
    float free_energy;          /**< F = -T * log(Z) */
    float entropy;              /**< S = (E - F) / T */
    float mean_energy;          /**< <E> estimate */
    float energy_variance;      /**< Var(E) */
    float heat_capacity;        /**< C = Var(E) / T^2 */
    float std_error;            /**< Standard error of log(Z) */
} qmc_partition_result_t;

/**
 * @brief Estimate partition function via MCMC
 *
 * WHAT: Estimate Z = sum_i exp(-E_i / T)
 * WHY:  Required for thermodynamic properties, free energy
 * HOW:  Thermodynamic integration or Wang-Landau
 *
 * @param energy_fn Energy function
 * @param initial_state Starting configuration
 * @param dim State dimensionality
 * @param config Estimation configuration
 * @param user_data Passed to energy function
 * @param result Output partition result
 * @return QMC_OK on success
 */
qmc_result_t qmc_estimate_partition(
    qmc_energy_fn energy_fn,
    const float* initial_state,
    uint32_t dim,
    const qmc_partition_config_t* config,
    void* user_data,
    qmc_partition_result_t* result
);

/* ============================================================================
 * MCTS-Guided Quantum Walk API
 * ============================================================================ */

/**
 * @brief Coin operator types for quantum walk
 */
typedef enum qmc_coin_type {
    QMC_COIN_HADAMARD,          /**< Hadamard coin */
    QMC_COIN_GROVER,            /**< Grover diffusion coin */
    QMC_COIN_FOURIER,           /**< Fourier coin */
    QMC_COIN_IDENTITY,          /**< Identity (classical random walk) */
    QMC_COIN_CUSTOM             /**< User-defined coin */
} qmc_coin_type_t;

/**
 * @brief Configuration for MCTS-guided quantum walk
 */
typedef struct qmc_walk_config {
    uint32_t max_steps;         /**< Maximum walk steps */
    uint32_t mcts_iterations;   /**< MCTS iterations per step */
    float exploration_constant; /**< UCB exploration parameter */
    bool adaptive_coin;         /**< Use MCTS to select coin per step */
    uint32_t seed;              /**< RNG seed */
} qmc_walk_config_t;

/**
 * @brief Result of MCTS-guided quantum walk
 */
typedef struct qmc_walk_result {
    uint32_t target_reached;    /**< Whether target was reached */
    uint32_t steps_taken;       /**< Steps to reach target (or max) */
    float target_probability;   /**< Probability at target node */
    float mean_hitting_time;    /**< Estimated mean hitting time */
    qmc_coin_type_t* coin_sequence; /**< Selected coins per step */
    uint32_t num_coins;         /**< Length of coin sequence */
} qmc_walk_result_t;

/**
 * @brief MCTS-guided quantum walk to target
 *
 * WHAT: Use MCTS to select optimal coin operators for quantum walk
 * WHY:  Adaptive coin selection can reach targets faster
 * HOW:  MCTS state = walk state, actions = coin operators
 *
 * @param adjacency Graph adjacency matrix (row-major)
 * @param num_nodes Number of graph nodes
 * @param start_node Starting node
 * @param target_node Target node
 * @param config Walk configuration
 * @param result Output walk result
 * @return QMC_OK on success
 */
qmc_result_t qmc_walk_mcts(
    const uint8_t* adjacency,
    uint32_t num_nodes,
    uint32_t start_node,
    uint32_t target_node,
    const qmc_walk_config_t* config,
    qmc_walk_result_t* result
);

/**
 * @brief Free walk result resources
 */
void qmc_walk_result_free(qmc_walk_result_t* result);

/* ============================================================================
 * Entropy Estimation API
 * ============================================================================ */

/**
 * @brief Configuration for entropy estimation
 */
typedef struct qmc_entropy_config {
    uint32_t num_samples;       /**< Number of samples */
    bool use_stratified;        /**< Use stratified sampling */
    uint32_t num_strata;        /**< Number of strata (if stratified) */
    uint32_t seed;              /**< RNG seed */
} qmc_entropy_config_t;

/**
 * @brief Result of entropy estimation
 */
typedef struct qmc_entropy_result {
    float shannon_entropy;      /**< H = -sum(p * log(p)) */
    float renyi_entropy_2;      /**< H_2 = -log(sum(p^2)) */
    float min_entropy;          /**< H_inf = -log(max(p)) */
    float variance;             /**< Estimation variance */
    float std_error;            /**< Standard error */
} qmc_entropy_result_t;

/**
 * @brief Estimate entropy via Monte Carlo
 *
 * WHAT: Estimate Shannon entropy of quantum state
 * WHY:  Direct computation is O(N), MC can be faster for large N
 * HOW:  Sample from distribution, estimate -E[log(p)]
 *
 * @param probabilities Probability distribution
 * @param num_states Number of states
 * @param config Estimation configuration
 * @param result Output entropy result
 * @return QMC_OK on success
 */
qmc_result_t qmc_estimate_entropy(
    const float* probabilities,
    uint32_t num_states,
    const qmc_entropy_config_t* config,
    qmc_entropy_result_t* result
);

/* ============================================================================
 * SAT Solving with MCTS Variable Ordering
 * ============================================================================ */

/**
 * @brief Configuration for MCTS-guided SAT solving
 */
typedef struct qmc_sat_config {
    uint32_t mcts_iterations;   /**< MCTS iterations for variable ordering */
    uint32_t max_depth;         /**< Maximum search depth */
    float exploration_constant; /**< UCB exploration parameter */
    bool use_unit_propagation;  /**< Apply unit propagation */
    uint32_t seed;              /**< RNG seed */
} qmc_sat_config_t;

/**
 * @brief Result of MCTS-guided SAT solving
 */
typedef struct qmc_sat_result {
    bool satisfiable;           /**< Whether formula is satisfiable */
    uint8_t* assignment;        /**< Variable assignments (if SAT) */
    uint32_t num_variables;     /**< Number of variables */
    uint32_t nodes_explored;    /**< MCTS nodes explored */
    uint32_t propagations;      /**< Unit propagations performed */
    float confidence;           /**< Confidence in result */
} qmc_sat_result_t;

/**
 * @brief Clause representation for SAT
 */
typedef struct qmc_clause {
    int32_t* literals;          /**< Literals (positive = true, negative = false) */
    uint32_t num_literals;      /**< Number of literals in clause */
} qmc_clause_t;

/**
 * @brief CNF formula representation
 */
typedef struct qmc_cnf {
    qmc_clause_t* clauses;      /**< Array of clauses */
    uint32_t num_clauses;       /**< Number of clauses */
    uint32_t num_variables;     /**< Number of variables */
} qmc_cnf_t;

/**
 * @brief Solve SAT using MCTS-guided variable ordering
 *
 * WHAT: Use MCTS to guide DPLL variable selection
 * WHY:  Better variable ordering can exponentially reduce search space
 * HOW:  MCTS state = partial assignment, actions = variable assignments
 *
 * @param cnf CNF formula to solve
 * @param config SAT solving configuration
 * @param result Output SAT result
 * @return QMC_OK on success
 */
qmc_result_t qmc_solve_sat_mcts(
    const qmc_cnf_t* cnf,
    const qmc_sat_config_t* config,
    qmc_sat_result_t* result
);

/**
 * @brief Free SAT result resources
 */
void qmc_sat_result_free(qmc_sat_result_t* result);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Compute probabilities from amplitudes
 *
 * @param amplitudes Input amplitudes
 * @param probabilities Output probabilities (|amplitude|^2)
 * @param num_states Number of states
 */
void qmc_amplitudes_to_probabilities(
    const float* amplitudes,
    float* probabilities,
    uint32_t num_states
);

/**
 * @brief Normalize probability distribution
 *
 * @param probabilities Distribution to normalize (modified in place)
 * @param num_states Number of states
 * @return Sum before normalization
 */
float qmc_normalize_distribution(
    float* probabilities,
    uint32_t num_states
);

/**
 * @brief Compute KL divergence between distributions
 *
 * @param p First distribution
 * @param q Second distribution
 * @param num_states Number of states
 * @return KL(p || q)
 */
float qmc_kl_divergence(
    const float* p,
    const float* q,
    uint32_t num_states
);

/**
 * @brief Compute fidelity between quantum states
 *
 * @param amplitudes1 First state amplitudes
 * @param amplitudes2 Second state amplitudes
 * @param num_states Number of states
 * @return Fidelity |<psi1|psi2>|^2
 */
float qmc_fidelity(
    const float* amplitudes1,
    const float* amplitudes2,
    uint32_t num_states
);

/**
 * @brief Binary search for cumulative distribution sampling
 *
 * More efficient than linear search for large state spaces.
 *
 * @param cumulative Cumulative distribution
 * @param num_states Number of states
 * @param target Random value in [0,1)
 * @return Sampled state index
 */
uint32_t qmc_binary_sample(
    const float* cumulative,
    uint32_t num_states,
    float target
);

/**
 * @brief Build cumulative distribution from probabilities
 *
 * @param probabilities Input probabilities
 * @param cumulative Output cumulative distribution
 * @param num_states Number of states
 */
void qmc_build_cumulative(
    const float* probabilities,
    float* cumulative,
    uint32_t num_states
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_MONTE_CARLO_H */
