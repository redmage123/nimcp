/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_ph_substrate_bridge.h - pH Dynamics to Bio-Async Messaging Bridge
//=============================================================================
/**
 * @file nimcp_ph_substrate_bridge.h
 * @brief Bridge connecting pH dynamics with bio-async substrate messaging
 *
 * WHAT: Bidirectional bridge between pH dynamics and the bio-async messaging
 *       substrate for distributing pH state and receiving metabolic commands.
 *
 * WHY:  pH information needs to propagate across the neural system:
 *       - pH affects multiple downstream systems asynchronously
 *       - pH changes trigger homeostatic responses system-wide
 *       - Metabolic demands from other modules affect pH regulation
 *       - Decoupled messaging allows efficient parallel processing
 *
 * HOW:  Two-way integration:
 *       1. pH -> Substrate: Publish pH state, alerts, changes
 *       2. Substrate -> pH: Receive acid loads, metabolic signals
 *       3. Pub/Sub channels: Topic-based pH event distribution
 *       4. Priority messaging: Critical pH alerts get priority
 *
 * MESSAGE ARCHITECTURE:
 * ```
 * pH DYNAMICS                              BIO-ASYNC SUBSTRATE
 * ---------------------------------------------------------------
 * pH state updates                      -> nimcp.ph.state channel
 * Critical pH alerts                    -> nimcp.ph.alert (priority)
 * Regional pH changes                   -> nimcp.ph.region.<id>
 * Buffer exhaustion                     -> nimcp.ph.buffer.depleted
 * Metabolic acid load                   <- nimcp.metabolism.acid
 * Activity-induced load                 <- nimcp.activity.load
 * Respiratory adjustment                <- nimcp.respiration.command
 * ```
 *
 * CHANNEL HIERARCHY:
 * - nimcp.ph.* : All pH-related messages
 * - nimcp.ph.state : Regular state updates
 * - nimcp.ph.alert : Critical alerts (acidosis/alkalosis)
 * - nimcp.ph.region.* : Per-region updates
 * - nimcp.ph.pump.* : Pump activity updates
 * - nimcp.ph.buffer.* : Buffer status updates
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_SUBSTRATE_BRIDGE_H
#define NIMCP_PH_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PH_SUBSTRATE_MODULE_NAME        "ph_substrate_bridge"

/** Maximum message queue depth */
#define PH_SUBSTRATE_MAX_QUEUE          256

/** Maximum channel subscriptions */
#define PH_SUBSTRATE_MAX_SUBSCRIPTIONS  32

/** Maximum topic length */
#define PH_SUBSTRATE_MAX_TOPIC_LEN      64

/** Default publish interval (ms) */
#define PH_SUBSTRATE_PUBLISH_INTERVAL   10.0f

/** Alert message priority */
#define PH_SUBSTRATE_PRIORITY_ALERT     10

/** Normal message priority */
#define PH_SUBSTRATE_PRIORITY_NORMAL    5

/** Low priority (stats, debug) */
#define PH_SUBSTRATE_PRIORITY_LOW       1

/** State update topic */
#define PH_SUBSTRATE_TOPIC_STATE        "nimcp.ph.state"

/** Alert topic */
#define PH_SUBSTRATE_TOPIC_ALERT        "nimcp.ph.alert"

/** Pump activity topic */
#define PH_SUBSTRATE_TOPIC_PUMP         "nimcp.ph.pump"

/** Buffer status topic */
#define PH_SUBSTRATE_TOPIC_BUFFER       "nimcp.ph.buffer"

/** Metabolic load topic (incoming) */
#define PH_SUBSTRATE_TOPIC_METABOLIC    "nimcp.metabolism.acid"

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Message types for pH substrate communication
 */
