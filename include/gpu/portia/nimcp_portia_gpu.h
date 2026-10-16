/**
 * @file nimcp_portia_gpu.h
 * @brief GPU-accelerated Portia Spider Vision Kernels
 *
 * WHAT: CUDA kernels for Portia spider-inspired visual cognition
 * WHY:  GPU acceleration for sophisticated spatial reasoning in tiny brain
 * HOW:  Custom kernels for attention, spatial mapping, route planning
 *
 * ARCHITECTURE:
 * - Portia spider has exceptional visual cognition despite small brain
 * - Specialized for prey recognition and route planning
 * - Uses selective attention and cognitive mapping
 * - Implements detour behavior and spatial reasoning
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PORTIA_GPU_H
#define NIMCP_PORTIA_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Portia Vision Parameters
//=============================================================================

/**
 * @brief Portia visual attention parameters
 */
typedef struct {
    float salience_threshold;     /**< Threshold for attention capture */
    float movement_sensitivity;   /**< Sensitivity to movement */
    float prey_template_weight;   /**< Weight for prey recognition */
    float novelty_bonus;          /**< Bonus for novel objects */
    int attention_resolution;     /**< Attention map resolution */
    float saccade_rate;           /**< Saccade movement rate */
    float fixation_duration;      /**< Mean fixation duration (ms) */
    int max_tracked_objects;      /**< Maximum objects to track */
} nimcp_gpu_portia_attention_params_t;

/**
 * @brief Portia spatial cognition parameters
 */
typedef struct {
    int map_resolution;           /**< Cognitive map resolution */
    float landmark_weight;        /**< Weight for landmark memory */
    float path_integration_gain;  /**< Path integration gain */
    float detour_threshold;       /**< Threshold for detour planning */
    int planning_depth;           /**< Route planning depth */
    float obstacle_memory_decay;  /**< Obstacle memory decay rate */
    float goal_persistence;       /**< Goal persistence weight */
    bool use_mental_rotation;     /**< Enable mental rotation */
} nimcp_gpu_portia_spatial_params_t;

/**
 * @brief Portia prey recognition parameters
 */
typedef struct {
    int num_prey_templates;       /**< Number of prey templates */
    float template_match_threshold; /**< Template matching threshold */
    float size_tolerance;         /**< Size variation tolerance */
    float motion_pattern_weight;  /**< Motion pattern recognition weight */
    float deceptive_approach_rate;/**< Deceptive stalking rate */
    bool cryptic_prey_detection;  /**< Enable cryptic prey detection */
} nimcp_gpu_portia_prey_params_t;

//=============================================================================
// Portia State Structures
//=============================================================================

/**
 * @brief Portia attention state
 */
typedef struct {
    nimcp_gpu_tensor_t* salience_map;     /**< Visual salience map */
    nimcp_gpu_tensor_t* attention_focus;  /**< Current attention focus */
    nimcp_gpu_tensor_t* tracked_objects;  /**< Tracked object positions */
    nimcp_gpu_tensor_t* object_velocities;/**< Object velocity estimates */
    nimcp_gpu_tensor_t* fixation_history; /**< Recent fixation points */
    nimcp_gpu_tensor_t* novelty_map;      /**< Novelty detection map */
    size_t map_width;                     /**< Map width */
    size_t map_height;                    /**< Map height */
    size_t n_tracked;                     /**< Number of tracked objects */
} nimcp_gpu_portia_attention_state_t;

/**
 * @brief Portia cognitive map state
 */
typedef struct {
    nimcp_gpu_tensor_t* spatial_map;      /**< Allocentric spatial map */
    nimcp_gpu_tensor_t* landmark_memory;  /**< Landmark representations */
    nimcp_gpu_tensor_t* obstacle_map;     /**< Known obstacles */
    nimcp_gpu_tensor_t* path_history;     /**< Recent path taken */
    nimcp_gpu_tensor_t* goal_location;    /**< Current goal */
    nimcp_gpu_tensor_t* planned_route;    /**< Planned route to goal */
    nimcp_gpu_tensor_t* current_position; /**< Egocentric position */
    nimcp_gpu_tensor_t* heading;          /**< Current heading direction */
    size_t map_size;                      /**< Map grid size */
} nimcp_gpu_portia_spatial_state_t;

