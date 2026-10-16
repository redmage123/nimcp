/**
 * @file nimcp_hopfield_memory.h
 * @brief Modern Hopfield Memory - GPU-Accelerated Associative Memory
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Modern continuous Hopfield network for content-addressable memory
 * WHY:  Enable associative retrieval and pattern completion in omnidirectional inference
 * HOW:  Softmax attention over stored patterns with GPU acceleration
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * MODERN HOPFIELD NETWORKS (Ramsauer et al., 2020):
 * -------------------------------------------------
 * Classical Hopfield networks store binary patterns with limited capacity.
 * Modern continuous Hopfield networks use:
 *
 *   E(ξ) = -log(Σ_i exp(β × ξ^T × x_i)) + (β/2)||ξ||² + const
 *
 * Where:
 * - ξ is the query pattern
 * - x_i are stored patterns
 * - β is inverse temperature (sharpness)
 *
 * Update rule becomes attention:
 *
 *   ξ_new = Σ_i softmax(β × ξ^T × x_i) × x_i
 *
 * This is equivalent to a single attention head!
 *
 * CAPACITY:
 * ---------
 * Classical: O(N) patterns for N neurons
 * Modern exponential: exp(O(d)) patterns for dimension d
 *
 * INTEGRATION POINTS:
 * -------------------
 * - JEPA bidirectional: Associative direction uses Hopfield
 * - Temporal replay: Patterns as memory traces
 * - Bio-async: Pattern store/retrieve messages
 * - Immune: Pattern matching for antigen recognition
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HOPFIELD_MEMORY_H
#define NIMCP_HOPFIELD_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for Hopfield memory */
#define BIO_MODULE_HOPFIELD_MEMORY              0x0E20

/** @brief Default pattern capacity */
#define HOPFIELD_DEFAULT_CAPACITY               1024

/** @brief Maximum pattern capacity */
#define HOPFIELD_MAX_CAPACITY                   65536

/** @brief Default inverse temperature */
#define HOPFIELD_DEFAULT_BETA                   1.0f

/** @brief Maximum inverse temperature */
#define HOPFIELD_MAX_BETA                       100.0f

/** @brief Default convergence iterations */
#define HOPFIELD_DEFAULT_ITERATIONS             3

/** @brief Convergence threshold */
#define HOPFIELD_CONVERGENCE_THRESHOLD          1e-6f

/** @brief Default pattern dimension */
#define HOPFIELD_DEFAULT_DIM                    256

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Hopfield network mode
 *
 * Different update rules for different capacity/accuracy tradeoffs
 */
typedef enum {
    HOPFIELD_MODE_SOFTMAX = 0,      /**< Modern continuous (default) */
    HOPFIELD_MODE_EXPONENTIAL,      /**< Exponential capacity variant */
    HOPFIELD_MODE_POLYNOMIAL,       /**< Polynomial capacity variant */
    HOPFIELD_MODE_SPARSE            /**< Sparse attention variant */
} hopfield_mode_t;

/**
 * @brief Pattern storage mode
 */
typedef enum {
    HOPFIELD_STORE_OVERWRITE = 0,   /**< Overwrite oldest pattern */
    HOPFIELD_STORE_REJECT,          /**< Reject if at capacity */
    HOPFIELD_STORE_MERGE            /**< Merge with similar pattern */
} hopfield_store_mode_t;

/**
 * @brief GPU acceleration mode
 */
typedef enum {
    HOPFIELD_GPU_DISABLED = 0,      /**< CPU only */
    HOPFIELD_GPU_AUTO,              /**< Auto-select based on workload */
    HOPFIELD_GPU_PREFERRED,         /**< Prefer GPU if available */
    HOPFIELD_GPU_REQUIRED           /**< Require GPU (fail if unavailable) */
} hopfield_gpu_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Pattern metadata
 */
typedef struct {
    uint32_t pattern_id;            /**< Unique pattern identifier */
    uint64_t store_timestamp;       /**< When pattern was stored */
    uint64_t access_count;          /**< Number of retrievals */
    uint64_t last_access;           /**< Last access timestamp */
    float strength;                 /**< Pattern strength/importance */
    void* user_data;                /**< Optional user metadata */
} hopfield_pattern_meta_t;