typedef enum {
    PH_SUBSTRATE_MSG_STATE_UPDATE = 0,   /**< Regular state update */
    PH_SUBSTRATE_MSG_ALERT,              /**< Critical pH alert */
    PH_SUBSTRATE_MSG_REGION_UPDATE,      /**< Region-specific update */
    PH_SUBSTRATE_MSG_PUMP_STATUS,        /**< Pump activity report */
    PH_SUBSTRATE_MSG_BUFFER_STATUS,      /**< Buffer status report */
    PH_SUBSTRATE_MSG_METABOLIC_LOAD,     /**< Incoming metabolic load */
    PH_SUBSTRATE_MSG_COMMAND,            /**< Control command */
    PH_SUBSTRATE_MSG_COUNT
} ph_substrate_msg_type_t;

/**
 * @brief Alert severity levels
 */
typedef enum {
    PH_SUBSTRATE_ALERT_NONE = 0,         /**< No alert */
    PH_SUBSTRATE_ALERT_WARNING,          /**< Mild deviation */
    PH_SUBSTRATE_ALERT_MODERATE,         /**< Moderate deviation */
    PH_SUBSTRATE_ALERT_SEVERE,           /**< Severe deviation */
    PH_SUBSTRATE_ALERT_CRITICAL          /**< Life-threatening */
} ph_substrate_alert_t;

/**
 * @brief Command types from substrate
 */
typedef enum {
    PH_SUBSTRATE_CMD_NONE = 0,           /**< No command */
    PH_SUBSTRATE_CMD_ADD_ACID,           /**< Add acid load */
    PH_SUBSTRATE_CMD_ADD_BASE,           /**< Add base load */
    PH_SUBSTRATE_CMD_SET_PUMP,           /**< Set pump activity */
    PH_SUBSTRATE_CMD_SET_VENTILATION,    /**< Set ventilation */
    PH_SUBSTRATE_CMD_RESET               /**< Reset pH to normal */
} ph_substrate_cmd_t;

/**
 * @brief Subscription callback result
 */
typedef enum {
    PH_SUBSTRATE_CB_OK = 0,              /**< Message processed */
    PH_SUBSTRATE_CB_SKIP,                /**< Skip message */
    PH_SUBSTRATE_CB_ERROR                /**< Processing error */
} ph_substrate_cb_result_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Publishing parameters */
    bool enable_state_publish;           /**< Publish state updates */
    bool enable_alert_publish;           /**< Publish alerts */
    bool enable_region_publish;          /**< Publish region updates */
    float state_publish_interval_ms;     /**< State update interval */
    float alert_publish_interval_ms;     /**< Min alert interval */

    /** Subscription parameters */
    bool subscribe_metabolic;            /**< Subscribe to metabolic */
    bool subscribe_commands;             /**< Subscribe to commands */
    bool subscribe_activity;             /**< Subscribe to activity loads */

    /** Queue parameters */
    uint32_t outgoing_queue_size;        /**< Outgoing message queue */
    uint32_t incoming_queue_size;        /**< Incoming message queue */
    bool drop_on_full;                   /**< Drop if queue full */

    /** Priority parameters */
    uint8_t state_priority;              /**< Priority for state msgs */
    uint8_t alert_priority;              /**< Priority for alerts */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_substrate_config_t;

/**
 * @brief pH state message payload
 */
typedef struct {
    float extracellular_ph;              /**< Extracellular pH */
    float intracellular_ph;              /**< Intracellular pH */
    float vesicular_ph;                  /**< Vesicular pH */
    float ph_deviation;                  /**< Deviation from normal */
    float buffer_capacity;               /**< Available buffering */
    float conductance_modifier;          /**< Conductance effect */
    float release_modifier;              /**< NT release effect */
    uint32_t region_id;                  /**< Source region (if applicable) */
    float timestamp_ms;                  /**< Message timestamp */
} ph_substrate_state_msg_t;

/**
 * @brief pH alert message payload
 */
typedef struct {
    ph_substrate_alert_t severity;       /**< Alert severity */
    float current_ph;                    /**< Current pH value */
    float deviation;                     /**< Deviation from normal */
    float rate_of_change;                /**< pH change rate */
    uint32_t affected_region;            /**< Affected region ID */
    char compartment[16];                /**< Affected compartment */
    float timestamp_ms;                  /**< Alert timestamp */
} ph_substrate_alert_msg_t;

