/**
 * @file nimcp_hypothalamus_orchestrator.c
 * @brief Implementation of hypothalamus orchestrator for bridge coordination
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Central orchestrator implementation for hypothalamus module integration
 * WHY: Coordinates all hypothalamus bridges for unified drive management
 * HOW: Event-driven publish-subscribe with drive aggregation and response coordination
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_orchestrator, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Subscription entry
 */
typedef struct {
    uint32_t subscriber_id;             /**< Subscriber bridge ID */
    hypo_event_type_t event_type;       /**< Event type subscribed to */
    hypo_event_callback_t callback;     /**< Callback function */
    void* user_data;                    /**< User context */
    bool is_active;                     /**< Whether subscription is active */
    bool subscribe_to_bridge;           /**< Subscribe to bridge instead of event */
    hypo_bridge_type_t source_type;     /**< Source bridge type (if subscribe_to_bridge) */
} hypo_subscription_t;

/**
 * @brief Async event queue entry
 */
typedef struct {
    hypo_event_data_t event;            /**< Event data */
    uint32_t publisher_id;              /**< Publisher bridge ID */
    bool is_valid;                      /**< Whether entry is valid */
} hypo_event_queue_entry_t;

/**
 * @brief Internal orchestrator structure
 */
struct hypo_orchestrator_struct {
    /* Configuration */
    hypo_orch_config_t config;

    /* State */
    hypo_orch_state_t state;
    bool is_stressed;
    char stress_reason[NIMCP_ERROR_BUFFER_MEDIUM];
    uint64_t stress_time;

    /* Registered bridges */
    hypo_bridge_info_t bridges[HYPO_ORCH_MAX_BRIDGES];
    uint32_t num_bridges;
    uint32_t next_bridge_id;

    /* Subscriptions */
    hypo_subscription_t subscriptions[HYPO_ORCH_MAX_SUBSCRIPTIONS];
    uint32_t num_subscriptions;

    /* Drive tracking */
    float unified_drive_level;
    hypo_urgency_t current_urgency;
    uint32_t active_drive_count;
    uint64_t last_decay_time;

    /* Event queue (for async) */
    hypo_event_queue_entry_t* event_queue;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Statistics */
    hypo_orch_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;

    /* Integration handles */
    void* immune_system;
    void* bio_async_router;
    void* logger;
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
static int find_bridge_index(hypo_orchestrator_t orch, uint32_t bridge_id)
{
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].bridge_id == bridge_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found — normal lookup miss, not an error */
}

/**
 * @brief Find bridge by type
 */
static int find_bridge_by_type(hypo_orchestrator_t orch, hypo_bridge_type_t type)
{
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].type == type) {
            return (int)i;
        }
    }
    return -1;  /* Not found — normal lookup miss, not an error */
}

/**
 * @brief Calculate unified drive level from all bridges
 */
static void calculate_unified_drive(hypo_orchestrator_t orch)
{
    if (orch->num_bridges == 0) {
        orch->unified_drive_level = 0.0f;
        orch->current_urgency = HYPO_URGENCY_NONE;
        orch->active_drive_count = 0;
        return;
    }

    float max_drive = 0.0f;
    float sum_drive = 0.0f;
    uint32_t active_count = 0;
    uint32_t reporting_count = 0;

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (!orch->bridges[i].is_active) continue;

        float drive = orch->bridges[i].current_drive_level;
        if (drive > 0.0f) {
            reporting_count++;
            sum_drive += drive;
            if (drive > max_drive) {
                max_drive = drive;
            }
            if (drive > orch->config.moderate_threshold) {
                active_count++;
            }
        }
    }

    /* Unified drive = weighted combination of max and average */
    if (reporting_count > 0) {
        float avg_drive = sum_drive / reporting_count;
        /* 70% max, 30% average for unified drive */
        orch->unified_drive_level = 0.7f * max_drive + 0.3f * avg_drive;
    } else {
        orch->unified_drive_level = 0.0f;
    }

    /* Clamp to [0, 1] */
    if (orch->unified_drive_level > 1.0f) {
        orch->unified_drive_level = 1.0f;
    }

    /* Update urgency */
    if (orch->unified_drive_level >= orch->config.urgent_threshold) {
        orch->current_urgency = HYPO_URGENCY_URGENT;
    } else if (orch->unified_drive_level >= orch->config.elevated_threshold) {
        orch->current_urgency = HYPO_URGENCY_ELEVATED;
    } else if (orch->unified_drive_level >= orch->config.moderate_threshold) {
        orch->current_urgency = HYPO_URGENCY_MODERATE;
    } else if (orch->unified_drive_level > 0.0f) {
        orch->current_urgency = HYPO_URGENCY_LOW;
    } else {
        orch->current_urgency = HYPO_URGENCY_NONE;
    }

    orch->active_drive_count = active_count;

    /* Update statistics */
    if (orch->unified_drive_level > orch->stats.peak_drive_level) {
        orch->stats.peak_drive_level = orch->unified_drive_level;
    }
}

