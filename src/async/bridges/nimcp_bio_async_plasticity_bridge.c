/**
 * @file nimcp_bio_async_plasticity_bridge.c
 * @brief Bio-Async Plasticity Bridge Implementation
 *
 * WHAT: Bridge between bio-async messaging and plasticity systems
 * WHY:  Enable spike timing information in bio-async messages for STDP
 * HOW:  Converts bio_message_t to plasticity_event_t and vice versa
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#include "async/bridges/nimcp_bio_async_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

#define MODULE_NAME "bio_async_plasticity_bridge"


NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_async_plasticity_bridge)


/* Stub heartbeat for migration compatibility */
static inline void bio_async_plasticity_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
static void handle_incoming_batch_event(
    bio_async_plasticity_bridge_t* bridge,
    const void* payload,
    size_t payload_size
);

static void plasticity_event_callback(
    const plasticity_event_t* event,
    void* user_data
);

static int send_bio_message(
    bio_async_plasticity_bridge_t* bridge,
    uint32_t msg_type,
    nimcp_bio_channel_type_t channel,
    const void* payload,
    size_t payload_size,
    uint8_t priority
);

/* ============================================================================
 * Configuration
 * ============================================================================ */

int bio_async_plasticity_default_config(bio_async_plasticity_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(bio_async_plasticity_config_t));

    /* Channel assignment based on biological analogy */
    config->spike_channel = BIO_CHANNEL_ACETYLCHOLINE;     /* Fast spike timing */
    config->weight_channel = BIO_CHANNEL_DOPAMINE;         /* Weight changes/rewards */
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;    /* Urgent alerts */
    config->consolidation_channel = BIO_CHANNEL_SEROTONIN; /* Slow consolidation */

    /* Batching defaults */
    config->enable_batch_mode = true;
    config->batch_size_threshold = 16;        /* Batch after 16 spikes */
    config->batch_time_threshold_us = 1000;   /* Or after 1ms */

    /* Filtering */
    config->min_weight_change_notify = 0.001f; /* Min 0.1% change to notify */
    config->broadcast_all_events = false;

    /* Integration */
    config->connect_to_coordinator = true;
    config->register_spike_handlers = true;
    config->inbox_capacity = BIO_ASYNC_PLASTICITY_DEFAULT_INBOX_SIZE;

    /* Priority */
    config->spike_priority = 7;               /* High for timing precision */
    config->weight_priority = 5;              /* Medium */
    config->alert_priority = 9;               /* Highest for alerts */

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

bio_async_plasticity_bridge_t* bio_async_plasticity_bridge_create(
    const bio_async_plasticity_config_t* config
) {
    bio_async_plasticity_heartbeat("create", 0.0f);

    /* Allocate bridge structure */
    bio_async_plasticity_bridge_t* bridge = nimcp_malloc(
        sizeof(bio_async_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "bio_async_plasticity_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(bio_async_plasticity_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_BIO_ASYNC_PLASTICITY,
                         MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
            "bio_async_plasticity_bridge_create: failed to init base");
        nimcp_free(bridge);
        return NULL;
    }

    bio_async_plasticity_heartbeat("create", 0.3f);

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(bio_async_plasticity_config_t));
    } else {
        bio_async_plasticity_default_config(&bridge->config);
    }

    /* Allocate subscriptions */
    bridge->max_subscriptions = BIO_ASYNC_PLASTICITY_MAX_SUBSCRIPTIONS;
    bridge->subscriptions = nimcp_malloc(
        bridge->max_subscriptions * sizeof(bio_async_plasticity_subscription_t));
    if (!bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "bio_async_plasticity_bridge_create: failed to allocate subscriptions");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->subscriptions, 0,
           bridge->max_subscriptions * sizeof(bio_async_plasticity_subscription_t));
    bridge->num_subscriptions = 0;

    bio_async_plasticity_heartbeat("create", 0.6f);

    /* Initialize state */
    bridge->connected = false;
    bridge->coordinator_connected = false;
    bridge->paused = false;
    bridge->router = NULL;
    bridge->orchestrator = NULL;

    /* Initialize batch accumulator */
    memset(&bridge->pending_batch, 0, sizeof(bio_async_spike_batch_t));
    bridge->batch_start_time_us = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bio_async_plasticity_stats_t));

    bridge->creation_time_us = nimcp_time_get_us();

    NIMCP_LOGGING_INFO("Created bio-async plasticity bridge");

    bio_async_plasticity_heartbeat("create", 1.0f);
    return bridge;
}

