/**
 * @file nimcp_rand.h
 * @brief Unified Random Number Generation Module for NIMCP
 *
 * WHAT: Centralized, thread-safe RNG with quantum-enhanced sampling capabilities
 * WHY:  Replace fragmented rand() calls with a unified, thread-safe API that
 *       integrates with UMM, thread utilities, and quantum algorithms
 * HOW:  Thread-local state with multiple generator backends (LCG, quantum walk,
 *       adaptive Monte Carlo) selectable per-context
 *
 * FEATURES:
 * - Thread-safe via thread-local storage (no mutex contention)
 * - Multiple backends: Fast LCG, Box-Muller Gaussian, Pink Noise, Quantum Walk
 * - Quantum-enhanced sampling via AMCS and QMCTS integration
 * - UMM integration for large state allocations
 * - Reproducible seeding with deterministic sequences
 * - Cryptographic-quality option for security-critical use
 *
 * USAGE:
 *   // Global initialization (call once at startup)
 *   nimcp_rand_init(NULL);  // Use defaults
 *
 *   // Thread-local RNG (fast path, no allocation)
 *   float u = nimcp_rand_uniform();
 *   int32_t i = nimcp_rand_int(100);
 *   float g = nimcp_rand_normal(0.0f, 1.0f);
 *
 *   // Context-based RNG (for reproducibility)
 *   nimcp_rand_ctx_t* ctx = nimcp_rand_ctx_create(12345);
 *   float u2 = nimcp_rand_ctx_uniform(ctx);
 *   nimcp_rand_ctx_destroy(ctx);
 *
 *   // Quantum-enhanced sampling
 *   float* samples = nimcp_rand_quantum_sample(distribution, n, num_samples);
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#ifndef NIMCP_RAND_H
#define NIMCP_RAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/** Maximum number of parallel RNG contexts */
#define NIMCP_RAND_MAX_CONTEXTS 1024

/** Default seed for reproducibility (0 = use time-based seed) */
#define NIMCP_RAND_DEFAULT_SEED 0

/** Pink noise octaves for Voss-McCartney algorithm */
#define NIMCP_RAND_PINK_OCTAVES 16

/** Box-Muller cache size (2 = standard, more for batching) */
#define NIMCP_RAND_BOX_MULLER_CACHE 2

/*=============================================================================
 * TYPES
 *===========================================================================*/

/**
 * @brief RNG backend types
 */
typedef enum nimcp_rand_backend {
    NIMCP_RAND_BACKEND_LCG,         /**< Linear Congruential Generator (fast) */
    NIMCP_RAND_BACKEND_XORSHIFT,    /**< Xorshift128+ (better quality) */
    NIMCP_RAND_BACKEND_PCG,         /**< PCG family (excellent quality) */
    NIMCP_RAND_BACKEND_QUANTUM,     /**< Quantum walk-based (highest quality) */
    NIMCP_RAND_BACKEND_CSPRNG,      /**< Cryptographic (security-critical) */
    NIMCP_RAND_BACKEND_AUTO         /**< Auto-select based on use case */
} nimcp_rand_backend_t;

/**
 * @brief Distribution types for specialized sampling
 */
typedef enum nimcp_rand_distribution {
    NIMCP_RAND_DIST_UNIFORM,        /**< Uniform [0, 1) */
    NIMCP_RAND_DIST_NORMAL,         /**< Gaussian N(0, 1) */
    NIMCP_RAND_DIST_EXPONENTIAL,    /**< Exponential (rate=1) */
    NIMCP_RAND_DIST_POISSON,        /**< Poisson */
    NIMCP_RAND_DIST_PINK,           /**< 1/f pink noise */
    NIMCP_RAND_DIST_CUSTOM          /**< User-defined CDF */
} nimcp_rand_distribution_t;

/**
 * @brief Global configuration for nimcp_rand subsystem
 */
typedef struct nimcp_rand_config {
    nimcp_rand_backend_t default_backend;   /**< Default backend for new contexts */
    uint64_t global_seed;                   /**< Global seed (0 = time-based) */
    bool enable_quantum;                    /**< Enable quantum-enhanced sampling */
    bool enable_csprng;                     /**< Enable cryptographic RNG */
    bool thread_local_seeding;              /**< Auto-seed each thread uniquely */
    size_t quantum_walk_nodes;              /**< Nodes for quantum walk backend */
    float quantum_decoherence;              /**< Decoherence rate [0-1] */
} nimcp_rand_config_t;

