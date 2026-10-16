/**
 * @file nimcp_monte_carlo.h
 * @brief Monte Carlo Tree Search and Sampling Algorithms for NIMCP
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Central repository for Monte Carlo algorithms
 * WHY:  Eliminate code duplication, provide generic decision-making infrastructure
 * HOW:  Generic callback-based APIs that work with any state representation
 *
 * ALGORITHMS PROVIDED:
 * - Monte Carlo Tree Search (MCTS) with UCB1 selection
 * - Monte Carlo Integration/Estimation
 * - Importance Sampling
 * - Metropolis-Hastings MCMC
 * - Stratified Sampling
 *
 * USAGE PATTERN:
 * All algorithms use callbacks to access state/action spaces,
 * allowing them to work with any underlying data representation.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MONTE_CARLO_H
#define NIMCP_MONTE_CARLO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default exploration constant for UCB1 (sqrt(2)) */
#define MCTS_DEFAULT_EXPLORATION  NIMCP_SQRT2_F

/** Default discount factor for value backpropagation */
#define MCTS_DEFAULT_DISCOUNT     0.99f

/** Maximum nodes for stack-allocated tree storage */
#define MCTS_MAX_STACK_NODES      256

/** Default number of MCTS iterations */
#define MCTS_DEFAULT_ITERATIONS   100

/** Default maximum tree depth */
#define MCTS_DEFAULT_MAX_DEPTH    50

/** Default Monte Carlo samples for estimation */
#define MC_DEFAULT_SAMPLES        10000

/** Default burn-in for MCMC methods */
#define MC_DEFAULT_BURNIN         1000

/* ============================================================================
 * Return Codes
 * ============================================================================ */

typedef enum nimcp_mc_result {
    NIMCP_MC_OK                =  0,   /**< Success */
    NIMCP_MC_ERROR_NULL        = -1,   /**< NULL parameter */
    NIMCP_MC_ERROR_MEMORY      = -2,   /**< Memory allocation failed */
    NIMCP_MC_ERROR_CALLBACK    = -3,   /**< Callback returned error */
    NIMCP_MC_ERROR_INVALID     = -4,   /**< Invalid parameter */
    NIMCP_MC_ERROR_MAX_NODES   = -5,   /**< Tree exceeded max nodes */
    NIMCP_MC_ERROR_MAX_DEPTH   = -6,   /**< Tree exceeded max depth */
    NIMCP_MC_ERROR_NO_ACTIONS  = -7,   /**< No valid actions from state */
    NIMCP_MC_ERROR_CONVERGENCE = -8    /**< Failed to converge */
} nimcp_mc_result_t;

/* ============================================================================
 * Monte Carlo Sampling Method Selection
 * ============================================================================ */

typedef enum mc_sampling_method {
    MC_SAMPLE_UNIFORM,              /**< Simple uniform random sampling */
    MC_SAMPLE_IMPORTANCE,           /**< Importance sampling with weights */
    MC_SAMPLE_STRATIFIED,           /**< Stratified sampling for variance reduction */
    MC_SAMPLE_METROPOLIS_HASTINGS   /**< Metropolis-Hastings MCMC */
} mc_sampling_method_t;

/* ============================================================================
 * GPU Acceleration Configuration
 * ============================================================================ */

/**
 * @brief GPU acceleration mode for MC algorithms
 */
typedef enum mc_gpu_mode {
    MC_GPU_DISABLED,                /**< CPU only (default) */
    MC_GPU_AUTO,                    /**< Auto-select based on problem size */
    MC_GPU_PREFERRED,               /**< Use GPU if available */
    MC_GPU_REQUIRED                 /**< Fail if GPU unavailable */
} mc_gpu_mode_t;

/** Forward declaration for GPU context (optional dependency) */
struct nimcp_gpu_context_s;

/**
 * @brief GPU configuration for MC algorithms
 */
typedef struct mc_gpu_config {
    mc_gpu_mode_t mode;             /**< GPU mode */
    struct nimcp_gpu_context_s* ctx; /**< GPU context (NULL = auto) */
    uint32_t min_samples_for_gpu;   /**< Minimum samples to use GPU (default 10000) */
    uint32_t threads_per_block;     /**< CUDA threads per block (default 256) */
} mc_gpu_config_t;