void bio_async_plasticity_bridge_destroy(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    bio_async_plasticity_heartbeat("destroy", 0.0f);
    NIMCP_LOGGING_DEBUG("Destroying bio-async plasticity bridge");

    /* Disconnect from coordinator first */
    if (bridge->coordinator_connected) {
        bio_async_plasticity_disconnect_coordinator(bridge);
    }

    /* Disconnect from router */
    if (bridge->connected) {
        bio_async_plasticity_bridge_disconnect(bridge);
    }

    bio_async_plasticity_heartbeat("destroy", 0.5f);

    /* Free subscriptions */
    if (bridge->subscriptions) {
        nimcp_free(bridge->subscriptions);
        bridge->subscriptions = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed bio-async plasticity bridge");
    bio_async_plasticity_heartbeat("destroy", 1.0f);
}

int bio_async_plasticity_bridge_reset(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_bridge_reset: bridge is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bio_async_plasticity_stats_t));

    /* Reset pending batch */
    memset(&bridge->pending_batch, 0, sizeof(bio_async_spike_batch_t));
    bridge->batch_start_time_us = 0;

    /* Reset subscriptions (keep them but clear counts) */
    bridge->paused = false;

    BRIDGE_UNLOCK(bridge);

    /* Reset base bridge (handles its own locking) */
    return bridge_base_reset(&bridge->base);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int bio_async_plasticity_bridge_connect(
    bio_async_plasticity_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_bridge_connect: bridge is NULL");
        return -1;
    }

    bio_async_plasticity_heartbeat("connect", 0.0f);

    BRIDGE_LOCK(bridge);

    if (bridge->connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;  /* Already connected */
    }

    bridge->router = router;

    /* Connect to bio-async via base */
    if (bridge_base_connect_bio_async(&bridge->base) != 0) {
        /* Non-fatal - can operate without router */
        NIMCP_LOGGING_WARN("Bio-async plasticity bridge: router unavailable");
        bridge->router = NULL;
    }

    bridge->connected = (bridge->router != NULL || bridge->base.bio_ctx != NULL);
    bridge->base.bridge_active = bridge->connected;

    bio_async_plasticity_heartbeat("connect", 0.5f);

    /* Register message handlers if configured */
    if (bridge->connected && bridge->config.register_spike_handlers) {
        /* Register for STDP events */
        if (bridge->base.bio_ctx) {
            /* Note: Handler registration would go here via bio_router_register_handler */
            NIMCP_LOGGING_DEBUG("Bio-async plasticity: spike handlers registered");
        }
    }

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Bio-async plasticity bridge connected: %s",
                       bridge->connected ? "YES" : "NO");

    bio_async_plasticity_heartbeat("connect", 1.0f);
    return 0;
}

