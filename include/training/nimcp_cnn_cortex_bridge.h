/**
 * @file nimcp_cnn_cortex_bridge.h
 * @brief CNN Training Integration with Visual and Audio Cortexes
 *
 * WHAT: Bridge enabling CNN training to use perception cortex features as input
 *       and optionally propagate gradients back for STDP-based plasticity.
 *
 * WHY: Visual/audio cortexes provide biologically-realistic feature extraction
 *      (Gabor filters for V1, Mel filterbanks for A1). Using these as CNN input
 *      enables transfer learning from innate perceptual structure.
 *
 * HOW: Feature Extraction + Trainable Head approach:
 *      1. Use visual/audio cortex as frozen feature extractors
 *      2. Train CNN head on perception features (transfer learning)
 *      3. Optionally enable gradient feedback to cortex for STDP refinement
 *      4. Modulate learning rate based on perception quality
 *
 * BIOLOGICAL BASIS:
 * - V1/A1 have largely innate structure with plasticity for fine-tuning
 * - Gabor filters in V1 ≈ orientation-selective simple cells
 * - Mel filterbanks ≈ cochlear frequency decomposition
 * - Gradient feedback models top-down attention modulating sensory plasticity
 *
 * INTEGRATION:
 * - CNN Training Module: Provides trainable head on perception features
 * - Visual Cortex: V1-style feature extraction (Gabor convolution + pooling)
 * - Audio Cortex: A1-style feature extraction (FFT → Mel → MFCC)
 * - STDP: Gradient feedback converted to STDP signals for cortex plasticity
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 * @version 1.0.0
 */

#ifndef NIMCP_CNN_CORTEX_BRIDGE_H
#define NIMCP_CNN_CORTEX_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct cnn_cortex_bridge cnn_cortex_bridge_t;
typedef struct cnn_trainer_s cnn_trainer_t;
typedef struct visual_cortex_struct visual_cortex_struct_t;
typedef visual_cortex_struct_t* visual_cortex_handle_t;
typedef struct audio_cortex audio_cortex_struct_t;
typedef audio_cortex_struct_t* audio_cortex_handle_t;
typedef struct perception_training_bridge perception_training_bridge_t;

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define CNN_CORTEX_BRIDGE_MODULE_NAME      "cnn_cortex_bridge"
#define CNN_CORTEX_BRIDGE_MODULE_VERSION   "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_CNN_CORTEX_BRIDGE       0x0713

/** Bio-async message types */
#define BIO_MSG_CNN_CORTEX_FEATURES        0x0714  /**< Feature extraction complete */
#define BIO_MSG_CNN_CORTEX_GRADIENT        0x0715  /**< Gradient feedback available */
#define BIO_MSG_CNN_CORTEX_QUALITY         0x0716  /**< Perception quality update */

/** Default configuration values */
#define CNN_CORTEX_DEFAULT_GRADIENT_SCALE        0.01f
#define CNN_CORTEX_DEFAULT_LR_MIN_FACTOR         0.5f
#define CNN_CORTEX_DEFAULT_LR_MAX_FACTOR         1.5f
#define CNN_CORTEX_DEFAULT_CONFIDENCE_THRESHOLD  0.3f
#define CNN_CORTEX_DEFAULT_QUALITY_THRESHOLD     0.3f

/** Maximum feature dimensions */
#define CNN_CORTEX_MAX_VISUAL_FEATURES     4096
#define CNN_CORTEX_MAX_AUDIO_FEATURES      2048

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Operation mode for CNN-cortex bridge
 *
 * WHAT: How the bridge processes cortex features and gradients
 * WHY: Different modes for different use cases (inference vs training)
 * HOW: Mode controls feature extraction and gradient feedback behavior
 */
typedef enum {
    CNN_CORTEX_MODE_DISABLED = 0,       /**< Bridge disabled */
    CNN_CORTEX_MODE_FEATURE_ONLY,       /**< Extract features only (inference) */
    CNN_CORTEX_MODE_TRAINING,           /**< Feature extraction + gradient feedback */
    CNN_CORTEX_MODE_FINE_TUNING,        /**< Active gradient feedback to cortex STDP */
    CNN_CORTEX_MODE_COUNT
} cnn_cortex_mode_t;

