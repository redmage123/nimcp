/**
 * @file nimcp_occipital_adapter.h
 * @brief Brain adapter for Occipital Cortex (Visual Cortex) integration
 *
 * WHAT: Unified adapter connecting occipital cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates V1-V5 visual hierarchy as a cohesive visual processing unit
 *
 * ARCHITECTURE:
 * - Wraps all visual cortex sub-modules (V1, V2, V3, V4, V5/MT)
 * - Provides high-level API for visual processing pipeline
 * - Integrates with LGN (thalamus) for input gating
 * - Connects to event bus for inter-module communication
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Models Brodmann areas 17 (V1), 18 (V2), 19 (V3, V4, V5)
 * - V1: Edge detection, orientation selectivity, ocular dominance
 * - V2: Contour integration, texture, figure-ground segregation
 * - V3: Dynamic form processing
 * - V4: Color constancy, complex form processing
 * - V5/MT: Motion detection, optic flow
 *
 * VISUAL PROCESSING HIERARCHY:
 * - Retina -> LGN -> V1 -> V2 -> V3 -> (V4 | V5/MT)
 * - Dorsal "where" stream: V1 -> V2 -> V3 -> V5/MT -> Parietal
 * - Ventral "what" stream: V1 -> V2 -> V4 -> Temporal
 *
 * @version Phase O1: Occipital Cortex Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_OCCIPITAL_ADAPTER_H
#define NIMCP_OCCIPITAL_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declarations for sub-modules */
typedef struct v1_processor v1_processor_t;
typedef struct v2_processor v2_processor_t;
typedef struct v3_processor v3_processor_t;
typedef struct v4_processor v4_processor_t;
typedef struct v5_mt_processor v5_mt_processor_t;

/* Forward declaration for opaque adapter type */
typedef struct occipital_adapter occipital_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define OCCIPITAL_DEFAULT_IMAGE_WIDTH       224
#define OCCIPITAL_DEFAULT_IMAGE_HEIGHT      224
#define OCCIPITAL_DEFAULT_MAX_FEATURES      512
#define OCCIPITAL_DEFAULT_NUM_ORIENTATIONS  8
#define OCCIPITAL_DEFAULT_NUM_SCALES        4
#define OCCIPITAL_DEFAULT_COLOR_CHANNELS    3
#define OCCIPITAL_DEFAULT_MOTION_FRAMES     5

/**
 * @brief Visual processing areas (Brodmann areas)
 */
typedef enum {
    VISUAL_AREA_V1 = 0,   /**< Primary visual cortex (BA17) */
    VISUAL_AREA_V2,       /**< Secondary visual cortex (BA18) */
    VISUAL_AREA_V3,       /**< Tertiary visual cortex (BA19) */
    VISUAL_AREA_V4,       /**< Color and form area (BA19) */
    VISUAL_AREA_V5_MT,    /**< Motion area (BA19/37) */
    VISUAL_AREA_COUNT
} visual_area_t;

/**
 * @brief Visual processing stream
 */
typedef enum {
    VISUAL_STREAM_DORSAL = 0,  /**< "Where" pathway: V1 -> V2 -> V3 -> V5/MT -> Parietal */
    VISUAL_STREAM_VENTRAL,     /**< "What" pathway: V1 -> V2 -> V4 -> Temporal */
    VISUAL_STREAM_BOTH         /**< Process both streams */
} visual_stream_t;

/**
 * @brief Feature types detected by visual cortex
 * NOTE: Using VISUAL_FEATURE_ prefix to avoid conflict with nimcp_feature_hypercolumns.h
 */
typedef enum {
    VISUAL_FEATURE_EDGE = 0,          /**< Edge/contour */
    VISUAL_FEATURE_ORIENTATION,       /**< Orientation (0-180 degrees) */
    VISUAL_FEATURE_COLOR,             /**< Color (hue, saturation) */
    VISUAL_FEATURE_TEXTURE,           /**< Texture pattern */
    VISUAL_FEATURE_MOTION,            /**< Motion direction and speed */
    VISUAL_FEATURE_DEPTH,             /**< Depth/disparity */
    VISUAL_FEATURE_FORM,              /**< Shape/form */
    VISUAL_FEATURE_FACE,              /**< Face-specific features */
    VISUAL_FEATURE_OBJECT             /**< Object identity */
} visual_feature_type_t;

