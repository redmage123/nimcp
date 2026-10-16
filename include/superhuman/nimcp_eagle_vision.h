/**
 * @file nimcp_eagle_vision.h
 * @brief Enhanced Long-Range Pattern Detection - Superhuman Vision Module
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Superhuman visual processing with eagle-like acuity and spectrum sensitivity
 * WHY:  Enable enhanced pattern detection capabilities beyond human visual limits
 * HOW:  High-resolution foveal simulation, motion detection, UV spectrum processing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EAGLE VISUAL SYSTEM:
 * -------------------
 * 1. Foveal Density:
 *    - Eagles: ~1M cones/mm^2 (vs ~200K in humans)
 *    - Two foveae: deep central for sharp focus, shallow temporal for peripheral
 *    - Provides 4-8x higher visual acuity than humans
 *    - Reference: Tucker (2000) "The deep fovea, sideways vision and spiral flight"
 *
 * 2. Motion Detection:
 *    - Superior temporal frequency resolution
 *    - Enhanced motion parallax processing
 *    - Specialized for detecting small movements at distance
 *    - Reference: Potier et al. (2018) "Visual abilities in raptors"
 *
 * 3. UV Sensitivity:
 *    - Birds possess UV-sensitive cone (fourth cone type)
 *    - UV markings on prey (urine trails) visible
 *    - Enhanced contrast discrimination
 *    - Reference: Hart (2001) "The visual ecology of avian photoreceptors"
 *
 * IMPLEMENTATION:
 * ---------------
 * - High-acuity foveal simulation with 4x resolution enhancement
 * - Motion detection through temporal derivative analysis
 * - UV spectrum simulation through wavelength extension
 * - Pattern detection optimized for long-range targets
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EAGLE_VISION_H
#define NIMCP_EAGLE_VISION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EAGLE_VISION_MAX_ACUITY          8.0f   /**< Maximum acuity multiplier (8x human) */
#define EAGLE_VISION_MIN_ACUITY          1.0f   /**< Minimum acuity (human baseline) */
#define EAGLE_VISION_DEFAULT_ACUITY      4.0f   /**< Default enhanced acuity */
#define EAGLE_VISION_FOVEA_COUNT         2      /**< Deep central + shallow temporal */
#define EAGLE_VISION_UV_WAVELENGTH_MIN   300    /**< UV-A lower bound (nm) */
#define EAGLE_VISION_UV_WAVELENGTH_MAX   400    /**< UV-A upper bound (nm) */
#define EAGLE_VISION_MOTION_THRESHOLD    0.01f  /**< Minimum detectable motion */
#define EAGLE_VISION_MAX_TARGETS         256    /**< Maximum tracked targets */
#define EAGLE_VISION_HISTORY_SIZE        16     /**< Frames for motion analysis */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    EAGLE_VISION_SUCCESS                 = 0,
    EAGLE_VISION_ERROR_NULL_POINTER      = -1,
    EAGLE_VISION_ERROR_INVALID_PARAM     = -2,
    EAGLE_VISION_ERROR_NO_MEMORY         = -3,
    EAGLE_VISION_ERROR_NOT_INITIALIZED   = -4,
    EAGLE_VISION_ERROR_INVALID_STATE     = -5,
    EAGLE_VISION_ERROR_BUFFER_TOO_SMALL  = -6,
    EAGLE_VISION_ERROR_NO_INPUT          = -7,
    EAGLE_VISION_ERROR_PROCESSING_FAILED = -8
} eagle_vision_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Fovea type for dual-fovea simulation
 */
typedef enum {
    EAGLE_FOVEA_CENTRAL,    /**< Deep fovea - high acuity, narrow field */
    EAGLE_FOVEA_TEMPORAL,   /**< Shallow fovea - motion detection, wider field */
    EAGLE_FOVEA_BOTH        /**< Combined processing from both foveae */
} eagle_fovea_type_t;

