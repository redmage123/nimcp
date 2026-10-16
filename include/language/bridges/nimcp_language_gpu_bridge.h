//=============================================================================
// nimcp_language_gpu_bridge.h - Language-GPU Acceleration Bridge
//=============================================================================
/**
 * @file nimcp_language_gpu_bridge.h
 * @brief GPU acceleration bridge for Language Layer processing
 *
 * WHAT: Bridge enabling GPU-accelerated language processing operations
 * WHY:  Accelerate computationally intensive language operations
 * HOW:  Batch processing, parallel embedding lookups, GPU semantic spreading
 *
 * GPU-ACCELERATED OPERATIONS:
 * - Phoneme recognition: Batch spectral processing
 * - Lexical access: Parallel word lookup via embedding similarity
 * - Semantic spreading: GPU graph traversal
 * - Embedding operations: Dot products, nearest neighbors
 * - Attention computation: Parallel attention scores
 *
 * KEY CONNECTIONS:
 * - GPU Execution Context: CUDA/OpenCL execution
 * - Wernicke GPU (if enabled): Comprehension acceleration
 * - Broca GPU (if enabled): Production acceleration
 * - NLP Network: Embedding and attention GPU ops
 *
 * BATCHING STRATEGY:
 * - Collect operations until batch threshold or timeout
 * - Execute batch on GPU
 * - Route results via bio-async
 *
 * @version 1.0.0 - Phase L4: Advanced Language Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_GPU_BRIDGE_H
#define NIMCP_LANGUAGE_GPU_BRIDGE_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_gpu_bridge language_gpu_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct gpu_execution_context gpu_execution_context_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_GPU_MODULE_NAME      "language_gpu_bridge"
#define LANGUAGE_GPU_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_GPU_DEFAULT_DEVICE_ID              0
#define LANGUAGE_GPU_DEFAULT_BATCH_SIZE             32
#define LANGUAGE_GPU_DEFAULT_BATCH_TIMEOUT_MS       5.0f
#define LANGUAGE_GPU_DEFAULT_MAX_MEMORY_MB          256

/** Batch thresholds */
#define LANGUAGE_GPU_PHONEME_BATCH_THRESHOLD        16
#define LANGUAGE_GPU_WORD_BATCH_THRESHOLD           8
#define LANGUAGE_GPU_EMBEDDING_BATCH_THRESHOLD      32

/** Buffer sizes */
#define LANGUAGE_GPU_MAX_BATCH_SIZE                 128
#define LANGUAGE_GPU_MAX_PENDING_OPS                256

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief GPU operation types
 */
typedef enum {
    GPU_OP_PHONEME_RECOGNIZE = 0,     /**< Phoneme recognition batch */
    GPU_OP_LEXICAL_LOOKUP,            /**< Word embedding lookup */
    GPU_OP_SEMANTIC_SPREAD,           /**< Semantic spreading activation */
    GPU_OP_EMBEDDING_SIMILARITY,      /**< Embedding similarity computation */
    GPU_OP_ATTENTION_COMPUTE,         /**< Attention score computation */
    GPU_OP_EMBEDDING_BATCH,           /**< Batch embedding operations */
    GPU_OP_SOFTMAX,                   /**< Softmax computation */
    GPU_OP_MATRIX_MULTIPLY,           /**< Matrix multiplication */
    GPU_OP_COUNT
} gpu_operation_type_t;

/**
 * @brief GPU execution status
 */
typedef enum {
    GPU_STATUS_IDLE = 0,              /**< No pending operations */
    GPU_STATUS_BATCHING,              /**< Collecting for batch */
    GPU_STATUS_EXECUTING,             /**< GPU execution in progress */
    GPU_STATUS_TRANSFERRING,          /**< Data transfer */
    GPU_STATUS_COMPLETE,              /**< Operation complete */
    GPU_STATUS_ERROR,                 /**< Error occurred */
    GPU_STATUS_COUNT
} gpu_status_t;

/**
 * @brief GPU backend type
 */
