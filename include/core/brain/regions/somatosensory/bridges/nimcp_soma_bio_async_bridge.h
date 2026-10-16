/**
 * @file nimcp_soma_bio_async_bridge.h
 * @brief Somatosensory Cortex Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Central bio-async integration for somatosensory cortex that provides
 *       comprehensive message routing for touch, pain, proprioception, and
 *       temperature signals via the bio-router.
 *
 * WHY: The somatosensory cortex processes all bodily sensations and needs to:
 *      - Route touch events to motor cortex and attention systems
 *      - Broadcast pain signals urgently to hypothalamus and motor systems
 *      - Synchronize proprioceptive state with cerebellum and parietal cortex
 *      - Coordinate temperature signals with hypothalamus for homeostasis
 *
 * HOW: Registers somatosensory as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      motor efference copy and attention modulation requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * SOMATOSENSORY OUTPUT PATHWAYS:
 * ------------------------------
 * 1. Cortical projections:
 *    - Motor cortex: Sensorimotor integration, grip control
 *    - Parietal cortex: Body schema, spatial awareness
 *    - Mapped to: SOMA_BIO_MSG_TOUCH_EVENT, SOMA_BIO_MSG_PROPRIO_UPDATE
 *
 * 2. Limbic projections:
 *    - Insula: Interoception, pain affect
 *    - Amygdala: Pain-related fear responses
 *    - Mapped to: SOMA_BIO_MSG_PAIN_ALERT, SOMA_BIO_MSG_TEMPERATURE
 *
 * 3. Subcortical projections:
 *    - Thalamus VPL/VPM: Relay feedback
 *    - Hypothalamus: Temperature regulation, stress response
 *    - Cerebellum: Proprioceptive coordination
 *    - Medulla: Autonomic reflexes, pain withdrawal
 *    - Mapped to: SOMA_BIO_MSG_DANGER_WITHDRAWAL, SOMA_BIO_MSG_BODY_MAP_CHANGE
 *
 * SOMATOSENSORY INPUT PATHWAYS:
 * -----------------------------
 * 1. Motor (efference copy):
 *    - Expected sensory consequences of movement
 *    - Prediction error computation
 *
 * 2. Attention (top-down):
 *    - Selective attention to body regions
 *    - Gain modulation of tactile processing
 *
 * 3. Hypothalamic (homeostatic):
 *    - Temperature setpoint signals
 *    - Stress-induced analgesia
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

#ifndef NIMCP_SOMA_BIO_ASYNC_BRIDGE_H
#define NIMCP_SOMA_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module ID for somatosensory in bio-async system (0x3100 - 0x31FF reserved) */
#define BIO_MODULE_ID_SOMATOSENSORY     0x3100

/** Maximum number of module subscriptions */
#define SOMA_BIO_MAX_SUBSCRIPTIONS      64

/** Maximum pending messages in inbox */
#define SOMA_BIO_MAX_INBOX_SIZE         256

/** Maximum pending messages in outbox */
#define SOMA_BIO_MAX_OUTBOX_SIZE        128

/** Default broadcast interval for touch state (ms) */
#define SOMA_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define SOMA_BIO_MESSAGE_TTL_MS         5000

/** Pain urgency threshold for immediate routing */
#define SOMA_BIO_PAIN_URGENCY_THRESHOLD 0.7f

/** Temperature extreme threshold for urgent messaging */
#define SOMA_BIO_TEMP_EXTREME_THRESHOLD 0.8f

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Somatosensory bio-async message types
 *
 * WHAT: Message type enumeration for somatosensory bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific somatosensory output pathway
 */
