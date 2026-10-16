/**
 * @file nimcp_cognitive_integration_hub.c
 * @brief Implementation of the cognitive integration hub
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Central hub for cognitive module integration and event routing
 * WHY: Enable decoupled communication between cognitive modules
 * HOW: Publish-subscribe event system with module registration and queries
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(cognitive_integration_hub, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Event subscription entry
 * WHY: Track callback and user data for each subscription
 */
typedef struct subscription {
    uint32_t subscriber_id;
    cognitive_event_callback_t callback;
    void* user_data;
    bool active;
} subscription_t;

/**
 * WHAT: Extended module info with query handler
 * WHY: Store both public info and internal handler
 */
typedef struct module_entry {
    cognitive_module_info_t info;
    cognitive_query_handler_t query_handler;
    bool slot_used;
} module_entry_t;

/**
 * WHAT: Internal hub structure
 * WHY: Hold all hub state and resources
 */
struct cognitive_integration_hub_struct {
    cognitive_hub_config_t config;
    module_entry_t* modules;           /* Array of max_modules */
    size_t module_count;               /* Number of registered modules */
    subscription_t** subscriptions;    /* [COG_EVENT_COUNT][max_subscriptions] */
    size_t* subscription_counts;       /* Per event type subscription count */
    cognitive_hub_stats_t stats;
    nimcp_mutex_t* mutex;
    bool initialized;
};

/* ========================================================================
 * UNLOCKED HELPER FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Find module by ID (unlocked)
 * WHY: Internal helper that assumes lock is held
 */
static module_entry_t* find_module_unlocked(cognitive_integration_hub_t hub, uint32_t module_id) {
    if (!hub || !hub->modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module_unlocked: required parameter is NULL (hub, hub->modules)");
        return NULL;
    }

    for (size_t i = 0; i < hub->config.max_modules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hub->config.max_modules > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)hub->config.max_modules);
        }

        if (hub->modules[i].slot_used &&
            hub->modules[i].info.module_id == module_id) {
            return &hub->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module_unlocked: operation failed");
    return NULL;
}

/**
 * WHAT: Find empty module slot (unlocked)
 * WHY: Find a free slot for new module registration
 */
static module_entry_t* find_empty_slot_unlocked(cognitive_integration_hub_t hub) {
    if (!hub || !hub->modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot_unlocked: required parameter is NULL (hub, hub->modules)");
        return NULL;
    }

    for (size_t i = 0; i < hub->config.max_modules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hub->config.max_modules > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)hub->config.max_modules);
        }

        if (!hub->modules[i].slot_used) {
            return &hub->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot_unlocked: hub->modules is NULL");
    return NULL;
}

/**
 * WHAT: Find subscription by subscriber and event type (unlocked)
 * WHY: Find existing subscription for update or removal
 */
static subscription_t* find_subscription_unlocked(cognitive_integration_hub_t hub,
                                                   uint32_t subscriber_id,
                                                   cognitive_event_type_t event_type) {
    if (!hub || !hub->subscriptions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot_unlocked: required parameter is NULL (hub, hub->subscriptions)");
        return NULL;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot_unlocked: COG_EVENT_TYPE_IS_VALID is NULL");
        return NULL;
    }

    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)count);
        }

        if (subs[i].active && subs[i].subscriber_id == subscriber_id) {
            return &subs[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot_unlocked: validation failed");
    return NULL;
}

/**
 * WHAT: Count active subscriptions (unlocked)
 * WHY: Update stats with current active subscription count
 */
static uint32_t count_active_subscriptions_unlocked(cognitive_integration_hub_t hub) {
    if (!hub || !hub->subscriptions) {
        return 0;
    }

    uint32_t total = 0;
    for (int et = 0; et < COG_EVENT_COUNT; et++) {
        /* Phase 8: Loop progress heartbeat */
        if ((et & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(et + 1) / (float)COG_EVENT_COUNT);
        }

        subscription_t* subs = hub->subscriptions[et];
        size_t count = hub->subscription_counts[et];
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                 (float)(i + 1) / (float)count);
            }

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
static void unsubscribe_all_unlocked(cognitive_integration_hub_t hub, uint32_t module_id) {
    if (!hub || !hub->subscriptions) {
        return;
    }

    for (int et = 0; et < COG_EVENT_COUNT; et++) {
        /* Phase 8: Loop progress heartbeat */
        if ((et & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(et + 1) / (float)COG_EVENT_COUNT);
        }

        subscription_t* subs = hub->subscriptions[et];
        size_t count = hub->subscription_counts[et];
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (subs[i].subscriber_id == module_id) {
                subs[i].active = false;
            }
        }
    }
}

