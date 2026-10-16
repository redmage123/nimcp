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
// nimcp_no_substrate_bridge.h - Nitric Oxide to Bio-Async Substrate Bridge
//=============================================================================
/**
 * @file nimcp_no_substrate_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and bio-async
 *        messaging substrate
 *
 * WHAT: Connects NO gasotransmitter signaling with the bio-async messaging
 *       system for distributed neural-glial-vascular communication.
 *
 * WHY:  Nitric oxide is an ideal bio-async signaling molecule:
 *       - Diffuses rapidly without synaptic constraints
 *       - Volume transmission enables broadcast messaging
 *       - Short half-life provides temporal precision
 *       - Multiple NOS isoforms enable diverse message types
 *       - Vascular effects coordinate metabolic messaging
 *
 * HOW:  Integration pathways:
 *       1. NO → Messages: NO events converted to bio-async messages
 *       2. Messages → NO: Incoming messages can trigger NOS activity
 *       3. Vascular routing: Blood flow affects message propagation
 *       4. Spatial domains: NO diffusion defines message broadcast radius
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIO-ASYNC SUBSTRATE                      NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Message broadcast (multicast)         ← NO volume transmission
 * Message priority/urgency              ← NO concentration gradient
 * Message decay (TTL)                   ← NO half-life (~1s)
 * Message domains (regions)             ← NO diffusion sphere (~100um)
 * Transport rate                        ← Vasodilation (blood flow)
 * ```
 *
 * MESSAGE TYPES:
 * - NOS_ACTIVATION: nNOS/eNOS/iNOS activation events
 * - NO_RELEASE: NO concentration changes
 * - CGMP_SIGNAL: Downstream cGMP pathway activation
 * - VASCULAR_STATE: Blood flow/vasodilation changes
 * - RETROGRADE_SIGNAL: Synaptic modulation events
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_SUBSTRATE_BRIDGE_H
#define NIMCP_NO_SUBSTRATE_BRIDGE_H

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
#define NO_SUBSTRATE_MODULE_NAME        "no_substrate_bridge"

/** Maximum message sources */
#define NO_SUBSTRATE_MAX_SOURCES        256

/** Maximum pending messages */
#define NO_SUBSTRATE_MAX_PENDING        1024

/** Default message TTL based on NO half-life (ms) */
#define NO_SUBSTRATE_MSG_TTL_MS         1000.0f

/** Default broadcast radius (um) */
#define NO_SUBSTRATE_BROADCAST_RADIUS   100.0f

/** Bio-async module ID for NO substrate */
#define BIO_MODULE_NO_SUBSTRATE         0x0E04

/** NO substrate message class */
#define NO_SUBSTRATE_MSG_CLASS          0x4E4F  /* "NO" in ASCII */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief NO-derived message types
 */
typedef enum {
    NO_MSG_NOS_ACTIVATION = 0,          /**< NOS enzyme activation */
    NO_MSG_NO_RELEASE,                  /**< NO release event */
    NO_MSG_NO_CONCENTRATION,            /**< NO concentration update */
    NO_MSG_CGMP_SIGNAL,                 /**< cGMP pathway activation */
    NO_MSG_VASCULAR_STATE,              /**< Vasodilation state */
    NO_MSG_RETROGRADE,                  /**< Retrograde synaptic signal */
    NO_MSG_METABOLIC,                   /**< Metabolic modulation */
    NO_MSG_CUSTOM                       /**< User-defined message */
} no_substrate_msg_type_t;

/**
 * @brief Message priority based on NO concentration
 */
typedef enum {
    NO_MSG_PRIORITY_LOW = 0,            /**< Basal NO levels */
    NO_MSG_PRIORITY_NORMAL,             /**< Moderate NO elevation */
    NO_MSG_PRIORITY_HIGH,               /**< Strong NO burst */
    NO_MSG_PRIORITY_URGENT              /**< Pathological NO levels */
} no_substrate_priority_t;

/**
 * @brief Message delivery mode
 */
typedef enum {
    NO_MSG_DELIVERY_LOCAL = 0,          /**< Single target */
    NO_MSG_DELIVERY_BROADCAST,          /**< Volume transmission sphere */
    NO_MSG_DELIVERY_VASCULAR,           /**< Follow blood vessels */
    NO_MSG_DELIVERY_DIRECTED            /**< Specific targets */
} no_substrate_delivery_t;

/**
 * @brief NOS source type for message origin
 */
typedef enum {
    NO_MSG_SOURCE_NNOS = 0,             /**< Neuronal NOS */
    NO_MSG_SOURCE_ENOS,                 /**< Endothelial NOS */
    NO_MSG_SOURCE_INOS,                 /**< Inducible NOS */
    NO_MSG_SOURCE_EXTERNAL              /**< External NO donor */
} no_substrate_source_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-Substrate bridge
 */