typedef enum {
    SOMA_BIO_MSG_TOUCH_EVENT = 0,       /**< Touch detected at body location */
    SOMA_BIO_MSG_PAIN_ALERT,            /**< Pain signal (urgent priority) */
    SOMA_BIO_MSG_PROPRIO_UPDATE,        /**< Proprioceptive state change */
    SOMA_BIO_MSG_TEMPERATURE,           /**< Temperature sensation */
    SOMA_BIO_MSG_BODY_MAP_CHANGE,       /**< Body schema update */
    SOMA_BIO_MSG_MOTOR_EFFERENCE,       /**< Efference copy from motor cortex */
    SOMA_BIO_MSG_PREDICTION_ERROR,      /**< Sensory prediction error */
    SOMA_BIO_MSG_ATTENTION_REQUEST,     /**< Request attention to body part */
    SOMA_BIO_MSG_DANGER_WITHDRAWAL,     /**< Urgent withdrawal reflex trigger */
    SOMA_BIO_MSG_TEXTURE_RESULT,        /**< Texture processing result */
    SOMA_BIO_MSG_GRIP_FORCE,            /**< Grip force adjustment signal */
    SOMA_BIO_MSG_REQUEST_STATE,         /**< Request current somatosensory state */
    SOMA_BIO_MSG_MODULATE_GAIN,         /**< External gain modulation request */
    SOMA_BIO_MSG_COUNT
} soma_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define SOMA_BIO_SUB_TOUCH_EVENT        (1U << SOMA_BIO_MSG_TOUCH_EVENT)
#define SOMA_BIO_SUB_PAIN_ALERT         (1U << SOMA_BIO_MSG_PAIN_ALERT)
#define SOMA_BIO_SUB_PROPRIO_UPDATE     (1U << SOMA_BIO_MSG_PROPRIO_UPDATE)
#define SOMA_BIO_SUB_TEMPERATURE        (1U << SOMA_BIO_MSG_TEMPERATURE)
#define SOMA_BIO_SUB_BODY_MAP_CHANGE    (1U << SOMA_BIO_MSG_BODY_MAP_CHANGE)
#define SOMA_BIO_SUB_MOTOR_EFFERENCE    (1U << SOMA_BIO_MSG_MOTOR_EFFERENCE)
#define SOMA_BIO_SUB_PREDICTION_ERROR   (1U << SOMA_BIO_MSG_PREDICTION_ERROR)
#define SOMA_BIO_SUB_ATTENTION_REQUEST  (1U << SOMA_BIO_MSG_ATTENTION_REQUEST)
#define SOMA_BIO_SUB_DANGER_WITHDRAWAL  (1U << SOMA_BIO_MSG_DANGER_WITHDRAWAL)
#define SOMA_BIO_SUB_TEXTURE_RESULT     (1U << SOMA_BIO_MSG_TEXTURE_RESULT)
#define SOMA_BIO_SUB_GRIP_FORCE         (1U << SOMA_BIO_MSG_GRIP_FORCE)
#define SOMA_BIO_SUB_ALL                (0xFFFFFFFFU)

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Touch event message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Touch location and properties */
    body_segment_t body_segment;        /**< Body segment touched */
    float position[3];                  /**< Position on segment [0,1] */
    float intensity;                    /**< Touch intensity [0, 1] */
    touch_modality_t touch_type;        /**< Type of touch */

    /* Processing results */
    float cortical_activation;          /**< Cortical response strength */
    uint32_t active_receptors;          /**< Number of active receptors */
    uint32_t touch_id;                  /**< Unique touch event ID */

    uint64_t timestamp_us;              /**< Event timestamp */
} soma_bio_touch_event_msg_t;

/**
 * @brief Pain alert message payload (high priority)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Pain location and properties */
    body_segment_t body_segment;        /**< Body segment in pain */
    pain_type_t pain_type;              /**< Type of pain */
    float intensity;                    /**< Pain intensity [0, 1] */
    float urgency;                      /**< Urgency level [0, 1] */

    /* Pain characteristics */
    bool is_chronic;                    /**< Chronic vs acute pain */
    bool is_referred;                   /**< Referred pain */
    bool requires_withdrawal;           /**< Triggers withdrawal reflex */

    /* Modulation state */
    float descending_modulation;        /**< Descending inhibition level */
    float gate_control_state;           /**< Gate control modulation */

    uint32_t pain_id;                   /**< Unique pain event ID */
    uint64_t onset_time_us;             /**< Pain onset timestamp */
    uint64_t timestamp_us;              /**< Current timestamp */
} soma_bio_pain_alert_msg_t;

/**
 * @brief Proprioceptive update message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Body segment state */
    body_segment_t body_segment;        /**< Body segment */
    float joint_angle;                  /**< Current joint angle (radians) */
    float angular_velocity;             /**< Angular velocity (rad/s) */
    float muscle_tension;               /**< Muscle tension [0, 1] */
    float muscle_length;                /**< Muscle length [0, 1] */

    /* Movement characteristics */
    bool is_active_movement;            /**< Active vs passive movement */
    float position_confidence;          /**< Position estimate confidence */
    float velocity_confidence;          /**< Velocity estimate confidence */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} soma_bio_proprio_update_msg_t;

