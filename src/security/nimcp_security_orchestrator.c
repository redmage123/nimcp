/**
 * @file nimcp_security_orchestrator.c
 * @brief Implementation of security orchestrator for bridge coordination
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "security/nimcp_security_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_orchestrator, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Subscription entry
 */
typedef struct {
    uint32_t subscriber_id;             /**< Subscriber bridge ID */
    security_event_type_t event_type;   /**< Event type subscribed to */
    security_event_callback_t callback; /**< Callback function */
    void* user_data;                    /**< User context */
    bool is_active;                     /**< Whether subscription is active */
    bool subscribe_to_bridge;           /**< Subscribe to bridge instead of event */
    security_bridge_type_t source_type; /**< Source bridge type (if subscribe_to_bridge) */
} security_subscription_t;

/**
 * @brief Async event queue entry
 */
typedef struct {
    security_event_data_t event;        /**< Event data */
    uint32_t publisher_id;              /**< Publisher bridge ID */
    bool is_valid;                      /**< Whether entry is valid */
} security_event_queue_entry_t;

/**
 * @brief Internal orchestrator structure
 */
struct security_orchestrator_struct {
    /* Configuration */
    security_orch_config_t config;

    /* State */
    security_orch_state_t state;
    bool is_locked_down;
    char lockdown_reason[NIMCP_ERROR_BUFFER_MEDIUM];
    uint64_t lockdown_time;

    /* Registered bridges */
    security_bridge_info_t bridges[SEC_ORCH_MAX_BRIDGES];
    uint32_t num_bridges;
    uint32_t next_bridge_id;

    /* Subscriptions */
    security_subscription_t subscriptions[SEC_ORCH_MAX_SUBSCRIPTIONS];
    uint32_t num_subscriptions;

    /* Threat tracking */
    float unified_threat_level;
    security_severity_t current_severity;
    uint32_t active_threat_count;
    uint64_t last_decay_time;

    /* Event queue (for async) */
    security_event_queue_entry_t* event_queue;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Statistics */
    security_orch_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;

    /* Integration handles */
    void* immune_system;
    void* cognitive_hub;
    bool bio_async_connected;

    /* Timestamps */
    uint64_t create_time;
    uint32_t next_event_id;
};

/* ============================================================================
 * HELPER MACROS
 * ============================================================================ */

#define ORCH_LOCK(o) nimcp_mutex_lock((o)->mutex)
#define ORCH_UNLOCK(o) nimcp_mutex_unlock((o)->mutex)

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    return nimcp_time_monotonic_us();
}

/**
 * @brief Find bridge by ID
 */
static int find_bridge_index(security_orchestrator_t orch, uint32_t bridge_id)
{
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].bridge_id == bridge_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_bridge_index: validation failed");
    return -1;
}

/**
 * @brief Find bridge by type
 */
static int find_bridge_by_type(security_orchestrator_t orch, security_bridge_type_t type)
{
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].type == type) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_bridge_by_type: validation failed");
    return -1;
}

/**
 * @brief Calculate unified threat level from all bridges
 */
static void calculate_unified_threat(security_orchestrator_t orch)
{
    if (orch->num_bridges == 0) {
        orch->unified_threat_level = 0.0f;
        orch->current_severity = SEC_SEVERITY_NONE;
        orch->active_threat_count = 0;
        return;
    }

    float max_threat = 0.0f;
    float sum_threat = 0.0f;
    uint32_t active_count = 0;
    uint32_t reporting_count = 0;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (!orch->bridges[i].is_active) continue;

        float threat = orch->bridges[i].current_threat_level;
        if (threat > 0.0f) {
            reporting_count++;
            sum_threat += threat;
            if (threat > max_threat) {
                max_threat = threat;
            }
            if (threat > orch->config.medium_threshold) {
                active_count++;
            }
        }
    }

    /* Unified threat = weighted combination of max and average */
    if (reporting_count > 0) {
        float avg_threat = sum_threat / reporting_count;
        /* 70% max, 30% average for unified threat */
        orch->unified_threat_level = 0.7f * max_threat + 0.3f * avg_threat;
    } else {
        orch->unified_threat_level = 0.0f;
    }

    /* Clamp to [0, 1] */
    if (orch->unified_threat_level > 1.0f) {
        orch->unified_threat_level = 1.0f;
    }

    /* Update severity */
    if (orch->unified_threat_level >= orch->config.critical_threshold) {
        orch->current_severity = SEC_SEVERITY_CRITICAL;
    } else if (orch->unified_threat_level >= orch->config.high_threshold) {
        orch->current_severity = SEC_SEVERITY_HIGH;
    } else if (orch->unified_threat_level >= orch->config.medium_threshold) {
        orch->current_severity = SEC_SEVERITY_MEDIUM;
    } else if (orch->unified_threat_level > 0.0f) {
        orch->current_severity = SEC_SEVERITY_LOW;
    } else {
        orch->current_severity = SEC_SEVERITY_NONE;
    }

    orch->active_threat_count = active_count;

    /* Update statistics */
    if (orch->unified_threat_level > orch->stats.peak_threat_level) {
        orch->stats.peak_threat_level = orch->unified_threat_level;
    }
}