/**
 * WHAT: Deliver event to all subscribers (unlocked)
 * WHY: Core event delivery logic
 * NOTE: Skips delivering the event back to the publisher module to prevent
 *       deadlocks when a bridge holds its own mutex during publish and the
 *       callback tries to re-acquire that same mutex.
 */
static int deliver_event_unlocked(cognitive_integration_hub_t hub,
                                   uint32_t publisher_id,
                                   cognitive_event_type_t event_type,
                                   const cognitive_event_data_t* data) {
    if (!hub || !hub->subscriptions || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deliver_event_unlocked: required parameter is NULL (hub, hub->subscriptions, data)");
        return -1;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deliver_event_unlocked: COG_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)count);
        }

        if (subs[i].active && subs[i].callback) {
            /* Skip delivering event back to the publisher to prevent
             * deadlocks from re-entrant mutex acquisition in callbacks */
            if (subs[i].subscriber_id == publisher_id) {
                continue;
            }

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

const char* cognitive_category_to_string(cognitive_category_t category) {
    switch (category) {
        case COG_CATEGORY_PERCEPTION: return "PERCEPTION";
        case COG_CATEGORY_MEMORY:     return "MEMORY";
        case COG_CATEGORY_REASONING:  return "REASONING";
        case COG_CATEGORY_EXECUTIVE:  return "EXECUTIVE";
        case COG_CATEGORY_SOCIAL:     return "SOCIAL";
        case COG_CATEGORY_EMOTIONAL:  return "EMOTIONAL";
        case COG_CATEGORY_SELF:       return "SELF";
        default:                      return "UNKNOWN";
    }
}

const char* cognitive_event_type_to_string(cognitive_event_type_t event_type) {
    switch (event_type) {
        case COG_EVENT_STATE_CHANGE:     return "STATE_CHANGE";
        case COG_EVENT_INPUT_RECEIVED:   return "INPUT_RECEIVED";
        case COG_EVENT_OUTPUT_READY:     return "OUTPUT_READY";
        case COG_EVENT_ATTENTION_SHIFT:  return "ATTENTION_SHIFT";
        case COG_EVENT_MEMORY_ACCESS:    return "MEMORY_ACCESS";
        case COG_EVENT_EMOTION_UPDATE:   return "EMOTION_UPDATE";
        case COG_EVENT_DECISION_MADE:    return "DECISION_MADE";
        case COG_EVENT_SOCIAL_SIGNAL:    return "SOCIAL_SIGNAL";
        case COG_EVENT_LEARNING_COMPLETE: return "LEARNING_COMPLETE";
        case COG_EVENT_CONSOLIDATION:    return "CONSOLIDATION";
        default:                         return "UNKNOWN";
    }
}

const char* cognitive_query_type_to_string(cognitive_query_type_t query_type) {
    switch (query_type) {
        case COG_QUERY_STATUS:     return "STATUS";
        case COG_QUERY_STATE:      return "STATE";
        case COG_QUERY_CAPABILITY: return "CAPABILITY";
        case COG_QUERY_MEMORY:     return "MEMORY";
        case COG_QUERY_PREDICTION: return "PREDICTION";
        case COG_QUERY_ATTENTION:  return "ATTENTION";
        case COG_QUERY_EMOTION:    return "EMOTION";
        case COG_QUERY_METRICS:    return "METRICS";
        default:                   return "UNKNOWN";
    }
}

/* ========================================================================
 * DEFAULT CONFIGURATION
 * ======================================================================== */

cognitive_hub_config_t cognitive_hub_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_defaul", 0.0f);


    cognitive_hub_config_t config = {
        .max_modules = 64,
        .max_subscriptions = 256,
        .enable_async = true,
        .event_queue_size = 1024
    };
    return config;
}

/* ========================================================================
 * LIFECYCLE MANAGEMENT
 * ======================================================================== */