/**
 * @brief Feature source priority when both cortexes connected
 *
 * WHAT: Which cortex takes priority for feature extraction
 * WHY: Some tasks are visual-dominant, others audio-dominant
 * HOW: Priority determines primary feature source
 */
typedef enum {
    CNN_CORTEX_PRIORITY_VISUAL = 0,     /**< Visual cortex primary */
    CNN_CORTEX_PRIORITY_AUDIO,          /**< Audio cortex primary */
    CNN_CORTEX_PRIORITY_MULTIMODAL,     /**< Concatenate both features */
    CNN_CORTEX_PRIORITY_COUNT
} cnn_cortex_priority_t;

/**
 * @brief Gradient feedback method for cortex plasticity
 *
 * WHAT: How CNN gradients are converted to STDP signals
 * WHY: Different methods for different biological plausibility requirements
 * HOW: Gradient magnitude/sign converted to STDP modulation
 *
 * BIOLOGICAL BASIS:
 * - MAGNITUDE: Larger gradients → stronger plasticity signal
 * - SIGN: Positive gradients → LTP, negative → LTD
 * - HEBBIAN: Combine gradient with activation for Hebbian learning
 */
typedef enum {
    CNN_CORTEX_GRADIENT_MAGNITUDE = 0,  /**< Use gradient magnitude as STDP signal */
    CNN_CORTEX_GRADIENT_SIGN,           /**< Use gradient sign (LTP/LTD direction) */
    CNN_CORTEX_GRADIENT_HEBBIAN,        /**< Hebbian: gradient × activation */
    CNN_CORTEX_GRADIENT_NONE,           /**< Disable gradient feedback */
    CNN_CORTEX_GRADIENT_METHOD_COUNT
} cnn_cortex_gradient_method_t;

//=============================================================================
// Training State Structures
//=============================================================================

/**
 * @brief Visual cortex training state for gradient feedback
 *
 * WHAT: Cached activations and outputs from visual cortex forward pass
 * WHY: Needed for gradient feedback to visual cortex STDP
 * HOW: Populated during feature extraction, used during backward pass
 */
typedef struct {
    float* conv_output;              /**< Cached conv layer output */
    uint32_t conv_output_size;       /**< Conv output size */
    float* pool_output;              /**< Cached pool layer output */
    uint32_t pool_output_size;       /**< Pool output size */
    float* feature_weights;          /**< Feature layer weights (read-only) */
    uint32_t feature_dim;            /**< Feature dimensionality */
    float confidence;                /**< Visual processing confidence [0-1] */
    float novelty;                   /**< Novelty score [0-1] */
    uint64_t timestamp_ms;           /**< When state was captured */
    bool valid;                      /**< State validity flag */
} visual_cortex_training_state_t;

/**
 * @brief Audio cortex training state for gradient feedback
 *
 * WHAT: Cached activations and outputs from audio cortex forward pass
 * WHY: Needed for gradient feedback to audio cortex internal network
 * HOW: Populated during feature extraction, used during backward pass
 */
typedef struct {
    float* mel_features;             /**< Cached mel filterbank features */
    uint32_t num_mel_filters;        /**< Number of mel filters */
    float* mfcc_features;            /**< Cached MFCC coefficients */
    uint32_t num_mfcc;               /**< Number of MFCCs */
    float quality;                   /**< Audio signal quality [0-1] */
    float speech_salience;           /**< Speech salience [0-1] */
    float temporal_coherence;        /**< Temporal coherence [0-1] */
    uint64_t timestamp_ms;           /**< When state was captured */
    bool valid;                      /**< State validity flag */
} audio_cortex_training_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for CNN-cortex bridge
 *
 * WHAT: All configurable parameters for the bridge
 * WHY: Allow customization for different training scenarios
 * HOW: Passed to create function, copied internally
 */
