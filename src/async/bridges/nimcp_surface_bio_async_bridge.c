/**
 * @file nimcp_surface_bio_async_bridge.c
 * @brief Surface Geometry Bio-Async Bridge Implementation
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "async/bridges/nimcp_surface_bio_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_bio_async_bridge)

#define LOG_MODULE "SURFACE_BIO_ASYNC_BRIDGE"


//=============================================================================
// CONSTANTS
//=============================================================================

#define MAX_SUBSCRIPTIONS 64
#define MAX_PENDING_MESSAGES 256
#define MODULE_NAME "surface_bio_async"

//=============================================================================
// MESSAGE TYPE NAMES
//=============================================================================

static const char* MSG_TYPE_NAMES[] = {
    [BIO_MSG_SURFACE_GEOMETRY_UPDATE - 0x1400] = "GEOMETRY_UPDATE",
    [BIO_MSG_SURFACE_BRANCH_FORMED - 0x1400] = "BRANCH_FORMED",
    [BIO_MSG_SURFACE_TRIFURCATION - 0x1400] = "TRIFURCATION",
    [BIO_MSG_SURFACE_SPROUT - 0x1400] = "SPROUT",
    [BIO_MSG_SURFACE_SYNAPSE_SPROUT - 0x1400] = "SYNAPSE_SPROUT",
    [BIO_MSG_SURFACE_OPTIMIZATION_DONE - 0x1400] = "OPTIMIZATION_DONE",
    [BIO_MSG_SURFACE_ANOMALY - 0x1400] = "ANOMALY",
    [BIO_MSG_SURFACE_MATERIAL_UPDATE - 0x1400] = "MATERIAL_UPDATE",
    [BIO_MSG_SURFACE_REQUEST - 0x1400] = "REQUEST",
    [BIO_MSG_SURFACE_MODULATE - 0x1400] = "MODULATE",
    [BIO_MSG_SURFACE_VALIDATION - 0x1400] = "VALIDATION",
    [BIO_MSG_SURFACE_REGION_STATS - 0x1400] = "REGION_STATS"
};

static const char* CHANNEL_NAMES[] = {
    "DOPAMINE",
    "SEROTONIN",
    "NOREPINEPHRINE",
    "ACETYLCHOLINE",
    "GLUTAMATE",
    "GABA"
};

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_bio_async_default_config(surface_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_bio_async_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Channel defaults based on biological analogy */
    config->default_channel = BIO_CHANNEL_SEROTONIN;     /* Slow changes */
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE; /* Anomalies */
    config->reward_channel = BIO_CHANNEL_DOPAMINE;       /* Rewards */

    /* Timing */
    config->update_interval_ms = 50;
    config->batch_threshold = 10;

    /* Filtering */
    config->filter_duplicate_updates = true;
    config->update_threshold = 0.001f;

    /* Priority */
    config->default_priority = 5;
    config->anomaly_priority = 9;

    return 0;
}

//=============================================================================
// LIFECYCLE
//=============================================================================

surface_bio_async_bridge_t* surface_bio_async_bridge_create(
    const surface_bio_async_config_t* config
) {
    BRIDGE_CREATE_BEGIN(surface_bio_async_bridge_t, bridge,
                        BIO_MODULE_SURFACE_BIO_ASYNC, MODULE_NAME);

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(surface_bio_async_config_t));
    } else {
        surface_bio_async_default_config(&bridge->config);
    }

    /* Allocate subscriptions */
    bridge->max_subscriptions = MAX_SUBSCRIPTIONS;
    bridge->subscriptions = nimcp_malloc(
        bridge->max_subscriptions * sizeof(surface_bio_subscription_t));
    if (!bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "surface_bio_async_bridge_create: failed to allocate subscriptions");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->subscriptions, 0,
           bridge->max_subscriptions * sizeof(surface_bio_subscription_t));
    bridge->num_subscriptions = 0;

    /* Initialize state */
    bridge->connected = false;
    bridge->paused = false;
    bridge->router = NULL;
    bridge->geometry_ctx = NULL;

    /* Statistics */
    bridge->messages_sent = 0;
    bridge->messages_received = 0;
    bridge->messages_dropped = 0;
    bridge->bytes_sent = 0;
    bridge->bytes_received = 0;

    return bridge;
}