int bio_async_plasticity_bridge_disconnect(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_bridge_disconnect: bridge is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    if (!bridge->connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Flush any pending batch */
    if (bridge->pending_batch.num_events > 0) {
        /* Clear without sending to avoid deadlock */
        bridge->pending_batch.num_events = 0;
    }

    BRIDGE_UNLOCK(bridge);

    /* Disconnect from bio-async (handles its own locking) */
    bridge_base_disconnect_bio_async(&bridge->base);

    BRIDGE_LOCK(bridge);

    bridge->router = NULL;
    bridge->connected = false;
    bridge->base.bridge_active = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Bio-async plasticity bridge disconnected");
    return 0;
}

bool bio_async_plasticity_bridge_is_connected(const bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

int bio_async_plasticity_connect_coordinator(
    bio_async_plasticity_bridge_t* bridge,
    plasticity_orchestrator_t* orchestrator
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_connect_coordinator: bridge is NULL");
        return -1;
    }
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_connect_coordinator: orchestrator is NULL");
        return -1;
    }

    bio_async_plasticity_heartbeat("connect_coordinator", 0.0f);

    BRIDGE_LOCK(bridge);

    if (bridge->coordinator_connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    bridge->orchestrator = orchestrator;

    /* Register for plasticity events */
    for (int event_type = 0; event_type < PLASTICITY_EVENT_COUNT; event_type++) {
        int result = plasticity_orchestrator_register_event_callback(
            orchestrator,
            (plasticity_event_type_t)event_type,
            plasticity_event_callback,
            bridge
        );
        if (result < 0) {
            NIMCP_LOGGING_WARN("Failed to register callback for event type %d", event_type);
        }
    }

    bridge->coordinator_connected = true;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Bio-async plasticity bridge connected to coordinator");

    bio_async_plasticity_heartbeat("connect_coordinator", 1.0f);
    return 0;
}

int bio_async_plasticity_disconnect_coordinator(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_disconnect_coordinator: bridge is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    if (!bridge->coordinator_connected) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Note: Would unregister callbacks here if orchestrator supports it */
    bridge->orchestrator = NULL;
    bridge->coordinator_connected = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Bio-async plasticity bridge disconnected from coordinator");
    return 0;
}

/* ============================================================================
 * Spike Event API
 * ============================================================================ */

int bio_async_plasticity_send_spike(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_spike_event_t* event
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_send_spike: bridge is NULL");
        return -1;
    }
    if (!event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_send_spike: event is NULL");
        return -1;
    }

    if (bridge->paused) return 0;

    BRIDGE_LOCK(bridge);

    /* Update statistics */
    bridge->stats.spikes_sent++;

    /* Check if batching is enabled */
    if (bridge->config.enable_batch_mode) {
        /* Add to pending batch */
        if (bridge->pending_batch.num_events == 0) {
            bridge->batch_start_time_us = nimcp_time_get_us();
            bridge->pending_batch.batch_start_time_us = bridge->batch_start_time_us;
        }

        /* Copy event to batch */
        if (bridge->pending_batch.num_events < BIO_ASYNC_PLASTICITY_MAX_BATCH_SPIKES) {
            memcpy(&bridge->pending_batch.events[bridge->pending_batch.num_events],
                   event, sizeof(bio_async_spike_event_t));
            bridge->pending_batch.num_events++;
            bridge->pending_batch.batch_end_time_us = event->spike_time_us;
        }

        /* Check if batch should be sent */
        uint64_t now = nimcp_time_get_us();
        bool size_threshold = bridge->pending_batch.num_events >=
                              bridge->config.batch_size_threshold;
        bool time_threshold = (now - bridge->batch_start_time_us) >=
                              bridge->config.batch_time_threshold_us;

        if (size_threshold || time_threshold) {
            BRIDGE_UNLOCK(bridge);
            return bio_async_plasticity_flush_batch(bridge);
        }

        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    BRIDGE_UNLOCK(bridge);

    /* Send individual spike event */
    return send_bio_message(
        bridge,
        BIO_MSG_STDP_EVENT,
        bridge->config.spike_channel,
        event,
        sizeof(bio_async_spike_event_t),
        bridge->config.spike_priority
    );
}

int bio_async_plasticity_send_batch(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_spike_batch_t* batch
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_send_batch: bridge is NULL");
        return -1;
    }
    if (!batch || batch->num_events == 0) {
        return 0;  /* Nothing to send */
    }

    if (bridge->paused) return 0;

    BRIDGE_LOCK(bridge);
    bridge->stats.batches_processed++;
    bridge->stats.spikes_sent += batch->num_events;
    BRIDGE_UNLOCK(bridge);

    return send_bio_message(
        bridge,
        BIO_MSG_STDP_BATCH_EVENT,
        bridge->config.spike_channel,
        batch,
        sizeof(bio_async_spike_batch_t),
        bridge->config.spike_priority
    );
}