/**
 * @brief Metabolic load message payload (incoming)
 */
typedef struct {
    float acid_load;                     /**< Proton equivalents */
    float source_activity;               /**< Source activity level */
    uint32_t source_region;              /**< Source region ID */
    char source_module[32];              /**< Source module name */
    float duration_ms;                   /**< Load duration */
    float timestamp_ms;                  /**< Message timestamp */
} ph_substrate_load_msg_t;

/**
 * @brief Command message payload (incoming)
 */
typedef struct {
    ph_substrate_cmd_t command;          /**< Command type */
    float value;                         /**< Command value */
    uint32_t target_region;              /**< Target region (0 = all) */
    char target_pump[16];                /**< Target pump (if applicable) */
    float timestamp_ms;                  /**< Command timestamp */
} ph_substrate_cmd_msg_t;

/**
 * @brief Generic message wrapper
 */
typedef struct {
    ph_substrate_msg_type_t type;        /**< Message type */
    uint8_t priority;                    /**< Message priority */
    char topic[PH_SUBSTRATE_MAX_TOPIC_LEN]; /**< Topic string */
    uint32_t sequence;                   /**< Sequence number */
    float timestamp_ms;                  /**< Message timestamp */

    /** Payload union */
    union {
        ph_substrate_state_msg_t state;
        ph_substrate_alert_msg_t alert;
        ph_substrate_load_msg_t load;
        ph_substrate_cmd_msg_t cmd;
        uint8_t raw[128];                /**< Raw bytes */
    } payload;
} ph_substrate_message_t;

/**
 * @brief Message callback function type
 */
typedef ph_substrate_cb_result_t (*ph_substrate_callback_t)(
    const ph_substrate_message_t* message,
    void* user_data
);

/**
 * @brief Subscription descriptor
 */
typedef struct {
    char topic[PH_SUBSTRATE_MAX_TOPIC_LEN]; /**< Topic pattern */
    ph_substrate_callback_t callback;    /**< Callback function */
    void* user_data;                     /**< User data for callback */
    bool active;                         /**< Subscription active */
    uint64_t messages_received;          /**< Messages received */
} ph_substrate_subscription_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_published;         /**< Total messages published */
    uint64_t messages_received;          /**< Total messages received */
    uint64_t alerts_published;           /**< Alerts published */
    uint64_t commands_processed;         /**< Commands processed */
    uint64_t metabolic_loads_received;   /**< Metabolic loads received */
    uint64_t queue_drops;                /**< Messages dropped (queue full) */
    float total_acid_load_received;      /**< Cumulative acid load */
    float avg_publish_latency_ms;        /**< Average publish latency */
    uint32_t active_subscriptions;       /**< Active subscription count */
    float last_update_ms;                /**< Last update timestamp */
} ph_substrate_stats_t;