/**
 * @brief Temperature message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Temperature state */
    body_segment_t body_segment;        /**< Body segment */
    float temperature_celsius;          /**< Temperature in Celsius */
    temp_sensation_t sensation;         /**< Temperature sensation category */

    /* Comfort/danger assessment */
    float comfort_level;                /**< Thermal comfort [0, 1] */
    bool is_dangerous;                  /**< Temperature is dangerous */
    bool triggers_hypothalamus;         /**< Needs hypothalamic response */

    uint64_t timestamp_us;              /**< Measurement timestamp */
} soma_bio_temperature_msg_t;

/**
 * @brief Body map change message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Change type */
    uint32_t change_type;               /**< Type of body map change */
    body_segment_t affected_segment;    /**< Affected body segment */

    /* Change details */
    float magnification_change;         /**< Cortical magnification change */
    float receptive_field_change;       /**< Receptive field size change */
    bool tool_extension;                /**< Tool use body schema extension */

    uint64_t timestamp_us;              /**< Change timestamp */
} soma_bio_body_map_change_msg_t;

/**
 * @brief Motor efference copy message payload (incoming)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Movement parameters */
    body_segment_t target_segment;      /**< Target body segment */
    float expected_position[3];         /**< Expected position */
    float expected_velocity[3];         /**< Expected velocity */
    float expected_force;               /**< Expected force */

    /* Movement timing */
    uint64_t movement_onset_us;         /**< Movement start time */
    uint64_t expected_completion_us;    /**< Expected completion time */

    uint32_t motor_command_id;          /**< Motor command ID for matching */
    uint64_t timestamp_us;              /**< Efference copy timestamp */
} soma_bio_motor_efference_msg_t;

/**
 * @brief Sensory prediction error message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Error details */
    body_segment_t body_segment;        /**< Body segment with error */
    float prediction_error;             /**< Prediction error magnitude */
    float expected_value;               /**< What was expected */
    float actual_value;                 /**< What was sensed */

    /* Error source */
    uint32_t error_source;              /**< Source of prediction error */
    bool is_touch_error;                /**< Touch prediction error */
    bool is_proprio_error;              /**< Proprioceptive error */
    bool is_pain_error;                 /**< Pain prediction error */

    uint64_t timestamp_us;              /**< Error detection timestamp */
} soma_bio_prediction_error_msg_t;

/**
 * @brief Danger withdrawal trigger message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Withdrawal trigger */
    body_segment_t body_segment;        /**< Body segment to withdraw */
    float urgency;                      /**< Withdrawal urgency [0, 1] */
    float recommended_velocity;         /**< Recommended withdrawal speed */

    /* Danger source */
    uint32_t danger_type;               /**< Type of danger */
    float danger_intensity;             /**< Danger intensity */
    float position[3];                  /**< Danger position on segment */

    uint64_t timestamp_us;              /**< Trigger timestamp */
} soma_bio_danger_withdrawal_msg_t;

/**
 * @brief Gain modulation request payload (incoming)
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    /* Modulation parameters */
    body_segment_t target_segment;      /**< Target segment (or 0xFF for all) */
    float gain_multiplier;              /**< Gain multiplier [0.5, 2.0] */
    float attention_level;              /**< Attention level [0, 1] */

    /* Source information */
    uint32_t requester_module;          /**< Who is requesting */
    uint32_t modulation_reason;         /**< Why modulation requested */
    uint64_t duration_ms;               /**< Requested duration */

    uint64_t timestamp_us;              /**< Request timestamp */
} soma_bio_gain_modulation_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;          /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} soma_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Somatosensory bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t touch_broadcast_interval_ms;    /**< Touch state broadcast interval */
    uint32_t proprio_broadcast_interval_ms;  /**< Proprioceptive broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast sensory state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    float pain_urgency_threshold;            /**< Threshold for urgent pain messages */
    float temp_extreme_threshold;            /**< Threshold for extreme temperature */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent (pain) messages */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum module subscriptions */

    /* Feature flags */
    bool enable_pain_routing;                /**< Enable pain signal routing */
    bool enable_proprio_routing;             /**< Enable proprioceptive routing */
    bool enable_temperature_routing;         /**< Enable temperature routing */
    bool enable_motor_efference;             /**< Enable motor efference processing */
    bool enable_prediction_error;            /**< Enable prediction error computation */
    bool enable_logging;                     /**< Enable message logging */
} soma_bio_async_config_t;

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
    uint64_t touch_events_sent;              /**< Touch event broadcasts */
    uint64_t pain_alerts_sent;               /**< Pain alert broadcasts */
    uint64_t proprio_updates_sent;           /**< Proprioceptive updates sent */
    uint64_t temperature_sent;               /**< Temperature messages sent */
    uint64_t prediction_errors_sent;         /**< Prediction error signals */
    uint64_t withdrawals_triggered;          /**< Withdrawal triggers sent */

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
} soma_bio_async_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Somatosensory bio-async bridge handle
 */
