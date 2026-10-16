/**
 * @file nimcp_red_nucleus.h
 * @brief Red Nucleus - Motor Coordination and Motor Learning Center
 *
 * WHAT: Neural substrate for motor coordination, motor learning,
 *       rubrospinal motor control, and cerebellar integration
 * WHY:  Critical for forelimb movement control, motor error processing,
 *       and skilled movement facilitation
 * HOW:  Implements rubrospinal tract output, olivocerebellar loop integration,
 *       motor command processing, and error-based motor learning
 *
 * BIOLOGICAL BASIS:
 * - Receives inputs from motor cortex (corticorubral fibers)
 * - Receives inputs from cerebellum via dentate nucleus (dentato-rubral)
 * - Outputs to spinal cord via rubrospinal tract (mainly forelimb)
 * - Outputs to inferior olive for cerebellar error learning
 * - Part of dentato-rubro-thalamic pathway
 * - Critical for posture, balance, and motor adaptation
 *
 * SUBDIVISIONS IMPLEMENTED:
 * - Magnocellular (RNm): Large neurons, rubrospinal tract origin
 *   - Controls forelimb movements, especially distal
 *   - More developed in non-primates
 * - Parvocellular (RNp): Small neurons, receives dentate input
 *   - Projects to inferior olive (rubro-olivary)
 *   - Critical for motor learning pathway
 *
 * MOTOR FUNCTIONS:
 * - Velocity control: Movement speed modulation
 * - Force control: Muscle force generation
 * - Position control: Limb positioning
 * - Trajectory planning: Movement path computation
 * - Motor error processing: Error-based adaptation
 * - Motor learning: Skill acquisition and refinement
 *
 * FULL INTEGRATION WITH:
 * - Security module (BBB registration), Immune system
 * - KG wiring (node registration, state updates, queries)
 * - Bio-async (message types, subscription masks)
 * - Brain initialization, SNN/STDP/plasticity modules
 * - Hypothalamus, Omnidirectional module, Cognitive/Training layers
 * - NIMCP utilities, Math utilities, Threading (mutex)
 * - Quantum algorithms (QMC, QMCTS), Perception layer
 * - Symbolic logic, Swarm, Dragonfly, Portia
 * - Logging, Thalamic layer, Neural substrate layer
 * - Cerebellum (CRITICAL for olivocerebellar loop)
 *
 * @version 1.0
 * @date 2026-01-13
 */

#ifndef NIMCP_RED_NUCLEUS_H
#define NIMCP_RED_NUCLEUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Platform and utilities */
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

/* Forward declarations for integration */
struct nimcp_brain_kg;
struct nimcp_bio_router;
struct nimcp_immune_system;
struct nimcp_security_context;
struct nimcp_snn_network;
struct nimcp_plasticity_engine;
struct nimcp_hypothalamus;
struct nimcp_thalamus;
struct nimcp_cognitive_hub;
struct nimcp_training_context;
struct nimcp_perception_system;
struct nimcp_symbolic_engine;
struct nimcp_swarm_context;
struct nimcp_dragonfly_context;
struct nimcp_portia_context;
struct nimcp_qmc_context;
struct nimcp_omni_predictor;
struct nimcp_neural_substrate;
struct cerebellum_adapter;

/*=============================================================================
 * RED NUCLEUS SUBDIVISIONS
 *===========================================================================*/

/**
 * @brief Red Nucleus subdivisions with distinct functional roles
 *
 * BIOLOGICAL BASIS:
 * - Magnocellular: Large neurons projecting to spinal cord
 * - Parvocellular: Small neurons projecting to inferior olive
 */
typedef enum {
    RN_SUBDIV_MAGNOCELLULAR = 0,  /**< Rubrospinal tract, forelimb control */
    RN_SUBDIV_PARVOCELLULAR,      /**< Rubro-olivary, motor learning */
    RN_SUBDIV_COUNT
} rn_subdivision_t;

