/**
 * @file nimcp_dragonfly_visual_bridge.h
 * @brief Visual Cortex Bridge for Dragonfly Module
 *
 * WHAT: Connects dragonfly interception system to visual cortex perception
 * WHY:  Visual cortex provides V1-style feature extraction for target detection
 * HOW:  Extracts motion, salience, and position from visual cortex output
 *
 * INTEGRATION PIPELINE:
 * Visual Cortex → Motion Detection → Dragonfly TSDN → Tracking → Interception
 *
 * BIOLOGICAL REFERENCE:
 * - Dragonfly optic lobes receive retinotopic input similar to V1
 * - Target-selective descending neurons (TSDNs) integrate motion and position
 * - Visual attention peaks drive target selection (CSTMD1-like)
 *
 * @author NIMCP Team
 * @date 2024-12-28
 */

#ifndef NIMCP_DRAGONFLY_VISUAL_BRIDGE_H
#define NIMCP_DRAGONFLY_VISUAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

/* Forward declarations */
typedef struct visual_cortex_struct visual_cortex_t;
typedef struct dragonfly_visual_bridge_s dragonfly_visual_bridge_t;

//=============================================================================
// Constants
//=============================================================================

#define VISUAL_BRIDGE_MAX_DETECTIONS 16    /**< Max detections per frame */
#define MAX_MOTION_REGIONS           32    /**< Max motion regions per frame */
#define MAX_ATTENTION_PEAKS          16    /**< Max attention peaks */

//=============================================================================
// Visual Feature Types (for visual cortex integration)
//=============================================================================

/**
 * @brief Motion region detected in visual field
 *
 * Represents a region with coherent motion from visual cortex V5/MT output.
 */
typedef struct {
    float center_x;        /**< Center X in normalized coords [0,1] */
    float center_y;        /**< Center Y in normalized coords [0,1] */
    float size;            /**< Region size (area in pixels) */
    float velocity_x;      /**< Motion velocity X (pixels/frame) */
    float velocity_y;      /**< Motion velocity Y (pixels/frame) */
    float contrast;        /**< Contrast against background [0,1] */
    float salience;        /**< Computed salience [0,1] */
} visual_motion_region_t;

/**
 * @brief Visual features extracted by visual cortex
 *
 * Contains motion regions and other features for dragonfly processing.
 */
typedef struct {
    visual_motion_region_t motion_regions[MAX_MOTION_REGIONS];
    uint32_t num_motion_regions;
    float global_motion_x;    /**< Global optic flow X */
    float global_motion_y;    /**< Global optic flow Y */
    float avg_luminance;      /**< Average luminance */
    uint64_t timestamp_us;    /**< Extraction timestamp */
} visual_features_t;

/**
 * @brief Attention peak location
 */
typedef struct {
    float x;               /**< Peak X in normalized coords [0,1] */
    float y;               /**< Peak Y in normalized coords [0,1] */
    float salience;        /**< Salience at peak [0,1] */
    float sigma;           /**< Attention spread (Gaussian sigma) */
} attention_peak_t;

/**
 * @brief Attention map from visual cortex
 *
 * Top-down and bottom-up attention combined.
 */
typedef struct attention_map_struct {
    attention_peak_t peaks[MAX_ATTENTION_PEAKS];
    uint32_t num_peaks;
    float* saliency_map;      /**< Optional full saliency map (width*height) */
    uint32_t map_width;       /**< Saliency map width */
    uint32_t map_height;      /**< Saliency map height */
    float peak_threshold;     /**< Threshold for peak detection */
} attention_map_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Motion blob detected in visual field
 */
typedef struct {
    float center_x;           /**< Center X in image (pixels) */
    float center_y;           /**< Center Y in image (pixels) */
    float size_pixels;        /**< Blob size in pixels */
    float velocity_x;         /**< Motion velocity X (pixels/frame) */
    float velocity_y;         /**< Motion velocity Y (pixels/frame) */
    float contrast;           /**< Contrast against background [0,1] */
    float salience;           /**< Attention salience [0,1] */
    uint32_t track_id;        /**< Assigned tracking ID */
} motion_blob_t;

/**
 * @brief Visual processing result
 */
typedef struct {
    motion_blob_t blobs[VISUAL_BRIDGE_MAX_DETECTIONS];
    uint32_t num_blobs;
    float attention_peak_x;   /**< X of attention peak */
    float attention_peak_y;   /**< Y of attention peak */
    float peak_salience;      /**< Salience at peak */
    uint64_t timestamp_us;    /**< Processing timestamp */
    uint32_t frame_number;    /**< Sequential frame counter */
} visual_motion_result_t;

/**
 * @brief Camera/visual space calibration
 */
typedef struct {
    float focal_length;       /**< Camera focal length (pixels) */
    float principal_x;        /**< Principal point X */
    float principal_y;        /**< Principal point Y */
    uint32_t image_width;     /**< Image width (pixels) */
    uint32_t image_height;    /**< Image height (pixels) */
    float baseline_distance;  /**< Baseline for stereo (0 for mono) */
} visual_calibration_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Motion detection */
    float min_motion_speed;       /**< Min pixels/frame to detect motion */
    float min_blob_size;          /**< Min blob size in pixels */
    float max_blob_size;          /**< Max blob size in pixels */
    float contrast_threshold;     /**< Min contrast to detect */

    /* Depth estimation */
    bool estimate_depth;          /**< Enable monocular depth from size */
    float assumed_target_size_m;  /**< Assumed real target size (meters) */

    /* Attention */
    bool use_attention_filter;    /**< Weight by attention map */
    float attention_threshold;    /**< Min salience to process */

    /* Calibration */
    visual_calibration_t calibration;

    /* Update rate */
    float frame_dt_s;             /**< Time between frames */
} visual_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t frames_processed;
    uint64_t blobs_detected;
    uint64_t detections_sent;
    float avg_process_time_us;
    float avg_blobs_per_frame;
    uint32_t current_tracked;
} visual_bridge_stats_t;

