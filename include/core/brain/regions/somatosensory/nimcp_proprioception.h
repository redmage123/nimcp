/**
 * @file nimcp_proprioception.h
 * @brief Proprioceptive Processing for Body Position Sensing
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * This module implements proprioceptive processing:
 * - Muscle spindle signal processing (length, velocity)
 * - Golgi tendon organ processing (tension)
 * - Joint receptor processing (angle, position)
 * - Body schema estimation
 * - Movement prediction and error detection
 *
 * Processed primarily in:
 * - Area 3a (primary proprioceptive cortex)
 * - Posterior parietal cortex (body schema)
 * - Cerebellum (coordination)
 */

#ifndef NIMCP_PROPRIOCEPTION_H
#define NIMCP_PROPRIOCEPTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PROPRIO_MAX_JOINTS          64
#define PROPRIO_MAX_MUSCLES         128
#define PROPRIO_SPINDLE_GAIN        1.0f
#define PROPRIO_GTO_GAIN            0.5f
#define PROPRIO_JOINT_RESOLUTION    0.5f    /* Degrees */
#define PROPRIO_UPDATE_RATE_HZ      100.0f
#define PROPRIO_PREDICTION_HORIZON  50.0f   /* ms */
#define PROPRIO_KALMAN_Q            0.01f   /* Process noise */
#define PROPRIO_KALMAN_R            0.1f    /* Measurement noise */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Joint types
 */
typedef enum {
    JOINT_HINGE = 0,        /* Single axis rotation (elbow) */
    JOINT_PIVOT,            /* Rotation around axis (radius) */
    JOINT_BALL_SOCKET,      /* Multi-axis rotation (shoulder, hip) */
    JOINT_SADDLE,           /* Two-axis movement (thumb) */
    JOINT_GLIDING,          /* Sliding motion (wrist bones) */
    JOINT_FIXED             /* No movement (skull sutures) */
} joint_type_t;

/**
 * @brief Muscle spindle fiber types
 */
typedef enum {
    SPINDLE_IA = 0,         /* Primary afferent - velocity + length */
    SPINDLE_II              /* Secondary afferent - length only */
} spindle_type_t;

/**
 * @brief Movement state
 */
typedef enum {
    MOVEMENT_STATIC = 0,
    MOVEMENT_ACCELERATING,
    MOVEMENT_CONSTANT_VELOCITY,
    MOVEMENT_DECELERATING,
    MOVEMENT_OSCILLATING
} movement_state_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Proprioception configuration
 */
typedef struct {
    float spindle_gain;
    float gto_gain;
    float joint_receptor_gain;
    float prediction_weight;
    float kalman_process_noise;
    float kalman_measurement_noise;
    bool enable_prediction;
    bool enable_cerebellar_integration;
    float update_rate_hz;
} proprio_config_t;

/**
 * @brief Muscle spindle state
 */
typedef struct {
    uint32_t spindle_id;
    body_segment_t segment;
    spindle_type_t type;

    /* Physical state */
    float muscle_length;        /* Current length (normalized) */
    float muscle_velocity;      /* Lengthening velocity */
    float intrafusal_tension;   /* Gamma motor neuron setting */

    /* Output */
    float ia_firing_rate;       /* Primary afferent */
    float ii_firing_rate;       /* Secondary afferent */

    /* Adaptation */
    float adaptation_state;
    float baseline_firing;

    /* SNN integration */
    uint32_t snn_neuron_id;
} muscle_spindle_t;

/**
 * @brief Golgi tendon organ state
 */
typedef struct {
    uint32_t gto_id;
    body_segment_t segment;

    /* Physical state */
    float muscle_tension;       /* Current tension */
    float tension_threshold;    /* Activation threshold */

    /* Output */
    float ib_firing_rate;       /* Ib afferent firing rate */

    /* Protective reflex threshold */
    float protective_threshold;
    bool protective_active;

    /* SNN integration */
    uint32_t snn_neuron_id;
} golgi_tendon_organ_t;

/**
 * @brief Joint receptor state
 */
typedef struct {
    uint32_t receptor_id;
    body_segment_t segment;
    joint_type_t joint_type;

    /* Position sensing */
    float joint_angle[3];       /* Angles (up to 3 DOF) */
    float angular_velocity[3];
    float angular_range[2];     /* Min/max range per axis */

    /* Receptor properties */
    float position_threshold;
    float velocity_threshold;
    bool at_limit;              /* Near joint limit */

    /* Output */
    float firing_rate;
    float limit_warning;

    /* SNN integration */
    uint32_t snn_neuron_id;
} joint_receptor_t;

/**
 * @brief Body segment state estimation
 */