typedef struct {
    cnn_cortex_mode_t mode;                 /**< Operation mode */
    cnn_cortex_priority_t priority;         /**< Feature source priority */

    /* === Cortex Enables === */
    bool freeze_cortex_weights;             /**< Freeze cortex weights (default: true) */
    bool enable_gradient_feedback;          /**< Enable gradient feedback to cortex STDP */
    cnn_cortex_gradient_method_t gradient_method; /**< Gradient conversion method */
    float gradient_feedback_scale;          /**< Scale factor for gradient signals */

    /* === Learning Rate Modulation === */
    bool enable_perception_modulation;      /**< Modulate LR by perception quality */
    float lr_min_factor;                    /**< Minimum LR multiplier */
    float lr_max_factor;                    /**< Maximum LR multiplier */
    float confidence_scale;                 /**< How much confidence affects LR */
    float quality_scale;                    /**< How much quality affects LR */

    /* === Quality Thresholds === */
    float visual_confidence_threshold;      /**< Min confidence for training */
    float audio_quality_threshold;          /**< Min quality for training */
    bool skip_low_quality_samples;          /**< Skip samples below threshold */

    /* === Feature Caching === */
    bool cache_features;                    /**< Cache extracted features */
    uint32_t cache_size;                    /**< Feature cache size (samples) */

    /* === Integration === */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    bool integrate_perception_bridge;       /**< Coordinate with perception-training bridge */

    /* === Update Settings === */
    uint32_t update_interval_ms;            /**< Update interval in milliseconds */
} cnn_cortex_bridge_config_t;

//=============================================================================
// Effects and Metrics Structures
//=============================================================================

/**
 * @brief Current perception metrics from connected cortexes
 *
 * WHAT: Aggregated perception quality metrics
 * WHY: Used for LR modulation and skip decisions
 * HOW: Updated after each feature extraction
 */
typedef struct {
    /* Visual metrics */
    float visual_confidence;         /**< Visual processing confidence [0-1] */
    float visual_novelty;            /**< Visual novelty score [0-1] */
    bool visual_available;           /**< Visual cortex connected and ready */

    /* Audio metrics */
    float audio_quality;             /**< Audio signal quality [0-1] */
    float speech_salience;           /**< Speech salience [0-1] */
    bool audio_available;            /**< Audio cortex connected and ready */

    /* Computed modulation */
    float lr_factor;                 /**< Computed LR multiplier [lr_min, lr_max] */
    bool skip_sample;                /**< Sample should be skipped (low quality) */

    /* Timestamp */
    uint64_t last_update_ms;         /**< When metrics were last updated */
    bool valid;                      /**< Metrics validity */
} cnn_cortex_perception_metrics_t;

/**
 * @brief Gradient feedback state
 *
 * WHAT: Gradients to be sent to cortex for STDP modulation
 * WHY: Enables top-down learning signal to perception cortexes
 * HOW: Computed during CNN backward pass, applied to cortex
 */
typedef struct {
    float* visual_gradients;         /**< Gradients for visual cortex */
    uint32_t visual_gradient_size;   /**< Visual gradient size */
    float* audio_gradients;          /**< Gradients for audio cortex */
    uint32_t audio_gradient_size;    /**< Audio gradient size */
    float gradient_norm;             /**< Total gradient norm */
    bool pending;                    /**< Gradients pending application */
} cnn_cortex_gradient_state_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Statistics for CNN-cortex bridge
 *
 * WHAT: Tracking of bridge activity and performance
 * WHY: Monitoring and debugging
 * HOW: Accumulated during operation, queryable via API
 */
typedef struct {
    /* Feature extraction counts */
    uint64_t total_feature_extractions;
    uint64_t visual_extractions;
    uint64_t audio_extractions;
    uint64_t multimodal_extractions;

    /* Gradient feedback counts */
    uint64_t total_gradient_feedbacks;
    uint64_t visual_feedbacks;
    uint64_t audio_feedbacks;

    /* Skip and quality metrics */
    uint64_t samples_skipped;
    uint64_t samples_processed;
    float avg_visual_confidence;
    float avg_audio_quality;
    float avg_lr_factor;

    /* Timing */
    float avg_extraction_time_us;
    float avg_feedback_time_us;
    uint64_t total_updates;
    uint64_t last_update_ms;

    /* Connection status */
    bool trainer_connected;
    bool visual_cortex_connected;
    bool audio_cortex_connected;
    bool perception_bridge_connected;
    bool bio_async_connected;
    cnn_cortex_mode_t current_mode;
} cnn_cortex_bridge_stats_t;