void surface_bio_async_bridge_destroy(surface_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "surface_bio_async");

    /* Disconnect first */
    if (bridge->connected) {
        surface_bio_async_bridge_disconnect(bridge);
    }

    /* Free subscriptions */
    if (bridge->subscriptions) {
        nimcp_free(bridge->subscriptions);
    }

    /* Free pending messages if allocated */
    if (bridge->pending_messages) {
        nimcp_free(bridge->pending_messages);
    }

    BRIDGE_DESTROY(bridge);
}

int surface_bio_async_bridge_reset(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    /* Reset statistics (with locking) */
    BRIDGE_LOCK(bridge);
    bridge->messages_sent = 0;
    bridge->messages_received = 0;
    bridge->messages_dropped = 0;
    bridge->bytes_sent = 0;
    bridge->bytes_received = 0;
    bridge->pending_count = 0;
    bridge->paused = false;

    /* Clear subscriptions */
    for (uint32_t i = 0; i < bridge->num_subscriptions; i++) {
        bridge->subscriptions[i].active = false;
    }
    bridge->num_subscriptions = 0;
    BRIDGE_UNLOCK(bridge);

    /* bridge_base_reset handles its own locking */
    return bridge_base_reset(&bridge->base);
}

//=============================================================================
// CONNECTION
//=============================================================================