/**
 * @brief Spectrum processing mode
 */
typedef enum {
    EAGLE_SPECTRUM_VISIBLE,     /**< Standard visible spectrum (400-700nm) */
    EAGLE_SPECTRUM_UV_ENHANCED, /**< Extended UV sensitivity (300-700nm) */
    EAGLE_SPECTRUM_FULL         /**< Full UV + visible processing */
} eagle_spectrum_mode_t;

/**
 * @brief Motion detection sensitivity level
 */
typedef enum {
    EAGLE_MOTION_LOW,       /**< Low sensitivity - large movements only */
    EAGLE_MOTION_MEDIUM,    /**< Medium sensitivity - normal detection */
    EAGLE_MOTION_HIGH,      /**< High sensitivity - subtle movements */
    EAGLE_MOTION_ULTRA      /**< Ultra sensitivity - micro-movements */
} eagle_motion_sensitivity_t;

/**
 * @brief Target priority classification
 */
typedef enum {
    EAGLE_TARGET_PRIORITY_NONE,     /**< No priority assigned */
    EAGLE_TARGET_PRIORITY_LOW,      /**< Background interest */
    EAGLE_TARGET_PRIORITY_MEDIUM,   /**< Moderate attention */
    EAGLE_TARGET_PRIORITY_HIGH,     /**< Primary focus */
    EAGLE_TARGET_PRIORITY_CRITICAL  /**< Immediate attention required */
} eagle_target_priority_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief 2D point for target localization
 */
typedef struct {
    float x;                /**< X coordinate (normalized 0-1) */
    float y;                /**< Y coordinate (normalized 0-1) */
} eagle_point_t;

/**
 * @brief Detected motion vector
 */
typedef struct {
    eagle_point_t position;     /**< Current position */
    float velocity_x;           /**< X velocity (units/frame) */
    float velocity_y;           /**< Y velocity (units/frame) */
    float magnitude;            /**< Motion magnitude */
    float direction;            /**< Motion direction (radians) */
    float confidence;           /**< Detection confidence [0-1] */
} eagle_motion_vector_t;

/**
 * @brief UV spectrum detection result
 */
typedef struct {
    float uv_intensity;         /**< Overall UV intensity [0-1] */
    float wavelength_peak;      /**< Peak wavelength detected (nm) */
    float contrast_boost;       /**< Contrast enhancement factor */
    bool trail_detected;        /**< UV trail pattern detected */
    eagle_point_t trail_start;  /**< Trail start point if detected */
    eagle_point_t trail_end;    /**< Trail end point if detected */
} eagle_uv_result_t;

/**
 * @brief Detected target information
 */
typedef struct {
    uint32_t target_id;             /**< Unique target identifier */
    eagle_point_t position;         /**< Target centroid position */
    float distance_estimate;        /**< Estimated distance (relative) */
    float size_estimate;            /**< Estimated size (relative) */
    float confidence;               /**< Detection confidence [0-1] */
    eagle_target_priority_t priority; /**< Assigned priority */
    eagle_motion_vector_t motion;   /**< Associated motion if tracking */
    bool is_moving;                 /**< Target in motion */
    uint32_t frames_tracked;        /**< Tracking duration */
} eagle_target_t;

/**
 * @brief Pattern recognition result
 */
typedef struct {
    uint32_t pattern_id;        /**< Pattern type identifier */
    float match_confidence;     /**< Pattern match confidence [0-1] */
    eagle_point_t location;     /**< Pattern location */
    float scale;                /**< Pattern scale */
    float rotation;             /**< Pattern rotation (radians) */
    const char* pattern_name;   /**< Human-readable pattern name */
} eagle_pattern_result_t;

/**
 * @brief Fovea state for dual-fovea processing
 */
