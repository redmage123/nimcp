/**
 * @file nimcp_rn_bio_async_bridge.h
 * @brief Red Nucleus Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for Red Nucleus that provides
 *       comprehensive message routing for motor coordination via bio-router.
 *
 * WHY: The Red Nucleus needs to communicate:
 *      - Motor commands to spinal cord via rubrospinal tract
 *      - Motor error signals for learning and adaptation
 *      - Learning triggers for cerebellar coordination
 *      - Limb coordination signals between effectors
 *      - Postural adjustment commands
 *      - Cerebellar feedback for motor refinement
 *
 * HOW: Registers Red Nucleus as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes
 *      incoming modulation from cerebellum and motor cortex.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * RED NUCLEUS OUTPUT SIGNALS:
 * ---------------------------
 * 1. Motor commands:
 *    - Rubrospinal tract output to spinal cord
 *    - Forelimb and distal limb control
 *    - Mapped to: RN_BIO_MSG_MOTOR_CMD
 *
 * 2. Error signals:
 *    - Motor error detection for learning
 *    - Sent to inferior olive for cerebellar adaptation
 *    - Mapped to: RN_BIO_MSG_ERROR_SIGNAL
 *
 * 3. Learning triggers:
 *    - Cerebellar adaptation signals
 *    - Skill acquisition events
 *    - Mapped to: RN_BIO_MSG_LEARNING_UPDATE
 *
 * RED NUCLEUS INPUT SIGNALS:
 * --------------------------
 * 1. Cerebellar feedback:
 *    - Dentate nucleus input for motor correction
 *    - Timing adjustments from cerebellum
 *
 * 2. Cortical commands:
 *    - Motor cortex corticorubral input
 *    - Movement planning signals
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RN_BIO_ASYNC_BRIDGE_H
#define NIMCP_RN_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

/* Include main Red Nucleus header for message type definitions */
#include "core/brain/regions/red_nucleus/nimcp_red_nucleus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Red Nucleus module ID */
#define RN_BIO_BRIDGE_MODULE_ID         0x5004

/** Maximum number of module subscriptions */
#define RN_BIO_BRIDGE_MAX_SUBSCRIPTIONS 64

/** Maximum pending messages in inbox */
#define RN_BIO_BRIDGE_MAX_INBOX_SIZE    256

/** Maximum pending messages in outbox */
#define RN_BIO_BRIDGE_MAX_OUTBOX_SIZE   128

/** Default broadcast interval for motor state (ms) */
#define RN_BIO_BRIDGE_DEFAULT_INTERVAL_MS  20

/** Message expiry time (ms) */
#define RN_BIO_BRIDGE_MESSAGE_TTL_MS    5000

/** Motor error threshold for learning trigger */
#define RN_BIO_BRIDGE_ERROR_THRESHOLD   0.1f

/** Minimum interval between learning broadcasts (ms) */
#define RN_BIO_BRIDGE_LEARNING_MIN_MS   50

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Motor command message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Command identification */
    uint32_t command_type;              /**< Command type (velocity/force/position) */
    uint32_t effector_id;               /**< Target effector */
    uint32_t sequence_id;               /**< Sequence for multi-command */

    /* Command values */
    float magnitude;                    /**< Command magnitude [0, 1] */
    float urgency;                      /**< Command urgency [0, 1] */
    float duration_ms;                  /**< Command duration */

    /* 3D vector value */
    float value_x;                      /**< X component */
    float value_y;                      /**< Y component */
    float value_z;                      /**< Z component */

    /* Subdivision state */
    float magnocellular_activity;       /**< Magnocellular subdivision activity */
    float parvocellular_activity;       /**< Parvocellular subdivision activity */

    uint64_t timestamp_us;              /**< Command timestamp */
} rn_bio_bridge_motor_cmd_msg_t;

/**
 * @brief Motor error signal message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Error identification */
    uint32_t error_type;                /**< Error type (position/velocity/force) */
    uint32_t effector_id;               /**< Affected effector */
    uint32_t command_id;                /**< Related command ID */

    /* Error values */
    float error_magnitude;              /**< Error magnitude [-1, 1] */
    float error_x;                      /**< Error X component */
    float error_y;                      /**< Error Y component */
    float error_z;                      /**< Error Z component */

    /* Context */
    float cumulative_error;             /**< Cumulative error for session */
    bool triggers_learning;             /**< Whether this triggers learning */

    uint64_t timestamp_us;              /**< Error detection timestamp */
} rn_bio_bridge_error_msg_t;