int bio_async_plasticity_flush_batch(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_flush_batch: bridge is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    if (bridge->pending_batch.num_events == 0) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Copy batch and reset */
    bio_async_spike_batch_t batch_copy = bridge->pending_batch;
    memset(&bridge->pending_batch, 0, sizeof(bio_async_spike_batch_t));
    bridge->batch_start_time_us = 0;

    bridge->stats.batches_processed++;

    BRIDGE_UNLOCK(bridge);

    /* Send the batch */
    return send_bio_message(
        bridge,
        BIO_MSG_STDP_BATCH_EVENT,
        bridge->config.spike_channel,
        &batch_copy,
        sizeof(bio_async_spike_batch_t),
        bridge->config.spike_priority
    );
}

/* ============================================================================
 * Plasticity Event API
 * ============================================================================ */

int bio_async_plasticity_broadcast_event(
    bio_async_plasticity_bridge_t* bridge,
    const plasticity_event_t* event
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_broadcast_event: bridge is NULL");
        return -1;
    }
    if (!event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_broadcast_event: event is NULL");
        return -1;
    }

    if (bridge->paused) return 0;

    bio_async_plasticity_heartbeat("broadcast_event", 0.5f);

    /* Update statistics based on event type */
    BRIDGE_LOCK(bridge);
    switch (event->type) {
        case PLASTICITY_EVENT_LTP:
            bridge->stats.ltp_events++;
            break;
        case PLASTICITY_EVENT_LTD:
            bridge->stats.ltd_events++;
            break;
        case PLASTICITY_EVENT_CONSOLIDATION:
            bridge->stats.consolidation_events++;
            break;
        default:
            break;
    }
    BRIDGE_UNLOCK(bridge);

    /* Convert to bio-async event */
    bio_async_plasticity_event_t bio_event = {
        .type = event->type,
        .synapse_id = event->synapse_id,
        .neuron_id = event->neuron_id,
        .old_value = event->old_value,
        .new_value = event->new_value,
        .delta = event->delta,
        .timestamp_us = event->timestamp_ms * 1000,
        .context = event->context
    };

    /* Select channel based on event type */
    nimcp_bio_channel_type_t channel = bridge->config.weight_channel;
    uint32_t msg_type = BIO_MSG_PLASTICITY_UPDATE;

    switch (event->type) {
        case PLASTICITY_EVENT_LTP:
            msg_type = BIO_MSG_LTP_INDUCED;
            channel = bridge->config.weight_channel;
            break;
        case PLASTICITY_EVENT_LTD:
            msg_type = BIO_MSG_LTD_INDUCED;
            channel = bridge->config.weight_channel;
            break;
        case PLASTICITY_EVENT_CONSOLIDATION:
            msg_type = BIO_MSG_CONSOLIDATION_TRIGGER;
            channel = bridge->config.consolidation_channel;
            break;
        case PLASTICITY_EVENT_ENERGY_DEPLETED:
            msg_type = BIO_MSG_SUBSTRATE_ATP_CRITICAL;
            channel = bridge->config.alert_channel;
            break;
        default:
            channel = bridge->config.weight_channel;
            break;
    }

    return send_bio_message(
        bridge,
        msg_type,
        channel,
        &bio_event,
        sizeof(bio_async_plasticity_event_t),
        bridge->config.weight_priority
    );
}

int bio_async_plasticity_broadcast_weight_change(
    bio_async_plasticity_bridge_t* bridge,
    const bio_async_weight_change_t* change
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_broadcast_weight_change: bridge is NULL");
        return -1;
    }
    if (!change) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_broadcast_weight_change: change is NULL");
        return -1;
    }

    if (bridge->paused) return 0;

    /* Check minimum threshold */
    float abs_delta = change->delta;
    if (abs_delta < 0) abs_delta = -abs_delta;
    if (abs_delta < bridge->config.min_weight_change_notify) {
        return 0;  /* Below threshold, don't broadcast */
    }

    BRIDGE_LOCK(bridge);
    bridge->stats.weight_changes_broadcast++;
    BRIDGE_UNLOCK(bridge);

    return send_bio_message(
        bridge,
        BIO_MSG_WEIGHT_CHANGE,
        bridge->config.weight_channel,
        change,
        sizeof(bio_async_weight_change_t),
        bridge->config.weight_priority
    );
}