/**
 * @brief RNG context for reproducible sequences
 */
typedef struct nimcp_rand_ctx nimcp_rand_ctx_t;

/**
 * @brief Statistics for RNG usage
 */
typedef struct nimcp_rand_stats {
    uint64_t uniform_calls;         /**< Number of uniform samples generated */
    uint64_t normal_calls;          /**< Number of Gaussian samples generated */
    uint64_t int_calls;             /**< Number of integer samples generated */
    uint64_t pink_calls;            /**< Number of pink noise samples generated */
    uint64_t quantum_calls;         /**< Number of quantum-enhanced samples */
    uint64_t context_creates;       /**< Number of contexts created */
    uint64_t context_destroys;      /**< Number of contexts destroyed */
    uint64_t seed_reseeds;          /**< Number of reseed operations */
} nimcp_rand_stats_t;

/**
 * @brief Result codes for nimcp_rand operations
 */
typedef enum nimcp_rand_result {
    NIMCP_RAND_OK = 0,              /**< Success */
    NIMCP_RAND_ERROR_NULL = -1,     /**< NULL pointer argument */
    NIMCP_RAND_ERROR_INIT = -2,     /**< Initialization failed */
    NIMCP_RAND_ERROR_MEMORY = -3,   /**< Memory allocation failed */
    NIMCP_RAND_ERROR_BACKEND = -4,  /**< Backend not available */
    NIMCP_RAND_ERROR_INVALID = -5,  /**< Invalid parameter */
    NIMCP_RAND_ERROR_QUANTUM = -6   /**< Quantum backend error */
} nimcp_rand_result_t;

/*=============================================================================
 * GLOBAL INITIALIZATION
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
nimcp_rand_config_t nimcp_rand_default_config(void);

/**
 * @brief Initialize the nimcp_rand subsystem
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_RAND_OK on success
 *
 * WHAT: Initialize global RNG state
 * WHY:  Must be called before using any nimcp_rand functions
 * HOW:  Sets up thread-local storage, quantum backend if enabled
 */
nimcp_rand_result_t nimcp_rand_init(const nimcp_rand_config_t* config);

/**
 * @brief Shutdown the nimcp_rand subsystem
 *
 * WHAT: Clean up global RNG state
 * WHY:  Release resources, particularly quantum backend
 * HOW:  Destroys all contexts, frees UMM allocations
 */
void nimcp_rand_shutdown(void);

/**
 * @brief Check if nimcp_rand is initialized
 * @return true if initialized
 */
bool nimcp_rand_is_initialized(void);

/*=============================================================================
 * THREAD-LOCAL RNG (FAST PATH)
 *===========================================================================*/

/**
 * @brief Generate uniform random float in [0, 1)
 * @return Random float
 *
 * Thread-safe via thread-local storage. No mutex overhead.
 */
float nimcp_rand_uniform(void);

/**
 * @brief Generate uniform random double in [0, 1)
 * @return Random double
 */
double nimcp_rand_uniform_double(void);

/**
 * @brief Generate random integer in [0, max)
 * @param max Exclusive upper bound
 * @return Random integer
 */
int32_t nimcp_rand_int(int32_t max);

/**
 * @brief Generate random integer in [min, max]
 * @param min Inclusive lower bound
 * @param max Inclusive upper bound
 * @return Random integer
 */
int32_t nimcp_rand_range(int32_t min, int32_t max);

/**
 * @brief Generate random uint32_t in [0, max)
 * @param max Exclusive upper bound
 * @return Random uint32_t
 */
uint32_t nimcp_rand_uint(uint32_t max);

/**
 * @brief Generate Gaussian random float N(mean, stddev^2)
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 * @return Random Gaussian float
 *
 * Uses Box-Muller transform with thread-local caching.
 */
float nimcp_rand_normal(float mean, float stddev);

/**
 * @brief Generate exponential random float with given rate
 * @param rate Rate parameter (1/mean)
 * @return Random exponential float
 */
float nimcp_rand_exponential(float rate);