typedef struct {
    eagle_fovea_type_t type;        /**< Fovea type */
    eagle_point_t gaze_point;       /**< Current gaze center */
    float field_of_view;            /**< FOV in degrees */
    float acuity_current;           /**< Current acuity level */
    float depth_factor;             /**< Fovea depth (central deeper) */
    uint32_t active_neurons;        /**< Simulated active neurons */
} eagle_fovea_state_t;

/**
 * @brief Eagle vision configuration
 */
typedef struct {
    /* Acuity settings */
    float acuity_multiplier;            /**< Visual acuity enhancement [1-8] */
    bool enable_dual_fovea;             /**< Enable dual-fovea simulation */
    float central_fovea_weight;         /**< Weight for central fovea [0-1] */
    float temporal_fovea_weight;        /**< Weight for temporal fovea [0-1] */

    /* Motion detection */
    bool enable_motion_detection;       /**< Enable motion tracking */
    eagle_motion_sensitivity_t motion_sensitivity; /**< Motion sensitivity level */
    float motion_threshold;             /**< Minimum motion threshold */
    uint32_t motion_history_frames;     /**< Frames for motion analysis */

    /* UV spectrum */
    bool enable_uv_sensitivity;         /**< Enable UV spectrum processing */
    eagle_spectrum_mode_t spectrum_mode; /**< Spectrum processing mode */
    float uv_sensitivity_factor;        /**< UV detection sensitivity [0-2] */

    /* Pattern detection */
    bool enable_pattern_detection;      /**< Enable pattern recognition */
    float pattern_threshold;            /**< Minimum pattern confidence */
    uint32_t max_patterns;              /**< Maximum patterns to detect */

    /* Target tracking */
    bool enable_target_tracking;        /**< Enable target tracking */
    uint32_t max_targets;               /**< Maximum tracked targets */
    float target_persistence_frames;    /**< Frames to retain lost targets */

    /* Performance */
    bool enable_parallel_processing;    /**< Enable parallel computation */
    uint32_t processing_threads;        /**< Number of processing threads */
} eagle_vision_config_t;

/**
 * @brief Eagle vision processing state
 */
typedef struct {
    /* Current acuity state */
    float current_acuity;               /**< Active acuity level */
    eagle_fovea_state_t foveae[EAGLE_VISION_FOVEA_COUNT]; /**< Fovea states */

    /* Motion state */
    uint32_t active_motion_vectors;     /**< Currently tracked motions */
    float max_motion_magnitude;         /**< Maximum detected motion */
    float avg_motion_magnitude;         /**< Average motion magnitude */

    /* UV state */
    float uv_ambient_level;             /**< Ambient UV intensity */
    uint32_t uv_trails_detected;        /**< UV trails detected this frame */

    /* Target state */
    uint32_t active_targets;            /**< Currently tracked targets */
    uint32_t high_priority_targets;     /**< High priority target count */
    uint32_t new_targets_this_frame;    /**< Newly acquired targets */
    uint32_t lost_targets_this_frame;   /**< Lost targets this frame */

    /* Pattern state */
    uint32_t patterns_detected;         /**< Patterns detected this frame */
    float avg_pattern_confidence;       /**< Average pattern confidence */

    /* Processing state */
    uint64_t frames_processed;          /**< Total frames processed */
    float processing_load;              /**< Current processing load [0-1] */
    bool is_initialized;                /**< Initialization status */
} eagle_vision_state_t;

/**
 * @brief Eagle vision statistics
 */
typedef struct {
    /* Lifetime statistics */
    uint64_t total_frames_processed;    /**< All frames processed */
    uint64_t total_targets_detected;    /**< All targets ever detected */
    uint64_t total_patterns_detected;   /**< All patterns ever detected */
    uint64_t total_motion_events;       /**< All motion events detected */
    uint64_t total_uv_trails;           /**< All UV trails detected */

    /* Performance metrics */
    float avg_processing_time_ms;       /**< Average processing time */
    float max_processing_time_ms;       /**< Maximum processing time */
    float avg_detection_latency_ms;     /**< Average detection latency */

    /* Accuracy metrics */
    float avg_target_confidence;        /**< Average target confidence */
    float avg_pattern_confidence;       /**< Average pattern confidence */
    float avg_motion_accuracy;          /**< Motion prediction accuracy */

    /* Acuity metrics */
    float avg_effective_acuity;         /**< Average effective acuity */
    float peak_acuity_achieved;         /**< Peak acuity reached */
    uint64_t acuity_adjustments;        /**< Number of acuity adjustments */
} eagle_vision_stats_t;

