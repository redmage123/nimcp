/**
 * @file nimcp_training_integration_hub.c
 * @brief Implementation of the training integration hub
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Central hub for training module integration and event routing
 * WHY: Enable decoupled communication between training modules
 * HOW: Publish-subscribe event system with module registration and queries
 *
 * @author NIMCP Development Team
 */

#include "training/integration/nimcp_training_integration_hub.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/containers/nimcp_ring_buffer.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_integration_hub)

/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Event subscription entry
 * WHY: Track callback and user data for each subscription
 */
typedef struct subscription {
    uint32_t subscriber_id;
    training_event_callback_t callback;
    void* user_data;
    bool active;
} subscription_t;

/**
 * WHAT: Extended module info with query handler
 * WHY: Store both public info and internal handler
 */
typedef struct module_entry {
    training_module_info_t info;
    training_query_handler_t query_handler;
    bool slot_used;
} module_entry_t;

/**
 * WHAT: Internal hub structure
 * WHY: Hold all hub state and resources
 */
/** Entry in the async event queue — copies the event data (no payload ownership) */
typedef struct {
    uint32_t publisher_id;
    training_event_data_t data;
} async_event_entry_t;

#define ASYNC_QUEUE_CAPACITY 256

struct training_integration_hub_struct {
    training_hub_config_t config;
    module_entry_t* modules;           /* Array of max_modules */
    size_t module_count;               /* Number of registered modules */
    subscription_t** subscriptions;    /* [TRAINING_EVENT_COUNT][max_subscriptions] */
    size_t* subscription_counts;       /* Per event type subscription count */
    training_hub_stats_t stats;
    nimcp_mutex_t* mutex;
    bool initialized;
    nimcp_ring_buffer_t* async_queue;   /* Ring buffer of async_event_entry_t */
};

/* ========================================================================
 * UNLOCKED HELPER FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Find module by ID (unlocked)
 * WHY: Internal helper that assumes lock is held
 */
