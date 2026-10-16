/**
 * @file nimcp_security_async_bridge.c
 * @brief Security Module - Bio-Async Router Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Implementation of bidirectional security-async bridge
 * WHY:  Enable real-time security coordination across all modules
 * HOW:  Broadcasts security events, receives distributed threat intel
 *
 * @author NIMCP Development Team
 */

#include "security/async/nimcp_security_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_tier_optimization.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_async_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define SECURITY_ASYNC_MODULE_NAME "security_async_bridge"
#define SECURITY_ASYNC_MODULE_ID   0x0630  /* Security async bridge ID */

#define DEFAULT_MAX_PENDING_EVENTS     256
#define DEFAULT_MAX_THREAT_INTEL_CACHE 128
#define DEFAULT_MAX_PATTERN_UPDATES    64
#define DEFAULT_EVENT_BATCH_INTERVAL   50
#define DEFAULT_INTEL_REFRESH_INTERVAL 30000
#define DEFAULT_PATTERN_SYNC_INTERVAL  60000
#define DEFAULT_THREAT_PRIORITY        10

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static int queue_event(security_async_bridge_t* bridge,
                       const security_async_event_t* event);
static int send_bio_message(security_async_bridge_t* bridge,
                            uint32_t msg_type,
                            const void* payload,
                            size_t payload_size,
                            nimcp_bio_channel_type_t channel);
static uint64_t get_current_time_us(void);
static void update_latency_stats(security_async_bridge_t* bridge,
                                 uint64_t start_time);

/* ============================================================================
 * Default Config Implementation
 * ============================================================================ */

int security_async_default_config(security_async_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(security_async_config_t));

    /* Enable all features by default */
    config->enable_threat_broadcast = true;
    config->enable_policy_announcements = true;
    config->enable_pattern_sync = true;
    config->enable_rate_limit_events = true;
    config->enable_bbb_alerts = true;
    config->enable_anomaly_events = true;
    config->enable_distributed_intel = true;
    config->enable_event_bus = false;  /* Disabled by default */

    /* Conservative thresholds */
    config->broadcast_threshold = SECURITY_EVENT_SEVERITY_MEDIUM;
    config->bbb_alert_threshold = BBB_SEVERITY_MEDIUM;
    config->anomaly_alert_threshold = 0.7f;

    /* Capacity settings */
    config->max_pending_events = DEFAULT_MAX_PENDING_EVENTS;
    config->max_threat_intel_cache = DEFAULT_MAX_THREAT_INTEL_CACHE;
    config->max_pattern_updates = DEFAULT_MAX_PATTERN_UPDATES;

    /* Timing settings */
    config->event_batch_interval_ms = DEFAULT_EVENT_BATCH_INTERVAL;
    config->intel_refresh_interval_ms = DEFAULT_INTEL_REFRESH_INTERVAL;
    config->pattern_sync_interval_ms = DEFAULT_PATTERN_SYNC_INTERVAL;

    /* Priority settings - use priority channels */
    config->threat_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->policy_channel = BIO_CHANNEL_SEROTONIN;
    config->threat_priority = DEFAULT_THREAT_PRIORITY;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_async_bridge_t* security_async_bridge_create(
    const security_async_config_t* config
) {
    security_async_config_t default_config;

    /* Use defaults if no config provided */
    if (!config) {
        if (security_async_default_config(&default_config) != 0) {
            NIMCP_LOGGING_ERROR("Failed to get default config");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_async_bridge_create: validation failed");
            return NULL;
        }
        config = &default_config;
    }

    /* Allocate bridge structure */
    security_async_bridge_t* bridge = nimcp_malloc(sizeof(security_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_async_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_async_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, SECURITY_ASYNC_MODULE_ID,
                         SECURITY_ASYNC_MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_async_bridge_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(security_async_config_t));

    /* Allocate event queue */
    bridge->event_queue_capacity = config->max_pending_events;
    bridge->event_queue = nimcp_malloc(
        bridge->event_queue_capacity * sizeof(security_async_event_t));
    if (!bridge->event_queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_async_bridge_create: failed to allocate event queue");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->event_queue, 0,
           bridge->event_queue_capacity * sizeof(security_async_event_t));

    /* Allocate threat intel cache */
    bridge->intel_cache.capacity = config->max_threat_intel_cache;
    bridge->intel_cache.entries = nimcp_malloc(
        bridge->intel_cache.capacity * sizeof(threat_intel_entry_t));
    if (!bridge->intel_cache.entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_async_bridge_create: failed to allocate intel cache");
        nimcp_free(bridge->event_queue);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->intel_cache.entries, 0,
           bridge->intel_cache.capacity * sizeof(threat_intel_entry_t));

    /* Initialize state */
    bridge->state.is_active = true;
    bridge->state.emergency_mode = false;

    NIMCP_LOGGING_INFO("Created security-async bridge (queue_capacity=%zu, "
                       "intel_cache=%zu)",
                       bridge->event_queue_capacity,
                       bridge->intel_cache.capacity);

    return bridge;
}

void security_async_bridge_destroy(security_async_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    NIMCP_LOGGING_INFO("Destroying security-async bridge");

    /* Disconnect from bio-async */
    if (bridge->base.bio_async_enabled) {
        security_async_disconnect_bio_async(bridge);
    }

    /* Free threat intel cache */
    if (bridge->intel_cache.entries) {
        nimcp_free(bridge->intel_cache.entries);
        bridge->intel_cache.entries = NULL;
    }

    /* Free event queue */
    if (bridge->event_queue) {
        nimcp_free(bridge->event_queue);
        bridge->event_queue = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
}

/* ============================================================================
 * Security System Connection Implementation
 * ============================================================================ */

int security_async_connect_bbb(
    security_async_bridge_t* bridge,
    bbb_system_t bbb_system
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bbb_system) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL BBB system");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->bbb_system = bbb_system;
    bridge->state.bbb_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected BBB system to security-async bridge");
    return 0;
}

int security_async_connect_anomaly_detector(
    security_async_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!detector) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL anomaly detector");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->anomaly_detector = detector;
    bridge->state.anomaly_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected anomaly detector to security-async bridge");
    return 0;
}