/**
 * @brief Generate pink noise sample (1/f)
 * @return Pink noise sample in [-1, 1]
 *
 * Uses Voss-McCartney algorithm with 16 octaves.
 */
float nimcp_rand_pink(void);

/**
 * @brief Generate random bytes
 * @param buffer Output buffer
 * @param len Number of bytes to generate
 * @return NIMCP_RAND_OK on success
 *
 * Uses cryptographic RNG if enabled, otherwise fast LCG.
 */
nimcp_rand_result_t nimcp_rand_bytes(uint8_t* buffer, size_t len);

/*=============================================================================
 * SEEDING
 *===========================================================================*/

/**
 * @brief Set seed for current thread's RNG
 * @param seed Seed value (0 = use time-based)
 *
 * WHAT: Reseed thread-local RNG
 * WHY:  Enable reproducible sequences
 * HOW:  Resets LCG state, clears Box-Muller cache
 */
void nimcp_rand_seed(uint64_t seed);

/**
 * @brief Get current seed for current thread
 * @return Current seed value
 */
uint64_t nimcp_rand_get_seed(void);

/**
 * @brief Generate a seed from system entropy
 * @return High-entropy seed
 *
 * Uses time, address space randomization, and /dev/urandom if available.
 */
uint64_t nimcp_rand_entropy_seed(void);

/*=============================================================================
 * CONTEXT-BASED RNG (FOR REPRODUCIBILITY)
 *===========================================================================*/

/**
 * @brief Create an RNG context with specific seed
 * @param seed Initial seed (0 = use entropy)
 * @return New context or NULL on failure
 *
 * WHAT: Create isolated RNG state
 * WHY:  Enable reproducible sequences independent of thread-local state
 * HOW:  Allocates state via UMM, initializes with given seed
 */
nimcp_rand_ctx_t* nimcp_rand_ctx_create(uint64_t seed);

/**
 * @brief Create context with specific backend
 * @param seed Initial seed
 * @param backend Backend type to use
 * @return New context or NULL on failure
 */
nimcp_rand_ctx_t* nimcp_rand_ctx_create_with_backend(
    uint64_t seed,
    nimcp_rand_backend_t backend
);

/**
 * @brief Destroy an RNG context
 * @param ctx Context to destroy
 */
void nimcp_rand_ctx_destroy(nimcp_rand_ctx_t* ctx);

/**
 * @brief Clone an RNG context (same state)
 * @param ctx Context to clone
 * @return Cloned context or NULL on failure
 */
nimcp_rand_ctx_t* nimcp_rand_ctx_clone(const nimcp_rand_ctx_t* ctx);

/**
 * @brief Reseed a context
 * @param ctx Context to reseed
 * @param seed New seed
 */
void nimcp_rand_ctx_seed(nimcp_rand_ctx_t* ctx, uint64_t seed);

/* Context-based sampling functions */
float nimcp_rand_ctx_uniform(nimcp_rand_ctx_t* ctx);
double nimcp_rand_ctx_uniform_double(nimcp_rand_ctx_t* ctx);
int32_t nimcp_rand_ctx_int(nimcp_rand_ctx_t* ctx, int32_t max);
int32_t nimcp_rand_ctx_range(nimcp_rand_ctx_t* ctx, int32_t min, int32_t max);
uint32_t nimcp_rand_ctx_uint(nimcp_rand_ctx_t* ctx, uint32_t max);
float nimcp_rand_ctx_normal(nimcp_rand_ctx_t* ctx, float mean, float stddev);
float nimcp_rand_ctx_exponential(nimcp_rand_ctx_t* ctx, float rate);
float nimcp_rand_ctx_pink(nimcp_rand_ctx_t* ctx);
nimcp_rand_result_t nimcp_rand_ctx_bytes(nimcp_rand_ctx_t* ctx, uint8_t* buffer, size_t len);

/*=============================================================================
 * ARRAY/BATCH OPERATIONS
 *===========================================================================*/

/**
 * @brief Fill array with uniform random floats
 * @param out Output array
 * @param n Number of elements
 */
void nimcp_rand_uniform_array(float* out, size_t n);

/**
 * @brief Fill array with Gaussian random floats
 * @param out Output array
 * @param n Number of elements
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 */
void nimcp_rand_normal_array(float* out, size_t n, float mean, float stddev);