/**
 * @brief Occipital cortex adapter configuration
 */
typedef struct {
    /* Image dimensions */
    uint32_t image_width;            /**< Input image width */
    uint32_t image_height;           /**< Input image height */
    uint32_t color_channels;         /**< Number of color channels (1=gray, 3=RGB) */

    /* V1 configuration */
    uint32_t v1_num_orientations;    /**< Number of orientation channels (default: 8) */
    uint32_t v1_num_scales;          /**< Number of spatial scales (default: 4) */
    bool v1_enable_gabor;            /**< Enable Gabor filter bank */
    bool v1_enable_contrast_norm;    /**< Enable divisive normalization */

    /* V2 configuration */
    bool v2_enable_contour;          /**< Enable contour integration */
    bool v2_enable_texture;          /**< Enable texture processing */
    bool v2_enable_figure_ground;    /**< Enable figure-ground segregation */

    /* V4 configuration */
    bool v4_enable_color;            /**< Enable color constancy */
    bool v4_enable_complex_form;     /**< Enable complex form processing */
    uint32_t v4_color_space;         /**< Color space (0=RGB, 1=LAB, 2=HSV) */

    /* V5/MT configuration */
    bool v5_enable_motion;           /**< Enable motion processing */
    uint32_t v5_motion_frames;       /**< Number of frames for motion estimation */
    bool v5_enable_optic_flow;       /**< Enable optic flow computation */

    /* Processing options */
    visual_stream_t active_stream;   /**< Which stream(s) to process */
    uint32_t max_features;           /**< Maximum features per area */
    bool enable_attention;           /**< Enable attentional modulation */
    bool enable_feedback;            /**< Enable top-down feedback connections */

    /* Event system */
    bool enable_events;              /**< Enable event bus integration */

    /* Training */
    bool enable_training;            /**< Enable learning capabilities */
    float learning_rate;             /**< Base learning rate */

    /* Bio-async communication */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} occipital_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    OCCIPITAL_STATUS_IDLE = 0,       /**< Ready for input */
    OCCIPITAL_STATUS_V1_PROCESSING,  /**< Primary visual processing */
    OCCIPITAL_STATUS_V2_PROCESSING,  /**< Secondary visual processing */
    OCCIPITAL_STATUS_V4_PROCESSING,  /**< Color/form processing */
    OCCIPITAL_STATUS_V5_PROCESSING,  /**< Motion processing */
    OCCIPITAL_STATUS_INTEGRATION,    /**< Integrating features */
    OCCIPITAL_STATUS_READY,          /**< Output ready for retrieval */
    OCCIPITAL_STATUS_ERROR           /**< Error state */
} occipital_status_t;

/**
 * @brief Error codes for occipital cortex operations
 */
typedef enum {
    OCCIPITAL_ERROR_NONE = 0,
    OCCIPITAL_ERROR_INVALID_INPUT,
    OCCIPITAL_ERROR_V1_FAILURE,
    OCCIPITAL_ERROR_V2_FAILURE,
    OCCIPITAL_ERROR_V4_FAILURE,
    OCCIPITAL_ERROR_V5_FAILURE,
    OCCIPITAL_ERROR_INTEGRATION_FAILURE,
    OCCIPITAL_ERROR_BUFFER_OVERFLOW,
    OCCIPITAL_ERROR_NO_INPUT,
    OCCIPITAL_ERROR_INTERNAL
} occipital_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Visual input image
 */
typedef struct {
    float* data;                     /**< Pixel data (CHW format) */
    uint32_t width;                  /**< Image width */
    uint32_t height;                 /**< Image height */
    uint32_t channels;               /**< Number of channels */
    uint64_t timestamp_us;           /**< Capture timestamp (microseconds) */
    uint32_t frame_id;               /**< Frame sequence number */
} visual_input_t;

/**
 * @brief Detected visual feature
 */
typedef struct {
    visual_feature_type_t type;      /**< Feature type */
    visual_area_t source_area;       /**< Which area detected this feature */
    float x;                         /**< X position (normalized 0-1) */
    float y;                         /**< Y position (normalized 0-1) */
    float scale;                     /**< Spatial scale */
    float orientation;               /**< Orientation (radians, for edges) */
    float strength;                  /**< Feature strength/confidence [0-1] */
    float* descriptor;               /**< Feature descriptor vector */
    uint32_t descriptor_size;        /**< Descriptor dimension */
} visual_feature_t;