int security_async_connect_pattern_db(
    security_async_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!pattern_db) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL pattern database");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->pattern_db = pattern_db;
    bridge->state.pattern_db_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected pattern database to security-async bridge");
    return 0;
}

int security_async_connect_policy_engine(
    security_async_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!policy_engine) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL policy engine");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->policy_engine = policy_engine;
    bridge->state.policy_engine_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected policy engine to security-async bridge");
    return 0;
}

int security_async_connect_rate_limiter(
    security_async_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!rate_limiter) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL rate limiter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->rate_limiter = rate_limiter;
    bridge->state.rate_limiter_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected rate limiter to security-async bridge");
    return 0;
}

int security_async_connect_bio_router(
    security_async_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!router) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL bio-router");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->router = router;
    bridge->base.system_b = router;
    bridge->base.system_b_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected bio-router to security-async bridge");
    return 0;
}

int security_async_connect_event_bus(
    security_async_bridge_t* bridge,
    void* event_bus
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!event_bus) {
        NIMCP_LOGGING_ERROR("Cannot connect NULL event bus");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->event_bus = event_bus;
    bridge->state.event_bus_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected event bus to security-async bridge");
    return 0;
}

/* ============================================================================
 * Bio-Async Connection Implementation
 * ============================================================================ */

int security_async_connect_bio_async(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    /* Check if bio-async is initialized */
    if (!nimcp_bio_async_is_initialized()) {
        NIMCP_LOGGING_WARN("Bio-async not initialized, cannot connect");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Use bridge_base function for registration */
    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to connect to bio-async");
        return result;
    }

    NIMCP_LOGGING_INFO("Connected security-async bridge to bio-async router");
    return 0;
}

int security_async_disconnect_bio_async(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Already disconnected */
    }

    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to disconnect from bio-async");
        return result;
    }

    NIMCP_LOGGING_INFO("Disconnected security-async bridge from bio-async router");
    return 0;
}

