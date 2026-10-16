/**
 * @file nimcp_hypothalamus_gpu.h
 * @brief GPU-Accelerated Hypothalamus Drive Dynamics
 *
 * WHAT: CUDA kernels for parallel drive state evolution and reward computation
 * WHY:  GPU acceleration for biologically-inspired drive dynamics at scale
 * HOW:  Custom kernels for ODE integration, setpoint evaluation, reward signals
 *
 * ARCHITECTURE:
 * =============
 *
 *   +------------------------------------------------------------------+
 *   |                 HYPOTHALAMUS GPU KERNELS                         |
 *   |                                                                  |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   |  |  Drive State   |  |   Setpoint     |  |    Reward      |      |
 *   |  |  Integration   |  |  Evaluation    |  |   Computation  |      |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   |           |                  |                   |               |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   |  |   Urgency      |  |   Homeostatic  |  |  Alignment     |      |
 *   |  |   Priority     |  |   Controller   |  |   Scoring      |      |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   |                              |                                   |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   |  |   Nucleus      |  |  Drive-Goal    |  |    Batch       |      |
 *   |  |   Activity     |  |   Mapping      |  |   Processing   |      |
 *   |  +----------------+  +----------------+  +----------------+      |
 *   +------------------------------------------------------------------+
 *
 * HOT PATHS ACCELERATED:
 * =====================
 * 1. Drive State Integration: Parallel Euler/RK4 for all drives simultaneously
 * 2. Setpoint Deviation: Batch computation across multiple agents/contexts
 * 3. Reward Signal: Parallel reward aggregation with alignment weights
 * 4. Urgency Ranking: GPU-accelerated priority sorting
 *
 * BYRNES ALIGNMENT SAFETY:
 * ========================
 * All GPU kernels respect alignment weights and setpoint locks:
 * - Alignment weights are read-only in GPU memory
 * - Setpoint modifications require CPU-side verification
 * - Reward computation includes alignment bonus/penalty terms
 *
 * @version Phase 18: GPU Acceleration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_GPU_H
#define NIMCP_HYPOTHALAMUS_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Number of drive types (matches HYPO_DRIVE_COUNT) */
#define NIMCP_HYPO_GPU_DRIVE_COUNT        9

/** @brief Number of nucleus types */
#define NIMCP_HYPO_GPU_NUCLEUS_COUNT      10

/** @brief Maximum batch size for multi-agent processing */
#define NIMCP_HYPO_GPU_MAX_BATCH          1024

/** @brief Default CUDA block size */
#define NIMCP_HYPO_GPU_BLOCK_SIZE         256

/** @brief Warp size for reduction operations */
#define NIMCP_HYPO_GPU_WARP_SIZE          32

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief ODE integration method
 */
typedef enum {
    NIMCP_HYPO_GPU_EULER = 0,         /**< Forward Euler (fast, less accurate) */
    NIMCP_HYPO_GPU_RK2,               /**< Runge-Kutta 2nd order */
    NIMCP_HYPO_GPU_RK4,               /**< Runge-Kutta 4th order (accurate) */
    NIMCP_HYPO_GPU_ADAPTIVE           /**< Adaptive step size */
} nimcp_hypo_gpu_integrator_t;

/**
 * @brief Reward computation mode
 */
typedef enum {
    NIMCP_HYPO_GPU_REWARD_SIMPLE = 0, /**< Simple drive satisfaction sum */
    NIMCP_HYPO_GPU_REWARD_WEIGHTED,   /**< Weighted by alignment parameters */
    NIMCP_HYPO_GPU_REWARD_ALIGNED,    /**< Full alignment bonus/penalty */
    NIMCP_HYPO_GPU_REWARD_RPE         /**< Reward Prediction Error mode */
} nimcp_hypo_gpu_reward_mode_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief GPU drive dynamics configuration
 */
typedef struct {
    nimcp_hypo_gpu_integrator_t integrator;  /**< ODE integration method */
    float dt;                                 /**< Time step (seconds) */
    float dt_min;                             /**< Minimum adaptive dt */
    float dt_max;                             /**< Maximum adaptive dt */
    float error_tolerance;                    /**< Adaptive error tolerance */
    uint32_t threads_per_block;              /**< CUDA threads per block */
    uint32_t max_blocks;                      /**< Maximum blocks (0 = auto) */
} nimcp_hypo_gpu_dynamics_config_t;