typedef struct {
    /** Message parameters */
    float default_ttl_ms;                    /**< Default message TTL */
    float broadcast_radius_um;               /**< Default broadcast radius */
    bool enable_priority_scaling;            /**< Priority based on NO level */
    bool enable_ttl_decay;                   /**< TTL decreases with distance */

    /** Delivery parameters */
    no_substrate_delivery_t default_delivery; /**< Default delivery mode */
    bool enable_vascular_routing;            /**< Route via blood vessels */
    float vascular_speed_factor;             /**< Blood flow speed factor */

    /** Threshold parameters */
    float activation_threshold_nm;           /**< NO threshold for message */
    float urgent_threshold_nm;               /**< Threshold for urgent priority */
    float concentration_scale;               /**< NO concentration to value */

    /** Bio-async integration */
    bool enable_bio_async;                   /**< Enable bio-async router */
    uint16_t module_id;                      /**< Bio-async module ID */

    /** Timing */
    float update_interval_ms;                /**< Update interval */
} no_substrate_config_t;

/**
 * @brief NO-derived bio-async message
 */
typedef struct {
    uint32_t message_id;                     /**< Unique message ID */
    no_substrate_msg_type_t type;            /**< Message type */
    no_substrate_priority_t priority;        /**< Message priority */
    no_substrate_delivery_t delivery;        /**< Delivery mode */

    /** Source information */
    uint32_t source_id;                      /**< NO source ID */
    no_substrate_source_type_t source_type;  /**< NOS isoform */
    float source_position[3];                /**< Source position (um) */

    /** Content */
    float no_concentration_nm;               /**< NO concentration (nM) */
    float cgmp_level_um;                     /**< cGMP level (uM) */
    float vasodilation_factor;               /**< Vasodilation (1.0 = baseline) */
    float custom_value;                      /**< User-defined value */

    /** Timing */
    float timestamp_ms;                      /**< Creation time */
    float ttl_ms;                            /**< Time to live */
    float propagation_distance_um;           /**< Distance traveled */

    /** Targets */
    uint32_t target_ids[8];                  /**< Specific target IDs */
    uint32_t num_targets;                    /**< Number of targets */
    float broadcast_radius_um;               /**< Broadcast radius */
} no_substrate_message_t;

/**
 * @brief Message source registration
 */
typedef struct {
    uint32_t source_id;                      /**< Source ID */
    float position[3];                       /**< Position (um) */
    no_substrate_source_type_t nos_type;     /**< NOS isoform */

    /** Current state */
    float no_concentration_nm;               /**< Current NO level */
    float nos_activity;                      /**< NOS activity (0-1) */
    bool is_active;                          /**< Currently producing NO */

    /** Message stats */
    uint32_t messages_sent;                  /**< Total messages sent */
    float last_message_ms;                   /**< Last message time */
} no_substrate_source_t;

/**
 * @brief Message receiver registration
 */
typedef struct {
    uint32_t receiver_id;                    /**< Receiver ID */
    float position[3];                       /**< Position (um) */

    /** Subscription */
    no_substrate_msg_type_t subscribed_types[8]; /**< Subscribed message types */
    uint32_t num_subscriptions;              /**< Number of subscriptions */
    float receive_radius_um;                 /**< Reception radius */

    /** Stats */
    uint32_t messages_received;              /**< Total messages received */
    float last_receive_ms;                   /**< Last receive time */
} no_substrate_receiver_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total updates */
    uint64_t messages_created;               /**< Messages created */
    uint64_t messages_delivered;             /**< Messages delivered */
    uint64_t messages_expired;               /**< Messages expired (TTL) */
    uint64_t broadcasts;                     /**< Broadcast deliveries */
    uint64_t vascular_routes;                /**< Vascular routed messages */

    uint32_t active_sources;                 /**< Active NO sources */
    uint32_t registered_receivers;           /**< Registered receivers */
    uint32_t pending_messages;               /**< Messages in transit */

    float mean_delivery_time_ms;             /**< Average delivery time */
    float mean_propagation_um;               /**< Average propagation distance */
    float last_update_ms;
} no_substrate_stats_t;

/** Opaque bridge handle */
typedef struct no_substrate_bridge_struct no_substrate_bridge_t;

