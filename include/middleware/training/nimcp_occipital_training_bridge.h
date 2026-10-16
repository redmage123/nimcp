/**
 * @file nimcp_occipital_training_bridge.h
 * @brief Bridge between Occipital Cortex V1-V5 hierarchy and training pipeline
 *
 * WHAT: Connects occipital visual processing (V1-V5) to training system
 * WHY: Enable supervised learning for visual feature detectors and motion estimators
 * HOW: Collects visual confidence/novelty, modulates learning rates, provides targets
 *
 * BIOLOGICAL BASIS:
 * - V1 Gabor filters: Trainable orientation/spatial frequency selectivity
 * - V2 Association fields: Learnable contour integration
 * - V4 Color constancy: Adaptable illumination-invariant color processing
 * - V5/MT Motion: Trainable temporal filters for optic flow estimation
 *
 * TRAINING EFFECTS:
 * - High visual confidence → boost learning rate (clear signal, reliable gradients)
 * - High feature novelty → exploration mode (novelty-driven learning)
 * - Low confidence → conservative learning (noisy gradients)
 * - Motion stability → temporal filter training stability
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_OCCIPITAL_TRAINING_BRIDGE_H
#define NIMCP_OCCIPITAL_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct occipital_training_bridge occipital_training_bridge_t;
typedef struct occipital_adapter occipital_adapter_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Bio-async module ID */
#define BIO_MODULE_OCCIPITAL_TRAINING 0x2D20

/** Default configuration values */
#define OCCIPITAL_TRAINING_DEFAULT_UPDATE_INTERVAL_MS   100
#define OCCIPITAL_TRAINING_DEFAULT_LR_MIN_FACTOR        0.5f
#define OCCIPITAL_TRAINING_DEFAULT_LR_MAX_FACTOR        1.5f

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Visual area training mode
 */
typedef enum {
    OCCIPITAL_TRAIN_V1 = 0,       /**< Train V1 Gabor filters */
    OCCIPITAL_TRAIN_V2,           /**< Train V2 association fields */
    OCCIPITAL_TRAIN_V4,           /**< Train V4 color/form */
    OCCIPITAL_TRAIN_V5,           /**< Train V5 motion filters */
    OCCIPITAL_TRAIN_ALL,          /**< Train all areas */
    OCCIPITAL_TRAIN_COUNT
} occipital_training_area_t;

/**
 * @brief Training effects from occipital to training pipeline
 *
 * Captures V1-V5 state for learning rate and sample weight modulation.
 */
typedef struct {
    /* Per-area confidence [0-1] */
    float v1_confidence;          /**< V1 edge/orientation confidence */
    float v2_confidence;          /**< V2 contour integration confidence */
    float v4_confidence;          /**< V4 color/form confidence */
    float v5_confidence;          /**< V5 motion detection confidence */
    float overall_confidence;     /**< Weighted overall confidence */

    /* Novelty scores [0-1] */
    float feature_novelty;        /**< How novel are detected features */
    float motion_novelty;         /**< How novel is motion pattern */

    /* Motion stability [0-1] */
    float motion_stability;       /**< V5 motion estimation stability */
    float color_stability;        /**< V4 color constancy stability */

    /* Per-feature attention weights */
    float* attention_weights;     /**< Per-feature attention [num_features] */
    uint32_t num_features;        /**< Number of visual features */

    /* Computed modulations */
    float lr_factor;              /**< Learning rate multiplier [0.5-1.5] */
    float sample_weight;          /**< Sample importance [0.1-2.0] */
    bool skip_sample;             /**< Skip this sample (too noisy) */

    /* Metadata */
    uint64_t timestamp_ms;        /**< When effects were computed */
    bool valid;                   /**< Whether effects are current */
} occipital_training_effects_t;

/**
 * @brief Training targets for supervised visual learning
 */