/*=============================================================================
 * MOTOR COMMAND TYPES
 *===========================================================================*/

/**
 * @brief Motor command types processed by Red Nucleus
 */
typedef enum {
    RN_CMD_VELOCITY = 0,      /**< Movement velocity command */
    RN_CMD_FORCE,             /**< Force generation command */
    RN_CMD_POSITION,          /**< Target position command */
    RN_CMD_TRAJECTORY,        /**< Trajectory specification */
    RN_CMD_POSTURE,           /**< Postural adjustment */
    RN_CMD_BALANCE,           /**< Balance correction */
    RN_CMD_SKILLED,           /**< Skilled movement sequence */
    RN_CMD_COUNT
} rn_motor_cmd_type_t;

/**
 * @brief Motor effector targets
 */
typedef enum {
    RN_EFFECTOR_FORELIMB_PROXIMAL = 0,  /**< Shoulder/upper arm */
    RN_EFFECTOR_FORELIMB_DISTAL,        /**< Hand/fingers */
    RN_EFFECTOR_HINDLIMB_PROXIMAL,      /**< Hip/thigh */
    RN_EFFECTOR_HINDLIMB_DISTAL,        /**< Foot/toes */
    RN_EFFECTOR_AXIAL,                  /**< Trunk/core */
    RN_EFFECTOR_COUNT
} rn_effector_t;

/*=============================================================================
 * MOTOR COMMAND STRUCTURES
 *===========================================================================*/

/**
 * @brief 3D vector for motor commands
 */
typedef struct {
    float x;    /**< X component */
    float y;    /**< Y component */
    float z;    /**< Z component */
} rn_vector3_t;

/**
 * @brief Motor command structure
 */
typedef struct {
    rn_motor_cmd_type_t type;     /**< Command type */
    rn_effector_t effector;       /**< Target effector */
    rn_vector3_t value;           /**< Command value (type-dependent) */
    float magnitude;              /**< Command magnitude [0, 1] */
    float urgency;                /**< Command urgency [0, 1] */
    float duration_ms;            /**< Commanded duration */
    uint64_t timestamp_us;        /**< When issued */
    uint32_t sequence_id;         /**< For multi-command sequences */
} rn_motor_command_t;

/**
 * @brief Trajectory point for path planning
 */
typedef struct {
    rn_vector3_t position;        /**< Target position */
    rn_vector3_t velocity;        /**< Target velocity at point */
    float time_ms;                /**< Time to reach point */
} rn_trajectory_point_t;

/**
 * @brief Complete trajectory specification
 */
typedef struct {
    rn_trajectory_point_t* points;  /**< Array of waypoints */
    uint32_t num_points;            /**< Number of waypoints */
    rn_effector_t effector;         /**< Target effector */
    float total_duration_ms;        /**< Total trajectory duration */
    bool smooth_interpolation;      /**< Use smooth interpolation */
} rn_trajectory_t;

/*=============================================================================
 * MOTOR LEARNING
 *===========================================================================*/

/**
 * @brief Motor error types
 */
typedef enum {
    RN_ERROR_POSITION = 0,    /**< Position error */
    RN_ERROR_VELOCITY,        /**< Velocity error */
    RN_ERROR_FORCE,           /**< Force error */
    RN_ERROR_TIMING,          /**< Timing error */
    RN_ERROR_TRAJECTORY,      /**< Trajectory deviation */
    RN_ERROR_COUNT
} rn_error_type_t;

/**
 * @brief Motor error signal
 */
typedef struct {
    rn_error_type_t type;         /**< Error type */
    rn_effector_t effector;       /**< Affected effector */
    float error_magnitude;        /**< Error magnitude [-1, 1] */
    rn_vector3_t error_vector;    /**< Directional error */
    uint64_t timestamp_us;        /**< When error detected */
    uint32_t command_id;          /**< Related command ID */
} rn_motor_error_t;

/**
 * @brief Motor learning state
 */
