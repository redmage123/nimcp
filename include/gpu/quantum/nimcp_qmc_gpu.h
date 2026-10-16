/**
 * @file nimcp_qmc_gpu.h
 * @brief GPU-Accelerated Quantum Monte Carlo API
 *
 * WHAT: CUDA-accelerated Monte Carlo Simulation and MCTS
 * WHY:  Massive parallelism for MC sampling and tree search
 * HOW:  GPU kernels for parallel sampling, UCB1, and rollouts
 *
 * KEY FEATURES:
 * 1. Parallel Monte Carlo Sampling:
 *    - Thousands of samples in parallel
 *    - cuRAND-based RNG for each thread
 *    - Stratified and importance sampling
 *
 * 2. GPU-Accelerated MCTS:
 *    - Parallel UCB1 computation across nodes
 *    - Batch rollout evaluation
 *    - Tree parallelization strategies
 *
 * 3. GPU Monte Carlo Integration:
 *    - Parallel function evaluation
 *    - Variance reduction on GPU
 *    - Multi-dimensional integration
 *
 * PERFORMANCE:
 * - 100-1000x speedup for MC sampling
 * - 10-100x speedup for MCTS evaluation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_QMC_GPU_H
#define NIMCP_QMC_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Include GPU headers - they handle extern "C" internally
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief GPU Monte Carlo configuration
 */
typedef struct qmc_gpu_config_s {
    uint32_t num_samples;           /**< Number of MC samples */
    uint32_t threads_per_block;     /**< CUDA threads per block (default 256) */
    uint32_t max_blocks;            /**< Maximum number of blocks (0 = auto) */
    bool use_stratified;            /**< Use stratified sampling */
    bool use_sobol;                 /**< Use Sobol quasi-random sequences */
    uint64_t seed;                  /**< RNG seed (0 = use time) */
} qmc_gpu_config_t;

/**
 * @brief GPU MCTS configuration
 */
typedef struct qmcts_gpu_config_s {
    uint32_t num_iterations;        /**< MCTS iterations */
    uint32_t num_rollouts;          /**< Rollouts per evaluation */
    uint32_t max_depth;             /**< Maximum tree depth */
    float exploration_constant;     /**< UCB1 exploration constant (sqrt(2)) */
    uint32_t batch_size;            /**< Batch size for parallel evaluation */
    bool virtual_loss;              /**< Use virtual loss for parallelization */
    bool root_parallelization;      /**< Use root parallelization strategy */
} qmcts_gpu_config_t;

/**
 * @brief GPU SAT solver configuration
 */
typedef struct qmc_sat_gpu_config_s {
    uint32_t num_variables;         /**< Number of boolean variables */
    uint32_t num_clauses;           /**< Number of CNF clauses */
    uint32_t max_literals;          /**< Max literals per clause */
    uint32_t mcts_iterations;       /**< MCTS iterations for search */
    uint32_t random_samples;        /**< Random sampling for estimation */
    float timeout_ms;               /**< Timeout in milliseconds (0 = none) */
} qmc_sat_gpu_config_t;

//=============================================================================
// GPU Monte Carlo State
//=============================================================================

/**
 * @brief GPU RNG state handle
 */
typedef struct qmc_gpu_rng_struct* qmc_gpu_rng_t;

/**
 * @brief GPU MCTS tree handle
 */
typedef struct qmc_gpu_mcts_struct* qmc_gpu_mcts_t;

/**
 * @brief GPU SAT solver handle
 */
typedef struct qmc_gpu_sat_struct* qmc_gpu_sat_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default GPU MC configuration
 */
qmc_gpu_config_t qmc_gpu_default_config(void);

/**
 * @brief Get default GPU MCTS configuration
 */
qmcts_gpu_config_t qmcts_gpu_default_config(void);

/**
 * @brief Get default GPU SAT configuration
 */
qmc_sat_gpu_config_t qmc_sat_gpu_default_config(uint32_t num_vars, uint32_t num_clauses);

//=============================================================================
// RNG Lifecycle
//=============================================================================