/**
 * @brief Retrieval result
 */
typedef struct {
    float* pattern;                 /**< Retrieved pattern [dim] */
    uint32_t pattern_id;            /**< ID of best matching pattern */
    float similarity;               /**< Cosine similarity to query */
    float energy;                   /**< Energy at retrieval */
    uint32_t iterations;            /**< Iterations to converge */
    bool converged;                 /**< Did retrieval converge? */
} hopfield_retrieval_result_t;

/**
 * @brief Batch retrieval result
 */
typedef struct {
    hopfield_retrieval_result_t* results; /**< Array of results */
    uint32_t num_results;           /**< Number of results */
    float avg_similarity;           /**< Average similarity */
    float avg_energy;               /**< Average energy */
    uint64_t total_time_us;         /**< Total retrieval time */
} hopfield_batch_result_t;

/**
 * @brief Hopfield memory configuration
 */
typedef struct {
    /* Architecture */
    uint32_t pattern_dim;           /**< Pattern dimension */
    uint32_t capacity;              /**< Maximum stored patterns */
    hopfield_mode_t mode;           /**< Hopfield mode */
    hopfield_store_mode_t store_mode; /**< Storage mode when full */

    /* Parameters */
    float beta;                     /**< Inverse temperature */
    uint32_t max_iterations;        /**< Max convergence iterations */
    float convergence_threshold;    /**< Convergence epsilon */
    float similarity_threshold;     /**< Min similarity for merge */

    /* GPU configuration */
    hopfield_gpu_mode_t gpu_mode;   /**< GPU acceleration mode */
    uint32_t min_batch_for_gpu;     /**< Min batch size for GPU */

    /* Memory management */
    bool enable_metadata;           /**< Track pattern metadata */
    bool normalize_patterns;        /**< L2 normalize patterns */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} hopfield_config_t;

/**
 * @brief Hopfield memory statistics
 */
typedef struct {
    uint32_t patterns_stored;       /**< Current stored pattern count */
    uint64_t total_stores;          /**< Total store operations */
    uint64_t total_retrievals;      /**< Total retrieval operations */
    uint64_t successful_retrievals; /**< Retrievals with convergence */
    float avg_similarity;           /**< Running average similarity */
    float avg_iterations;           /**< Running average iterations */
    float avg_energy;               /**< Running average energy */
    uint64_t gpu_retrievals;        /**< GPU retrieval count */
    uint64_t cpu_retrievals;        /**< CPU retrieval count */
    float capacity_used;            /**< Capacity utilization [0,1] */
} hopfield_stats_t;

/**
 * @brief Hopfield memory state
 */
