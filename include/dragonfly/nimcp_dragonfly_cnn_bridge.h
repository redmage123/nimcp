/**
 * @file nimcp_dragonfly_cnn_bridge.h
 * @brief Dragonfly-to-CNN Training Bridge
 *
 * WHAT: Bridges dragonfly visual processing to CNN training pipeline
 * WHY:  Train motion detection and target recognition with backpropagation
 * HOW:  Extract training data from dragonfly, convert to CNN format, train networks
 *
 * BIOLOGICAL BASIS:
 * - Dragonfly lobula neurons develop motion selectivity through experience
 * - Retinal preprocessing extracts features before TSDN processing
 * - This bridge allows CNN pretraining of motion detectors that convert to SNN
 *
 * TRAINING SCENARIOS:
 * 1. Motion Detection: Train CNN to detect moving targets in visual field
 * 2. Target Classification: Distinguish prey types (fast vs slow, small vs large)
 * 3. Background Subtraction: Learn to ignore stationary objects
 * 4. Trajectory Estimation: Predict future target position from motion history
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_CNN_BRIDGE_H
#define NIMCP_DRAGONFLY_CNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_CNN_MAX_FRAMES 16         /**< Max frames in motion history */
#define DRAGONFLY_CNN_FEATURE_DIM 128       /**< Default feature vector size */
#define DRAGONFLY_CNN_MAX_TARGETS 8         /**< Max simultaneous training targets */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief CNN training task types for dragonfly
 */
typedef enum {
    CNN_TASK_MOTION_DETECTION = 0,      /**< Binary: is target moving? */
    CNN_TASK_VELOCITY_ESTIMATION,       /**< Regression: target velocity vector */
    CNN_TASK_TARGET_CLASSIFICATION,     /**< Multi-class: target type */
    CNN_TASK_SALIENCY_PREDICTION,       /**< Heatmap: attention priority */
    CNN_TASK_TRAJECTORY_PREDICTION,     /**< Sequence: future positions */
    CNN_TASK_EVASION_DETECTION,         /**< Binary: is target evading? */
    CNN_TASK_TYPE_COUNT
} dragonfly_cnn_task_t;

/**
 * @brief Training data source
 */
typedef enum {
    CNN_DATA_SYNTHETIC = 0,             /**< Generated synthetic targets */
    CNN_DATA_RECORDED,                  /**< Recorded tracking sessions */
    CNN_DATA_AUGMENTED,                 /**< Augmented from existing data */
    CNN_DATA_ONLINE                     /**< Real-time tracking data */
} dragonfly_cnn_data_source_t;

/**
 * @brief Feature extraction mode
 */
typedef enum {
    CNN_FEATURE_RAW_FRAMES = 0,         /**< Raw visual frames */
    CNN_FEATURE_MOTION_VECTORS,         /**< Computed motion vectors */
    CNN_FEATURE_TSDN_ACTIVATIONS,       /**< TSDN population response */
    CNN_FEATURE_TRACKING_STATE,         /**< Full tracking state */
    CNN_FEATURE_HYBRID                  /**< Multiple feature types */
} dragonfly_cnn_feature_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Visual frame for CNN input
 */
typedef struct {
    float* data;                /**< Frame pixel data */
    uint32_t width;             /**< Frame width */
    uint32_t height;            /**< Frame height */
    uint32_t channels;          /**< Number of channels */
    float timestamp_ms;         /**< Frame timestamp */
} dragonfly_cnn_frame_t;

/**
 * @brief Motion history for temporal CNN
 */
typedef struct {
    dragonfly_cnn_frame_t frames[DRAGONFLY_CNN_MAX_FRAMES];
    uint32_t num_frames;        /**< Current frames in history */
    uint32_t max_frames;        /**< Maximum history size */
    float frame_interval_ms;    /**< Time between frames */
} dragonfly_cnn_motion_history_t;

/**
 * @brief Training sample for CNN
 */
typedef struct {
    float* input;               /**< Input features */
    uint32_t input_dims[4];     /**< [batch, channels, height, width] */
    float* target;              /**< Target output */
    uint32_t target_dims[2];    /**< [batch, output_size] */
    float weight;               /**< Sample weight for loss */
    uint32_t class_label;       /**< For classification tasks */
    float interception_success; /**< Reward signal */
} dragonfly_cnn_sample_t;