/**
 * @brief Create GPU RNG state
 *
 * WHAT: Initialize cuRAND states on GPU
 * WHY:  Enable parallel random number generation
 * HOW:  Allocate curandState array, initialize with seed
 *
 * @param ctx GPU context
 * @param num_generators Number of parallel generators
 * @param seed Random seed (0 = use time)
 * @return RNG handle or NULL on failure
 */
qmc_gpu_rng_t qmc_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t num_generators,
    uint64_t seed);

/**
 * @brief Destroy GPU RNG state
 */
void qmc_gpu_rng_destroy(qmc_gpu_rng_t rng);

/**
 * @brief Reseed RNG state
 */
bool qmc_gpu_rng_reseed(qmc_gpu_rng_t rng, uint64_t seed);

//=============================================================================
// GPU Monte Carlo Sampling API
//=============================================================================

/**
 * @brief Generate uniform random samples on GPU
 *
 * WHAT: Fill tensor with uniform random values in [0, 1]
 * WHY:  Base operation for MC sampling
 * HOW:  Parallel curand_uniform calls
 *
 * @param ctx GPU context
 * @param rng RNG state
 * @param output Output tensor (device memory)
 * @return true on success
 */
bool qmc_gpu_sample_uniform(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output);

/**
 * @brief Generate normal random samples on GPU
 *
 * @param ctx GPU context
 * @param rng RNG state
 * @param output Output tensor
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 * @return true on success
 */
bool qmc_gpu_sample_normal(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output,
    float mean,
    float stddev);

/**
 * @brief Sample from categorical distribution on GPU
 *
 * WHAT: Sample indices according to probability distribution
 * WHY:  Core operation for MCTS action selection
 * HOW:  Parallel prefix sum + binary search
 *
 * @param ctx GPU context
 * @param rng RNG state
 * @param probabilities Input probabilities (device, size num_categories)
 * @param num_categories Number of categories
 * @param output Output samples (device, size num_samples)
 * @param num_samples Number of samples to draw
 * @return true on success
 */
bool qmc_gpu_sample_categorical(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    const nimcp_gpu_tensor_t* probabilities,
    uint32_t num_categories,
    nimcp_gpu_tensor_t* output,
    uint32_t num_samples);

/**
 * @brief Generate stratified samples on GPU
 *
 * WHAT: Generate samples with guaranteed coverage of sample space
 * WHY:  Lower variance than simple random sampling
 * HOW:  Divide [0,1] into strata, sample once per stratum
 *
 * @param ctx GPU context
 * @param rng RNG state
 * @param output Output tensor
 * @param num_strata Number of strata per dimension
 * @return true on success
 */
bool qmc_gpu_sample_stratified(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output,
    uint32_t num_strata);

//=============================================================================
// GPU Monte Carlo Integration API
//=============================================================================

/**
 * @brief GPU Monte Carlo integration result
 */
typedef struct qmc_gpu_integration_result_s {
    float estimate;                 /**< Integration estimate */
    float variance;                 /**< Estimate variance */
    float std_error;                /**< Standard error */
    uint32_t num_samples;           /**< Samples used */
    float time_ms;                  /**< Computation time */
} qmc_gpu_integration_result_t;

/**
 * @brief Compute Monte Carlo integration on GPU
 *
 * WHAT: Estimate integral via GPU-parallel sampling
 * WHY:  Fast integration for high-dimensional functions
 * HOW:  Parallel function evaluation, GPU reduction
 *
 * @param ctx GPU context
 * @param rng RNG state
 * @param values Function values at sample points (device tensor)
 * @param num_samples Number of samples
 * @param domain_volume Volume of integration domain
 * @param result Output result
 * @return true on success
 */
bool qmc_gpu_integrate(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    const nimcp_gpu_tensor_t* values,
    uint32_t num_samples,
    float domain_volume,
    qmc_gpu_integration_result_t* result);

/**
 * @brief Compute importance-weighted integration on GPU
 *
 * @param ctx GPU context
 * @param values Function values (device)
 * @param weights Importance weights (device)
 * @param num_samples Number of samples
 * @param result Output result
 * @return true on success
 */