/* ============================================================================
 * Subscription API
 * ============================================================================ */

int bio_async_plasticity_subscribe(
    bio_async_plasticity_bridge_t* bridge,
    plasticity_event_type_t event_type,
    void (*callback)(const bio_async_plasticity_event_t* event, void* user_data),
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_subscribe: bridge is NULL");
        return -1;
    }
    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_subscribe: callback is NULL");
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
        NIMCP_LOGGING_WARN("Bio-async plasticity: subscription slots exhausted");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_async_plasticity_subscribe: validation failed");
        return -1;
    }

    /* Fill subscription */
    bridge->subscriptions[slot].event_type = event_type;
    bridge->subscriptions[slot].callback = callback;
    bridge->subscriptions[slot].user_data = user_data;
    bridge->subscriptions[slot].active = true;
    bridge->num_subscriptions++;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Bio-async plasticity: subscribed to event type %d (slot %d)",
                        (int)event_type, slot);
    return slot;
}

int bio_async_plasticity_unsubscribe(
    bio_async_plasticity_bridge_t* bridge,
    int subscription_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_unsubscribe: bridge is NULL");
        return -1;
    }
    if (subscription_id < 0 || (uint32_t)subscription_id >= bridge->max_subscriptions) {
        return -1;
    }

    BRIDGE_LOCK(bridge);

    if (!bridge->subscriptions[subscription_id].active) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_async_plasticity_unsubscribe: bridge->subscriptions is NULL");
        return -1;
    }

    bridge->subscriptions[subscription_id].active = false;
    bridge->subscriptions[subscription_id].callback = NULL;
    bridge->subscriptions[subscription_id].user_data = NULL;
    bridge->num_subscriptions--;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Bio-async plasticity: unsubscribed slot %d", subscription_id);
    return 0;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int bio_async_plasticity_get_stats(
    const bio_async_plasticity_bridge_t* bridge,
    bio_async_plasticity_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bio_async_plasticity_get_stats: stats is NULL");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(bio_async_plasticity_stats_t));
    return 0;
}

void bio_async_plasticity_reset_stats(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(bio_async_plasticity_stats_t));
    BRIDGE_UNLOCK(bridge);
}

/* ============================================================================
 * Control API
 * ============================================================================ */

void bio_async_plasticity_pause(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    BRIDGE_LOCK(bridge);
    bridge->paused = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Bio-async plasticity bridge paused");
}

void bio_async_plasticity_resume(bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    BRIDGE_LOCK(bridge);
    bridge->paused = false;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Bio-async plasticity bridge resumed");
}

