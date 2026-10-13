/**
 * @file nimcp.h
 * @brief Unified NIMCP Public API - Single entry point for all language bindings
 * @version 2.6.1
 * @date 2025-11-04
 *
 * This is the ONLY header file that language bindings should include.
 * It provides a consistent, stable API with proper namespacing.
 *
 * Design Goals:
 * - Single namespace (nimcp_* prefix for ALL symbols)
 * - Opaque handles (no struct exposure)
 * - Version stability (semantic versioning)
 * - Clean C API (C99 compatible)
 * - Language binding friendly (simple types, no complex macros)
 */

#ifndef NIMCP_H
#define NIMCP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Version Information
//=============================================================================

#define NIMCP_VERSION_MAJOR 2
#define NIMCP_VERSION_MINOR 7
#define NIMCP_VERSION_PATCH 0
#define NIMCP_VERSION_STRING "2.7.0"

/**
 * @brief Get NIMCP version as string
 * @return Version string (e.g., "2.6.1")
 */
const char* nimcp_version(void);

/**
 * @brief Get NIMCP version as integer (MAJOR * 10000 + MINOR * 100 + PATCH)
 * @return Version integer (e.g., 20601)
 */
int nimcp_version_int(void);

//=============================================================================
// Opaque Handle Types (consistent naming: nimcp_*_t)
//=============================================================================

/**
 * @brief Opaque handle to a brain instance (high-level learning system)
 */
typedef struct nimcp_brain_handle* nimcp_brain_t;

/**
 * @brief Opaque handle to a neural network instance (low-level control)
 */
typedef struct nimcp_network_handle* nimcp_network_t;

/**
 * @brief Opaque handle to an ethics module
 */
typedef struct nimcp_ethics_handle* nimcp_ethics_t;

/**
 * @brief Opaque handle to a knowledge graph
 */
typedef struct nimcp_knowledge_handle* nimcp_knowledge_t;

//=============================================================================
// Enumerations (consistent naming: NIMCP_*)
//=============================================================================

/**
 * @brief Brain size presets
 */
typedef enum {
    NIMCP_BRAIN_TINY = 0,   /**< 100 neurons, <1MB,  ~0.1ms inference */
    NIMCP_BRAIN_SMALL = 1,  /**< 1K neurons,  ~10MB, ~0.5ms inference */
    NIMCP_BRAIN_MEDIUM = 2, /**< 10K neurons, ~50MB, ~5ms inference */
    NIMCP_BRAIN_LARGE = 3   /**< 100K neurons, ~500MB, ~50ms inference */
} nimcp_brain_size_t;

/**
 * @brief Brain task templates
 */
typedef enum {
    NIMCP_TASK_CLASSIFICATION = 0,   /**< Multi-class classification */
    NIMCP_TASK_REGRESSION = 1,       /**< Continuous value prediction */
    NIMCP_TASK_PATTERN_MATCHING = 2, /**< Pattern recognition */
    NIMCP_TASK_SEQUENCE = 3,         /**< Temporal sequence learning */
    NIMCP_TASK_ASSOCIATION = 4       /**< Association learning */
} nimcp_brain_task_t;

/**
 * @brief Return codes for all NIMCP functions
 */
typedef enum {
    NIMCP_OK = 0,              /**< Success */
    NIMCP_ERROR = -1,               /**< Generic error */
    NIMCP_ERROR_NULL_ARG = -2,      /**< NULL argument provided */
    NIMCP_ERROR_INVALID = -3,       /**< Invalid argument value */
    NIMCP_ERROR_INVALID_ARG = -3,   /**< Invalid argument (alias) */
    NIMCP_ERROR_MEMORY = -4,        /**< Memory allocation failed */
    NIMCP_ERROR_IO = -5,            /**< I/O operation failed */
    NIMCP_ERROR_NOT_IMPLEMENTED = -6 /**< Feature not yet implemented */
} nimcp_status_t;

//=============================================================================
// Brain API - High-Level Learning Interface
//=============================================================================

/**
 * @brief Create a brain with preset configuration
 *
 * @param name Human-readable name (e.g., "ethics", "classifier")
 * @param size Brain size preset (NIMCP_BRAIN_TINY to NIMCP_BRAIN_LARGE)
 * @param task Task template (NIMCP_TASK_CLASSIFICATION, etc.)
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create(
    const char* name,
    nimcp_brain_size_t size,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs
);

/**
 * @brief Destroy a brain and free all resources
 *
 * @param brain Brain handle
 */