/** Opaque bridge handle */
typedef struct ph_substrate_bridge_struct ph_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_default_config(ph_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-substrate bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_substrate_bridge_t* ph_substrate_bridge_create(
    const ph_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_substrate_bridge_destroy(ph_substrate_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async substrate
 *
 * @param bridge Bridge handle
 * @param substrate_handle Opaque substrate handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_connect(
    ph_substrate_bridge_t* bridge,
    void* substrate_handle
);

/**
 * @brief Disconnect from substrate
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_disconnect(ph_substrate_bridge_t* bridge);

//=============================================================================
// Publishing API (pH -> Substrate)
//=============================================================================

/**
 * @brief Publish pH state update
 *
 * WHAT: Publishes current pH state to substrate
 * WHY:  Distributes pH information to subscribers
 * HOW:  Serializes state, queues for delivery
 *
 * @param bridge Bridge handle
 * @param state State to publish
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_publish_state(
    ph_substrate_bridge_t* bridge,
    const ph_substrate_state_msg_t* state
);

/**
 * @brief Publish pH alert
 *
 * WHAT: Publishes critical pH alert
 * WHY:  High-priority notification of dangerous pH
 * HOW:  Creates alert message with priority
 *
 * @param bridge Bridge handle
 * @param alert Alert to publish
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_publish_alert(
    ph_substrate_bridge_t* bridge,
    const ph_substrate_alert_msg_t* alert
);

/**
 * @brief Publish region-specific update
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param state State for region
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_publish_region(
    ph_substrate_bridge_t* bridge,
    uint32_t region_id,
    const ph_substrate_state_msg_t* state
);

/**
 * @brief Publish custom message
 *
 * @param bridge Bridge handle
 * @param message Message to publish
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_publish(
    ph_substrate_bridge_t* bridge,
    const ph_substrate_message_t* message
);

//=============================================================================
// Subscription API (Substrate -> pH)
//=============================================================================

/**
 * @brief Subscribe to topic with callback
 *
 * WHAT: Registers callback for topic messages
 * WHY:  Enables async reception of substrate messages
 * HOW:  Pattern matching on topic, callback invocation
 *
 * @param bridge Bridge handle
 * @param topic Topic pattern (supports wildcards)
 * @param callback Callback function
 * @param user_data User data for callback
 * @return Subscription ID or -1 on error
 */
NIMCP_EXPORT int ph_substrate_subscribe(
    ph_substrate_bridge_t* bridge,
    const char* topic,
    ph_substrate_callback_t callback,
    void* user_data
);

/**
 * @brief Unsubscribe from topic
 *
 * @param bridge Bridge handle
 * @param subscription_id Subscription ID from subscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_unsubscribe(
    ph_substrate_bridge_t* bridge,
    int subscription_id
);

/**
 * @brief Get subscription info
 *
 * @param bridge Bridge handle
 * @param subscription_id Subscription ID
 * @param subscription Output subscription info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_get_subscription(
    const ph_substrate_bridge_t* bridge,
    int subscription_id,
    ph_substrate_subscription_t* subscription
);

//=============================================================================
// Incoming Message API
//=============================================================================

/**
 * @brief Process incoming metabolic load
 *
 * WHAT: Handles incoming acid load from metabolism
 * WHY:  External modules report acid production
 * HOW:  Accumulates load for pH dynamics
 *
 * @param bridge Bridge handle
 * @param load Metabolic load message
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_process_load(
    ph_substrate_bridge_t* bridge,
    const ph_substrate_load_msg_t* load
);

/**
 * @brief Process incoming command
 *
 * @param bridge Bridge handle
 * @param cmd Command message
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_process_command(
    ph_substrate_bridge_t* bridge,
    const ph_substrate_cmd_msg_t* cmd
);

/**
 * @brief Get accumulated acid load from substrate
 *
 * @param bridge Bridge handle
 * @param acid_load Output: accumulated acid load
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_get_acid_load(
    const ph_substrate_bridge_t* bridge,
    float* acid_load
);

/**
 * @brief Reset accumulated acid load
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_reset_acid_load(
    ph_substrate_bridge_t* bridge
);

/**
 * @brief Poll for next incoming message
 *
 * @param bridge Bridge handle
 * @param message Output message (if available)
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return 1 if message available, 0 if none, -1 on error
 */
NIMCP_EXPORT int ph_substrate_poll(
    ph_substrate_bridge_t* bridge,
    ph_substrate_message_t* message,
    float timeout_ms
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process queues, invoke callbacks, publish updates
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_update(
    ph_substrate_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Flush outgoing message queue
 *
 * @param bridge Bridge handle
 * @return Number of messages flushed
 */
NIMCP_EXPORT int ph_substrate_flush(ph_substrate_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_reset(ph_substrate_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_substrate_get_stats(
    const ph_substrate_bridge_t* bridge,
    ph_substrate_stats_t* stats
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge handle
 * @return true if connected to substrate
 */
NIMCP_EXPORT bool ph_substrate_is_connected(
    const ph_substrate_bridge_t* bridge
);

/**
 * @brief Get outgoing queue depth
 *
 * @param bridge Bridge handle
 * @return Number of messages in outgoing queue
 */
NIMCP_EXPORT uint32_t ph_substrate_get_queue_depth(
    const ph_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_SUBSTRATE_BRIDGE_H */