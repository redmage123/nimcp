//=============================================================================
// nimcp_pr_bio_bridge.c - Prime Resonant Bio-Async Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_bio_bridge.c
 * @brief Implementation of PR Memory <-> Bio-Async integration bridge
 *
 * WHAT: Implements message routing, subscriber management, and priority queue
 * WHY:  Enable biologically-realistic memory system coordination
 * HOW:  Priority queue for messages, subscriber registry, bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_pr_bio_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_bio_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_bio_bridge module (instance-level) */
static inline void pr_bio_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_bio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_bio_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_bio_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_BIO_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform Abstraction
//=============================================================================

/* Simple mutex abstraction */
#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION pr_bio_mutex_t;
    #define PR_BIO_MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define PR_BIO_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define PR_BIO_MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define PR_BIO_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    typedef nimcp_mutex_t pr_bio_mutex_t;
    #define PR_BIO_MUTEX_INIT(m) nimcp_mutex_init(&(m), NULL)
    #define PR_BIO_MUTEX_DESTROY(m) nimcp_mutex_destroy(&(m))
    #define PR_BIO_MUTEX_LOCK(m) nimcp_mutex_lock(&(m))
    #define PR_BIO_MUTEX_UNLOCK(m) nimcp_mutex_unlock(&(m))
#endif

/* High-resolution timing */
#ifdef _WIN32
static uint64_t get_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000000) / freq.QuadPart);
}
#else
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}
#endif

static uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char g_pr_bio_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_pr_bio_last_error, msg, sizeof(g_pr_bio_last_error) - 1);
        g_pr_bio_last_error[sizeof(g_pr_bio_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Priority queue entry
 *
 * Messages stored in min-heap ordered by priority then timestamp
 */
typedef struct {
    pr_bio_message_t message;
    uint64_t enqueue_time_ms;
} pr_bio_queue_entry_t;

/**
 * @brief Internal bridge structure
 */
struct pr_bio_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_bio_bridge_config_t config;

    /* Message queue (min-heap by priority) */
    pr_bio_queue_entry_t* queue;
    size_t queue_size;
    size_t queue_capacity;
    pr_bio_mutex_t queue_mutex;
    bool queue_mutex_initialized;

    /* Subscribers */
    pr_bio_subscriber_t* subscribers;
    size_t num_subscribers;
    size_t subscriber_capacity;
    pr_bio_mutex_t subscriber_mutex;
    bool subscriber_mutex_initialized;
    uint64_t next_subscriber_id;

    /* Default priorities per type */
    pr_message_priority_t type_priorities[PR_MSG_TYPE_COUNT];

    /* Bio-async integration */
    bool is_connected;
    uint32_t sequence_counter;

    /* Statistics */
    pr_bio_bridge_stats_t stats;
    pr_bio_mutex_t stats_mutex;
    bool stats_mutex_initialized;

    /* State */
    bool initialized;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_bio_bridge, struct pr_bio_bridge_struct)

//=============================================================================
// Default Priority Mapping
//=============================================================================

/**
 * @brief Get default priority for message type
 */
static pr_message_priority_t default_priority_for_type(pr_message_type_t type) {
    switch (type) {
        case PR_MSG_FLASHBULB:
            return PR_PRIORITY_CRITICAL;
        case PR_MSG_ENCODE:
        case PR_MSG_RETRIEVE:
            return PR_PRIORITY_HIGH;
        case PR_MSG_CONSOLIDATE:
        case PR_MSG_ENTANGLE:
        case PR_MSG_PROMOTE:
        case PR_MSG_DEMOTE:
        case PR_MSG_REINFORCE:
        case PR_MSG_RECONSOLIDATE:
            return PR_PRIORITY_NORMAL;
        case PR_MSG_DECAY:
        case PR_MSG_EVICT:
            return PR_PRIORITY_LOW;
        default:
            return PR_PRIORITY_NORMAL;
    }
}

/**
 * @brief Get bio-async channel for message type
 */
static nimcp_bio_channel_type_t channel_for_type(pr_message_type_t type) {
    switch (type) {
        case PR_MSG_ENCODE:
        case PR_MSG_ENTANGLE:
        case PR_MSG_PROMOTE:
        case PR_MSG_REINFORCE:
            return BIO_CHANNEL_DOPAMINE;  /* Reward/completion */
        case PR_MSG_RETRIEVE:
            return BIO_CHANNEL_ACETYLCHOLINE;  /* Fast attention */
        case PR_MSG_CONSOLIDATE:
        case PR_MSG_RECONSOLIDATE:
            return BIO_CHANNEL_SEROTONIN;  /* Slow coordination */
        case PR_MSG_FLASHBULB:
        case PR_MSG_DECAY:
        case PR_MSG_DEMOTE:
        case PR_MSG_EVICT:
            return BIO_CHANNEL_NOREPINEPHRINE;  /* Alert/priority */
        default:
            return BIO_CHANNEL_DOPAMINE;
    }
}

//=============================================================================
// Priority Queue Operations (Min-Heap)
//=============================================================================

/**
 * @brief Compare queue entries (returns true if a should come before b)
 */
static bool queue_entry_less(const pr_bio_queue_entry_t* a,
                             const pr_bio_queue_entry_t* b) {
    /* Lower priority value = higher priority */
    if (a->message.priority != b->message.priority) {
        return a->message.priority < b->message.priority;
    }
    /* Same priority: earlier enqueue time first (FIFO within priority) */
    return a->enqueue_time_ms < b->enqueue_time_ms;
}

/**
 * @brief Heapify up (for insertion)
 */
static void heap_up(pr_bio_queue_entry_t* queue, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (queue_entry_less(&queue[index], &queue[parent])) {
            pr_bio_queue_entry_t tmp = queue[index];
            queue[index] = queue[parent];
            queue[parent] = tmp;
            index = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief Heapify down (for removal)
 */
static void heap_down(pr_bio_queue_entry_t* queue, size_t size, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < size && queue_entry_less(&queue[left], &queue[smallest])) {
            smallest = left;
        }
        if (right < size && queue_entry_less(&queue[right], &queue[smallest])) {
            smallest = right;
        }

        if (smallest != index) {
            pr_bio_queue_entry_t tmp = queue[index];
            queue[index] = queue[smallest];
            queue[smallest] = tmp;
            index = smallest;
        } else {
            break;
        }
    }
}

/**
 * @brief Push entry onto queue
 */
static bool queue_push(pr_bio_bridge_t bridge, const pr_bio_queue_entry_t* entry) {
    if (bridge->queue_size >= bridge->queue_capacity) {
        /* Try to grow queue */
        if (bridge->queue_capacity >= bridge->config.max_pending_messages) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "queue_push: capacity exceeded");
            return false;  /* At max capacity */
        }
        size_t new_cap = bridge->queue_capacity * PR_BIO_QUEUE_GROWTH_FACTOR;
        if (new_cap > bridge->config.max_pending_messages) {
            new_cap = bridge->config.max_pending_messages;
        }
        pr_bio_queue_entry_t* new_queue = (pr_bio_queue_entry_t*)nimcp_realloc(
            bridge->queue, new_cap * sizeof(pr_bio_queue_entry_t));
        if (!new_queue) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "queue_push: new_queue is NULL");
            return false;
        }
        bridge->queue = new_queue;
        bridge->queue_capacity = new_cap;
    }

    bridge->queue[bridge->queue_size] = *entry;
    heap_up(bridge->queue, bridge->queue_size);
    bridge->queue_size++;
    return true;
}