void nimcp_brain_destroy(nimcp_brain_t brain);

/**
 * @brief Learn from a single example
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param label Output label/class
 * @param confidence Confidence/importance of this example (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_learn_example(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    const char* label,
    float confidence
);

/**
 * @brief Make a decision/prediction
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @param out_label Buffer to store predicted label (must be pre-allocated, min 64 bytes)
 * @param out_confidence Pointer to store prediction confidence (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_predict(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Batch prediction (v2.7.0)
 *
 * Predict on multiple samples simultaneously for better throughput.
 * Auto-parallelizes across available CPU cores.
 *
 * @param brain Brain handle
 * @param features Array of feature pointers [batch_size]
 * @param num_features Feature dimension (same for all samples)
 * @param batch_size Number of samples
 * @param out_labels Array of label buffers [batch_size][64] (pre-allocated)
 * @param out_confidences Array of confidence scores [batch_size] (pre-allocated)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_predict_batch(
    nimcp_brain_t brain,
    const float** features,
    uint32_t num_features,
    uint32_t batch_size,
    char** out_labels,
    float* out_confidences
);

/**
 * @brief Async prediction (v2.7.0)
 *
 * Non-blocking prediction that returns immediately with a future handle.
 * Result can be retrieved later with nimcp_future_wait().
 *
 * @param brain Brain handle
 * @param features Input features array
 * @param num_features Number of input features
 * @return Future handle or NULL on error
 */
typedef struct nimcp_future_handle* nimcp_future_t;