/**
 * @brief Learning signal message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Learning state */
    uint32_t effector_id;               /**< Effector being learned */
    float learning_rate;                /**< Current learning rate */
    float adaptation_gain;              /**< Current adaptation gain */
    float skill_level;                  /**< Acquired skill level [0, 1] */

    /* Error history summary */
    float avg_error;                    /**< Average recent error */
    float error_reduction;              /**< Error reduction since start */
    float error_integral;               /**< Integrated error */
    float error_derivative;             /**< Error derivative */

    /* Training metrics */
    uint64_t training_iterations;       /**< Total training iterations */
    bool convergence_detected;          /**< Whether learning converged */

    uint64_t timestamp_us;              /**< Learning update timestamp */
} rn_bio_bridge_learning_msg_t;

/**
 * @brief Limb coordination message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Coordination state */
    uint32_t primary_effector;          /**< Primary effector in coordination */
    uint32_t secondary_effector;        /**< Secondary effector */
    float coordination_strength;        /**< Coordination coupling [0, 1] */
    float phase_difference;             /**< Phase difference between effectors */

    /* Movement parameters */
    float primary_output;               /**< Primary effector output level */
    float secondary_output;             /**< Secondary effector output level */
    float synchrony;                    /**< Inter-limb synchrony [0, 1] */

    /* Timing */
    float cycle_period_ms;              /**< Coordination cycle period */
    float phase_offset_ms;              /**< Phase offset between limbs */

    uint64_t timestamp_us;              /**< Coordination timestamp */
} rn_bio_bridge_limb_coord_msg_t;

/**
 * @brief Postural adjustment message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Adjustment command */
    float adjustment_x;                 /**< Postural adjustment X */
    float adjustment_y;                 /**< Postural adjustment Y */
    float adjustment_z;                 /**< Postural adjustment Z */
    float urgency;                      /**< Adjustment urgency [0, 1] */

    /* Context */
    float balance_deviation;            /**< Current balance deviation */
    float center_of_mass_shift;         /**< Center of mass shift */
    bool is_compensatory;               /**< Whether compensatory or voluntary */

    /* Effector involvement */
    float axial_involvement;            /**< Trunk/core involvement [0, 1] */
    float proximal_involvement;         /**< Proximal limb involvement [0, 1] */
    float distal_involvement;           /**< Distal limb involvement [0, 1] */

    uint64_t timestamp_us;              /**< Adjustment timestamp */
} rn_bio_bridge_posture_msg_t;

/**
 * @brief Cerebellar feedback message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Dentate signal */
    float dentate_activity;             /**< Dentate nucleus output activity */
    float timing_adjustment;            /**< Timing correction from cerebellum */

    /* Motor corrections */
    float correction_x;                 /**< Motor correction X */
    float correction_y;                 /**< Motor correction Y */
    float correction_z;                 /**< Motor correction Z */
    float correction_magnitude;         /**< Total correction magnitude */

    /* Olivary output (to inferior olive) */
    float olivary_error_signal;         /**< Error for olive */
    float learning_request;             /**< Learning rate request */

    /* Thalamic projection */
    float thalamic_activity;            /**< Thalamic projection strength */
    float motor_readiness;              /**< Motor preparation signal */

    uint64_t timestamp_us;              /**< Feedback timestamp */
} rn_bio_bridge_cerebellar_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry for bridge
 */
typedef struct {
    bio_module_id_t module_id;          /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} rn_bio_bridge_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Red Nucleus bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< Motor state broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast motor state */
    bool enable_error_broadcast;             /**< Broadcast on every error */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float error_learning_threshold;          /**< Error threshold for learning */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t motor_channel;   /**< Channel for motor commands */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_learning_broadcast;          /**< Broadcast learning state */
    bool enable_coordination_broadcast;      /**< Broadcast limb coordination */
    bool enable_posture_broadcast;           /**< Enable postural broadcasts */
    bool enable_cerebellar_broadcast;        /**< Enable cerebellar feedback */
    bool enable_logging;                     /**< Enable message logging */
} rn_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                  /**< Total messages sent */
    uint64_t messages_received;              /**< Total messages received */
    uint64_t messages_dropped;               /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;                /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t motor_cmd_broadcasts;           /**< Motor command broadcasts */
    uint64_t error_broadcasts;               /**< Error signal broadcasts */
    uint64_t learning_broadcasts;            /**< Learning update broadcasts */
    uint64_t coordination_broadcasts;        /**< Coordination broadcasts */
    uint64_t posture_broadcasts;             /**< Postural adjustment broadcasts */
    uint64_t cerebellar_broadcasts;          /**< Cerebellar feedback broadcasts */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active subs */
    uint32_t peak_subscriptions;             /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast timestamp */
    float avg_message_latency_us;            /**< Average message latency */
    float max_message_latency_us;            /**< Peak message latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Message handler errors */
    uint64_t routing_errors;                 /**< Routing failures */
} rn_bio_bridge_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Red Nucleus bio-async bridge handle
 */