/* ============================================================================
 * MCTS Selection Policy
 * ============================================================================ */

typedef enum mcts_selection_policy {
    MCTS_SELECT_UCB1,               /**< UCB1: Q + c * sqrt(ln(N)/n) */
    MCTS_SELECT_UCB1_TUNED,         /**< UCB1-Tuned with variance estimate */
    MCTS_SELECT_PUCT,               /**< PUCT: Q + c * P * sqrt(N)/(1+n) */
    MCTS_SELECT_EPSILON_GREEDY      /**< Epsilon-greedy selection */
} mcts_selection_policy_t;

/* ============================================================================
 * MCTS Callback Types
 * ============================================================================ */

/**
 * @brief Callback to get number of available actions from a state
 *
 * @param state       Opaque pointer to current state
 * @param user_data   User-provided context pointer
 * @return Number of actions, or 0 if terminal state
 */
typedef uint32_t (*mcts_get_action_count_fn)(
    const void* state,
    void* user_data
);

/**
 * @brief Callback to get a specific action ID
 *
 * @param state       Opaque pointer to current state
 * @param action_idx  Index of the action (0 to action_count-1)
 * @param user_data   User-provided context pointer
 * @return Action ID, or UINT32_MAX on error
 */
typedef uint32_t (*mcts_get_action_fn)(
    const void* state,
    uint32_t action_idx,
    void* user_data
);

/**
 * @brief Callback to apply an action and get resulting state
 *
 * The callback should allocate and return a new state object.
 * The caller is responsible for freeing it via free_state callback.
 *
 * @param state       Opaque pointer to current state
 * @param action      Action ID to apply
 * @param user_data   User-provided context pointer
 * @return Pointer to new state, or NULL on error
 */
typedef void* (*mcts_apply_action_fn)(
    const void* state,
    uint32_t action,
    void* user_data
);

/**
 * @brief Callback to evaluate a state (heuristic value)
 *
 * Should return a value estimate for the state, typically in [0, 1].
 * This is used during rollout/simulation phase.
 *
 * @param state       Opaque pointer to state to evaluate
 * @param user_data   User-provided context pointer
 * @return Heuristic value, higher is better
 */
typedef float (*mcts_evaluate_fn)(
    const void* state,
    void* user_data
);

/**
 * @brief Callback to check if a state is terminal
 *
 * @param state       Opaque pointer to state
 * @param user_data   User-provided context pointer
 * @return true if state is terminal (no more actions possible)
 */
typedef bool (*mcts_is_terminal_fn)(
    const void* state,
    void* user_data
);

/**
 * @brief Callback to free a state object
 *
 * Called to clean up states created by apply_action callback.
 *
 * @param state       Opaque pointer to state to free
 * @param user_data   User-provided context pointer
 */
typedef void (*mcts_free_state_fn)(
    void* state,
    void* user_data
);

/**
 * @brief Callback to clone a state object
 *
 * Creates a deep copy of the state for tree storage.
 *
 * @param state       Opaque pointer to state to clone
 * @param user_data   User-provided context pointer
 * @return Pointer to cloned state, or NULL on error
 */
typedef void* (*mcts_clone_state_fn)(
    const void* state,
    void* user_data
);

/**
 * @brief Callback to get prior probability for an action (for PUCT)
 *
 * Optional callback for PUCT selection policy.
 *
 * @param state       Opaque pointer to current state
 * @param action      Action ID
 * @param user_data   User-provided context pointer
 * @return Prior probability in [0, 1]
 */
typedef float (*mcts_get_prior_fn)(
    const void* state,
    uint32_t action,
    void* user_data
);

/* ============================================================================
 * Monte Carlo Sampling Callback Types
 * ============================================================================ */

/**
 * @brief Callback to draw a sample from proposal distribution
 *
 * @param user_data   User-provided context pointer
 * @return Sampled value
 */
typedef float (*mc_sample_fn)(void* user_data);