/**
 * @brief Apply threat decay
 */
static void apply_threat_decay(security_orchestrator_t orch)
{
    uint64_t now = get_timestamp_us();
    uint64_t elapsed_us = now - orch->last_decay_time;
    float elapsed_sec = (float)elapsed_us / 1000000.0f;

    if (elapsed_sec < 0.1f) {
        return;  /* Don't decay more often than 100ms */
    }

    float decay_factor = expf(-orch->config.threat_decay_rate * elapsed_sec);

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].current_threat_level > 0.0f) {
            orch->bridges[i].current_threat_level *= decay_factor;
            if (orch->bridges[i].current_threat_level < 0.01f) {
                orch->bridges[i].current_threat_level = 0.0f;
            }
        }
    }

    orch->last_decay_time = now;
    calculate_unified_threat(orch);
}

/**
 * @brief Deliver event to subscribers
 */
static void deliver_event(
    security_orchestrator_t orch,
    uint32_t publisher_id,
    const security_event_data_t* event
)
{
    uint64_t start_time = get_timestamp_us();

    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        security_subscription_t* sub = &orch->subscriptions[i];
        if (!sub->is_active) continue;

        bool should_deliver = false;

        if (sub->subscribe_to_bridge) {
            /* Deliver if event is from subscribed bridge type */
            should_deliver = (event->source == sub->source_type);
        } else {
            /* Deliver if event type matches */
            should_deliver = (event->event_type == sub->event_type);
        }

        if (should_deliver && sub->callback) {
            /* Don't deliver to self */
            if (sub->subscriber_id != publisher_id) {
                sub->callback(event, sub->user_data);
                orch->stats.events_delivered++;
            }
        }
    }

    /* Update timing statistics */
    uint64_t elapsed = get_timestamp_us() - start_time;
    orch->stats.avg_event_latency_us =
        (orch->stats.avg_event_latency_us * orch->stats.events_published +
         elapsed) / (orch->stats.events_published + 1);
}

/**
 * @brief Update bridge threat level from event
 */
static void update_bridge_threat_from_event(
    security_orchestrator_t orch,
    uint32_t bridge_id,
    const security_event_data_t* event
)
{
    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) return;

    security_bridge_info_t* bridge = &orch->bridges[idx];
    bridge->last_event_time = event->timestamp;
    bridge->events_published++;

    /* Update threat level based on event type */
    switch (event->event_type) {
        case SEC_EVENT_THREAT_DETECTED:
        case SEC_EVENT_THREAT_ESCALATED:
        case SEC_EVENT_ATTACK_STARTED:
        case SEC_EVENT_ATTACK_ONGOING:
            bridge->current_threat_level = event->threat.threat_level;
            bridge->threats_detected++;
            orch->stats.threats_detected++;
            break;

        case SEC_EVENT_THREAT_MITIGATED:
        case SEC_EVENT_THREAT_CLEARED:
        case SEC_EVENT_ATTACK_BLOCKED:
            bridge->current_threat_level *= 0.5f;
            orch->stats.threats_mitigated++;
            break;

        case SEC_EVENT_ATTACK_COMPLETED:
            if (event->attack.confidence > 0.8f) {
                orch->stats.attacks_blocked++;
            }
            break;

        case SEC_EVENT_BYZANTINE_DETECTED:
        case SEC_EVENT_GRADIENT_POISONED:
            bridge->current_threat_level = fmaxf(
                bridge->current_threat_level,
                event->byzantine.anomaly_score
            );
            bridge->threats_detected++;
            break;

        case SEC_EVENT_INJECTION_ATTEMPT:
        case SEC_EVENT_BELIEF_CORRUPTED:
        case SEC_EVENT_MEMORY_CORRUPTION:
            bridge->current_threat_level = fmaxf(
                bridge->current_threat_level,
                0.8f  /* High threat for integrity violations */
            );
            bridge->threats_detected++;
            break;

        default:
            break;
    }

    /* Recalculate unified threat */
    calculate_unified_threat(orch);

    /* Auto-lockdown on critical threat */
    if (orch->config.auto_lockdown &&
        orch->current_severity == SEC_SEVERITY_CRITICAL &&
        !orch->is_locked_down) {
        security_orch_trigger_lockdown(orch, "Auto-lockdown: Critical threat detected");
    }
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