typedef struct rn_bio_bridge_struct rn_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_default_config(rn_bio_bridge_config_t* config);

/**
 * @brief Create Red Nucleus bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
rn_bio_bridge_t* rn_bio_bridge_create(const rn_bio_bridge_config_t* config);

/**
 * @brief Destroy Red Nucleus bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void rn_bio_bridge_destroy(rn_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to Red Nucleus and router
 *
 * @param bridge Bridge handle
 * @param rn Red Nucleus to connect
 * @param router Bio-router for message dispatch
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_connect(
    rn_bio_bridge_t* bridge,
    nimcp_red_nucleus_t* rn,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_disconnect(rn_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool rn_bio_bridge_is_connected(const rn_bio_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int rn_bio_bridge_process_inbox(rn_bio_bridge_t* bridge, uint32_t max_messages);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_update(rn_bio_bridge_t* bridge, uint32_t delta_ms);

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Broadcast motor command
 *
 * @param bridge Bridge handle
 * @param command_type Command type
 * @param effector_id Target effector
 * @param magnitude Command magnitude
 * @param value_x X component
 * @param value_y Y component
 * @param value_z Z component
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_motor_cmd(
    rn_bio_bridge_t* bridge,
    uint32_t command_type,
    uint32_t effector_id,
    float magnitude,
    float value_x,
    float value_y,
    float value_z
);

/**
 * @brief Broadcast motor error signal
 *
 * @param bridge Bridge handle
 * @param error_type Error type
 * @param effector_id Affected effector
 * @param error_magnitude Error magnitude
 * @param error_x Error X component
 * @param error_y Error Y component
 * @param error_z Error Z component
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_error(
    rn_bio_bridge_t* bridge,
    uint32_t error_type,
    uint32_t effector_id,
    float error_magnitude,
    float error_x,
    float error_y,
    float error_z
);

/**
 * @brief Broadcast learning signal
 *
 * @param bridge Bridge handle
 * @param effector_id Effector being learned
 * @param learning_rate Current learning rate
 * @param skill_level Current skill level
 * @param avg_error Average error
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_learning(
    rn_bio_bridge_t* bridge,
    uint32_t effector_id,
    float learning_rate,
    float skill_level,
    float avg_error
);

/**
 * @brief Broadcast limb coordination signal
 *
 * @param bridge Bridge handle
 * @param primary_effector Primary effector
 * @param secondary_effector Secondary effector
 * @param coordination_strength Coordination strength
 * @param synchrony Inter-limb synchrony
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_limb_coord(
    rn_bio_bridge_t* bridge,
    uint32_t primary_effector,
    uint32_t secondary_effector,
    float coordination_strength,
    float synchrony
);

/**
 * @brief Broadcast postural adjustment
 *
 * @param bridge Bridge handle
 * @param adjustment_x Adjustment X
 * @param adjustment_y Adjustment Y
 * @param adjustment_z Adjustment Z
 * @param urgency Adjustment urgency
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_posture(
    rn_bio_bridge_t* bridge,
    float adjustment_x,
    float adjustment_y,
    float adjustment_z,
    float urgency
);

/**
 * @brief Broadcast cerebellar feedback
 *
 * @param bridge Bridge handle
 * @param dentate_activity Dentate activity
 * @param correction_x Correction X
 * @param correction_y Correction Y
 * @param correction_z Correction Z
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_broadcast_cerebellar(
    rn_bio_bridge_t* bridge,
    float dentate_activity,
    float correction_x,
    float correction_y,
    float correction_z
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to Red Nucleus messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to subscribe
 * @param msg_types Bitmask of message types (use RN_BIO_SUB_* macros)
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_subscribe_module(
    rn_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from Red Nucleus messages
 *
 * @param bridge Bridge handle
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_unsubscribe_module(rn_bio_bridge_t* bridge, uint32_t module_id);

/**
 * @brief Update module subscription types
 *
 * @param bridge Bridge handle
 * @param module_id Module to update
 * @param msg_types New bitmask of message types
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_update_subscription(
    rn_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type to query
 * @return Number of subscribers
 */
uint32_t rn_bio_bridge_get_subscriber_count(
    const rn_bio_bridge_t* bridge,
    rn_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_get_stats(
    const rn_bio_bridge_t* bridge,
    rn_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int rn_bio_bridge_reset_stats(rn_bio_bridge_t* bridge);

/**
 * @brief Get message type name for bridge messages
 *
 * @param msg_type Message type
 * @return Static string name
 */
const char* rn_bio_bridge_msg_type_name(rn_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void rn_bio_bridge_print_summary(const rn_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RN_BIO_ASYNC_BRIDGE_H */