/**
 * @brief Visual input frame for processing
 */
typedef struct {
    const float* data;          /**< Image data (RGBUV channels) */
    uint32_t width;             /**< Image width */
    uint32_t height;            /**< Image height */
    uint32_t channels;          /**< Number of channels (3-4) */
    uint32_t stride;            /**< Row stride in floats */
    float timestamp_ms;         /**< Frame timestamp */
    bool has_uv_channel;        /**< UV channel present */
} eagle_vision_input_t;

/**
 * @brief Processing output containing all detections
 */
typedef struct {
    /* Targets */
    eagle_target_t* targets;            /**< Detected targets array */
    uint32_t num_targets;               /**< Number of targets */
    uint32_t max_targets;               /**< Array capacity */

    /* Motion vectors */
    eagle_motion_vector_t* motion_vectors; /**< Motion vectors array */
    uint32_t num_motion_vectors;        /**< Number of motion vectors */
    uint32_t max_motion_vectors;        /**< Array capacity */

    /* Patterns */
    eagle_pattern_result_t* patterns;   /**< Detected patterns array */
    uint32_t num_patterns;              /**< Number of patterns */
    uint32_t max_patterns;              /**< Array capacity */

    /* UV results */
    eagle_uv_result_t uv_result;        /**< UV processing result */

    /* Metadata */
    float processing_time_ms;           /**< Processing time for this frame */
    uint64_t frame_number;              /**< Processed frame number */
} eagle_vision_output_t;

/**
 * @brief Opaque eagle vision system handle
 */
typedef struct eagle_vision_system eagle_vision_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Provide sensible starting point for eagle vision processing
 * HOW:  Set all fields to validated default values
 *
 * @param config Output configuration structure
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_default_config(eagle_vision_config_t* config);

/**
 * @brief Create eagle vision processing system
 *
 * WHAT: Allocate and initialize eagle vision processor
 * WHY:  Enable superhuman visual pattern detection
 * HOW:  Allocate memory, initialize foveae, setup motion tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return New eagle vision system or NULL on failure
 */
eagle_vision_system_t* eagle_vision_create(const eagle_vision_config_t* config);

/**
 * @brief Destroy eagle vision system
 *
 * WHAT: Release all resources
 * WHY:  Clean shutdown and memory management
 * HOW:  Free internal buffers, history, tracking state
 *
 * @param system System to destroy (NULL-safe)
 */
void eagle_vision_destroy(eagle_vision_system_t* system);

/**
 * @brief Reset system to initial state
 *
 * WHAT: Clear all tracking and history without reallocation
 * WHY:  Prepare for new scene/sequence
 * HOW:  Zero state, clear history buffers, reset statistics
 *
 * @param system System to reset
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_reset(eagle_vision_system_t* system);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update system configuration
 *
 * @param system Active system
 * @param config New configuration
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_set_config(eagle_vision_system_t* system,
                            const eagle_vision_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param system Active system
 * @param config Output configuration
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_get_config(const eagle_vision_system_t* system,
                            eagle_vision_config_t* config);

/**
 * @brief Set visual acuity level
 *
 * @param system Active system
 * @param acuity Acuity multiplier [1.0-8.0]
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_set_acuity(eagle_vision_system_t* system, float acuity);

/**
 * @brief Set gaze point for foveal processing
 *
 * @param system Active system
 * @param fovea Which fovea to direct
 * @param point Normalized gaze point (0-1 coordinates)
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_set_gaze(eagle_vision_system_t* system,
                          eagle_fovea_type_t fovea,
                          eagle_point_t point);

/* ============================================================================
 * Processing API
 * ============================================================================ */