int security_orch_default_config(security_orch_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));

    config->max_bridges = SEC_ORCH_MAX_BRIDGES;
    config->max_subscriptions = SEC_ORCH_MAX_SUBSCRIPTIONS;
    config->enable_async = true;
    config->event_queue_size = SEC_ORCH_MAX_EVENT_QUEUE;

    config->critical_threshold = SEC_ORCH_DEFAULT_CRITICAL_THRESHOLD;
    config->high_threshold = SEC_ORCH_DEFAULT_HIGH_THRESHOLD;
    config->medium_threshold = SEC_ORCH_DEFAULT_MEDIUM_THRESHOLD;
    config->threat_decay_rate = SEC_ORCH_DEFAULT_THREAT_DECAY;

    config->auto_lockdown = true;
    config->enable_cascade = true;
    config->enable_recovery = true;
    config->lockdown_timeout_ms = 30000;

    config->connect_immune = false;
    config->connect_cognitive_hub = false;
    config->enable_audit = true;

    return 0;
}

security_orchestrator_t security_orch_create(const security_orch_config_t* config)
{
    security_orchestrator_t orch = nimcp_calloc(1, sizeof(*orch));
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_orch_create: failed to allocate orchestrator");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&orch->config, config, sizeof(orch->config));
    } else {
        security_orch_default_config(&orch->config);
    }

    /* Initialize mutex */
    mutex_attr_t attr = { .type = MUTEX_TYPE_RECURSIVE };
    orch->mutex = nimcp_mutex_create(&attr);
    if (!orch->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_orch_create: failed to create mutex");
        nimcp_free(orch);
        return NULL;
    }

    /* Initialize event queue if async enabled */
    if (orch->config.enable_async) {
        orch->event_queue = nimcp_calloc(
            orch->config.event_queue_size,
            sizeof(security_event_queue_entry_t)
        );
        if (!orch->event_queue) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_orch_create: failed to allocate event queue");
            nimcp_mutex_free(orch->mutex);
            nimcp_free(orch);
            return NULL;
        }
    }

    /* Initialize state */
    orch->state = SEC_ORCH_STATE_IDLE;
    orch->create_time = get_timestamp_us();
    orch->last_decay_time = orch->create_time;
    orch->next_bridge_id = 1;
    orch->next_event_id = 1;

    return orch;
}

void security_orch_destroy(security_orchestrator_t orch)
{
    if (!orch) return;

    ORCH_LOCK(orch);

    /* Free event queue */
    if (orch->event_queue) {
        nimcp_free(orch->event_queue);
    }

    ORCH_UNLOCK(orch);

    /* Destroy mutex */
    nimcp_mutex_free(orch->mutex);

    nimcp_free(orch);
}