bool security_async_is_bio_async_connected(const security_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Security -> Async Implementation (Outbound)
 * ============================================================================ */

int security_async_broadcast_threat(
    security_async_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const char* description,
    const uint8_t* threat_hash
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Check threshold */
    if ((int)severity < (int)bridge->config.broadcast_threshold) {
        NIMCP_LOGGING_DEBUG("Threat below broadcast threshold, skipping");
        return 0;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create threat event */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));

    event.category = SECURITY_EVENT_CATEGORY_THREAT;
    event.severity = (security_event_severity_t)severity;
    event.timestamp_us = start_time;
    event.source_module = bridge->base.module_id;
    event.event_id = (uint32_t)(bridge->stats.threat_events + 1);
    event.data.threat.threat_type = threat_type;
    event.data.threat.action_taken = BBB_ACTION_BLOCK;

    if (threat_hash) {
        memcpy(event.data.threat.threat_hash, threat_hash, 32);
    }

    if (description) {
        strncpy(event.description, description, sizeof(event.description) - 1);
    }

    /* Update state */
    bridge->state.last_threat_time_ms = start_time / 1000;
    bridge->security_effects.active_threat = true;
    bridge->security_effects.current_threat_level = severity;

    /* Send via bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_THREAT_DETECTED_EXT,
                                      &event,
                                      sizeof(event),
                                      bridge->config.threat_channel);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    /* Queue for event bus */
    queue_event(bridge, &event);

    /* Update statistics */
    bridge->stats.threat_events++;
    bridge->stats.events_published++;

    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Broadcast threat: type=%d, severity=%d, desc=%s",
                       threat_type, severity,
                       description ? description : "none");

    return 0;
}

int security_async_publish_event(
    security_async_bridge_t* bridge,
    const security_async_event_t* event
) {
    if (!bridge || !event) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Queue for processing */
    int result = queue_event(bridge, event);
    if (result != 0) {
        bridge->stats.events_dropped++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return result;
    }

    /* Send via bio-async if connected and above threshold */
    if (bridge->base.bio_async_enabled &&
        (int)event->severity >= (int)bridge->config.broadcast_threshold) {

        nimcp_bio_channel_type_t channel = bridge->config.policy_channel;
        uint32_t msg_type = BIO_MSG_SECURITY_EVENT;

        /* Map event category to extended message type */
        switch (event->category) {
            case SECURITY_EVENT_CATEGORY_THREAT:
                msg_type = BIO_MSG_SECURITY_THREAT_DETECTED_EXT;
                channel = bridge->config.threat_channel;
                break;
            case SECURITY_EVENT_CATEGORY_POLICY:
                msg_type = BIO_MSG_SECURITY_POLICY_CHANGE_EXT;
                break;
            case SECURITY_EVENT_CATEGORY_PATTERN:
                msg_type = BIO_MSG_SECURITY_PATTERN_UPDATE_EXT;
                break;
            case SECURITY_EVENT_CATEGORY_RATE_LIMIT:
                msg_type = BIO_MSG_SECURITY_RATE_LIMIT_HIT_EXT;
                break;
            case SECURITY_EVENT_CATEGORY_BBB:
                msg_type = BIO_MSG_SECURITY_BBB_ALERT_EXT;
                channel = bridge->config.threat_channel;
                break;
            case SECURITY_EVENT_CATEGORY_ANOMALY:
                msg_type = BIO_MSG_SECURITY_ANOMALY_DETECTED_EXT;
                channel = bridge->config.threat_channel;
                break;
            default:
                break;
        }

        result = send_bio_message(bridge,
                                  msg_type,
                                  event,
                                  sizeof(*event),
                                  channel);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    /* Update statistics by category */
    switch (event->category) {
        case SECURITY_EVENT_CATEGORY_THREAT:
            bridge->stats.threat_events++;
            break;
        case SECURITY_EVENT_CATEGORY_POLICY:
            bridge->stats.policy_events++;
            break;
        case SECURITY_EVENT_CATEGORY_PATTERN:
            bridge->stats.pattern_events++;
            break;
        case SECURITY_EVENT_CATEGORY_RATE_LIMIT:
            bridge->stats.rate_limit_events++;
            break;
        case SECURITY_EVENT_CATEGORY_BBB:
            bridge->stats.bbb_events++;
            break;
        case SECURITY_EVENT_CATEGORY_ANOMALY:
            bridge->stats.anomaly_events++;
            break;
        default:
            break;
    }

    bridge->stats.events_published++;
    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int security_async_announce_policy_change(
    security_async_bridge_t* bridge,
    nimcp_policy_action_t action,
    const char* rule_name,
    const char* description
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_policy_announcements) {
        return 0;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create policy event */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));

    event.category = SECURITY_EVENT_CATEGORY_POLICY;
    event.severity = SECURITY_EVENT_SEVERITY_INFO;
    event.timestamp_us = start_time;
    event.source_module = bridge->base.module_id;
    event.event_id = (uint32_t)(bridge->stats.policy_events + 1);
    event.data.policy.action = action;
    event.data.policy.policy_severity = NIMCP_POLICY_SEVERITY_INFO;

    if (rule_name) {
        strncpy(event.data.policy.rule_name, rule_name,
                sizeof(event.data.policy.rule_name) - 1);
    }

    if (description) {
        strncpy(event.description, description, sizeof(event.description) - 1);
    }

    /* Update state */
    bridge->state.last_policy_change_ms = start_time / 1000;
    bridge->security_effects.policy_updated = true;

    /* Send via bio-async */
    if (bridge->base.bio_async_enabled) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_POLICY_CHANGE_EXT,
                                      &event,
                                      sizeof(event),
                                      bridge->config.policy_channel);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    queue_event(bridge, &event);

    bridge->stats.policy_events++;
    bridge->stats.events_published++;
    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Announced policy change: action=%d, rule=%s",
                       action, rule_name ? rule_name : "none");

    return 0;
}