/**
 * @brief Process single frame
 *
 * WHAT: Run full eagle vision pipeline on input frame
 * WHY:  Main entry point for visual processing
 * HOW:  Foveal enhancement, motion detection, UV processing, pattern matching
 *
 * @param system Active system
 * @param input Input frame to process
 * @param output Output structure (caller allocates arrays)
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_process_frame(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_vision_output_t* output);

/**
 * @brief Detect motion in frame
 *
 * @param system Active system
 * @param input Current frame
 * @param vectors Output motion vectors array
 * @param max_vectors Array capacity
 * @param num_vectors Output: vectors detected
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_detect_motion(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_motion_vector_t* vectors,
                               uint32_t max_vectors,
                               uint32_t* num_vectors);

/**
 * @brief Process UV spectrum data
 *
 * @param system Active system
 * @param input Frame with UV channel
 * @param result UV processing result
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_process_uv(eagle_vision_system_t* system,
                            const eagle_vision_input_t* input,
                            eagle_uv_result_t* result);

/**
 * @brief Detect patterns at enhanced acuity
 *
 * @param system Active system
 * @param input Input frame
 * @param patterns Output patterns array
 * @param max_patterns Array capacity
 * @param num_patterns Output: patterns detected
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_detect_patterns(eagle_vision_system_t* system,
                                 const eagle_vision_input_t* input,
                                 eagle_pattern_result_t* patterns,
                                 uint32_t max_patterns,
                                 uint32_t* num_patterns);

/**
 * @brief Track targets across frames
 *
 * @param system Active system
 * @param input Current frame
 * @param targets Output targets array
 * @param max_targets Array capacity
 * @param num_targets Output: targets tracked
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_track_targets(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_target_t* targets,
                               uint32_t max_targets,
                               uint32_t* num_targets);

/**
 * @brief Estimate distance to target
 *
 * WHAT: Compute relative distance to detected target
 * WHY:  Eagles have excellent depth perception for hunting
 * HOW:  Use size, motion parallax, and binocular cues
 *
 * @param system Active system
 * @param target Target to estimate distance
 * @param distance Output distance estimate
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_estimate_distance(eagle_vision_system_t* system,
                                   const eagle_target_t* target,
                                   float* distance);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get current processing state
 *
 * @param system Active system
 * @param state Output state structure
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_get_state(const eagle_vision_system_t* system,
                           eagle_vision_state_t* state);

/**
 * @brief Get accumulated statistics
 *
 * @param system Active system
 * @param stats Output statistics structure
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_get_stats(const eagle_vision_system_t* system,
                           eagle_vision_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param system Active system
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_reset_stats(eagle_vision_system_t* system);

/**
 * @brief Get fovea state
 *
 * @param system Active system
 * @param fovea Which fovea
 * @param state Output fovea state
 * @return EAGLE_VISION_SUCCESS or error code
 */
int eagle_vision_get_fovea_state(const eagle_vision_system_t* system,
                                 eagle_fovea_type_t fovea,
                                 eagle_fovea_state_t* state);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Allocate output structure with arrays
 *
 * @param max_targets Maximum targets capacity
 * @param max_motions Maximum motion vectors capacity
 * @param max_patterns Maximum patterns capacity
 * @return Allocated output or NULL
 */
eagle_vision_output_t* eagle_vision_output_create(uint32_t max_targets,
                                                  uint32_t max_motions,
                                                  uint32_t max_patterns);

/**
 * @brief Free output structure and arrays
 *
 * @param output Output to free (NULL-safe)
 */
void eagle_vision_output_destroy(eagle_vision_output_t* output);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* eagle_vision_error_string(eagle_vision_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EAGLE_VISION_H */