/** Message handler callback */
typedef void (*no_substrate_handler_t)(
    const no_substrate_message_t* message,
    void* user_data
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_default_config(no_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-Substrate bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_substrate_bridge_t* no_substrate_bridge_create(
    const no_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_substrate_bridge_destroy(no_substrate_bridge_t* bridge);

//=============================================================================
// Source Management API
//=============================================================================

/**
 * @brief Register NO source as message producer
 *
 * WHAT: Registers NO source for message generation
 * WHY:  NO sources broadcast via bio-async messaging
 * HOW:  Creates source entry linked to bio-async router
 *
 * @param bridge Bridge handle
 * @param source_id NO source ID
 * @param position 3D position [x, y, z] in um
 * @param nos_type NOS isoform type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_register_source(
    no_substrate_bridge_t* bridge,
    uint32_t source_id,
    const float position[3],
    no_substrate_source_type_t nos_type
);

/**
 * @brief Unregister NO source
 *
 * @param bridge Bridge handle
 * @param source_id Source to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_unregister_source(
    no_substrate_bridge_t* bridge,
    uint32_t source_id
);

/**
 * @brief Register message receiver
 *
 * WHAT: Registers entity to receive NO messages
 * WHY:  Enable subscription to NO signaling events
 * HOW:  Creates receiver with type subscriptions
 *
 * @param bridge Bridge handle
 * @param receiver_id Receiver ID
 * @param position 3D position
 * @param handler Message callback
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_register_receiver(
    no_substrate_bridge_t* bridge,
    uint32_t receiver_id,
    const float position[3],
    no_substrate_handler_t handler,
    void* user_data
);

/**
 * @brief Subscribe receiver to message type
 *
 * @param bridge Bridge handle
 * @param receiver_id Receiver ID
 * @param msg_type Message type to subscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_subscribe(
    no_substrate_bridge_t* bridge,
    uint32_t receiver_id,
    no_substrate_msg_type_t msg_type
);

//=============================================================================
// Message Creation API
//=============================================================================

/**
 * @brief Create and send NO message
 *
 * WHAT: Creates bio-async message from NO event
 * WHY:  Convert NO signaling to distributed messaging
 * HOW:  Populates message, adds to routing queue
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param type Message type
 * @param no_concentration_nm NO concentration (nM)
 * @param[out] message_id Assigned message ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_send_message(
    no_substrate_bridge_t* bridge,
    uint32_t source_id,
    no_substrate_msg_type_t type,
    float no_concentration_nm,
    uint32_t* message_id
);

/**
 * @brief Create message with full parameters
 *
 * @param bridge Bridge handle
 * @param message Message to send (will be copied)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_send_full_message(
    no_substrate_bridge_t* bridge,
    const no_substrate_message_t* message
);

/**
 * @brief Broadcast NO release event
 *
 * WHAT: Broadcasts NO release to all receivers in radius
 * WHY:  Volume transmission is broadcast by nature
 * HOW:  Creates broadcast message with spatial radius
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param no_concentration_nm NO concentration
 * @param radius_um Broadcast radius (0 for default)
 * @return Number of receivers notified, -1 on error
 */
NIMCP_EXPORT int no_substrate_broadcast_release(
    no_substrate_bridge_t* bridge,
    uint32_t source_id,
    float no_concentration_nm,
    float radius_um
);

/**
 * @brief Send vascular state update
 *
 * WHAT: Broadcasts vasodilation state via vascular routing
 * WHY:  NO-mediated blood flow affects message propagation
 * HOW:  Routes message along vascular pathways
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param vasodilation_factor Vasodilation (1.0 = baseline)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_send_vascular_state(
    no_substrate_bridge_t* bridge,
    uint32_t source_id,
    float vasodilation_factor
);

//=============================================================================
// Message Receiving API
//=============================================================================

/**
 * @brief Poll for pending messages for receiver
 *
 * @param bridge Bridge handle
 * @param receiver_id Receiver ID
 * @param[out] messages Array to fill with messages
 * @param max_messages Maximum messages to retrieve
 * @return Number of messages retrieved
 */
NIMCP_EXPORT int no_substrate_poll_messages(
    no_substrate_bridge_t* bridge,
    uint32_t receiver_id,
    no_substrate_message_t* messages,
    uint32_t max_messages
);

/**
 * @brief Check if receiver has pending messages
 *
 * @param bridge Bridge handle
 * @param receiver_id Receiver ID
 * @return Number of pending messages
 */
NIMCP_EXPORT int no_substrate_pending_count(
    const no_substrate_bridge_t* bridge,
    uint32_t receiver_id
);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_connect_router(
    no_substrate_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_disconnect_router(
    no_substrate_bridge_t* bridge
);

/**
 * @brief Check if connected to bio-async router
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
NIMCP_EXPORT bool no_substrate_is_connected(
    const no_substrate_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of message routing
 * WHY:  Process pending messages, apply TTL decay
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_update(
    no_substrate_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_reset(no_substrate_bridge_t* bridge);

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
NIMCP_EXPORT int no_substrate_get_stats(
    const no_substrate_bridge_t* bridge,
    no_substrate_stats_t* stats
);

/**
 * @brief Get source information
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param[out] source Source data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_get_source(
    const no_substrate_bridge_t* bridge,
    uint32_t source_id,
    no_substrate_source_t* source
);

/**
 * @brief Get receiver information
 *
 * @param bridge Bridge handle
 * @param receiver_id Receiver ID
 * @param[out] receiver Receiver data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_substrate_get_receiver(
    const no_substrate_bridge_t* bridge,
    uint32_t receiver_id,
    no_substrate_receiver_t* receiver
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_SUBSTRATE_BRIDGE_H */