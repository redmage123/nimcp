/**
 * @file nimcp_dragonfly_vision_gpu.h
 * @brief GPU-Accelerated Dragonfly Vision System Kernels
 *
 * WHAT: CUDA kernels for dragonfly-inspired visual processing
 * WHY:  Enable massive parallel processing of visual field (like ~30,000 ommatidia)
 * HOW:  Custom kernels for target tracking, optical flow, prey detection, collision avoidance
 *
 * BIOLOGICAL REFERENCE:
 * - Dragonflies have compound eyes with ~30,000 ommatidia per eye
 * - Each ommatidium processes independently with local motion detection
 * - STMD neurons detect small targets against complex backgrounds
 * - CSTMD1 implements winner-take-all target selection
 * - TSDN population vector encodes target direction with 16 neurons
 *
 * GPU ARCHITECTURE:
 * - Each thread = one ommatidium or one target
 * - Block = local neighborhood for motion integration
 * - Grid = entire visual field
 *
 * KEY INSIGHT:
 * Dragonfly vision is inherently parallel - this maps naturally to GPU:
 * - Per-pixel optical flow (Lucas-Kanade)
 * - Per-ommatidium motion detection
 * - Per-target Kalman filter updates
 * - Population vector encoding (16 TSDNs)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_DRAGONFLY_VISION_GPU_H
#define NIMCP_DRAGONFLY_VISION_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

#define DFV_MAX_TARGETS 32                 /**< Maximum tracked targets */
#define DFV_TSDN_NEURONS 16                /**< TSDN population size */
#define DFV_KALMAN_STATE_DIM 6             /**< State dimension (pos + vel) */
#define DFV_OPTICAL_FLOW_WINDOW 5          /**< Lucas-Kanade window size */
#define DFV_STMD_KERNEL_SIZE 7             /**< STMD spatial kernel size */
#define DFV_ATTENTION_SECTORS 8            /**< Attention priority sectors */

//=============================================================================
// Multi-Target Tracking State
//=============================================================================

/**
 * @brief GPU target state for Kalman filter
 *
 * Each target has 6D state: [x, y, z, vx, vy, vz]
 */
typedef struct {
    nimcp_gpu_tensor_t* state;             /**< State [n_targets, 6] */
    nimcp_gpu_tensor_t* covariance;        /**< Covariance [n_targets, 6, 6] */
    nimcp_gpu_tensor_t* confidence;        /**< Tracking confidence [n_targets] */
    nimcp_gpu_tensor_t* priority;          /**< Priority score [n_targets] */
    nimcp_gpu_tensor_t* visible;           /**< Visibility flag [n_targets] */
    uint32_t n_targets;                    /**< Current number of targets */
    uint32_t max_targets;                  /**< Maximum targets (DFV_MAX_TARGETS) */
} dfv_target_state_t;

/**
 * @brief Kalman filter parameters (per target type)
 */
typedef struct {
    float process_noise;                   /**< Process noise (Q) */
    float measurement_noise;               /**< Measurement noise (R) */
    float dt;                              /**< Time step (seconds) */
    float velocity_decay;                  /**< Velocity decay (for Singer model) */
    float acceleration_variance;           /**< Acceleration variance (Singer model) */
} dfv_kalman_params_t;

//=============================================================================
// Optical Flow State
//=============================================================================

/**
 * @brief GPU optical flow state
 */
typedef struct {
    nimcp_gpu_tensor_t* flow_u;            /**< Horizontal flow [H, W] */
    nimcp_gpu_tensor_t* flow_v;            /**< Vertical flow [H, W] */
    nimcp_gpu_tensor_t* magnitude;         /**< Flow magnitude [H, W] */
    nimcp_gpu_tensor_t* direction;         /**< Flow direction [H, W] */
    nimcp_gpu_tensor_t* confidence;        /**< Flow confidence [H, W] */
    nimcp_gpu_tensor_t* prev_frame;        /**< Previous frame buffer [H, W] */
    uint32_t width;                        /**< Frame width */
    uint32_t height;                       /**< Frame height */
    int window_size;                       /**< Lucas-Kanade window size */
} dfv_optical_flow_state_t;

//=============================================================================
// Gaze Control State
//=============================================================================

/**
 * @brief GPU gaze control state
 */