bool qmc_gpu_integrate_importance(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* values,
    const nimcp_gpu_tensor_t* weights,
    uint32_t num_samples,
    qmc_gpu_integration_result_t* result);

//=============================================================================
// GPU MCTS API
//=============================================================================

/**
 * @brief MCTS node statistics (for GPU batch)
 */
typedef struct qmcts_gpu_node_stats_s {
    uint32_t visit_count;           /**< Number of visits */
    float total_value;              /**< Sum of values from rollouts */
    float mean_value;               /**< Mean value */
    float ucb1_score;               /**< UCB1 score */
    uint32_t parent_idx;            /**< Parent node index */
    uint32_t num_children;          /**< Number of children */
    uint32_t first_child_idx;       /**< Index of first child */
} qmcts_gpu_node_stats_t;

/**
 * @brief Create GPU MCTS tree
 *
 * @param ctx GPU context
 * @param config MCTS configuration
 * @param max_nodes Maximum nodes in tree
 * @return MCTS handle or NULL
 */
qmc_gpu_mcts_t qmcts_gpu_create(
    nimcp_gpu_context_t* ctx,
    const qmcts_gpu_config_t* config,
    uint32_t max_nodes);

/**
 * @brief Destroy GPU MCTS tree
 */
void qmcts_gpu_destroy(qmc_gpu_mcts_t mcts);

/**
 * @brief Reset MCTS tree for new search
 */
bool qmcts_gpu_reset(qmc_gpu_mcts_t mcts);

/**
 * @brief Compute UCB1 scores for all nodes in parallel
 *
 * WHAT: Calculate UCB1 = mean + c * sqrt(ln(parent_visits) / visits)
 * WHY:  Core MCTS selection step, highly parallelizable
 * HOW:  One thread per node, atomic updates
 *
 * @param ctx GPU context
 * @param mcts MCTS handle
 * @param exploration_constant UCB1 c parameter
 * @return true on success
 */
bool qmcts_gpu_compute_ucb1(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    float exploration_constant);

/**
 * @brief Perform batch rollouts on GPU
 *
 * WHAT: Execute multiple rollouts in parallel
 * WHY:  Main MCTS bottleneck, massively parallelizable
 * HOW:  Batch evaluation of terminal states
 *
 * @param ctx GPU context
 * @param mcts MCTS handle
 * @param rng RNG state
 * @param start_states Starting state indices
 * @param num_rollouts Number of rollouts
 * @param values Output values (device tensor)
 * @return true on success
 */
bool qmcts_gpu_batch_rollout(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    qmc_gpu_rng_t rng,
    const uint32_t* start_states,
    uint32_t num_rollouts,
    nimcp_gpu_tensor_t* values);

/**
 * @brief Backpropagate values through tree on GPU
 *
 * WHAT: Update node statistics from leaf to root
 * WHY:  Core MCTS backprop step
 * HOW:  Parallel path updates with atomic operations
 *
 * @param ctx GPU context
 * @param mcts MCTS handle
 * @param leaf_indices Leaf node indices
 * @param values Values to backpropagate
 * @param num_leaves Number of leaves
 * @return true on success
 */
bool qmcts_gpu_backpropagate(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    const uint32_t* leaf_indices,
    const float* values,
    uint32_t num_leaves);

/**
 * @brief Run complete MCTS search on GPU
 *
 * WHAT: Full MCTS with GPU acceleration
 * WHY:  End-to-end GPU-accelerated tree search
 * HOW:  Iterate: select → expand → rollout → backprop
 *
 * @param ctx GPU context
 * @param mcts MCTS handle
 * @param rng RNG state
 * @param num_iterations Number of MCTS iterations
 * @param best_action Output: best action index
 * @param best_value Output: value of best action
 * @return true on success
 */
bool qmcts_gpu_search(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    qmc_gpu_rng_t rng,
    uint32_t num_iterations,
    uint32_t* best_action,
    float* best_value);

//=============================================================================
// GPU SAT Solver API
//=============================================================================

/**
 * @brief GPU SAT solving result
 */
