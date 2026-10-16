/**
 * @file nimcp_training_data_pipeline.h
 * @brief Training Data Pipeline for NIMCP
 *
 * WHAT: Comprehensive data pipeline for efficient training data management
 * WHY:  Standardize data loading, batching, augmentation, and transformation
 * HOW:  Ring buffers for prefetching, worker threads for parallel loading,
 *       transform chains for augmentation
 *
 * BIOLOGICAL GROUNDING:
 * - Data pipeline mirrors sensory processing pathways
 * - Prefetching models anticipatory attention
 * - Augmentation models perceptual invariance learning
 * - Shuffling models hippocampal memory replay
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#ifndef NIMCP_TRAINING_DATA_PIPELINE_H
#define NIMCP_TRAINING_DATA_PIPELINE_H

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

#define TRAINING_PIPELINE_MAX_TRANSFORMS    16   /**< Maximum transform chain length */
#define TRAINING_PIPELINE_MAX_WORKERS       8    /**< Maximum prefetch workers */
#define TRAINING_PIPELINE_DEFAULT_PREFETCH  2    /**< Default prefetch buffer count */
#define TRAINING_PIPELINE_MAX_CACHE_MB      512  /**< Maximum cache size in MB */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Data transform types
 *
 * WHAT: Available data transformation operations
 * WHY:  Build augmentation pipelines for robust training
 * HOW:  Applied sequentially in transform chain
 */
typedef enum {
    TRANSFORM_NONE = 0,             /**< No transformation */
    TRANSFORM_NORMALIZE,            /**< Normalize to [0,1] or mean/std */
    TRANSFORM_STANDARDIZE,          /**< Zero-mean, unit variance */
    TRANSFORM_RANDOM_CROP,          /**< Random spatial crop */
    TRANSFORM_CENTER_CROP,          /**< Center crop */
    TRANSFORM_RESIZE,               /**< Resize spatial dimensions */
    TRANSFORM_RANDOM_FLIP_H,        /**< Random horizontal flip */
    TRANSFORM_RANDOM_FLIP_V,        /**< Random vertical flip */
    TRANSFORM_RANDOM_ROTATE,        /**< Random rotation */
    TRANSFORM_RANDOM_SCALE,         /**< Random scaling */
    TRANSFORM_RANDOM_BRIGHTNESS,    /**< Random brightness adjustment */
    TRANSFORM_RANDOM_CONTRAST,      /**< Random contrast adjustment */
    TRANSFORM_GAUSSIAN_NOISE,       /**< Add Gaussian noise */
    TRANSFORM_SALT_PEPPER_NOISE,    /**< Add salt/pepper noise */
    TRANSFORM_CUTOUT,               /**< Random cutout/erasing */
    TRANSFORM_MIXUP,                /**< MixUp augmentation */
    TRANSFORM_CUTMIX,               /**< CutMix augmentation */
    TRANSFORM_TO_TENSOR,            /**< Convert to tensor format */
    TRANSFORM_PAD,                  /**< Zero-padding */
    TRANSFORM_CUSTOM,               /**< Custom transform function */
    TRANSFORM_TYPE_COUNT
} training_transform_type_t;

/**
 * @brief Sampling strategy for data loading
 */
typedef enum {
    SAMPLING_SEQUENTIAL = 0,        /**< Load in order */
    SAMPLING_RANDOM,                /**< Random sampling with replacement */
    SAMPLING_SHUFFLE,               /**< Shuffle once per epoch */
    SAMPLING_WEIGHTED,              /**< Weighted sampling */
    SAMPLING_STRATIFIED,            /**< Stratified by class */
    SAMPLING_CURRICULUM,            /**< Curriculum-based ordering */
    SAMPLING_STRATEGY_COUNT
} training_sampling_strategy_t;

/**
 * @brief Pipeline state
 */