/**
 * @brief GPU reward computation configuration
 */
typedef struct {
    nimcp_hypo_gpu_reward_mode_t mode;       /**< Reward computation mode */
    float alignment_weight;                   /**< Weight for alignment terms */
    float temporal_discount;                  /**< Future reward discounting */
    float reward_smoothing;                   /**< Temporal smoothing factor */
    bool include_rpe;                         /**< Include prediction error */
} nimcp_hypo_gpu_reward_config_t;

/**
 * @brief GPU homeostatic controller configuration
 */
typedef struct {
    float kp;                                /**< Proportional gain */
    float ki;                                /**< Integral gain */
    float kd;                                /**< Derivative gain */
    float integral_limit;                    /**< Anti-windup limit */
    float output_min;                        /**< Minimum controller output */
    float output_max;                        /**< Maximum controller output */
} nimcp_hypo_gpu_pid_config_t;

/*=============================================================================
 * GPU STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief GPU drive state tensor layout
 *
 * Shape: [batch_size, DRIVE_COUNT, 8]
 * Channels: level, urgency, satisfaction, setpoint, deviation,
 *           rise_rate, decay_rate, baseline
 */
typedef struct {
    nimcp_gpu_tensor_t* state;               /**< Drive state tensor */
    nimcp_gpu_tensor_t* urgency;             /**< Urgency vector [batch, drives] */
    nimcp_gpu_tensor_t* satisfaction;        /**< Satisfaction vector */
    nimcp_gpu_tensor_t* deviation;           /**< Setpoint deviation */
    size_t batch_size;                       /**< Current batch size */
} nimcp_hypo_gpu_drive_state_t;

/**
 * @brief GPU setpoint configuration (read-only on GPU)
 *
 * Shape: [batch_size, 4+4] = [physio setpoints, alignment weights]
 */
typedef struct {
    nimcp_gpu_tensor_t* setpoints;           /**< Setpoint values */
    nimcp_gpu_tensor_t* alignment_weights;   /**< Alignment weights (human_wellbeing, etc.) */
    nimcp_gpu_tensor_t* lock_mask;           /**< Which setpoints are locked */
    bool alignment_locked;                   /**< Global alignment lock flag */
} nimcp_hypo_gpu_setpoints_t;

/**
 * @brief GPU reward signal output
 */
typedef struct {
    nimcp_gpu_tensor_t* reward;              /**< Reward signal [batch] */
    nimcp_gpu_tensor_t* rpe;                 /**< Reward prediction error [batch] */
    nimcp_gpu_tensor_t* alignment_bonus;     /**< Alignment bonus [batch] */
    nimcp_gpu_tensor_t* alignment_penalty;   /**< Alignment penalty [batch] */
    nimcp_gpu_tensor_t* drive_rewards;       /**< Per-drive rewards [batch, drives] */
} nimcp_hypo_gpu_reward_output_t;

/**
 * @brief GPU nucleus activity state
 *
 * Shape: [batch_size, NUCLEUS_COUNT]
 */
typedef struct {
    nimcp_gpu_tensor_t* activity;            /**< Nucleus activity levels */
    nimcp_gpu_tensor_t* output;              /**< Nucleus output signals */
} nimcp_hypo_gpu_nucleus_state_t;

/**
 * @brief GPU homeostatic controller state
 */
typedef struct {
    nimcp_gpu_tensor_t* error;               /**< Current error [batch, variables] */
    nimcp_gpu_tensor_t* integral;            /**< Integral accumulator */
    nimcp_gpu_tensor_t* derivative;          /**< Derivative term */
    nimcp_gpu_tensor_t* output;              /**< Controller output */
    size_t n_variables;                      /**< Number of controlled variables */
} nimcp_hypo_gpu_controller_state_t;

/*=============================================================================
 * CONFIGURATION DEFAULTS
 *===========================================================================*/

/**
 * @brief Get default dynamics configuration
 */
NIMCP_EXPORT nimcp_hypo_gpu_dynamics_config_t nimcp_hypo_gpu_dynamics_config_default(void);

/**
 * @brief Get default reward configuration
 */
NIMCP_EXPORT nimcp_hypo_gpu_reward_config_t nimcp_hypo_gpu_reward_config_default(void);

