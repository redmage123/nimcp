/**
 * @file nimcp_training_checkpoint.h
 * @brief Training Checkpointing System for NIMCP
 *
 * WHAT: Save and restore training state for fault tolerance and resumption
 * WHY:  Long training runs need recovery from failures, support for
 *       pausing/resuming, and model versioning
 * HOW:  Serialize model weights, optimizer state, scheduler state,
 *       and training metadata to disk
 *
 * BIOLOGICAL GROUNDING:
 * - Models memory consolidation during sleep
 * - Checkpoints act like long-term memory snapshots
 * - Incremental checkpoints model working memory updates
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#ifndef NIMCP_TRAINING_CHECKPOINT_H
#define NIMCP_TRAINING_CHECKPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define CHECKPOINT_MAGIC            0x4E494D43  /**< "NIMC" magic number */
#define CHECKPOINT_VERSION          1           /**< Current checkpoint format version */
#define CHECKPOINT_MAX_PATH         1024        /**< Maximum path length */
#define CHECKPOINT_MAX_NAME         256         /**< Maximum checkpoint name */
#define CHECKPOINT_DEFAULT_KEEP     5           /**< Default checkpoints to keep */
#define CHECKPOINT_COMPRESSION_NONE 0           /**< No compression */
#define CHECKPOINT_COMPRESSION_ZSTD 1           /**< Zstd compression */
#define CHECKPOINT_COMPRESSION_LZ4  2           /**< LZ4 compression */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Checkpoint save triggers
 */
typedef enum {
    CHECKPOINT_TRIGGER_EPOCH = 0,       /**< Save every N epochs */
    CHECKPOINT_TRIGGER_STEPS,           /**< Save every N steps */
    CHECKPOINT_TRIGGER_TIME,            /**< Save every N minutes */
    CHECKPOINT_TRIGGER_IMPROVEMENT,     /**< Save on metric improvement */
    CHECKPOINT_TRIGGER_MANUAL,          /**< Manual save only */
    CHECKPOINT_TRIGGER_COUNT
} checkpoint_trigger_t;

/**
 * @brief Checkpoint content flags
 */
typedef enum {
    CHECKPOINT_CONTENT_WEIGHTS     = (1 << 0),  /**< Model weights */
    CHECKPOINT_CONTENT_OPTIMIZER   = (1 << 1),  /**< Optimizer state (momentum, etc.) */
    CHECKPOINT_CONTENT_SCHEDULER   = (1 << 2),  /**< LR scheduler state */
    CHECKPOINT_CONTENT_METRICS     = (1 << 3),  /**< Training metrics history */
    CHECKPOINT_CONTENT_CONFIG      = (1 << 4),  /**< Training configuration */
    CHECKPOINT_CONTENT_CURRICULUM  = (1 << 5),  /**< Curriculum learning state */
    CHECKPOINT_CONTENT_RNG         = (1 << 6),  /**< Random number generator state */
    CHECKPOINT_CONTENT_GRAD_SCALER = (1 << 7),  /**< Gradient scaler state (AMP) */
    CHECKPOINT_CONTENT_ALL         = 0xFF       /**< All content */
} checkpoint_content_t;

/**
 * @brief Checkpoint status
 */
typedef enum {
    CHECKPOINT_STATUS_OK = 0,           /**< Valid checkpoint */
    CHECKPOINT_STATUS_CORRUPTED,        /**< Checksum mismatch */
    CHECKPOINT_STATUS_INCOMPLETE,       /**< Partial save (interrupted) */
    CHECKPOINT_STATUS_VERSION_MISMATCH, /**< Incompatible format version */
    CHECKPOINT_STATUS_NOT_FOUND,        /**< File not found */
    CHECKPOINT_STATUS_COUNT
} checkpoint_status_t;

//=============================================================================
// Checkpoint Metadata
//=============================================================================

/**
 * @brief Checkpoint metadata header
 */
typedef struct {
    uint32_t magic;                     /**< Magic number for validation */
    uint32_t version;                   /**< Format version */
    uint64_t timestamp;                 /**< Unix timestamp of save */
    uint32_t content_flags;             /**< What's included in checkpoint */

    /* Training progress */
    uint64_t epoch;                     /**< Current epoch */
    uint64_t global_step;               /**< Global training step */
    uint64_t samples_seen;              /**< Total samples processed */

    /* Performance at checkpoint */
    float train_loss;                   /**< Training loss */
    float val_loss;                     /**< Validation loss */
    float best_metric;                  /**< Best metric value seen */
    bool is_best;                       /**< Whether this is best checkpoint */

    /* Model info */
    uint64_t num_parameters;            /**< Total model parameters */
    char model_name[CHECKPOINT_MAX_NAME]; /**< Model architecture name */

    /* File info */
    uint64_t file_size;                 /**< Total checkpoint file size */
    uint32_t checksum;                  /**< CRC32 checksum */
    uint8_t compression;                /**< Compression method used */
} checkpoint_header_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Checkpoint manager configuration
 */