/**
 * @brief Motion vector from V5/MT
 */
typedef struct {
    float x;                         /**< X position (normalized 0-1) */
    float y;                         /**< Y position (normalized 0-1) */
    float dx;                        /**< Horizontal velocity (pixels/frame) */
    float dy;                        /**< Vertical velocity (pixels/frame) */
    float confidence;                /**< Motion confidence [0-1] */
} motion_vector_t;

/**
 * @brief Color perception result from V4
 */
typedef struct {
    float hue;                       /**< Perceived hue (0-360 degrees) */
    float saturation;                /**< Perceived saturation [0-1] */
    float brightness;                /**< Perceived brightness [0-1] */
    float x;                         /**< X position (normalized 0-1) */
    float y;                         /**< Y position (normalized 0-1) */
    float size;                      /**< Region size */
} color_percept_t;

/**
 * @brief Complete visual processing result
 */
typedef struct {
    /* V1 results */
    bool v1_processed;               /**< V1 processing completed */
    uint32_t edge_count;             /**< Number of edges detected */
    uint32_t orientation_histogram[8]; /**< Orientation distribution */

    /* V2 results */
    bool v2_processed;               /**< V2 processing completed */
    uint32_t contour_count;          /**< Number of contours */
    bool figure_ground_segmented;    /**< Figure-ground done */

    /* V4 results */
    bool v4_processed;               /**< V4 processing completed */
    uint32_t color_region_count;     /**< Number of color regions */
    uint32_t complex_form_count;     /**< Number of complex forms */

    /* V5/MT results */
    bool v5_processed;               /**< V5 processing completed */
    uint32_t motion_vector_count;    /**< Number of motion vectors */
    float global_motion_dx;          /**< Global horizontal motion */
    float global_motion_dy;          /**< Global vertical motion */

    /* Integrated results */
    uint32_t total_features;         /**< Total features extracted */
    float processing_time_ms;        /**< Total processing time */
    bool ready_for_downstream;       /**< Results ready for higher areas */
} visual_processing_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t frames_processed;       /**< Total frames */
    uint64_t features_extracted;     /**< Total features */
    uint64_t edges_detected;         /**< Total edges */
    uint64_t motions_detected;       /**< Total motion vectors */

    /* Per-area timing */
    float avg_v1_time_ms;            /**< Average V1 processing time */
    float avg_v2_time_ms;            /**< Average V2 processing time */
    float avg_v4_time_ms;            /**< Average V4 processing time */
    float avg_v5_time_ms;            /**< Average V5 processing time */
    float avg_total_time_ms;         /**< Average total processing time */
    float max_total_time_ms;         /**< Maximum processing time */

    /* Success/failure */
    uint64_t successful_frames;      /**< Successfully processed frames */
    uint64_t failed_frames;          /**< Failed frames */

    /* Training */
    uint64_t training_iterations;    /**< Training updates */
    float training_loss;             /**< Current training loss */
} occipital_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for feature detection (integration with higher areas)
 */
typedef void (*occipital_feature_callback_t)(
    const visual_feature_t* feature,
    void* user_data
);

/**
 * @brief Callback for motion detection (integration with parietal cortex)
 */