int security_async_broadcast_bbb_alert(
    security_async_bridge_t* bridge,
    const bbb_threat_report_t* report
) {
    if (!bridge || !report) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_bbb_alerts) {
        return 0;
    }

    /* Check severity threshold */
    if ((int)report->severity < (int)bridge->config.bbb_alert_threshold) {
        return 0;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create BBB event */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));

    event.category = SECURITY_EVENT_CATEGORY_BBB;
    event.severity = (security_event_severity_t)report->severity;
    event.timestamp_us = start_time;
    event.source_module = bridge->base.module_id;
    event.event_id = (uint32_t)(bridge->stats.bbb_events + 1);
    event.data.threat.threat_type = report->type;
    event.data.threat.action_taken = report->action_taken;
    memcpy(event.data.threat.threat_hash, report->threat_hash, 32);

    strncpy(event.description, report->description, sizeof(event.description) - 1);

    /* Send via bio-async */
    if (bridge->base.bio_async_enabled) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_BBB_ALERT_EXT,
                                      &event,
                                      sizeof(event),
                                      bridge->config.threat_channel);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    queue_event(bridge, &event);

    bridge->stats.bbb_events++;
    bridge->stats.events_published++;
    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Broadcast BBB alert: type=%d, severity=%d",
                       report->type, report->severity);

    return 0;
}

int security_async_broadcast_rate_limit(
    security_async_bridge_t* bridge,
    const char* client_id,
    nimcp_penalty_action_t penalty,
    uint32_t violation_count
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_rate_limit_events) {
        return 0;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create rate limit event */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));

    event.category = SECURITY_EVENT_CATEGORY_RATE_LIMIT;
    event.severity = (penalty >= PENALTY_BLOCK_TEMPORARY) ?
                     SECURITY_EVENT_SEVERITY_HIGH :
                     SECURITY_EVENT_SEVERITY_MEDIUM;
    event.timestamp_us = start_time;
    event.source_module = bridge->base.module_id;
    event.event_id = (uint32_t)(bridge->stats.rate_limit_events + 1);
    event.data.rate_limit.penalty = penalty;
    event.data.rate_limit.violation_count = violation_count;

    if (client_id) {
        strncpy(event.data.rate_limit.client_id, client_id,
                sizeof(event.data.rate_limit.client_id) - 1);
    }

    snprintf(event.description, sizeof(event.description),
             "Rate limit: client=%s, penalty=%d, violations=%u",
             client_id ? client_id : "unknown", penalty, violation_count);

    /* Send via bio-async */
    if (bridge->base.bio_async_enabled) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_RATE_LIMIT_HIT_EXT,
                                      &event,
                                      sizeof(event),
                                      BIO_CHANNEL_ACETYLCHOLINE);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    queue_event(bridge, &event);

    bridge->stats.rate_limit_events++;
    bridge->stats.events_published++;
    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Broadcast rate limit: client=%s, penalty=%d",
                        client_id ? client_id : "unknown", penalty);

    return 0;
}