/**
 * @brief CNN bridge configuration
 */
typedef struct {
    dragonfly_cnn_task_t task;              /**< Training task type */
    dragonfly_cnn_data_source_t data_source; /**< Where data comes from */
    dragonfly_cnn_feature_mode_t feature_mode; /**< Feature extraction mode */

    /* Input configuration */
    uint32_t frame_width;                   /**< Input frame width */
    uint32_t frame_height;                  /**< Input frame height */
    uint32_t motion_history_frames;         /**< Frames for temporal input */
    float frame_sample_rate_hz;             /**< Frame sampling rate */

    /* Training parameters */
    uint32_t batch_size;                    /**< Training batch size */
    float learning_rate;                    /**< Base learning rate */
    float reward_scale;                     /**< Scale for reward signal */
    bool use_reward_shaping;                /**< Shape rewards for sparse signal */

    /* Data augmentation */
    bool augment_flip;                      /**< Horizontal flip augmentation */
    bool augment_rotate;                    /**< Rotation augmentation */
    bool augment_scale;                     /**< Scale augmentation */
    bool augment_noise;                     /**< Noise injection */
    float augment_probability;              /**< Probability of augmentation */

    /* SNN conversion */
    bool enable_snn_conversion;             /**< Prepare for SNN conversion */
    float target_firing_rate;               /**< Target SNN firing rate (Hz) */
} dragonfly_cnn_config_t;

/**
 * @brief Training statistics
 */
typedef struct {
    uint64_t samples_processed;
    uint64_t batches_processed;
    uint64_t epochs_completed;
    float current_loss;
    float average_loss;
    float min_loss;
    float detection_accuracy;
    float velocity_mse;
    float trajectory_error;
    float avg_interception_reward;
    float training_time_sec;
} dragonfly_cnn_stats_t;

/**
 * @brief CNN bridge handle
 */
typedef struct dragonfly_cnn_bridge_s dragonfly_cnn_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize default CNN bridge configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_bridge_default_config(dragonfly_cnn_config_t* config);

/**
 * @brief Validate CNN bridge configuration
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_cnn_bridge_validate_config(const dragonfly_cnn_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create CNN training bridge
 * @param dragonfly Dragonfly system (may be NULL)
 * @param cnn_trainer CNN trainer handle (may be NULL)
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
dragonfly_cnn_bridge_t* dragonfly_cnn_bridge_create(
    dragonfly_system_t* dragonfly,
    void* cnn_trainer,
    const dragonfly_cnn_config_t* config
);

/**
 * @brief Destroy CNN bridge
 * @param bridge Bridge to destroy
 */