typedef enum {
    PIPELINE_STATE_IDLE = 0,        /**< Not running */
    PIPELINE_STATE_LOADING,         /**< Loading data */
    PIPELINE_STATE_READY,           /**< Batch ready */
    PIPELINE_STATE_EXHAUSTED,       /**< Epoch complete */
    PIPELINE_STATE_ERROR,           /**< Error occurred */
    PIPELINE_STATE_COUNT
} training_pipeline_state_t;

//=============================================================================
// Transform Configuration Structures
//=============================================================================

/**
 * @brief Normalization transform configuration
 */
typedef struct {
    float* mean;                    /**< Per-channel mean (NULL for min-max) */
    float* std;                     /**< Per-channel std (NULL for min-max) */
    uint32_t num_channels;          /**< Number of channels */
    float min_val;                  /**< Target minimum (min-max normalization) */
    float max_val;                  /**< Target maximum (min-max normalization) */
} transform_normalize_config_t;

/**
 * @brief Crop transform configuration
 */
typedef struct {
    uint32_t crop_height;           /**< Output height */
    uint32_t crop_width;            /**< Output width */
    float scale_min;                /**< Minimum scale for random crop */
    float scale_max;                /**< Maximum scale for random crop */
    float ratio_min;                /**< Minimum aspect ratio */
    float ratio_max;                /**< Maximum aspect ratio */
} transform_crop_config_t;

/**
 * @brief Resize transform configuration
 */
typedef struct {
    uint32_t target_height;         /**< Target height */
    uint32_t target_width;          /**< Target width */
    bool preserve_aspect;           /**< Preserve aspect ratio */
    bool use_bilinear;              /**< Use bilinear interpolation */
} transform_resize_config_t;

/**
 * @brief Rotation transform configuration
 */
typedef struct {
    float min_degrees;              /**< Minimum rotation angle */
    float max_degrees;              /**< Maximum rotation angle */
    float fill_value;               /**< Value for rotated pixels outside bounds */
} transform_rotate_config_t;

/**
 * @brief Color jitter configuration
 */
typedef struct {
    float brightness_factor;        /**< Brightness jitter range */
    float contrast_factor;          /**< Contrast jitter range */
    float saturation_factor;        /**< Saturation jitter range */
    float hue_factor;               /**< Hue jitter range */
} transform_color_config_t;

/**
 * @brief Noise injection configuration
 */
typedef struct {
    float noise_stddev;             /**< Gaussian noise standard deviation */
    float salt_prob;                /**< Salt (white pixel) probability */
    float pepper_prob;              /**< Pepper (black pixel) probability */
} transform_noise_config_t;

/**
 * @brief Cutout/erasing configuration
 */
typedef struct {
    float min_area;                 /**< Minimum erased area fraction */
    float max_area;                 /**< Maximum erased area fraction */
    float min_ratio;                /**< Minimum erase aspect ratio */
    float max_ratio;                /**< Maximum erase aspect ratio */
    float fill_value;               /**< Fill value (or -1 for random) */
    uint32_t max_erases;            /**< Maximum number of erases */
} transform_cutout_config_t;

/**
 * @brief MixUp configuration
 */
typedef struct {
    float alpha;                    /**< Beta distribution alpha parameter */
} transform_mixup_config_t;

/**
 * @brief Custom transform function signature
 *
 * @param input Input tensor (modified in-place or returns new tensor)
 * @param label Label tensor (modified for mixup-style augmentation)
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*custom_transform_fn)(
    nimcp_tensor_t** input,
    nimcp_tensor_t** label,
    void* user_data
);

/**
 * @brief Single transform specification
 */
typedef struct {
    training_transform_type_t type;  /**< Transform type */
    float probability;               /**< Probability of applying [0,1] */
    union {
        transform_normalize_config_t normalize;
        transform_crop_config_t crop;
        transform_resize_config_t resize;
        transform_rotate_config_t rotate;
        transform_color_config_t color;
        transform_noise_config_t noise;
        transform_cutout_config_t cutout;
        transform_mixup_config_t mixup;
    } config;
    custom_transform_fn custom_fn;   /**< Custom transform function */
    void* custom_data;               /**< Custom transform user data */
} training_transform_t;