static module_entry_t* find_module_unlocked(training_integration_hub_t hub, uint32_t module_id) {
    if (!hub || !hub->modules) {
        return NULL;
    }

    for (size_t i = 0; i < hub->config.max_modules; i++) {
        if (hub->modules[i].slot_used &&
            hub->modules[i].info.module_id == module_id) {
            return &hub->modules[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Find empty module slot (unlocked)
 * WHY: Find a free slot for new module registration
 */
static module_entry_t* find_empty_slot_unlocked(training_integration_hub_t hub) {
    if (!hub || !hub->modules) {
        return NULL;
    }

    for (size_t i = 0; i < hub->config.max_modules; i++) {
        if (!hub->modules[i].slot_used) {
            return &hub->modules[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Find subscription by subscriber and event type (unlocked)
 * WHY: Find existing subscription for update or removal
 */
static subscription_t* find_subscription_unlocked(training_integration_hub_t hub,
                                                   uint32_t subscriber_id,
                                                   training_event_type_t event_type) {
    if (!hub || !hub->subscriptions) {
        return NULL;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        return NULL;
    }

    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        if (subs[i].active && subs[i].subscriber_id == subscriber_id) {
            return &subs[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Count active subscriptions (unlocked)
 * WHY: Update stats with current active subscription count
 */
static uint32_t count_active_subscriptions_unlocked(training_integration_hub_t hub) {
    if (!hub || !hub->subscriptions) {
        return 0;
    }

    uint32_t total = 0;
    for (int et = 0; et < TRAINING_EVENT_COUNT; et++) {
        subscription_t* subs = hub->subscriptions[et];
        size_t count = hub->subscription_counts[et];
        for (size_t i = 0; i < count; i++) {
            if (subs[i].active) {
                total++;
            }
        }
    }
    return total;
}

/**
 * WHAT: Unsubscribe all events for a module (unlocked)
 * WHY: Cleanup when module unregisters
 */
static void unsubscribe_all_unlocked(training_integration_hub_t hub, uint32_t module_id) {
    if (!hub || !hub->subscriptions) {
        return;
    }

    for (int et = 0; et < TRAINING_EVENT_COUNT; et++) {
        subscription_t* subs = hub->subscriptions[et];
        size_t count = hub->subscription_counts[et];
        for (size_t i = 0; i < count; i++) {
            if (subs[i].subscriber_id == module_id) {
                subs[i].active = false;
            }
        }
    }
}

/**
 * WHAT: Deliver event to all subscribers (unlocked)
 * WHY: Core event delivery logic
 */
static int deliver_event_unlocked(training_integration_hub_t hub,
                                   training_event_type_t event_type,
                                   const training_event_data_t* data) {
    if (!hub || !hub->subscriptions || !data) {
        return -1;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        return -1;
    }

    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        if (subs[i].active && subs[i].callback) {
            /* Check if subscriber module is active */
            module_entry_t* mod = find_module_unlocked(hub, subs[i].subscriber_id);
            if (mod && mod->info.is_active) {
                /* Call callback - errors are logged but not propagated */
                subs[i].callback(data, subs[i].user_data);
                hub->stats.events_delivered++;
            }
        }
    }

    return 0;
}

/* ========================================================================
 * STRING CONVERSION UTILITIES
 * ======================================================================== */

const char* training_category_to_string(training_category_t category) {
    switch (category) {
        case TRAINING_CATEGORY_CURRICULUM:    return "CURRICULUM";
        case TRAINING_CATEGORY_OPTIMIZATION:  return "OPTIMIZATION";
        case TRAINING_CATEGORY_ARCHITECTURE:  return "ARCHITECTURE";
        case TRAINING_CATEGORY_COMPRESSION:   return "COMPRESSION";
        case TRAINING_CATEGORY_DISTRIBUTED:   return "DISTRIBUTED";
        case TRAINING_CATEGORY_ROBUSTNESS:    return "ROBUSTNESS";
        case TRAINING_CATEGORY_CONTINUAL:     return "CONTINUAL";
        case TRAINING_CATEGORY_DATA:          return "DATA";
        default:                              return "UNKNOWN";
    }
}

const char* training_event_type_to_string(training_event_type_t event_type) {
    switch (event_type) {
        case TRAINING_EVENT_DIFFICULTY_UPDATED:    return "DIFFICULTY_UPDATED";
        case TRAINING_EVENT_EPOCH_COMPLETE:        return "EPOCH_COMPLETE";
        case TRAINING_EVENT_BATCH_COMPLETE:        return "BATCH_COMPLETE";
        case TRAINING_EVENT_LOSS_COMPUTED:         return "LOSS_COMPUTED";
        case TRAINING_EVENT_GRADIENT_READY:        return "GRADIENT_READY";
        case TRAINING_EVENT_WEIGHTS_UPDATED:       return "WEIGHTS_UPDATED";
        case TRAINING_EVENT_LR_ADJUSTED:           return "LR_ADJUSTED";
        case TRAINING_EVENT_TASK_SAMPLED:          return "TASK_SAMPLED";
        case TRAINING_EVENT_CHECKPOINT_SAVED:      return "CHECKPOINT_SAVED";
        case TRAINING_EVENT_HYPERPARAMS_TUNED:     return "HYPERPARAMS_TUNED";
        case TRAINING_EVENT_QUANTIZATION_APPLIED:  return "QUANTIZATION_APPLIED";
        case TRAINING_EVENT_KNOWLEDGE_DISTILLED:   return "KNOWLEDGE_DISTILLED";
        case TRAINING_EVENT_ADVERSARIAL_UPDATED:   return "ADVERSARIAL_UPDATED";
        case TRAINING_EVENT_TASK_SWITCHED:         return "TASK_SWITCHED";
        case TRAINING_EVENT_MEMORY_REPLAYED:       return "MEMORY_REPLAYED";
        case TRAINING_EVENT_ARCHITECTURE_UPDATED:  return "ARCHITECTURE_UPDATED";
        case TRAINING_EVENT_VALIDATION_COMPLETE:   return "VALIDATION_COMPLETE";
        case TRAINING_EVENT_EARLY_STOPPING:        return "EARLY_STOPPING";
        default:                                   return "UNKNOWN";
    }
}

const char* training_query_type_to_string(training_query_type_t query_type) {
    switch (query_type) {
        case TRAINING_QUERY_STATUS:            return "STATUS";
        case TRAINING_QUERY_DIFFICULTY_SCORE:  return "DIFFICULTY_SCORE";
        case TRAINING_QUERY_LEARNING_RATE:     return "LEARNING_RATE";
        case TRAINING_QUERY_LOSS_HISTORY:      return "LOSS_HISTORY";
        case TRAINING_QUERY_GRADIENT_STATS:    return "GRADIENT_STATS";
        case TRAINING_QUERY_TASK_PROGRESS:     return "TASK_PROGRESS";
        case TRAINING_QUERY_COMPRESSION_RATIO: return "COMPRESSION_RATIO";
        case TRAINING_QUERY_RESOURCE_USAGE:    return "RESOURCE_USAGE";
        case TRAINING_QUERY_SAMPLE_WEIGHTS:    return "SAMPLE_WEIGHTS";
        case TRAINING_QUERY_ARCHITECTURE_STATE: return "ARCHITECTURE_STATE";
        case TRAINING_QUERY_CHECKPOINT_INFO:   return "CHECKPOINT_INFO";
        case TRAINING_QUERY_HYPERPARAMS:       return "HYPERPARAMS";
        default:                               return "UNKNOWN";
    }
}

/* ========================================================================
 * DEFAULT CONFIGURATION
 * ======================================================================== */

training_hub_config_t training_hub_default_config(void) {
    training_hub_config_t config = {
        .max_modules = 32,
        .max_subscriptions = 128,
        .enable_async = true,
        .event_queue_size = 512,
        .enable_metrics = true
    };
    return config;
}

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

training_integration_hub_t training_hub_create(const training_hub_config_t* config) {
    /* Use defaults if config not provided */
    training_hub_config_t cfg = config ? *config : training_hub_default_config();

    /* Validate configuration */
    if (cfg.max_modules == 0 || cfg.max_subscriptions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_create: cfg.max_modules is zero");
        return NULL;
    }

    /* Allocate hub structure */
    training_integration_hub_t hub = nimcp_calloc(1, sizeof(struct training_integration_hub_struct));
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hub is NULL");

        return NULL;
    }

    hub->config = cfg;

    /* Allocate modules array */
    hub->modules = nimcp_calloc(cfg.max_modules, sizeof(module_entry_t));
    if (!hub->modules) {
        nimcp_free(hub);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_hub_create: hub->modules is NULL");
        return NULL;
    }

    /* Allocate subscriptions 2D array: [TRAINING_EVENT_COUNT][max_subscriptions] */
    hub->subscriptions = nimcp_calloc(TRAINING_EVENT_COUNT, sizeof(subscription_t*));
    if (!hub->subscriptions) {
        nimcp_free(hub->modules);
        nimcp_free(hub);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_hub_create: hub->subscriptions is NULL");
        return NULL;
    }

    for (int i = 0; i < TRAINING_EVENT_COUNT; i++) {
        hub->subscriptions[i] = nimcp_calloc(cfg.max_subscriptions, sizeof(subscription_t));
        if (!hub->subscriptions[i]) {
            /* Cleanup already allocated */
            for (int j = 0; j < i; j++) {
                nimcp_free(hub->subscriptions[j]);
            }
            nimcp_free(hub->subscriptions);
            nimcp_free(hub->modules);
            nimcp_free(hub);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_create: hub->subscriptions is NULL");
            return NULL;
        }
    }

    /* Allocate subscription counts array */
    hub->subscription_counts = nimcp_calloc(TRAINING_EVENT_COUNT, sizeof(size_t));
    if (!hub->subscription_counts) {
        for (int i = 0; i < TRAINING_EVENT_COUNT; i++) {
            nimcp_free(hub->subscriptions[i]);
        }
        nimcp_free(hub->subscriptions);
        nimcp_free(hub->modules);
        nimcp_free(hub);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_hub_create: hub->subscription_counts is NULL");
        return NULL;
    }

    /* Create mutex - use recursive mutex to allow nested calls from callbacks */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_RECURSIVE };
    hub->mutex = nimcp_mutex_create(&mutex_attr);
    if (!hub->mutex) {
        nimcp_free(hub->subscription_counts);
        for (int i = 0; i < TRAINING_EVENT_COUNT; i++) {
            nimcp_free(hub->subscriptions[i]);
        }
        nimcp_free(hub->subscriptions);
        nimcp_free(hub->modules);
        nimcp_free(hub);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_create: hub->mutex is NULL");
        return NULL;
    }

    /* Initialize stats */
    memset(&hub->stats, 0, sizeof(training_hub_stats_t));

    hub->module_count = 0;
    hub->async_queue = nimcp_ring_buffer_create(sizeof(async_event_entry_t),
                                                 ASYNC_QUEUE_CAPACITY);
    hub->initialized = true;

    return hub;
}

void training_hub_destroy(training_integration_hub_t hub) {
    if (!hub) {
        return;
    }

    /* Mark as not initialized to prevent further operations */
    hub->initialized = false;

    /* Free subscription arrays */
    if (hub->subscriptions) {
        for (int i = 0; i < TRAINING_EVENT_COUNT; i++) {
            if (hub->subscriptions[i]) {
                nimcp_free(hub->subscriptions[i]);
            }
        }
        nimcp_free(hub->subscriptions);
    }

    /* Free subscription counts */
    if (hub->subscription_counts) {
        nimcp_free(hub->subscription_counts);
    }

    /* Free modules array */
    if (hub->modules) {
        nimcp_free(hub->modules);
    }

    /* Free async queue */
    if (hub->async_queue) {
        nimcp_ring_buffer_destroy(hub->async_queue);
    }

    /* Destroy mutex */
    if (hub->mutex) {
        nimcp_mutex_free(hub->mutex);
    }

    /* Free hub structure */
    nimcp_free(hub);
}

/* ========================================================================
 * MODULE REGISTRATION
 * ======================================================================== */

int training_hub_register_module(training_integration_hub_t hub,
                                  uint32_t module_id,
                                  training_category_t category,
                                  const char* name,
                                  void* context) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!TRAINING_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_destroy: TRAINING_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Check if module_id already registered */
    if (find_module_unlocked(hub, module_id)) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "training_hub_destroy: validation failed");
        return -1;
    }

    /* Find empty slot */
    module_entry_t* slot = find_empty_slot_unlocked(hub);
    if (!slot) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_destroy: slot is NULL");
        return -1;
    }

    /* Fill in module info */
    slot->info.module_id = module_id;
    slot->info.category = category;
    slot->info.context = context;
    slot->info.is_active = true;
    slot->query_handler = NULL;
    slot->slot_used = true;

    /* Copy name safely */
    if (name) {
        strncpy(slot->info.name, name, sizeof(slot->info.name) - 1);
        slot->info.name[sizeof(slot->info.name) - 1] = '\0';
    } else {
        slot->info.name[0] = '\0';
    }

    hub->module_count++;
    hub->stats.registered_modules = (uint32_t)hub->module_count;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

int training_hub_unregister_module(training_integration_hub_t hub,
                                    uint32_t module_id) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    /* Unsubscribe all events for this module */
    unsubscribe_all_unlocked(hub, module_id);

    /* Clear slot */
    memset(mod, 0, sizeof(module_entry_t));
    mod->slot_used = false;

    hub->module_count--;
    hub->stats.registered_modules = (uint32_t)hub->module_count;
    hub->stats.active_subscriptions = count_active_subscriptions_unlocked(hub);

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * EVENT SUBSCRIPTION
 * ======================================================================== */

int training_hub_subscribe(training_integration_hub_t hub,
                            uint32_t subscriber_id,
                            training_event_type_t event_type,
                            training_event_callback_t callback,
                            void* user_data) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "callback is NULL");


        return -1;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_destroy: TRAINING_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Check subscriber is registered */
    if (!find_module_unlocked(hub, subscriber_id)) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    /* Check for existing subscription - update if found */
    subscription_t* existing = find_subscription_unlocked(hub, subscriber_id, event_type);
    if (existing) {
        existing->callback = callback;
        existing->user_data = user_data;
        nimcp_mutex_unlock(hub->mutex);
        return 0;
    }

    /* Find empty slot in subscriptions for this event type */
    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    /* First try to reuse inactive slot */
    subscription_t* slot = NULL;
    for (size_t i = 0; i < count; i++) {
        if (!subs[i].active) {
            slot = &subs[i];
            break;
        }
    }

    /* If no inactive slot, use next available */
    if (!slot) {
        if (count >= hub->config.max_subscriptions) {
            nimcp_mutex_unlock(hub->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "training_hub_destroy: capacity exceeded");
            return -1;
        }
        slot = &subs[count];
        hub->subscription_counts[event_type]++;
    }

    slot->subscriber_id = subscriber_id;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->active = true;

    hub->stats.active_subscriptions = count_active_subscriptions_unlocked(hub);

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

int training_hub_unsubscribe(training_integration_hub_t hub,
                              uint32_t subscriber_id,
                              training_event_type_t event_type) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: TRAINING_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    subscription_t* sub = find_subscription_unlocked(hub, subscriber_id, event_type);
    if (!sub) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    sub->active = false;
    hub->stats.active_subscriptions = count_active_subscriptions_unlocked(hub);

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * EVENT PUBLISHING
 * ======================================================================== */

int training_hub_publish(training_integration_hub_t hub,
                          uint32_t publisher_id,
                          training_event_type_t event_type,
                          const training_event_data_t* data) {
    if (!hub || !hub->initialized || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, data)");
        return -1;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: TRAINING_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Check publisher is registered */
    if (!find_module_unlocked(hub, publisher_id)) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    hub->stats.events_published++;

    /* Update training-specific metrics */
    if (event_type == TRAINING_EVENT_DIFFICULTY_UPDATED) {
        hub->stats.difficulty_updates++;
    } else if (event_type == TRAINING_EVENT_LR_ADJUSTED) {
        hub->stats.lr_adjustments++;
    } else if (event_type == TRAINING_EVENT_TASK_SWITCHED ||
               event_type == TRAINING_EVENT_TASK_SAMPLED) {
        hub->stats.task_switches++;
    } else if (event_type == TRAINING_EVENT_CHECKPOINT_SAVED) {
        hub->stats.checkpoints_saved++;
    }

    int result = deliver_event_unlocked(hub, event_type, data);

    nimcp_mutex_unlock(hub->mutex);
    return result;
}

int training_hub_publish_async(training_integration_hub_t hub,
                                uint32_t publisher_id,
                                training_event_type_t event_type,
                                const training_event_data_t* data) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "training_hub_publish_async: hub NULL or not initialized");
        return -1;
    }

    if (!hub->async_queue) {
        /* No queue — fall back to synchronous delivery */
        return training_hub_publish(hub, publisher_id, event_type, data);
    }

    /* Build a queue entry with a shallow copy of event data.
     * Per the API contract: "Asynchronous: Hub copies payload, caller can free" */
    async_event_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.publisher_id = publisher_id;
    if (data) {
        entry.data = *data;
        /* Deep-copy payload if present */
        if (data->payload && data->payload_size > 0) {
            entry.data.payload = nimcp_malloc(data->payload_size);
            if (entry.data.payload) {
                memcpy(entry.data.payload, data->payload, data->payload_size);
            } else {
                entry.data.payload_size = 0;
            }
        }
    }
    entry.data.event_type = event_type;

    nimcp_mutex_lock(hub->mutex);
    bool ok = nimcp_ring_buffer_push(hub->async_queue, &entry);
    if (ok) {
        hub->stats.async_queue_depth = (uint32_t)nimcp_ring_buffer_size(hub->async_queue);
        if (hub->stats.async_queue_depth > hub->stats.async_queue_max) {
            hub->stats.async_queue_max = hub->stats.async_queue_depth;
        }
    }
    nimcp_mutex_unlock(hub->mutex);

    return ok ? 0 : -1;
}

/* ========================================================================
 * INTER-MODULE QUERIES
 * ======================================================================== */

int training_hub_query_module(training_integration_hub_t hub,
                               uint32_t requester_id,
                               uint32_t target_id,
                               const training_query_t* query,
                               training_query_result_t* result) {
    if (!hub || !hub->initialized || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, query, result)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Check requester is registered */
    if (!find_module_unlocked(hub, requester_id)) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        return -1;
    }

    /* Find target module */
    module_entry_t* target = find_module_unlocked(hub, target_id);
    if (!target) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        return -1;
    }

    /* Check target is active */
    if (!target->info.is_active) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        return -1;
    }

    /* Check target has query handler */
    if (!target->query_handler) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        return -1;
    }

    hub->stats.queries_processed++;

    /* Call query handler (unlock during callback to avoid deadlock) */
    training_query_handler_t handler = target->query_handler;
    void* context = target->info.context;

    nimcp_mutex_unlock(hub->mutex);

    int handler_result = handler(query, result, context);
    if (handler_result != 0) {
        hub->stats.queries_failed++;
        return -1;
    }

    return 0;
}