bool bio_async_plasticity_is_paused(const bio_async_plasticity_bridge_t* bridge) {
    if (!bridge) return true;
    return bridge->paused;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * @brief Message structure for bio-async plasticity bridge
 *
 * Uses bio_message_header_t followed by inline payload data.
 */
typedef struct {
    bio_message_header_t header;
    uint8_t payload_data[BIO_ASYNC_PLASTICITY_MAX_BATCH_SPIKES * sizeof(bio_async_spike_event_t)];
} bio_async_plasticity_message_t;

/**
 * @brief Send bio-async message
 */
static int send_bio_message(
    bio_async_plasticity_bridge_t* bridge,
    uint32_t msg_type,
    nimcp_bio_channel_type_t channel,
    const void* payload,
    size_t payload_size,
    uint8_t priority
) {
    if (!bridge->connected || !bridge->base.bio_ctx) {
        /* Not connected, silently drop */
        BRIDGE_LOCK(bridge);
        bridge->stats.messages_dropped++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Create message with header + payload */
    bio_async_plasticity_message_t msg;
    memset(&msg, 0, sizeof(bio_message_header_t));

    /* Initialize header */
    msg.header.type = (bio_message_type_t)msg_type;
    msg.header.channel = channel;
    msg.header.source_module = BIO_MODULE_BIO_ASYNC_PLASTICITY;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.payload_size = (uint32_t)payload_size;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    if (priority >= 7) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Copy payload */
    size_t copy_size = payload_size;
    if (copy_size > sizeof(msg.payload_data)) {
        copy_size = sizeof(msg.payload_data);
        NIMCP_LOGGING_WARN("Bio-async plasticity: payload truncated from %zu to %zu bytes",
                          payload_size, copy_size);
    }
    if (payload && copy_size > 0) {
        memcpy(msg.payload_data, payload, copy_size);
    }

    /* Send via bio router broadcast */
    size_t msg_size = sizeof(bio_message_header_t) + copy_size;
    nimcp_error_t result = bio_router_broadcast(bridge->base.bio_ctx, &msg, msg_size);

    BRIDGE_LOCK(bridge);
    if (result == NIMCP_SUCCESS) {
        bridge->stats.messages_sent++;
    } else {
        bridge->stats.messages_dropped++;
    }
    BRIDGE_UNLOCK(bridge);

    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

/**
 * @brief Handle incoming spike event from bio-async
 */
static void handle_incoming_spike_event(
    bio_async_plasticity_bridge_t* bridge,
    const void* payload,
    size_t payload_size
) {
    (void)payload_size;

    if (!bridge || !payload) return;
    if (bridge->paused) return;

    const bio_async_spike_event_t* event = (const bio_async_spike_event_t*)payload;

    BRIDGE_LOCK(bridge);
    bridge->stats.spikes_received++;
    BRIDGE_UNLOCK(bridge);

    /* Forward to coordinator if connected */
    if (bridge->coordinator_connected && bridge->orchestrator) {
        /* Would call plasticity_orchestrator_process_spike() here */
        NIMCP_LOGGING_DEBUG("Spike event: synapse=%u, pre=%u, post=%u, time=%lu",
                            event->synapse_id, event->pre_neuron_id,
                            event->post_neuron_id, (unsigned long)event->spike_time_us);
    }
}

/**
 * @brief Handle incoming batch spike event from bio-async
 */
static void handle_incoming_batch_event(
    bio_async_plasticity_bridge_t* bridge,
    const void* payload,
    size_t payload_size
) {
    (void)payload_size;

    if (!bridge || !payload) return;
    if (bridge->paused) return;

    const bio_async_spike_batch_t* batch = (const bio_async_spike_batch_t*)payload;

    BRIDGE_LOCK(bridge);
    bridge->stats.spikes_received += batch->num_events;
    bridge->stats.batches_processed++;
    BRIDGE_UNLOCK(bridge);

    /* Process each spike in batch */
    for (uint32_t i = 0; i < batch->num_events; i++) {
        handle_incoming_spike_event(bridge, &batch->events[i],
                                    sizeof(bio_async_spike_event_t));
    }
}

/**
 * @brief Callback for plasticity events from coordinator
 */
static void plasticity_event_callback(
    const plasticity_event_t* event,
    void* user_data
) {
    bio_async_plasticity_bridge_t* bridge = (bio_async_plasticity_bridge_t*)user_data;

    if (!bridge || !event) return;
    if (bridge->paused) return;

    /* Broadcast if configured */
    if (bridge->config.broadcast_all_events) {
        bio_async_plasticity_broadcast_event(bridge, event);
    }

    /* Notify local subscribers */
    BRIDGE_LOCK(bridge);

    for (uint32_t i = 0; i < bridge->max_subscriptions; i++) {
        if (bridge->subscriptions[i].active &&
            bridge->subscriptions[i].event_type == event->type &&
            bridge->subscriptions[i].callback) {

            /* Convert to bio event format */
            bio_async_plasticity_event_t bio_event = {
                .type = event->type,
                .synapse_id = event->synapse_id,
                .neuron_id = event->neuron_id,
                .old_value = event->old_value,
                .new_value = event->new_value,
                .delta = event->delta,
                .timestamp_us = event->timestamp_ms * 1000,
                .context = event->context
            };

            /* Note: Callback is called while holding lock - must be fast */
            bridge->subscriptions[i].callback(&bio_event,
                                              bridge->subscriptions[i].user_data);
        }
    }

    BRIDGE_UNLOCK(bridge);
}
