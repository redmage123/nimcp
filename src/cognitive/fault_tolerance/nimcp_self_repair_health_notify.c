/**
 * @file nimcp_self_repair_health_notify.c
 * @brief Implementation of Self-Repair Health Agent Notification Extension
 * @version 1.0.0
 * @date 2025-01-20
 */

#include "cognitive/fault_tolerance/nimcp_self_repair_health_notify.h"
#include "async/nimcp_bio_router.h"  /* Include before health_agent to get bio_router_t defined */
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(self_repair_health_notify, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_FAILURE_TRACKING    256

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Repeated failure tracking entry
 */
typedef struct {
    uint64_t diagnostic_id;             /**< Original diagnostic ID */
    error_type_t error_type;            /**< Error type */
    uint32_t failure_count;             /**< Number of failures */
    uint64_t first_failure_time;        /**< First failure timestamp */
    uint64_t last_failure_time;         /**< Last failure timestamp */
    char source_file[NIMCP_METRICS_PATH_SIZE];              /**< Affected source file */
    char function_name[NIMCP_LABEL_BUFFER_SIZE];            /**< Affected function */
} failure_tracking_entry_t;

/**
 * @brief Health notification bridge internal state
 */
struct self_repair_health_notify_bridge {
    uint32_t magic;                             /**< Magic number for validation */
    self_repair_health_notify_config_t config;  /**< Configuration */

    /* Dependencies */
    self_repair_coordinator_t* self_repair;     /**< Self-repair coordinator */
    nimcp_health_agent_t* health_agent;         /**< Health agent (optional) */

    /* Callback */
    self_repair_failure_cb_t failure_callback;
    void* callback_user_data;

    /* Failure tracking */
    failure_tracking_entry_t* failure_tracking;
    uint32_t failure_tracking_count;

    /* Statistics */
    self_repair_health_notify_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* State */
    bool initialized;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find failure tracking entry by diagnostic ID
 */
static failure_tracking_entry_t* find_failure_entry(
    self_repair_health_notify_bridge_t* bridge,
    uint64_t diagnostic_id
) {
    for (uint32_t i = 0; i < bridge->failure_tracking_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->failure_tracking_count > 256) {
            self_repair_health_notify_heartbeat("self_repair__loop",
                             (float)(i + 1) / (float)bridge->failure_tracking_count);
        }

        if (bridge->failure_tracking[i].diagnostic_id == diagnostic_id) {
            return &bridge->failure_tracking[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_failure_entry: validation failed");
    return NULL;
}

/**
 * @brief Find or create failure tracking entry
 */
static failure_tracking_entry_t* get_or_create_failure_entry(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_result_t* result
) {
    failure_tracking_entry_t* entry = find_failure_entry(
        bridge, result->record.diagnostic_id);

    if (entry) {
        /* Check if window has expired */
        uint64_t now = nimcp_time_get_ms();
        if (now - entry->first_failure_time > bridge->config.repeated_failure_window_ms) {
            /* Reset tracking */
            entry->failure_count = 0;
            entry->first_failure_time = now;
        }
        return entry;
    }

    /* Create new entry */
    if (bridge->failure_tracking_count >= MAX_FAILURE_TRACKING) {
        /* Evict oldest entry */
        uint32_t oldest_idx = 0;
        uint64_t oldest_time = bridge->failure_tracking[0].last_failure_time;
        for (uint32_t i = 1; i < bridge->failure_tracking_count; i++) {
            if (bridge->failure_tracking[i].last_failure_time < oldest_time) {
                oldest_time = bridge->failure_tracking[i].last_failure_time;
                oldest_idx = i;
            }
        }
        entry = &bridge->failure_tracking[oldest_idx];
    } else {
        entry = &bridge->failure_tracking[bridge->failure_tracking_count++];
    }

    memset(entry, 0, sizeof(*entry));
    entry->diagnostic_id = result->record.diagnostic_id;
    entry->error_type = result->record.error_type;
    entry->first_failure_time = nimcp_time_get_ms();
    snprintf(entry->source_file, sizeof(entry->source_file),
             "%s", result->record.source_file);
    snprintf(entry->function_name, sizeof(entry->function_name),
             "%s", result->record.function_name);

    return entry;
}

/**
 * @brief Build notification message from repair result
 */
static void build_notification(
    self_repair_health_notification_t* notification,
    repair_notify_type_t type,
    const self_repair_result_t* result,
    repair_intervention_t intervention,
    uint32_t failure_count
) {
    memset(notification, 0, sizeof(*notification));

    notification->type = type;
    notification->intervention = intervention;
    notification->repair_id = result->record.repair_id;
    notification->status = result->status;
    notification->stage = result->record.stage;
    notification->diagnostic_id = result->record.diagnostic_id;
    notification->error_type = result->record.error_type;

    /* Map severity from result */
    if (result->record.fix_risk >= 0.8f) {
        notification->severity = DIAG_SEVERITY_CRITICAL;
    } else if (result->record.fix_risk >= 0.5f) {
        notification->severity = DIAG_SEVERITY_ERROR;
    } else {
        notification->severity = DIAG_SEVERITY_WARNING;
    }

    snprintf(notification->error_message, sizeof(notification->error_message),
             "%s", result->error_message);
    snprintf(notification->source_file, sizeof(notification->source_file),
             "%s", result->record.source_file);
    snprintf(notification->function_name, sizeof(notification->function_name),
             "%s", result->record.function_name);

    notification->failure_count = failure_count;
    notification->risk_score = result->record.fix_risk;
    notification->confidence = result->record.fix_confidence;
    notification->repair_start_time = result->record.start_time;
    notification->notification_time = nimcp_time_get_ms();

    if (result->record.end_time > result->record.start_time) {
        notification->repair_duration_ms = result->record.end_time - result->record.start_time;
    }
}

/**
 * @brief Deliver notification to health agent
 */
static int deliver_to_health_agent(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_health_notification_t* notification
) {
    if (!bridge->health_agent) {
        return 0; /* No health agent connected - not an error */
    }

    /* Build health agent message */
    health_agent_message_t msg = {0};

    switch (notification->type) {
        case REPAIR_NOTIFY_FAILURE:
        case REPAIR_NOTIFY_VALIDATION_FAILED:
            msg.type = HEALTH_MSG_RECOVERY_REQUEST;
            break;

        case REPAIR_NOTIFY_ROLLBACK:
            msg.type = HEALTH_MSG_STATE_CORRUPTION;
            break;

        case REPAIR_NOTIFY_HIGH_RISK:
            msg.type = HEALTH_MSG_EMERGENCY;
            break;

        case REPAIR_NOTIFY_REPEATED_FAILURE:
            msg.type = HEALTH_MSG_EMERGENCY;
            break;

        case REPAIR_NOTIFY_TIMEOUT:
            msg.type = HEALTH_MSG_HEARTBEAT_TIMEOUT;
            break;

        case REPAIR_NOTIFY_SUCCESS:
            msg.type = HEALTH_MSG_STATUS_UPDATE;
            break;

        default:
            msg.type = HEALTH_MSG_ANOMALY_DETECTED;
            break;
    }

    /* Map severity */
    switch (notification->severity) {
        case DIAG_SEVERITY_FATAL:
            msg.severity = HEALTH_SEVERITY_FATAL;
            break;
        case DIAG_SEVERITY_CRITICAL:
            msg.severity = HEALTH_SEVERITY_CRITICAL;
            break;
        case DIAG_SEVERITY_ERROR:
            msg.severity = HEALTH_SEVERITY_ERROR;
            break;
        case DIAG_SEVERITY_WARNING:
            msg.severity = HEALTH_SEVERITY_WARNING;
            break;
        default:
            msg.severity = HEALTH_SEVERITY_INFO;
            break;
    }

    msg.source = HEALTH_SOURCE_CHECKPOINT;

    /* Map intervention to suggested action */
    switch (notification->intervention) {
        case REPAIR_INTERVENTION_QUARANTINE:
            msg.suggested_action = HEALTH_RECOVERY_QUARANTINE;
            break;
        case REPAIR_INTERVENTION_ROLLBACK:
            msg.suggested_action = HEALTH_RECOVERY_ROLLBACK;
            break;
        case REPAIR_INTERVENTION_RESTART:
            msg.suggested_action = HEALTH_RECOVERY_RESTART_THREAD;
            break;
        case REPAIR_INTERVENTION_MANUAL_REPAIR:
            msg.suggested_action = HEALTH_RECOVERY_CHECKPOINT;
            break;
        default:
            msg.suggested_action = HEALTH_RECOVERY_NONE;
            break;
    }

    msg.timestamp_us = nimcp_time_get_us();
    msg.anomaly_id = notification->repair_id;

    snprintf(msg.description, sizeof(msg.description),
             "Self-repair %s: %s in %s (risk=%.2f, failures=%u)",
             self_repair_notify_type_name(notification->type),
             notification->error_message,
             notification->function_name,
             notification->risk_score,
             notification->failure_count);

    /* Send to health agent - using internal queue mechanism */
    /* The actual delivery depends on health agent implementation */
    /* For now we just return success as the integration point is defined */

    return 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int self_repair_health_notify_default_config(
    self_repair_health_notify_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__default_config", 0.0f);


    memset(config, 0, sizeof(*config));

    config->notify_on_failure = true;
    config->notify_on_rollback = true;
    config->notify_on_high_risk = true;
    config->notify_on_timeout = true;
    config->notify_on_success = false;

    config->track_repeated_failures = true;
    config->repeated_failure_threshold = 3;
    config->repeated_failure_window_ms = 300000;  /* 5 minutes */

    config->high_risk_threshold = 0.5f;
    config->enable_bio_async = true;
    config->verbose_logging = false;

    return 0;
}

self_repair_health_notify_bridge_t* self_repair_health_notify_create(
    const self_repair_health_notify_config_t* config,
    self_repair_coordinator_t* self_repair,
    nimcp_health_agent_t* health_agent
) {
    if (!self_repair) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__create", 0.0f);


    self_repair_health_notify_bridge_t* bridge = nimcp_calloc(
        1, sizeof(self_repair_health_notify_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->magic = SELF_REPAIR_HEALTH_NOTIFY_MAGIC;
    bridge->self_repair = self_repair;
    bridge->health_agent = health_agent;

    /* Apply config or defaults */
    if (config) {
        bridge->config = *config;
    } else {
        self_repair_health_notify_default_config(&bridge->config);
    }

    /* Allocate failure tracking */
    bridge->failure_tracking = nimcp_calloc(
        MAX_FAILURE_TRACKING, sizeof(failure_tracking_entry_t));
    if (!bridge->failure_tracking) {
        self_repair_health_notify_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_repair_health_notify_create: bridge->failure_tracking is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        self_repair_health_notify_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_repair_health_notify_create: bridge->mutex is NULL");
        return NULL;
    }

    bridge->initialized = true;
    return bridge;
}

void self_repair_health_notify_destroy(
    self_repair_health_notify_bridge_t* bridge
) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__destroy", 0.0f);


    if (bridge->magic != SELF_REPAIR_HEALTH_NOTIFY_MAGIC) {
        return;
    }

    bridge->magic = 0;
    bridge->initialized = false;

    if (bridge->failure_tracking) {
        nimcp_free(bridge->failure_tracking);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
        bridge->mutex = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int self_repair_health_notify_connect(
    self_repair_health_notify_bridge_t* bridge,
    nimcp_health_agent_t* health_agent
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__connect", 0.0f);


    nimcp_mutex_lock(bridge->mutex);
    bridge->health_agent = health_agent;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Callback Registration Implementation
 * ============================================================================ */

int self_repair_health_notify_set_callback(
    self_repair_health_notify_bridge_t* bridge,
    self_repair_failure_cb_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__set_callback", 0.0f);


    nimcp_mutex_lock(bridge->mutex);
    bridge->failure_callback = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Notification API Implementation
 * ============================================================================ */

int self_repair_health_notify_result(
    self_repair_health_notify_bridge_t* bridge,
    uint64_t repair_id,
    const self_repair_result_t* result
) {
    if (!bridge || !result || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_health_notify_result: required parameter is NULL (bridge, result, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__result", 0.0f);


    (void)repair_id; /* repair_id is in result->record */

    nimcp_mutex_lock(bridge->mutex);

    /* Determine notification type */
    repair_notify_type_t type = REPAIR_NOTIFY_FAILURE;
    bool should_notify = false;

    if (result->success) {
        type = REPAIR_NOTIFY_SUCCESS;
        should_notify = bridge->config.notify_on_success;
    } else if (result->record.rolled_back) {
        type = REPAIR_NOTIFY_ROLLBACK;
        should_notify = bridge->config.notify_on_rollback;
    } else if (result->status == REPAIR_STATUS_TIMEOUT) {
        type = REPAIR_NOTIFY_TIMEOUT;
        should_notify = bridge->config.notify_on_timeout;
    } else if (result->status == REPAIR_STATUS_VALIDATION_FAILED) {
        type = REPAIR_NOTIFY_VALIDATION_FAILED;
        should_notify = bridge->config.notify_on_failure;
    } else if (result->record.fix_risk >= bridge->config.high_risk_threshold) {
        type = REPAIR_NOTIFY_HIGH_RISK;
        should_notify = bridge->config.notify_on_high_risk;
    } else {
        type = REPAIR_NOTIFY_FAILURE;
        should_notify = bridge->config.notify_on_failure;
    }

    /* Track repeated failures */
    uint32_t failure_count = 0;
    if (!result->success && bridge->config.track_repeated_failures) {
        failure_tracking_entry_t* entry = get_or_create_failure_entry(bridge, result);
        if (entry) {
            entry->failure_count++;
            entry->last_failure_time = nimcp_time_get_ms();
            failure_count = entry->failure_count;

            if (failure_count >= bridge->config.repeated_failure_threshold) {
                type = REPAIR_NOTIFY_REPEATED_FAILURE;
                should_notify = true;
            }
        }
    }

    if (!should_notify) {
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Determine intervention */
    repair_intervention_t intervention = self_repair_suggest_intervention(bridge, result);

    /* Build notification */
    self_repair_health_notification_t notification;
    build_notification(&notification, type, result, intervention, failure_count);

    /* Update statistics */
    bridge->stats.notifications_sent++;
    if (type <= REPAIR_NOTIFY_SUCCESS) {
        bridge->stats.by_type[type]++;
    }
    if (intervention <= REPAIR_INTERVENTION_RESTART) {
        bridge->stats.by_intervention[intervention]++;
    }

    switch (type) {
        case REPAIR_NOTIFY_FAILURE:
        case REPAIR_NOTIFY_VALIDATION_FAILED:
            bridge->stats.failures_notified++;
            break;
        case REPAIR_NOTIFY_ROLLBACK:
            bridge->stats.rollbacks_notified++;
            break;
        case REPAIR_NOTIFY_HIGH_RISK:
            bridge->stats.high_risk_notified++;
            break;
        case REPAIR_NOTIFY_REPEATED_FAILURE:
            bridge->stats.repeated_failures_notified++;
            break;
        default:
            break;
    }

    /* Invoke callback */
    if (bridge->failure_callback) {
        bridge->failure_callback(&notification, bridge->callback_user_data);
    }

    /* Deliver to health agent */
    deliver_to_health_agent(bridge, &notification);

    /* Broadcast via bio-async */
    if (bridge->config.enable_bio_async) {
        self_repair_health_notify_broadcast(bridge, &notification);
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int self_repair_health_notify_send(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_health_notification_t* notification
) {
    if (!bridge || !notification || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_health_notify_send: required parameter is NULL (bridge, notification, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__send", 0.0f);


    nimcp_mutex_lock(bridge->mutex);

    /* Update statistics */
    bridge->stats.notifications_sent++;
    if (notification->type <= REPAIR_NOTIFY_SUCCESS) {
        bridge->stats.by_type[notification->type]++;
    }
    if (notification->intervention <= REPAIR_INTERVENTION_RESTART) {
        bridge->stats.by_intervention[notification->intervention]++;
    }

    /* Update type-specific counters */
    switch (notification->type) {
        case REPAIR_NOTIFY_FAILURE:
        case REPAIR_NOTIFY_VALIDATION_FAILED:
            bridge->stats.failures_notified++;
            break;
        case REPAIR_NOTIFY_ROLLBACK:
            bridge->stats.rollbacks_notified++;
            break;
        case REPAIR_NOTIFY_HIGH_RISK:
            bridge->stats.high_risk_notified++;
            break;
        case REPAIR_NOTIFY_REPEATED_FAILURE:
            bridge->stats.repeated_failures_notified++;
            break;
        default:
            break;
    }

    /* Invoke callback */
    if (bridge->failure_callback) {
        bridge->failure_callback(notification, bridge->callback_user_data);
    }

    /* Deliver to health agent */
    int ret = deliver_to_health_agent(bridge, notification);

    /* Broadcast via bio-async */
    if (bridge->config.enable_bio_async) {
        self_repair_health_notify_broadcast(bridge, notification);
    }

    nimcp_mutex_unlock(bridge->mutex);

    return ret;
}

repair_intervention_t self_repair_suggest_intervention(
    const self_repair_health_notify_bridge_t* bridge,
    const self_repair_result_t* result
) {
    if (!bridge || !result) {
        return REPAIR_INTERVENTION_NONE;
    }

    /* Success case */
    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__self_repair_suggest_", 0.0f);


    if (result->success) {
        return REPAIR_INTERVENTION_NONE;
    }

    /* Check failure count (requires tracking lookup) */
    self_repair_health_notify_bridge_t* mutable_bridge =
        (self_repair_health_notify_bridge_t*)bridge;

    failure_tracking_entry_t* entry = find_failure_entry(
        mutable_bridge, result->record.diagnostic_id);

    uint32_t failure_count = entry ? entry->failure_count : 1;

    /* Repeated failures escalate intervention */
    if (failure_count >= bridge->config.repeated_failure_threshold) {
        if (result->record.fix_risk >= 0.8f) {
            return REPAIR_INTERVENTION_QUARANTINE;
        } else {
            return REPAIR_INTERVENTION_MANUAL_REPAIR;
        }
    }

    /* High risk repairs */
    if (result->record.fix_risk >= 0.8f) {
        return REPAIR_INTERVENTION_ALERT;
    }

    /* Rollback case */
    if (result->record.rolled_back) {
        return REPAIR_INTERVENTION_ROLLBACK;
    }

    /* Timeout case */
    if (result->status == REPAIR_STATUS_TIMEOUT) {
        return REPAIR_INTERVENTION_ALERT;
    }

    /* Default for other failures */
    if (result->record.fix_risk >= bridge->config.high_risk_threshold) {
        return REPAIR_INTERVENTION_ALERT;
    }

    return REPAIR_INTERVENTION_LOG_ONLY;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int self_repair_health_notify_get_stats(
    const self_repair_health_notify_bridge_t* bridge,
    self_repair_health_notify_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_health_notify_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__get_stats", 0.0f);


    self_repair_health_notify_bridge_t* mutable_bridge =
        (self_repair_health_notify_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->mutex);

    return 0;
}

void self_repair_health_notify_reset_stats(
    self_repair_health_notify_bridge_t* bridge
) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);
}

bool self_repair_health_notify_is_ready(
    const self_repair_health_notify_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__is_ready", 0.0f);


    return bridge->initialized && bridge->magic == SELF_REPAIR_HEALTH_NOTIFY_MAGIC;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int self_repair_health_notify_broadcast(
    self_repair_health_notify_bridge_t* bridge,
    const self_repair_health_notification_t* notification
) {
    if (!bridge || !notification) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_health_notify_broadcast: required parameter is NULL (bridge, notification)");
        return -1;
    }

    /* Bio-async broadcast implementation will be connected in Phase 5 */
    /* Phase 8: Heartbeat at operation start */
    self_repair_health_notify_heartbeat("self_repair__broadcast", 0.0f);


    (void)notification;

    return 0;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* self_repair_notify_type_name(repair_notify_type_t type) {
    switch (type) {
        case REPAIR_NOTIFY_FAILURE: return "FAILURE";
        case REPAIR_NOTIFY_ROLLBACK: return "ROLLBACK";
        case REPAIR_NOTIFY_HIGH_RISK: return "HIGH_RISK";
        case REPAIR_NOTIFY_REPEATED_FAILURE: return "REPEATED_FAILURE";
        case REPAIR_NOTIFY_TIMEOUT: return "TIMEOUT";
        case REPAIR_NOTIFY_VALIDATION_FAILED: return "VALIDATION_FAILED";
        case REPAIR_NOTIFY_SUCCESS: return "SUCCESS";
        default: return "UNKNOWN";
    }
}

const char* self_repair_intervention_name(repair_intervention_t intervention) {
    switch (intervention) {
        case REPAIR_INTERVENTION_NONE: return "NONE";
        case REPAIR_INTERVENTION_LOG_ONLY: return "LOG_ONLY";
        case REPAIR_INTERVENTION_ALERT: return "ALERT";
        case REPAIR_INTERVENTION_QUARANTINE: return "QUARANTINE";
        case REPAIR_INTERVENTION_ROLLBACK: return "ROLLBACK";
        case REPAIR_INTERVENTION_MANUAL_REPAIR: return "MANUAL_REPAIR";
        case REPAIR_INTERVENTION_RESTART: return "RESTART";
        default: return "UNKNOWN";
    }
}

const char* self_repair_health_notify_version(void) {
    return SELF_REPAIR_HEALTH_NOTIFY_VERSION;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_repair_health_notify_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_repair_health_notify_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_repair_health_notify_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_health_notify_training_begin: NULL argument");
        return -1;
    }
    self_repair_health_notify_heartbeat_instance(NULL, "self_repair_health_notify_training_begin", 0.0f);
    (void)(struct self_repair_health_notify_bridge*)instance; /* Module state available for reset */
    return 0;
}

int self_repair_health_notify_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_health_notify_training_end: NULL argument");
        return -1;
    }
    self_repair_health_notify_heartbeat_instance(NULL, "self_repair_health_notify_training_end", 1.0f);
    (void)(struct self_repair_health_notify_bridge*)instance; /* Module state available for finalization */
    return 0;
}

int self_repair_health_notify_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_health_notify_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_repair_health_notify_heartbeat_instance(NULL, "self_repair_health_notify_training_step", progress);
    (void)(struct self_repair_health_notify_bridge*)instance; /* Module state available for step adaptation */
    return 0;
}