int surface_bio_async_bridge_connect(
    surface_bio_async_bridge_t* bridge,
    bio_router_t* router
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    if (bridge->connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;  /* Already connected */
    }

    bridge->router = router;

    /* Connect to bio-async via base */
    if (bridge_base_connect_bio_async(&bridge->base) != 0) {
        /* Non-fatal - can operate without router */
        bridge->router = NULL;
    }

    bridge->connected = (bridge->router != NULL);
    bridge->base.bridge_active = bridge->connected;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_bio_async_bridge_disconnect(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    if (!bridge->connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Flush pending messages (inline to avoid deadlock) */
    bridge->pending_count = 0;

    BRIDGE_UNLOCK(bridge);

    /* Disconnect from bio-async (handles its own locking) */
    bridge_base_disconnect_bio_async(&bridge->base);

    BRIDGE_LOCK(bridge);

    bridge->router = NULL;
    bridge->connected = false;
    bridge->base.bridge_active = false;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

bool surface_bio_async_bridge_is_connected(const surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge->connected;
}

int surface_bio_async_bridge_set_geometry_ctx(
    surface_bio_async_bridge_t* bridge,
    void* ctx
) {
    BRIDGE_NULL_CHECK(bridge);

    /* Note: bridge_base_connect_a handles its own locking */
    bridge->geometry_ctx = ctx;
    return bridge_base_connect_a(&bridge->base, ctx);
}

//=============================================================================
// SUBSCRIPTION
//=============================================================================

int surface_bio_async_bridge_subscribe(
    surface_bio_async_bridge_t* bridge,
    bio_msg_surface_type_t msg_type,
    void (*callback)(const void*, size_t, void*),
    void* user_data
) {
    BRIDGE_NULL_CHECK(bridge);

    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_bio_async_bridge_subscribe: callback is NULL");
        return -1;
    }
    if (msg_type < BIO_MSG_SURFACE_GEOMETRY_UPDATE || msg_type > BIO_MSG_SURFACE_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_bio_async_bridge_subscribe: validation failed");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Find empty slot */
    int slot = -1;
    for (uint32_t i = 0; i < bridge->max_subscriptions; i++) {
        if (!bridge->subscriptions[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_bio_async_bridge_subscribe: validation failed");
        return -1;  /* No room */
    }

    /* Create subscription */
    surface_bio_subscription_t* sub = &bridge->subscriptions[slot];
    sub->msg_type = msg_type;
    sub->subscriber_module_id = bridge->base.module_id;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->active = true;

    bridge->num_subscriptions++;

    BRIDGE_UNLOCK(bridge);
    return slot;  /* Return subscription ID */
}

int surface_bio_async_bridge_unsubscribe(
    surface_bio_async_bridge_t* bridge,
    int subscription_id
) {
    BRIDGE_NULL_CHECK(bridge);

    if (subscription_id < 0 || (uint32_t)subscription_id >= bridge->max_subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_bio_async_bridge_unsubscribe: capacity exceeded");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    if (bridge->subscriptions[subscription_id].active) {
        bridge->subscriptions[subscription_id].active = false;
        bridge->num_subscriptions--;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// MESSAGE SENDING - INTERNAL HELPER
//=============================================================================

static int send_message_internal(
    surface_bio_async_bridge_t* bridge,
    bio_msg_surface_type_t msg_type,
    const void* payload,
    size_t payload_size,
    nimcp_bio_channel_type_t channel,
    uint8_t priority
) {
    if (!bridge->connected) {
        bridge->messages_dropped++;
        return -1;  /* Disconnected is normal operational state */
    }

    /* Rate limiting */
    uint64_t now = nimcp_time_monotonic_ms();
    if (now - bridge->last_send_time_ms < bridge->config.update_interval_ms) {
        /* Queue message instead of dropping */
        bridge->pending_count++;
        return 0;
    }

    /* In a full implementation, this would call bio_router_send() */
    /* For now, just track statistics */
    (void)msg_type;
    (void)channel;
    (void)priority;

    bridge->messages_sent++;
    bridge->bytes_sent += payload_size;
    bridge->last_send_time_ms = now;

    /* Deliver to local subscribers */
    for (uint32_t i = 0; i < bridge->max_subscriptions; i++) {
        surface_bio_subscription_t* sub = &bridge->subscriptions[i];
        if (sub->active && sub->msg_type == msg_type && sub->callback) {
            sub->callback(payload, payload_size, sub->user_data);
        }
    }

    bridge_base_record_update(&bridge->base);
    return 0;
}

//=============================================================================
// MESSAGE SENDING
//=============================================================================

int surface_bio_async_send_geometry_update(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_geometry_update_t* update
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(update);

    BRIDGE_LOCK(bridge);
    int result = send_message_internal(
        bridge,
        BIO_MSG_SURFACE_GEOMETRY_UPDATE,
        update,
        sizeof(*update),
        bridge->config.default_channel,
        bridge->config.default_priority
    );
    BRIDGE_UNLOCK(bridge);

    return result;
}

int surface_bio_async_send_branch_formed(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_branch_formed_t* branch
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(branch);

    nimcp_bio_channel_type_t channel = bridge->config.default_channel;

    /* Trifurcations are significant - use reward channel */
    if (branch->branch_type == SURFACE_BRANCH_TRIFURCATION) {
        channel = bridge->config.reward_channel;
    }

    BRIDGE_LOCK(bridge);
    int result = send_message_internal(
        bridge,
        branch->branch_type == SURFACE_BRANCH_TRIFURCATION ?
            BIO_MSG_SURFACE_TRIFURCATION : BIO_MSG_SURFACE_BRANCH_FORMED,
        branch,
        sizeof(*branch),
        channel,
        bridge->config.default_priority
    );
    BRIDGE_UNLOCK(bridge);

    return result;
}

int surface_bio_async_send_optimization_done(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_optimization_done_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    /* Use reward channel for successful optimization */
    nimcp_bio_channel_type_t channel = result->converged ?
        bridge->config.reward_channel : bridge->config.default_channel;

    BRIDGE_LOCK(bridge);
    int ret = send_message_internal(
        bridge,
        BIO_MSG_SURFACE_OPTIMIZATION_DONE,
        result,
        sizeof(*result),
        channel,
        bridge->config.default_priority
    );
    BRIDGE_UNLOCK(bridge);

    return ret;
}

int surface_bio_async_send_anomaly(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_anomaly_t* anomaly
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(anomaly);

    /* Anomalies always use urgent channel with high priority */
    BRIDGE_LOCK(bridge);
    int result = send_message_internal(
        bridge,
        BIO_MSG_SURFACE_ANOMALY,
        anomaly,
        sizeof(*anomaly),
        bridge->config.urgent_channel,
        bridge->config.anomaly_priority
    );
    BRIDGE_UNLOCK(bridge);

    return result;
}

int surface_bio_async_send_material_update(
    surface_bio_async_bridge_t* bridge,
    const surface_bio_msg_material_update_t* update
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(update);

    BRIDGE_LOCK(bridge);
    int result = send_message_internal(
        bridge,
        BIO_MSG_SURFACE_MATERIAL_UPDATE,
        update,
        sizeof(*update),
        bridge->config.default_channel,
        bridge->config.default_priority
    );
    BRIDGE_UNLOCK(bridge);

    return result;
}

int surface_bio_async_send_raw(
    surface_bio_async_bridge_t* bridge,
    bio_msg_surface_type_t msg_type,
    const void* payload,
    size_t payload_size,
    nimcp_bio_channel_type_t channel,
    uint8_t priority
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(payload);

    BRIDGE_LOCK(bridge);
    int result = send_message_internal(
        bridge,
        msg_type,
        payload,
        payload_size,
        channel,
        priority
    );
    BRIDGE_UNLOCK(bridge);

    return result;
}

//=============================================================================
// MESSAGE PROCESSING
//=============================================================================

int surface_bio_async_process_messages(
    surface_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    BRIDGE_NULL_CHECK(bridge);

    if (bridge->paused) return 0;

    BRIDGE_LOCK(bridge);

    /* In a full implementation, this would poll the router for messages
     * and dispatch to callbacks. For now, just return 0. */
    (void)max_messages;

    int processed = 0;

    BRIDGE_UNLOCK(bridge);
    return processed;
}

int surface_bio_async_flush(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Send any pending messages */
    bridge->pending_count = 0;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int surface_bio_async_pause(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->paused = true;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int surface_bio_async_resume(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->paused = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

//=============================================================================
// STATISTICS
//=============================================================================

int surface_bio_async_get_stats(
    const surface_bio_async_bridge_t* bridge,
    surface_bio_async_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    stats->messages_sent = bridge->messages_sent;
    stats->messages_received = bridge->messages_received;
    stats->messages_dropped = bridge->messages_dropped;
    stats->bytes_sent = bridge->bytes_sent;
    stats->bytes_received = bridge->bytes_received;
    stats->active_subscriptions = bridge->num_subscriptions;
    stats->pending_messages = bridge->pending_count;
    stats->connected = bridge->connected;
    stats->paused = bridge->paused;

    return 0;
}

int surface_bio_async_reset_stats(surface_bio_async_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->messages_sent = 0;
    bridge->messages_received = 0;
    bridge->messages_dropped = 0;
    bridge->bytes_sent = 0;
    bridge->bytes_received = 0;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

//=============================================================================
// UTILITY
//=============================================================================

const char* surface_bio_msg_type_name_async(bio_msg_surface_type_t msg_type) {
    if (msg_type >= BIO_MSG_SURFACE_GEOMETRY_UPDATE &&
        msg_type <= BIO_MSG_SURFACE_REGION_STATS) {
        return MSG_TYPE_NAMES[msg_type - 0x1400];
    }
    return "UNKNOWN";
}

const char* surface_bio_channel_name(nimcp_bio_channel_type_t channel) {
    if (channel < sizeof(CHANNEL_NAMES) / sizeof(CHANNEL_NAMES[0])) {
        return CHANNEL_NAMES[channel];
    }
    return "UNKNOWN";
}