/**
 * @brief Apply drive decay
 */
static void apply_drive_decay(hypo_orchestrator_t orch)
{
    uint64_t now = get_timestamp_us();
    uint64_t elapsed_us = now - orch->last_decay_time;
    float elapsed_sec = (float)elapsed_us / 1000000.0f;

    if (elapsed_sec < 0.1f) {
        return;  /* Don't decay more often than 100ms */
    }

    float decay_factor = expf(-orch->config.drive_decay_rate * elapsed_sec);

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].current_drive_level > 0.0f) {
            orch->bridges[i].current_drive_level *= decay_factor;
            if (orch->bridges[i].current_drive_level < 0.01f) {
                orch->bridges[i].current_drive_level = 0.0f;
            }
        }
    }

    orch->last_decay_time = now;
    calculate_unified_drive(orch);
}

/**
 * @brief Deliver event to subscribers
 */
static void deliver_event(
    hypo_orchestrator_t orch,
    uint32_t publisher_id,
    const hypo_event_data_t* event
)
{
    uint64_t start_time = get_timestamp_us();

    for (uint32_t i = 0; i < orch->num_subscriptions; i++) {
        hypo_subscription_t* sub = &orch->subscriptions[i];
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
    if (orch->stats.events_published > 0) {
        orch->stats.avg_event_latency_us =
            (orch->stats.avg_event_latency_us * orch->stats.events_published +
             elapsed) / (orch->stats.events_published + 1);
    } else {
        orch->stats.avg_event_latency_us = elapsed;
    }
}

/**
 * @brief Update bridge drive level from event
 */
static void update_bridge_drive_from_event(
    hypo_orchestrator_t orch,
    uint32_t bridge_id,
    const hypo_event_data_t* event
)
{
    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) return;

    hypo_bridge_info_t* bridge = &orch->bridges[idx];
    bridge->last_event_time = event->timestamp;
    bridge->events_published++;

    /* Update drive level based on event type */
    switch (event->event_type) {
        case HYPO_EVENT_DRIVE_ACTIVATED:
            bridge->current_drive_level = event->drive.drive_level;
            bridge->drives_reported++;
            orch->stats.drives_activated++;
            break;

        case HYPO_EVENT_DRIVE_SATISFIED:
            bridge->current_drive_level *= 0.3f;  /* Significant reduction */
            orch->stats.drives_satisfied++;
            break;

        case HYPO_EVENT_DRIVE_CONFLICT:
            /* Take the higher of current or reported */
            bridge->current_drive_level = fmaxf(
                bridge->current_drive_level,
                event->drive.drive_level
            );
            orch->stats.conflicts_detected++;
            break;

        case HYPO_EVENT_HOMEOSTATIC_ALERT:
            if (event->homeostatic.is_critical) {
                bridge->current_drive_level = fmaxf(
                    bridge->current_drive_level,
                    0.9f  /* High priority for critical homeostatic alerts */
                );
            } else {
                bridge->current_drive_level = fmaxf(
                    bridge->current_drive_level,
                    fabsf(event->homeostatic.deviation)
                );
            }
            break;

        case HYPO_EVENT_STRESS_RESPONSE:
            bridge->current_drive_level = fmaxf(
                bridge->current_drive_level,
                event->stress.stress_level
            );
            orch->stats.stress_responses++;
            break;

        case HYPO_EVENT_REWARD_SIGNAL:
            /* Reward reduces drive proportionally */
            bridge->current_drive_level *= (1.0f - event->reward.reward_magnitude * 0.5f);
            break;

        case HYPO_EVENT_SETPOINT_CHANGE:
        case HYPO_EVENT_CIRCADIAN_PHASE:
        case HYPO_EVENT_AUTONOMIC_SHIFT:
        case HYPO_EVENT_ALIGNMENT_CHECK:
            /* These don't directly affect drive level */
            break;

        default:
            break;
    }

    /* Clamp drive level */
    if (bridge->current_drive_level > 1.0f) {
        bridge->current_drive_level = 1.0f;
    }
    if (bridge->current_drive_level < 0.0f) {
        bridge->current_drive_level = 0.0f;
    }

    /* Recalculate unified drive */
    calculate_unified_drive(orch);

    /* Auto-stress on urgent drive */
    if (orch->config.auto_regulate &&
        orch->current_urgency == HYPO_URGENCY_URGENT &&
        !orch->is_stressed) {
        hypo_orch_trigger_stress(orch, "Auto-stress: Urgent drive detected");
    }
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

int hypo_orch_default_config(hypo_orch_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->max_bridges = HYPO_ORCH_MAX_BRIDGES;
    config->max_subscriptions = HYPO_ORCH_MAX_SUBSCRIPTIONS;
    config->enable_async = true;
    config->event_queue_size = HYPO_ORCH_MAX_EVENT_QUEUE;

    config->urgent_threshold = HYPO_ORCH_DEFAULT_URGENT_THRESHOLD;
    config->elevated_threshold = HYPO_ORCH_DEFAULT_ELEVATED_THRESHOLD;
    config->moderate_threshold = HYPO_ORCH_DEFAULT_MODERATE_THRESHOLD;
    config->drive_decay_rate = HYPO_ORCH_DEFAULT_DRIVE_DECAY;

    config->auto_regulate = true;
    config->enable_cascade = true;
    config->enable_recovery = true;
    config->regulation_timeout_ms = 60000;

    config->connect_immune = false;
    config->connect_bio_async = false;
    config->enable_logging = true;

    return 0;
}

hypo_orchestrator_t hypo_orch_create(const hypo_orch_config_t* config)
{
    hypo_orchestrator_t orch = nimcp_calloc(1, sizeof(*orch));
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "orch is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&orch->config, config, sizeof(orch->config));
    } else {
        hypo_orch_default_config(&orch->config);
    }

    /* Initialize mutex with recursive type */
    mutex_attr_t attr = { .type = MUTEX_TYPE_RECURSIVE };
    orch->mutex = nimcp_mutex_create(&attr);
    if (!orch->mutex) {
        nimcp_free(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_orch_create: orch->mutex is NULL");
        return NULL;
    }

    /* Initialize event queue if async enabled */
    if (orch->config.enable_async) {
        orch->event_queue = nimcp_calloc(
            orch->config.event_queue_size,
            sizeof(hypo_event_queue_entry_t)
        );
        if (!orch->event_queue) {
            nimcp_mutex_destroy(orch->mutex);
            nimcp_free(orch);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_orch_create: orch->event_queue is NULL");
            return NULL;
        }
    }

    /* Initialize state */
    orch->state = HYPO_ORCH_STATE_IDLE;
    orch->create_time = get_timestamp_us();
    orch->last_decay_time = orch->create_time;
    orch->next_bridge_id = 1;
    orch->next_event_id = 1;

    return orch;
}

void hypo_orch_destroy(hypo_orchestrator_t orch)
{
    if (!orch) return;

    ORCH_LOCK(orch);

    /* Free event queue */
    if (orch->event_queue) {
        nimcp_free(orch->event_queue);
        orch->event_queue = NULL;
    }

    ORCH_UNLOCK(orch);

    /* Destroy mutex */
    nimcp_mutex_destroy(orch->mutex);

    nimcp_free(orch);
}

int hypo_orch_reset(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_reset: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    /* Reset state */
    orch->state = HYPO_ORCH_STATE_IDLE;
    orch->is_stressed = false;
    memset(orch->stress_reason, 0, sizeof(orch->stress_reason));

    /* Reset drive tracking */
    orch->unified_drive_level = 0.0f;
    orch->current_urgency = HYPO_URGENCY_NONE;
    orch->active_drive_count = 0;
    orch->last_decay_time = get_timestamp_us();

    /* Reset bridge drive levels */
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        orch->bridges[i].current_drive_level = 0.0f;
        orch->bridges[i].events_published = 0;
        orch->bridges[i].drives_reported = 0;
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

int hypo_orch_register_bridge(
    hypo_orchestrator_t orch,
    hypo_bridge_type_t bridge_type,
    const char* name,
    void* bridge_handle,
    void* context,
    uint32_t* bridge_id_out
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_register_bridge: orch is NULL");
        return -1;
    }
    if (bridge_type <= HYPO_BRIDGE_UNKNOWN || bridge_type >= HYPO_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_register_bridge: invalid bridge_type");
        return -1;
    }

    ORCH_LOCK(orch);

    /* Check if bridge type already registered */
    if (find_bridge_by_type(orch, bridge_type) >= 0) {
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_register_bridge: capacity exceeded");
        return -1;
    }

    /* Check capacity */
    if (orch->num_bridges >= orch->config.max_bridges) {
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_orch_register_bridge: capacity exceeded");
        return -1;
    }

    /* Add bridge */
    hypo_bridge_info_t* bridge = &orch->bridges[orch->num_bridges];
    memset(bridge, 0, sizeof(*bridge));

    bridge->bridge_id = orch->next_bridge_id++;
    bridge->type = bridge_type;
    bridge->bridge_handle = bridge_handle;
    bridge->context = context;
    bridge->is_active = true;
    bridge->is_connected = false;
    bridge->current_drive_level = 0.0f;
    bridge->last_event_time = get_timestamp_us();

    if (name) {
        strncpy(bridge->name, name, HYPO_ORCH_MAX_NAME_LEN - 1);
    } else {
        strncpy(bridge->name, hypo_bridge_type_name(bridge_type),
                HYPO_ORCH_MAX_NAME_LEN - 1);
    }

    if (bridge_id_out) {
        *bridge_id_out = bridge->bridge_id;
    }

    orch->num_bridges++;
    orch->stats.registered_bridges++;
    orch->stats.active_bridges++;

    /* Publish registration event */
    hypo_event_data_t event = {
        .event_type = HYPO_EVENT_DRIVE_ACTIVATED,
        .source = bridge_type,
        .urgency = HYPO_URGENCY_NONE,
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

int hypo_orch_unregister_bridge(
    hypo_orchestrator_t orch,
    uint32_t bridge_id
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_unregister_bridge: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_unregister_bridge: validation failed");
        return -1;
    }

    hypo_bridge_info_t* bridge = &orch->bridges[idx];

    /* Publish unregistration event */
    hypo_event_data_t event = {
        .event_type = HYPO_EVENT_DRIVE_SATISFIED,
        .source = bridge->type,
        .urgency = HYPO_URGENCY_NONE,
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
                (orch->num_bridges - idx - 1) * sizeof(hypo_bridge_info_t));
    }
    orch->num_bridges--;

    orch->stats.registered_bridges--;
    orch->stats.active_bridges--;

    /* Recalculate unified drive */
    calculate_unified_drive(orch);

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_get_bridge_info(
    hypo_orchestrator_t orch,
    uint32_t bridge_id,
    hypo_bridge_info_t* info
)
{
    if (!orch || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_bridge_info: orch or info is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    int idx = find_bridge_index(orch, bridge_id);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_get_bridge_info: validation failed");
        return -1;
    }

    memcpy(info, &orch->bridges[idx], sizeof(*info));

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_get_bridge_by_type(
    hypo_orchestrator_t orch,
    hypo_bridge_type_t bridge_type,
    void** bridge_handle_out
)
{
    if (!orch || !bridge_handle_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_bridge_by_type: orch or bridge_handle_out is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    int idx = find_bridge_by_type(orch, bridge_type);
    if (idx < 0) {
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_get_bridge_by_type: validation failed");
        return -1;
    }

    *bridge_handle_out = orch->bridges[idx].bridge_handle;

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_subscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_t event_type,
    hypo_event_callback_t callback,
    void* user_data
)
{
    if (!orch || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_subscribe: orch or callback is NULL");
        return -1;
    }
    if (event_type >= HYPO_EVENT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_subscribe: invalid event_type");
        return -1;
    }

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
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_orch_subscribe: capacity exceeded");
            return -1;
        }
        slot = (int)orch->num_subscriptions++;
    }

    hypo_subscription_t* sub = &orch->subscriptions[slot];
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

int hypo_orch_subscribe_to_bridge(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_bridge_type_t source_type,
    hypo_event_callback_t callback,
    void* user_data
)
{
    if (!orch || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_subscribe_to_bridge: orch or callback is NULL");
        return -1;
    }
    if (source_type >= HYPO_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_subscribe_to_bridge: invalid source_type");
        return -1;
    }

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
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_orch_subscribe_to_bridge: capacity exceeded");
            return -1;
        }
        slot = (int)orch->num_subscriptions++;
    }

    hypo_subscription_t* sub = &orch->subscriptions[slot];
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

int hypo_orch_unsubscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_t event_type
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_unsubscribe: orch is NULL");
        return -1;
    }

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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_orch_unsubscribe: operation failed");
    return -1;
}