//=============================================================================
// Main Bridge Structure (Opaque in implementation)
//=============================================================================

/**
 * @brief CNN-cortex bridge structure
 *
 * WHAT: Bridge connecting CNN trainer to visual/audio cortexes
 * WHY: Enables perception-based feature extraction and gradient feedback
 * HOW: Contains connections, state, and configuration
 */
struct cnn_cortex_bridge {
    bridge_base_t base;                     /**< Base bridge infrastructure */

    /* Connected modules */
    cnn_trainer_t* trainer;                 /**< CNN trainer (system_a alias) */
    visual_cortex_handle_t visual_cortex;   /**< Visual cortex handle */
    audio_cortex_handle_t audio_cortex;     /**< Audio cortex handle */
    perception_training_bridge_t* perception_bridge; /**< Perception-training bridge */

    /* Configuration */
    cnn_cortex_bridge_config_t config;      /**< Configuration */

    /* Cached features */
    nimcp_tensor_t* visual_features;        /**< Cached visual features tensor */
    nimcp_tensor_t* audio_features;         /**< Cached audio features tensor */
    nimcp_tensor_t* combined_features;      /**< Combined multimodal features */

    /* Training state */
    visual_cortex_training_state_t visual_state; /**< Visual cortex training state */
    audio_cortex_training_state_t audio_state;   /**< Audio cortex training state */

    /* Perception metrics */
    cnn_cortex_perception_metrics_t metrics; /**< Current perception metrics */

    /* Gradient feedback state */
    cnn_cortex_gradient_state_t gradient_state; /**< Gradient feedback state */