typedef struct {
    /* Error history (circular buffer) */
    float error_history[32];      /**< Recent error magnitudes */
    uint32_t error_index;         /**< Current index */
    uint32_t error_count;         /**< Total errors processed */

    /* Adaptation state */
    float adaptation_gain;        /**< Current adaptation gain */
    float learning_rate;          /**< Current learning rate */
    float error_integral;         /**< Integrated error for I-control */
    float error_derivative;       /**< Error derivative for D-control */

    /* Performance metrics */
    float avg_error;              /**< Average recent error */
    float error_reduction;        /**< Error reduction since start */
    float skill_level;            /**< Acquired skill level [0, 1] */
    uint64_t training_iterations; /**< Total training iterations */
} rn_learning_state_t;

/*=============================================================================
 * CEREBELLAR INTEGRATION
 *===========================================================================*/

/**
 * @brief Dentato-rubro signal (from cerebellar dentate nucleus)
 */
typedef struct {
    float activity;               /**< Dentate output activity */
    float motor_correction[8];    /**< Motor correction vector */
    uint32_t num_corrections;     /**< Number of correction dimensions */
    float timing_adjustment;      /**< Timing correction */
    uint64_t timestamp_us;        /**< Signal timestamp */
} rn_dentate_signal_t;

/**
 * @brief Rubro-olivary output (to inferior olive)
 */
typedef struct {
    float error_signal;           /**< Error for olive */
    rn_error_type_t error_type;   /**< Type of error */
    rn_effector_t effector;       /**< Related effector */
    float learning_request;       /**< Learning rate request */
    uint64_t timestamp_us;        /**< Signal timestamp */
} rn_olivary_output_t;

/**
 * @brief Thalamic projection (dentato-rubro-thalamic)
 */
typedef struct {
    float activity;               /**< Thalamic projection strength */
    float motor_readiness;        /**< Motor preparation signal */
    float movement_intention;     /**< Movement intention signal */
    uint64_t timestamp_us;        /**< Signal timestamp */
} rn_thalamic_output_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Red Nucleus bio-async message types
 */
typedef enum {
    RN_BIO_MSG_MOTOR_CMD = 0,       /**< Motor command issued */
    RN_BIO_MSG_ERROR_SIGNAL,        /**< Motor error detected */
    RN_BIO_MSG_LEARNING_UPDATE,     /**< Learning state changed */
    RN_BIO_MSG_CEREBELLAR_INPUT,    /**< Cerebellar signal received */
    RN_BIO_MSG_OLIVARY_OUTPUT,      /**< Olivary output sent */
    RN_BIO_MSG_THALAMIC_OUTPUT,     /**< Thalamic projection */
    RN_BIO_MSG_POSTURE_ADJUST,      /**< Postural adjustment */
    RN_BIO_MSG_STATE_REQUEST,       /**< Request for RN state */
    RN_BIO_MSG_COUNT
} rn_bio_msg_type_t;

/**
 * @brief Subscription masks for bio-async
 */
#define RN_BIO_SUB_MOTOR_CMD      (1U << RN_BIO_MSG_MOTOR_CMD)
#define RN_BIO_SUB_ERROR          (1U << RN_BIO_MSG_ERROR_SIGNAL)
#define RN_BIO_SUB_LEARNING       (1U << RN_BIO_MSG_LEARNING_UPDATE)
#define RN_BIO_SUB_CEREBELLAR     (1U << RN_BIO_MSG_CEREBELLAR_INPUT)
#define RN_BIO_SUB_OLIVARY        (1U << RN_BIO_MSG_OLIVARY_OUTPUT)
#define RN_BIO_SUB_THALAMIC       (1U << RN_BIO_MSG_THALAMIC_OUTPUT)
#define RN_BIO_SUB_POSTURE        (1U << RN_BIO_MSG_POSTURE_ADJUST)
#define RN_BIO_SUB_ALL            (0xFFFFFFFFU)

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

/**
 * @brief KG node types for Red Nucleus
 */
