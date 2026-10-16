/**
 * @file nimcp_surface_bio_async_bridge.h
 * @brief Surface Geometry Bio-Async Bridge
 *
 * WHAT: Async messaging bridge for surface geometry events
 * WHY:  Enables real-time geometry updates across brain modules
 * HOW:  Integrates with bio-router using neuromodulator channels
 *
 * CHANNELS:
 * - SEROTONIN: Slow geometry changes, parameter updates
 * - NOREPINEPHRINE: Urgent anomaly notifications
 * - DOPAMINE: Optimization rewards/completion
 * - ACETYLCHOLINE: Attention-modulated geometry requests
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_BIO_ASYNC_BRIDGE_H
#define NIMCP_SURFACE_BIO_ASYNC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/geometry/nimcp_surface_geometry_types.h"

//=============================================================================
// MODULE IDENTIFIER
//=============================================================================

/** Bio-async module ID for surface geometry */
#define BIO_MODULE_SURFACE_BIO_ASYNC    0x1410

//=============================================================================
// BIO MESSAGE TYPES
//=============================================================================

/**
 * @brief Bio-async message IDs for surface geometry
 *
 * Range: 0x1400 - 0x14FF reserved for surface geometry
 */
typedef enum {
    BIO_MSG_SURFACE_GEOMETRY_UPDATE = 0x1400,   /**< Geometry parameter update */
    BIO_MSG_SURFACE_BRANCH_FORMED = 0x1401,     /**< Branch formation event */
    BIO_MSG_SURFACE_TRIFURCATION = 0x1402,      /**< Trifurcation detected */
    BIO_MSG_SURFACE_SPROUT = 0x1403,            /**< Orthogonal sprout formed */
    BIO_MSG_SURFACE_SYNAPSE_SPROUT = 0x1404,    /**< Sprout -> synapse event */
    BIO_MSG_SURFACE_OPTIMIZATION_DONE = 0x1405, /**< Optimization complete */
    BIO_MSG_SURFACE_ANOMALY = 0x1406,           /**< Geometry anomaly */
    BIO_MSG_SURFACE_MATERIAL_UPDATE = 0x1407,   /**< Material budget change */
    BIO_MSG_SURFACE_REQUEST = 0x1408,           /**< Geometry request */
    BIO_MSG_SURFACE_MODULATE = 0x1409,          /**< Parameter modulation */
    BIO_MSG_SURFACE_VALIDATION = 0x140A,        /**< Validation result */
    BIO_MSG_SURFACE_REGION_STATS = 0x140B,      /**< Region statistics */
    BIO_MSG_SURFACE_MAX = 0x14FF
} bio_msg_surface_type_t;

//=============================================================================
// MESSAGE PAYLOADS
//=============================================================================

/**
 * @brief Geometry update message payload
 */
typedef struct surface_bio_msg_geometry_update_struct {
    uint32_t branch_point_id;
    surface_geometry_params_t params;
    float position[3];
    uint64_t timestamp_ms;
} surface_bio_msg_geometry_update_t;

/**
 * @brief Branch formation message payload
 */
typedef struct surface_bio_msg_branch_formed_struct {
    uint32_t branch_point_id;
    surface_branch_type_t branch_type;
    float position[3];
    float chi;
    float rho;
    uint64_t timestamp_ms;
} surface_bio_msg_branch_formed_t;

/**
 * @brief Optimization complete message payload
 */
typedef struct surface_bio_msg_optimization_done_struct {
    uint32_t optimization_id;
    float surface_area;
    float wire_length;
    uint32_t iterations;
    bool converged;
    surface_branch_type_t final_branch_type;
    uint64_t duration_ms;
} surface_bio_msg_optimization_done_t;

/**
 * @brief Anomaly detection message payload
 */
typedef struct surface_bio_msg_anomaly_struct {
    uint32_t branch_point_id;
    surface_error_t error_code;
    float expected_value;
    float actual_value;
    const char* description;
    uint64_t timestamp_ms;
} surface_bio_msg_anomaly_t;

/**
 * @brief Material budget update message payload
 */
typedef struct surface_bio_msg_material_update_struct {
    uint32_t region_id;
    float old_budget;
    float new_budget;
    float consumed;
    float remaining;
} surface_bio_msg_material_update_t;

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Bio-async bridge configuration
 */
typedef struct surface_bio_async_config_struct {
    /* Channel configuration */
    nimcp_bio_channel_type_t default_channel;   /**< Default channel (SEROTONIN) */
    nimcp_bio_channel_type_t urgent_channel;    /**< Urgent channel (NOREPINEPHRINE) */
    nimcp_bio_channel_type_t reward_channel;    /**< Reward channel (DOPAMINE) */

    /* Timing */
    uint32_t update_interval_ms;                /**< Min interval between updates */
    uint32_t batch_threshold;                   /**< Messages before batch send */

    /* Filtering */
    bool filter_duplicate_updates;              /**< Filter duplicate geometry updates */
    float update_threshold;                     /**< Min change to trigger update */

    /* Priority */
    uint8_t default_priority;                   /**< Default message priority */
    uint8_t anomaly_priority;                   /**< Anomaly message priority */
} surface_bio_async_config_t;

//=============================================================================
// SUBSCRIPTION
//=============================================================================

/**
 * @brief Message subscription entry
 */
typedef struct surface_bio_subscription_struct {
    bio_msg_surface_type_t msg_type;
    uint32_t subscriber_module_id;
    void (*callback)(const void* payload, size_t payload_size, void* user_data);
    void* user_data;
    bool active;
} surface_bio_subscription_t;

//=============================================================================
// BRIDGE STRUCTURE
//=============================================================================

/**
 * @brief Surface geometry bio-async bridge
 */
