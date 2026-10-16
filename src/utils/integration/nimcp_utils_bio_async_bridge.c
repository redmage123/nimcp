/**
 * @file nimcp_utils_bio_async_bridge.c
 * @brief Implementation of Utils Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of the utils bio-async bridge for centralized
 *       logging, memory tracking, timer events, and service coordination.
 *
 * WHY: Provides a unified interface for utility services to communicate
 *      with other modules via bio-async messaging.
 *
 * HOW: Implements message creation, routing, subscription management,
 *      and periodic updates.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/integration/nimcp_utils_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Try to include nimcp_memory if available, otherwise use stdlib */
#ifdef NIMCP_HAS_MEMORY_H
#include "utils/memory/nimcp_memory.h"

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(utils_bio_async_bridge)

#define LOG_MODULE "UTILS_BIO_ASYNC_BRIDGE"

#define UTILS_MALLOC(size) nimcp_malloc(size)
#define UTILS_CALLOC(n, size) nimcp_calloc(n, size)
#define UTILS_FREE(ptr) nimcp_free(ptr)
#else
#define UTILS_MALLOC(size) nimcp_malloc(size)
#define UTILS_CALLOC(n, size) nimcp_calloc(n, size)
#define UTILS_FREE(ptr) nimcp_free(ptr)
#endif

/* ============================================================================
 * Module ID for Utils Bridge
 * ============================================================================ */

#define UTILS_BIO_MODULE_ID 0x5001  /* Utils bio-async module ID */

/* ============================================================================
 * Internal State
 * ============================================================================ */

typedef struct {
    size_t total_allocated;             /**< Total bytes allocated */
    size_t total_capacity;              /**< Total pool capacity */
    bool memory_warning_sent;           /**< Warning sent flag */
    bool memory_critical_sent;          /**< Critical sent flag */
    uint32_t next_sequence_id;          /**< Next message sequence ID */
    uint32_t heartbeat_sequence;        /**< Heartbeat sequence number */
} utils_internal_state_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

struct utils_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    utils_bio_bridge_config_t config;
    bio_router_t router;

    utils_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    bool initialized;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;
    uint32_t time_since_heartbeat_ms;

    utils_internal_state_t internal;
    utils_bio_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint32_t get_next_sequence_id(utils_bio_bridge_t* bridge) {
    return bridge->internal.next_sequence_id++;
}

static utils_bio_subscription_t* find_subscription(
    utils_bio_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge || !bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (bridge, bridge->subscriptions)");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id &&
            bridge->subscriptions[i].active) {
            return &bridge->subscriptions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (bridge, bridge->subscriptions)");
    return NULL;
}

static void init_header(
    utils_bio_msg_header_t* header,
    utils_bio_bridge_t* bridge,
    utils_bio_msg_type_t type,
    utils_bio_channel_type_t channel,
    uint32_t flags
) {
    header->type = type;
    header->sequence_id = get_next_sequence_id(bridge);
    header->source_module = UTILS_BIO_MODULE_ID;
    header->target_module = 0;  /* Broadcast */
    header->timestamp_us = get_timestamp_us();
    header->channel = channel;
    header->payload_size = 0;  /* Set by caller */
    header->flags = flags;
}