nimcp_future_t nimcp_brain_predict_async(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Wait for async prediction result (v2.7.0)
 *
 * Blocks until prediction completes or timeout.
 *
 * @param future Future handle from nimcp_brain_predict_async()
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @param out_label Buffer to store predicted label (must be pre-allocated, min 64 bytes)
 * @param out_confidence Pointer to store prediction confidence
 * @return NIMCP_OK on success, NIMCP_ERROR_TIMEOUT on timeout
 */
nimcp_status_t nimcp_future_wait_predict(
    nimcp_future_t future,
    uint32_t timeout_ms,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Cancel async prediction (v2.7.0)
 *
 * @param future Future handle
 * @return true if cancelled, false if already completed
 */
bool nimcp_future_cancel(nimcp_future_t future);

/**
 * @brief Destroy future handle (v2.7.0)
 *
 * @param future Future handle
 * @param cancel_if_pending Cancel operation if still pending
 */
void nimcp_future_destroy(nimcp_future_t future, bool cancel_if_pending);

/**
 * @brief Batch learning (v2.7.0)
 *
 * Learn from multiple examples simultaneously.
 *
 * @param brain Brain handle
 * @param features Array of feature pointers [batch_size]
 * @param num_features Feature dimension
 * @param batch_size Number of samples
 * @param labels Array of label strings [batch_size]
 * @param confidences Array of confidence weights [batch_size]
 * @return Average loss over batch, or negative on error
 */
float nimcp_brain_learn_batch(
    nimcp_brain_t brain,
    const float** features,
    uint32_t num_features,
    uint32_t batch_size,
    const char** labels,
    const float* confidences
);

/**
 * @brief Save brain to file
 *
 * @param brain Brain handle
 * @param filepath Path to save file
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_save(nimcp_brain_t brain, const char* filepath);

/**
 * @brief Load brain from file
 *
 * @param filepath Path to saved brain file
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_load(const char* filepath);

/**
 * @brief Create brain from YAML or JSON configuration file
 *
 * Supports loading brain configuration from YAML (.yaml, .yml) or JSON (.json) files.
 * Configuration includes architecture, training parameters, plasticity settings, and ethics.
 *
 * Example YAML config:
 * ```yaml
 * brain:
 *   name: "classifier"
 *   size: small           # tiny, small, medium, large
 *   task: classification  # classification, regression, pattern_matching, sequence, association
 *   architecture:
 *     num_inputs: 784
 *     num_outputs: 10
 *     num_hidden: 256
 *     learning_rate: 0.01
 * ```
 *
 * @param config_filepath Path to YAML or JSON configuration file
 * @return Brain handle or NULL on error
 */
nimcp_brain_t nimcp_brain_create_from_config(const char* config_filepath);

/**
 * @brief Brain probe statistics - comprehensive snapshot of brain state
 */
typedef struct {
    char task_name[64];           /**< Brain name */
    nimcp_brain_size_t size;      /**< Size preset */
    nimcp_brain_task_t task;      /**< Task type */
    uint32_t num_neurons;         /**< Total neurons */
    uint32_t num_synapses;        /**< Total synapses */
    uint32_t num_active_synapses; /**< Non-pruned synapses */

    uint64_t total_inferences;     /**< Total inference count */
    uint64_t total_learning_steps; /**< Total learning steps */

    float avg_sparsity;          /**< Average sparsity (0.0-1.0) */
    float avg_inference_time_us; /**< Average inference time (microseconds) */
    float current_learning_rate; /**< Current learning rate */

    float accuracy;      /**< Validation accuracy (0.0-1.0) */
    size_t memory_bytes; /**< Memory usage in bytes */

    uint32_t num_inputs;  /**< Number of inputs */
    uint32_t num_outputs; /**< Number of outputs */

    // Copy-on-Write (COW) cache statistics
    bool is_cow_clone;          /**< True if this brain is a COW clone */
    uint32_t cow_ref_count;     /**< Reference count for shared data (0 if not COW) */
    size_t cow_shared_bytes;    /**< Bytes shared via COW (0 if not COW) */
    size_t cow_private_bytes;   /**< Bytes private to this brain (always > 0) */
} nimcp_brain_probe_t;

/**
 * @brief Probe brain state - get comprehensive statistics
 *
 * This function provides a snapshot of the brain's current state including
 * architecture, performance metrics, and resource usage. Similar to a network
 * probe, it returns all relevant information for monitoring and debugging.
 *
 * @param brain Brain handle
 * @param probe Output structure to fill with brain statistics
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_probe(nimcp_brain_t brain, nimcp_brain_probe_t* probe);

//=============================================================================
// Copy-on-Write (COW) Cache API - Efficient Memory Sharing
//=============================================================================

/**
 * @brief Opaque handle to a brain snapshot (for COW save/restore)
 */
typedef struct nimcp_brain_snapshot_handle* nimcp_brain_snapshot_t;

/**
 * @brief Clone a brain using copy-on-write (COW) caching
 *
 * WHAT: Creates a lightweight clone that shares memory with the original
 * WHY:  Enables efficient brain replication (86% memory savings)
 * HOW:  Uses nimcp_cache_reference() to share large structures (weights, connections)
 *
 * The clone initially shares all large data structures (neural network weights,
 * connections, knowledge base) with the original brain. Memory is only copied
 * when either brain modifies shared data (copy-on-write semantics).
 *
 * PERFORMANCE:
 * - Clone time: <10ms (vs ~1000ms for full copy)
 * - Memory overhead: ~1MB metadata (vs ~50MB full copy)
 * - Memory savings: 86% for replicas, 99% for snapshots
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1) initially, O(n) after modifications
 *
 * @param original Brain to clone
 * @return Cloned brain handle or NULL on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_brain_t original = nimcp_brain_create(...);
 * nimcp_brain_t clone = nimcp_brain_clone_cow(original);
 * // clone shares memory with original (fast, low memory)
 * nimcp_brain_learn_example(clone, ...);  // Triggers copy on first write
 * ```
 */
nimcp_brain_t nimcp_brain_clone_cow(nimcp_brain_t original);

/**
 * @brief Create instant snapshot of brain state using COW
 *
 * WHAT: Creates zero-copy snapshot for rollback/checkpointing
 * WHY:  Enables instant state capture without memory overhead
 * HOW:  Uses nimcp_cache_reference() to share all brain data
 *
 * Snapshots are instant (<1ms) and use minimal memory (~48 bytes overhead).
 * The original brain can continue learning while snapshot preserves the
 * original state. Snapshot automatically copies data if brain modifies it.
 *
 * PERFORMANCE:
 * - Snapshot time: <1ms (zero-copy)
 * - Memory overhead: ~48 bytes metadata
 * - Memory savings: 99% vs traditional snapshot
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1) overhead
 *
 * @param brain Brain to snapshot
 * @return Snapshot handle or NULL on error
 *
 * EXAMPLE:
 * ```c
 * nimcp_brain_snapshot_t checkpoint = nimcp_brain_snapshot_cow(brain);
 * train_epochs(brain, 100);
 * if (performance < threshold) {
 *     nimcp_brain_restore_cow(brain, checkpoint);  // Instant rollback
 * }
 * nimcp_brain_snapshot_destroy(checkpoint);
 * ```
 */
nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(nimcp_brain_t brain);

/**
 * @brief Restore brain state from COW snapshot
 *
 * WHAT: Restores brain to snapshot state
 * WHY:  Enables instant rollback to previous state
 * HOW:  Replaces current state with snapshot references
 *
 * The brain's current state is replaced with the snapshot state.
 * This is instant because it just swaps cached pointers.
 *
 * PERFORMANCE: <1ms (pointer swapping)
 * THREAD SAFETY: Thread-safe
 * MEMORY: O(1)
 *
 * @param brain Brain to restore
 * @param snapshot Snapshot to restore from
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_brain_restore_cow(nimcp_brain_t brain, nimcp_brain_snapshot_t snapshot);

/**
 * @brief Destroy brain snapshot and release references
 *
 * @param snapshot Snapshot handle
 */
void nimcp_brain_snapshot_destroy(nimcp_brain_snapshot_t snapshot);

//=============================================================================
// Neural Network API - Low-Level Interface (Advanced Users)
//=============================================================================

/**
 * @brief Create a neural network with custom configuration
 *
 * @param num_inputs Number of input neurons
 * @param num_outputs Number of output neurons
 * @param num_hidden Number of hidden neurons
 * @param learning_rate Learning rate (typically 0.001 - 0.1)
 * @return Network handle or NULL on error
 */
nimcp_network_t nimcp_network_create(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_hidden,
    float learning_rate
);

/**
 * @brief Destroy a neural network
 *
 * @param network Network handle
 */
void nimcp_network_destroy(nimcp_network_t network);

/**
 * @brief Forward pass through network
 *
 * @param network Network handle
 * @param inputs Input array (size = num_inputs)
 * @param num_inputs Number of inputs
 * @param outputs Output array (size = num_outputs, pre-allocated)
 * @param num_outputs Number of outputs
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_network_forward(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    float* outputs,
    uint32_t num_outputs
);

/**
 * @brief Train network on a single example (supervised learning)
 *
 * @param network Network handle
 * @param inputs Input array
 * @param num_inputs Number of inputs
 * @param targets Target output array
 * @param num_targets Number of target outputs
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_network_train(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets
);

//=============================================================================
// Ethics Module API
//=============================================================================

/**
 * @brief Create an ethics module
 *
 * @return Ethics module handle or NULL on error
 */
nimcp_ethics_t nimcp_ethics_create(void);

/**
 * @brief Destroy an ethics module
 *
 * @param ethics Ethics module handle
 */
void nimcp_ethics_destroy(nimcp_ethics_t ethics);

/**
 * @brief Check if an action is ethically acceptable
 *
 * @param ethics Ethics module handle
 * @param situation Situation features array
 * @param num_features Number of features
 * @param out_score Ethical score (-1.0 = harmful, 0.0 = neutral, 1.0 = beneficial)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_ethics_check(
    nimcp_ethics_t ethics,
    const float* situation,
    uint32_t num_features,
    float* out_score
);

//=============================================================================
// Knowledge Graph API
//=============================================================================

/**
 * @brief Create a knowledge graph
 *
 * @return Knowledge graph handle or NULL on error
 */
nimcp_knowledge_t nimcp_knowledge_create(void);

/**
 * @brief Destroy a knowledge graph
 *
 * @param knowledge Knowledge graph handle
 */
void nimcp_knowledge_destroy(nimcp_knowledge_t knowledge);

/**
 * @brief Add a fact to the knowledge graph
 *
 * @param knowledge Knowledge graph handle
 * @param subject Subject entity
 * @param predicate Relationship type
 * @param object Object entity
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_knowledge_add_fact(
    nimcp_knowledge_t knowledge,
    const char* subject,
    const char* predicate,
    const char* object
);

/**
 * @brief Query the knowledge graph
 *
 * @param knowledge Knowledge graph handle
 * @param query Query string
 * @param out_result Result buffer (pre-allocated, min 1024 bytes)
 * @param max_result_len Maximum result length
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_knowledge_query(
    nimcp_knowledge_t knowledge,
    const char* query,
    char* out_result,
    uint32_t max_result_len
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error message for last error
 *
 * @return Error message string (statically allocated)
 */
const char* nimcp_get_error(void);

/**
 * @brief Initialize NIMCP library (call once at startup)
 *
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_status_t nimcp_init(void);

/**
 * @brief Shutdown NIMCP library (call once at cleanup)
 */
void nimcp_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_H