int hypo_orch_publish(
    hypo_orchestrator_t orch,
    uint32_t publisher_id,
    const hypo_event_data_t* event
)
{
    if (!orch || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_publish: orch or event is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    /* Update bridge state from event */
    update_bridge_drive_from_event(orch, publisher_id, event);

    /* Deliver to subscribers */
    deliver_event(orch, publisher_id, event);

    orch->stats.events_published++;

    /* Update orchestrator state based on event */
    if (event->event_type == HYPO_EVENT_STRESS_RESPONSE) {
        orch->state = HYPO_ORCH_STATE_STRESS;
    } else if (event->event_type == HYPO_EVENT_DRIVE_CONFLICT) {
        orch->state = HYPO_ORCH_STATE_CONFLICT;
    } else if (orch->unified_drive_level > orch->config.moderate_threshold) {
        orch->state = HYPO_ORCH_STATE_REGULATING;
    } else if (orch->unified_drive_level > 0.0f) {
        orch->state = HYPO_ORCH_STATE_MONITORING;
    } else {
        orch->state = HYPO_ORCH_STATE_IDLE;
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_publish_async(
    hypo_orchestrator_t orch,
    uint32_t publisher_id,
    const hypo_event_data_t* event
)
{
    if (!orch || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_publish_async: orch or event is NULL");
        return -1;
    }
    if (!orch->config.enable_async || !orch->event_queue) {
        /* Fall back to sync publish */
        return hypo_orch_publish(orch, publisher_id, event);
    }

    ORCH_LOCK(orch);

    /* Check queue capacity */
    if (orch->queue_count >= orch->config.event_queue_size) {
        orch->stats.events_dropped++;
        ORCH_UNLOCK(orch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hypo_orch_publish_async: capacity exceeded");
        return -1;
    }

    /* Add to queue */
    hypo_event_queue_entry_t* entry = &orch->event_queue[orch->queue_tail];
    memcpy(&entry->event, event, sizeof(hypo_event_data_t));
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

int hypo_orch_report_drive(
    hypo_orchestrator_t orch,
    uint32_t bridge_id,
    uint32_t drive_type,
    float drive_level,
    hypo_urgency_t urgency,
    const char* description
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_report_drive: orch is NULL");
        return -1;
    }

    hypo_event_data_t event = {
        .event_type = HYPO_EVENT_DRIVE_ACTIVATED,
        .source = HYPO_BRIDGE_UNKNOWN,
        .urgency = urgency,
        .timestamp = get_timestamp_us(),
        .event_id = 0
    };

    /* Find source bridge type */
    ORCH_LOCK(orch);
    int idx = find_bridge_index(orch, bridge_id);
    if (idx >= 0) {
        event.source = orch->bridges[idx].type;
    }
    event.event_id = orch->next_event_id++;
    ORCH_UNLOCK(orch);

    event.drive.drive_type = drive_type;
    event.drive.drive_level = drive_level;
    event.drive.urgency_weight = (float)urgency / (float)HYPO_URGENCY_URGENT;

    if (description) {
        strncpy(event.drive.description, description, sizeof(event.drive.description) - 1);
    }

    return hypo_orch_publish(orch, bridge_id, &event);
}

int hypo_orch_get_drive_state(
    hypo_orchestrator_t orch,
    hypo_unified_drive_state_t* state
)
{
    if (!orch || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_drive_state: orch or state is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    /* Apply decay before assessment */
    apply_drive_decay(orch);

    memset(state, 0, sizeof(*state));

    state->unified_drive_level = orch->unified_drive_level;
    state->urgency = orch->current_urgency;
    state->active_drives = orch->active_drive_count;
    state->assessment_time = get_timestamp_us();

    /* Populate per-bridge breakdown */
    float max_drive = 0.0f;
    hypo_bridge_type_t max_source = HYPO_BRIDGE_UNKNOWN;
    uint32_t reporting = 0;

    for (uint32_t i = 0; i < orch->num_bridges && i < HYPO_BRIDGE_COUNT; i++) {
        hypo_bridge_info_t* bridge = &orch->bridges[i];
        state->bridge_drives[i].type = bridge->type;
        state->bridge_drives[i].drive_level = bridge->current_drive_level;
        state->bridge_drives[i].drive_count = bridge->drives_reported;
        state->bridge_drives[i].is_primary_source = false;

        if (bridge->current_drive_level > 0.0f) {
            reporting++;
        }

        if (bridge->current_drive_level > max_drive) {
            max_drive = bridge->current_drive_level;
            max_source = bridge->type;
        }
    }

    /* Mark primary source */
    for (uint32_t i = 0; i < orch->num_bridges && i < HYPO_BRIDGE_COUNT; i++) {
        if (orch->bridges[i].type == max_source) {
            state->bridge_drives[i].is_primary_source = true;
            break;
        }
    }

    state->bridges_reporting = reporting;
    state->primary_source = max_source;

    /* Generate summary */
    snprintf(state->drive_summary, sizeof(state->drive_summary),
             "Unified drive: %.2f (%s), %u active, source: %s",
             state->unified_drive_level,
             hypo_urgency_name(state->urgency),
             state->active_drives,
             hypo_bridge_type_name(max_source));

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_get_drive_level(
    hypo_orchestrator_t orch,
    float* drive_level
)
{
    if (!orch || !drive_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_drive_level: orch or drive_level is NULL");
        return -1;
    }

    ORCH_LOCK(orch);
    *drive_level = orch->unified_drive_level;
    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_update_drive_decay(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_update_drive_decay: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);
    apply_drive_decay(orch);
    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_clear_drives(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_clear_drives: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        orch->bridges[i].current_drive_level = 0.0f;
    }

    orch->unified_drive_level = 0.0f;
    orch->current_urgency = HYPO_URGENCY_NONE;
    orch->active_drive_count = 0;

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_trigger_stress(
    hypo_orchestrator_t orch,
    const char* reason
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_trigger_stress: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    orch->is_stressed = true;
    orch->stress_time = get_timestamp_us();
    orch->state = HYPO_ORCH_STATE_STRESS;

    if (reason) {
        strncpy(orch->stress_reason, reason, sizeof(orch->stress_reason) - 1);
    }

    orch->stats.stress_responses++;

    /* Publish stress event */
    hypo_event_data_t event = {
        .event_type = HYPO_EVENT_STRESS_RESPONSE,
        .source = HYPO_BRIDGE_UNKNOWN,
        .urgency = HYPO_URGENCY_URGENT,
        .timestamp = get_timestamp_us(),
        .event_id = orch->next_event_id++
    };
    event.stress.stress_level = orch->unified_drive_level;
    event.stress.is_acute = true;

    deliver_event(orch, 0, &event);

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_release_stress(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_release_stress: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    orch->is_stressed = false;
    memset(orch->stress_reason, 0, sizeof(orch->stress_reason));

    if (orch->config.enable_recovery) {
        orch->state = HYPO_ORCH_STATE_RECOVERY;
    } else {
        orch->state = HYPO_ORCH_STATE_MONITORING;
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_is_stressed(
    hypo_orchestrator_t orch,
    bool* in_stress
)
{
    if (!orch || !in_stress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_is_stressed: orch or in_stress is NULL");
        return -1;
    }

    ORCH_LOCK(orch);
    *in_stress = orch->is_stressed;
    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_broadcast_response(
    hypo_orchestrator_t orch,
    hypo_event_type_t response_type,
    const hypo_event_data_t* data
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_broadcast_response: orch is NULL");
        return -1;
    }

    hypo_event_data_t event;
    if (data) {
        memcpy(&event, data, sizeof(event));
    } else {
        memset(&event, 0, sizeof(event));
    }

    event.event_type = response_type;
    event.timestamp = get_timestamp_us();

    ORCH_LOCK(orch);
    event.event_id = orch->next_event_id++;

    /* Deliver to all bridges */
    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        if (orch->bridges[i].is_active) {
            event.source = orch->bridges[i].type;
            deliver_event(orch, 0, &event);
        }
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_get_state(
    hypo_orchestrator_t orch,
    hypo_orch_state_t* state
)
{
    if (!orch || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_state: orch or state is NULL");
        return -1;
    }

    ORCH_LOCK(orch);
    *state = orch->state;
    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_get_stats(
    hypo_orchestrator_t orch,
    hypo_orch_stats_t* stats
)
{
    if (!orch || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_get_stats: orch or stats is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    memcpy(stats, &orch->stats, sizeof(*stats));

    /* Calculate uptime */
    stats->uptime_us = get_timestamp_us() - orch->create_time;

    /* Calculate average drive level */
    if (orch->num_bridges > 0) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < orch->num_bridges; i++) {
            sum += orch->bridges[i].current_drive_level;
        }
        stats->avg_drive_level = sum / orch->num_bridges;
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_reset_stats(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_reset_stats: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    uint32_t registered = orch->stats.registered_bridges;
    uint32_t active = orch->stats.active_bridges;
    uint32_t connected = orch->stats.connected_bridges;
    uint32_t subscriptions = orch->stats.active_subscriptions;

    memset(&orch->stats, 0, sizeof(orch->stats));

    orch->stats.registered_bridges = registered;
    orch->stats.active_bridges = active;
    orch->stats.connected_bridges = connected;
    orch->stats.active_subscriptions = subscriptions;

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_connect_bio_async(
    hypo_orchestrator_t orch,
    void* router
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_connect_bio_async: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    orch->bio_async_router = router;
    orch->bio_async_connected = true;
    orch->stats.connected_bridges++;

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_disconnect_bio_async(hypo_orchestrator_t orch)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_disconnect_bio_async: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    if (orch->bio_async_connected) {
        orch->bio_async_router = NULL;
        orch->bio_async_connected = false;
        if (orch->stats.connected_bridges > 0) {
            orch->stats.connected_bridges--;
        }
    }

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_connect_immune(
    hypo_orchestrator_t orch,
    void* immune_system
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_connect_immune: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    orch->immune_system = immune_system;
    orch->stats.connected_bridges++;

    ORCH_UNLOCK(orch);

    return 0;
}

int hypo_orch_connect_logging(
    hypo_orchestrator_t orch,
    void* logger
)
{
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_orch_connect_logging: orch is NULL");
        return -1;
    }

    ORCH_LOCK(orch);

    orch->logger = logger;

    ORCH_UNLOCK(orch);

    return 0;
}

/* ============================================================================
 * UTILITY FUNCTION IMPLEMENTATIONS
 * ============================================================================ */

const char* hypo_bridge_type_name(hypo_bridge_type_t type)
{
    static const char* names[] = {
        "Unknown",
        "Emotion",
        "Executive",
        "Attention",
        "Sleep",
        "Immune",
        "Wellbeing",
        "Memory",
        "Perception",
        "Salience",
        "Reasoning",
        "GlobalWorkspace",
        "Introspection",
        "Curiosity",
        "GameTheory",
        "Imagination",
        "Epistemic",
        "Collective",
        "Bias",
        "TheoryOfMind",
        "Predictive",
        "Logging",
        "BioAsync"
    };

    if (type >= 0 && type < HYPO_BRIDGE_COUNT) {
        return names[type];
    }
    return "Invalid";
}

const char* hypo_event_type_name(hypo_event_type_t type)
{
    static const char* names[] = {
        "DriveActivated",
        "DriveSatisfied",
        "DriveConflict",
        "HomeostaticAlert",
        "CircadianPhase",
        "StressResponse",
        "AutonomicShift",
        "AlignmentCheck",
        "RewardSignal",
        "SetpointChange"
    };

    if (type >= 0 && type < HYPO_EVENT_COUNT) {
        return names[type];
    }
    return "Invalid";
}

const char* hypo_urgency_name(hypo_urgency_t urgency)
{
    static const char* names[] = {
        "None",
        "Low",
        "Moderate",
        "Elevated",
        "Urgent"
    };

    if (urgency >= 0 && urgency <= HYPO_URGENCY_URGENT) {
        return names[urgency];
    }
    return "Invalid";
}

const char* hypo_orch_state_name(hypo_orch_state_t state)
{
    static const char* names[] = {
        "Uninitialized",
        "Idle",
        "Monitoring",
        "Regulating",
        "Conflict",
        "Stress",
        "Recovery",
        "Error"
    };

    if (state >= 0 && state <= HYPO_ORCH_STATE_ERROR) {
        return names[state];
    }
    return "Invalid";
}

void hypo_orch_print_summary(hypo_orchestrator_t orch)
{
    if (!orch) {
        printf("Hypothalamus Orchestrator: NULL\n");
        return;
    }

    ORCH_LOCK(orch);

    printf("\n=== Hypothalamus Orchestrator Summary ===\n");
    printf("State: %s\n", hypo_orch_state_name(orch->state));
    printf("Stressed: %s\n", orch->is_stressed ? "Yes" : "No");
    if (orch->is_stressed) {
        printf("  Reason: %s\n", orch->stress_reason);
    }
    printf("\nDrive State:\n");
    printf("  Unified Level: %.3f\n", orch->unified_drive_level);
    printf("  Urgency: %s\n", hypo_urgency_name(orch->current_urgency));
    printf("  Active Drives: %u\n", orch->active_drive_count);
    printf("\nBridges: %u registered, %u active\n",
           orch->num_bridges, orch->stats.active_bridges);

    for (uint32_t i = 0; i < orch->num_bridges; i++) {
        hypo_bridge_info_t* b = &orch->bridges[i];
        printf("  [%u] %s: drive=%.3f, events=%u\n",
               b->bridge_id, b->name, b->current_drive_level, b->events_published);
    }

    printf("\nSubscriptions: %u active\n", orch->stats.active_subscriptions);
    printf("=========================================\n\n");

    ORCH_UNLOCK(orch);
}

void hypo_orch_print_stats(const hypo_orch_stats_t* stats)
{
    if (!stats) {
        printf("Hypothalamus Orchestrator Stats: NULL\n");
        return;
    }

    printf("\n=== Hypothalamus Orchestrator Statistics ===\n");
    printf("Bridges:\n");
    printf("  Registered: %u\n", stats->registered_bridges);
    printf("  Active: %u\n", stats->active_bridges);
    printf("  Connected: %u\n", stats->connected_bridges);

    printf("\nEvents:\n");
    printf("  Published: %lu\n", (unsigned long)stats->events_published);
    printf("  Delivered: %lu\n", (unsigned long)stats->events_delivered);
    printf("  Dropped: %lu\n", (unsigned long)stats->events_dropped);
    printf("  Subscriptions: %u\n", stats->active_subscriptions);
    printf("  Queue Depth: %u (max: %u)\n",
           stats->async_queue_depth, stats->async_queue_max);

    printf("\nDrives:\n");
    printf("  Activated: %lu\n", (unsigned long)stats->drives_activated);
    printf("  Satisfied: %lu\n", (unsigned long)stats->drives_satisfied);
    printf("  Conflicts: %lu\n", (unsigned long)stats->conflicts_detected);
    printf("  Stress Responses: %u\n", stats->stress_responses);
    printf("  Avg Level: %.3f\n", stats->avg_drive_level);
    printf("  Peak Level: %.3f\n", stats->peak_drive_level);

    printf("\nTiming:\n");
    printf("  Avg Event Latency: %lu us\n", (unsigned long)stats->avg_event_latency_us);
    printf("  Avg Response Time: %lu us\n", (unsigned long)stats->avg_response_time_us);
    printf("  Uptime: %lu us (%.2f sec)\n",
           (unsigned long)stats->uptime_us,
           (float)stats->uptime_us / 1000000.0f);
    printf("=============================================\n\n");
}