typedef enum {
    RN_KG_NODE_REGION = 0,        /**< Red Nucleus region node */
    RN_KG_NODE_SUBDIVISION,       /**< Subdivision node */
    RN_KG_NODE_MOTOR_CMD,         /**< Motor command node */
    RN_KG_NODE_ERROR_SIGNAL,      /**< Error signal node */
    RN_KG_NODE_LEARNING,          /**< Learning state node */
    RN_KG_NODE_CONNECTION         /**< Connection/edge node */
} rn_kg_node_type_t;

/**
 * @brief KG wiring state for Red Nucleus
 */
typedef struct {
    uint64_t region_node_id;      /**< Main RN node in KG */
    uint64_t subdiv_node_ids[RN_SUBDIV_COUNT];
    uint64_t cmd_node_ids[RN_CMD_COUNT];
    uint64_t effector_node_ids[RN_EFFECTOR_COUNT];
    uint32_t edge_count;          /**< Number of KG edges */
    bool registered;              /**< Registration complete */
    uint64_t admin_token;         /**< Security token for KG ops */
} rn_kg_state_t;

/*=============================================================================
 * STATISTICS AND METRICS
 *===========================================================================*/

/**
 * @brief Red Nucleus operational statistics
 */
typedef struct {
    /* Command statistics */
    uint64_t commands_issued;
    uint64_t commands_completed;
    uint64_t commands_aborted;

    /* Error statistics */
    uint64_t errors_detected;
    uint64_t errors_corrected;
    float avg_error_magnitude;
    float max_error_magnitude;

    /* Learning statistics */
    uint64_t learning_updates;
    float total_skill_improvement;
    float avg_learning_rate;

    /* Cerebellar integration */
    uint64_t dentate_signals_received;
    uint64_t olivary_outputs_sent;
    uint64_t thalamic_outputs_sent;

    /* Bio-async */
    uint64_t bio_msgs_sent;
    uint64_t bio_msgs_received;

    /* KG */
    uint64_t kg_updates;

    /* Immune */
    uint64_t immune_alerts;

    /* Timing */
    float avg_command_latency_us;
    float avg_processing_time_us;
} rn_stats_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Red Nucleus configuration
 */
typedef struct {
    /* Motor control parameters */
    float velocity_gain;            /**< Velocity command gain */
    float force_gain;               /**< Force command gain */
    float position_gain;            /**< Position command gain */
    float damping_coefficient;      /**< Movement damping */

    /* Learning parameters */
    float base_learning_rate;       /**< Base learning rate */
    float error_threshold;          /**< Error threshold for learning */
    float adaptation_rate;          /**< Adaptation rate */
    float skill_decay_rate;         /**< Skill decay when unused */
    uint32_t error_history_size;    /**< Size of error history */

    /* Cerebellar integration */
    float dentate_weight;           /**< Weight for dentate input */
    float olivary_gain;             /**< Gain for olivary output */
    float thalamic_threshold;       /**< Threshold for thalamic output */

    /* Subdivision weights */
    float magnocellular_weight;     /**< Weight for magnocellular output */
    float parvocellular_weight;     /**< Weight for parvocellular output */

    /* Integration settings */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_kg_wiring;          /**< Enable KG registration */
    bool enable_immune;             /**< Enable immune monitoring */
    bool enable_security;           /**< Enable security checks */
    bool enable_logging;            /**< Enable detailed logging */
    bool enable_quantum;            /**< Enable QMC optimization */
    bool enable_cerebellar;         /**< Enable cerebellar integration */

    /* Resource limits */
    uint32_t max_commands_queued;   /**< Max queued commands */
    uint32_t max_trajectory_points; /**< Max trajectory waypoints */
    uint32_t update_interval_ms;    /**< State update interval */

    /* Platform tier */
    platform_tier_t platform_tier;
} rn_config_t;

/*=============================================================================
 * MAIN RED NUCLEUS STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete Red Nucleus system state
 */