static void safe_strncpy(char* dest, const char* src, size_t n) {
    if (!dest || n == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int utils_bio_bridge_default_config(utils_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->broadcast_interval_ms = UTILS_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = UTILS_BIO_MESSAGE_TTL_MS;

    config->default_channel = UTILS_BIO_CHANNEL_SEROTONIN;  /* Slow, steady */
    config->alert_channel = UTILS_BIO_CHANNEL_NOREPINEPHRINE;  /* Urgent */
    config->log_channel = UTILS_BIO_CHANNEL_ACETYLCHOLINE;  /* Fast */

    config->max_subscriptions = UTILS_BIO_MAX_SUBSCRIPTIONS;

    config->memory_warning_threshold = UTILS_BIO_MEMORY_WARNING_THRESHOLD;
    config->memory_critical_threshold = UTILS_BIO_MEMORY_CRITICAL_THRESHOLD;

    config->min_log_level = UTILS_LOG_LEVEL_INFO;
    config->enable_log_broadcast = true;
    config->enable_file_info = false;

    config->enable_memory_tracking = true;
    config->enable_timer_tracking = true;
    config->enable_metrics_broadcast = true;
    config->enable_service_coordination = true;
    config->enable_heartbeat = true;
    config->heartbeat_interval_ms = 1000;

    config->enable_debug_logging = false;

    return 0;
}

int utils_bio_bridge_validate_config(const utils_bio_bridge_config_t* config) {
    if (!config) return UTILS_BIO_ERROR_INVALID_PARAM;

    if (config->max_subscriptions == 0) {
        return UTILS_BIO_ERROR_INVALID_PARAM;
    }

    if (config->memory_warning_threshold < 0.0f ||
        config->memory_warning_threshold > 1.0f) {
        return UTILS_BIO_ERROR_INVALID_PARAM;
    }

    if (config->memory_critical_threshold < 0.0f ||
        config->memory_critical_threshold > 1.0f) {
        return UTILS_BIO_ERROR_INVALID_PARAM;
    }

    if (config->memory_warning_threshold > config->memory_critical_threshold) {
        return UTILS_BIO_ERROR_INVALID_PARAM;
    }

    if (config->min_log_level >= UTILS_LOG_LEVEL_COUNT) {
        return UTILS_BIO_ERROR_INVALID_PARAM;
    }

    return 0;
}

utils_bio_bridge_t* utils_bio_bridge_create(
    const utils_bio_bridge_config_t* config
) {
    utils_bio_bridge_t* bridge = UTILS_CALLOC(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    if (utils_bio_bridge_init(bridge, config) != 0) {
        UTILS_FREE(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "utils_bio_bridge_create: validation failed");
        return NULL;
    }

    return bridge;
}

int utils_bio_bridge_init(
    utils_bio_bridge_t* bridge,
    const utils_bio_bridge_config_t* config
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_init: bridge is NULL");
        return -1;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        if (utils_bio_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "utils_bio_bridge_init: validation failed");
            return -1;
        }
        bridge->config = *config;
    } else {
        utils_bio_bridge_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = UTILS_CALLOC(
        bridge->subscription_capacity,
        sizeof(utils_bio_subscription_t)
    );

    if (!bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_init: bridge->subscriptions is NULL");
        return -1;
    }

    bridge->last_broadcast_us = get_timestamp_us();
    bridge->initialized = true;

    return 0;
}

void utils_bio_bridge_destroy(utils_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "utils_bio_async");

    if (bridge->connected) {
        utils_bio_bridge_disconnect(bridge);
    }

    if (bridge->subscriptions) {
        UTILS_FREE(bridge->subscriptions);
        bridge->subscriptions = NULL;
    }

    bridge->initialized = false;
    UTILS_FREE(bridge);
}

int utils_bio_bridge_reset(utils_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Preserve config, reset state */
    if (bridge->connected) {
        utils_bio_bridge_disconnect(bridge);
    }

    /* Clear subscriptions */
    if (bridge->subscriptions) {
        memset(bridge->subscriptions, 0,
               bridge->subscription_capacity * sizeof(utils_bio_subscription_t));
    }
    bridge->subscription_count = 0;

    /* Reset internal state */
    memset(&bridge->internal, 0, sizeof(bridge->internal));
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->last_broadcast_us = get_timestamp_us();
    bridge->time_since_broadcast_ms = 0;
    bridge->time_since_heartbeat_ms = 0;

    return 0;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int utils_bio_bridge_connect(
    utils_bio_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge || !bridge->initialized) {
        return UTILS_BIO_ERROR_NOT_INITIALIZED;
    }

    bridge->router = router;
    bridge->connected = true;

    /* Reset alert flags on connect */
    bridge->internal.memory_warning_sent = false;
    bridge->internal.memory_critical_sent = false;

    return 0;
}

int utils_bio_bridge_disconnect(utils_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool utils_bio_bridge_is_connected(const utils_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API Implementation
 * ============================================================================ */

int utils_bio_bridge_process_inbox(
    utils_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;  /* Placeholder - would process from router inbox */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int utils_bio_bridge_update(
    utils_bio_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;
    bridge->time_since_heartbeat_ms += delta_ms;

    /* Check for auto-broadcast */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.broadcast_interval_ms) {

        if (bridge->config.enable_metrics_broadcast) {
            /* Would broadcast current metrics */
        }

        bridge->time_since_broadcast_ms = 0;
    }

    /* Check for heartbeat */
    if (bridge->config.enable_heartbeat &&
        bridge->time_since_heartbeat_ms >= bridge->config.heartbeat_interval_ms) {

        utils_bio_bridge_send_heartbeat(
            bridge,
            UTILS_BIO_MODULE_ID,
            "utils_bridge",
            UTILS_SERVICE_STATUS_HEALTHY,
            0.0f
        );

        bridge->time_since_heartbeat_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Memory Event API Implementation
 * ============================================================================ */

int utils_bio_bridge_broadcast_memory_alloc(
    utils_bio_bridge_t* bridge,
    void* address,
    size_t size,
    uint32_t pool_id,
    const char* tag
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_memory_alloc: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_memory_alloc_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_MEMORY_ALLOC,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.address = address;
    msg.size = size;
    msg.alignment = 0;  /* Unknown */
    msg.pool_id = pool_id;
    msg.tag = tag;

    bridge->internal.total_allocated += size;
    msg.total_allocated = bridge->internal.total_allocated;
    msg.pool_capacity = bridge->internal.total_capacity;

    if (bridge->internal.total_capacity > 0) {
        msg.utilization = (float)bridge->internal.total_allocated /
                         (float)bridge->internal.total_capacity;
    }

    msg.source_module = UTILS_BIO_MODULE_ID;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.memory_events_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    return 0;
}

int utils_bio_bridge_broadcast_memory_free(
    utils_bio_bridge_t* bridge,
    void* address,
    size_t size,
    uint32_t pool_id
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_memory_free: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_memory_free_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_MEMORY_FREE,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.address = address;
    msg.size = size;
    msg.pool_id = pool_id;

    if (bridge->internal.total_allocated >= size) {
        bridge->internal.total_allocated -= size;
    }
    msg.total_allocated = bridge->internal.total_allocated;
    msg.pool_capacity = bridge->internal.total_capacity;

    if (bridge->internal.total_capacity > 0) {
        msg.utilization = (float)bridge->internal.total_allocated /
                         (float)bridge->internal.total_capacity;
    }

    msg.source_module = UTILS_BIO_MODULE_ID;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.memory_events_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int utils_bio_bridge_send_memory_pressure(
    utils_bio_bridge_t* bridge,
    uint32_t pool_id,
    float utilization,
    uint32_t severity
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_send_memory_pressure: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_memory_pressure_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_MEMORY_PRESSURE,
                bridge->config.alert_channel,
                UTILS_BIO_MSG_FLAG_BROADCAST | UTILS_BIO_MSG_FLAG_URGENT);

    msg.pool_id = pool_id;
    msg.utilization = utilization;
    msg.severity = severity;

    if (bridge->internal.total_capacity > 0) {
        msg.bytes_total = bridge->internal.total_capacity;
        msg.bytes_available = (size_t)((1.0f - utilization) *
                                        (float)bridge->internal.total_capacity);
    }

    msg.timestamp_us = get_timestamp_us();

    if (severity >= 1) {
        bridge->stats.memory_criticals++;
    } else {
        bridge->stats.memory_warnings++;
    }

    bridge->stats.messages_sent++;
    return 0;
}

int utils_bio_bridge_broadcast_pool_status(
    utils_bio_bridge_t* bridge,
    uint32_t pool_id,
    const char* pool_name,
    size_t capacity,
    size_t allocated
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "utils_bio_bridge_broadcast_pool_status: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_memory_pool_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_MEMORY_POOL_STATUS,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.pool_id = pool_id;
    safe_strncpy(msg.pool_name, pool_name, sizeof(msg.pool_name));
    msg.total_capacity = capacity;
    msg.bytes_allocated = allocated;
    msg.bytes_free = capacity > allocated ? capacity - allocated : 0;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Timer Event API Implementation
 * ============================================================================ */

int utils_bio_bridge_broadcast_timer_event(
    utils_bio_bridge_t* bridge,
    uint32_t timer_id,
    const char* timer_name,
    uint64_t scheduled_us,
    bool is_periodic
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_timer_event: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_timer_event_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_TIMER_EVENT,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.timer_id = timer_id;
    safe_strncpy(msg.timer_name, timer_name, sizeof(msg.timer_name));
    msg.scheduled_time_us = scheduled_us;
    msg.fired_time_us = get_timestamp_us();
    msg.drift_us = (int64_t)(msg.fired_time_us - scheduled_us);
    msg.is_periodic = is_periodic;
    msg.owner_module = UTILS_BIO_MODULE_ID;
    msg.timestamp_us = msg.fired_time_us;

    bridge->stats.timer_events_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int utils_bio_bridge_broadcast_timer_schedule(
    utils_bio_bridge_t* bridge,
    uint32_t timer_id,
    const char* timer_name,
    uint64_t fire_time_us,
    uint32_t interval_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_timer_schedule: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_timer_schedule_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_TIMER_SCHEDULE,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.timer_id = timer_id;
    safe_strncpy(msg.timer_name, timer_name, sizeof(msg.timer_name));
    msg.fire_time_us = fire_time_us;
    msg.interval_ms = interval_ms;
    msg.is_periodic = (interval_ms > 0);
    msg.owner_module = UTILS_BIO_MODULE_ID;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.timer_events_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Logging API Implementation
 * ============================================================================ */

int utils_bio_bridge_broadcast_log(
    utils_bio_bridge_t* bridge,
    utils_log_level_t level,
    const char* module_name,
    const char* message
) {
    return utils_bio_bridge_broadcast_log_ex(
        bridge, level, module_name, message, NULL, 0, NULL
    );
}

int utils_bio_bridge_broadcast_log_ex(
    utils_bio_bridge_t* bridge,
    utils_log_level_t level,
    const char* module_name,
    const char* message,
    const char* file,
    uint32_t line,
    const char* function
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_log_ex: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    /* Check minimum log level */
    if (level < bridge->config.min_log_level) {
        return 0;  /* Silently skip */
    }

    if (!bridge->config.enable_log_broadcast) {
        return 0;
    }

    utils_bio_log_entry_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Select appropriate message type based on level */
    utils_bio_msg_type_t msg_type;
    uint32_t flags = UTILS_BIO_MSG_FLAG_BROADCAST;

    switch (level) {
        case UTILS_LOG_LEVEL_ERROR:
        case UTILS_LOG_LEVEL_FATAL:
            msg_type = UTILS_MSG_LOG_ERROR;
            flags |= UTILS_BIO_MSG_FLAG_URGENT;
            break;
        case UTILS_LOG_LEVEL_WARNING:
            msg_type = UTILS_MSG_LOG_WARNING;
            break;
        case UTILS_LOG_LEVEL_DEBUG:
        case UTILS_LOG_LEVEL_TRACE:
            msg_type = UTILS_MSG_LOG_DEBUG;
            break;
        default:
            msg_type = UTILS_MSG_LOG_ENTRY;
            break;
    }

    init_header(&msg.header, bridge, msg_type, bridge->config.log_channel, flags);

    msg.level = level;
    msg.source_module = UTILS_BIO_MODULE_ID;
    safe_strncpy(msg.module_name, module_name, sizeof(msg.module_name));
    safe_strncpy(msg.message, message, sizeof(msg.message));

    if (bridge->config.enable_file_info && file) {
        safe_strncpy(msg.file, file, sizeof(msg.file));
        msg.line = line;
        if (function) {
            safe_strncpy(msg.function, function, sizeof(msg.function));
        }
    }

    msg.timestamp_us = get_timestamp_us();

    bridge->stats.log_entries_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Metrics API Implementation
 * ============================================================================ */

int utils_bio_bridge_broadcast_metrics(
    utils_bio_bridge_t* bridge,
    const char* module_name,
    const utils_bio_metric_entry_t* metrics,
    uint32_t metric_count
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_metrics: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }
    if (!metrics || metric_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "utils_bio_bridge_broadcast_metrics: metrics is NULL");
        return -1;
    }

    if (metric_count > UTILS_BIO_MAX_METRICS_ENTRIES) {
        metric_count = UTILS_BIO_MAX_METRICS_ENTRIES;
    }

    utils_bio_metrics_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_METRICS,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.source_module = UTILS_BIO_MODULE_ID;
    safe_strncpy(msg.module_name, module_name, sizeof(msg.module_name));
    msg.metric_count = metric_count;

    memcpy(msg.metrics, metrics, metric_count * sizeof(utils_bio_metric_entry_t));

    msg.period_start_us = bridge->last_broadcast_us;
    msg.period_end_us = get_timestamp_us();
    msg.timestamp_us = msg.period_end_us;

    bridge->stats.metrics_broadcasts++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Service Coordination API Implementation
 * ============================================================================ */

int utils_bio_bridge_broadcast_service_status(
    utils_bio_bridge_t* bridge,
    uint32_t service_id,
    const char* service_name,
    utils_service_status_t status,
    float health_score
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_broadcast_service_status: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_service_status_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_SERVICE_STATUS,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.service_id = service_id;
    safe_strncpy(msg.service_name, service_name, sizeof(msg.service_name));
    msg.status = status;
    msg.health_score = health_score;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.service_updates++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

int utils_bio_bridge_register_service(
    utils_bio_bridge_t* bridge,
    uint32_t service_id,
    const char* service_name,
    const char* service_type,
    uint32_t capabilities
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_register_service: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_service_register_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_SERVICE_REGISTER,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.service_id = service_id;
    safe_strncpy(msg.service_name, service_name, sizeof(msg.service_name));
    safe_strncpy(msg.service_type, service_type, sizeof(msg.service_type));
    msg.module_id = UTILS_BIO_MODULE_ID;
    msg.capabilities = capabilities;
    msg.version = 1;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.registered_services++;
    bridge->stats.messages_sent++;
    return 0;
}

int utils_bio_bridge_send_heartbeat(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    const char* module_name,
    utils_service_status_t status,
    float load
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_send_heartbeat: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    utils_bio_heartbeat_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    init_header(&msg.header, bridge, UTILS_MSG_HEARTBEAT,
                bridge->config.default_channel, UTILS_BIO_MSG_FLAG_BROADCAST);

    msg.module_id = module_id;
    safe_strncpy(msg.module_name, module_name, sizeof(msg.module_name));
    msg.status = status;
    msg.load = load;
    msg.messages_processed = bridge->stats.messages_received;
    msg.errors = bridge->stats.handler_errors;
    msg.heartbeat_sequence = bridge->internal.heartbeat_sequence++;
    msg.last_activity_us = bridge->last_broadcast_us;
    msg.timestamp_us = get_timestamp_us();

    bridge->stats.heartbeats_sent++;
    bridge->stats.broadcasts_sent++;
    return 0;
}

/* ============================================================================
 * Subscription Management API Implementation
 * ============================================================================ */

int utils_bio_bridge_subscribe_module(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) return UTILS_BIO_ERROR_INVALID_PARAM;

    /* Check if already subscribed */
    utils_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask = msg_types;
        return 0;
    }

    /* Check capacity */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        return UTILS_BIO_ERROR_SUBSCRIPTION_FULL;
    }

    /* Find free slot */
    for (uint32_t i = 0; i < bridge->subscription_capacity; i++) {
        if (!bridge->subscriptions[i].active) {
            bridge->subscriptions[i].module_id = module_id;
            bridge->subscriptions[i].msg_type_mask = msg_types;
            bridge->subscriptions[i].active = true;
            bridge->subscriptions[i].subscription_time = get_timestamp_us();
            bridge->subscriptions[i].messages_sent = 0;
            bridge->subscription_count++;

            if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
                bridge->stats.peak_subscriptions = bridge->subscription_count;
            }
            bridge->stats.active_subscriptions = bridge->subscription_count;
            return 0;
        }
    }

    return UTILS_BIO_ERROR_SUBSCRIPTION_FULL;
}

int utils_bio_bridge_unsubscribe_module(
    utils_bio_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_unsubscribe_module: bridge is NULL");
        return -1;
    }

    utils_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) return UTILS_BIO_ERROR_SERVICE_NOT_FOUND;

    sub->active = false;
    bridge->subscription_count--;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

int utils_bio_bridge_update_subscription(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_update_subscription: bridge is NULL");
        return -1;
    }

    utils_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) return UTILS_BIO_ERROR_SERVICE_NOT_FOUND;

    sub->msg_type_mask = msg_types;
    return 0;
}

uint32_t utils_bio_bridge_get_subscriber_count(
    const utils_bio_bridge_t* bridge,
    utils_bio_msg_type_t msg_type
) {
    if (!bridge || msg_type >= UTILS_MSG_TYPE_COUNT) return 0;

    uint32_t mask = (1U << msg_type);
    uint32_t count = 0;

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Statistics and Diagnostics API Implementation
 * ============================================================================ */

int utils_bio_bridge_get_stats(
    const utils_bio_bridge_t* bridge,
    utils_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int utils_bio_bridge_reset_stats(utils_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utils_bio_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Preserve subscription counts */
    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    uint32_t services = bridge->stats.registered_services;
    uint32_t timers = bridge->stats.active_timers;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;
    bridge->stats.registered_services = services;
    bridge->stats.active_timers = timers;

    return 0;
}

const char* utils_bio_msg_type_name(utils_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case UTILS_MSG_MEMORY_ALLOC:      return "MEMORY_ALLOC";
        case UTILS_MSG_MEMORY_FREE:       return "MEMORY_FREE";
        case UTILS_MSG_MEMORY_PRESSURE:   return "MEMORY_PRESSURE";
        case UTILS_MSG_MEMORY_POOL_STATUS: return "MEMORY_POOL_STATUS";
        case UTILS_MSG_TIMER_EVENT:       return "TIMER_EVENT";
        case UTILS_MSG_TIMER_SCHEDULE:    return "TIMER_SCHEDULE";
        case UTILS_MSG_TIMER_CANCEL:      return "TIMER_CANCEL";
        case UTILS_MSG_TIMER_DEADLINE:    return "TIMER_DEADLINE";
        case UTILS_MSG_LOG_ENTRY:         return "LOG_ENTRY";
        case UTILS_MSG_LOG_ERROR:         return "LOG_ERROR";
        case UTILS_MSG_LOG_WARNING:       return "LOG_WARNING";
        case UTILS_MSG_LOG_DEBUG:         return "LOG_DEBUG";
        case UTILS_MSG_METRICS:           return "METRICS";
        case UTILS_MSG_METRICS_SNAPSHOT:  return "METRICS_SNAPSHOT";
        case UTILS_MSG_SERVICE_STATUS:    return "SERVICE_STATUS";
        case UTILS_MSG_SERVICE_REGISTER:  return "SERVICE_REGISTER";
        case UTILS_MSG_SERVICE_UNREGISTER: return "SERVICE_UNREGISTER";
        case UTILS_MSG_SERVICE_QUERY:     return "SERVICE_QUERY";
        case UTILS_MSG_SERVICE_RESPONSE:  return "SERVICE_RESPONSE";
        case UTILS_MSG_HEARTBEAT:         return "HEARTBEAT";
        case UTILS_MSG_SYNC_REQUEST:      return "SYNC_REQUEST";
        case UTILS_MSG_SYNC_RESPONSE:     return "SYNC_RESPONSE";
        case UTILS_MSG_SHUTDOWN_NOTICE:   return "SHUTDOWN_NOTICE";
        default:                          return "UNKNOWN";
    }
}

const char* utils_bio_log_level_name(utils_log_level_t level) {
    switch (level) {
        case UTILS_LOG_LEVEL_TRACE:   return "TRACE";
        case UTILS_LOG_LEVEL_DEBUG:   return "DEBUG";
        case UTILS_LOG_LEVEL_INFO:    return "INFO";
        case UTILS_LOG_LEVEL_WARNING: return "WARNING";
        case UTILS_LOG_LEVEL_ERROR:   return "ERROR";
        case UTILS_LOG_LEVEL_FATAL:   return "FATAL";
        default:                      return "UNKNOWN";
    }
}

const char* utils_bio_service_status_name(utils_service_status_t status) {
    switch (status) {
        case UTILS_SERVICE_STATUS_UNKNOWN:   return "UNKNOWN";
        case UTILS_SERVICE_STATUS_STARTING:  return "STARTING";
        case UTILS_SERVICE_STATUS_HEALTHY:   return "HEALTHY";
        case UTILS_SERVICE_STATUS_DEGRADED:  return "DEGRADED";
        case UTILS_SERVICE_STATUS_UNHEALTHY: return "UNHEALTHY";
        case UTILS_SERVICE_STATUS_STOPPING:  return "STOPPING";
        case UTILS_SERVICE_STATUS_STOPPED:   return "STOPPED";
        default:                             return "UNKNOWN";
    }
}

void utils_bio_bridge_print_summary(const utils_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("Utils Bio-Async Bridge: NULL\n");
        return;
    }

    printf("\n=== Utils Bio-Async Bridge Summary ===\n");
    printf("Status: %s\n", bridge->connected ? "Connected" : "Disconnected");
    printf("Initialized: %s\n", bridge->initialized ? "Yes" : "No");
    printf("\nSubscriptions:\n");
    printf("  Active: %u\n", bridge->stats.active_subscriptions);
    printf("  Peak: %u\n", bridge->stats.peak_subscriptions);
    printf("\nMessage Statistics:\n");
    printf("  Sent: %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("  Received: %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("  Dropped: %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("  Broadcasts: %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("\nEvent Statistics:\n");
    printf("  Memory Events: %lu\n", (unsigned long)bridge->stats.memory_events_sent);
    printf("  Timer Events: %lu\n", (unsigned long)bridge->stats.timer_events_sent);
    printf("  Log Entries: %lu\n", (unsigned long)bridge->stats.log_entries_sent);
    printf("  Metrics Broadcasts: %lu\n", (unsigned long)bridge->stats.metrics_broadcasts);
    printf("  Heartbeats: %lu\n", (unsigned long)bridge->stats.heartbeats_sent);
    printf("\nAlerts:\n");
    printf("  Memory Warnings: %lu\n", (unsigned long)bridge->stats.memory_warnings);
    printf("  Memory Criticals: %lu\n", (unsigned long)bridge->stats.memory_criticals);
    printf("\nErrors:\n");
    printf("  Handler Errors: %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("  Routing Errors: %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("\nServices:\n");
    printf("  Registered: %u\n", bridge->stats.registered_services);
    printf("  Active Timers: %u\n", bridge->stats.active_timers);
    printf("=====================================\n\n");
}