//=============================================================================
// Pipeline Configuration
//=============================================================================

/**
 * @brief Data pipeline configuration
 */
typedef struct {
    /* Batching */
    uint32_t batch_size;            /**< Samples per batch */
    bool drop_last;                 /**< Drop incomplete final batch */

    /* Sampling */
    training_sampling_strategy_t sampling;  /**< Sampling strategy */
    float* sample_weights;          /**< Weights for weighted sampling (NULL if unused) */
    uint32_t num_weights;           /**< Number of sample weights */

    /* Shuffling */
    bool shuffle;                   /**< Enable shuffling */
    uint64_t shuffle_seed;          /**< Random seed (0 = use time) */
    uint32_t shuffle_buffer_size;   /**< Buffer size for streaming shuffle */

    /* Prefetching */
    bool enable_prefetch;           /**< Enable background prefetching */
    uint32_t prefetch_factor;       /**< Number of batches to prefetch */
    uint32_t num_workers;           /**< Number of worker threads */

    /* Transforms */
    training_transform_t* transforms;  /**< Transform chain */
    uint32_t num_transforms;        /**< Number of transforms */

    /* Caching */
    bool cache_dataset;             /**< Cache entire dataset in memory */
    bool cache_transforms;          /**< Cache transformed samples */
    size_t max_cache_bytes;         /**< Maximum cache size */

    /* Memory */
    bool pin_memory;                /**< Pin memory for faster GPU transfer */
    bool use_memory_pool;           /**< Use pooled memory allocation */

    /* Multi-epoch */
    bool repeat_forever;            /**< Repeat dataset indefinitely */
    uint32_t max_epochs;            /**< Maximum epochs (0 = unlimited) */
} training_pipeline_config_t;

//=============================================================================
// Statistics and State
//=============================================================================

/**
 * @brief Pipeline statistics
 */
typedef struct {
    uint64_t total_samples_loaded;  /**< Total samples loaded */
    uint64_t total_batches_produced; /**< Total batches produced */
    uint64_t current_epoch;         /**< Current epoch number */
    uint64_t samples_in_epoch;      /**< Samples seen in current epoch */

    /* Timing */
    double avg_batch_load_time_ms;  /**< Average batch load time */
    double avg_transform_time_ms;   /**< Average transform time */
    double total_wait_time_ms;      /**< Total time waiting for data */

    /* Cache statistics */
    uint64_t cache_hits;            /**< Number of cache hits */
    uint64_t cache_misses;          /**< Number of cache misses */
    size_t cache_memory_used;       /**< Current cache memory usage */

    /* Transform statistics */
    uint64_t transforms_applied;    /**< Total transforms applied */
    uint64_t transforms_skipped;    /**< Transforms skipped (probability) */

    /* Error tracking */
    uint64_t load_errors;           /**< Number of load errors */
    uint64_t transform_errors;      /**< Number of transform errors */
} training_pipeline_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Training data pipeline context (opaque)
 */
typedef struct training_pipeline_ctx_s training_pipeline_ctx_t;

/**
 * @brief Training batch structure
 */
typedef struct {
    nimcp_tensor_t* data;           /**< Batch data tensor [batch, ...] */
    nimcp_tensor_t* labels;         /**< Batch labels tensor [batch, ...] */
    uint32_t batch_size;            /**< Actual batch size (may be < config for last batch) */
    uint32_t* indices;              /**< Original sample indices */
    uint64_t batch_id;              /**< Sequential batch ID */
    uint64_t epoch;                 /**< Epoch number */
    bool is_last_batch;             /**< True if last batch in epoch */
} training_batch_t;

//=============================================================================
// Pipeline Lifecycle
//=============================================================================