/**
 * @brief Fisher-Yates shuffle for uint32_t array
 * @param array Array to shuffle
 * @param n Number of elements
 */
void nimcp_rand_shuffle_u32(uint32_t* array, size_t n);

/**
 * @brief Generic Fisher-Yates shuffle
 * @param array Array to shuffle
 * @param n Number of elements
 * @param element_size Size of each element
 */
void nimcp_rand_shuffle(void* array, size_t n, size_t element_size);

/**
 * @brief Weighted random choice
 * @param weights Array of weights (need not sum to 1)
 * @param n Number of choices
 * @return Selected index
 */
uint32_t nimcp_rand_choice(const float* weights, uint32_t n);

/**
 * @brief Sample k indices without replacement
 * @param n Population size
 * @param k Sample size
 * @param out Output array of k indices
 * @return NIMCP_RAND_OK on success
 */
nimcp_rand_result_t nimcp_rand_sample(uint32_t n, uint32_t k, uint32_t* out);

/*=============================================================================
 * QUANTUM-ENHANCED SAMPLING
 *===========================================================================*/

/**
 * @brief Sample from probability distribution using quantum walk
 * @param probabilities Probability distribution (must sum to 1)
 * @param n Number of states
 * @param num_samples Number of samples to generate
 * @param samples Output array (allocated by caller)
 * @return NIMCP_RAND_OK on success
 *
 * WHAT: Quantum-enhanced importance sampling
 * WHY:  Better convergence for complex distributions
 * HOW:  Uses quantum walk to generate samples, importance weighting for correction
 */
nimcp_rand_result_t nimcp_rand_quantum_sample(
    const float* probabilities,
    uint32_t n,
    uint32_t num_samples,
    uint32_t* samples
);

/**
 * @brief Adaptive Monte Carlo sampling (AMCS)
 * @param energy_fn Energy function to minimize
 * @param initial_state Initial state vector
 * @param dim State dimensionality
 * @param num_samples Number of samples
 * @param samples Output samples (dim * num_samples floats)
 * @param user_data User data for energy function
 * @return NIMCP_RAND_OK on success
 *
 * WHAT: Adaptive Metropolis-Hastings with quantum tunneling
 * WHY:  Escape local minima, optimal acceptance rate
 * HOW:  Adapts step size to maintain 0.234 acceptance rate
 */
nimcp_rand_result_t nimcp_rand_amcs(
    float (*energy_fn)(const float* state, uint32_t dim, void* user_data),
    const float* initial_state,
    uint32_t dim,
    uint32_t num_samples,
    float* samples,
    void* user_data
);

/**
 * @brief MCTS-guided sampling for combinatorial problems
 * @param evaluate_fn Evaluation function for states
 * @param initial_state Initial state
 * @param state_size Size of state in bytes
 * @param max_iterations Maximum MCTS iterations
 * @param best_state Output best state found
 * @param user_data User data for evaluation
 * @return NIMCP_RAND_OK on success
 *
 * WHAT: Quantum Monte Carlo Tree Search
 * WHY:  Efficient exploration of discrete state spaces
 * HOW:  MCTS with quantum-enhanced rollouts
 */
nimcp_rand_result_t nimcp_rand_qmcts(
    float (*evaluate_fn)(const void* state, void* user_data),
    const void* initial_state,
    size_t state_size,
    uint32_t max_iterations,
    void* best_state,
    void* user_data
);

/*=============================================================================
 * STATISTICS AND DEBUGGING
 *===========================================================================*/

/**
 * @brief Get global RNG statistics
 * @param stats Output statistics struct
 */
void nimcp_rand_get_stats(nimcp_rand_stats_t* stats);

/**
 * @brief Reset global RNG statistics
 */
void nimcp_rand_reset_stats(void);

/**
 * @brief Get backend name as string
 * @param backend Backend type
 * @return Backend name string
 */
const char* nimcp_rand_backend_name(nimcp_rand_backend_t backend);

/**
 * @brief Run self-test of RNG quality
 * @return NIMCP_RAND_OK if tests pass
 *
 * Tests: mean, variance, chi-squared uniformity, Box-Muller normality
 */
nimcp_rand_result_t nimcp_rand_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAND_H */
