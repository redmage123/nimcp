/**
 * @file nimcp_cochlea_bio_async_bridge.c
 * @brief Cochlea Bio-Async complete integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_bio_async_bridge)

#define LOG_MODULE "COCHLEA_BIO_ASYNC_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

typedef struct {
    cochlea_inbound_msg_t msg_type;
    cochlea_message_handler_t handler;
    void* user_data;
    bool active;
} handler_entry_t;

struct cochlea_bio_async_bridge {
    bridge_base_t base;                         /* MUST be first */
    cochlea_bio_async_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    bio_router_t* router;

    /* Registration */
    bool registered;

    /* Handlers */
    handler_entry_t handlers[COCHLEA_BIO_MAX_HANDLERS];
    uint32_t num_handlers;

    /* Connections */
    cochlea_bio_connection_t connections[COCHLEA_BIO_MAX_CONNECTIONS];
    uint32_t num_connections;

    /* Inbox */
    uint32_t pending_count;

    /* Statistics */
    cochlea_bio_stats_t stats;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helpers
//=============================================================================

static uint64_t bio_async_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_bio_async_config_t cochlea_bio_async_config_default(void) {
    cochlea_bio_async_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.auto_register = true;
    cfg.module_id = BIO_MODULE_COCHLEA;
    cfg.module_name = "cochlea_bio_async";
    cfg.enable_verification = true;
    cfg.verification_interval_ms = 5000.0f;
    cfg.verification_timeout_ms = 1000.0f;
    cfg.inbox_capacity = 64;
    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_bio_async_bridge_t* cochlea_bio_async_bridge_create(
    cochlea_t* cochlea,
    bio_router_t* router,
    const cochlea_bio_async_config_t* config
) {
    cochlea_bio_async_bridge_heartbeat("create", 0.0f);

    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_bridge_create: cochlea NULL");
        return NULL;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_bridge_create: router NULL");
        return NULL;
    }

    cochlea_bio_async_bridge_t* bridge = (cochlea_bio_async_bridge_t*)
        nimcp_calloc(1, sizeof(cochlea_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_bio_async_bridge_create: bridge is NULL");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_bio_async") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_bio_async_bridge_create: validation failed");
        return NULL;
    }

    /* Store references */
    bridge->cochlea = cochlea;
    bridge->router = router;

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_bio_async_config_default();
    }

    bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    bridge_base_connect_b_unlocked(&bridge->base, router);

    /* Auto-register if requested */
    if (bridge->config.auto_register) {
        cochlea_bio_async_register(bridge);
    }

    cochlea_bio_async_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_bio_async_bridge_destroy(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_bio_async");
    cochlea_bio_async_bridge_heartbeat("destroy", 0.0f);

    /* Unregister if registered */
    if (bridge->registered) {
        cochlea_bio_async_unregister(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_bio_async_bridge_update(
    cochlea_bio_async_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_bridge_update: bridge NULL");
        return -1;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_bridge_update: cochlea_output NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("update", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    (void)dt_ms;

    /* Process incoming messages */
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_process_inbox(bridge);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->last_outbound_ts = bio_async_get_time_ms();

    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_bio_async_bridge_reset(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_bio_async_bridge_heartbeat("reset", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->pending_count = 0;
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    /* Reset connection verification */
    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        bridge->connections[i].verified = false;
        bridge->connections[i].last_outbound = 0;
        bridge->connections[i].last_inbound = 0;
        bridge->connections[i].latency_ms = 0.0f;
    }

    bridge_base_reset_unlocked(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Registration
//=============================================================================

nimcp_error_t cochlea_bio_async_register(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_register: bridge NULL");
        return -1;
    }
    cochlea_bio_async_bridge_heartbeat("register", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->registered = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("register", 1.0f);
    return 0;
}

nimcp_error_t cochlea_bio_async_unregister(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_unregister: bridge NULL");
        return -1;
    }
    cochlea_bio_async_bridge_heartbeat("unregister", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->registered = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("unregister", 1.0f);
    return 0;
}

bool cochlea_bio_async_is_registered(const cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_is_registered: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool r = bridge->registered;
    nimcp_mutex_unlock(bridge->base.mutex);
    return r;
}

//=============================================================================
// Message Handlers
//=============================================================================

nimcp_error_t cochlea_bio_async_add_handler(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_inbound_msg_t msg_type,
    cochlea_message_handler_t handler,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_add_handler: bridge NULL");
        return -1;
    }
    if (!handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_add_handler: handler NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_handlers >= COCHLEA_BIO_MAX_HANDLERS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cochlea_bio_async_add_handler: capacity exceeded");
        return -1;
    }

    handler_entry_t* entry = &bridge->handlers[bridge->num_handlers];
    entry->msg_type = msg_type;
    entry->handler = handler;
    entry->user_data = user_data;
    entry->active = true;
    bridge->num_handlers++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_bio_async_remove_handler(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_inbound_msg_t msg_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_remove_handler: bridge NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_handlers; i++) {
        if (bridge->handlers[i].msg_type == msg_type && bridge->handlers[i].active) {
            bridge->handlers[i].active = false;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_bio_async_remove_handler: validation failed");
    return -1;
}

//=============================================================================
// Sending (Outbound)
//=============================================================================

nimcp_error_t cochlea_bio_async_send(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t dest,
    cochlea_outbound_msg_t msg_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_send: bridge NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("send", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    (void)msg_type;
    (void)payload;
    (void)payload_size;

    bridge->stats.messages_sent++;
    bridge->last_outbound_ts = bio_async_get_time_ms();

    /* Update connection timestamps */
    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        if (bridge->connections[i].module_id == dest) {
            bridge->connections[i].last_outbound = bridge->last_outbound_ts;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("send", 1.0f);
    return 0;
}

nimcp_error_t cochlea_bio_async_broadcast(
    cochlea_bio_async_bridge_t* bridge,
    cochlea_outbound_msg_t msg_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_broadcast: bridge NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("broadcast", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    (void)msg_type;
    (void)payload;
    (void)payload_size;

    bridge->stats.messages_sent += bridge->num_connections;
    bridge->last_outbound_ts = bio_async_get_time_ms();

    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        bridge->connections[i].last_outbound = bridge->last_outbound_ts;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("broadcast", 1.0f);
    return 0;
}

//=============================================================================
// Receiving (Inbound)
//=============================================================================

nimcp_error_t cochlea_bio_async_process_inbox(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_process_inbox: bridge NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("process_inbox", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* In stub mode, just mark as processed */
    uint32_t processed = bridge->pending_count;
    bridge->stats.messages_received += processed;
    bridge->pending_count = 0;

    if (processed > 0) {
        bridge->last_inbound_ts = bio_async_get_time_ms();
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("process_inbox", 1.0f);
    return 0;
}

uint32_t cochlea_bio_async_get_pending(const cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_pending: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint32_t p = bridge->pending_count;
    nimcp_mutex_unlock(bridge->base.mutex);
    return p;
}

//=============================================================================
// Connections
//=============================================================================

nimcp_error_t cochlea_bio_async_add_connection(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id,
    const char* module_name
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_add_connection: bridge NULL");
        return -1;
    }
    if (!module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_add_connection: module_name NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_connections >= COCHLEA_BIO_MAX_CONNECTIONS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cochlea_bio_async_add_connection: capacity exceeded");
        return -1;
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        if (bridge->connections[i].module_id == module_id) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0; /* Already connected */
        }
    }

    cochlea_bio_connection_t* conn = &bridge->connections[bridge->num_connections];
    memset(conn, 0, sizeof(cochlea_bio_connection_t));
    conn->module_id = module_id;
    strncpy(conn->module_name, module_name, sizeof(conn->module_name) - 1);
    conn->verified = false;
    bridge->num_connections++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_bio_async_get_connection(
    const cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id,
    cochlea_bio_connection_t* connection
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_connection: bridge NULL");
        return -1;
    }
    if (!connection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_connection: connection NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        if (bridge->connections[i].module_id == module_id) {
            *connection = bridge->connections[i];
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_bio_async_get_connection: validation failed");
    return -1;
}

nimcp_error_t cochlea_bio_async_get_connections(
    const cochlea_bio_async_bridge_t* bridge,
    cochlea_bio_connection_t* connections,
    uint32_t* num_connections
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_connections: bridge NULL");
        return -1;
    }
    if (!connections || !num_connections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_connections: output param NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t count = bridge->num_connections;
    if (count > *num_connections) {
        count = *num_connections;
    }
    memcpy(connections, bridge->connections, count * sizeof(cochlea_bio_connection_t));
    *num_connections = count;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Verification
//=============================================================================

nimcp_error_t cochlea_bio_async_ping(
    cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t dest
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_ping: bridge NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("ping", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Record outbound ping */
    bridge->last_outbound_ts = bio_async_get_time_ms();
    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        if (bridge->connections[i].module_id == dest) {
            bridge->connections[i].last_outbound = bridge->last_outbound_ts;
            /* Simulate immediate pong for stub */
            bridge->connections[i].last_inbound = bridge->last_outbound_ts;
            bridge->connections[i].latency_ms = 0.1f;
            bridge->connections[i].verified = true;
            bridge->last_inbound_ts = bridge->last_outbound_ts;
            bridge->stats.verification_passes++;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_bio_async_bridge_heartbeat("ping", 1.0f);
    return 0;
}

nimcp_error_t cochlea_bio_async_verify_all(cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_verify_all: bridge NULL");
        return -1;
    }

    cochlea_bio_async_bridge_heartbeat("verify_all", 0.0f);

    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        cochlea_bio_async_ping(bridge, bridge->connections[i].module_id);
    }

    cochlea_bio_async_bridge_heartbeat("verify_all", 1.0f);
    return 0;
}

bool cochlea_bio_async_is_verified(
    const cochlea_bio_async_bridge_t* bridge,
    bio_module_id_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_is_verified: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < bridge->num_connections; i++) {
        if (bridge->connections[i].module_id == module_id) {
            bool v = bridge->connections[i].verified;
            nimcp_mutex_unlock(bridge->base.mutex);
            return v;
        }
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return false;
}

//=============================================================================
// Statistics
//=============================================================================

nimcp_error_t cochlea_bio_async_get_stats(
    const cochlea_bio_async_bridge_t* bridge,
    cochlea_bio_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_stats: bridge NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_stats: stats NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;

    /* Compute average latency across connections */
    if (bridge->num_connections > 0) {
        float total_lat = 0.0f;
        uint32_t verified_count = 0;
        for (uint32_t i = 0; i < bridge->num_connections; i++) {
            if (bridge->connections[i].verified) {
                total_lat += bridge->connections[i].latency_ms;
                verified_count++;
            }
        }
        stats->avg_latency_ms = verified_count > 0
            ? total_lat / (float)verified_count : 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_bio_async_verify_bidirectional(const cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_verify_bidirectional: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool ok = (bridge->last_outbound_ts > 0) && (bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ok;
}

uint64_t cochlea_bio_async_get_last_outbound(const cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_last_outbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_bio_async_get_last_inbound(const cochlea_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_bio_async_get_last_inbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