typedef struct surface_bio_async_bridge_struct {
    /* Base bridge (MUST be first) */
    bridge_base_t base;

    /* Surface geometry context */
    void* geometry_ctx;

    /* Bio router handle */
    bio_router_t* router;

    /* Configuration */
    surface_bio_async_config_t config;

    /* Subscriptions */
    surface_bio_subscription_t* subscriptions;
    uint32_t num_subscriptions;
    uint32_t max_subscriptions;

    /* Message queue */
    void* pending_messages;
    uint32_t pending_count;
    uint32_t max_pending;

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t bytes_sent;
    uint64_t bytes_received;

    /* Timing */
    uint64_t last_send_time_ms;
    uint64_t last_receive_time_ms;

    /* State */
    bool connected;
    bool paused;
} surface_bio_async_bridge_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_bio_async_default_config(surface_bio_async_config_t* config);

/**
 * @brief Create bio-async bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Created bridge or NULL on failure
 */
surface_bio_async_bridge_t* surface_bio_async_bridge_create(
    const surface_bio_async_config_t* config
);

/**
 * @brief Destroy bio-async bridge
 *
 * @param bridge Bridge to destroy
 */
void surface_bio_async_bridge_destroy(surface_bio_async_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int surface_bio_async_bridge_reset(surface_bio_async_bridge_t* bridge);

//=============================================================================
// CONNECTION
//=============================================================================

/**
 * @brief Connect to bio router
 *
 * @param bridge Bridge
 * @param router Bio router
 * @return 0 on success, -1 on error
 */
int surface_bio_async_bridge_connect(
    surface_bio_async_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_bio_async_bridge_disconnect(surface_bio_async_bridge_t* bridge);

/**
 * @brief Check connection status
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool surface_bio_async_bridge_is_connected(const surface_bio_async_bridge_t* bridge);

/**
 * @brief Set geometry context
 *
 * @param bridge Bridge
 * @param ctx Geometry context
 * @return 0 on success, -1 on error
 */
int surface_bio_async_bridge_set_geometry_ctx(
    surface_bio_async_bridge_t* bridge,
    void* ctx
);

//=============================================================================
// SUBSCRIPTION
//=============================================================================

/**
 * @brief Subscribe to message type
 *
 * @param bridge Bridge
 * @param msg_type Message type
 * @param callback Callback function
 * @param user_data User data for callback
 * @return Subscription ID or -1 on error
 */
int surface_bio_async_bridge_subscribe(
    surface_bio_async_bridge_t* bridge,
    bio_msg_surface_type_t msg_type,
    void (*callback)(const void*, size_t, void*),
    void* user_data
);

/**
 * @brief Unsubscribe from message type
 *
 * @param bridge Bridge
 * @param subscription_id Subscription ID from subscribe
 * @return 0 on success, -1 on error
 */
int surface_bio_async_bridge_unsubscribe(
    surface_bio_async_bridge_t* bridge,
    int subscription_id
);

//=============================================================================
// MESSAGE SENDING
//=============================================================================

/**
 * @brief Send geometry update message
 *
 * @param bridge Bridge
 * @param update Update payload
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_geometry_update(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_geometry_update_t* update
);

/**
 * @brief Send branch formation message
 *
 * @param bridge Bridge
 * @param branch Branch formation payload
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_branch_formed(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_branch_formed_t* branch
);

/**
 * @brief Send optimization complete message
 *
 * @param bridge Bridge
 * @param result Optimization result payload
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_optimization_done(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_optimization_done_t* result
);

/**
 * @brief Send anomaly message (urgent channel)
 *
 * @param bridge Bridge
 * @param anomaly Anomaly payload
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_anomaly(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_anomaly_t* anomaly
);

/**
 * @brief Send material budget update
 *
 * @param bridge Bridge
 * @param update Material update payload
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_material_update(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_material_update_t* update
);

/**
 * @brief Send raw message
 *
 * @param bridge Bridge
 * @param msg_type Message type
 * @param payload Payload data
 * @param payload_size Payload size
 * @param channel Channel to use
 * @param priority Message priority
 * @return 0 on success, -1 on error
 */
int surface_bio_async_send_raw(
    surface_bio_async_bridge_t* bridge,
    bio_msg_surface_type_t msg_type,
    const void* payload,
    size_t payload_size,
    nimcp_bio_channel_type_t channel,
    uint8_t priority
);

//=============================================================================
// MESSAGE PROCESSING
//=============================================================================

/**
 * @brief Process pending messages
 *
 * @param bridge Bridge
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
int surface_bio_async_process_messages(
    surface_bio_async_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Flush pending messages
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_bio_async_flush(surface_bio_async_bridge_t* bridge);

/**
 * @brief Pause message processing
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_bio_async_pause(surface_bio_async_bridge_t* bridge);

/**
 * @brief Resume message processing
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_bio_async_resume(surface_bio_async_bridge_t* bridge);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get bridge statistics
 */
typedef struct surface_bio_async_stats_struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t active_subscriptions;
    uint32_t pending_messages;
    bool connected;
    bool paused;
} surface_bio_async_stats_t;

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int surface_bio_async_get_stats(
    const surface_bio_async_bridge_t* bridge,
    surface_bio_async_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_bio_async_reset_stats(surface_bio_async_bridge_t* bridge);

//=============================================================================
// UTILITY
//=============================================================================

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name
 */
const char* surface_bio_msg_type_name_async(bio_msg_surface_type_t msg_type);

/**
 * @brief Get channel name
 *
 * @param channel Channel type
 * @return Human-readable name
 */
const char* surface_bio_channel_name(nimcp_bio_channel_type_t channel);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_BIO_ASYNC_BRIDGE_H */