typedef enum {
    GPU_BACKEND_NONE = 0,             /**< No GPU available */
    GPU_BACKEND_CUDA,                 /**< NVIDIA CUDA */
    GPU_BACKEND_OPENCL,               /**< OpenCL */
    GPU_BACKEND_METAL,                /**< Apple Metal */
    GPU_BACKEND_VULKAN,               /**< Vulkan compute */
    GPU_BACKEND_COUNT
} gpu_backend_t;

/**
 * @brief Memory transfer direction
 */
typedef enum {
    TRANSFER_HOST_TO_DEVICE = 0,      /**< CPU → GPU */
    TRANSFER_DEVICE_TO_HOST,          /**< GPU → CPU */
    TRANSFER_DEVICE_TO_DEVICE,        /**< GPU → GPU */
    TRANSFER_COUNT
} transfer_direction_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief GPU device information
 */
typedef struct {
    uint32_t device_id;               /**< Device ID */
    char name[128];                   /**< Device name */
    gpu_backend_t backend;            /**< Backend type */
    size_t total_memory;              /**< Total memory (bytes) */
    size_t available_memory;          /**< Available memory */
    uint32_t compute_capability_major;/**< Compute capability major */
    uint32_t compute_capability_minor;/**< Compute capability minor */
    bool tensor_cores_available;      /**< Tensor cores present */
    bool fp16_supported;              /**< Half precision supported */
} gpu_device_info_t;

/**
 * @brief Pending GPU operation
 */
typedef struct {
    gpu_operation_type_t type;        /**< Operation type */
    void* input_data;                 /**< Input data pointer */
    void* output_data;                /**< Output data pointer */
    size_t input_size;                /**< Input size (bytes) */
    size_t output_size;               /**< Output size (bytes) */
    uint32_t batch_count;             /**< Items in batch */
    uint64_t submit_time_ms;          /**< Submission timestamp */
    void* user_data;                  /**< User context */
    void (*callback)(void* result, void* user_data);  /**< Completion callback */
} gpu_pending_op_t;

/**
 * @brief Phoneme batch operation
 */
typedef struct {
    /* Input spectral frames */
    float* spectral_frames;           /**< Spectral data (batch x frame_dim) */
    uint32_t frame_dim;               /**< Spectral frame dimension */
    uint32_t num_frames;              /**< Number of frames */

    /* Output */
    uint32_t* phoneme_ids;            /**< Output phoneme IDs */
    float* confidences;               /**< Output confidences */
    uint32_t max_output;              /**< Maximum outputs */
} phoneme_batch_op_t;

/**
 * @brief Lexical lookup batch operation
 */
typedef struct {
    /* Input phoneme sequences */
    uint32_t* phoneme_sequences;      /**< Phoneme sequences */
    uint32_t* sequence_lengths;       /**< Length of each sequence */
    uint32_t num_sequences;           /**< Number of sequences */

    /* Output */
    uint32_t* word_ids;               /**< Matched word IDs */
    float* match_scores;              /**< Match scores */
    uint32_t candidates_per_sequence; /**< Candidates per input */
} lexical_batch_op_t;

/**
 * @brief Semantic spreading batch operation
 */
typedef struct {
    /* Input activations */
    uint32_t* source_concept_ids;     /**< Source concept IDs */
    float* source_activations;        /**< Source activation levels */
    uint32_t num_sources;             /**< Number of sources */

    /* Spreading parameters */
    float decay_rate;                 /**< Spreading decay */
    uint32_t max_depth;               /**< Maximum spreading depth */

    /* Output */
    uint32_t* activated_concepts;     /**< Activated concept IDs */
    float* activation_levels;         /**< Resulting activations */
    uint32_t max_activated;           /**< Maximum outputs */
    uint32_t* num_activated;          /**< Actual outputs */
} semantic_spread_op_t;

/**
 * @brief Embedding operation batch
 */