/**
 * @brief Get default pipeline configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Simplify pipeline setup
 * HOW:  Set batch size 32, shuffle enabled, 2x prefetch
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int training_pipeline_default_config(training_pipeline_config_t* config);

/**
 * @brief Create training data pipeline
 *
 * WHAT: Allocate and initialize pipeline with dataset
 * WHY:  Set up efficient data loading infrastructure
 * HOW:  Create workers, allocate buffers, prepare transforms
 *
 * @param data Full dataset tensor [num_samples, ...]
 * @param labels Full labels tensor [num_samples, ...]
 * @param config Pipeline configuration
 * @return Pipeline context or NULL on failure
 */
training_pipeline_ctx_t* training_pipeline_create(
    const nimcp_tensor_t* data,
    const nimcp_tensor_t* labels,
    const training_pipeline_config_t* config
);

/**
 * @brief Create pipeline from file paths
 *
 * WHAT: Create pipeline that loads data from disk lazily
 * WHY:  Support datasets too large for memory
 * HOW:  Memory-mapped files or streaming reads
 *
 * @param data_paths Array of file paths to data files
 * @param label_paths Array of file paths to label files
 * @param num_files Number of files
 * @param config Pipeline configuration
 * @return Pipeline context or NULL on failure
 */
training_pipeline_ctx_t* training_pipeline_create_from_files(
    const char** data_paths,
    const char** label_paths,
    uint32_t num_files,
    const training_pipeline_config_t* config
);

/**
 * @brief Destroy training pipeline
 *
 * WHAT: Free all pipeline resources
 * WHY:  Prevent memory leaks
 * HOW:  Stop workers, free buffers, release cache
 *
 * @param pipeline Pipeline to destroy (NULL-safe)
 */
void training_pipeline_destroy(training_pipeline_ctx_t* pipeline);

//=============================================================================
// Batch Access API
//=============================================================================

/**
 * @brief Get next batch from pipeline
 *
 * WHAT: Retrieve next batch of training data
 * WHY:  Main interface for training loop
 * HOW:  Returns prefetched batch or loads synchronously
 *
 * @param pipeline Pipeline context
 * @param batch Output batch structure (caller must call training_batch_release)
 * @return 0 on success, NIMCP_ERROR_NOT_FOUND if epoch exhausted, negative on error
 */
int training_pipeline_next_batch(
    training_pipeline_ctx_t* pipeline,
    training_batch_t* batch
);

/**
 * @brief Peek at next batch without consuming
 *
 * WHAT: Look at next batch without advancing iterator
 * WHY:  Preview data for debugging or lookahead
 * HOW:  Returns reference to prefetch buffer
 *
 * @param pipeline Pipeline context
 * @param batch Output batch structure (read-only, do not release)
 * @return 0 on success, negative on error
 */
int training_pipeline_peek_batch(
    const training_pipeline_ctx_t* pipeline,
    training_batch_t* batch
);

/**
 * @brief Release batch resources
 *
 * WHAT: Free batch tensors after use
 * WHY:  Return memory to pool
 * HOW:  Marks batch as available for reuse
 *
 * @param batch Batch to release
 */
void training_batch_release(training_batch_t* batch);

/**
 * @brief Check if more batches available in current epoch
 *
 * @param pipeline Pipeline context
 * @return true if more batches available
 */
bool training_pipeline_has_more(const training_pipeline_ctx_t* pipeline);

/**
 * @brief Get number of batches per epoch
 *
 * @param pipeline Pipeline context
 * @return Number of batches in one epoch
 */
uint32_t training_pipeline_num_batches(const training_pipeline_ctx_t* pipeline);

//=============================================================================
// Epoch Management
//=============================================================================

/**
 * @brief Reset pipeline for new epoch
 *
 * WHAT: Reset iterator to beginning, reshuffle if enabled
 * WHY:  Start next training epoch
 * HOW:  Reset index, shuffle indices, clear prefetch
 *
 * @param pipeline Pipeline context
 * @return 0 on success, negative on error
 */