int security_async_broadcast_pattern_update(
    security_async_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    nimcp_pattern_category_t category,
    bool is_new
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_pattern_sync) {
        return 0;
    }

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create pattern event */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));

    event.category = SECURITY_EVENT_CATEGORY_PATTERN;
    event.severity = SECURITY_EVENT_SEVERITY_INFO;
    event.timestamp_us = start_time;
    event.source_module = bridge->base.module_id;
    event.event_id = (uint32_t)(bridge->stats.pattern_events + 1);
    event.data.pattern.category = category;
    event.data.pattern.pattern_id = pattern_id;
    event.data.pattern.threat_score = 0.0f;

    snprintf(event.description, sizeof(event.description),
             "Pattern %s: id=%u, category=%d",
             is_new ? "added" : "updated", pattern_id, category);

    /* Send via bio-async */
    if (bridge->base.bio_async_enabled) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_PATTERN_UPDATE_EXT,
                                      &event,
                                      sizeof(event),
                                      bridge->config.policy_channel);
        if (result != 0) {
            bridge->stats.broadcast_failures++;
        }
    }

    queue_event(bridge, &event);

    bridge->stats.pattern_events++;
    bridge->stats.patterns_synced++;
    bridge->stats.events_published++;
    update_latency_stats(bridge, start_time);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Broadcast pattern update: id=%u, category=%d, new=%d",
                        pattern_id, category, is_new);

    return 0;
}

/* ============================================================================
 * Async -> Security Implementation (Inbound)
 * ============================================================================ */

int security_async_receive_threat_report(
    security_async_bridge_t* bridge,
    uint32_t source_module,
    bbb_threat_type_t threat_type,
    const uint8_t* threat_hash,
    float confidence
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for existing entry in cache */
    bool found = false;
    size_t existing_idx = 0;

    if (threat_hash) {
        for (size_t i = 0; i < bridge->intel_cache.count; i++) {
            if (memcmp(bridge->intel_cache.entries[i].threat_hash,
                       threat_hash, 32) == 0) {
                found = true;
                existing_idx = i;
                break;
            }
        }
    }

    uint64_t now_ms = nimcp_time_get_ms();

    if (found) {
        /* Update existing entry */
        threat_intel_entry_t* entry = &bridge->intel_cache.entries[existing_idx];
        entry->observation_count++;
        entry->last_seen_ms = now_ms;

        /* Increase confidence with multiple reports */
        entry->confidence = fminf(1.0f, entry->confidence + confidence * 0.1f);

        /* Mark as confirmed if multiple sources */
        if (entry->source_node != source_module) {
            entry->confirmed = true;
        }
    } else if (bridge->intel_cache.count < bridge->intel_cache.capacity) {
        /* Add new entry */
        threat_intel_entry_t* entry =
            &bridge->intel_cache.entries[bridge->intel_cache.count];

        if (threat_hash) {
            memcpy(entry->threat_hash, threat_hash, 32);
        }
        entry->threat_type = threat_type;
        entry->severity = BBB_SEVERITY_MEDIUM;
        entry->source_node = source_module;
        entry->first_seen_ms = now_ms;
        entry->last_seen_ms = now_ms;
        entry->observation_count = 1;
        entry->confidence = confidence;
        entry->confirmed = false;

        bridge->intel_cache.count++;
    }

    /* Update async effects */
    bridge->async_effects.peer_threat_reports++;
    bridge->async_effects.network_threat_level =
        fminf(1.0f, bridge->async_effects.network_threat_level + 0.1f);

    bridge->stats.events_received++;
    bridge->stats.intel_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Received threat report from module 0x%04X: type=%d, "
                        "confidence=%.2f",
                        source_module, threat_type, confidence);

    return 0;
}

int security_async_receive_pattern_update(
    security_async_bridge_t* bridge,
    uint32_t source_module,
    const nimcp_pattern_entry_t* entry
) {
    if (!bridge || !entry) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update async effects */
    bridge->async_effects.peer_pattern_updates++;
    bridge->stats.events_received++;

    /* If pattern DB is connected, we could add the pattern */
    /* For now, just record the update */
    bridge->state.pending_patterns++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Received pattern update from module 0x%04X: "
                        "category=%d",
                        source_module, entry->category);

    return 0;
}

int security_async_request_threat_intel(
    security_async_bridge_t* bridge,
    const uint8_t* threat_hash
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_distributed_intel) {
        return 0;
    }

    if (!bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("Cannot request threat intel: bio-async not connected");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create intel request message */
    struct {
        uint32_t request_id;
        uint8_t threat_hash[32];
        bool specific_threat;
    } request;

    request.request_id = (uint32_t)(bridge->stats.intel_received + 1);
    request.specific_threat = (threat_hash != NULL);
    if (threat_hash) {
        memcpy(request.threat_hash, threat_hash, 32);
    } else {
        memset(request.threat_hash, 0, 32);
    }

    int result = send_bio_message(bridge,
                                  BIO_MSG_SECURITY_THREAT_INTEL_REQUEST,
                                  &request,
                                  sizeof(request),
                                  BIO_CHANNEL_ACETYLCHOLINE);

    nimcp_mutex_unlock(bridge->base.mutex);

    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to send threat intel request");
        return result;
    }

    NIMCP_LOGGING_DEBUG("Requested threat intel%s",
                        threat_hash ? " for specific threat" : " (general)");

    return 0;
}