typedef struct {
    /* Query embeddings */
    float* query_embeddings;          /**< Query vectors (batch x dim) */
    uint32_t num_queries;             /**< Number of queries */
    uint32_t embedding_dim;           /**< Embedding dimension */

    /* Database embeddings (on GPU) */
    uint32_t database_size;           /**< Number of entries */

    /* Output */
    uint32_t* top_k_indices;          /**< Top-K indices */
    float* top_k_scores;              /**< Top-K similarity scores */
    uint32_t k;                       /**< Number of neighbors */
} embedding_batch_op_t;

/**
 * @brief Attention computation batch
 */
typedef struct {
    /* Query, Key, Value */
    float* queries;                   /**< Query vectors */
    float* keys;                      /**< Key vectors */
    float* values;                    /**< Value vectors */
    uint32_t seq_length;              /**< Sequence length */
    uint32_t num_heads;               /**< Number of attention heads */
    uint32_t head_dim;                /**< Dimension per head */
    uint32_t batch_size;              /**< Batch size */

    /* Output */
    float* attention_output;          /**< Attention output */
    float* attention_weights;         /**< Attention weights (optional) */
} attention_batch_op_t;

/**
 * @brief GPU memory allocation
 */
typedef struct {
    void* device_ptr;                 /**< Device memory pointer */
    size_t size;                      /**< Allocation size */
    bool pinned;                      /**< Host memory pinned */
    uint32_t allocation_id;           /**< Allocation ID */
} gpu_allocation_t;

/**
 * @brief Memory pool state
 */
typedef struct {
    size_t pool_size;                 /**< Total pool size */
    size_t used_memory;               /**< Currently used */
    size_t peak_usage;                /**< Peak usage */
    uint32_t num_allocations;         /**< Active allocations */
    uint32_t allocation_failures;     /**< Allocation failures */
} memory_pool_state_t;

/**
 * @brief Batch queue state
 */
typedef struct {
    /* Phoneme batch */
    phoneme_batch_op_t* phoneme_queue;/**< Pending phoneme ops */
    uint32_t phoneme_count;           /**< Items in queue */
    uint64_t phoneme_first_submit_ms; /**< First item submit time */

    /* Lexical batch */
    lexical_batch_op_t* lexical_queue;/**< Pending lexical ops */
    uint32_t lexical_count;           /**< Items in queue */
    uint64_t lexical_first_submit_ms; /**< First item submit time */

    /* Embedding batch */
    embedding_batch_op_t* embedding_queue;  /**< Pending embedding ops */
    uint32_t embedding_count;         /**< Items in queue */
    uint64_t embedding_first_submit_ms;/**< First item submit time */
} batch_queue_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Operation counts */
    uint64_t phoneme_ops;             /**< Phoneme operations */
    uint64_t lexical_ops;             /**< Lexical operations */
    uint64_t semantic_ops;            /**< Semantic operations */
    uint64_t embedding_ops;           /**< Embedding operations */
    uint64_t attention_ops;           /**< Attention operations */

    /* Batching */
    uint64_t batches_executed;        /**< Total batches */
    float avg_batch_size;             /**< Average batch size */
    float batch_utilization;          /**< Batch fill rate */

    /* Performance */
    float avg_phoneme_latency_ms;     /**< Phoneme op latency */
    float avg_lexical_latency_ms;     /**< Lexical op latency */
    float avg_semantic_latency_ms;    /**< Semantic op latency */
    float gpu_utilization;            /**< GPU utilization [0-1] */

    /* Memory */
    size_t current_memory_usage;      /**< Current memory used */
    size_t peak_memory_usage;         /**< Peak memory usage */

    /* Errors */
    uint64_t errors;                  /**< Total errors */
    uint64_t last_update_time_ms;     /**< Last update timestamp */
} language_gpu_stats_t;

//=============================================================================
// Bridge State Structure
//=============================================================================

/**
 * @brief Language-GPU bridge state
 */
struct language_gpu_bridge {
    /* Configuration */
    language_gpu_config_t config;     /**< Bridge configuration */
    bool initialized;                 /**< Initialization state */
    bool active;                      /**< Active processing */
    bool gpu_available;               /**< GPU hardware available */

    /* Connected components */
    language_orchestrator_t* orchestrator;    /**< Parent orchestrator */
    gpu_execution_context_t* gpu_ctx;         /**< GPU execution context */