typedef struct soma_bio_router_struct soma_bio_router_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int soma_bio_async_default_config(soma_bio_async_config_t* config);

/**
 * @brief Create somatosensory bio-async bridge
 */
soma_bio_router_t* soma_bio_async_bridge_create(
    const soma_bio_async_config_t* config
);

/**
 * @brief Destroy somatosensory bio-async bridge
 */
void soma_bio_async_bridge_destroy(soma_bio_router_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to somatosensory cortex and router
 */
int soma_bio_async_connect(
    soma_bio_router_t* bridge,
    nimcp_somatosensory_t* soma,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 */
int soma_bio_async_disconnect(soma_bio_router_t* bridge);

/**
 * @brief Check if bridge is connected
 */
bool soma_bio_async_is_connected(const soma_bio_router_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 */
int soma_bio_async_process_inbox(
    soma_bio_router_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 */
int soma_bio_async_update(
    soma_bio_router_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Broadcast API - Touch
 * ============================================================================ */

/**
 * @brief Broadcast touch event to all subscribers
 */
int soma_bio_async_broadcast_touch(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    const float* position,
    float intensity,
    touch_modality_t touch_type
);

/**
 * @brief Broadcast texture processing result
 */
int soma_bio_async_broadcast_texture(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float roughness,
    float hardness
);

/**
 * @brief Broadcast grip force adjustment
 */
int soma_bio_async_broadcast_grip_force(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float current_force,
    float recommended_force
);

/* ============================================================================
 * Broadcast API - Pain
 * ============================================================================ */

/**
 * @brief Broadcast pain alert (high priority)
 */
int soma_bio_async_broadcast_pain(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    pain_type_t pain_type,
    float intensity
);

/**
 * @brief Broadcast danger withdrawal trigger
 */
int soma_bio_async_broadcast_withdrawal(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float urgency,
    uint32_t danger_type
);

/* ============================================================================
 * Broadcast API - Proprioception
 * ============================================================================ */

/**
 * @brief Broadcast proprioceptive update
 */
int soma_bio_async_broadcast_proprio(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float joint_angle,
    float angular_velocity,
    float muscle_tension
);

/**
 * @brief Broadcast body map change
 */
int soma_bio_async_broadcast_body_map_change(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    uint32_t change_type,
    bool tool_extension
);

/* ============================================================================
 * Broadcast API - Temperature
 * ============================================================================ */

/**
 * @brief Broadcast temperature sensation
 */
int soma_bio_async_broadcast_temperature(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float temperature_celsius,
    temp_sensation_t sensation
);

/* ============================================================================
 * Broadcast API - Prediction Error
 * ============================================================================ */

/**
 * @brief Broadcast sensory prediction error
 */
int soma_bio_async_broadcast_prediction_error(
    soma_bio_router_t* bridge,
    body_segment_t segment,
    float prediction_error,
    uint32_t error_source
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to somatosensory messages
 */
int soma_bio_async_subscribe_module(
    soma_bio_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from somatosensory messages
 */
int soma_bio_async_unsubscribe_module(
    soma_bio_router_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 */
int soma_bio_async_update_subscription(
    soma_bio_router_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 */
uint32_t soma_bio_async_get_subscriber_count(
    const soma_bio_router_t* bridge,
    soma_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int soma_bio_async_get_stats(
    const soma_bio_router_t* bridge,
    soma_bio_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 */
int soma_bio_async_reset_stats(soma_bio_router_t* bridge);

/**
 * @brief Get message type name
 */
const char* soma_bio_msg_type_name(soma_bio_msg_type_t msg_type);

/**
 * @brief Print bridge summary to stdout
 */
void soma_bio_async_print_summary(const soma_bio_router_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMA_BIO_ASYNC_BRIDGE_H */