typedef struct {
    /* Directory and naming */
    char save_dir[CHECKPOINT_MAX_PATH]; /**< Directory for checkpoints */
    char prefix[CHECKPOINT_MAX_NAME];   /**< Checkpoint filename prefix */
    bool include_timestamp;             /**< Include timestamp in filename */
    bool include_epoch;                 /**< Include epoch in filename */
    bool include_metric;                /**< Include metric in filename */

    /* Save triggers */
    checkpoint_trigger_t trigger;       /**< When to save */
    uint32_t save_frequency;            /**< Frequency (epochs/steps/minutes) */
    bool save_on_interrupt;             /**< Save on SIGINT/SIGTERM */

    /* Content selection */
    uint32_t content_flags;             /**< What to include (checkpoint_content_t) */
    bool save_optimizer;                /**< Save optimizer state */
    bool save_scheduler;                /**< Save LR scheduler state */

    /* Best model tracking */
    bool track_best;                    /**< Track best metric checkpoint */
    char best_metric_name[64];          /**< Name of metric to track */
    bool maximize_metric;               /**< true = higher is better */

    /* Checkpoint management */
    uint32_t keep_last_n;               /**< Keep last N checkpoints (0 = all) */
    bool keep_best;                     /**< Always keep best checkpoint */
    bool overwrite;                     /**< Overwrite existing checkpoints */

    /* Compression */
    uint8_t compression;                /**< Compression method */
    int compression_level;              /**< Compression level (method-specific) */

    /* Validation */
    bool compute_checksum;              /**< Compute CRC32 checksum */
    bool verify_on_load;                /**< Verify checksum on load */

    /* Callbacks */
    void (*on_save)(const char* path, void* user_data);   /**< Called after save */
    void (*on_load)(const char* path, void* user_data);   /**< Called after load */
    void* callback_data;                /**< User data for callbacks */
} checkpoint_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Checkpoint manager statistics
 */
typedef struct {
    uint64_t total_saves;               /**< Total checkpoints saved */
    uint64_t total_loads;               /**< Total checkpoints loaded */
    uint64_t failed_saves;              /**< Failed save attempts */
    uint64_t failed_loads;              /**< Failed load attempts */

    double total_save_time_ms;          /**< Total time spent saving */
    double total_load_time_ms;          /**< Total time spent loading */
    double avg_save_time_ms;            /**< Average save time */
    double avg_load_time_ms;            /**< Average load time */

    uint64_t total_bytes_saved;         /**< Total bytes written */
    uint64_t total_bytes_loaded;        /**< Total bytes read */

    uint64_t checkpoints_on_disk;       /**< Current checkpoints in directory */
    uint64_t disk_space_used;           /**< Disk space used by checkpoints */

    char best_checkpoint_path[CHECKPOINT_MAX_PATH]; /**< Path to best checkpoint */
    float best_metric_value;            /**< Best metric value */
} checkpoint_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Checkpoint manager context (opaque)
 */
typedef struct checkpoint_mgr_s checkpoint_mgr_t;

/**
 * @brief Loaded checkpoint data
 */
typedef struct {
    checkpoint_header_t header;         /**< Checkpoint metadata */

    /* Weight data */
    nimcp_tensor_t** weights;           /**< Array of weight tensors */
    char** weight_names;                /**< Names of each weight tensor */
    uint32_t num_weights;               /**< Number of weight tensors */

    /* Optimizer state */
    void* optimizer_state;              /**< Serialized optimizer state */
    size_t optimizer_state_size;        /**< Size of optimizer state */

    /* Scheduler state */
    void* scheduler_state;              /**< Serialized scheduler state */
    size_t scheduler_state_size;        /**< Size of scheduler state */

    /* Additional state */
    void* curriculum_state;             /**< Curriculum learning state */
    size_t curriculum_state_size;       /**< Size of curriculum state */

    /* RNG state */
    uint64_t rng_seed;                  /**< Main RNG seed */
    void* rng_state;                    /**< Full RNG state */
    size_t rng_state_size;              /**< Size of RNG state */

    /* Metrics history */
    float* train_losses;                /**< Training loss per epoch */
    float* val_losses;                  /**< Validation loss per epoch */
    uint32_t metrics_epochs;            /**< Number of epochs with metrics */
} checkpoint_data_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default checkpoint configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Simplify setup
 * HOW:  Save every 5 epochs, keep last 5, track best
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int checkpoint_default_config(checkpoint_config_t* config);

/**
 * @brief Create checkpoint manager
 *
 * WHAT: Initialize checkpoint management system
 * WHY:  Coordinate checkpoint saves and loads
 * HOW:  Create directory, scan existing checkpoints
 *
 * @param config Checkpoint configuration
 * @return Checkpoint manager or NULL on failure
 */