int security_async_share_threat_intel(
    security_async_bridge_t* bridge,
    uint32_t max_entries
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_distributed_intel) {
        return 0;
    }

    if (!bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("Cannot share threat intel: bio-async not connected");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t to_share = (max_entries == 0) ?
                        (uint32_t)bridge->intel_cache.count :
                        (max_entries < bridge->intel_cache.count ?
                         max_entries : (uint32_t)bridge->intel_cache.count);

    for (uint32_t i = 0; i < to_share; i++) {
        int result = send_bio_message(bridge,
                                      BIO_MSG_SECURITY_THREAT_INTEL_SHARE,
                                      &bridge->intel_cache.entries[i],
                                      sizeof(threat_intel_entry_t),
                                      bridge->config.policy_channel);
        if (result == 0) {
            bridge->stats.intel_shared++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Shared %u threat intel entries", to_share);

    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int security_async_update_security_effects(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay threat level over time */
    uint64_t now_ms = nimcp_time_get_ms();
    uint64_t time_since_threat = now_ms - bridge->state.last_threat_time_ms;

    if (time_since_threat > 60000) {  /* 60 seconds */
        bridge->security_effects.active_threat = false;
        bridge->security_effects.current_threat_level = BBB_SEVERITY_NONE;
    }

    /* Compute priority boost based on threat level */
    switch (bridge->security_effects.current_threat_level) {
        case BBB_SEVERITY_CRITICAL:
            bridge->security_effects.priority_boost = 1.0f;
            bridge->security_effects.bypass_normal_routing = true;
            break;
        case BBB_SEVERITY_HIGH:
            bridge->security_effects.priority_boost = 0.7f;
            bridge->security_effects.bypass_normal_routing = false;
            break;
        case BBB_SEVERITY_MEDIUM:
            bridge->security_effects.priority_boost = 0.3f;
            bridge->security_effects.bypass_normal_routing = false;
            break;
        default:
            bridge->security_effects.priority_boost = 0.0f;
            bridge->security_effects.bypass_normal_routing = false;
            break;
    }

    /* Compute rate reduction in emergency mode */
    if (bridge->state.emergency_mode) {
        bridge->security_effects.rate_reduction_factor = 0.5f;
        bridge->security_effects.emergency_throttle = true;
        bridge->security_effects.active_threat = true;
        /* Emergency mode overrides threat-level priority boost */
        if (bridge->security_effects.priority_boost < 1.0f) {
            bridge->security_effects.priority_boost = 1.0f;
        }
    } else {
        bridge->security_effects.rate_reduction_factor = 1.0f;
        bridge->security_effects.emergency_throttle = false;
    }

    /* Clear policy updated flag after a cycle */
    bridge->security_effects.policy_updated = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int security_async_update_async_effects(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_ms = nimcp_time_get_ms();

    /* Decay network threat level */
    bridge->async_effects.network_threat_level *= 0.99f;

    /* Check intel freshness */
    uint64_t intel_age = now_ms - bridge->intel_cache.last_refresh_ms;
    if (intel_age > bridge->config.intel_refresh_interval_ms) {
        bridge->async_effects.stale_intel_count++;
    }

    /* Update correlation confidence based on confirmed threats */
    uint32_t confirmed_count = 0;
    for (size_t i = 0; i < bridge->intel_cache.count; i++) {
        if (bridge->intel_cache.entries[i].confirmed) {
            confirmed_count++;
        }
    }

    if (bridge->intel_cache.count > 0) {
        bridge->async_effects.correlation_confidence =
            (float)confirmed_count / (float)bridge->intel_cache.count;
    }

    bridge->async_effects.last_intel_update_ms = now_ms;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int security_async_bridge_update(
    security_async_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    (void)delta_ms;  /* Currently unused */

    /* Update both directions */
    int result = security_async_update_security_effects(bridge);
    if (result != 0) {
        return result;
    }

    result = security_async_update_async_effects(bridge);
    if (result != 0) {
        return result;
    }

    /* Process pending events */
    security_async_process_events(bridge, 0);

    /* Record update in base */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t security_async_process_events(
    security_async_bridge_t* bridge,
    uint32_t max_events
) {
    if (!bridge) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Process inbox from bio-async if connected */
    uint32_t processed = 0;

    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        processed = bio_router_process_inbox(bridge->base.bio_ctx,
                                             max_events ? max_events : 32);
    }

    /* Update pending counts */
    bridge->state.pending_events = (uint32_t)(
        (bridge->event_queue_tail >= bridge->event_queue_head) ?
        (bridge->event_queue_tail - bridge->event_queue_head) :
        (bridge->event_queue_capacity - bridge->event_queue_head +
         bridge->event_queue_tail));

    nimcp_mutex_unlock(bridge->base.mutex);

    return processed;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_async_get_security_effects(
    const security_async_bridge_t* bridge,
    security_async_effects_t* effects
) {
    if (!bridge || !effects) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->security_effects, sizeof(security_async_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int security_async_get_async_effects(
    const security_async_bridge_t* bridge,
    async_security_effects_t* effects
) {
    if (!bridge || !effects) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->async_effects, sizeof(async_security_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int security_async_get_state(
    const security_async_bridge_t* bridge,
    security_async_state_t* state
) {
    if (!bridge || !state) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->state, sizeof(security_async_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int security_async_get_stats(
    const security_async_bridge_t* bridge,
    security_async_stats_t* stats
) {
    if (!bridge || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(security_async_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int security_async_reset_stats(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(security_async_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Reset security-async bridge statistics");
    return 0;
}

bool security_async_is_connected(const security_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Require at least BBB and bio-async connected for full functionality */
    return bridge->state.bbb_connected && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Threat Intelligence Implementation
 * ============================================================================ */

int security_async_cache_threat_intel(
    security_async_bridge_t* bridge,
    const threat_intel_entry_t* entry
) {
    if (!bridge || !entry) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if entry already exists */
    for (size_t i = 0; i < bridge->intel_cache.count; i++) {
        if (memcmp(bridge->intel_cache.entries[i].threat_hash,
                   entry->threat_hash, 32) == 0) {
            /* Update existing */
            memcpy(&bridge->intel_cache.entries[i], entry,
                   sizeof(threat_intel_entry_t));
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Add new if capacity available */
    if (bridge->intel_cache.count >= bridge->intel_cache.capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("Threat intel cache full");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    memcpy(&bridge->intel_cache.entries[bridge->intel_cache.count],
           entry, sizeof(threat_intel_entry_t));
    bridge->intel_cache.count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cached threat intel entry (total=%zu)",
                        bridge->intel_cache.count);

    return 0;
}

bool security_async_lookup_threat_intel(
    const security_async_bridge_t* bridge,
    const uint8_t* threat_hash,
    threat_intel_entry_t* entry
) {
    if (!bridge || !threat_hash) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_async_lookup_threat_intel: required parameter is NULL (bridge, threat_hash)");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    for (size_t i = 0; i < bridge->intel_cache.count; i++) {
        if (memcmp(bridge->intel_cache.entries[i].threat_hash,
                   threat_hash, 32) == 0) {
            if (entry) {
                memcpy(entry, &bridge->intel_cache.entries[i],
                       sizeof(threat_intel_entry_t));
            }
            nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
            return true;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return false;
}

int security_async_clear_threat_intel(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(bridge->intel_cache.entries, 0,
           bridge->intel_cache.capacity * sizeof(threat_intel_entry_t));
    bridge->intel_cache.count = 0;
    bridge->intel_cache.last_refresh_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cleared threat intel cache");
    return 0;
}

int security_async_get_intel_stats(
    const security_async_bridge_t* bridge,
    uint32_t* count,
    uint32_t* confirmed
) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    if (count) {
        *count = (uint32_t)bridge->intel_cache.count;
    }

    if (confirmed) {
        *confirmed = 0;
        for (size_t i = 0; i < bridge->intel_cache.count; i++) {
            if (bridge->intel_cache.entries[i].confirmed) {
                (*confirmed)++;
            }
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Emergency Mode Implementation
 * ============================================================================ */

int security_async_enter_emergency_mode(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->state.emergency_mode) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already in emergency mode */
    }

    bridge->state.emergency_mode = true;
    bridge->security_effects.emergency_throttle = true;
    bridge->security_effects.rate_reduction_factor = 0.25f;
    bridge->security_effects.bypass_normal_routing = true;
    bridge->security_effects.priority_boost = 1.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast emergency mode activation */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));
    event.category = SECURITY_EVENT_CATEGORY_SYSTEM;
    event.severity = SECURITY_EVENT_SEVERITY_CRITICAL;
    event.timestamp_us = get_current_time_us();
    event.source_module = bridge->base.module_id;
    strncpy(event.description, "Emergency security mode activated",
            sizeof(event.description) - 1);

    security_async_publish_event(bridge, &event);

    NIMCP_LOGGING_INFO("Entered emergency security mode");
    return 0;
}

int security_async_exit_emergency_mode(security_async_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->state.emergency_mode) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not in emergency mode */
    }

    bridge->state.emergency_mode = false;
    bridge->security_effects.emergency_throttle = false;
    bridge->security_effects.rate_reduction_factor = 1.0f;
    bridge->security_effects.bypass_normal_routing = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast emergency mode deactivation */
    security_async_event_t event;
    memset(&event, 0, sizeof(event));
    event.category = SECURITY_EVENT_CATEGORY_SYSTEM;
    event.severity = SECURITY_EVENT_SEVERITY_INFO;
    event.timestamp_us = get_current_time_us();
    event.source_module = bridge->base.module_id;
    strncpy(event.description, "Emergency security mode deactivated",
            sizeof(event.description) - 1);

    security_async_publish_event(bridge, &event);

    NIMCP_LOGGING_INFO("Exited emergency security mode");
    return 0;
}

bool security_async_is_emergency_mode(const security_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.emergency_mode;
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static int queue_event(security_async_bridge_t* bridge,
                       const security_async_event_t* event) {
    /* Caller must hold mutex */

    size_t next_tail = (bridge->event_queue_tail + 1) %
                       bridge->event_queue_capacity;

    if (next_tail == bridge->event_queue_head) {
        /* Queue full */
        bridge->stats.queue_overflows++;
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    memcpy(&bridge->event_queue[bridge->event_queue_tail],
           event, sizeof(security_async_event_t));
    bridge->event_queue_tail = next_tail;
    bridge->state.pending_events++;

    return 0;
}

static int send_bio_message(security_async_bridge_t* bridge,
                            uint32_t msg_type,
                            const void* payload,
                            size_t payload_size,
                            nimcp_bio_channel_type_t channel) {
    /* Caller must hold mutex */

    if (!bridge->base.bio_ctx) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Allocate message buffer */
    size_t msg_size = sizeof(bio_message_header_t) + payload_size;
    uint8_t* msg_buffer = nimcp_malloc(msg_size);
    if (!msg_buffer) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Fill header */
    bio_message_header_t* header = (bio_message_header_t*)msg_buffer;
    header->type = msg_type;
    header->source_module = bridge->base.module_id;
    header->target_module = 0;  /* Broadcast */
    header->payload_size = (uint32_t)payload_size;
    header->timestamp_us = get_current_time_us();
    header->channel = channel;
    header->sequence_id = 0;
    header->flags = (uint32_t)bridge->config.threat_priority;  /* Store priority in flags */

    /* Copy payload */
    if (payload && payload_size > 0) {
        memcpy(msg_buffer + sizeof(bio_message_header_t), payload, payload_size);
    }

    /* Broadcast message */
    nimcp_error_t result = bio_router_broadcast(bridge->base.bio_ctx,
                                                msg_buffer,
                                                msg_size);

    nimcp_free(msg_buffer);

    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

static uint64_t get_current_time_us(void) {
    return nimcp_time_get_us();
}

static void update_latency_stats(security_async_bridge_t* bridge,
                                 uint64_t start_time) {
    /* Caller must hold mutex */

    uint64_t elapsed = get_current_time_us() - start_time;
    float elapsed_f = (float)elapsed;

    /* Update max */
    if (elapsed_f > bridge->stats.max_broadcast_latency_us) {
        bridge->stats.max_broadcast_latency_us = elapsed_f;
    }

    /* Update average with exponential moving average */
    float alpha = 0.1f;
    bridge->stats.avg_broadcast_latency_us =
        alpha * elapsed_f +
        (1.0f - alpha) * bridge->stats.avg_broadcast_latency_us;
}