/**
 * @brief Get default PID controller configuration
 */
NIMCP_EXPORT nimcp_hypo_gpu_pid_config_t nimcp_hypo_gpu_pid_config_default(void);

/*=============================================================================
 * STATE LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create GPU drive state
 *
 * @param ctx GPU context
 * @param batch_size Batch size for parallel processing
 * @return Drive state handle or NULL on failure
 */
NIMCP_EXPORT nimcp_hypo_gpu_drive_state_t* nimcp_hypo_gpu_drive_state_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size);

/**
 * @brief Destroy GPU drive state
 */
NIMCP_EXPORT void nimcp_hypo_gpu_drive_state_destroy(
    nimcp_hypo_gpu_drive_state_t* state);

/**
 * @brief Create GPU setpoints
 *
 * @param ctx GPU context
 * @param batch_size Batch size
 * @param alignment_locked Whether alignment weights are locked
 * @return Setpoints handle or NULL
 */
NIMCP_EXPORT nimcp_hypo_gpu_setpoints_t* nimcp_hypo_gpu_setpoints_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    bool alignment_locked);

/**
 * @brief Destroy GPU setpoints
 */
NIMCP_EXPORT void nimcp_hypo_gpu_setpoints_destroy(
    nimcp_hypo_gpu_setpoints_t* setpoints);

/**
 * @brief Create GPU reward output
 */
NIMCP_EXPORT nimcp_hypo_gpu_reward_output_t* nimcp_hypo_gpu_reward_output_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size);

/**
 * @brief Destroy GPU reward output
 */
NIMCP_EXPORT void nimcp_hypo_gpu_reward_output_destroy(
    nimcp_hypo_gpu_reward_output_t* output);

/**
 * @brief Create GPU controller state
 */
NIMCP_EXPORT nimcp_hypo_gpu_controller_state_t* nimcp_hypo_gpu_controller_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t n_variables);

/**
 * @brief Destroy GPU controller state
 */
NIMCP_EXPORT void nimcp_hypo_gpu_controller_destroy(
    nimcp_hypo_gpu_controller_state_t* state);

/*=============================================================================
 * DRIVE DYNAMICS KERNELS
 *===========================================================================*/

/**
 * @brief Integrate drive state forward in time
 *
 * GPU-accelerated ODE integration for all drives in parallel.
 * Supports Euler, RK2, RK4, and adaptive methods.
 *
 * @param ctx GPU context
 * @param state Drive state to update (in-place)
 * @param dt Time step in seconds
 * @param config Integration configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_drive_integrate(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    float dt,
    const nimcp_hypo_gpu_dynamics_config_t* config);

/**
 * @brief Compute drive urgency from current levels
 *
 * Maps drive levels to urgency values considering:
 * - Deviation from setpoint
 * - Time since last satisfaction
 * - Global arousal modulation
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param setpoints Setpoint configuration
 * @param arousal Global arousal level [0, 1]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_compute_urgency(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    float arousal);

/**
 * @brief Apply drive satisfaction
 *
 * Updates drive state based on satisfaction event.
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param drive_mask Mask of which drives are satisfied [batch, drives]
 * @param satisfaction_levels How well satisfied [batch, drives]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_satisfy_drives(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_gpu_tensor_t* drive_mask,
    const nimcp_gpu_tensor_t* satisfaction_levels);

/**
 * @brief Compute setpoint deviation
 *
 * Batch computation of deviation from setpoints.
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param setpoints Setpoint configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_compute_deviation(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints);

/*=============================================================================
 * REWARD COMPUTATION KERNELS
 *===========================================================================*/

/**
 * @brief Compute reward signal from drive state
 *
 * GPU-accelerated reward computation with alignment weights.
 *
 * @param ctx GPU context
 * @param state Current drive state
 * @param setpoints Setpoint configuration (includes alignment weights)
 * @param output Reward output tensors
 * @param config Reward computation configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_compute_reward(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_hypo_gpu_reward_output_t* output,
    const nimcp_hypo_gpu_reward_config_t* config);

/**
 * @brief Compute reward prediction error
 *
 * RPE = actual_reward - expected_reward
 *
 * @param ctx GPU context
 * @param actual Actual reward received
 * @param expected Expected reward (from value function)
 * @param rpe_out Output RPE tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_compute_rpe(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* expected,
    nimcp_gpu_tensor_t* rpe_out);

/**
 * @brief Compute alignment score
 *
 * Evaluates how well current behavior aligns with alignment weights.
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param setpoints Setpoints with alignment weights
 * @param score_out Output alignment score [batch]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_compute_alignment(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_gpu_tensor_t* score_out);

/*=============================================================================
 * HOMEOSTATIC CONTROLLER KERNELS
 *===========================================================================*/

