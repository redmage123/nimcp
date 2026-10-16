/**
 * @file nimcp_security_bio_async_bridge.c
 * @brief Implementation of Security Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/integration/nimcp_security_bio_async_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_bio_async_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#define LOG_MODULE "SECURITY_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    uint32_t current_security_level;
    bool lockdown_active;
    uint32_t lockdown_level;
    uint64_t lockdown_start_time;
    uint64_t last_lockdown_end_time;
    uint32_t next_request_id;
    uint64_t start_time_us;
} security_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct security_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    security_bio_bridge_config_t config;
    bio_router_t router;

    security_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_heartbeat_us;
    uint64_t last_metrics_us;
    uint32_t time_since_heartbeat_ms;
    uint32_t time_since_metrics_ms;

    security_internal_state_t internal;
    security_bio_bridge_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds (safe wrapper)
 *
 * SECURITY FIX: Check clock_gettime() return value to avoid using garbage data.
 * On failure, returns 0 which may affect timing but won't cause undefined behavior.
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /* clock_gettime failed - return 0 to be safe */
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static security_bio_subscription_t* find_subscription(
    security_bio_bridge_t* b, uint32_t module_id
) {
    if (!b || !b->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subscription: required parameter is NULL (b, b->subscriptions)");
        return NULL;
    }

    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id &&
            b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    return NULL;
}

static uint32_t count_subscribers_for_type(
    const security_bio_bridge_t* b,
    security_bio_msg_type_t msg_type
) {
    if (!b || !b->subscriptions) return 0;

    uint32_t count = 0;
    uint64_t type_mask = (1ULL << msg_type);

    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].active &&
            (b->subscriptions[i].msg_type_mask & type_mask)) {
            count++;
        }
    }
    return count;
}

static void safe_strncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int security_bio_bridge_default_config(security_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_default_config: config is NULL");
        return -1;
    }

    config->heartbeat_interval_ms = 1000;  /* 1 second */
    config->metrics_interval_ms = 5000;    /* 5 seconds */
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = SECURITY_BIO_MESSAGE_TTL_MS;
    config->urgent_timeout_ms = SECURITY_BIO_URGENT_TIMEOUT_MS;
    config->default_channel = BIO_CHANNEL_SEROTONIN;
    config->alert_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->urgent_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->max_subscriptions = SECURITY_BIO_MAX_SUBSCRIPTIONS;
    config->threat_escalation_threshold = 0.7f;
    config->enable_auto_lockdown = true;
    config->lockdown_cooldown_ms = 5000;  /* 5 seconds between lockdowns */
    config->enable_threat_routing = true;
    config->enable_access_control = true;
    config->enable_consensus = true;
    config->enable_metrics_broadcast = true;
    config->enable_logging = false;

    return 0;
}