    /* Device info */
    gpu_device_info_t device_info;    /**< GPU device information */
    gpu_backend_t backend;            /**< Active backend */

    /* Status */
    gpu_status_t status;              /**< Current status */

    /* Batch queues */
    batch_queue_state_t batch_queue;  /**< Batch queue state */

    /* Pending operations */
    gpu_pending_op_t* pending_ops;    /**< Pending operations */
    uint32_t num_pending;             /**< Number pending */
    uint32_t max_pending;             /**< Maximum pending */

    /* Memory management */
    memory_pool_state_t memory_pool;  /**< Memory pool state */

    /* GPU-resident data */
    float* word_embeddings_gpu;       /**< Word embeddings on GPU */
    uint32_t word_embedding_count;    /**< Number of embeddings */
    uint32_t embedding_dim;           /**< Embedding dimension */

    float* concept_embeddings_gpu;    /**< Concept embeddings on GPU */
    uint32_t concept_count;           /**< Number of concepts */

    /* Semantic graph on GPU */
    uint32_t* adjacency_gpu;          /**< Adjacency list on GPU */
    float* edge_weights_gpu;          /**< Edge weights on GPU */
    uint32_t graph_nodes;             /**< Graph node count */

    /* Statistics */
    language_gpu_stats_t stats;       /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;         /**< Bio-async router */
    bool bio_async_registered;        /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-GPU bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_gpu_bridge_t* language_gpu_bridge_create(
    const language_gpu_config_t* config
);

/**
 * @brief Destroy language-GPU bridge
 *
 * @param bridge Bridge instance
 */
void language_gpu_bridge_destroy(language_gpu_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_init(language_gpu_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_start(language_gpu_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_stop(language_gpu_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to language orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Language orchestrator
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_connect_orchestrator(
    language_gpu_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to GPU execution context
 *
 * @param bridge Bridge instance
 * @param gpu_ctx GPU execution context
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_connect_gpu_context(
    language_gpu_bridge_t* bridge,
    gpu_execution_context_t* gpu_ctx
);

//=============================================================================
// GPU Availability API
//=============================================================================

/**
 * @brief Check if GPU is available
 *
 * @param bridge Bridge instance
 * @return true if GPU available
 */
bool language_gpu_bridge_is_available(
    const language_gpu_bridge_t* bridge
);

/**
 * @brief Get GPU device info
 *
 * @param bridge Bridge instance
 * @param info Output device info
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_get_device_info(
    const language_gpu_bridge_t* bridge,
    gpu_device_info_t* info
);

/**
 * @brief Get current GPU status
 *
 * @param bridge Bridge instance
 * @return Current status
 */
gpu_status_t language_gpu_bridge_get_status(
    const language_gpu_bridge_t* bridge
);

//=============================================================================
// Data Upload API
//=============================================================================

/**
 * @brief Upload word embeddings to GPU
 *
 * @param bridge Bridge instance
 * @param embeddings Embedding matrix (count x dim)
 * @param count Number of embeddings
 * @param dim Embedding dimension
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_upload_word_embeddings(
    language_gpu_bridge_t* bridge,
    const float* embeddings,
    uint32_t count,
    uint32_t dim
);

/**
 * @brief Upload concept embeddings to GPU
 *
 * @param bridge Bridge instance
 * @param embeddings Embedding matrix (count x dim)
 * @param count Number of embeddings
 * @param dim Embedding dimension
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_upload_concept_embeddings(
    language_gpu_bridge_t* bridge,
    const float* embeddings,
    uint32_t count,
    uint32_t dim
);

/**
 * @brief Upload semantic graph to GPU
 *
 * @param bridge Bridge instance
 * @param adjacency Adjacency list
 * @param weights Edge weights
 * @param num_nodes Number of nodes
 * @param num_edges Number of edges
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_upload_semantic_graph(
    language_gpu_bridge_t* bridge,
    const uint32_t* adjacency,
    const float* weights,
    uint32_t num_nodes,
    uint32_t num_edges
);

//=============================================================================
// Batch Operation API
//=============================================================================

/**
 * @brief Submit phoneme recognition batch
 *
 * @param bridge Bridge instance
 * @param op Phoneme batch operation
 * @return 0 on success (queued), -1 on error
 */
int language_gpu_bridge_submit_phoneme_batch(
    language_gpu_bridge_t* bridge,
    const phoneme_batch_op_t* op
);

/**
 * @brief Submit lexical lookup batch
 *
 * @param bridge Bridge instance
 * @param op Lexical batch operation
 * @return 0 on success (queued), -1 on error
 */
int language_gpu_bridge_submit_lexical_batch(
    language_gpu_bridge_t* bridge,
    const lexical_batch_op_t* op
);

/**
 * @brief Submit semantic spreading batch
 *
 * @param bridge Bridge instance
 * @param op Semantic spreading operation
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_submit_semantic_spread(
    language_gpu_bridge_t* bridge,
    const semantic_spread_op_t* op
);

/**
 * @brief Submit embedding similarity batch
 *
 * @param bridge Bridge instance
 * @param op Embedding batch operation
 * @return 0 on success (queued), -1 on error
 */
int language_gpu_bridge_submit_embedding_batch(
    language_gpu_bridge_t* bridge,
    const embedding_batch_op_t* op
);

/**
 * @brief Submit attention computation
 *
 * @param bridge Bridge instance
 * @param op Attention batch operation
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_submit_attention_batch(
    language_gpu_bridge_t* bridge,
    const attention_batch_op_t* op
);

/**
 * @brief Flush pending batches (force execution)
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_flush(language_gpu_bridge_t* bridge);

//=============================================================================
// Synchronous Operation API (blocking)
//=============================================================================

/**
 * @brief Compute word similarity (blocking)
 *
 * @param bridge Bridge instance
 * @param query_embedding Query embedding
 * @param dim Embedding dimension
 * @param top_k Number of results
 * @param result_ids Output word IDs
 * @param result_scores Output similarity scores
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_word_similarity_sync(
    language_gpu_bridge_t* bridge,
    const float* query_embedding,
    uint32_t dim,
    uint32_t top_k,
    uint32_t* result_ids,
    float* result_scores
);

/**
 * @brief Compute semantic spread (blocking)
 *
 * @param bridge Bridge instance
 * @param source_concepts Source concept IDs
 * @param source_activations Source activations
 * @param num_sources Number of sources
 * @param max_depth Maximum spreading depth
 * @param result_concepts Output activated concepts
 * @param result_activations Output activations
 * @param max_results Maximum results
 * @return Number of activated concepts, or -1 on error
 */
int language_gpu_bridge_semantic_spread_sync(
    language_gpu_bridge_t* bridge,
    const uint32_t* source_concepts,
    const float* source_activations,
    uint32_t num_sources,
    uint32_t max_depth,
    uint32_t* result_concepts,
    float* result_activations,
    uint32_t max_results
);

//=============================================================================
// Memory Management API
//=============================================================================

/**
 * @brief Get memory pool state
 *
 * @param bridge Bridge instance
 * @param state Output memory state
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_get_memory_state(
    const language_gpu_bridge_t* bridge,
    memory_pool_state_t* state
);

/**
 * @brief Free unused GPU memory
 *
 * @param bridge Bridge instance
 * @return Bytes freed
 */
size_t language_gpu_bridge_free_unused_memory(
    language_gpu_bridge_t* bridge
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update bridge (call each frame/cycle)
 *
 * Processes batch timeouts, executes ready batches
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_update(
    language_gpu_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_get_stats(
    const language_gpu_bridge_t* bridge,
    language_gpu_stats_t* stats
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_bio_async_register(
    language_gpu_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_gpu_bridge_bio_async_unregister(
    language_gpu_bridge_t* bridge
);

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* gpu_operation_type_to_string(gpu_operation_type_t type);
const char* gpu_status_to_string(gpu_status_t status);
const char* gpu_backend_to_string(gpu_backend_t backend);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_GPU_BRIDGE_H */