void dragonfly_cnn_bridge_destroy(dragonfly_cnn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_bridge_reset(dragonfly_cnn_bridge_t* bridge);

//=============================================================================
// Data Collection
//=============================================================================

/**
 * @brief Add visual frame to motion history
 * @param bridge CNN bridge
 * @param frame Visual frame to add
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_add_frame(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_frame_t* frame
);

/**
 * @brief Record tracking episode for training
 * @param bridge CNN bridge
 * @param success Whether interception was successful
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_record_episode(
    dragonfly_cnn_bridge_t* bridge,
    bool success
);

/**
 * @brief Extract features from dragonfly state
 * @param bridge CNN bridge
 * @param features Output feature buffer
 * @param feature_dim Feature dimension
 * @return Number of features extracted, -1 on error
 */
int dragonfly_cnn_extract_features(
    dragonfly_cnn_bridge_t* bridge,
    float* features,
    uint32_t feature_dim
);

/**
 * @brief Generate synthetic training target
 * @param bridge CNN bridge
 * @param sample Output training sample
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_generate_sample(
    dragonfly_cnn_bridge_t* bridge,
    dragonfly_cnn_sample_t* sample
);

//=============================================================================
// Training
//=============================================================================

/**
 * @brief Run one training step
 * @param bridge CNN bridge
 * @return Current loss, -1.0 on error
 */
float dragonfly_cnn_train_step(dragonfly_cnn_bridge_t* bridge);

/**
 * @brief Train on batch of samples
 * @param bridge CNN bridge
 * @param samples Training samples
 * @param num_samples Number of samples
 * @return Average loss, -1.0 on error
 */
float dragonfly_cnn_train_batch(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_sample_t* samples,
    uint32_t num_samples
);

/**
 * @brief Evaluate on validation data
 * @param bridge CNN bridge
 * @param samples Validation samples
 * @param num_samples Number of samples
 * @return Validation loss, -1.0 on error
 */
float dragonfly_cnn_evaluate(
    dragonfly_cnn_bridge_t* bridge,
    const dragonfly_cnn_sample_t* samples,
    uint32_t num_samples
);

/**
 * @brief Update learning rate
 * @param bridge CNN bridge
 * @param lr New learning rate
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_set_learning_rate(
    dragonfly_cnn_bridge_t* bridge,
    float lr
);

//=============================================================================
// Inference
//=============================================================================

/**
 * @brief Run inference on current state
 * @param bridge CNN bridge
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return Number of outputs, -1 on error
 */
int dragonfly_cnn_infer(
    dragonfly_cnn_bridge_t* bridge,
    float* output,
    uint32_t output_size
);

/**
 * @brief Detect motion in current frame
 * @param bridge CNN bridge
 * @return Motion probability [0-1], -1.0 on error
 */
float dragonfly_cnn_detect_motion(dragonfly_cnn_bridge_t* bridge);

/**
 * @brief Estimate target velocity
 * @param bridge CNN bridge
 * @param vx Output X velocity
 * @param vy Output Y velocity
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_estimate_velocity(
    dragonfly_cnn_bridge_t* bridge,
    float* vx,
    float* vy
);

/**
 * @brief Predict target trajectory
 * @param bridge CNN bridge
 * @param predictions Output prediction buffer
 * @param num_steps Number of future steps to predict
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_predict_trajectory(
    dragonfly_cnn_bridge_t* bridge,
    float* predictions,
    uint32_t num_steps
);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to dragonfly system
 * @param bridge CNN bridge
 * @param dragonfly Dragonfly system
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_connect_dragonfly(
    dragonfly_cnn_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

/**
 * @brief Connect to CNN trainer
 * @param bridge CNN bridge
 * @param trainer CNN trainer handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_connect_trainer(
    dragonfly_cnn_bridge_t* bridge,
    void* trainer
);

/**
 * @brief Check if training is active
 * @param bridge CNN bridge
 * @return true if training
 */
bool dragonfly_cnn_is_training(const dragonfly_cnn_bridge_t* bridge);

/**
 * @brief Set training mode
 * @param bridge CNN bridge
 * @param training Enable/disable training mode
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_set_training(dragonfly_cnn_bridge_t* bridge, bool training);

//=============================================================================
// SNN Conversion
//=============================================================================

/**
 * @brief Prepare CNN weights for SNN conversion
 * @param bridge CNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_prepare_snn_conversion(dragonfly_cnn_bridge_t* bridge);

/**
 * @brief Get normalized activation statistics
 * @param bridge CNN bridge
 * @param layer_idx Layer index
 * @param mean Output mean activation
 * @param std Output std activation
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_get_activation_stats(
    dragonfly_cnn_bridge_t* bridge,
    uint32_t layer_idx,
    float* mean,
    float* std
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get training statistics
 * @param bridge CNN bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_bridge_get_stats(
    const dragonfly_cnn_bridge_t* bridge,
    dragonfly_cnn_stats_t* stats
);

/**
 * @brief Reset training statistics
 * @param bridge CNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_cnn_bridge_reset_stats(dragonfly_cnn_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Get task name
 * @param task Task type
 * @return Task name string
 */
const char* dragonfly_cnn_task_name(dragonfly_cnn_task_t task);

/**
 * @brief Get feature mode name
 * @param mode Feature mode
 * @return Mode name string
 */
const char* dragonfly_cnn_feature_mode_name(dragonfly_cnn_feature_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_CNN_BRIDGE_H */