int training_pipeline_reset_epoch(training_pipeline_ctx_t* pipeline);

/**
 * @brief Get current epoch number
 *
 * @param pipeline Pipeline context
 * @return Current epoch (0-indexed)
 */
uint64_t training_pipeline_get_epoch(const training_pipeline_ctx_t* pipeline);

/**
 * @brief Set random seed for shuffling
 *
 * WHAT: Set seed for reproducible shuffling
 * WHY:  Reproducibility in experiments
 * HOW:  Reseeds internal RNG
 *
 * @param pipeline Pipeline context
 * @param seed Random seed
 * @return 0 on success, negative on error
 */
int training_pipeline_set_seed(training_pipeline_ctx_t* pipeline, uint64_t seed);

//=============================================================================
// Transform Management
//=============================================================================

/**
 * @brief Add transform to pipeline
 *
 * WHAT: Append transform to transform chain
 * WHY:  Build augmentation pipeline dynamically
 * HOW:  Adds to end of transform list
 *
 * @param pipeline Pipeline context
 * @param transform Transform to add
 * @return 0 on success, negative on error
 */
int training_pipeline_add_transform(
    training_pipeline_ctx_t* pipeline,
    const training_transform_t* transform
);

/**
 * @brief Clear all transforms
 *
 * @param pipeline Pipeline context
 */
void training_pipeline_clear_transforms(training_pipeline_ctx_t* pipeline);

/**
 * @brief Set transform chain
 *
 * WHAT: Replace entire transform chain
 * WHY:  Configure augmentation pipeline
 * HOW:  Copies transform array
 *
 * @param pipeline Pipeline context
 * @param transforms Array of transforms
 * @param num_transforms Number of transforms
 * @return 0 on success, negative on error
 */
int training_pipeline_set_transforms(
    training_pipeline_ctx_t* pipeline,
    const training_transform_t* transforms,
    uint32_t num_transforms
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get pipeline statistics
 *
 * @param pipeline Pipeline context
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int training_pipeline_get_stats(
    const training_pipeline_ctx_t* pipeline,
    training_pipeline_stats_t* stats
);

/**
 * @brief Reset pipeline statistics
 *
 * @param pipeline Pipeline context
 */
void training_pipeline_reset_stats(training_pipeline_ctx_t* pipeline);

/**
 * @brief Get pipeline state
 *
 * @param pipeline Pipeline context
 * @return Current pipeline state
 */
training_pipeline_state_t training_pipeline_get_state(
    const training_pipeline_ctx_t* pipeline
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get transform type name
 *
 * @param type Transform type enum
 * @return String name
 */
const char* training_transform_type_name(training_transform_type_t type);

/**
 * @brief Get sampling strategy name
 *
 * @param strategy Sampling strategy enum
 * @return String name
 */
const char* training_sampling_strategy_name(training_sampling_strategy_t strategy);

/**
 * @brief Create standard image augmentation transforms
 *
 * WHAT: Create common image augmentation chain
 * WHY:  Quick setup for vision tasks
 * HOW:  Normalize, random crop, flip, color jitter
 *
 * @param transforms Output array (must have space for at least 8 transforms)
 * @param num_transforms Output: number of transforms created
 * @param image_size Target image size
 * @return 0 on success, negative on error
 */
int training_pipeline_create_image_augmentation(
    training_transform_t* transforms,
    uint32_t* num_transforms,
    uint32_t image_size
);

/**
 * @brief Create standard audio augmentation transforms
 *
 * WHAT: Create common audio augmentation chain
 * WHY:  Quick setup for audio tasks
 * HOW:  Normalize, noise, time stretch (if supported)
 *
 * @param transforms Output array
 * @param num_transforms Output: number of transforms created
 * @return 0 on success, negative on error
 */
int training_pipeline_create_audio_augmentation(
    training_transform_t* transforms,
    uint32_t* num_transforms
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_DATA_PIPELINE_H */