typedef struct nimcp_red_nucleus {
    /* Configuration */
    rn_config_t config;

    /* Subdivision states */
    struct {
        float activity[RN_SUBDIV_COUNT];
        float modulation[RN_SUBDIV_COUNT];
        bool active[RN_SUBDIV_COUNT];
    } subdivisions;

    /* Motor command state */
    rn_motor_command_t* command_queue;  /**< Queued motor commands */
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_size;
    uint32_t queue_capacity;
    rn_motor_command_t current_command;
    bool command_active;

    /* Motor output state */
    float rubrospinal_output[RN_EFFECTOR_COUNT];  /**< Per-effector output */
    float output_magnitude;                        /**< Total output */

    /* Trajectory state */
    rn_trajectory_t* current_trajectory;
    uint32_t trajectory_index;
    float trajectory_progress;            /**< Progress along trajectory [0, 1] */

    /* Learning state */
    rn_learning_state_t learning[RN_EFFECTOR_COUNT];  /**< Per-effector learning */
    float global_learning_modulation;     /**< Global learning rate modulation */

    /* Error state */
    rn_motor_error_t last_error;
    float cumulative_error;

    /* Cerebellar integration */
    rn_dentate_signal_t dentate_input;
    rn_olivary_output_t olivary_output;
    rn_thalamic_output_t thalamic_output;

    /* Cortical input */
    float cortical_input[RN_CMD_COUNT];   /**< Per-command type input */

    /* Integration handles */
    struct nimcp_brain_kg* kg;
    struct nimcp_bio_router* bio_router;
    struct nimcp_immune_system* immune;
    struct nimcp_security_context* security;
    struct nimcp_snn_network* snn;
    struct nimcp_plasticity_engine* plasticity;
    struct nimcp_hypothalamus* hypothalamus;
    struct nimcp_thalamus* thalamus;
    struct nimcp_cognitive_hub* cognitive_hub;
    struct nimcp_training_context* training;
    struct nimcp_perception_system* perception;
    struct nimcp_symbolic_engine* symbolic;
    struct nimcp_swarm_context* swarm;
    struct nimcp_dragonfly_context* dragonfly;
    struct nimcp_portia_context* portia;
    struct nimcp_qmc_context* qmc;
    struct nimcp_omni_predictor* omni;
    struct nimcp_neural_substrate* substrate;
    struct cerebellum_adapter* cerebellum;

    /* KG wiring state */
    rn_kg_state_t kg_state;

    /* Statistics */
    rn_stats_t stats;

    /* Threading */
    nimcp_mutex_t* mutex;

    /* Logging */
    nimcp_logger_t* logger;

    /* State flags */
    bool initialized;
    bool connected;
    uint64_t last_update_us;
} nimcp_red_nucleus_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default Red Nucleus configuration
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_default_config(rn_config_t* config);

/**
 * @brief Create Red Nucleus instance
 *
 * @param config Configuration (NULL for defaults)
 * @return New Red Nucleus instance, or NULL on failure
 */
NIMCP_EXPORT nimcp_red_nucleus_t* rn_create(const rn_config_t* config);

/**
 * @brief Destroy Red Nucleus instance
 *
 * @param rn Red Nucleus to destroy
 */
NIMCP_EXPORT void rn_destroy(nimcp_red_nucleus_t* rn);

/**
 * @brief Initialize Red Nucleus (post-creation setup)
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init(nimcp_red_nucleus_t* rn);

/**
 * @brief Reset Red Nucleus to initial state
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_reset(nimcp_red_nucleus_t* rn);

/*=============================================================================
 * MOTOR COMMAND API
 *===========================================================================*/