typedef void (*occipital_motion_callback_t)(
    const motion_vector_t* motion,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*occipital_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for occipital cortex adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
occipital_config_t occipital_default_config(void);

/**
 * @brief Create occipital cortex adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for visual processing initialization
 * HOW:  Create V1-V5 processors; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
occipital_adapter_t* occipital_create(const occipital_config_t* config);

/**
 * @brief Destroy occipital cortex adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers
 *
 * @param adapter Adapter to destroy
 */
void occipital_destroy(occipital_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new frame without full reinitialization
 * HOW:  Reset all sub-modules, clear feature buffers
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool occipital_reset(occipital_adapter_t* adapter);

/*=============================================================================
 * VISUAL PROCESSING PIPELINE
 *===========================================================================*/

/**
 * @brief Set input image for processing
 *
 * WHAT: Provide image data for visual processing
 * WHY:  Set up input before processing pipeline
 * HOW:  Copy or reference image data into input buffer
 *
 * @param adapter Adapter instance
 * @param input Visual input image
 * @return true on success, false on failure
 */
bool occipital_set_input(occipital_adapter_t* adapter, const visual_input_t* input);

/**
 * @brief Process current frame through visual hierarchy
 *
 * WHAT: Run full visual processing pipeline (V1 -> V2 -> V4/V5)
 * WHY:  Extract features from input image
 * HOW:  Sequential processing through visual areas
 *
 * @param adapter Adapter instance
 * @param result Output result structure (optional, can be NULL)
 * @return true on success, false on any pipeline failure
 */
bool occipital_process(occipital_adapter_t* adapter,
                       visual_processing_result_t* result);

/**
 * @brief Process V1 (primary visual cortex) only
 *
 * WHAT: Run edge detection and orientation analysis
 * WHY:  First stage of visual processing
 * HOW:  Gabor filter bank, contrast normalization
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool occipital_process_v1(occipital_adapter_t* adapter);

/**
 * @brief Process V2 (contour integration)
 *
 * WHAT: Run contour and texture processing
 * WHY:  Second stage combining V1 outputs
 * HOW:  Association field, texture synthesis
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool occipital_process_v2(occipital_adapter_t* adapter);

/**
 * @brief Process V4 (color and form)
 *
 * WHAT: Run color constancy and complex form processing
 * WHY:  Ventral stream for "what" pathway
 * HOW:  Color normalization, shape primitives
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool occipital_process_v4(occipital_adapter_t* adapter);

/**
 * @brief Process V5/MT (motion)
 *
 * WHAT: Run motion detection and optic flow
 * WHY:  Dorsal stream for "where" pathway
 * HOW:  Motion energy, flow computation
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool occipital_process_v5(occipital_adapter_t* adapter);

/*=============================================================================
 * FEATURE ACCESS
 *===========================================================================*/

/**
 * @brief Get number of detected features
 *
 * @param adapter Adapter instance
 * @param area Visual area (or VISUAL_AREA_COUNT for all)
 * @return Number of features
 */
uint32_t occipital_get_feature_count(const occipital_adapter_t* adapter,
                                      visual_area_t area);

/**
 * @brief Get feature by index
 *
 * @param adapter Adapter instance
 * @param area Visual area
 * @param index Feature index
 * @param feature Output feature (filled on success)
 * @return true if found, false otherwise
 */
bool occipital_get_feature(const occipital_adapter_t* adapter,
                           visual_area_t area,
                           uint32_t index,
                           visual_feature_t* feature);

/**
 * @brief Get all features from an area
 *
 * @param adapter Adapter instance
 * @param area Visual area
 * @param features Output buffer (must be pre-allocated)
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool occipital_get_features(const occipital_adapter_t* adapter,
                            visual_area_t area,
                            visual_feature_t* features,
                            uint32_t* count);

/**
 * @brief Get motion vectors from V5/MT
 *
 * @param adapter Adapter instance
 * @param vectors Output buffer
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool occipital_get_motion_vectors(const occipital_adapter_t* adapter,
                                  motion_vector_t* vectors,
                                  uint32_t* count);

/**
 * @brief Get color percepts from V4
 *
 * @param adapter Adapter instance
 * @param percepts Output buffer
 * @param count Input: buffer capacity; Output: actual count
 * @return true on success
 */
bool occipital_get_color_percepts(const occipital_adapter_t* adapter,
                                  color_percept_t* percepts,
                                  uint32_t* count);

/*=============================================================================
 * ATTENTION MODULATION
 *===========================================================================*/

/**
 * @brief Apply spatial attention
 *
 * WHAT: Modulate processing based on attended location
 * WHY:  Enable top-down attentional control
 * HOW:  Multiplicative gain at attended location
 *
 * @param adapter Adapter instance
 * @param x Attention center X (normalized 0-1)
 * @param y Attention center Y (normalized 0-1)
 * @param radius Attention radius (normalized 0-1)
 * @param gain Attentional gain (>1 enhances, <1 suppresses)
 * @return true on success
 */
bool occipital_apply_spatial_attention(occipital_adapter_t* adapter,
                                       float x, float y,
                                       float radius, float gain);

/**
 * @brief Apply feature-based attention
 *
 * WHAT: Modulate processing based on attended feature
 * WHY:  Enable feature-selective attention (e.g., "attend to red")
 * HOW:  Boost processing of matching features
 *
 * @param adapter Adapter instance
 * @param feature_type Type of feature to attend
 * @param gain Attentional gain
 * @return true on success
 */
bool occipital_apply_feature_attention(occipital_adapter_t* adapter,
                                       visual_feature_type_t feature_type,
                                       float gain);

/*=============================================================================
 * CALLBACKS AND EVENTS
 *===========================================================================*/

/**
 * @brief Set feature detection callback
 *
 * @param adapter Adapter instance
 * @param callback Feature handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool occipital_set_feature_callback(occipital_adapter_t* adapter,
                                    occipital_feature_callback_t callback,
                                    void* user_data);

/**
 * @brief Set motion detection callback
 *
 * @param adapter Adapter instance
 * @param callback Motion handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool occipital_set_motion_callback(occipital_adapter_t* adapter,
                                   occipital_motion_callback_t callback,
                                   void* user_data);

/**
 * @brief Set event callback
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool occipital_set_event_callback(occipital_adapter_t* adapter,
                                  occipital_event_callback_t callback,
                                  void* user_data);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Train on labeled features
 *
 * WHAT: Provide supervised signal for feature learning
 * WHY:  Enable feature detector improvement
 * HOW:  Backpropagate error to filter weights
 *
 * @param adapter Adapter instance
 * @param target_features Expected features
 * @param num_features Number of target features
 * @param learning_rate Learning rate (0 = use config default)
 * @return true on success
 */
bool occipital_train(occipital_adapter_t* adapter,
                     const visual_feature_t* target_features,
                     uint32_t num_features,
                     float learning_rate);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 */
occipital_status_t occipital_get_status(const occipital_adapter_t* adapter);

/**
 * @brief Get last error code
 */
occipital_error_t occipital_get_last_error(const occipital_adapter_t* adapter);

/**
 * @brief Get error description string
 */
const char* occipital_error_string(occipital_error_t error);

/**
 * @brief Get status description string
 */
const char* occipital_status_string(occipital_status_t status);

/**
 * @brief Get adapter statistics
 */
bool occipital_get_stats(const occipital_adapter_t* adapter, occipital_stats_t* stats);

/**
 * @brief Get adapter configuration
 */
bool occipital_get_config(const occipital_adapter_t* adapter, occipital_config_t* config);

/*=============================================================================
 * SUB-MODULE ACCESS (Advanced)
 *===========================================================================*/

/**
 * @brief Get V1 processor handle
 */
v1_processor_t* occipital_get_v1_processor(occipital_adapter_t* adapter);

/**
 * @brief Get V2 processor handle
 */
v2_processor_t* occipital_get_v2_processor(occipital_adapter_t* adapter);

/**
 * @brief Get V4 processor handle
 */
v4_processor_t* occipital_get_v4_processor(occipital_adapter_t* adapter);

/**
 * @brief Get V5/MT processor handle
 */
v5_mt_processor_t* occipital_get_v5_processor(occipital_adapter_t* adapter);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t occipital_get_bio_context(occipital_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t occipital_process_bio_messages(occipital_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request LGN input asynchronously
 *
 * WHAT: Request visual input from LGN (thalamus)
 * WHY:  Async communication with thalamic relay
 * HOW:  Sends visual input request, returns future
 *
 * @param adapter Adapter instance
 * @return Future for visual input, or NULL on failure
 */
nimcp_bio_future_t occipital_request_lgn_input_async(occipital_adapter_t* adapter);

/**
 * @brief Broadcast visual features to downstream areas
 *
 * WHAT: Notify parietal and temporal cortex of detected features
 * WHY:  Allow higher areas to use visual features
 * HOW:  Broadcasts visual feature message
 *
 * @param adapter Adapter instance
 * @param result Visual processing result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t occipital_broadcast_features(
    occipital_adapter_t* adapter,
    const visual_processing_result_t* result
);

/**
 * @brief Handle attention modulation from higher areas
 *
 * WHAT: Process top-down attention signals
 * WHY:  Enable attentional modulation from prefrontal/parietal
 * HOW:  Apply gain modulation based on attention message
 *
 * @param adapter Adapter instance
 * @param x Attended X position
 * @param y Attended Y position
 * @param feature_type Attended feature type
 * @param gain Attention gain
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t occipital_handle_attention(
    occipital_adapter_t* adapter,
    float x, float y,
    visual_feature_type_t feature_type,
    float gain
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_ADAPTER_H */