typedef struct {
    nimcp_gpu_tensor_t* attention_map;     /**< Attention priority map [H, W] */
    nimcp_gpu_tensor_t* saccade_target;    /**< Saccade target [2] (az, el) */
    nimcp_gpu_tensor_t* pursuit_velocity;  /**< Smooth pursuit velocity [2] */
    nimcp_gpu_tensor_t* head_position;     /**< Head angles [3] (yaw, pitch, roll) */
    nimcp_gpu_tensor_t* sector_priority;   /**< Sector priorities [N_SECTORS] */
    float vor_gain;                        /**< Vestibular-ocular reflex gain */
    float pursuit_gain;                    /**< Smooth pursuit gain */
    bool saccade_in_progress;              /**< Saccade active flag */
} dfv_gaze_state_t;

//=============================================================================
// Prey Detection State (STMD neurons)
//=============================================================================

/**
 * @brief GPU STMD (Small Target Motion Detector) state
 */
typedef struct {
    nimcp_gpu_tensor_t* stmd_response;     /**< STMD neuron response [H, W] */
    nimcp_gpu_tensor_t* velocity_tuning;   /**< Velocity-tuned responses [H, W] */
    nimcp_gpu_tensor_t* temporal_buffer;   /**< Temporal history [N, H, W] */
    nimcp_gpu_tensor_t* fg_mask;           /**< Figure-ground mask [H, W] */
    nimcp_gpu_tensor_t* detection_map;     /**< Detection probability [H, W] */
    uint32_t buffer_depth;                 /**< Temporal buffer depth */
    uint32_t current_frame;                /**< Current frame index in buffer */
    float optimal_size_deg;                /**< Optimal target size (degrees) */
    float optimal_velocity_dps;            /**< Optimal velocity (deg/s) */
} dfv_stmd_state_t;

//=============================================================================
// Collision Avoidance State
//=============================================================================

/**
 * @brief GPU collision avoidance state
 */
typedef struct {
    nimcp_gpu_tensor_t* ttc_map;           /**< Time-to-collision map [H, W] */
    nimcp_gpu_tensor_t* looming;           /**< Looming detector output [H, W] */
    nimcp_gpu_tensor_t* escape_vector;     /**< Escape direction [3] */
    nimcp_gpu_tensor_t* obstacle_mask;     /**< Obstacle detection mask [H, W] */
    nimcp_gpu_tensor_t* sector_clearance;  /**< Clearance per sector [N_SECTORS] */
    float min_ttc_threshold;               /**< Minimum TTC warning threshold */
    float critical_ttc_threshold;          /**< Critical TTC threshold */
} dfv_collision_state_t;

//=============================================================================
// TSDN Population Vector State
//=============================================================================

/**
 * @brief GPU TSDN (Target-Selective Descending Neuron) state
 */
typedef struct {
    nimcp_gpu_tensor_t* firing_rates;      /**< TSDN firing rates [16] */
    nimcp_gpu_tensor_t* preferred_dirs;    /**< Preferred directions [16] */
    nimcp_gpu_tensor_t* population_vector; /**< Decoded direction [3] */
    nimcp_gpu_tensor_t* facilitation;      /**< Predictive facilitation [16] */
    float tuning_width;                    /**< Tuning curve width (radians) */
    float tuning_exponent;                 /**< Cosine tuning exponent */
    float gain;                            /**< Global gain modulation */
} dfv_tsdn_state_t;

//=============================================================================
// Unified Dragonfly Vision GPU Context
//=============================================================================

/**
 * @brief Complete GPU dragonfly vision context
 */
typedef struct {
    dfv_target_state_t* targets;           /**< Multi-target tracking */
    dfv_optical_flow_state_t* flow;        /**< Optical flow processing */
    dfv_gaze_state_t* gaze;                /**< Gaze control */
    dfv_stmd_state_t* stmd;                /**< Small target motion detection */
    dfv_collision_state_t* collision;      /**< Collision avoidance */
    dfv_tsdn_state_t* tsdn;                /**< TSDN population encoding */
    nimcp_gpu_context_t* gpu_ctx;          /**< GPU context reference */
    uint32_t frame_width;                  /**< Input frame width */
    uint32_t frame_height;                 /**< Input frame height */
    bool initialized;                      /**< Initialization flag */
} dfv_gpu_context_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create dragonfly vision GPU context
 *
 * @param gpu_ctx GPU context
 * @param frame_width Input frame width
 * @param frame_height Input frame height
 * @return Dragonfly vision context or NULL on failure
 */
NIMCP_EXPORT dfv_gpu_context_t* dfv_gpu_context_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t frame_width,
    uint32_t frame_height
);