/**
 * @brief Issue motor command
 *
 * @param rn Red Nucleus instance
 * @param cmd Motor command to issue
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_issue_command(
    nimcp_red_nucleus_t* rn,
    const rn_motor_command_t* cmd);

/**
 * @brief Issue velocity command
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @param velocity Target velocity vector
 * @param duration_ms Command duration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_command_velocity(
    nimcp_red_nucleus_t* rn,
    rn_effector_t effector,
    const rn_vector3_t* velocity,
    float duration_ms);

/**
 * @brief Issue force command
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @param force Target force vector
 * @param duration_ms Command duration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_command_force(
    nimcp_red_nucleus_t* rn,
    rn_effector_t effector,
    const rn_vector3_t* force,
    float duration_ms);

/**
 * @brief Issue position command
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @param position Target position
 * @param duration_ms Time to reach position
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_command_position(
    nimcp_red_nucleus_t* rn,
    rn_effector_t effector,
    const rn_vector3_t* position,
    float duration_ms);

/**
 * @brief Issue trajectory command
 *
 * @param rn Red Nucleus instance
 * @param trajectory Trajectory specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_command_trajectory(
    nimcp_red_nucleus_t* rn,
    const rn_trajectory_t* trajectory);

/**
 * @brief Issue posture adjustment command
 *
 * @param rn Red Nucleus instance
 * @param adjustment Postural adjustment vector
 * @param urgency Command urgency [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_command_posture(
    nimcp_red_nucleus_t* rn,
    const rn_vector3_t* adjustment,
    float urgency);

/**
 * @brief Get current rubrospinal output
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @return Output level for effector
 */
NIMCP_EXPORT float rn_get_output(
    const nimcp_red_nucleus_t* rn,
    rn_effector_t effector);

/**
 * @brief Get all rubrospinal outputs
 *
 * @param rn Red Nucleus instance
 * @param outputs Output array (size RN_EFFECTOR_COUNT)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_get_all_outputs(
    const nimcp_red_nucleus_t* rn,
    float* outputs);

/*=============================================================================
 * MOTOR LEARNING API
 *===========================================================================*/

/**
 * @brief Process motor error signal
 *
 * @param rn Red Nucleus instance
 * @param error Error signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_process_error(
    nimcp_red_nucleus_t* rn,
    const rn_motor_error_t* error);

/**
 * @brief Report motor error (simplified interface)
 *
 * @param rn Red Nucleus instance
 * @param effector Affected effector
 * @param error_type Type of error
 * @param error_magnitude Error magnitude [-1, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_report_error(
    nimcp_red_nucleus_t* rn,
    rn_effector_t effector,
    rn_error_type_t error_type,
    float error_magnitude);

/**
 * @brief Get learning state for effector
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @param state Output learning state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_get_learning_state(
    const nimcp_red_nucleus_t* rn,
    rn_effector_t effector,
    rn_learning_state_t* state);

/**
 * @brief Get skill level for effector
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @return Skill level [0, 1]
 */
NIMCP_EXPORT float rn_get_skill_level(
    const nimcp_red_nucleus_t* rn,
    rn_effector_t effector);

/**
 * @brief Set learning rate modulation
 *
 * @param rn Red Nucleus instance
 * @param modulation Learning rate modulation [0, 2]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_set_learning_modulation(
    nimcp_red_nucleus_t* rn,
    float modulation);

/**
 * @brief Reset learning state for effector
 *
 * @param rn Red Nucleus instance
 * @param effector Target effector
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_reset_learning(
    nimcp_red_nucleus_t* rn,
    rn_effector_t effector);

/*=============================================================================
 * CEREBELLAR INTEGRATION API
 *===========================================================================*/