checkpoint_mgr_t* checkpoint_mgr_create(const checkpoint_config_t* config);

/**
 * @brief Destroy checkpoint manager
 *
 * @param mgr Manager to destroy (NULL-safe)
 */
void checkpoint_mgr_destroy(checkpoint_mgr_t* mgr);

//=============================================================================
// Save API
//=============================================================================

/**
 * @brief Save checkpoint
 *
 * WHAT: Save current training state to disk
 * WHY:  Persist state for recovery/resumption
 * HOW:  Serialize weights, optimizer, metrics to file
 *
 * @param mgr Checkpoint manager
 * @param weights Array of weight tensors
 * @param weight_names Names for each tensor
 * @param num_weights Number of tensors
 * @param epoch Current epoch
 * @param step Current global step
 * @param train_loss Current training loss
 * @param val_loss Current validation loss (or -1)
 * @return 0 on success, negative on error
 */
int training_checkpoint_save(
    checkpoint_mgr_t* mgr,
    nimcp_tensor_t** weights,
    const char** weight_names,
    uint32_t num_weights,
    uint64_t epoch,
    uint64_t step,
    float train_loss,
    float val_loss
);

/**
 * @brief Save checkpoint with optimizer state
 *
 * WHAT: Save weights + optimizer state
 * WHY:  Full training state for exact resumption
 * HOW:  Includes momentum, adaptive learning rate state
 *
 * @param mgr Checkpoint manager
 * @param weights Weight tensors
 * @param weight_names Tensor names
 * @param num_weights Number of tensors
 * @param optimizer_state Serialized optimizer state
 * @param optimizer_state_size Size of optimizer state
 * @param epoch Current epoch
 * @param step Current step
 * @param train_loss Training loss
 * @param val_loss Validation loss
 * @return 0 on success, negative on error
 */
int checkpoint_save_full(
    checkpoint_mgr_t* mgr,
    nimcp_tensor_t** weights,
    const char** weight_names,
    uint32_t num_weights,
    const void* optimizer_state,
    size_t optimizer_state_size,
    uint64_t epoch,
    uint64_t step,
    float train_loss,
    float val_loss
);

/**
 * @brief Check if checkpoint should be saved now
 *
 * WHAT: Check trigger condition
 * WHY:  Determine if it's time to save
 * HOW:  Check epoch/step/time since last save
 *
 * @param mgr Checkpoint manager
 * @param epoch Current epoch
 * @param step Current step
 * @param metric Current metric value (for improvement trigger)
 * @return true if checkpoint should be saved
 */
bool checkpoint_should_save(
    checkpoint_mgr_t* mgr,
    uint64_t epoch,
    uint64_t step,
    float metric
);

/**
 * @brief Mark checkpoint as best
 *
 * WHAT: Mark specified checkpoint as best
 * WHY:  Track best model for deployment
 * HOW:  Updates metadata, optionally copies to best.ckpt
 *
 * @param mgr Checkpoint manager
 * @param checkpoint_path Path to checkpoint
 * @return 0 on success, negative on error
 */
int checkpoint_mark_best(
    checkpoint_mgr_t* mgr,
    const char* checkpoint_path
);

//=============================================================================
// Load API
//=============================================================================

/**
 * @brief Load checkpoint from path
 *
 * WHAT: Load training state from checkpoint file
 * WHY:  Resume training or restore model
 * HOW:  Deserialize weights and state from file
 *
 * @param mgr Checkpoint manager (can be NULL for standalone load)
 * @param path Checkpoint file path
 * @param data Output checkpoint data (caller must call checkpoint_data_free)
 * @return 0 on success, negative on error
 */
int training_checkpoint_load(
    checkpoint_mgr_t* mgr,
    const char* path,
    checkpoint_data_t* data
);

/**
 * @brief Load latest checkpoint
 *
 * WHAT: Find and load most recent checkpoint
 * WHY:  Resume training from interruption
 * HOW:  Scan directory for newest checkpoint
 *
 * @param mgr Checkpoint manager
 * @param data Output checkpoint data
 * @return 0 on success, NIMCP_ERROR_NOT_FOUND if none exist
 */
int checkpoint_load_latest(
    checkpoint_mgr_t* mgr,
    checkpoint_data_t* data
);

/**
 * @brief Load best checkpoint
 *
 * WHAT: Load checkpoint with best metric
 * WHY:  Deploy best model
 * HOW:  Load checkpoint marked as best
 *
 * @param mgr Checkpoint manager
 * @param data Output checkpoint data
 * @return 0 on success, NIMCP_ERROR_NOT_FOUND if none exist
 */
int checkpoint_load_best(
    checkpoint_mgr_t* mgr,
    checkpoint_data_t* data
);