int training_hub_get_module_info(training_integration_hub_t hub,
                                  uint32_t module_id,
                                  training_module_info_t* info) {
    if (!hub || !hub->initialized || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, info)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    *info = mod->info;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * QUERY HANDLER REGISTRATION
 * ======================================================================== */

int training_hub_register_query_handler(training_integration_hub_t hub,
                                         uint32_t module_id,
                                         training_query_handler_t handler) {
    if (!hub || !hub->initialized || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, handler)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    mod->query_handler = handler;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * MODULE STATE MANAGEMENT
 * ======================================================================== */

int training_hub_set_module_active(training_integration_hub_t hub,
                                    uint32_t module_id,
                                    bool is_active) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    mod->info.is_active = is_active;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

int training_hub_get_stats(training_integration_hub_t hub,
                            training_hub_stats_t* stats) {
    if (!hub || !hub->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, stats)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    *stats = hub->stats;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

int training_hub_reset_stats(training_integration_hub_t hub) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_reset_stats: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Preserve module and subscription counts, reset counters */
    uint32_t reg_modules = hub->stats.registered_modules;
    uint32_t active_subs = hub->stats.active_subscriptions;

    memset(&hub->stats, 0, sizeof(training_hub_stats_t));

    hub->stats.registered_modules = reg_modules;
    hub->stats.active_subscriptions = active_subs;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * CATEGORY-BASED OPERATIONS
 * ======================================================================== */

int training_hub_get_modules_by_category(training_integration_hub_t hub,
                                          training_category_t category,
                                          uint32_t* module_ids,
                                          uint32_t max_modules,
                                          uint32_t* count) {
    if (!hub || !hub->initialized || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_reset_stats: required parameter is NULL (hub, hub->initialized, count)");
        return -1;
    }

    if (!TRAINING_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_reset_stats: TRAINING_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    *count = 0;

    for (size_t i = 0; i < hub->config.max_modules && *count < max_modules; i++) {
        if (hub->modules[i].slot_used &&
            hub->modules[i].info.category == category) {
            if (module_ids) {
                module_ids[*count] = hub->modules[i].info.module_id;
            }
            (*count)++;
        }
    }

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

int training_hub_publish_to_category(training_integration_hub_t hub,
                                      uint32_t publisher_id,
                                      training_category_t category,
                                      training_event_type_t event_type,
                                      const training_event_data_t* data) {
    if (!hub || !hub->initialized || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_hub_reset_stats: required parameter is NULL (hub, hub->initialized, data)");
        return -1;
    }

    if (!TRAINING_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_reset_stats: TRAINING_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    if (!TRAINING_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_hub_reset_stats: TRAINING_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    /* Check publisher is registered */
    if (!find_module_unlocked(hub, publisher_id)) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    hub->stats.events_published++;

    /* Deliver to all subscribers in the category */
    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        if (subs[i].active && subs[i].callback) {
            /* Check if subscriber module is active and in the category */
            module_entry_t* mod = find_module_unlocked(hub, subs[i].subscriber_id);
            if (mod && mod->info.is_active && mod->info.category == category) {
                subs[i].callback(data, subs[i].user_data);
                hub->stats.events_delivered++;
            }
        }
    }

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * ASYNC QUEUE MANAGEMENT
 * ======================================================================== */

int training_hub_flush_async_queue(training_integration_hub_t hub,
                                    uint32_t timeout_ms) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "training_hub_flush_async_queue: hub NULL or not initialized");
        return -1;
    }
    (void)timeout_ms;  /* No blocking wait needed — we drain immediately */

    if (!hub->async_queue) {
        return 0;
    }

    /* Drain all queued events, delivering each synchronously */
    int delivered = 0;
    async_event_entry_t entry;
    while (true) {
        nimcp_mutex_lock(hub->mutex);
        bool got = nimcp_ring_buffer_pop_front(hub->async_queue, &entry);
        if (got) {
            hub->stats.async_queue_depth =
                (uint32_t)nimcp_ring_buffer_size(hub->async_queue);
        }
        nimcp_mutex_unlock(hub->mutex);

        if (!got) break;

        /* Deliver synchronously (publish handles its own locking) */
        training_hub_publish(hub, entry.publisher_id,
                             entry.data.event_type, &entry.data);
        /* Free deep-copied payload */
        if (entry.data.payload) {
            nimcp_free(entry.data.payload);
        }
        delivered++;
    }

    return delivered;
}

uint32_t training_hub_get_async_queue_depth(training_integration_hub_t hub) {
    if (!hub || !hub->initialized || !hub->async_queue) {
        return 0;
    }

    return (uint32_t)nimcp_ring_buffer_size(hub->async_queue);
}

/* ========================================================================
 * CONVENIENCE HELPERS FOR COMMON TRAINING EVENTS
 * ======================================================================== */

int training_hub_publish_difficulty_update(training_integration_hub_t hub,
                                            uint32_t publisher_id,
                                            float old_difficulty,
                                            float new_difficulty) {
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub is NULL");

        return -1;
    }

    training_difficulty_payload_t payload = {
        .old_difficulty = old_difficulty,
        .new_difficulty = new_difficulty,
        .samples_at_level = 0,
        .progression_rate = 0.0f
    };

    training_event_data_t event = {
        .event_type = TRAINING_EVENT_DIFFICULTY_UPDATED,
        .source_module_id = publisher_id,
        .timestamp = 0,  /* Could use nimcp_time_get_us() if available */
        .priority = TRAINING_PRIORITY_NORMAL,
        .epoch = 0,
        .batch = 0,
        .loss_value = 0.0f,
        .learning_rate = 0.0f,
        .payload = &payload,
        .payload_size = sizeof(payload)
    };

    return training_hub_publish(hub, publisher_id, TRAINING_EVENT_DIFFICULTY_UPDATED, &event);
}

int training_hub_publish_loss(training_integration_hub_t hub,
                               uint32_t publisher_id,
                               uint32_t epoch,
                               uint32_t batch,
                               float loss) {
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub is NULL");

        return -1;
    }

    training_loss_payload_t payload = {
        .total_loss = loss,
        .primary_loss = loss,
        .regularization_loss = 0.0f,
        .auxiliary_loss = 0.0f,
        .batch_size = 0
    };

    training_event_data_t event = {
        .event_type = TRAINING_EVENT_LOSS_COMPUTED,
        .source_module_id = publisher_id,
        .timestamp = 0,
        .priority = TRAINING_PRIORITY_NORMAL,
        .epoch = epoch,
        .batch = batch,
        .loss_value = loss,
        .learning_rate = 0.0f,
        .payload = &payload,
        .payload_size = sizeof(payload)
    };

    return training_hub_publish(hub, publisher_id, TRAINING_EVENT_LOSS_COMPUTED, &event);
}

int training_hub_publish_lr_adjustment(training_integration_hub_t hub,
                                        uint32_t publisher_id,
                                        float old_lr,
                                        float new_lr) {
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub is NULL");

        return -1;
    }

    training_lr_payload_t payload = {
        .old_lr = old_lr,
        .new_lr = new_lr,
        .scheduler_name = NULL,
        .step = 0
    };

    training_event_data_t event = {
        .event_type = TRAINING_EVENT_LR_ADJUSTED,
        .source_module_id = publisher_id,
        .timestamp = 0,
        .priority = TRAINING_PRIORITY_NORMAL,
        .epoch = 0,
        .batch = 0,
        .loss_value = 0.0f,
        .learning_rate = new_lr,
        .payload = &payload,
        .payload_size = sizeof(payload)
    };

    return training_hub_publish(hub, publisher_id, TRAINING_EVENT_LR_ADJUSTED, &event);
}

int training_hub_publish_epoch_complete(training_integration_hub_t hub,
                                         uint32_t publisher_id,
                                         uint32_t epoch,
                                         float avg_loss) {
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub is NULL");

        return -1;
    }

    training_event_data_t event = {
        .event_type = TRAINING_EVENT_EPOCH_COMPLETE,
        .source_module_id = publisher_id,
        .timestamp = 0,
        .priority = TRAINING_PRIORITY_NORMAL,
        .epoch = epoch,
        .batch = 0,
        .loss_value = avg_loss,
        .learning_rate = 0.0f,
        .payload = NULL,
        .payload_size = 0
    };

    return training_hub_publish(hub, publisher_id, TRAINING_EVENT_EPOCH_COMPLETE, &event);
}