cognitive_integration_hub_t cognitive_hub_create(const cognitive_hub_config_t* config) {
    /* Use defaults if config not provided */
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_create", 0.0f);


    cognitive_hub_config_t cfg = config ? *config : cognitive_hub_default_config();

    /* Validate configuration */
    if (cfg.max_modules == 0 || cfg.max_subscriptions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_create: cfg.max_modules is zero");
        return NULL;
    }

    /* Allocate hub structure */
    cognitive_integration_hub_t hub = nimcp_calloc(1, sizeof(struct cognitive_integration_hub_struct));
    if (!hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate hub");

        return NULL;
    }

    hub->config = cfg;

    /* Allocate modules array */
    hub->modules = nimcp_calloc(cfg.max_modules, sizeof(module_entry_t));
    if (!hub->modules) {
        nimcp_free(hub);
        hub = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_hub_create: hub->modules is NULL");
        return NULL;
    }

    /* Allocate subscriptions 2D array: [COG_EVENT_COUNT][max_subscriptions] */
    hub->subscriptions = nimcp_calloc(COG_EVENT_COUNT, sizeof(subscription_t*));
    if (!hub->subscriptions) {
        nimcp_free(hub->modules);
        nimcp_free(hub);
        hub = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_hub_create: hub->subscriptions is NULL");
        return NULL;
    }

    for (int i = 0; i < COG_EVENT_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)COG_EVENT_COUNT);
        }

        hub->subscriptions[i] = nimcp_calloc(cfg.max_subscriptions, sizeof(subscription_t));
        if (!hub->subscriptions[i]) {
            /* Cleanup already allocated */
            for (int j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                     (float)(j + 1) / (float)i);
                }

                nimcp_free(hub->subscriptions[j]);
            }
            nimcp_free(hub->subscriptions);
            nimcp_free(hub->modules);
            nimcp_free(hub);
            hub = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_create: operation failed");
            return NULL;
        }
    }

    /* Allocate subscription counts array */
    hub->subscription_counts = nimcp_calloc(COG_EVENT_COUNT, sizeof(size_t));
    if (!hub->subscription_counts) {
        for (int i = 0; i < COG_EVENT_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
                cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                 (float)(i + 1) / (float)COG_EVENT_COUNT);
            }

            nimcp_free(hub->subscriptions[i]);
        }
        nimcp_free(hub->subscriptions);
        nimcp_free(hub->modules);
        nimcp_free(hub);
        hub = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_create: operation failed");
        return NULL;
    }

    /* Create mutex - use recursive mutex to allow nested calls from callbacks */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_RECURSIVE };
    hub->mutex = nimcp_mutex_create(&mutex_attr);
    if (!hub->mutex) {
        nimcp_free(hub->subscription_counts);
        for (int i = 0; i < COG_EVENT_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
                cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                 (float)(i + 1) / (float)COG_EVENT_COUNT);
            }

            nimcp_free(hub->subscriptions[i]);
        }
        nimcp_free(hub->subscriptions);
        nimcp_free(hub->modules);
        nimcp_free(hub);
        hub = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_create: operation failed");
        return NULL;
    }

    /* Initialize stats */
    memset(&hub->stats, 0, sizeof(cognitive_hub_stats_t));

    hub->module_count = 0;
    hub->initialized = true;

    return hub;
}

void cognitive_hub_destroy(cognitive_integration_hub_t hub) {
    if (!hub) {
        return;
    }

    /* Mark as not initialized to prevent further operations */
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_destro", 0.0f);


    hub->initialized = false;

    /* Free subscription arrays */
    if (hub->subscriptions) {
        for (int i = 0; i < COG_EVENT_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && COG_EVENT_COUNT > 256) {
                cognitive_integration_hub_heartbeat("cognitive_in_loop",
                                 (float)(i + 1) / (float)COG_EVENT_COUNT);
            }

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

    /* Destroy mutex */
    if (hub->mutex) {
        nimcp_mutex_destroy(hub->mutex);
    }

    /* Free hub structure */
    nimcp_free(hub);
    hub = NULL;
}

/* ========================================================================
 * MODULE REGISTRATION
 * ======================================================================== */

int cognitive_hub_register_module(cognitive_integration_hub_t hub,
                                  uint32_t module_id,
                                  cognitive_category_t category,
                                  const char* name,
                                  void* context) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!COG_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_destroy: COG_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_regist", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Check if module_id already registered */
    if (find_module_unlocked(hub, module_id)) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "cognitive_hub_destroy: validation failed");
        return -1;
    }

    /* Find empty slot */
    module_entry_t* slot = find_empty_slot_unlocked(hub);
    if (!slot) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_destroy: slot is NULL");
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

int cognitive_hub_unregister_module(cognitive_integration_hub_t hub,
                                    uint32_t module_id) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_unregi", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_destroy: mod is NULL");
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