typedef struct {
    body_segment_t segment;

    /* Position/velocity state */
    float position[3];          /* Estimated position */
    float velocity[3];          /* Estimated velocity */
    float acceleration[3];      /* Estimated acceleration */
    float orientation[4];       /* Quaternion orientation */
    float angular_velocity[3];

    /* Kalman filter state */
    float* state_estimate;      /* Full state vector */
    float* covariance;          /* Estimation covariance */
    uint32_t state_dim;

    /* Confidence */
    float position_confidence;
    float velocity_confidence;

    /* Prediction */
    float* predicted_state;
    float prediction_error;

    /* Movement classification */
    movement_state_t movement_state;
    float movement_magnitude;

    /* Timing */
    uint64_t last_update;
    float update_dt;
} segment_state_t;

/**
 * @brief Full body state estimate
 */
typedef struct {
    segment_state_t segments[BODY_SEG_COUNT];

    /* Global body state */
    float center_of_mass[3];
    float total_momentum[3];
    float balance_state;        /* 0.0 = falling, 1.0 = stable */

    /* Posture */
    float* posture_vector;
    uint32_t posture_dim;
    float posture_stability;

    /* Reference frames */
    float world_to_body[16];    /* 4x4 transform matrix */
    float body_to_world[16];
} body_state_t;

/**
 * @brief Proprioceptive processing context
 */
typedef struct {
    proprio_config_t config;

    /* Receptors */
    muscle_spindle_t* spindles;
    uint32_t num_spindles;
    golgi_tendon_organ_t* gtos;
    uint32_t num_gtos;
    joint_receptor_t* joint_receptors;
    uint32_t num_joint_receptors;

    /* State estimation */
    body_state_t body_state;

    /* Efference copy */
    float* expected_state;
    float* motor_command;
    uint32_t command_dim;

    /* Forward model */
    void* forward_model;        /* Internal model for prediction */
    float forward_model_error;

    /* Statistics */
    uint32_t updates_processed;
    float avg_prediction_error;
    float avg_estimation_confidence;
} proprio_ctx_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default proprioception configuration
 */
proprio_config_t proprio_default_config(void);

/**
 * @brief Create proprioception context
 */
proprio_ctx_t* proprio_create(const proprio_config_t* config);

/**
 * @brief Destroy proprioception context
 */
void proprio_destroy(proprio_ctx_t* ctx);

/**
 * @brief Reset proprioception context
 */
int proprio_reset(proprio_ctx_t* ctx);

/**
 * @brief Initialize receptors for body segment
 */
int proprio_init_segment(proprio_ctx_t* ctx,
                         body_segment_t segment,
                         uint32_t num_spindles,
                         uint32_t num_gtos,
                         joint_type_t joint_type);

/*=============================================================================
 * MUSCLE SPINDLE PROCESSING
 *===========================================================================*/

/**
 * @brief Update muscle spindle state
 */
int proprio_update_spindle(proprio_ctx_t* ctx,
                           uint32_t spindle_id,
                           float muscle_length,
                           float muscle_velocity);

/**
 * @brief Set gamma motor neuron activation (fusimotor)
 */
int proprio_set_gamma_activation(proprio_ctx_t* ctx,
                                 uint32_t spindle_id,
                                 float gamma_static,
                                 float gamma_dynamic);

/**
 * @brief Get spindle firing rates
 */
int proprio_get_spindle_output(proprio_ctx_t* ctx,
                               uint32_t spindle_id,
                               float* ia_rate,
                               float* ii_rate);

/**
 * @brief Get all spindle outputs for segment
 */
int proprio_get_segment_spindle_output(proprio_ctx_t* ctx,
                                       body_segment_t segment,
                                       float* ia_rates,
                                       float* ii_rates,
                                       uint32_t max_outputs,
                                       uint32_t* num_outputs);

/*=============================================================================
 * GOLGI TENDON ORGAN PROCESSING
 *===========================================================================*/

/**
 * @brief Update GTO state
 */
int proprio_update_gto(proprio_ctx_t* ctx,
                       uint32_t gto_id,
                       float muscle_tension);

/**
 * @brief Get GTO output
 */
float proprio_get_gto_output(proprio_ctx_t* ctx, uint32_t gto_id);

/**
 * @brief Check if protective reflex should trigger
 */
bool proprio_check_protective_reflex(proprio_ctx_t* ctx,
                                     body_segment_t segment);

/**
 * @brief Get total muscle tension for segment
 */
float proprio_get_segment_tension(proprio_ctx_t* ctx, body_segment_t segment);

/*=============================================================================
 * JOINT RECEPTOR PROCESSING
 *===========================================================================*/

/**
 * @brief Update joint receptor state
 */
int proprio_update_joint(proprio_ctx_t* ctx,
                         uint32_t receptor_id,
                         const float* joint_angles,
                         uint32_t num_angles);

/**
 * @brief Get joint angle estimate
 */
int proprio_get_joint_angle(proprio_ctx_t* ctx,
                            body_segment_t segment,
                            float* angles,
                            uint32_t max_angles,
                            uint32_t* num_angles);

/**
 * @brief Check if joint is near limit
 */