typedef struct {
    /* V1 targets */
    float* target_orientations;   /**< Target orientation map */
    float* target_spatial_freq;   /**< Target spatial frequency map */
    uint32_t v1_target_count;     /**< Number of V1 targets */

    /* V2 targets */
    float* target_contours;       /**< Target contour map */
    uint32_t v2_target_count;     /**< Number of V2 targets */

    /* V4 targets */
    float* target_colors;         /**< Target color values */
    float* target_forms;          /**< Target form descriptors */
    uint32_t v4_target_count;     /**< Number of V4 targets */

    /* V5 targets */
    float* target_motion_dx;      /**< Target horizontal motion */
    float* target_motion_dy;      /**< Target vertical motion */
    uint32_t v5_target_count;     /**< Number of V5 targets */

    /* Metadata */
    float supervision_strength;   /**< How strongly to apply supervision [0-1] */
} occipital_training_targets_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Enable/disable per area */
    bool enable_v1_training;      /**< Train V1 filters */
    bool enable_v2_training;      /**< Train V2 association */
    bool enable_v4_training;      /**< Train V4 color/form */
    bool enable_v5_training;      /**< Train V5 motion */

    /* Modulation strengths */
    float confidence_lr_scale;    /**< How much confidence affects LR */
    float novelty_lr_scale;       /**< How much novelty affects LR */
    float stability_lr_scale;     /**< How much stability affects LR */

    /* LR limits */
    float lr_min_factor;          /**< Minimum LR multiplier */
    float lr_max_factor;          /**< Maximum LR multiplier */

    /* Skip thresholds */
    float skip_confidence_threshold;  /**< Skip if confidence < threshold */
    float skip_stability_threshold;   /**< Skip if stability < threshold */

    /* Update settings */
    uint32_t update_interval_ms;  /**< Update interval */
    bool enable_bio_async;        /**< Enable bio-async messaging */

    /* Integration flags */
    bool enable_perception_training;  /**< Integrate with perception training */
    bool enable_weight_update_router; /**< Route weight updates to optimizer */
} occipital_training_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Training counts */
    uint64_t total_training_steps;
    uint64_t v1_updates;
    uint64_t v2_updates;
    uint64_t v4_updates;
    uint64_t v5_updates;

    /* Modulation stats */
    uint64_t lr_increases;
    uint64_t lr_decreases;
    uint64_t samples_skipped;

    /* Average metrics */
    float avg_confidence;
    float avg_novelty;
    float avg_lr_factor;

    /* Connection status */
    bool occipital_connected;
    bool training_connected;
    bool bio_async_connected;

    /* Timing */
    uint64_t last_update_ms;
    float avg_update_time_us;
} occipital_training_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @param config Output configuration
 */
void occipital_training_default_config(occipital_training_config_t* config);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create occipital training bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
occipital_training_bridge_t* occipital_training_bridge_create(
    const occipital_training_config_t* config
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void occipital_training_bridge_destroy(occipital_training_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int occipital_training_bridge_reset(occipital_training_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to occipital adapter
 *
 * @param bridge Bridge to connect
 * @param occipital Occipital adapter handle
 * @return 0 on success, -1 on error
 */
int occipital_training_connect_occipital(
    occipital_training_bridge_t* bridge,
    occipital_adapter_t* occipital
);

/**
 * @brief Connect to training context
 *
 * @param bridge Bridge to connect
 * @param training Training context handle
 * @return 0 on success, -1 on error
 */
int occipital_training_connect_training(
    occipital_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training
);

/*=============================================================================
 * Training Effects API (Occipital -> Training)
 *===========================================================================*/

/**
 * @brief Update effects from occipital state
 *
 * Queries occipital adapter for V1-V5 confidence, novelty, stability.
 *
 * @param bridge Bridge to update
 * @return 0 on success, -1 on error
 */
int occipital_training_update_effects(occipital_training_bridge_t* bridge);

/**
 * @brief Get current training effects
 *
 * @param bridge Bridge to query
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int occipital_training_get_effects(
    const occipital_training_bridge_t* bridge,
    occipital_training_effects_t* effects
);

/**
 * @brief Get modulated learning rate for visual training
 *
 * @param bridge Bridge to query
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float occipital_training_get_modulated_lr(
    const occipital_training_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Check if sample should be skipped
 *
 * @param bridge Bridge to query
 * @return true if sample too noisy for training
 */
bool occipital_training_should_skip(
    const occipital_training_bridge_t* bridge
);

/**
 * @brief Get per-feature attention scaling
 *
 * @param bridge Bridge to query
 * @param factors Output scaling factors
 * @param num_features Number of features
 * @return 0 on success, -1 on error
 */
int occipital_training_get_attention_scaling(
    const occipital_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features
);

/*=============================================================================
 * Training Targets API (Supervision -> Occipital)
 *===========================================================================*/

/**
 * @brief Apply training targets to occipital
 *
 * Provides supervised targets for V1-V5 filter learning.
 *
 * @param bridge Bridge to update
 * @param targets Training targets
 * @return 0 on success, -1 on error
 */
int occipital_training_apply_targets(
    occipital_training_bridge_t* bridge,
    const occipital_training_targets_t* targets
);

/**
 * @brief Train specific visual area
 *
 * @param bridge Bridge to use
 * @param area Area to train
 * @param learning_rate Learning rate for this step
 * @return 0 on success, -1 on error
 */
int occipital_training_train_area(
    occipital_training_bridge_t* bridge,
    occipital_training_area_t area,
    float learning_rate
);

/**
 * @brief Compute training loss for visual area
 *
 * @param bridge Bridge to query
 * @param area Area to compute loss for
 * @param loss Output loss value
 * @return 0 on success, -1 on error
 */
int occipital_training_compute_loss(
    const occipital_training_bridge_t* bridge,
    occipital_training_area_t area,
    float* loss
);

/*=============================================================================
 * Update Cycle API
 *===========================================================================*/

/**
 * @brief Main update cycle
 *
 * @param bridge Bridge to update
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int occipital_training_update(
    occipital_training_bridge_t* bridge,
    uint64_t delta_ms
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int occipital_training_get_stats(
    const occipital_training_bridge_t* bridge,
    occipital_training_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void occipital_training_reset_stats(occipital_training_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_TRAINING_BRIDGE_H */