typedef struct qmc_sat_gpu_result_s {
    bool satisfiable;               /**< Formula is satisfiable */
    bool* assignment;               /**< Variable assignment (host memory) */
    uint32_t num_variables;         /**< Number of variables */
    float sat_probability;          /**< Estimated P(SAT) */
    uint32_t iterations_used;       /**< MCTS iterations used */
    uint32_t clauses_satisfied;     /**< Number of satisfied clauses */
    float time_ms;                  /**< Solving time */
} qmc_sat_gpu_result_t;

/**
 * @brief Create GPU SAT solver
 *
 * @param ctx GPU context
 * @param config SAT configuration
 * @return SAT solver handle or NULL
 */
qmc_gpu_sat_t qmc_sat_gpu_create(
    nimcp_gpu_context_t* ctx,
    const qmc_sat_gpu_config_t* config);

/**
 * @brief Destroy GPU SAT solver
 */
void qmc_sat_gpu_destroy(qmc_gpu_sat_t sat);

/**
 * @brief Set CNF formula for SAT solver
 *
 * WHAT: Upload CNF to GPU memory
 * WHY:  Prepare formula for GPU solving
 * HOW:  Pack clauses into GPU-friendly format
 *
 * @param sat SAT solver handle
 * @param clauses Clause literals (positive = true, negative = false)
 * @param clause_sizes Size of each clause
 * @param num_clauses Number of clauses
 * @return true on success
 */
bool qmc_sat_gpu_set_cnf(
    qmc_gpu_sat_t sat,
    const int32_t* clauses,
    const uint32_t* clause_sizes,
    uint32_t num_clauses);

/**
 * @brief Estimate SAT probability via GPU Monte Carlo
 *
 * WHAT: Estimate P(SAT) using parallel random sampling
 * WHY:  Quick satisfiability estimate without full solving
 * HOW:  Parallel assignment generation and checking
 *
 * @param ctx GPU context
 * @param sat SAT solver
 * @param rng RNG state
 * @param num_samples Number of samples
 * @param probability Output: estimated P(SAT)
 * @param variance Output: estimate variance (optional)
 * @return true on success
 */
bool qmc_sat_gpu_estimate_probability(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_sat_t sat,
    qmc_gpu_rng_t rng,
    uint32_t num_samples,
    float* probability,
    float* variance);

/**
 * @brief Solve SAT using GPU-accelerated MCTS
 *
 * WHAT: Find satisfying assignment using GPU MCTS
 * WHY:  Combine MCTS intelligence with GPU parallelism
 * HOW:  MCTS over variable assignments with GPU rollouts
 *
 * @param ctx GPU context
 * @param sat SAT solver
 * @param rng RNG state
 * @param result Output result
 * @return true on success
 */
bool qmc_sat_gpu_solve_mcts(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_sat_t sat,
    qmc_gpu_rng_t rng,
    qmc_sat_gpu_result_t* result);

/**
 * @brief Evaluate multiple assignments in parallel
 *
 * WHAT: Check clause satisfaction for batch of assignments
 * WHY:  GPU-accelerated fitness evaluation for search
 * HOW:  Parallel clause checking, count satisfied
 *
 * @param ctx GPU context
 * @param sat SAT solver
 * @param assignments Assignment matrix (num_assignments x num_vars, device)
 * @param num_assignments Number of assignments to evaluate
 * @param scores Output scores (clauses satisfied per assignment, device)
 * @return true on success
 */
bool qmc_sat_gpu_evaluate_batch(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_sat_t sat,
    const nimcp_gpu_tensor_t* assignments,
    uint32_t num_assignments,
    nimcp_gpu_tensor_t* scores);

/**
 * @brief Free SAT result resources
 */
void qmc_sat_gpu_result_free(qmc_sat_gpu_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if GPU QMC is available
 *
 * @return true if CUDA is available and initialized
 */
bool qmc_gpu_is_available(void);

/**
 * @brief Get GPU QMC version string
 */
const char* qmc_gpu_version(void);

/**
 * @brief Print GPU QMC diagnostics
 */
void qmc_gpu_print_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QMC_GPU_H */