int security_orch_reset(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    /* Reset state */
    orch->state = SEC_ORCH_STATE_IDLE;
    orch->is_locked_down = false;
    memset(orch->lockdown_reason, 0, sizeof(orch->lockdown_reason));

    /* Reset threat tracking */
    orch->unified_threat_level = 0.0f;
    orch->current_severity = SEC_SEVERITY_NONE;
    orch->active_threat_count = 0;
    orch->last_decay_time = get_timestamp_us();

    /* Reset bridge threat levels */
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        orch->bridges[i].current_threat_level = 0.0f;
        orch->bridges[i].events_published = 0;
        orch->bridges[i].threats_detected = 0;
    }

    /* Reset event queue */
    orch->queue_head = 0;
    orch->queue_tail = 0;
    orch->queue_count = 0;

    /* Reset statistics */
    memset(&orch->stats, 0, sizeof(orch->stats));
    orch->stats.registered_bridges = orch->num_bridges;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_register_bridge(
    security_orchestrator_t orch,
    security_bridge_type_t bridge_type,
    const char* name,
    void* bridge_handle,
    void* context,
    uint32_t* bridge_id_out
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(bridge_type > SEC_BRIDGE_UNKNOWN && bridge_type < SEC_BRIDGE_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid bridge_type");

    ORCH_LOCK(orch);

    /* Check if bridge type already registered */
    if (find_bridge_by_type(orch, bridge_type) >= 0) {
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS, "bridge type already registered");
    }

    /* Check capacity */
    if (orch->num_bridges >= orch->config.max_bridges) {
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "max bridges exceeded");
    }

    /* Add bridge */
    security_bridge_info_t* bridge = &orch->bridges[orch->num_bridges];
    memset(bridge, 0, sizeof(*bridge));

    bridge->bridge_id = orch->next_bridge_id++;
    bridge->type = bridge_type;
    bridge->bridge_handle = bridge_handle;
    bridge->context = context;
    bridge->is_active = true;
    bridge->is_connected = false;
    bridge->current_threat_level = 0.0f;
    bridge->last_event_time = get_timestamp_us();

    if (name) {
        strncpy(bridge->name, name, SEC_ORCH_MAX_NAME_LEN - 1);
    } else {
        strncpy(bridge->name, security_bridge_type_name(bridge_type),
                SEC_ORCH_MAX_NAME_LEN - 1);
    }

    if (bridge_id_out) {
        *bridge_id_out = bridge->bridge_id;
    }

    orch->num_bridges++;
    orch->stats.registered_bridges++;
    orch->stats.active_bridges++;

    /* Publish registration event */
    security_event_data_t event = {
        .event_type = SEC_EVENT_BRIDGE_REGISTERED,
        .source = bridge_type,
        .severity = SEC_SEVERITY_NONE,
        .timestamp = get_timestamp_us(),
        .event_id = orch->next_event_id++
    };
    event.bridge.bridge_type = bridge_type;
    event.bridge.bridge_id = bridge->bridge_id;
    strncpy(event.bridge.bridge_name, bridge->name, sizeof(event.bridge.bridge_name) - 1);

    deliver_event(orch, bridge->bridge_id, &event);

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_unregister_bridge(
    security_orchestrator_t orch,
    uint32_t bridge_id
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "bridge not found");
    }

    security_bridge_info_t* bridge = &orch->bridges[idx];

    /* Publish unregistration event */
    security_event_data_t event = {
        .event_type = SEC_EVENT_BRIDGE_UNREGISTERED,
        .source = bridge->type,
        .severity = SEC_SEVERITY_NONE,
        .timestamp = get_timestamp_us(),
        .event_id = orch->next_event_id++
    };
    event.bridge.bridge_type = bridge->type;
    event.bridge.bridge_id = bridge->bridge_id;
    strncpy(event.bridge.bridge_name, bridge->name, sizeof(event.bridge.bridge_name) - 1);

    deliver_event(orch, bridge->bridge_id, &event);

    /* Remove subscriptions for this bridge */
    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        if (orch->subscriptions[i].subscriber_id == bridge_id) {
            orch->subscriptions[i].is_active = false;
        }
    }

    /* Remove bridge by shifting array */
    if ((uint32_t)idx < orch->num_bridges - 1) {
        memmove(&orch->bridges[idx], &orch->bridges[idx + 1],
                (orch->num_bridges - idx - 1) * sizeof(security_bridge_info_t));
    }
    orch->num_bridges--;

    orch->stats.registered_bridges--;
    orch->stats.active_bridges--;

    /* Recalculate unified threat */
    calculate_unified_threat(orch);

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_get_bridge_info(
    security_orchestrator_t orch,
    uint32_t bridge_id,
    security_bridge_info_t* info
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(info, NIMCP_ERROR_NULL_POINTER, "info is NULL");

    ORCH_LOCK(orch);

    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "bridge not found");
    }

    memcpy(info, &orch->bridges[idx], sizeof(*info));

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_get_bridge_by_type(
    security_orchestrator_t orch,
    security_bridge_type_t bridge_type,
    void** bridge_handle_out
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(bridge_handle_out, NIMCP_ERROR_NULL_POINTER, "bridge_handle_out is NULL");

    ORCH_LOCK(orch);

    int idx = find_bridge_by_type(orch, bridge_type);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "bridge type not found");
    }

    *bridge_handle_out = orch->bridges[idx].bridge_handle;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_subscribe(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_event_type_t event_type,
    security_event_callback_t callback,
    void* user_data
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(callback, NIMCP_ERROR_NULL_POINTER, "callback is NULL");
    NIMCP_CHECK_THROW(event_type < SEC_EVENT_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid event_type");

    ORCH_LOCK(orch);

    /* Find or create subscription slot */
    int slot = -1;
    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        if (!orch->subscriptions[i].is_active) {
            slot = (int)i;
            break;
        }
        /* Check for duplicate */
        if (orch->subscriptions[i].subscriber_id == subscriber_id &&
            orch->subscriptions[i].event_type == event_type &&
            !orch->subscriptions[i].subscribe_to_bridge) {
            /* Update existing subscription */
            orch->subscriptions[i].callback = callback;
            orch->subscriptions[i].user_data = user_data;
            ORCH_UNLOCK(orch);
            return 0;
        }
    }

    if (slot < 0) {
        if (orch->num_subscriptions >= orch->config.max_subscriptions) {
            ORCH_UNLOCK(orch);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "max subscriptions exceeded");
        }
        slot = (int)orch->num_subscriptions++;
    }

    security_subscription_t* sub = &orch->subscriptions[slot];
    sub->subscriber_id = subscriber_id;
    sub->event_type = event_type;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->is_active = true;
    sub->subscribe_to_bridge = false;

    orch->stats.active_subscriptions++;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_subscribe_to_bridge(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_bridge_type_t source_type,
    security_event_callback_t callback,
    void* user_data
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(callback, NIMCP_ERROR_NULL_POINTER, "callback is NULL");
    NIMCP_CHECK_THROW(source_type < SEC_BRIDGE_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid source_type");

    ORCH_LOCK(orch);

    /* Find or create subscription slot */
    int slot = -1;
    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        if (!orch->subscriptions[i].is_active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        if (orch->num_subscriptions >= orch->config.max_subscriptions) {
            ORCH_UNLOCK(orch);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "max subscriptions exceeded");
        }
        slot = (int)orch->num_subscriptions++;
    }

    security_subscription_t* sub = &orch->subscriptions[slot];
    sub->subscriber_id = subscriber_id;
    sub->source_type = source_type;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->is_active = true;
    sub->subscribe_to_bridge = true;

    orch->stats.active_subscriptions++;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_unsubscribe(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_event_type_t event_type
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        if (orch->subscriptions[i].is_active &&
            orch->subscriptions[i].subscriber_id == subscriber_id &&
            orch->subscriptions[i].event_type == event_type &&
            !orch->subscriptions[i].subscribe_to_bridge) {
            orch->subscriptions[i].is_active = false;
            orch->stats.active_subscriptions--;
            ORCH_UNLOCK(orch);
            return 0;
        }
    }

    ORCH_UNLOCK(orch);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "subscription not found");
    return 0; /* unreachable */
}