//=============================================================================
// Visual Cortex Integration Functions
//=============================================================================

/**
 * @brief Extract visual features from image
 *
 * Wrapper to extract motion regions and features from visual cortex.
 * If visual cortex is NULL, returns empty features.
 *
 * @param cortex Visual cortex handle (can be NULL)
 * @param image Raw image data
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1=grayscale, 3=RGB)
 * @param features Output features structure
 * @return 0 on success, -1 on error
 */
int visual_cortex_extract_features(
    visual_cortex_t* cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    visual_features_t* features
);

/**
 * @brief Get attention map from visual cortex
 *
 * @param cortex Visual cortex handle
 * @return Attention map or NULL if unavailable
 */
const attention_map_t* visual_cortex_get_attention_map(visual_cortex_t* cortex);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
visual_bridge_config_t visual_bridge_default_config(void);

/**
 * @brief Validate configuration
 */
bool visual_bridge_validate_config(const visual_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create visual bridge
 *
 * @param dragonfly Dragonfly system to feed detections to
 * @param visual_cortex Visual cortex for feature extraction (can be NULL for test)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
dragonfly_visual_bridge_t* dragonfly_visual_bridge_create(
    dragonfly_system_t* dragonfly,
    visual_cortex_t* visual_cortex,
    const visual_bridge_config_t* config
);

/**
 * @brief Destroy bridge
 */
void dragonfly_visual_bridge_destroy(dragonfly_visual_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int dragonfly_visual_bridge_reset(dragonfly_visual_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process raw image frame
 *
 * WHAT: Extract motion and feed to dragonfly system
 * WHY:  Main entry point for visual input
 * HOW:  Uses visual cortex for features, then motion detection
 *
 * @param bridge Visual bridge
 * @param image Raw image data (grayscale or RGB depending on cortex config)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1 or 3)
 * @return 0 on success, -1 on error
 */
int dragonfly_visual_bridge_process_frame(
    dragonfly_visual_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels
);

/**
 * @brief Process visual cortex output directly
 *
 * @param bridge Visual bridge
 * @param features Visual cortex feature vector
 * @param feature_dim Feature dimension
 * @param attention Attention map (can be NULL)
 * @return 0 on success, -1 on error
 */
int dragonfly_visual_bridge_process_features(
    dragonfly_visual_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    const attention_map_t* attention
);

/**
 * @brief Inject synthetic motion blob (for testing)
 *
 * @param bridge Visual bridge
 * @param blob Motion blob to inject
 * @return 0 on success, -1 on error
 */
int dragonfly_visual_bridge_inject_blob(
    dragonfly_visual_bridge_t* bridge,
    const motion_blob_t* blob
);

/**
 * @brief Get latest motion detection result
 */
int dragonfly_visual_bridge_get_result(
    const dragonfly_visual_bridge_t* bridge,
    visual_motion_result_t* result
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert pixel coordinates to 3D position
 *
 * @param bridge Visual bridge (for calibration)
 * @param pixel_x Pixel X coordinate
 * @param pixel_y Pixel Y coordinate
 * @param depth_m Estimated or measured depth (meters)
 * @param position Output: 3D position (x, y, z)
 * @return 0 on success, -1 on error
 */
int dragonfly_visual_bridge_pixel_to_3d(
    const dragonfly_visual_bridge_t* bridge,
    float pixel_x,
    float pixel_y,
    float depth_m,
    float position[3]
);

/**
 * @brief Estimate depth from blob size
 *
 * @param bridge Visual bridge
 * @param blob_size_pixels Blob size in pixels
 * @return Estimated depth in meters, or -1.0 on error
 */
float dragonfly_visual_bridge_estimate_depth(
    const dragonfly_visual_bridge_t* bridge,
    float blob_size_pixels
);

/**
 * @brief Convert pixel velocity to angular rate
 *
 * @param bridge Visual bridge
 * @param vel_pixels_per_frame Velocity in pixels per frame
 * @return Angular rate in radians per second
 */
float dragonfly_visual_bridge_pixel_velocity_to_angular(
    const dragonfly_visual_bridge_t* bridge,
    float vel_pixels_per_frame
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 */
int dragonfly_visual_bridge_get_stats(
    const dragonfly_visual_bridge_t* bridge,
    visual_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int dragonfly_visual_bridge_reset_stats(dragonfly_visual_bridge_t* bridge);

//=============================================================================
// Configuration Update
//=============================================================================

/**
 * @brief Update configuration
 */
int dragonfly_visual_bridge_set_config(
    dragonfly_visual_bridge_t* bridge,
    const visual_bridge_config_t* config
);

/**
 * @brief Get current configuration
 */
int dragonfly_visual_bridge_get_config(
    const dragonfly_visual_bridge_t* bridge,
    visual_bridge_config_t* config
);

/**
 * @brief Update calibration
 */
int dragonfly_visual_bridge_set_calibration(
    dragonfly_visual_bridge_t* bridge,
    const visual_calibration_t* calibration
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_VISUAL_BRIDGE_H */