/**
 * @brief Destroy dragonfly vision GPU context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
NIMCP_EXPORT void dfv_gpu_context_destroy(dfv_gpu_context_t* ctx);

/**
 * @brief Reset dragonfly vision state
 *
 * @param ctx Dragonfly vision context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int dfv_gpu_reset(dfv_gpu_context_t* ctx);

//=============================================================================
// Multi-Target Tracking Kernels
//=============================================================================

/**
 * @brief Create target tracking state
 *
 * @param gpu_ctx GPU context
 * @param max_targets Maximum number of targets
 * @return Target state or NULL on failure
 */
NIMCP_EXPORT dfv_target_state_t* dfv_target_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t max_targets
);

/**
 * @brief Destroy target tracking state
 */
NIMCP_EXPORT void dfv_target_state_destroy(dfv_target_state_t* state);

/**
 * @brief Parallel target detection across visual field
 *
 * Detects potential targets based on motion and contrast.
 * Each thread processes one pixel/ommatidium.
 *
 * @param ctx Dragonfly vision context
 * @param frame Current frame [H, W]
 * @param motion_field Motion field from optical flow [H, W, 2]
 * @param detections Output: detection positions [max_detections, 3]
 * @param scores Output: detection scores [max_detections]
 * @param n_detections Output: number of detections
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_detect_targets(
    dfv_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* frame,
    const nimcp_gpu_tensor_t* motion_field,
    nimcp_gpu_tensor_t* detections,
    nimcp_gpu_tensor_t* scores,
    uint32_t* n_detections
);

/**
 * @brief Parallel Kalman filter prediction step
 *
 * State prediction: x_pred = F * x + B * u
 * Covariance prediction: P_pred = F * P * F^T + Q
 *
 * @param gpu_ctx GPU context
 * @param state Target state
 * @param params Kalman parameters
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_kalman_predict(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_target_state_t* state,
    const dfv_kalman_params_t* params
);

/**
 * @brief Parallel Kalman filter update step
 *
 * Innovation: y = z - H * x_pred
 * Kalman gain: K = P_pred * H^T * (H * P_pred * H^T + R)^-1
 * State update: x = x_pred + K * y
 *
 * @param gpu_ctx GPU context
 * @param state Target state
 * @param measurements Measurements [n_targets, 3]
 * @param measurement_valid Valid flags [n_targets]
 * @param params Kalman parameters
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_kalman_update(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_target_state_t* state,
    const nimcp_gpu_tensor_t* measurements,
    const nimcp_gpu_tensor_t* measurement_valid,
    const dfv_kalman_params_t* params
);

/**
 * @brief Data association using auction algorithm (GPU approximation)
 *
 * Associates detections with existing tracks using parallel Hungarian
 * approximation based on auction algorithm.
 *
 * @param gpu_ctx GPU context
 * @param state Target state
 * @param detections New detections [n_detections, 3]
 * @param n_detections Number of detections
 * @param association Output: track indices for each detection [-1 = new]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_data_association(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_target_state_t* state,
    const nimcp_gpu_tensor_t* detections,
    uint32_t n_detections,
    nimcp_gpu_tensor_t* association
);

/**
 * @brief Track lifecycle management (creation, update, deletion)
 *
 * @param gpu_ctx GPU context
 * @param state Target state
 * @param association Detection-track associations
 * @param detections New detections
 * @param n_detections Number of detections
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_track_management(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_target_state_t* state,
    const nimcp_gpu_tensor_t* association,
    const nimcp_gpu_tensor_t* detections,
    uint32_t n_detections
);

//=============================================================================
// Optical Flow Kernels
//=============================================================================

/**
 * @brief Create optical flow state
 *
 * @param gpu_ctx GPU context
 * @param width Frame width
 * @param height Frame height
 * @param window_size Lucas-Kanade window size
 * @return Optical flow state or NULL on failure
 */
NIMCP_EXPORT dfv_optical_flow_state_t* dfv_optical_flow_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t width,
    uint32_t height,
    int window_size
);

/**
 * @brief Destroy optical flow state
 */
NIMCP_EXPORT void dfv_optical_flow_state_destroy(dfv_optical_flow_state_t* state);