/**
 * @brief Pop top entry from queue
 */
static bool queue_pop(pr_bio_bridge_t bridge, pr_bio_queue_entry_t* entry) {
    if (bridge->queue_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_pop: bridge->queue_size is zero");
        return false;
    }

    *entry = bridge->queue[0];
    bridge->queue_size--;

    if (bridge->queue_size > 0) {
        bridge->queue[0] = bridge->queue[bridge->queue_size];
        heap_down(bridge->queue, bridge->queue_size, 0);
    }

    return true;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_config_t pr_bio_bridge_config_default(void) {
    pr_bio_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_pending_messages = PR_BIO_MAX_PENDING_MESSAGES;
    config.max_subscribers = PR_BIO_MAX_TOTAL_SUBSCRIBERS;
    config.default_timeout_ms = PR_BIO_DEFAULT_TIMEOUT_MS;
    config.batch_process_size = PR_BIO_DEFAULT_BATCH_SIZE;
    config.enable_statistics = true;
    config.enable_logging = false;
    config.auto_process_queue = false;
    config.default_channel = BIO_CHANNEL_DOPAMINE;

    return config;
}

NIMCP_EXPORT bool pr_bio_bridge_config_validate(const pr_bio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bio_bridge_config_validate: config is NULL");
        return false;
    }

    if (config->max_pending_messages == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_bio_bridge_config_validate: config->max_pending_messages is zero");
        return false;
    }
    if (config->max_subscribers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_bio_bridge_config_validate: config->max_subscribers is zero");
        return false;
    }
    if (config->batch_process_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_bio_bridge_config_validate: config->batch_process_size is zero");
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_t pr_bio_bridge_create(
    const pr_bio_bridge_config_t* config
) {
    /* Use default config if not provided */
    pr_bio_bridge_config_t cfg;
    if (config) {
        if (!pr_bio_bridge_config_validate(config)) {
            set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_bio_bridge_create: pr_bio_bridge_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_bio_bridge_config_default();
    }

    /* Allocate bridge structure */
    pr_bio_bridge_t bridge = (pr_bio_bridge_t)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        set_error("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->config = cfg;

    /* Allocate message queue */
    bridge->queue_capacity = PR_BIO_INITIAL_QUEUE_CAPACITY;
    if (bridge->queue_capacity > cfg.max_pending_messages) {
        bridge->queue_capacity = cfg.max_pending_messages;
    }
    bridge->queue = (pr_bio_queue_entry_t*)nimcp_calloc(
        bridge->queue_capacity, sizeof(pr_bio_queue_entry_t));
    if (!bridge->queue) {
        set_error("Failed to allocate message queue");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_bio_bridge_create: bridge->queue is NULL");
        return NULL;
    }
    bridge->queue_size = 0;

    /* Allocate subscriber array */
    bridge->subscriber_capacity = 64;  /* Start small, grow as needed */
    if (bridge->subscriber_capacity > cfg.max_subscribers) {
        bridge->subscriber_capacity = cfg.max_subscribers;
    }
    bridge->subscribers = (pr_bio_subscriber_t*)nimcp_calloc(
        bridge->subscriber_capacity, sizeof(pr_bio_subscriber_t));
    if (!bridge->subscribers) {
        set_error("Failed to allocate subscriber array");
        nimcp_free(bridge->queue);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_bio_bridge_create: bridge->subscribers is NULL");
        return NULL;
    }
    bridge->num_subscribers = 0;
    bridge->next_subscriber_id = 1;

    /* Initialize mutexes */
    PR_BIO_MUTEX_INIT(bridge->queue_mutex);
    bridge->queue_mutex_initialized = true;

    PR_BIO_MUTEX_INIT(bridge->subscriber_mutex);
    bridge->subscriber_mutex_initialized = true;

    PR_BIO_MUTEX_INIT(bridge->stats_mutex);
    bridge->stats_mutex_initialized = true;

    /* Initialize default priorities */
    for (int i = 0; i < PR_MSG_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_MSG_TYPE_COUNT > 256) {
            pr_bio_bridge_heartbeat("pr_bio_bridg_loop",
                             (float)(i + 1) / (float)PR_MSG_TYPE_COUNT);
        }

        bridge->type_priorities[i] = default_priority_for_type((pr_message_type_t)i);
    }

    /* Initialize state */
    bridge->is_connected = false;
    bridge->sequence_counter = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->initialized = true;

    return bridge;
}

NIMCP_EXPORT void pr_bio_bridge_destroy(pr_bio_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_bio");

    /* Disconnect if connected */
    if (bridge->is_connected) {
        pr_bio_bridge_disconnect(bridge);
    }

    /* Destroy mutexes */
    if (bridge->queue_mutex_initialized) {
        PR_BIO_MUTEX_DESTROY(bridge->queue_mutex);
    }
    if (bridge->subscriber_mutex_initialized) {
        PR_BIO_MUTEX_DESTROY(bridge->subscriber_mutex);
    }
    if (bridge->stats_mutex_initialized) {
        PR_BIO_MUTEX_DESTROY(bridge->stats_mutex);
    }

    /* Free arrays */
    if (bridge->queue) {
        nimcp_free(bridge->queue);
    }
    if (bridge->subscribers) {
        nimcp_free(bridge->subscribers);
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;
}

//=============================================================================
// Connection Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_connect(pr_bio_bridge_t bridge) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;
    if (bridge->is_connected) return PR_BIO_ERROR_ALREADY_CONNECTED;

    /* In a full implementation, this would:
     * 1. Check if bio-async is initialized
     * 2. Register with bio-async system
     * 3. Set up message handlers
     *
     * For now, just mark as connected
     */
    bridge->is_connected = true;

    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.last_activity_time_ms = get_time_ms();
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_disconnect(pr_bio_bridge_t bridge) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;

    /* Mark as disconnected */
    bridge->is_connected = false;

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT bool pr_bio_bridge_is_connected(const pr_bio_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->is_connected;
}

//=============================================================================
// Message Sending Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_send(
    pr_bio_bridge_t bridge,
    const pr_bio_message_t* message
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;
    if (!message) return PR_BIO_ERROR_NULL_POINTER;
    if (message->type >= PR_MSG_TYPE_COUNT) return PR_BIO_ERROR_INVALID_TYPE;

    /* Create queue entry */
    pr_bio_queue_entry_t entry;
    entry.message = *message;
    entry.enqueue_time_ms = get_time_ms();

    /* Assign sequence number */
    entry.message.sequence_number = bridge->sequence_counter++;

    /* Set timestamp if not already set */
    if (entry.message.timestamp <= 0.0f) {
        entry.message.timestamp = (float)entry.enqueue_time_ms;
    }

    /* Set priority if not explicitly set (priority 0 might be intentional CRITICAL) */
    /* Use type default if priority seems unset */
    if (message->priority >= PR_PRIORITY_COUNT) {
        entry.message.priority = bridge->type_priorities[message->type];
    }

    /* Add to queue */
    PR_BIO_MUTEX_LOCK(bridge->queue_mutex);
    bool success = queue_push(bridge, &entry);
    size_t queue_depth = bridge->queue_size;
    PR_BIO_MUTEX_UNLOCK(bridge->queue_mutex);

    if (!success) {
        if (bridge->config.enable_statistics) {
            PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
            bridge->stats.messages_dropped++;
            PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
        }
        return PR_BIO_ERROR_QUEUE_FULL;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.messages_sent++;
        bridge->stats.messages_by_type[message->type]++;
        bridge->stats.current_queue_depth = queue_depth;
        if (queue_depth > bridge->stats.peak_queue_depth) {
            bridge->stats.peak_queue_depth = queue_depth;
        }
        bridge->stats.last_activity_time_ms = entry.enqueue_time_ms;
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    /* Auto-process if configured */
    if (bridge->config.auto_process_queue) {
        pr_bio_bridge_process_pending(bridge);
    }

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_receive(
    pr_bio_bridge_t bridge,
    pr_bio_message_t* messages,
    size_t max_messages,
    size_t* count
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;
    if (!messages || !count) return PR_BIO_ERROR_NULL_POINTER;

    *count = 0;
    PR_BIO_MUTEX_LOCK(bridge->queue_mutex);

    while (*count < max_messages && bridge->queue_size > 0) {
        pr_bio_queue_entry_t entry;
        if (queue_pop(bridge, &entry)) {
            messages[*count] = entry.message;
            (*count)++;
        }
    }

    size_t remaining = bridge->queue_size;
    PR_BIO_MUTEX_UNLOCK(bridge->queue_mutex);

    /* Update statistics */
    if (bridge->config.enable_statistics && *count > 0) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.messages_received += *count;
        bridge->stats.current_queue_depth = remaining;
        bridge->stats.last_activity_time_ms = get_time_ms();
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT size_t pr_bio_bridge_process_pending(pr_bio_bridge_t bridge) {
    if (!bridge) return 0;

    size_t processed = 0;
    size_t batch_size = bridge->config.batch_process_size;

    while (processed < batch_size) {
        /* Get next message */
        pr_bio_queue_entry_t entry;
        PR_BIO_MUTEX_LOCK(bridge->queue_mutex);
        bool have_message = queue_pop(bridge, &entry);
        PR_BIO_MUTEX_UNLOCK(bridge->queue_mutex);

        if (!have_message) {
            break;
        }

        uint64_t process_start = get_time_us();

        /* Dispatch to subscribers */
        PR_BIO_MUTEX_LOCK(bridge->subscriber_mutex);

        /* Copy subscribers to avoid holding lock during callbacks */
        size_t num_subs = bridge->num_subscribers;
        pr_bio_subscriber_t* subs_copy = NULL;
        if (num_subs > 0) {
            subs_copy = (pr_bio_subscriber_t*)nimcp_malloc(
                num_subs * sizeof(pr_bio_subscriber_t));
            if (subs_copy) {
                memcpy(subs_copy, bridge->subscribers,
                       num_subs * sizeof(pr_bio_subscriber_t));
            }
        }

        PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);

        /* Invoke callbacks without lock */
        size_t notifications = 0;
        size_t succeeded = 0;
        if (subs_copy) {
            for (size_t i = 0; i < num_subs; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_subs > 256) {
                    pr_bio_bridge_heartbeat("pr_bio_bridg_loop",
                                     (float)(i + 1) / (float)num_subs);
                }

                if (!subs_copy[i].is_active) continue;
                if (subs_copy[i].type_filter != entry.message.type) continue;
                if (entry.message.priority > subs_copy[i].min_priority) continue;

                notifications++;
                if (subs_copy[i].callback(&entry.message, subs_copy[i].user_data)) {
                    succeeded++;
                }
            }
            nimcp_free(subs_copy);
            subs_copy = NULL;
        }

        uint64_t process_end = get_time_us();
        float process_time_ms = (float)(process_end - process_start) / 1000.0f;

        /* Update statistics */
        if (bridge->config.enable_statistics) {
            PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
            bridge->stats.notifications_sent += notifications;
            bridge->stats.callbacks_succeeded += succeeded;
            bridge->stats.callbacks_failed += (notifications - succeeded);

            /* Update latency tracking */
            float latency = (float)(process_start / 1000 - entry.enqueue_time_ms);
            if (bridge->stats.messages_received > 0) {
                float n = (float)bridge->stats.messages_received;
                bridge->stats.avg_latency_ms =
                    (bridge->stats.avg_latency_ms * (n - 1) + latency) / (fabsf(n) > 1e-7f ? n : 1e-7f);
            } else {
                bridge->stats.avg_latency_ms = latency;
            }
            if (latency > bridge->stats.max_latency_ms) {
                bridge->stats.max_latency_ms = latency;
            }

            /* Update process time tracking */
            if (processed > 0) {
                bridge->stats.avg_process_time_ms =
                    (bridge->stats.avg_process_time_ms * (float)processed +
                     process_time_ms) / (float)(processed + 1);
            } else {
                bridge->stats.avg_process_time_ms = process_time_ms;
            }

            bridge->stats.messages_received++;
            PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
        }

        processed++;
    }

    /* Update queue depth */
    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->queue_mutex);
        size_t depth = bridge->queue_size;
        PR_BIO_MUTEX_UNLOCK(bridge->queue_mutex);

        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.current_queue_depth = depth;
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return processed;
}

//=============================================================================
// Subscriber Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_subscribe(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_bio_subscriber_callback_t callback,
    void* user_data,
    uint64_t* subscriber_id
) {
    return pr_bio_bridge_subscribe_with_priority(
        bridge, type, PR_PRIORITY_LOW, callback, user_data, subscriber_id);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_subscribe_with_priority(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_message_priority_t min_priority,
    pr_bio_subscriber_callback_t callback,
    void* user_data,
    uint64_t* subscriber_id
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;
    if (!callback) return PR_BIO_ERROR_NULL_POINTER;
    if (type >= PR_MSG_TYPE_COUNT) return PR_BIO_ERROR_INVALID_TYPE;

    PR_BIO_MUTEX_LOCK(bridge->subscriber_mutex);

    /* Check capacity */
    if (bridge->num_subscribers >= bridge->config.max_subscribers) {
        PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);
        return PR_BIO_ERROR_SUBSCRIBER_LIMIT;
    }

    /* Grow array if needed */
    if (bridge->num_subscribers >= bridge->subscriber_capacity) {
        size_t new_cap = bridge->subscriber_capacity * 2;
        if (new_cap > bridge->config.max_subscribers) {
            new_cap = bridge->config.max_subscribers;
        }
        pr_bio_subscriber_t* new_subs = (pr_bio_subscriber_t*)nimcp_realloc(
            bridge->subscribers, new_cap * sizeof(pr_bio_subscriber_t));
        if (!new_subs) {
            PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);
            return PR_BIO_ERROR_NO_MEMORY;
        }
        bridge->subscribers = new_subs;
        bridge->subscriber_capacity = new_cap;
    }

    /* Add subscriber */
    uint64_t id = bridge->next_subscriber_id++;
    pr_bio_subscriber_t* sub = &bridge->subscribers[bridge->num_subscribers];
    sub->subscriber_id = id;
    sub->type_filter = type;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->min_priority = min_priority;
    sub->is_active = true;

    bridge->num_subscribers++;

    PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);

    if (subscriber_id) {
        *subscriber_id = id;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.active_subscribers = bridge->num_subscribers;
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_unsubscribe(
    pr_bio_bridge_t bridge,
    uint64_t subscriber_id
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;

    PR_BIO_MUTEX_LOCK(bridge->subscriber_mutex);

    /* Find subscriber */
    bool found = false;
    for (size_t i = 0; i < bridge->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_subscribers > 256) {
            pr_bio_bridge_heartbeat("pr_bio_bridg_loop",
                             (float)(i + 1) / (float)bridge->num_subscribers);
        }

        if (bridge->subscribers[i].subscriber_id == subscriber_id) {
            /* Remove by shifting remaining elements */
            for (size_t j = i; j < bridge->num_subscribers - 1; j++) {
                bridge->subscribers[j] = bridge->subscribers[j + 1];
            }
            bridge->num_subscribers--;
            found = true;
            break;
        }
    }

    PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);

    if (!found) {
        return PR_BIO_ERROR_NOT_FOUND;
    }

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.active_subscribers = bridge->num_subscribers;
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT size_t pr_bio_bridge_unsubscribe_all(
    pr_bio_bridge_t bridge,
    pr_message_type_t type
) {
    if (!bridge) return 0;

    PR_BIO_MUTEX_LOCK(bridge->subscriber_mutex);

    size_t removed = 0;
    size_t write_idx = 0;

    for (size_t i = 0; i < bridge->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_subscribers > 256) {
            pr_bio_bridge_heartbeat("pr_bio_bridg_loop",
                             (float)(i + 1) / (float)bridge->num_subscribers);
        }

        if (bridge->subscribers[i].type_filter == type) {
            removed++;
        } else {
            if (write_idx != i) {
                bridge->subscribers[write_idx] = bridge->subscribers[i];
            }
            write_idx++;
        }
    }

    bridge->num_subscribers = write_idx;

    PR_BIO_MUTEX_UNLOCK(bridge->subscriber_mutex);

    /* Update statistics */
    if (bridge->config.enable_statistics) {
        PR_BIO_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.active_subscribers = bridge->num_subscribers;
        PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return removed;
}

//=============================================================================
// Notification Helper Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_encode(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float importance
) {
    if (!bridge || !node) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_message_from_node(&msg, node, PR_MSG_ENCODE);
    msg.importance = importance;
    msg.priority = PR_PRIORITY_HIGH;

    return pr_bio_bridge_send(bridge, &msg);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_retrieve(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float resonance_score
) {
    if (!bridge || !node) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_message_from_node(&msg, node, PR_MSG_RETRIEVE);
    msg.importance = resonance_score;
    msg.priority = PR_PRIORITY_HIGH;

    return pr_bio_bridge_send(bridge, &msg);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_consolidate(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
) {
    if (!bridge || !node) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_message_from_node(&msg, node, PR_MSG_CONSOLIDATE);
    msg.previous_tier = from_tier;
    msg.tier = to_tier;

    /* Set appropriate sub-type based on direction */
    if (to_tier > from_tier) {
        msg.type = PR_MSG_PROMOTE;
    } else if (to_tier < from_tier) {
        msg.type = PR_MSG_DEMOTE;
    }

    return pr_bio_bridge_send(bridge, &msg);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_entangle(
    pr_bio_bridge_t bridge,
    uint64_t node1_id,
    uint64_t node2_id,
    float resonance
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_init_message(&msg, PR_MSG_ENTANGLE);
    msg.memory_id = node1_id;
    msg.correlation_id = node2_id;  /* Use correlation_id for second node */
    msg.importance = resonance;

    return pr_bio_bridge_send(bridge, &msg);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_decay(
    pr_bio_bridge_t bridge,
    uint64_t memory_id,
    float old_strength,
    float new_strength
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_init_message(&msg, PR_MSG_DECAY);
    msg.memory_id = memory_id;
    msg.importance = old_strength - new_strength;  /* Decay amount */

    /* Set quaternion components to represent decay */
    msg.quaternion.w = new_strength;
    msg.quaternion.x = 0.0f;
    msg.quaternion.y = 0.0f;
    msg.quaternion.z = old_strength;  /* Store old value for reference */

    return pr_bio_bridge_send(bridge, &msg);
}

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_flashbulb(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float arousal,
    float valence
) {
    if (!bridge || !node) return PR_BIO_ERROR_NULL_POINTER;

    pr_bio_message_t msg;
    pr_bio_bridge_message_from_node(&msg, node, PR_MSG_FLASHBULB);
    msg.priority = PR_PRIORITY_CRITICAL;  /* Flashbulbs are always critical */
    msg.importance = arousal;

    /* Encode arousal and valence in quaternion */
    msg.quaternion.x = arousal;
    msg.quaternion.y = valence;

    return pr_bio_bridge_send(bridge, &msg);
}

//=============================================================================
// Priority Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_set_priority(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_message_priority_t priority
) {
    if (!bridge) return PR_BIO_ERROR_NULL_POINTER;
    if (type >= PR_MSG_TYPE_COUNT) return PR_BIO_ERROR_INVALID_TYPE;
    if (priority >= PR_PRIORITY_COUNT) return PR_BIO_ERROR_INVALID_CONFIG;

    bridge->type_priorities[type] = priority;
    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT pr_message_priority_t pr_bio_bridge_get_priority(
    const pr_bio_bridge_t bridge,
    pr_message_type_t type
) {
    if (!bridge || type >= PR_MSG_TYPE_COUNT) {
        return PR_PRIORITY_NORMAL;
    }
    return bridge->type_priorities[type];
}

NIMCP_EXPORT nimcp_bio_channel_type_t pr_bio_bridge_get_channel(
    pr_message_type_t type
) {
    return channel_for_type(type);
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_get_stats(
    const pr_bio_bridge_t bridge,
    pr_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) return PR_BIO_ERROR_NULL_POINTER;

    PR_BIO_MUTEX_LOCK(((pr_bio_bridge_t)bridge)->stats_mutex);
    *stats = bridge->stats;
    PR_BIO_MUTEX_UNLOCK(((pr_bio_bridge_t)bridge)->stats_mutex);

    return PR_BIO_SUCCESS;
}

NIMCP_EXPORT void pr_bio_bridge_reset_stats(pr_bio_bridge_t bridge) {
    if (!bridge) return;

    PR_BIO_MUTEX_LOCK(bridge->stats_mutex);

    /* Keep current queue depth and subscriber count */
    size_t depth = bridge->stats.current_queue_depth;
    size_t subs = bridge->stats.active_subscribers;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.current_queue_depth = depth;
    bridge->stats.active_subscribers = subs;
    bridge->stats.last_activity_time_ms = get_time_ms();

    PR_BIO_MUTEX_UNLOCK(bridge->stats_mutex);
}

NIMCP_EXPORT size_t pr_bio_bridge_get_queue_depth(const pr_bio_bridge_t bridge) {
    if (!bridge) return 0;

    PR_BIO_MUTEX_LOCK(((pr_bio_bridge_t)bridge)->queue_mutex);
    size_t depth = bridge->queue_size;
    PR_BIO_MUTEX_UNLOCK(((pr_bio_bridge_t)bridge)->queue_mutex);

    return depth;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_bio_bridge_error_string(pr_bio_bridge_error_t error) {
    switch (error) {
        case PR_BIO_SUCCESS: return "Success";
        case PR_BIO_ERROR_NULL_POINTER: return "Null pointer";
        case PR_BIO_ERROR_NOT_CONNECTED: return "Not connected to bio-async";
        case PR_BIO_ERROR_QUEUE_FULL: return "Message queue full";
        case PR_BIO_ERROR_INVALID_TYPE: return "Invalid message type";
        case PR_BIO_ERROR_SUBSCRIBER_LIMIT: return "Subscriber limit reached";
        case PR_BIO_ERROR_NOT_FOUND: return "Not found";
        case PR_BIO_ERROR_TIMEOUT: return "Operation timed out";
        case PR_BIO_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_BIO_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_BIO_ERROR_ALREADY_CONNECTED: return "Already connected";
        default: return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_bio_bridge_type_name(pr_message_type_t type) {
    switch (type) {
        case PR_MSG_ENCODE: return "ENCODE";
        case PR_MSG_RETRIEVE: return "RETRIEVE";
        case PR_MSG_CONSOLIDATE: return "CONSOLIDATE";
        case PR_MSG_ENTANGLE: return "ENTANGLE";
        case PR_MSG_DECAY: return "DECAY";
        case PR_MSG_PROMOTE: return "PROMOTE";
        case PR_MSG_DEMOTE: return "DEMOTE";
        case PR_MSG_EVICT: return "EVICT";
        case PR_MSG_FLASHBULB: return "FLASHBULB";
        case PR_MSG_REINFORCE: return "REINFORCE";
        case PR_MSG_RECONSOLIDATE: return "RECONSOLIDATE";
        default: return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* pr_bio_bridge_priority_name(pr_message_priority_t priority) {
    switch (priority) {
        case PR_PRIORITY_CRITICAL: return "CRITICAL";
        case PR_PRIORITY_HIGH: return "HIGH";
        case PR_PRIORITY_NORMAL: return "NORMAL";
        case PR_PRIORITY_LOW: return "LOW";
        default: return "UNKNOWN";
    }
}

NIMCP_EXPORT void pr_bio_bridge_init_message(
    pr_bio_message_t* message,
    pr_message_type_t type
) {
    if (!message) return;

    memset(message, 0, sizeof(*message));
    message->type = type;
    message->priority = default_priority_for_type(type);
    message->timestamp = (float)get_time_ms();
    message->importance = 0.5f;  /* Default mid-importance */
    message->tier = PR_MEMORY_TIER_Z0;

    /* Initialize quaternion to identity */
    message->quaternion.w = 1.0f;
    message->quaternion.x = 0.0f;
    message->quaternion.y = 0.0f;
    message->quaternion.z = 0.0f;
}

NIMCP_EXPORT void pr_bio_bridge_message_from_node(
    pr_bio_message_t* message,
    const pr_memory_node_t* node,
    pr_message_type_t type
) {
    if (!message) return;

    pr_bio_bridge_init_message(message, type);

    if (node) {
        message->memory_id = node->node_id;
        message->signature = node->signature;
        message->quaternion = node->state;
        message->tier = node->tier;

        /* Use current strength as importance */
        message->importance = node->current_strength;
    }
}

NIMCP_EXPORT uint64_t pr_bio_bridge_current_time_ms(void) {
    return get_time_ms();
}

NIMCP_EXPORT void pr_bio_bridge_print_message(const pr_bio_message_t* message) {
    if (!message) {
        printf("pr_bio_message: NULL\n");
        return;
    }

    printf("pr_bio_message {\n");
    printf("  type: %s\n", pr_bio_bridge_type_name(message->type));
    printf("  priority: %s\n", pr_bio_bridge_priority_name(message->priority));
    printf("  memory_id: %lu\n", (unsigned long)message->memory_id);
    printf("  importance: %.3f\n", message->importance);
    printf("  timestamp: %.1f ms\n", message->timestamp);
    printf("  tier: %d\n", message->tier);
    printf("  sequence: %u\n", message->sequence_number);
    printf("  quaternion: (%.3f, %.3f, %.3f, %.3f)\n",
           message->quaternion.w, message->quaternion.x,
           message->quaternion.y, message->quaternion.z);
    printf("  channel: %s\n",
           nimcp_bio_channel_name(pr_bio_bridge_get_channel(message->type)));
    printf("}\n");
}

NIMCP_EXPORT void pr_bio_bridge_print_summary(const pr_bio_bridge_t bridge) {
    if (!bridge) {
        printf("pr_bio_bridge: NULL\n");
        return;
    }

    pr_bio_bridge_stats_t stats;
    pr_bio_bridge_get_stats(bridge, &stats);

    printf("=== PR Bio-Async Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "yes" : "no");
    printf("\nMessage Statistics:\n");
    printf("  Sent: %lu\n", (unsigned long)stats.messages_sent);
    printf("  Received: %lu\n", (unsigned long)stats.messages_received);
    printf("  Dropped: %lu\n", (unsigned long)stats.messages_dropped);
    printf("\nQueue Status:\n");
    printf("  Current depth: %zu\n", stats.current_queue_depth);
    printf("  Peak depth: %zu\n", stats.peak_queue_depth);
    printf("  Capacity: %zu\n", bridge->queue_capacity);
    printf("\nSubscribers:\n");
    printf("  Active: %zu\n", stats.active_subscribers);
    printf("  Notifications sent: %lu\n", (unsigned long)stats.notifications_sent);
    printf("  Callbacks succeeded: %lu\n", (unsigned long)stats.callbacks_succeeded);
    printf("  Callbacks failed: %lu\n", (unsigned long)stats.callbacks_failed);
    printf("\nTiming:\n");
    printf("  Avg latency: %.2f ms\n", stats.avg_latency_ms);
    printf("  Max latency: %.2f ms\n", stats.max_latency_ms);
    printf("  Avg process time: %.3f ms\n", stats.avg_process_time_ms);
    printf("\nMessages by Type:\n");
    for (int i = 0; i < PR_MSG_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_MSG_TYPE_COUNT > 256) {
            pr_bio_bridge_heartbeat("pr_bio_bridg_loop",
                             (float)(i + 1) / (float)PR_MSG_TYPE_COUNT);
        }

        if (stats.messages_by_type[i] > 0) {
            printf("  %s: %lu\n",
                   pr_bio_bridge_type_name((pr_message_type_t)i),
                   (unsigned long)stats.messages_by_type[i]);
        }
    }
    printf("===================================\n");
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_bio_bridge_set_instance_health_agent(
    pr_bio_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_bio_bridge_training_begin(pr_bio_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_bio_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_bio_bridge_heartbeat_instance(bridge->health_agent, "pr_bio_bridge_training_begin", 0.0f);
    return 0;
}

int pr_bio_bridge_training_end(pr_bio_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_bio_bridge_training_end: NULL argument");
        return -1;
    }
    pr_bio_bridge_heartbeat_instance(bridge->health_agent, "pr_bio_bridge_training_end", 1.0f);
    return 0;
}

int pr_bio_bridge_training_step(pr_bio_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_bio_bridge_training_step: NULL argument");
        return -1;
    }
    pr_bio_bridge_heartbeat_instance(bridge->health_agent, "pr_bio_bridge_training_step", progress);
    return 0;
}