/**
 * @brief Load weights only from checkpoint
 *
 * WHAT: Load only model weights, skip optimizer/scheduler
 * WHY:  Fine-tuning, evaluation, or transfer learning
 * HOW:  Partial deserialize
 *
 * @param path Checkpoint file path
 * @param weights Output weight tensor array
 * @param weight_names Output weight names
 * @param num_weights Output number of weights
 * @return 0 on success, negative on error
 */
int checkpoint_load_weights_only(
    const char* path,
    nimcp_tensor_t*** weights,
    char*** weight_names,
    uint32_t* num_weights
);

/**
 * @brief Free checkpoint data
 *
 * WHAT: Free loaded checkpoint resources
 * WHY:  Prevent memory leaks
 * HOW:  Free tensors, state buffers
 *
 * @param data Checkpoint data to free
 */
void checkpoint_data_free(checkpoint_data_t* data);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get checkpoint header without loading full data
 *
 * WHAT: Read checkpoint metadata
 * WHY:  Quick inspection without full load
 * HOW:  Read and parse header only
 *
 * @param path Checkpoint file path
 * @param header Output header
 * @return 0 on success, negative on error
 */
int checkpoint_read_header(
    const char* path,
    checkpoint_header_t* header
);

/**
 * @brief Verify checkpoint integrity
 *
 * WHAT: Check checkpoint file validity
 * WHY:  Detect corruption before loading
 * HOW:  Verify magic, version, checksum
 *
 * @param path Checkpoint file path
 * @return Checkpoint status
 */
checkpoint_status_t checkpoint_verify(const char* path);

/**
 * @brief List checkpoints in directory
 *
 * WHAT: Get list of available checkpoints
 * WHY:  Enumerate available restore points
 * HOW:  Scan directory for checkpoint files
 *
 * @param mgr Checkpoint manager
 * @param paths Output array of paths (caller must free)
 * @param headers Output array of headers (caller must free)
 * @param count Output: number of checkpoints found
 * @return 0 on success, negative on error
 */
int training_checkpoint_list(
    checkpoint_mgr_t* mgr,
    char*** paths,
    checkpoint_header_t** headers,
    uint32_t* count
);

/**
 * @brief Get path to best checkpoint
 *
 * @param mgr Checkpoint manager
 * @return Path to best checkpoint (or NULL if none)
 */
const char* checkpoint_get_best_path(const checkpoint_mgr_t* mgr);

/**
 * @brief Get path to latest checkpoint
 *
 * @param mgr Checkpoint manager
 * @return Path to latest checkpoint (or NULL if none)
 */
const char* checkpoint_get_latest_path(const checkpoint_mgr_t* mgr);

//=============================================================================
// Management API
//=============================================================================

/**
 * @brief Delete old checkpoints
 *
 * WHAT: Remove old checkpoints to free disk space
 * WHY:  Enforce keep_last_n policy
 * HOW:  Delete oldest checkpoints beyond limit
 *
 * @param mgr Checkpoint manager
 * @return Number of checkpoints deleted
 */
int checkpoint_cleanup(checkpoint_mgr_t* mgr);

/**
 * @brief Delete specific checkpoint
 *
 * @param path Checkpoint file path
 * @return 0 on success, negative on error
 */
int checkpoint_delete(const char* path);

/**
 * @brief Get checkpoint manager statistics
 *
 * @param mgr Checkpoint manager
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int checkpoint_get_stats(
    const checkpoint_mgr_t* mgr,
    checkpoint_stats_t* stats
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get trigger name
 *
 * @param trigger Trigger enum
 * @return String name
 */
const char* checkpoint_trigger_name(checkpoint_trigger_t trigger);

/**
 * @brief Get status name
 *
 * @param status Status enum
 * @return String name
 */
const char* checkpoint_status_name(checkpoint_status_t status);

/**
 * @brief Generate checkpoint filename
 *
 * WHAT: Create checkpoint filename from parameters
 * WHY:  Consistent naming convention
 * HOW:  Combine prefix, epoch, timestamp, metric
 *
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param prefix Filename prefix
 * @param epoch Epoch number
 * @param metric Metric value (or -1 to omit)
 * @return 0 on success, negative on error
 */
int checkpoint_generate_filename(
    char* buffer,
    size_t buffer_size,
    const char* prefix,
    uint64_t epoch,
    float metric
);

/**
 * @brief Compare checkpoints by age
 *
 * WHAT: Compare two checkpoints by timestamp
 * WHY:  Sorting checkpoints by age
 * HOW:  Compare timestamp fields
 *
 * @param path_a First checkpoint path
 * @param path_b Second checkpoint path
 * @return <0 if a older, >0 if b older, 0 if same
 */
int checkpoint_compare_age(const char* path_a, const char* path_b);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_CHECKPOINT_H */