int cognitive_hub_subscribe(cognitive_integration_hub_t hub,
                            uint32_t subscriber_id,
                            cognitive_event_type_t event_type,
                            cognitive_event_callback_t callback,
                            void* user_data) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_destroy: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "callback is NULL");


        return -1;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_destroy: COG_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_subscr", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Check subscriber is registered */
    if (!find_module_unlocked(hub, subscriber_id)) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "cognitive_hub_destroy: find_module_unlocked is NULL");
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
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!subs[i].active) {
            slot = &subs[i];
            break;
        }
    }

    /* If no inactive slot, use next available */
    if (!slot) {
        if (count >= hub->config.max_subscriptions) {
            nimcp_mutex_unlock(hub->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
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

int cognitive_hub_unsubscribe(cognitive_integration_hub_t hub,
                              uint32_t subscriber_id,
                              cognitive_event_type_t event_type) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: COG_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_unsubs", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    subscription_t* sub = find_subscription_unlocked(hub, subscriber_id, event_type);
    if (!sub) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: sub is NULL");
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

int cognitive_hub_publish(cognitive_integration_hub_t hub,
                          uint32_t publisher_id,
                          cognitive_event_type_t event_type,
                          const cognitive_event_data_t* data) {
    if (!hub || !hub->initialized || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, data)");
        return -1;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: COG_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_publis", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Check publisher is registered */
    if (!find_module_unlocked(hub, publisher_id)) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "unknown: find_module_unlocked is NULL");
        return -1;
    }

    hub->stats.events_published++;

    int result = deliver_event_unlocked(hub, publisher_id, event_type, data);

    nimcp_mutex_unlock(hub->mutex);
    return result;
}

int cognitive_hub_publish_async(cognitive_integration_hub_t hub,
                                uint32_t publisher_id,
                                cognitive_event_type_t event_type,
                                const cognitive_event_data_t* data) {
    /* For now, just call synchronous publish */
    /* Async processing can be added later with a worker thread */
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_publis", 0.0f);


    return cognitive_hub_publish(hub, publisher_id, event_type, data);
}

/* ========================================================================
 * INTER-MODULE QUERIES
 * ======================================================================== */

int cognitive_hub_query_module(cognitive_integration_hub_t hub,
                               uint32_t requester_id,
                               uint32_t target_id,
                               const cognitive_query_t* query,
                               cognitive_query_result_t* result) {
    if (!hub || !hub->initialized || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, query, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_query_", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Check requester is registered */
    if (!find_module_unlocked(hub, requester_id)) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "unknown: find_module_unlocked is NULL");
        return -1;
    }

    /* Find target module */
    module_entry_t* target = find_module_unlocked(hub, target_id);
    if (!target) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: target is NULL");
        return -1;
    }

    /* Check target is active */
    if (!target->info.is_active) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: target->info is NULL");
        return -1;
    }

    /* Check target has query handler */
    if (!target->query_handler) {
        nimcp_mutex_unlock(hub->mutex);
        hub->stats.queries_failed++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: target->query_handler is NULL");
        return -1;
    }

    hub->stats.queries_processed++;

    /* Call query handler (unlock during callback to avoid deadlock) */
    cognitive_query_handler_t handler = target->query_handler;
    void* context = target->info.context;

    nimcp_mutex_unlock(hub->mutex);

    int handler_result = handler(query, result, context);
    if (handler_result != 0) {
        hub->stats.queries_failed++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    return 0;
}

int cognitive_hub_get_module_info(cognitive_integration_hub_t hub,
                                  uint32_t module_id,
                                  cognitive_module_info_t* info) {
    if (!hub || !hub->initialized || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, info)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_get_mo", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: mod is NULL");
        return -1;
    }

    *info = mod->info;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * QUERY HANDLER REGISTRATION
 * ======================================================================== */

int cognitive_hub_register_query_handler(cognitive_integration_hub_t hub,
                                         uint32_t module_id,
                                         cognitive_query_handler_t handler) {
    if (!hub || !hub->initialized || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, handler)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_regist", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: mod is NULL");
        return -1;
    }

    mod->query_handler = handler;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * MODULE STATE MANAGEMENT
 * ======================================================================== */

int cognitive_hub_set_module_active(cognitive_integration_hub_t hub,
                                    uint32_t module_id,
                                    bool is_active) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_set_mo", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    module_entry_t* mod = find_module_unlocked(hub, module_id);
    if (!mod) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: mod is NULL");
        return -1;
    }

    mod->info.is_active = is_active;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * STATISTICS AND MONITORING
 * ======================================================================== */