int security_orch_publish(
    security_orchestrator_t orch,
    uint32_t publisher_id,
    const security_event_data_t* event
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "event is NULL");

    ORCH_LOCK(orch);

    /* Update bridge threat level */
    update_bridge_threat_from_event(orch, publisher_id, event);

    /* Deliver to subscribers */
    deliver_event(orch, publisher_id, event);

    orch->stats.events_published++;

    /* Update orchestrator state based on severity */
    if (orch->current_severity >= SEC_SEVERITY_HIGH) {
        orch->state = SEC_ORCH_STATE_RESPONDING;
    } else if (orch->current_severity >= SEC_SEVERITY_MEDIUM) {
        orch->state = SEC_ORCH_STATE_ALERT;
    } else if (orch->current_severity > SEC_SEVERITY_NONE) {
        orch->state = SEC_ORCH_STATE_MONITORING;
    } else {
        orch->state = SEC_ORCH_STATE_IDLE;
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_publish_async(
    security_orchestrator_t orch,
    uint32_t publisher_id,
    const security_event_data_t* event
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "event is NULL");
    NIMCP_CHECK_THROW(orch->config.enable_async, NIMCP_ERROR_NOT_INITIALIZED, "async not enabled");

    ORCH_LOCK(orch);

    /* Check queue capacity */
    if (orch->queue_count >= orch->config.event_queue_size) {
        orch->stats.events_dropped++;
        ORCH_UNLOCK(orch);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "event queue full");
    }

    /* Add to queue */
    security_event_queue_entry_t* entry = &orch->event_queue[orch->queue_tail];
    memcpy(&entry->event, event, sizeof(security_event_data_t));
    entry->publisher_id = publisher_id;
    entry->is_valid = true;

    orch->queue_tail = (orch->queue_tail + 1) % orch->config.event_queue_size;
    orch->queue_count++;

    if (orch->queue_count > orch->stats.async_queue_max) {
        orch->stats.async_queue_max = orch->queue_count;
    }
    orch->stats.async_queue_depth = orch->queue_count;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_report_threat(
    security_orchestrator_t orch,
    uint32_t bridge_id,
    float threat_level,
    security_severity_t severity,
    const char* description
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(threat_level >= 0.0f && threat_level <= 1.0f, NIMCP_ERROR_OUT_OF_RANGE, "threat_level out of range");

    security_event_data_t event = {
        .event_type = SEC_EVENT_THREAT_DETECTED,
        .severity = severity,
        .timestamp = get_timestamp_us()
    };

    ORCH_LOCK(orch);

    int idx = find_bridge_index(orch, bridge_id);
    if (idx >= 0) {
        event.source = orch->bridges[idx].type;
    }
    event.event_id = orch->next_event_id++;

    ORCH_UNLOCK(orch);

    event.threat.threat_level = threat_level;
    event.threat.threat_id = event.event_id;
    if (description) {
        strncpy(event.threat.description, description,
                sizeof(event.threat.description) - 1);
    }

    return security_orch_publish(orch, bridge_id, &event);
}