/**
 * @brief Update PI/PD homeostatic controller
 *
 * GPU-accelerated PID control for multiple variables.
 *
 * @param ctx GPU context
 * @param controller Controller state
 * @param current Current values
 * @param target Target setpoints
 * @param dt Time step
 * @param config PID configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_controller_update(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_controller_state_t* controller,
    const nimcp_gpu_tensor_t* current,
    const nimcp_gpu_tensor_t* target,
    float dt,
    const nimcp_hypo_gpu_pid_config_t* config);

/**
 * @brief Reset controller integral term
 *
 * Anti-windup reset when setpoint changes.
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_controller_reset_integral(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_controller_state_t* controller);

/*=============================================================================
 * PRIORITY AND URGENCY KERNELS
 *===========================================================================*/

/**
 * @brief Find highest priority drive
 *
 * GPU-accelerated argmax over urgency values.
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param priority_out Output priority indices [batch]
 * @param max_urgency_out Output max urgency values [batch]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_find_priority(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    nimcp_gpu_tensor_t* priority_out,
    nimcp_gpu_tensor_t* max_urgency_out);

/**
 * @brief Sort drives by urgency
 *
 * GPU-accelerated sorting for priority ordering.
 *
 * @param ctx GPU context
 * @param urgencies Input urgency values [batch, drives]
 * @param sorted_indices Output sorted indices [batch, drives]
 * @param sorted_values Output sorted values [batch, drives]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_sort_urgency(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* urgencies,
    nimcp_gpu_tensor_t* sorted_indices,
    nimcp_gpu_tensor_t* sorted_values);

/*=============================================================================
 * NUCLEUS ACTIVITY KERNELS
 *===========================================================================*/

/**
 * @brief Update nucleus activity from drive state
 *
 * Maps drive levels to nucleus activity patterns.
 *
 * @param ctx GPU context
 * @param state Drive state
 * @param nucleus Nucleus state to update
 * @param drive_nucleus_weights Weight matrix [drives, nuclei]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_update_nuclei(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    nimcp_hypo_gpu_nucleus_state_t* nucleus,
    const nimcp_gpu_tensor_t* drive_nucleus_weights);

/**
 * @brief Compute nucleus output signals
 *
 * Applies activation function to nucleus activity.
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_nucleus_output(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_nucleus_state_t* nucleus);

/*=============================================================================
 * DATA TRANSFER UTILITIES
 *===========================================================================*/

/**
 * @brief Upload drive state from CPU
 *
 * @param ctx GPU context
 * @param gpu_state GPU state to update
 * @param cpu_levels CPU drive levels [batch, drives]
 * @param batch_size Number of items in batch
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_upload_state(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* gpu_state,
    const float* cpu_levels,
    size_t batch_size);

/**
 * @brief Download drive state to CPU
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_download_state(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* gpu_state,
    float* cpu_levels,
    size_t batch_size);

/**
 * @brief Upload setpoints from CPU
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_upload_setpoints(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_setpoints_t* gpu_setpoints,
    const float* cpu_setpoints,
    const float* cpu_alignment_weights,
    size_t batch_size);

/**
 * @brief Download reward output to CPU
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_download_reward(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_reward_output_t* gpu_output,
    float* cpu_rewards,
    size_t batch_size);

/*=============================================================================
 * PLATFORM DETECTION
 *===========================================================================*/

/**
 * @brief Check if GPU acceleration is available
 *
 * @return true if CUDA/ROCm available and platform tier supports GPU
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_available(void);

/**
 * @brief Get GPU compute capability
 *
 * @param major Output major version
 * @param minor Output minor version
 * @return true if GPU available
 */
NIMCP_EXPORT bool nimcp_hypo_gpu_capability(int* major, int* minor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_GPU_H */