/**
 * @brief Portia prey tracking state
 */
typedef struct {
    nimcp_gpu_tensor_t* prey_templates;   /**< Learned prey templates */
    nimcp_gpu_tensor_t* current_prey;     /**< Current prey detection */
    nimcp_gpu_tensor_t* prey_trajectory;  /**< Predicted prey trajectory */
    nimcp_gpu_tensor_t* approach_plan;    /**< Approach strategy */
    nimcp_gpu_tensor_t* detection_confidence; /**< Detection confidence */
    size_t n_templates;                   /**< Number of templates */
    size_t template_dim;                  /**< Template dimension */
} nimcp_gpu_portia_prey_state_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_portia_attention_params_t nimcp_gpu_portia_attention_params_default(void);
NIMCP_EXPORT nimcp_gpu_portia_spatial_params_t nimcp_gpu_portia_spatial_params_default(void);
NIMCP_EXPORT nimcp_gpu_portia_prey_params_t nimcp_gpu_portia_prey_params_default(void);

//=============================================================================
// Attention Functions
//=============================================================================

/**
 * @brief Compute visual salience map
 */
NIMCP_EXPORT bool nimcp_gpu_portia_compute_salience(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params);

/**
 * @brief Update attention focus based on salience
 */
NIMCP_EXPORT bool nimcp_gpu_portia_update_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    float dt,
    const nimcp_gpu_portia_attention_params_t* params);

/**
 * @brief Track moving objects
 */
NIMCP_EXPORT bool nimcp_gpu_portia_track_objects(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    float dt,
    const nimcp_gpu_portia_attention_params_t* params);

/**
 * @brief Compute novelty detection
 */
NIMCP_EXPORT bool nimcp_gpu_portia_novelty_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params);

//=============================================================================
// Spatial Cognition Functions
//=============================================================================

/**
 * @brief Update cognitive spatial map
 */
NIMCP_EXPORT bool nimcp_gpu_portia_update_spatial_map(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* movement,
    const nimcp_gpu_portia_spatial_params_t* params);

/**
 * @brief Plan detour route to goal
 */
NIMCP_EXPORT bool nimcp_gpu_portia_plan_route(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* goal,
    const nimcp_gpu_portia_spatial_params_t* params);

/**
 * @brief Perform mental rotation for route planning
 */
NIMCP_EXPORT bool nimcp_gpu_portia_mental_rotation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    float rotation_angle,
    nimcp_gpu_tensor_t* rotated_view,
    const nimcp_gpu_portia_spatial_params_t* params);

/**
 * @brief Update path integration
 */
NIMCP_EXPORT bool nimcp_gpu_portia_path_integration(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* self_motion,
    float dt,
    const nimcp_gpu_portia_spatial_params_t* params);

/**
 * @brief Detect and plan detour around obstacle
 */
NIMCP_EXPORT bool nimcp_gpu_portia_detour_planning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* obstacle,
    const nimcp_gpu_portia_spatial_params_t* params);

//=============================================================================
// Prey Recognition Functions
//=============================================================================

/**
 * @brief Match visual input against prey templates
 */
NIMCP_EXPORT bool nimcp_gpu_portia_match_prey(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_tensor_t* visual_patch,
    const nimcp_gpu_portia_prey_params_t* params);

/**
 * @brief Predict prey trajectory
 */
NIMCP_EXPORT bool nimcp_gpu_portia_predict_prey_trajectory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    float dt,
    const nimcp_gpu_portia_prey_params_t* params);

/**
 * @brief Plan deceptive approach
 */
NIMCP_EXPORT bool nimcp_gpu_portia_plan_approach(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_portia_spatial_state_t* spatial_state,
    const nimcp_gpu_portia_prey_params_t* params);

/**
 * @brief Update prey templates through learning
 */
NIMCP_EXPORT bool nimcp_gpu_portia_update_prey_templates(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_tensor_t* successful_prey,
    float learning_rate,
    const nimcp_gpu_portia_prey_params_t* params);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_GPU_H