int security_orch_get_threat_assessment(
    security_orchestrator_t orch,
    security_threat_assessment_t* assessment
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(assessment, NIMCP_ERROR_NULL_POINTER, "assessment is NULL");

    ORCH_LOCK(orch);

    /* Apply decay before assessment */
    apply_threat_decay(orch);

    memset(assessment, 0, sizeof(*assessment));

    assessment->unified_threat_level = orch->unified_threat_level;
    assessment->severity = orch->current_severity;
    assessment->active_threats = orch->active_threat_count;
    assessment->assessment_time = get_timestamp_us();

    /* Collect per-bridge threats */
    uint32_t reporting = 0;
    float max_threat = 0.0f;
    security_bridge_type_t max_source = SEC_BRIDGE_UNKNOWN;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        security_bridge_info_t* bridge = &orch->bridges[i];
        if (!bridge->is_active) continue;

        assessment->bridge_threats[bridge->type].type = bridge->type;
        assessment->bridge_threats[bridge->type].threat_level = bridge->current_threat_level;
        assessment->bridge_threats[bridge->type].threat_count = bridge->threats_detected;

        if (bridge->current_threat_level > 0.0f) {
            reporting++;
            if (bridge->current_threat_level > max_threat) {
                max_threat = bridge->current_threat_level;
                max_source = bridge->type;
                assessment->bridge_threats[bridge->type].is_primary_source = true;
            }
        }
    }

    assessment->bridges_reporting = reporting;
    assessment->primary_threat_source = max_source;

    /* Generate summary */
    if (orch->unified_threat_level > 0.0f) {
        snprintf(assessment->threat_summary, sizeof(assessment->threat_summary),
                 "Unified threat level: %.2f (%s). Primary source: %s. "
                 "%u bridges reporting, %u active threats.",
                 orch->unified_threat_level,
                 security_severity_name(orch->current_severity),
                 security_bridge_type_name(max_source),
                 reporting, orch->active_threat_count);
    } else {
        strncpy(assessment->threat_summary, "No active threats detected.",
                sizeof(assessment->threat_summary) - 1);
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_get_threat_level(
    security_orchestrator_t orch,
    float* threat_level
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(threat_level, NIMCP_ERROR_NULL_POINTER, "threat_level is NULL");

    ORCH_LOCK(orch);
    *threat_level = orch->unified_threat_level;
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_update_threat_decay(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);
    apply_threat_decay(orch);
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_clear_threats(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        orch->bridges[i].current_threat_level = 0.0f;
    }

    orch->unified_threat_level = 0.0f;
    orch->current_severity = SEC_SEVERITY_NONE;
    orch->active_threat_count = 0;

    if (orch->is_locked_down && orch->config.enable_recovery) {
        security_orch_release_lockdown(orch);
    }

    orch->state = SEC_ORCH_STATE_IDLE;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_trigger_lockdown(
    security_orchestrator_t orch,
    const char* reason
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    if (orch->is_locked_down) {
        ORCH_UNLOCK(orch);
        return 0;  /* Already locked down */
    }

    orch->is_locked_down = true;
    orch->lockdown_time = get_timestamp_us();
    orch->state = SEC_ORCH_STATE_LOCKDOWN;

    if (reason) {
        strncpy(orch->lockdown_reason, reason, sizeof(orch->lockdown_reason) - 1);
    }

    orch->stats.lockdowns_triggered++;

    /* Publish lockdown event */
    security_event_data_t event = {
        .event_type = SEC_EVENT_ORCHESTRATOR_STATE,
        .source = SEC_BRIDGE_UNKNOWN,
        .severity = SEC_SEVERITY_CRITICAL,
        .timestamp = get_timestamp_us(),
        .event_id = orch->next_event_id++
    };
    event.custom.code = SEC_ORCH_STATE_LOCKDOWN;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].is_active) {
            deliver_event(orch, orch->bridges[i].bridge_id, &event);
        }
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_release_lockdown(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    if (!orch->is_locked_down) {
        ORCH_UNLOCK(orch);
        return 0;  /* Not locked down */
    }

    orch->is_locked_down = false;
    memset(orch->lockdown_reason, 0, sizeof(orch->lockdown_reason));
    orch->state = SEC_ORCH_STATE_RECOVERY;

    /* Publish release event */
    security_event_data_t event = {
        .event_type = SEC_EVENT_ORCHESTRATOR_STATE,
        .source = SEC_BRIDGE_UNKNOWN,
        .severity = SEC_SEVERITY_NONE,
        .timestamp = get_timestamp_us(),
        .event_id = orch->next_event_id++
    };
    event.custom.code = SEC_ORCH_STATE_RECOVERY;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].is_active) {
            deliver_event(orch, orch->bridges[i].bridge_id, &event);
        }
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_is_locked_down(
    security_orchestrator_t orch,
    bool* is_locked
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(is_locked, NIMCP_ERROR_NULL_POINTER, "is_locked is NULL");

    ORCH_LOCK(orch);
    *is_locked = orch->is_locked_down;
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_broadcast_response(
    security_orchestrator_t orch,
    security_event_type_t response_type,
    const security_event_data_t* data
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    security_event_data_t event;
    if (data) {
        memcpy(&event, data, sizeof(event));
    } else {
        memset(&event, 0, sizeof(event));
    }
    event.event_type = response_type;
    event.timestamp = get_timestamp_us();
    event.event_id = orch->next_event_id++;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].is_active) {
            deliver_event(orch, 0, &event);  /* 0 = orchestrator */
        }
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_get_state(
    security_orchestrator_t orch,
    security_orch_state_t* state
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    ORCH_LOCK(orch);
    *state = orch->state;
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_get_stats(
    security_orchestrator_t orch,
    security_orch_stats_t* stats
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    ORCH_LOCK(orch);

    memcpy(stats, &orch->stats, sizeof(*stats));

    /* Update uptime */
    stats->uptime_us = get_timestamp_us() - orch->create_time;

    /* Calculate average threat level */
    if (orch->stats.events_published > 0) {
        stats->avg_threat_level = orch->unified_threat_level;
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_reset_stats(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);

    uint32_t registered = orch->stats.registered_bridges;
    uint32_t active = orch->stats.active_bridges;
    uint32_t connected = orch->stats.connected_bridges;
    uint32_t subscriptions = orch->stats.active_subscriptions;

    memset(&orch->stats, 0, sizeof(orch->stats));

    /* Preserve current counts */
    orch->stats.registered_bridges = registered;
    orch->stats.active_bridges = active;
    orch->stats.connected_bridges = connected;
    orch->stats.active_subscriptions = subscriptions;

    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_connect_immune(
    security_orchestrator_t orch,
    void* immune_system
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);
    orch->immune_system = immune_system;
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_connect_cognitive_hub(
    security_orchestrator_t orch,
    void* cognitive_hub
)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);
    orch->cognitive_hub = cognitive_hub;
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_connect_bio_async(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = "security_orchestrator",
        .inbox_capacity = 0,  /* Use default */
        .user_data = orch
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        /* Router not initialized - this is okay, just mark as not connected */
        fprintf(stderr, "Bio-async router not available, skipping registration for security_orchestrator\n");
    }

    ORCH_LOCK(orch);
    orch->bio_async_connected = (ctx != NULL);
    ORCH_UNLOCK(orch);

    return 0;
}

int security_orch_disconnect_bio_async(security_orchestrator_t orch)
{
    NIMCP_CHECK_THROW(orch, NIMCP_ERROR_NULL_POINTER, "orch is NULL");

    ORCH_LOCK(orch);
    orch->bio_async_connected = false;
    ORCH_UNLOCK(orch);

    return 0;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* security_bridge_type_name(security_bridge_type_t type)
{
    static const char* names[] = {
        [SEC_BRIDGE_UNKNOWN] = "Unknown",
        [SEC_BRIDGE_DISTRIBUTED_TRAINING] = "Distributed Training",
        [SEC_BRIDGE_KNOWLEDGE_GRAPH] = "Knowledge Graph",
        [SEC_BRIDGE_GAME_THEORY] = "Game Theory",
        [SEC_BRIDGE_IMAGINATION] = "Imagination",
        [SEC_BRIDGE_CONTINUAL_LEARNING] = "Continual Learning",
        [SEC_BRIDGE_EPISTEMIC] = "Epistemic",
        [SEC_BRIDGE_COLLECTIVE] = "Collective",
        [SEC_BRIDGE_HIPPOCAMPUS] = "Hippocampus",
        [SEC_BRIDGE_BBB] = "Blood-Brain Barrier",
        [SEC_BRIDGE_ANOMALY_DETECTOR] = "Anomaly Detector",
        [SEC_BRIDGE_PATTERN_DB] = "Pattern Database",
        [SEC_BRIDGE_RATE_LIMITER] = "Rate Limiter",
        [SEC_BRIDGE_IMMUNE] = "Immune System"
    };

    if (type >= SEC_BRIDGE_COUNT) return "Invalid";
    return names[type] ? names[type] : "Unknown";
}

const char* security_event_type_name(security_event_type_t type)
{
    static const char* names[] = {
        [SEC_EVENT_THREAT_DETECTED] = "Threat Detected",
        [SEC_EVENT_THREAT_ESCALATED] = "Threat Escalated",
        [SEC_EVENT_THREAT_MITIGATED] = "Threat Mitigated",
        [SEC_EVENT_THREAT_CLEARED] = "Threat Cleared",
        [SEC_EVENT_ATTACK_STARTED] = "Attack Started",
        [SEC_EVENT_ATTACK_ONGOING] = "Attack Ongoing",
        [SEC_EVENT_ATTACK_BLOCKED] = "Attack Blocked",
        [SEC_EVENT_ATTACK_COMPLETED] = "Attack Completed",
        [SEC_EVENT_BYZANTINE_DETECTED] = "Byzantine Detected",
        [SEC_EVENT_WORKER_QUARANTINED] = "Worker Quarantined",
        [SEC_EVENT_CONSENSUS_VIOLATED] = "Consensus Violated",
        [SEC_EVENT_GRADIENT_POISONED] = "Gradient Poisoned",
        [SEC_EVENT_INJECTION_ATTEMPT] = "Injection Attempt",
        [SEC_EVENT_SCHEMA_VIOLATION] = "Schema Violation",
        [SEC_EVENT_BELIEF_CORRUPTED] = "Belief Corrupted",
        [SEC_EVENT_EVIDENCE_TAMPERED] = "Evidence Tampered",
        [SEC_EVENT_CONFABULATION_DETECTED] = "Confabulation Detected",
        [SEC_EVENT_REALITY_DRIFT] = "Reality Drift",
        [SEC_EVENT_STRATEGY_MANIPULATION] = "Strategy Manipulation",
        [SEC_EVENT_COALITION_ATTACK] = "Coalition Attack",
        [SEC_EVENT_MEMORY_CORRUPTION] = "Memory Corruption",
        [SEC_EVENT_FORGETTING_ATTACK] = "Forgetting Attack",
        [SEC_EVENT_REPLAY_POISONED] = "Replay Poisoned",
        [SEC_EVENT_CONSOLIDATION_TAMPERED] = "Consolidation Tampered",
        [SEC_EVENT_BRIDGE_REGISTERED] = "Bridge Registered",
        [SEC_EVENT_BRIDGE_UNREGISTERED] = "Bridge Unregistered",
        [SEC_EVENT_BRIDGE_CONNECTED] = "Bridge Connected",
        [SEC_EVENT_BRIDGE_DISCONNECTED] = "Bridge Disconnected",
        [SEC_EVENT_ORCHESTRATOR_STATE] = "Orchestrator State"
    };

    if (type >= SEC_EVENT_TYPE_COUNT) return "Invalid";
    return names[type] ? names[type] : "Unknown";
}

const char* security_severity_name(security_severity_t severity)
{
    static const char* names[] = {
        [SEC_SEVERITY_NONE] = "None",
        [SEC_SEVERITY_LOW] = "Low",
        [SEC_SEVERITY_MEDIUM] = "Medium",
        [SEC_SEVERITY_HIGH] = "High",
        [SEC_SEVERITY_CRITICAL] = "Critical"
    };

    if (severity > SEC_SEVERITY_CRITICAL) return "Invalid";
    return names[severity];
}

const char* security_orch_state_name(security_orch_state_t state)
{
    static const char* names[] = {
        [SEC_ORCH_STATE_UNINITIALIZED] = "Uninitialized",
        [SEC_ORCH_STATE_IDLE] = "Idle",
        [SEC_ORCH_STATE_MONITORING] = "Monitoring",
        [SEC_ORCH_STATE_ALERT] = "Alert",
        [SEC_ORCH_STATE_RESPONDING] = "Responding",
        [SEC_ORCH_STATE_LOCKDOWN] = "Lockdown",
        [SEC_ORCH_STATE_RECOVERY] = "Recovery",
        [SEC_ORCH_STATE_ERROR] = "Error"
    };

    if (state > SEC_ORCH_STATE_ERROR) return "Invalid";
    return names[state];
}

void security_orch_print_summary(security_orchestrator_t orch)
{
    if (!orch) {
        printf("Security Orchestrator: NULL\n");
        return;
    }

    ORCH_LOCK(orch);

    printf("\n=== Security Orchestrator Summary ===\n");
    printf("State: %s\n", security_orch_state_name(orch->state));
    printf("Lockdown: %s\n", orch->is_locked_down ? "YES" : "No");
    if (orch->is_locked_down) {
        printf("  Reason: %s\n", orch->lockdown_reason);
    }
    printf("\nThreat Assessment:\n");
    printf("  Unified Level: %.3f\n", orch->unified_threat_level);
    printf("  Severity: %s\n", security_severity_name(orch->current_severity));
    printf("  Active Threats: %u\n", orch->active_threat_count);

    printf("\nRegistered Bridges: %u\n", orch->num_bridges);
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        security_bridge_info_t* b = &orch->bridges[i];
        printf("  [%u] %s (%s): threat=%.3f, events=%u, threats=%u\n",
               b->bridge_id, b->name,
               b->is_active ? "active" : "inactive",
               b->current_threat_level,
               b->events_published, b->threats_detected);
    }

    printf("\nSubscriptions: %u\n", orch->num_subscriptions);
    printf("Async Queue: %u/%u\n", orch->queue_count, orch->config.event_queue_size);

    ORCH_UNLOCK(orch);
}

void security_orch_print_stats(const security_orch_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n=== Security Orchestrator Statistics ===\n");
    printf("Bridges:\n");
    printf("  Registered: %u\n", stats->registered_bridges);
    printf("  Active: %u\n", stats->active_bridges);
    printf("  Connected: %u\n", stats->connected_bridges);

    printf("\nEvents:\n");
    printf("  Published: %lu\n", (unsigned long)stats->events_published);
    printf("  Delivered: %lu\n", (unsigned long)stats->events_delivered);
    printf("  Dropped: %lu\n", (unsigned long)stats->events_dropped);
    printf("  Subscriptions: %u\n", stats->active_subscriptions);

    printf("\nThreats:\n");
    printf("  Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("  Mitigated: %lu\n", (unsigned long)stats->threats_mitigated);
    printf("  Attacks Blocked: %lu\n", (unsigned long)stats->attacks_blocked);
    printf("  Lockdowns: %u\n", stats->lockdowns_triggered);
    printf("  Peak Level: %.3f\n", stats->peak_threat_level);

    printf("\nAsync Queue:\n");
    printf("  Current: %u\n", stats->async_queue_depth);
    printf("  Max: %u\n", stats->async_queue_max);

    printf("\nTiming:\n");
    printf("  Avg Event Latency: %lu us\n", (unsigned long)stats->avg_event_latency_us);
    printf("  Avg Response Time: %lu us\n", (unsigned long)stats->avg_response_time_us);
    printf("  Uptime: %lu us\n", (unsigned long)stats->uptime_us);
}