/**
 * @brief Compute Lucas-Kanade optical flow (parallel per pixel)
 *
 * Each thread computes flow for one pixel using local window.
 * Solves: [sum(Ix^2)  sum(IxIy)] [u]   [sum(IxIt)]
 *         [sum(IxIy)  sum(Iy^2)] [v] = [sum(IyIt)]
 *
 * @param gpu_ctx GPU context
 * @param state Optical flow state
 * @param current_frame Current frame [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_optical_flow_lk(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_optical_flow_state_t* state,
    const nimcp_gpu_tensor_t* current_frame
);

/**
 * @brief Compute motion field from optical flow
 *
 * Aggregates per-pixel flow into coherent motion field.
 *
 * @param gpu_ctx GPU context
 * @param state Optical flow state
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_compute_motion_field(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_optical_flow_state_t* state
);

/**
 * @brief Looming detection (radial expansion detection)
 *
 * Detects expanding patterns indicating approaching objects.
 * Critical for collision avoidance.
 *
 * @param gpu_ctx GPU context
 * @param flow Optical flow state
 * @param looming_map Output: looming detection [H, W]
 * @param focus_of_expansion Output: FOE coordinates [2]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_detect_looming(
    nimcp_gpu_context_t* gpu_ctx,
    const dfv_optical_flow_state_t* flow,
    nimcp_gpu_tensor_t* looming_map,
    nimcp_gpu_tensor_t* focus_of_expansion
);

//=============================================================================
// Gaze Control Kernels
//=============================================================================

/**
 * @brief Create gaze control state
 */
NIMCP_EXPORT dfv_gaze_state_t* dfv_gaze_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t width,
    uint32_t height
);

/**
 * @brief Destroy gaze control state
 */
NIMCP_EXPORT void dfv_gaze_state_destroy(dfv_gaze_state_t* state);

/**
 * @brief Compute attention priority map
 *
 * Combines target salience, motion, and task priorities.
 *
 * @param gpu_ctx GPU context
 * @param state Gaze state
 * @param target_positions Target positions [n_targets, 3]
 * @param target_priorities Target priorities [n_targets]
 * @param n_targets Number of targets
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_compute_attention_map(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_gaze_state_t* state,
    const nimcp_gpu_tensor_t* target_positions,
    const nimcp_gpu_tensor_t* target_priorities,
    uint32_t n_targets
);

/**
 * @brief Plan saccade to target
 *
 * @param gpu_ctx GPU context
 * @param state Gaze state
 * @param target_az Target azimuth (radians)
 * @param target_el Target elevation (radians)
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_plan_saccade(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_gaze_state_t* state,
    float target_az,
    float target_el
);

/**
 * @brief Compute smooth pursuit velocity
 *
 * @param gpu_ctx GPU context
 * @param state Gaze state
 * @param target_velocity Target velocity [3]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_smooth_pursuit(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_gaze_state_t* state,
    const nimcp_gpu_tensor_t* target_velocity
);

//=============================================================================
// Prey Detection Kernels (STMD)
//=============================================================================

/**
 * @brief Create STMD (Small Target Motion Detector) state
 */
NIMCP_EXPORT dfv_stmd_state_t* dfv_stmd_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t width,
    uint32_t height,
    uint32_t buffer_depth
);

/**
 * @brief Destroy STMD state
 */
NIMCP_EXPORT void dfv_stmd_state_destroy(dfv_stmd_state_t* state);

/**
 * @brief STMD small target motion detection
 *
 * Implements biological STMD neuron response:
 * - Spatiotemporal filtering tuned to small targets
 * - Velocity tuning (optimal velocity ~30-50 deg/s for dragonflies)
 * - Size tuning (optimal ~1-3 degrees)
 *
 * @param gpu_ctx GPU context
 * @param state STMD state
 * @param frame Current frame [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_stmd_detect(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_stmd_state_t* state,
    const nimcp_gpu_tensor_t* frame
);

/**
 * @brief Figure-ground segregation
 *
 * Separates small moving targets from background clutter.
 *
 * @param gpu_ctx GPU context
 * @param state STMD state
 * @param optical_flow Optical flow [H, W, 2]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_figure_ground(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_stmd_state_t* state,
    const nimcp_gpu_tensor_t* optical_flow
);

/**
 * @brief Velocity-tuned filtering
 *
 * @param gpu_ctx GPU context
 * @param state STMD state
 * @param min_velocity Minimum velocity (deg/s)
 * @param max_velocity Maximum velocity (deg/s)
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_velocity_filter(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_stmd_state_t* state,
    float min_velocity,
    float max_velocity
);

//=============================================================================
// Collision Avoidance Kernels
//=============================================================================

/**
 * @brief Create collision avoidance state
 */
NIMCP_EXPORT dfv_collision_state_t* dfv_collision_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t width,
    uint32_t height
);

/**
 * @brief Destroy collision state
 */
NIMCP_EXPORT void dfv_collision_state_destroy(dfv_collision_state_t* state);