/**
 * @brief Callback to compute importance weight for a sample
 *
 * Weight = target(x) / proposal(x)
 *
 * @param sample      The sampled value
 * @param user_data   User-provided context pointer
 * @return Importance weight
 */
typedef float (*mc_weight_fn)(float sample, void* user_data);

/**
 * @brief Callback for the objective function to estimate
 *
 * @param sample      The sampled value
 * @param user_data   User-provided context pointer
 * @return Function value f(x)
 */
typedef float (*mc_objective_fn)(float sample, void* user_data);

/**
 * @brief Callback for Metropolis-Hastings proposal distribution
 *
 * Given current value, propose a new value.
 *
 * @param current     Current chain value
 * @param user_data   User-provided context pointer
 * @return Proposed new value
 */
typedef float (*mc_proposal_fn)(float current, void* user_data);

/**
 * @brief Callback for target density (unnormalized)
 *
 * For Metropolis-Hastings, this is the distribution we want to sample from.
 *
 * @param x           Value to evaluate
 * @param user_data   User-provided context pointer
 * @return Unnormalized density p(x)
 */
typedef float (*mc_density_fn)(float x, void* user_data);

/* ============================================================================
 * MCTS Configuration Structure
 * ============================================================================ */

/**
 * @brief Configuration for Monte Carlo Tree Search
 */
typedef struct mcts_config {
    /* Algorithm parameters */
    uint32_t max_iterations;          /**< Number of MCTS iterations */
    uint32_t max_depth;               /**< Maximum tree depth */
    uint32_t max_nodes;               /**< Maximum nodes in tree (memory limit) */
    float exploration_constant;       /**< UCB exploration parameter (c) */
    float discount_factor;            /**< Reward discount (gamma) */
    mcts_selection_policy_t policy;   /**< Selection policy */
    float epsilon;                    /**< For epsilon-greedy policy */

    /* Required callbacks */
    mcts_get_action_count_fn get_action_count;  /**< Get available actions */
    mcts_get_action_fn get_action;              /**< Get action by index */
    mcts_apply_action_fn apply_action;          /**< Apply action to state */
    mcts_evaluate_fn evaluate;                  /**< Evaluate state value */
    mcts_is_terminal_fn is_terminal;            /**< Check if terminal */
    mcts_free_state_fn free_state;              /**< Free state memory */
    mcts_clone_state_fn clone_state;            /**< Clone state */

    /* Optional callbacks */
    mcts_get_prior_fn get_prior;                /**< Get action prior (for PUCT) */

    /* User context */
    void* user_data;                  /**< Passed to all callbacks */

    /* Random seed (0 = use time-based seed) */
    uint32_t seed;
} mcts_config_t;

/**
 * @brief Result of MCTS search
 */
typedef struct mcts_result {
    uint32_t best_action;             /**< Recommended action ID */
    float best_value;                 /**< Expected value of best action */
    uint32_t* action_visits;          /**< Visit count per root action */
    float* action_values;             /**< Q-value per root action */
    uint32_t num_actions;             /**< Number of root actions */
    uint32_t nodes_created;           /**< Total nodes in tree */
    uint32_t iterations_completed;    /**< Actual iterations run */
    uint32_t max_depth_reached;       /**< Deepest node in tree */
} mcts_result_t;

/* ============================================================================
 * Monte Carlo Sampling Configuration Structure
 * ============================================================================ */

/**
 * @brief Configuration for Monte Carlo estimation/sampling
 */
typedef struct mc_config {
    mc_sampling_method_t method;      /**< Sampling method */
    uint32_t num_samples;             /**< Number of samples to draw */
    uint32_t burnin;                  /**< Burn-in period for MCMC */
    float tolerance;                  /**< Convergence tolerance */
    bool store_samples;               /**< Whether to store raw samples */

    /* Callbacks (set based on method) */
    mc_sample_fn sampler;             /**< Draw from proposal */
    mc_weight_fn weight;              /**< Importance weight (for importance sampling) */
    mc_objective_fn objective;        /**< Function to estimate E[f(X)] */
    mc_proposal_fn proposal;          /**< Proposal for M-H */
    mc_density_fn density;            /**< Target density for M-H */

    /* User context */
    void* user_data;

    /* Random seed (0 = use time-based seed) */
    uint32_t seed;

    /* Stratified sampling parameters */
    uint32_t num_strata;              /**< Number of strata (for stratified) */

    /* GPU acceleration (optional) */
    mc_gpu_config_t gpu;              /**< GPU configuration (mode=MC_GPU_DISABLED by default) */
} mc_config_t;