    /* Statistics */
    cnn_cortex_bridge_stats_t stats;        /**< Accumulated statistics */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY: Ensure all fields have valid initial values
 * HOW: Sets mode to TRAINING, enables perception modulation
 *
 * @param config Configuration to initialize (must not be NULL)
 */
void cnn_cortex_bridge_default_config(cnn_cortex_bridge_config_t* config);

/**
 * @brief Create a new CNN-cortex bridge
 *
 * WHAT: Allocates and initializes bridge structure
 * WHY: Entry point for using the bridge
 * HOW: Allocates state, initializes tensors
 *
 * @param config Configuration (NULL uses defaults)
 * @return Bridge handle or NULL on failure
 */
cnn_cortex_bridge_t* cnn_cortex_bridge_create(
    const cnn_cortex_bridge_config_t* config
);

/**
 * @brief Destroy a CNN-cortex bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY: Proper cleanup
 * HOW: Disconnects modules, frees tensors, destroys mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void cnn_cortex_bridge_destroy(cnn_cortex_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to CNN trainer
 *
 * WHAT: Links bridge to CNN training module
 * WHY: Enables feature injection and gradient retrieval
 * HOW: Stores trainer reference, registers callbacks
 *
 * @param bridge Bridge to connect
 * @param trainer CNN trainer (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_connect_trainer(
    cnn_cortex_bridge_t* bridge,
    cnn_trainer_t* trainer
);

/**
 * @brief Connect to visual cortex
 *
 * WHAT: Links bridge to V1-style visual feature extraction
 * WHY: Enables visual feature extraction for CNN training
 * HOW: Stores cortex reference, queries feature dimensions
 *
 * BIOLOGICAL BASIS: V1 provides orientation-selective feature maps
 * via Gabor-like filters, serving as innate visual feature extractors.
 *
 * @param bridge Bridge to connect
 * @param visual_cortex Visual cortex handle (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_connect_visual_cortex(
    cnn_cortex_bridge_t* bridge,
    visual_cortex_handle_t visual_cortex
);

/**
 * @brief Connect to audio cortex
 *
 * WHAT: Links bridge to A1-style auditory feature extraction
 * WHY: Enables audio feature extraction for CNN training
 * HOW: Stores cortex reference, queries feature dimensions
 *
 * BIOLOGICAL BASIS: A1 provides tonotopic feature maps via
 * cochlear frequency decomposition, serving as innate auditory features.
 *
 * @param bridge Bridge to connect
 * @param audio_cortex Audio cortex handle (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_connect_audio_cortex(
    cnn_cortex_bridge_t* bridge,
    audio_cortex_handle_t audio_cortex
);

/**
 * @brief Connect to perception-training bridge
 *
 * WHAT: Links to perception-training bridge for coordination
 * WHY: Share perception metrics and modulation factors
 * HOW: Stores bridge reference, enables metric sharing
 *
 * @param bridge Bridge to connect
 * @param perception_bridge Perception-training bridge (may be NULL)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_connect_perception_bridge(
    cnn_cortex_bridge_t* bridge,
    perception_training_bridge_t* perception_bridge
);

/**
 * @brief Check if bridge is fully connected
 *
 * WHAT: Verify at least one cortex is connected
 * WHY: Determine if feature extraction is possible
 * HOW: Returns true if visual OR audio cortex connected
 *
 * @param bridge Bridge to check
 * @return true if at least one cortex connected
 */
bool cnn_cortex_bridge_is_connected(const cnn_cortex_bridge_t* bridge);

//=============================================================================
// Feature Extraction API
//=============================================================================

/**
 * @brief Extract visual features as tensor
 *
 * WHAT: Process image through visual cortex and return features as tensor
 * WHY: Provides perception-based features for CNN training
 * HOW: Calls visual_cortex_process(), wraps result in tensor
 *
 * @param bridge Bridge to use
 * @param image Input image data (grayscale uint8)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1=grayscale, 3=RGB)
 * @param features Output tensor (allocated by function, caller must destroy)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_extract_visual_features(
    cnn_cortex_bridge_t* bridge,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    nimcp_tensor_t** features
);

/**
 * @brief Extract audio features as tensor
 *
 * WHAT: Process audio through audio cortex and return features as tensor
 * WHY: Provides perception-based features for CNN training
 * HOW: Calls audio_cortex_process(), wraps result in tensor
 *
 * @param bridge Bridge to use
 * @param audio Input audio samples (float32)
 * @param num_samples Number of audio samples
 * @param num_channels Number of audio channels (1=mono, 2=stereo)
 * @param features Output tensor (allocated by function, caller must destroy)
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_extract_audio_features(
    cnn_cortex_bridge_t* bridge,
    const float* audio,
    uint32_t num_samples,
    uint8_t num_channels,
    nimcp_tensor_t** features
);

/**
 * @brief Extract multimodal features (visual + audio concatenated)
 *
 * WHAT: Extract and concatenate features from both cortexes
 * WHY: Enables multimodal CNN training
 * HOW: Extracts both, concatenates into single tensor
 *
 * @param bridge Bridge to use
 * @param image Visual input (may be NULL to skip)
 * @param image_width Image width
 * @param image_height Image height
 * @param image_channels Image channels
 * @param audio Audio input (may be NULL to skip)
 * @param num_audio_samples Audio sample count
 * @param num_audio_channels Audio channels
 * @param features Output concatenated tensor
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_extract_multimodal_features(
    cnn_cortex_bridge_t* bridge,
    const uint8_t* image,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t image_channels,
    const float* audio,
    uint32_t num_audio_samples,
    uint8_t num_audio_channels,
    nimcp_tensor_t** features
);

//=============================================================================
// Gradient Feedback API
//=============================================================================

/**
 * @brief Set gradients for cortex feedback
 *
 * WHAT: Provide CNN gradients for conversion to STDP signals
 * WHY: Enables top-down learning modulation of perception cortexes
 * HOW: Stores gradients for next feedback application
 *
 * @param bridge Bridge to use
 * @param gradients Gradient tensor from CNN backward pass
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_set_gradients(
    cnn_cortex_bridge_t* bridge,
    const nimcp_tensor_t* gradients
);

/**
 * @brief Propagate gradients to connected cortexes
 *
 * WHAT: Apply gradient feedback to cortex STDP mechanisms
 * WHY: Enables CNN training to modulate cortex plasticity
 * HOW: Converts gradients to STDP signals per configured method
 *
 * BIOLOGICAL BASIS: Models top-down attention modulating sensory
 * cortex plasticity via feedback projections from prefrontal cortex.
 *
 * @param bridge Bridge to use
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_propagate_gradients(cnn_cortex_bridge_t* bridge);

//=============================================================================
// Perception Modulation API
//=============================================================================

/**
 * @brief Get current perception metrics
 *
 * WHAT: Query aggregated perception quality metrics
 * WHY: Determine LR modulation and skip decisions
 * HOW: Copies internal metrics to output
 *
 * @param bridge Bridge to query
 * @param metrics Output metrics structure
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_get_perception_metrics(
    const cnn_cortex_bridge_t* bridge,
    cnn_cortex_perception_metrics_t* metrics
);

/**
 * @brief Get perception-modulated learning rate
 *
 * WHAT: Apply perception quality to base learning rate
 * WHY: High confidence → boost LR, low quality → reduce LR
 * HOW: base_lr × lr_factor (computed from confidence/quality)
 *
 * @param bridge Bridge to query
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 */
float cnn_cortex_bridge_get_modulated_lr(
    const cnn_cortex_bridge_t* bridge,
    float base_lr
);

/**
 * @brief Check if current sample should be skipped
 *
 * WHAT: Determine if perception quality is too low for training
 * WHY: Avoid training on corrupted/ambiguous samples
 * HOW: Returns true if confidence/quality below thresholds
 *
 * @param bridge Bridge to query
 * @return true if sample should be skipped
 */
bool cnn_cortex_bridge_should_skip_sample(const cnn_cortex_bridge_t* bridge);

//=============================================================================
// Training State API
//=============================================================================

/**
 * @brief Get visual cortex training state
 *
 * WHAT: Retrieve cached visual cortex activations
 * WHY: Needed for gradient computation and feedback
 * HOW: Copies internal state to output
 *
 * @param bridge Bridge to query
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_get_visual_state(
    const cnn_cortex_bridge_t* bridge,
    visual_cortex_training_state_t* state
);

/**
 * @brief Get audio cortex training state
 *
 * WHAT: Retrieve cached audio cortex activations
 * WHY: Needed for gradient computation and feedback
 * HOW: Copies internal state to output
 *
 * @param bridge Bridge to query
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_get_audio_state(
    const cnn_cortex_bridge_t* bridge,
    audio_cortex_training_state_t* state
);

//=============================================================================
// Update Cycle API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Main update cycle for bridge
 * WHY: Refresh perception metrics and apply pending gradients
 * HOW: Queries cortex state, computes metrics, applies gradients
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_update(cnn_cortex_bridge_t* bridge);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY: Enable inter-module communication
 * HOW: Uses bridge_base infrastructure
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_connect_bio_async(cnn_cortex_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_disconnect_bio_async(cnn_cortex_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge to query
 * @return true if connected to bio-async router
 */
bool cnn_cortex_bridge_is_bio_async_connected(const cnn_cortex_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY: Monitoring and debugging
 * HOW: Copies internal stats to output
 *
 * @param bridge Bridge to query
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_get_stats(
    const cnn_cortex_bridge_t* bridge,
    cnn_cortex_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY: Start fresh measurement period
 * HOW: Zeros all stat counters
 *
 * @param bridge Bridge to reset
 * @return 0 on success, negative on error
 */
int cnn_cortex_bridge_reset_stats(cnn_cortex_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert mode enum to string
 *
 * @param mode Mode to convert
 * @return String name of mode
 */
const char* cnn_cortex_mode_to_string(cnn_cortex_mode_t mode);

/**
 * @brief Convert priority enum to string
 *
 * @param priority Priority to convert
 * @return String name of priority
 */
const char* cnn_cortex_priority_to_string(cnn_cortex_priority_t priority);

/**
 * @brief Convert gradient method enum to string
 *
 * @param method Method to convert
 * @return String name of method
 */
const char* cnn_cortex_gradient_method_to_string(cnn_cortex_gradient_method_t method);

/**
 * @brief Dump bridge state for debugging
 *
 * @param bridge Bridge to dump
 */
void cnn_cortex_bridge_dump_state(const cnn_cortex_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CNN_CORTEX_BRIDGE_H */