bool proprio_joint_at_limit(proprio_ctx_t* ctx, body_segment_t segment);

/**
 * @brief Get joint range of motion
 */
int proprio_get_joint_range(proprio_ctx_t* ctx,
                            body_segment_t segment,
                            float* min_angles,
                            float* max_angles,
                            uint32_t max_axes);

/*=============================================================================
 * STATE ESTIMATION
 *===========================================================================*/

/**
 * @brief Update segment state estimate
 */
int proprio_update_segment_state(proprio_ctx_t* ctx,
                                 body_segment_t segment,
                                 float dt);

/**
 * @brief Get segment state
 */
int proprio_get_segment_state(proprio_ctx_t* ctx,
                              body_segment_t segment,
                              segment_state_t* state);

/**
 * @brief Get position estimate
 */
int proprio_get_position(proprio_ctx_t* ctx,
                         body_segment_t segment,
                         float* position);

/**
 * @brief Get velocity estimate
 */
int proprio_get_velocity(proprio_ctx_t* ctx,
                         body_segment_t segment,
                         float* velocity);

/**
 * @brief Get orientation estimate
 */
int proprio_get_orientation(proprio_ctx_t* ctx,
                            body_segment_t segment,
                            float* quaternion);

/**
 * @brief Get full body state
 */
int proprio_get_body_state(proprio_ctx_t* ctx, body_state_t* state);

/**
 * @brief Estimate center of mass
 */
int proprio_estimate_com(proprio_ctx_t* ctx, float* com);

/**
 * @brief Get posture vector
 */
int proprio_get_posture(proprio_ctx_t* ctx,
                        float* posture,
                        uint32_t max_dim,
                        uint32_t* actual_dim);

/*=============================================================================
 * PREDICTION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Set efference copy (motor command)
 */
int proprio_set_efference_copy(proprio_ctx_t* ctx,
                               body_segment_t segment,
                               const float* motor_command,
                               uint32_t command_dim);

/**
 * @brief Predict future state
 */
int proprio_predict_state(proprio_ctx_t* ctx,
                          body_segment_t segment,
                          float dt_future,
                          segment_state_t* predicted);

/**
 * @brief Compute prediction error
 */
float proprio_compute_prediction_error(proprio_ctx_t* ctx,
                                       body_segment_t segment);

/**
 * @brief Update forward model with prediction error
 */
int proprio_update_forward_model(proprio_ctx_t* ctx,
                                 body_segment_t segment,
                                 float prediction_error,
                                 float learning_rate);

/*=============================================================================
 * MOVEMENT ANALYSIS
 *===========================================================================*/

/**
 * @brief Classify movement state
 */
movement_state_t proprio_classify_movement(proprio_ctx_t* ctx,
                                           body_segment_t segment);

/**
 * @brief Detect movement onset
 */
bool proprio_detect_movement_onset(proprio_ctx_t* ctx,
                                   body_segment_t segment,
                                   float threshold);

/**
 * @brief Detect movement termination
 */
bool proprio_detect_movement_end(proprio_ctx_t* ctx,
                                 body_segment_t segment,
                                 float threshold);

/**
 * @brief Get movement trajectory
 */
int proprio_get_trajectory(proprio_ctx_t* ctx,
                           body_segment_t segment,
                           float* trajectory,
                           uint32_t max_points,
                           uint32_t* num_points);

/*=============================================================================
 * BALANCE AND STABILITY
 *===========================================================================*/

/**
 * @brief Estimate balance state
 */
float proprio_estimate_balance(proprio_ctx_t* ctx);

/**
 * @brief Check if falling
 */
bool proprio_is_falling(proprio_ctx_t* ctx, float threshold);

/**
 * @brief Get stability margin
 */
float proprio_get_stability_margin(proprio_ctx_t* ctx);

/**
 * @brief Get support polygon (feet positions)
 */
int proprio_get_support_polygon(proprio_ctx_t* ctx,
                                float* polygon,
                                uint32_t max_vertices,
                                uint32_t* num_vertices);

/*=============================================================================
 * UPDATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update all proprioception (call each timestep)
 */
int proprio_update(proprio_ctx_t* ctx, float dt);

/**
 * @brief Get proprioceptive output for downstream
 */
int proprio_get_output(proprio_ctx_t* ctx,
                       float* output,
                       uint32_t max_dim,
                       uint32_t* actual_dim);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* proprio_joint_type_name(joint_type_t type);
const char* proprio_movement_state_name(movement_state_t state);

/**
 * @brief Convert joint angles to position
 */
int proprio_forward_kinematics(const float* joint_angles,
                               uint32_t num_joints,
                               const float* link_lengths,
                               float* end_position);

/**
 * @brief Convert position to joint angles
 */
int proprio_inverse_kinematics(const float* target_position,
                               const float* link_lengths,
                               uint32_t num_joints,
                               float* joint_angles);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROPRIOCEPTION_H */