/**
 * @brief Compute time-to-collision map
 *
 * TTC = distance / closing_velocity (per pixel)
 *
 * @param gpu_ctx GPU context
 * @param state Collision state
 * @param depth_map Depth estimate [H, W]
 * @param optical_flow Optical flow [H, W, 2]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_compute_ttc(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_collision_state_t* state,
    const nimcp_gpu_tensor_t* depth_map,
    const nimcp_gpu_tensor_t* optical_flow
);

/**
 * @brief Plan escape trajectory
 *
 * Computes safe direction away from obstacles while
 * optionally maintaining pursuit goal.
 *
 * @param gpu_ctx GPU context
 * @param state Collision state
 * @param current_heading Current heading [3]
 * @param pursuit_direction Pursuit goal direction [3] (can be NULL)
 * @param escape_direction Output: escape direction [3]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_plan_escape(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_collision_state_t* state,
    const nimcp_gpu_tensor_t* current_heading,
    const nimcp_gpu_tensor_t* pursuit_direction,
    nimcp_gpu_tensor_t* escape_direction
);

/**
 * @brief Check path clearance for trajectory
 *
 * @param gpu_ctx GPU context
 * @param state Collision state
 * @param trajectory Planned trajectory points [n_points, 3]
 * @param n_points Number of trajectory points
 * @param min_clearance Output: minimum clearance along path
 * @return true if path is clear (above threshold)
 */
NIMCP_EXPORT bool dfv_gpu_check_path_clearance(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_collision_state_t* state,
    const nimcp_gpu_tensor_t* trajectory,
    uint32_t n_points,
    float* min_clearance
);

//=============================================================================
// TSDN Population Vector Kernels
//=============================================================================

/**
 * @brief Create TSDN population state
 */
NIMCP_EXPORT dfv_tsdn_state_t* dfv_tsdn_state_create(
    nimcp_gpu_context_t* gpu_ctx
);

/**
 * @brief Destroy TSDN state
 */
NIMCP_EXPORT void dfv_tsdn_state_destroy(dfv_tsdn_state_t* state);

/**
 * @brief Encode target direction as TSDN firing rates
 *
 * Each of 16 TSDNs responds based on cosine tuning:
 * rate[i] = max(0, cos(target_dir - preferred_dir[i]))^exponent
 *
 * @param gpu_ctx GPU context
 * @param state TSDN state
 * @param target_direction Target direction [2] (azimuth, elevation)
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_tsdn_encode(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_tsdn_state_t* state,
    const nimcp_gpu_tensor_t* target_direction
);

/**
 * @brief Decode population vector from TSDN firing rates
 *
 * Population vector: sum(rate[i] * preferred_dir[i]) / sum(rate[i])
 *
 * @param gpu_ctx GPU context
 * @param state TSDN state
 * @param decoded_direction Output: decoded direction [2]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_tsdn_decode(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_tsdn_state_t* state,
    nimcp_gpu_tensor_t* decoded_direction
);

/**
 * @brief Apply predictive facilitation to TSDN
 *
 * Boosts gain for TSDNs in predicted direction.
 *
 * @param gpu_ctx GPU context
 * @param state TSDN state
 * @param predicted_direction Predicted direction [2]
 * @param facilitation_strength Facilitation strength [0,1]
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_tsdn_facilitate(
    nimcp_gpu_context_t* gpu_ctx,
    dfv_tsdn_state_t* state,
    const nimcp_gpu_tensor_t* predicted_direction,
    float facilitation_strength
);

//=============================================================================
// Integrated Pipeline
//=============================================================================

/**
 * @brief Run complete dragonfly vision pipeline
 *
 * Processes one frame through:
 * 1. Optical flow computation
 * 2. STMD prey detection
 * 3. Target detection and tracking
 * 4. Collision avoidance
 * 5. TSDN encoding
 * 6. Gaze control
 *
 * @param ctx Dragonfly vision context
 * @param frame Current frame [H, W]
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool dfv_gpu_process_frame(
    dfv_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* frame,
    float dt
);

/**
 * @brief Get primary target info
 *
 * @param ctx Dragonfly vision context
 * @param position Output: target position [3]
 * @param velocity Output: target velocity [3]
 * @param confidence Output: tracking confidence
 * @return true if primary target exists
 */
NIMCP_EXPORT bool dfv_gpu_get_primary_target(
    const dfv_gpu_context_t* ctx,
    float* position,
    float* velocity,
    float* confidence
);

/**
 * @brief Get collision avoidance command
 *
 * @param ctx Dragonfly vision context
 * @param min_ttc Output: minimum time-to-collision
 * @param escape_dir Output: escape direction [3]
 * @return true if evasion needed
 */
NIMCP_EXPORT bool dfv_gpu_get_collision_command(
    const dfv_gpu_context_t* ctx,
    float* min_ttc,
    float* escape_dir
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_VISION_GPU_H */