/**
 * @brief Result of Monte Carlo estimation
 */
typedef struct mc_result {
    float estimate;                   /**< E[f(X)] estimate */
    float variance;                   /**< Sample variance */
    float std_error;                  /**< Standard error = sqrt(var/n) */
    float* samples;                   /**< Raw samples (if store_samples=true) */
    uint32_t num_samples;             /**< Actual samples used */
    float acceptance_rate;            /**< For MCMC: acceptance ratio */
    float effective_sample_size;      /**< ESS for importance sampling */
} mc_result_t;

/* ============================================================================
 * MCTS Public API
 * ============================================================================ */

/**
 * @brief Perform Monte Carlo Tree Search from initial state
 *
 * WHAT: Find best action via tree search with random rollouts
 * WHY:  Decision-making under uncertainty, planning
 * HOW:  Selection -> Expansion -> Simulation -> Backpropagation loop
 *
 * @param config        MCTS configuration with callbacks
 * @param initial_state Starting state (not modified)
 * @param result        Output: search results
 * @return NIMCP_MC_OK on success, or error code
 *
 * @note The result struct must be freed with mcts_result_free()
 */
nimcp_mc_result_t mcts_search(
    const mcts_config_t* config,
    const void* initial_state,
    mcts_result_t* result
);

/**
 * @brief Free MCTS result resources
 *
 * @param result  Result struct to free
 */
void mcts_result_free(mcts_result_t* result);

/**
 * @brief Initialize MCTS config with defaults
 *
 * Sets reasonable defaults for all parameters.
 * Caller must still set callbacks.
 *
 * @param config  Config struct to initialize
 */
void mcts_config_init(mcts_config_t* config);

/* ============================================================================
 * Monte Carlo Sampling Public API
 * ============================================================================ */

/**
 * @brief Perform Monte Carlo estimation
 *
 * WHAT: Estimate E[f(X)] using sampling
 * WHY:  Integration, expectation computation, posterior estimation
 * HOW:  Draw samples, compute function values, average
 *
 * @param config  Sampling configuration
 * @param result  Output: estimation results
 * @return NIMCP_MC_OK on success, or error code
 *
 * @note The result struct must be freed with mc_result_free()
 */
nimcp_mc_result_t mc_estimate(
    const mc_config_t* config,
    mc_result_t* result
);

/**
 * @brief Free MC result resources
 *
 * @param result  Result struct to free
 */
void mc_result_free(mc_result_t* result);

/**
 * @brief Initialize MC config with defaults
 *
 * @param config  Config struct to initialize
 */
void mc_config_init(mc_config_t* config);

/* ============================================================================
 * UCB Calculation Utilities
 * ============================================================================ */

/**
 * @brief Compute UCB1 value for node selection
 *
 * UCB1 = Q + c * sqrt(ln(N) / n)
 *
 * @param q_value        Mean value of node (Q)
 * @param visit_count    Number of visits to node (n)
 * @param parent_visits  Number of visits to parent (N)
 * @param c              Exploration constant
 * @return UCB1 value
 */
float mcts_compute_ucb1(
    float q_value,
    uint32_t visit_count,
    uint32_t parent_visits,
    float c
);

/**
 * @brief Compute PUCT value for node selection
 *
 * PUCT = Q + c * P * sqrt(N) / (1 + n)
 *
 * @param q_value        Mean value of node (Q)
 * @param visit_count    Number of visits to node (n)
 * @param parent_visits  Number of visits to parent (N)
 * @param prior          Prior probability (P)
 * @param c              Exploration constant
 * @return PUCT value
 */
float mcts_compute_puct(
    float q_value,
    uint32_t visit_count,
    uint32_t parent_visits,
    float prior,
    float c
);