security_bio_bridge_t* security_bio_bridge_create(
    const security_bio_bridge_config_t* config
) {
    security_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_bio_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        security_bio_bridge_default_config(&bridge->config);
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(bridge->subscription_capacity,
                                         sizeof(security_bio_subscription_t));
    if (!bridge->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_bio_bridge_create: failed to allocate subscriptions");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->internal.start_time_us = get_timestamp_us();
    bridge->internal.next_request_id = 1;
    bridge->internal.current_security_level = 1;  /* Default level */
    bridge->last_heartbeat_us = get_timestamp_us();
    bridge->last_metrics_us = get_timestamp_us();

    return bridge;
}

void security_bio_bridge_destroy(security_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "security_bio_async");

    if (bridge->connected) {
        security_bio_bridge_disconnect(bridge);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int security_bio_bridge_connect(
    security_bio_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_connect: bridge is NULL");
        return -1;
    }
    if (bridge->connected) {
        return -1;
    }

    bridge->router = router;
    bridge->connected = true;

    return 0;
}

int security_bio_bridge_disconnect(security_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool security_bio_bridge_is_connected(const security_bio_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int security_bio_bridge_process_inbox(
    security_bio_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_process_inbox: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    uint32_t processed = 0;
    (void)max_messages;  /* Placeholder for actual message processing */

    bridge->stats.messages_received += processed;
    return (int)processed;
}

int security_bio_bridge_update(
    security_bio_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    bridge->time_since_heartbeat_ms += delta_ms;
    bridge->time_since_metrics_ms += delta_ms;

    /* Auto-broadcast heartbeat */
    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_heartbeat_ms >= bridge->config.heartbeat_interval_ms) {
        security_bio_bridge_broadcast_heartbeat(bridge);
        bridge->time_since_heartbeat_ms = 0;
    }

    /* Auto-broadcast metrics */
    if (bridge->config.enable_metrics_broadcast &&
        bridge->time_since_metrics_ms >= bridge->config.metrics_interval_ms) {
        security_bio_bridge_broadcast_metrics(bridge);
        bridge->time_since_metrics_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Threat Notification API
 * ============================================================================ */

int security_bio_bridge_broadcast_threat(
    security_bio_bridge_t* bridge,
    security_threat_type_t threat_type,
    nimcp_threat_level_t severity,
    uint32_t source_module,
    float confidence,
    const char* details
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_threat: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_threat_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_THREAT_DETECTED;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.threat_type = threat_type;
    msg.severity = severity;
    msg.source_module = source_module;
    msg.confidence = confidence;
    msg.timestamp_us = msg.header.timestamp_us;

    if (details) {
        safe_strncpy(msg.details, details, sizeof(msg.details));
    }

    /* Check for auto-lockdown on critical threats */
    if (bridge->config.enable_auto_lockdown &&
        severity >= NIMCP_THREAT_CRITICAL &&
        confidence >= bridge->config.threat_escalation_threshold) {

        uint64_t now = get_timestamp_us();
        uint64_t cooldown_us = bridge->config.lockdown_cooldown_ms * 1000ULL;

        if (!bridge->internal.lockdown_active &&
            (now - bridge->internal.last_lockdown_end_time) >= cooldown_us) {
            security_bio_bridge_initiate_lockdown(bridge, severity,
                                                   threat_type, details);
        }
    }

    bridge->stats.threat_alerts_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->last_heartbeat_us = msg.timestamp_us;

    return 0;
}

int security_bio_bridge_broadcast_anomaly(
    security_bio_bridge_t* bridge,
    float content_score,
    float behavior_score,
    float timing_score,
    uint32_t source_module
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_anomaly: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_anomaly_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_ALERT;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.alert_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.content_anomaly_score = content_score;
    msg.behavior_anomaly_score = behavior_score;
    msg.timing_anomaly_score = timing_score;

    /* Compute overall score (max of all) */
    msg.overall_score = content_score;
    if (behavior_score > msg.overall_score) msg.overall_score = behavior_score;
    if (timing_score > msg.overall_score) msg.overall_score = timing_score;

    msg.source_module = source_module;
    msg.requires_action = (msg.overall_score >= bridge->config.threat_escalation_threshold);
    msg.timestamp_us = msg.header.timestamp_us;

    /* Mark as urgent if threshold exceeded */
    if (msg.requires_action) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    bridge->stats.anomaly_alerts_sent++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int security_bio_bridge_initiate_lockdown(
    security_bio_bridge_t* bridge,
    uint32_t level,
    security_threat_type_t trigger_threat,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        return -1;
    }
    if (bridge->internal.lockdown_active) {
        return -1;
    }

    security_bio_lockdown_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_ALERT;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.urgent_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = get_timestamp_us();

    msg.is_start = true;
    msg.lockdown_level = level;
    msg.trigger_threat = trigger_threat;
    msg.affected_modules = 0xFFFFFFFF;  /* All modules affected */
    msg.duration_estimate_ms = 30000;   /* Default 30 second estimate */
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    /* Update internal state */
    bridge->internal.lockdown_active = true;
    bridge->internal.lockdown_level = level;
    bridge->internal.lockdown_start_time = msg.timestamp_us;

    bridge->stats.lockdowns_initiated++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int security_bio_bridge_end_lockdown(
    security_bio_bridge_t* bridge,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        return -1;
    }
    if (!bridge->internal.lockdown_active) {
        return -1;
    }

    security_bio_lockdown_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_ALERT;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.alert_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.is_start = false;
    msg.lockdown_level = bridge->internal.lockdown_level;
    msg.trigger_threat = SECURITY_THREAT_NONE;
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    /* Update internal state */
    bridge->internal.lockdown_active = false;
    bridge->internal.last_lockdown_end_time = msg.timestamp_us;

    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Access Control Coordination API
 * ============================================================================ */

int security_bio_bridge_request_access(
    security_bio_bridge_t* bridge,
    uint32_t requester_module,
    uint32_t target_resource,
    security_access_type_t access_type,
    uint32_t* request_id
) {
    if (!bridge || !bridge->connected || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_request_access: required parameter is NULL (bridge, bridge->connected, request_id)");
        return -1;
    }

    security_bio_access_request_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_EVENT;
    msg.header.source_module = requester_module;
    msg.header.target_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.requester_module = requester_module;
    msg.target_resource = target_resource;
    msg.access_type = access_type;
    msg.request_id = bridge->internal.next_request_id++;
    msg.timestamp_us = msg.header.timestamp_us;

    *request_id = msg.request_id;

    bridge->stats.access_requests_processed++;
    bridge->stats.messages_sent++;

    return 0;
}

int security_bio_bridge_respond_access(
    security_bio_bridge_t* bridge,
    uint32_t request_id,
    security_decision_t decision,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_respond_access: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_access_response_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_EVENT;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.timestamp_us = get_timestamp_us();

    msg.request_id = request_id;
    msg.decision = decision;
    msg.trust_score = 1.0f;  /* Default full trust */
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    if (decision == SECURITY_DECISION_RATE_LIMITED) {
        msg.retry_after_ms = 1000;  /* Default 1 second retry */
    }

    if (decision == SECURITY_DECISION_DENIED ||
        decision == SECURITY_DECISION_LOCKDOWN) {
        bridge->stats.access_denied_count++;
    }

    bridge->stats.messages_sent++;

    return 0;
}

int security_bio_bridge_broadcast_level_change(
    security_bio_bridge_t* bridge,
    uint32_t old_level,
    uint32_t new_level,
    security_threat_type_t trigger_threat,
    const char* reason
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_level_change: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_level_change_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_LEVEL_CHANGE;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.alert_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.old_level = old_level;
    msg.new_level = new_level;
    msg.trigger_threat = trigger_threat;
    msg.trigger_module = BIO_MODULE_SECURITY;
    msg.timestamp_us = msg.header.timestamp_us;

    if (reason) {
        safe_strncpy(msg.reason, reason, sizeof(msg.reason));
    }

    /* Update internal state */
    bridge->internal.current_security_level = new_level;

    /* Mark as urgent if escalating */
    if (new_level > old_level) {
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Consensus Coordination API
 * ============================================================================ */

int security_bio_bridge_propose_consensus(
    security_bio_bridge_t* bridge,
    uint32_t proposal_id,
    const char* description
) {
    if (!bridge || !bridge->connected) {
        return -1;
    }
    if (!bridge->config.enable_consensus) {
        return -1;
    }

    (void)description;  /* Would be included in actual message */

    security_bio_consensus_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_CONSENSUS_REQUEST;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.proposal_id = proposal_id;
    msg.votes_for = 0;
    msg.votes_against = 0;
    msg.votes_pending = bridge->subscription_count;
    msg.agreement_ratio = 0.0f;
    msg.is_complete = false;
    msg.result = false;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.consensus_proposals++;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int security_bio_bridge_broadcast_consensus_update(
    security_bio_bridge_t* bridge,
    uint32_t proposal_id,
    uint32_t votes_for,
    uint32_t votes_against,
    bool is_complete,
    bool result
) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_consensus_update: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_consensus_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_CONSENSUS_RESPONSE;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.proposal_id = proposal_id;
    msg.votes_for = votes_for;
    msg.votes_against = votes_against;
    msg.votes_pending = 0;  /* Would be calculated */

    uint32_t total_votes = votes_for + votes_against;
    msg.agreement_ratio = total_votes > 0 ?
                          (float)votes_for / (float)total_votes : 0.0f;

    msg.is_complete = is_complete;
    msg.result = result;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

int security_bio_bridge_broadcast_heartbeat(security_bio_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_heartbeat: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_heartbeat_msg_t msg = {0};
    msg.header.type = BIO_MSG_HEALTH_CHECK;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.security_level = bridge->internal.current_security_level;
    msg.is_healthy = true;
    msg.lockdown_active = bridge->internal.lockdown_active;
    msg.active_monitors = 1;  /* Bridge itself */
    msg.uptime_ms = (uint64_t)(msg.header.timestamp_us -
                               bridge->internal.start_time_us) / 1000ULL;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->last_heartbeat_us = msg.timestamp_us;
    bridge->stats.broadcasts_sent++;

    return 0;
}

int security_bio_bridge_broadcast_metrics(security_bio_bridge_t* bridge) {
    if (!bridge || !bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_broadcast_metrics: required parameter is NULL (bridge, bridge->connected)");
        return -1;
    }

    security_bio_metrics_msg_t msg = {0};
    msg.header.type = BIO_MSG_SECURITY_AUDIT_EVENT;
    msg.header.source_module = BIO_MODULE_SECURITY;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = get_timestamp_us();

    msg.threats_detected = bridge->stats.threat_alerts_sent;
    msg.threats_mitigated = bridge->stats.lockdowns_initiated;
    msg.access_requests = bridge->stats.access_requests_processed;
    msg.access_denied = bridge->stats.access_denied_count;
    msg.avg_response_time_us = bridge->stats.avg_message_latency_us;
    msg.detection_rate = 1.0f;  /* Would be calculated */
    msg.false_positive_rate = 0.0f;  /* Would be tracked */
    msg.active_lockdowns = bridge->internal.lockdown_active ? 1 : 0;
    msg.current_security_level = bridge->internal.current_security_level;
    msg.timestamp_us = msg.header.timestamp_us;

    bridge->last_metrics_us = msg.timestamp_us;
    bridge->stats.broadcasts_sent++;

    return 0;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int security_bio_bridge_subscribe_module(
    security_bio_bridge_t* bridge,
    uint32_t module_id,
    uint64_t msg_types,
    uint32_t trust_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_subscribe_module: bridge is NULL");
        return -1;
    }

    /* Check if already subscribed */
    security_bio_subscription_t* existing = find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask = msg_types;
        existing->trust_level = trust_level;
        return 0;
    }

    /* Find free slot */
    if (bridge->subscription_count >= bridge->subscription_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "security_bio_bridge_subscribe_module: capacity exceeded");
        return -1;  /* Full */
    }

    security_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = get_timestamp_us();
    sub->messages_sent = 0;
    sub->trust_level = trust_level;

    bridge->subscription_count++;
    bridge->stats.active_subscriptions = bridge->subscription_count;

    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    return 0;
}

int security_bio_bridge_unsubscribe_module(
    security_bio_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_unsubscribe_module: bridge is NULL");
        return -1;
    }

    security_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_unsubscribe_module: sub is NULL");
        return -1;
    }

    sub->active = false;
    bridge->stats.active_subscriptions--;

    return 0;
}

int security_bio_bridge_update_trust_level(
    security_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t trust_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_update_trust_level: bridge is NULL");
        return -1;
    }

    security_bio_subscription_t* sub = find_subscription(bridge, module_id);
    if (!sub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_update_trust_level: sub is NULL");
        return -1;
    }

    sub->trust_level = trust_level;
    return 0;
}

uint32_t security_bio_bridge_get_subscriber_count(
    const security_bio_bridge_t* bridge,
    security_bio_msg_type_t msg_type
) {
    return count_subscribers_for_type(bridge, msg_type);
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int security_bio_bridge_get_stats(
    const security_bio_bridge_t* bridge,
    security_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int security_bio_bridge_reset_stats(security_bio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_bio_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = bridge->subscription_count;

    return 0;
}

const char* security_bio_msg_type_name(security_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case SECURITY_MSG_THREAT_DETECTED:    return "THREAT_DETECTED";
        case SECURITY_MSG_ANOMALY_ALERT:      return "ANOMALY_ALERT";
        case SECURITY_MSG_PATTERN_MATCH:      return "PATTERN_MATCH";
        case SECURITY_MSG_BBB_BREACH:         return "BBB_BREACH";
        case SECURITY_MSG_RATE_LIMIT_HIT:     return "RATE_LIMIT_HIT";
        case SECURITY_MSG_INJECTION_ATTEMPT:  return "INJECTION_ATTEMPT";
        case SECURITY_MSG_ACCESS_REQUEST:     return "ACCESS_REQUEST";
        case SECURITY_MSG_ACCESS_RESPONSE:    return "ACCESS_RESPONSE";
        case SECURITY_MSG_CAPABILITY_CHECK:   return "CAPABILITY_CHECK";
        case SECURITY_MSG_CAPABILITY_RESULT:  return "CAPABILITY_RESULT";
        case SECURITY_MSG_POLICY_DECISION:    return "POLICY_DECISION";
        case SECURITY_MSG_LEVEL_CHANGE:       return "LEVEL_CHANGE";
        case SECURITY_MSG_CONSENSUS_UPDATE:   return "CONSENSUS_UPDATE";
        case SECURITY_MSG_LOCKDOWN_START:     return "LOCKDOWN_START";
        case SECURITY_MSG_LOCKDOWN_END:       return "LOCKDOWN_END";
        case SECURITY_MSG_AUDIT_EVENT:        return "AUDIT_EVENT";
        case SECURITY_MSG_POLICY_UPDATE:      return "POLICY_UPDATE";
        case SECURITY_MSG_POLICY_REQUEST:     return "POLICY_REQUEST";
        case SECURITY_MSG_CONSENSUS_PROPOSAL: return "CONSENSUS_PROPOSAL";
        case SECURITY_MSG_CONSENSUS_VOTE:     return "CONSENSUS_VOTE";
        case SECURITY_MSG_CONSENSUS_RESULT:   return "CONSENSUS_RESULT";
        case SECURITY_MSG_HEARTBEAT:          return "HEARTBEAT";
        case SECURITY_MSG_METRICS_UPDATE:     return "METRICS_UPDATE";
        case SECURITY_MSG_COVERAGE_REPORT:    return "COVERAGE_REPORT";
        case SECURITY_MSG_HEALTH_STATUS:      return "HEALTH_STATUS";
        default:                              return "UNKNOWN";
    }
}

const char* security_threat_type_name(security_threat_type_t threat_type) {
    switch (threat_type) {
        case SECURITY_THREAT_NONE:            return "NONE";
        case SECURITY_THREAT_INJECTION:       return "INJECTION";
        case SECURITY_THREAT_ANOMALY:         return "ANOMALY";
        case SECURITY_THREAT_PATTERN_MATCH:   return "PATTERN_MATCH";
        case SECURITY_THREAT_RATE_VIOLATION:  return "RATE_VIOLATION";
        case SECURITY_THREAT_BBB_BREACH:      return "BBB_BREACH";
        case SECURITY_THREAT_CAPABILITY_ABUSE: return "CAPABILITY_ABUSE";
        case SECURITY_THREAT_CONSENSUS_ATTACK: return "CONSENSUS_ATTACK";
        case SECURITY_THREAT_EXCITOTOXICITY:  return "EXCITOTOXICITY";
        case SECURITY_THREAT_SYNAPTIC_POISON: return "SYNAPTIC_POISON";
        case SECURITY_THREAT_NEUROMOD_HIJACK: return "NEUROMOD_HIJACK";
        case SECURITY_THREAT_HEBBIAN_POISON:  return "HEBBIAN_POISON";
        case SECURITY_THREAT_UNKNOWN:         return "UNKNOWN";
        default:                              return "UNDEFINED";
    }
}

const char* security_access_type_name(security_access_type_t access_type) {
    switch (access_type) {
        case SECURITY_ACCESS_READ:            return "READ";
        case SECURITY_ACCESS_WRITE:           return "WRITE";
        case SECURITY_ACCESS_EXECUTE:         return "EXECUTE";
        case SECURITY_ACCESS_CREATE:          return "CREATE";
        case SECURITY_ACCESS_DELETE:          return "DELETE";
        case SECURITY_ACCESS_ADMIN:           return "ADMIN";
        case SECURITY_ACCESS_NETWORK:         return "NETWORK";
        case SECURITY_ACCESS_MEMORY:          return "MEMORY";
        case SECURITY_ACCESS_WEIGHT_UPDATE:   return "WEIGHT_UPDATE";
        case SECURITY_ACCESS_NEUROMOD:        return "NEUROMOD";
        case SECURITY_ACCESS_CONSENSUS:       return "CONSENSUS";
        default:                              return "UNKNOWN";
    }
}

const char* security_decision_name(security_decision_t decision) {
    switch (decision) {
        case SECURITY_DECISION_PENDING:       return "PENDING";
        case SECURITY_DECISION_GRANTED:       return "GRANTED";
        case SECURITY_DECISION_DENIED:        return "DENIED";
        case SECURITY_DECISION_RATE_LIMITED:  return "RATE_LIMITED";
        case SECURITY_DECISION_REQUIRES_CONSENSUS: return "REQUIRES_CONSENSUS";
        case SECURITY_DECISION_LOCKDOWN:      return "LOCKDOWN";
        default:                              return "UNKNOWN";
    }
}

void security_bio_bridge_print_summary(const security_bio_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Bio-Async Bridge: NULL\n");
        return;
    }

    printf("\n");
    printf("============================================================\n");
    printf("          SECURITY BIO-ASYNC BRIDGE SUMMARY\n");
    printf("============================================================\n");
    printf("\n");

    /* Connection Status */
    printf("CONNECTION STATUS\n");
    printf("  Connected:              %s\n", bridge->connected ? "YES" : "NO");
    printf("  Security Level:         %u\n", bridge->internal.current_security_level);
    printf("  Lockdown Active:        %s\n",
           bridge->internal.lockdown_active ? "YES" : "NO");
    printf("\n");

    /* Subscription Stats */
    printf("SUBSCRIPTIONS\n");
    printf("  Active:                 %u\n", bridge->stats.active_subscriptions);
    printf("  Peak:                   %u\n", bridge->stats.peak_subscriptions);
    printf("  Capacity:               %u\n", bridge->subscription_capacity);
    printf("\n");

    /* Message Stats */
    printf("MESSAGE STATISTICS\n");
    printf("  Sent:                   %lu\n", (unsigned long)bridge->stats.messages_sent);
    printf("  Received:               %lu\n", (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts:             %lu\n", (unsigned long)bridge->stats.broadcasts_sent);
    printf("  Dropped:                %lu\n", (unsigned long)bridge->stats.messages_dropped);
    printf("\n");

    /* Security Stats */
    printf("SECURITY STATISTICS\n");
    printf("  Threat Alerts:          %lu\n", (unsigned long)bridge->stats.threat_alerts_sent);
    printf("  Anomaly Alerts:         %lu\n", (unsigned long)bridge->stats.anomaly_alerts_sent);
    printf("  Access Requests:        %lu\n", (unsigned long)bridge->stats.access_requests_processed);
    printf("  Access Denied:          %lu\n", (unsigned long)bridge->stats.access_denied_count);
    printf("  Lockdowns:              %lu\n", (unsigned long)bridge->stats.lockdowns_initiated);
    printf("  Consensus Proposals:    %lu\n", (unsigned long)bridge->stats.consensus_proposals);
    printf("\n");

    /* Timing Stats */
    printf("TIMING STATISTICS\n");
    printf("  Avg Latency (us):       %.2f\n", bridge->stats.avg_message_latency_us);
    printf("  Max Latency (us):       %.2f\n", bridge->stats.max_message_latency_us);
    printf("  Avg Threat Response:    %.2f us\n", bridge->stats.avg_threat_response_us);
    printf("\n");

    /* Error Stats */
    printf("ERROR STATISTICS\n");
    printf("  Handler Errors:         %lu\n", (unsigned long)bridge->stats.handler_errors);
    printf("  Routing Errors:         %lu\n", (unsigned long)bridge->stats.routing_errors);
    printf("\n");

    printf("============================================================\n");
}