typedef struct hopfield_memory {
    /* Configuration */
    hopfield_config_t config;

    /* Pattern storage */
    float* patterns;                /**< Stored patterns [capacity × dim] */
    hopfield_pattern_meta_t* metadata; /**< Pattern metadata (optional) */
    uint32_t pattern_count;         /**< Current number of patterns */
    uint32_t next_pattern_id;       /**< Next pattern ID to assign */

    /* Query workspace */
    float* query_buffer;            /**< Query working buffer [dim] */
    float* similarity_buffer;       /**< Similarity scores [capacity] */
    float* attention_buffer;        /**< Attention weights [capacity] */

    /* GPU resources */
#ifdef NIMCP_ENABLE_CUDA
    struct nimcp_gpu_context_s* gpu_ctx;
    float* patterns_device;         /**< Patterns on GPU */
    float* query_device;            /**< Query on GPU */
    float* similarity_device;       /**< Similarities on GPU */
    bool gpu_initialized;
    bool patterns_synced;           /**< GPU patterns up to date */
#endif

    /* Statistics */
    hopfield_stats_t stats;

    /* Thread safety */
    void* mutex;
} hopfield_memory_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default Hopfield configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int hopfield_default_config(hopfield_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
int hopfield_validate_config(const hopfield_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Hopfield memory
 *
 * WHAT: Initialize associative memory system
 * WHY:  Enable content-addressable pattern storage/retrieval
 * HOW:  Allocate pattern storage, initialize GPU if enabled
 *
 * @param config Configuration (NULL for defaults)
 * @return New memory or NULL on failure
 */
hopfield_memory_t* hopfield_memory_create(const hopfield_config_t* config);

/**
 * @brief Destroy Hopfield memory
 *
 * @param memory Memory to destroy (NULL safe)
 */
void hopfield_memory_destroy(hopfield_memory_t* memory);

/**
 * @brief Clear all stored patterns
 *
 * @param memory Hopfield memory
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_clear(hopfield_memory_t* memory);

/* ============================================================================
 * Pattern Storage API
 * ============================================================================ */

/**
 * @brief Store a pattern
 *
 * WHAT: Add pattern to memory
 * WHY:  Build associative memory content
 * HOW:  Copy to pattern matrix, sync to GPU if needed
 *
 * @param memory Hopfield memory
 * @param pattern Pattern to store [dim]
 * @param pattern_id Output pattern ID (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_store(hopfield_memory_t* memory,
                           const float* pattern,
                           uint32_t* pattern_id);

/**
 * @brief Store pattern with metadata
 *
 * @param memory Hopfield memory
 * @param pattern Pattern to store [dim]
 * @param strength Pattern importance [0,1]
 * @param user_data Optional user metadata
 * @param pattern_id Output pattern ID (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_store_with_meta(hopfield_memory_t* memory,
                                     const float* pattern,
                                     float strength,
                                     void* user_data,
                                     uint32_t* pattern_id);

/**
 * @brief Store multiple patterns
 *
 * @param memory Hopfield memory
 * @param patterns Patterns to store [num_patterns × dim]
 * @param num_patterns Number of patterns
 * @param pattern_ids Output pattern IDs (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_store_batch(hopfield_memory_t* memory,
                                 const float* patterns,
                                 uint32_t num_patterns,
                                 uint32_t* pattern_ids);

/**
 * @brief Update an existing pattern
 *
 * @param memory Hopfield memory
 * @param pattern_id Pattern ID to update
 * @param pattern New pattern values [dim]
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_update_pattern(hopfield_memory_t* memory,
                                    uint32_t pattern_id,
                                    const float* pattern);

/**
 * @brief Remove a pattern
 *
 * @param memory Hopfield memory
 * @param pattern_id Pattern ID to remove
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_remove_pattern(hopfield_memory_t* memory,
                                    uint32_t pattern_id);

/* ============================================================================
 * Pattern Retrieval API
 * ============================================================================ */

/**
 * @brief Retrieve pattern by query
 *
 * WHAT: Content-addressable retrieval
 * WHY:  Core Hopfield operation - find best matching pattern
 * HOW:  Iterative attention until convergence
 *
 * @param memory Hopfield memory
 * @param query Query pattern [dim]
 * @param result Output retrieval result
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_retrieve(hopfield_memory_t* memory,
                              const float* query,
                              hopfield_retrieval_result_t* result);

/**
 * @brief Retrieve pattern with custom iterations
 *
 * @param memory Hopfield memory
 * @param query Query pattern [dim]
 * @param max_iterations Maximum iterations
 * @param result Output retrieval result
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_retrieve_iter(hopfield_memory_t* memory,
                                   const float* query,
                                   uint32_t max_iterations,
                                   hopfield_retrieval_result_t* result);

/**
 * @brief Batch retrieval (GPU-accelerated)
 *
 * WHAT: Retrieve multiple patterns in parallel
 * WHY:  Efficient batch processing on GPU
 * HOW:  Parallel attention computation
 *
 * @param memory Hopfield memory
 * @param queries Query patterns [num_queries × dim]
 * @param num_queries Number of queries
 * @param result Output batch result
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_retrieve_batch(hopfield_memory_t* memory,
                                    const float* queries,
                                    uint32_t num_queries,
                                    hopfield_batch_result_t* result);

/**
 * @brief Get top-k most similar patterns
 *
 * @param memory Hopfield memory
 * @param query Query pattern [dim]
 * @param k Number of patterns to return
 * @param pattern_ids Output pattern IDs [k]
 * @param similarities Output similarities [k]
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_top_k(hopfield_memory_t* memory,
                           const float* query,
                           uint32_t k,
                           uint32_t* pattern_ids,
                           float* similarities);

/* ============================================================================
 * Energy API
 * ============================================================================ */

/**
 * @brief Compute energy for a state
 *
 * WHAT: Calculate Hopfield energy
 * WHY:  Energy minimization drives retrieval
 * HOW:  E = -log(Σ exp(β × similarity)) + β/2 ||ξ||²
 *
 * @param memory Hopfield memory
 * @param state State to evaluate [dim]
 * @return Energy value, NAN on error
 */
float hopfield_memory_compute_energy(hopfield_memory_t* memory,
                                      const float* state);

/**
 * @brief Get similarity to all patterns
 *
 * @param memory Hopfield memory
 * @param query Query pattern [dim]
 * @param similarities Output similarities [pattern_count]
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_get_similarities(hopfield_memory_t* memory,
                                      const float* query,
                                      float* similarities);

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
/**
 * @brief Initialize GPU acceleration
 *
 * @param memory Hopfield memory
 * @param gpu_ctx GPU context (NULL for auto-create)
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_init_gpu(hopfield_memory_t* memory,
                              struct nimcp_gpu_context_s* gpu_ctx);

/**
 * @brief Sync patterns to GPU
 *
 * @param memory Hopfield memory
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_sync_to_gpu(hopfield_memory_t* memory);

/**
 * @brief Check if GPU is available
 *
 * @param memory Hopfield memory
 * @return true if GPU initialized
 */
bool hopfield_memory_has_gpu(const hopfield_memory_t* memory);
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param memory Hopfield memory
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_get_stats(const hopfield_memory_t* memory,
                               hopfield_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param memory Hopfield memory
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_reset_stats(hopfield_memory_t* memory);

/**
 * @brief Get pattern count
 *
 * @param memory Hopfield memory
 * @return Number of stored patterns
 */
uint32_t hopfield_memory_pattern_count(const hopfield_memory_t* memory);

/**
 * @brief Get capacity
 *
 * @param memory Hopfield memory
 * @return Maximum capacity
 */
uint32_t hopfield_memory_capacity(const hopfield_memory_t* memory);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param memory Hopfield memory
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_connect_bio_async(hopfield_memory_t* memory);

/**
 * @brief Disconnect from bio-async router
 *
 * @param memory Hopfield memory
 * @return NIMCP_SUCCESS on success
 */
int hopfield_memory_disconnect_bio_async(hopfield_memory_t* memory);

/* ============================================================================
 * Result Management API
 * ============================================================================ */

/**
 * @brief Create retrieval result
 *
 * @param dim Pattern dimension
 * @return New result or NULL
 */
hopfield_retrieval_result_t* hopfield_result_create(uint32_t dim);

/**
 * @brief Destroy retrieval result
 *
 * @param result Result to destroy (NULL safe)
 */
void hopfield_result_destroy(hopfield_retrieval_result_t* result);

/**
 * @brief Create batch result
 *
 * @param num_queries Number of queries
 * @param dim Pattern dimension
 * @return New batch result or NULL
 */
hopfield_batch_result_t* hopfield_batch_result_create(uint32_t num_queries,
                                                       uint32_t dim);

/**
 * @brief Destroy batch result
 *
 * @param result Batch result to destroy (NULL safe)
 */
void hopfield_batch_result_destroy(hopfield_batch_result_t* result);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert mode to string
 *
 * @param mode Hopfield mode
 * @return Human-readable string
 */
const char* hopfield_mode_to_string(hopfield_mode_t mode);

/**
 * @brief Convert store mode to string
 *
 * @param mode Store mode
 * @return Human-readable string
 */
const char* hopfield_store_mode_to_string(hopfield_store_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HOPFIELD_MEMORY_H */