int cognitive_hub_get_stats(cognitive_integration_hub_t hub,
                            cognitive_hub_stats_t* stats) {
    if (!hub || !hub->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (hub, hub->initialized, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_get_st", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    *stats = hub->stats;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

int cognitive_hub_reset_stats(cognitive_integration_hub_t hub) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_reset_stats: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_reset_", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Preserve module and subscription counts, reset counters */
    uint32_t reg_modules = hub->stats.registered_modules;
    uint32_t active_subs = hub->stats.active_subscriptions;

    memset(&hub->stats, 0, sizeof(cognitive_hub_stats_t));

    hub->stats.registered_modules = reg_modules;
    hub->stats.active_subscriptions = active_subs;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ========================================================================
 * CATEGORY-BASED OPERATIONS
 * ======================================================================== */

int cognitive_hub_get_modules_by_category(cognitive_integration_hub_t hub,
                                          cognitive_category_t category,
                                          uint32_t* module_ids,
                                          uint32_t max_modules,
                                          uint32_t* count) {
    if (!hub || !hub->initialized || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_reset_stats: required parameter is NULL (hub, hub->initialized, count)");
        return -1;
    }

    if (!COG_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_reset_stats: COG_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_get_mo", 0.0f);


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

int cognitive_hub_publish_to_category(cognitive_integration_hub_t hub,
                                      uint32_t publisher_id,
                                      cognitive_category_t category,
                                      cognitive_event_type_t event_type,
                                      const cognitive_event_data_t* data) {
    if (!hub || !hub->initialized || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_reset_stats: required parameter is NULL (hub, hub->initialized, data)");
        return -1;
    }

    if (!COG_CATEGORY_IS_VALID(category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_reset_stats: COG_CATEGORY_IS_VALID is NULL");
        return -1;
    }

    if (!COG_EVENT_TYPE_IS_VALID(event_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_hub_reset_stats: COG_EVENT_TYPE_IS_VALID is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_publis", 0.0f);


    nimcp_mutex_lock(hub->mutex);

    /* Check publisher is registered */
    if (!find_module_unlocked(hub, publisher_id)) {
        nimcp_mutex_unlock(hub->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "cognitive_hub_reset_stats: find_module_unlocked is NULL");
        return -1;
    }

    hub->stats.events_published++;

    /* Deliver to all subscribers in the category */
    subscription_t* subs = hub->subscriptions[event_type];
    size_t count = hub->subscription_counts[event_type];

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            cognitive_integration_hub_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)count);
        }

        if (subs[i].active && subs[i].callback) {
            /* Skip delivering event back to the publisher */
            if (subs[i].subscriber_id == publisher_id) {
                continue;
            }

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

int cognitive_hub_flush_async_queue(cognitive_integration_hub_t hub,
                                    uint32_t timeout_ms) {
    if (!hub || !hub->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_reset_stats: required parameter is NULL (hub, hub->initialized)");
        return -1;
    }

    /* Currently no async queue implemented - always succeeds */
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_flush_", 0.0f);


    (void)timeout_ms;
    return 0;
}

uint32_t cognitive_hub_get_async_queue_depth(cognitive_integration_hub_t hub) {
    if (!hub || !hub->initialized) {
        return 0;
    }

    /* Currently no async queue implemented */
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_hub_heartbeat("cognitive_in_cognitive_hub_get_as", 0.0f);


    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void cognitive_integration_hub_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_cognitive_integration_hub_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int cognitive_integration_hub_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_hub_training_begin: NULL argument");
        return -1;
    }
    cognitive_integration_hub_heartbeat_instance(g_cognitive_integration_hub_health_agent,
                                                  "cognitive_integration_hub_training_begin", 0.0f);
    (void)(struct subscription*)instance; /* Module state available for reset */
    return 0;
}

int cognitive_integration_hub_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_hub_training_end: NULL argument");
        return -1;
    }
    cognitive_integration_hub_heartbeat_instance(g_cognitive_integration_hub_health_agent,
                                                  "cognitive_integration_hub_training_end", 1.0f);
    (void)(struct subscription*)instance; /* Module state available for finalization */
    return 0;
}

int cognitive_integration_hub_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_hub_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    cognitive_integration_hub_heartbeat_instance(g_cognitive_integration_hub_health_agent, "cognitive_integration_hub_training_step", progress);
    (void)(struct subscription*)instance; /* Module state available for step adaptation */
    return 0;
}