/* ============================================================================
 * Random Number Generation Utilities (Thread-Safe)
 * ============================================================================ */

/**
 * @brief Generate uniform random float in [0, 1)
 *
 * Thread-safe using provided seed state.
 *
 * @param seed  Pointer to seed state (modified in place)
 * @return Random value in [0, 1)
 */
float mc_random_uniform(uint32_t* seed);

/**
 * @brief Generate uniform random integer in [0, max)
 *
 * @param seed  Pointer to seed state
 * @param max   Exclusive upper bound
 * @return Random integer in [0, max)
 */
uint32_t mc_random_int(uint32_t* seed, uint32_t max);

/**
 * @brief Generate normal (Gaussian) random value
 *
 * Uses Box-Muller transform.
 *
 * @param seed    Pointer to seed state
 * @param mean    Distribution mean
 * @param stddev  Distribution standard deviation
 * @return Random value from N(mean, stddev^2)
 */
float mc_random_normal(uint32_t* seed, float mean, float stddev);

/**
 * @brief Random weighted choice from discrete distribution
 *
 * @param seed     Pointer to seed state
 * @param weights  Array of (unnormalized) weights
 * @param n        Number of choices
 * @return Selected index in [0, n)
 */
uint32_t mc_random_choice(uint32_t* seed, const float* weights, uint32_t n);

/**
 * @brief Initialize seed from system time
 *
 * @return Seed value based on current time
 */
uint32_t mc_seed_from_time(void);

/* ============================================================================
 * Array Shuffling Utilities
 * ============================================================================ */

/**
 * @brief Fisher-Yates shuffle for uint32_t array
 *
 * Randomly permutes array in place.
 *
 * @param array  Array to shuffle
 * @param n      Number of elements
 * @param seed   Pointer to seed state
 */
void mc_shuffle_u32(uint32_t* array, uint32_t n, uint32_t* seed);

/**
 * @brief Fisher-Yates shuffle for generic array
 *
 * @param array  Array to shuffle
 * @param n      Number of elements
 * @param size   Size of each element in bytes
 * @param seed   Pointer to seed state
 */
void mc_shuffle(void* array, uint32_t n, size_t size, uint32_t* seed);

/* ============================================================================
 * Statistical Utilities
 * ============================================================================ */

/**
 * @brief Compute mean of float array
 *
 * @param values  Array of values
 * @param n       Number of values
 * @return Mean value
 */
float mc_mean(const float* values, uint32_t n);

/**
 * @brief Compute variance of float array
 *
 * @param values  Array of values
 * @param n       Number of values
 * @param mean    Pre-computed mean (or compute if NAN)
 * @return Sample variance (n-1 denominator)
 */
float mc_variance(const float* values, uint32_t n, float mean);

/**
 * @brief Compute standard error of mean
 *
 * @param variance  Sample variance
 * @param n         Number of samples
 * @return Standard error = sqrt(variance / n)
 */
float mc_std_error(float variance, uint32_t n);

/**
 * @brief Compute effective sample size for importance sampling
 *
 * ESS = (sum(w))^2 / sum(w^2)
 *
 * @param weights  Array of importance weights
 * @param n        Number of samples
 * @return Effective sample size
 */
float mc_effective_sample_size(const float* weights, uint32_t n);

/* ============================================================================
 * GPU Acceleration Utilities
 * ============================================================================ */

/**
 * @brief Initialize GPU config with defaults
 *
 * @param gpu_config  Config struct to initialize
 */
void mc_gpu_config_init(mc_gpu_config_t* gpu_config);

/**
 * @brief Check if GPU acceleration is available
 *
 * @return true if CUDA is available and properly initialized
 */
bool mc_gpu_available(void);

/**
 * @brief Check if GPU should be used for given config and sample count
 *
 * @param gpu_config  GPU configuration
 * @param num_samples Number of samples for the operation
 * @return true if GPU should be used
 */
bool mc_should_use_gpu(const mc_gpu_config_t* gpu_config, uint32_t num_samples);

/**
 * @brief Get GPU acceleration status string
 *
 * @return Human-readable status string
 */
const char* mc_gpu_status_string(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MONTE_CARLO_H */
