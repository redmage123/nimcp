/**
 * @file nimcp_soma_cerebellum_bridge.h
 * @brief Somatosensory-Cerebellum Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge connecting somatosensory cortex to cerebellum for motor
 *       coordination, proprioceptive integration, and sensorimotor learning.
 *
 * WHY: The cerebellum needs proprioceptive and tactile feedback for:
 *      - Fine motor control and coordination
 *      - Balance and posture maintenance
 *      - Motor learning and adaptation
 *      - Error correction during movement
 *
 * HOW: Routes proprioceptive signals, touch feedback during movement, and
 *      motor efference copies between somatosensory cortex and cerebellum.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Spinocerebellar tracts carry proprioceptive info to cerebellum
 * - Cerebellum compares expected vs actual sensory feedback
 * - Climbing fibers signal sensory prediction errors
 * - Purkinje cells integrate tactile/proprioceptive signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SOMA_CEREBELLUM_BRIDGE_H
#define NIMCP_SOMA_CEREBELLUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SOMA_CEREB_MAX_JOINTS           32
#define SOMA_CEREB_MAX_EFFECTORS        16
#define SOMA_CEREB_PREDICTION_HORIZON   100  /* ms */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    SOMA_CEREB_MSG_PROPRIO_UPDATE = 0,   /**< Proprioceptive update */
    SOMA_CEREB_MSG_TOUCH_DURING_MOVE,    /**< Touch during movement */
    SOMA_CEREB_MSG_PREDICTION_ERROR,     /**< Sensory prediction error */
    SOMA_CEREB_MSG_EFFERENCE_COPY,       /**< Motor efference copy */
    SOMA_CEREB_MSG_BALANCE_UPDATE,       /**< Balance/vestibular update */
    SOMA_CEREB_MSG_COORDINATION_REQ,     /**< Coordination request */
    SOMA_CEREB_MSG_COUNT
} soma_cereb_msg_type_t;

typedef enum {
    SOMA_CEREB_STATUS_IDLE = 0,
    SOMA_CEREB_STATUS_COORDINATING,
    SOMA_CEREB_STATUS_ADAPTING,
    SOMA_CEREB_STATUS_ERROR
} soma_cereb_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Proprioceptive state for a joint
 */
typedef struct {
    uint32_t joint_id;
    float angle;                /**< Joint angle (radians) */
    float angular_velocity;     /**< Angular velocity */
    float torque;               /**< Applied torque */
    float stretch;              /**< Muscle stretch */
    float tension;              /**< Muscle tension */
} soma_cereb_joint_state_t;

/**
 * @brief Sensory prediction error signal
 */
typedef struct {
    float* predicted;           /**< Predicted sensory state */
    float* actual;              /**< Actual sensory state */
    float* error;               /**< Prediction error */
    uint32_t dim;               /**< Dimensionality */
    float error_magnitude;      /**< Overall error magnitude */
    uint64_t timestamp;         /**< Error timestamp */
} soma_cereb_prediction_error_t;

/**
 * @brief Motor efference copy
 */
typedef struct {
    uint32_t* effector_ids;     /**< Effector IDs */
    float* commands;            /**< Motor commands */
    uint32_t num_effectors;     /**< Number of effectors */
    float* predicted_sensory;   /**< Predicted sensory consequence */
    uint32_t prediction_dim;    /**< Prediction dimensionality */
    uint64_t command_time;      /**< Command timestamp */
} soma_cereb_efference_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t max_joints;
    uint32_t max_effectors;
    uint32_t prediction_horizon_ms;
    float error_gain;
    float adaptation_rate;
    bool enable_prediction;
    bool enable_adaptation;
    bool enable_logging;
} soma_cereb_config_t;

typedef struct {
    uint64_t proprio_updates;
    uint64_t touch_events;
    uint64_t prediction_errors;
    uint64_t efference_copies;
    float avg_prediction_error;
    float adaptation_progress;
} soma_cereb_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct soma_cereb_bridge_struct soma_cereb_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int soma_cereb_default_config(soma_cereb_config_t* config);
soma_cereb_bridge_t* soma_cereb_bridge_create(const soma_cereb_config_t* config);
void soma_cereb_bridge_destroy(soma_cereb_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int soma_cereb_connect(soma_cereb_bridge_t* bridge, nimcp_somatosensory_t* soma, void* cerebellum);
int soma_cereb_disconnect(soma_cereb_bridge_t* bridge);
bool soma_cereb_is_connected(const soma_cereb_bridge_t* bridge);

/* ============================================================================
 * Proprioceptive API
 * ============================================================================ */

int soma_cereb_send_proprio(soma_cereb_bridge_t* bridge, const soma_cereb_joint_state_t* joints, uint32_t num_joints);
int soma_cereb_send_touch_during_move(soma_cereb_bridge_t* bridge, body_segment_t region, float intensity);
int soma_cereb_send_balance_update(soma_cereb_bridge_t* bridge, float roll, float pitch, float yaw);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int soma_cereb_send_prediction_error(soma_cereb_bridge_t* bridge, const soma_cereb_prediction_error_t* error);
int soma_cereb_receive_efference(soma_cereb_bridge_t* bridge, soma_cereb_efference_t* efference);
int soma_cereb_compute_prediction(soma_cereb_bridge_t* bridge, const soma_cereb_efference_t* efference, float* prediction);

/* ============================================================================
 * Coordination API
 * ============================================================================ */

int soma_cereb_request_coordination(soma_cereb_bridge_t* bridge, uint32_t task_id);
int soma_cereb_report_movement_complete(soma_cereb_bridge_t* bridge, uint32_t task_id, bool success);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int soma_cereb_get_stats(const soma_cereb_bridge_t* bridge, soma_cereb_stats_t* stats);
int soma_cereb_reset_stats(soma_cereb_bridge_t* bridge);
void soma_cereb_print_summary(const soma_cereb_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMA_CEREBELLUM_BRIDGE_H */