/**
 * @brief Process dentate nucleus input
 *
 * @param rn Red Nucleus instance
 * @param signal Dentate signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_process_dentate_input(
    nimcp_red_nucleus_t* rn,
    const rn_dentate_signal_t* signal);

/**
 * @brief Get olivary output (to inferior olive)
 *
 * @param rn Red Nucleus instance
 * @param output Output structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_get_olivary_output(
    const nimcp_red_nucleus_t* rn,
    rn_olivary_output_t* output);

/**
 * @brief Get thalamic output (dentato-rubro-thalamic)
 *
 * @param rn Red Nucleus instance
 * @param output Output structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_get_thalamic_output(
    const nimcp_red_nucleus_t* rn,
    rn_thalamic_output_t* output);

/**
 * @brief Connect to cerebellum adapter
 *
 * @param rn Red Nucleus instance
 * @param cerebellum Cerebellum adapter
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_cerebellum_connect(
    nimcp_red_nucleus_t* rn,
    struct cerebellum_adapter* cerebellum);

/**
 * @brief Process cerebellar error feedback
 *
 * @param rn Red Nucleus instance
 * @param error_magnitude Error from cerebellum [-1, 1]
 * @param error_type Error type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_process_cerebellar_error(
    nimcp_red_nucleus_t* rn,
    float error_magnitude,
    rn_error_type_t error_type);

/*=============================================================================
 * CORTICAL INPUT API
 *===========================================================================*/

/**
 * @brief Set cortical input (from motor cortex)
 *
 * @param rn Red Nucleus instance
 * @param cmd_type Command type being modulated
 * @param input Input level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_set_cortical_input(
    nimcp_red_nucleus_t* rn,
    rn_motor_cmd_type_t cmd_type,
    float input);

/**
 * @brief Get cortical modulation for command type
 *
 * @param rn Red Nucleus instance
 * @param cmd_type Command type
 * @return Cortical input level
 */
NIMCP_EXPORT float rn_get_cortical_input(
    const nimcp_red_nucleus_t* rn,
    rn_motor_cmd_type_t cmd_type);

/*=============================================================================
 * INTEGRATION API - KG WIRING
 *===========================================================================*/

/**
 * @brief Register Red Nucleus with Knowledge Graph
 *
 * @param rn Red Nucleus instance
 * @param kg Knowledge Graph handle
 * @param admin_token Security token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_register(
    nimcp_red_nucleus_t* rn,
    struct nimcp_brain_kg* kg,
    uint64_t admin_token);

/**
 * @brief Unregister Red Nucleus from Knowledge Graph
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_unregister(nimcp_red_nucleus_t* rn);

/**
 * @brief Update KG with current state
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_update_state(nimcp_red_nucleus_t* rn);

/**
 * @brief Query KG for related information
 *
 * @param rn Red Nucleus instance
 * @param query Query string
 * @param result Result buffer
 * @param result_size Result buffer size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_kg_query(
    nimcp_red_nucleus_t* rn,
    const char* query,
    void* result,
    size_t result_size);

/*=============================================================================
 * INTEGRATION API - BIO-ASYNC
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param rn Red Nucleus instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_bio_async_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_bio_router* router);

/**
 * @brief Disconnect from bio-async router
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_bio_async_disconnect(nimcp_red_nucleus_t* rn);

/**
 * @brief Broadcast Red Nucleus message
 *
 * @param rn Red Nucleus instance
 * @param msg_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_bio_async_broadcast(
    nimcp_red_nucleus_t* rn,
    rn_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);

/**
 * @brief Subscribe to messages
 *
 * @param rn Red Nucleus instance
 * @param subscription_mask Subscription bitmask
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_bio_async_subscribe(
    nimcp_red_nucleus_t* rn,
    uint32_t subscription_mask);

/*=============================================================================
 * INTEGRATION API - OTHER SYSTEMS
 *===========================================================================*/

/**
 * @brief Connect to immune system
 */
NIMCP_EXPORT int rn_immune_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_immune_system* immune);

/**
 * @brief Connect to security context
 */
NIMCP_EXPORT int rn_security_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_security_context* security);

/**
 * @brief Connect to SNN/plasticity
 */
NIMCP_EXPORT int rn_snn_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_snn_network* snn,
    struct nimcp_plasticity_engine* plasticity);

/**
 * @brief Connect to hypothalamus
 */
NIMCP_EXPORT int rn_hypothalamus_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_hypothalamus* hypo);

/**
 * @brief Connect to thalamus
 */
NIMCP_EXPORT int rn_thalamus_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_thalamus* thalamus);

/**
 * @brief Connect to cognitive hub
 */
NIMCP_EXPORT int rn_cognitive_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_cognitive_hub* hub);

/**
 * @brief Connect to training system
 */
NIMCP_EXPORT int rn_training_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_training_context* training);

/**
 * @brief Connect to perception system
 */
NIMCP_EXPORT int rn_perception_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_perception_system* perception);

/**
 * @brief Connect to symbolic logic engine
 */
NIMCP_EXPORT int rn_symbolic_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_symbolic_engine* symbolic);

/**
 * @brief Connect to swarm system
 */
NIMCP_EXPORT int rn_swarm_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_swarm_context* swarm);

/**
 * @brief Connect to dragonfly system
 */
NIMCP_EXPORT int rn_dragonfly_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_dragonfly_context* dragonfly);

/**
 * @brief Connect to portia system
 */
NIMCP_EXPORT int rn_portia_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_portia_context* portia);

/**
 * @brief Connect to QMC system
 */
NIMCP_EXPORT int rn_qmc_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_qmc_context* qmc);

/**
 * @brief Connect to omnidirectional predictor
 */
NIMCP_EXPORT int rn_omni_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_omni_predictor* omni);

/**
 * @brief Connect to neural substrate
 */
NIMCP_EXPORT int rn_substrate_connect(
    nimcp_red_nucleus_t* rn,
    struct nimcp_neural_substrate* substrate);

/*=============================================================================
 * UPDATE AND STATE API
 *===========================================================================*/

/**
 * @brief Update Red Nucleus state (call each timestep)
 *
 * @param rn Red Nucleus instance
 * @param dt Time delta in seconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_update(nimcp_red_nucleus_t* rn, float dt);

/**
 * @brief Get current statistics
 *
 * @param rn Red Nucleus instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_get_stats(
    const nimcp_red_nucleus_t* rn,
    rn_stats_t* stats);

/**
 * @brief Get subdivision activity
 *
 * @param rn Red Nucleus instance
 * @param subdiv Subdivision to query
 * @return Activity level [0, 1]
 */
NIMCP_EXPORT float rn_get_subdivision_activity(
    const nimcp_red_nucleus_t* rn,
    rn_subdivision_t subdiv);

/**
 * @brief Clear command queue
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_clear_commands(nimcp_red_nucleus_t* rn);

/**
 * @brief Abort current command
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_abort_command(nimcp_red_nucleus_t* rn);

/*=============================================================================
 * QUANTUM OPTIMIZATION API
 *===========================================================================*/

/**
 * @brief Optimize motor commands using QMC
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_qmc_optimize_commands(nimcp_red_nucleus_t* rn);

/**
 * @brief Use QMCTS for trajectory planning
 *
 * @param rn Red Nucleus instance
 * @param start Start position
 * @param goal Goal position
 * @param num_iterations QMCTS iterations
 * @param trajectory Output trajectory
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_qmcts_trajectory_search(
    nimcp_red_nucleus_t* rn,
    const rn_vector3_t* start,
    const rn_vector3_t* goal,
    uint32_t num_iterations,
    rn_trajectory_t* trajectory);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get subdivision name string
 *
 * @param subdiv Subdivision type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* rn_subdivision_string(rn_subdivision_t subdiv);

/**
 * @brief Get command type name string
 *
 * @param cmd_type Command type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* rn_cmd_type_string(rn_motor_cmd_type_t cmd_type);

/**
 * @brief Get effector name string
 *
 * @param effector Effector type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* rn_effector_string(rn_effector_t effector);

/**
 * @brief Get error type name string
 *
 * @param error_type Error type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* rn_error_type_string(rn_error_type_t error_type);

/**
 * @brief Get bio message type name string
 *
 * @param msg_type Message type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* rn_bio_msg_string(rn_bio_msg_type_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RED_NUCLEUS_H */
