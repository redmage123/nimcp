/**
 * @file nimcp_rcog_hub_bridge.c
 * @brief Recursive Cognition - Cognitive Integration Hub Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting recursive cognition engine to the cognitive integration hub
 * WHY:  Enables rcog to participate in cross-module cognitive events
 * HOW:  Implements event subscription, publication, and query handling
 *
 * BIOLOGICAL BASIS:
 * - Models prefrontal cortex connections to other brain regions
 * - Goal-directed behavior requires integration with memory and attention
 * - Executive function coordinates with sensory and motor systems
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_rcog_hub_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(rcog_hub_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RCOG_HUB_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define RCOG_HUB_BRIDGE_NAME "RecursiveCognition"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal rcog hub bridge structure
 */
struct rcog_hub_bridge {
    bridge_base_t base;                      /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    rcog_hub_bridge_config_t config;         /**< Bridge configuration */
    cognitive_integration_hub_t hub;          /**< Connected cognitive hub */
    rcog_engine_t* engine;                    /**< Connected rcog engine */

    /* State */
    bool initialized;                         /**< Initialization flag */
    bool connected;                           /**< Connection status */

    /* Callbacks */
    rcog_hub_input_callback_t input_callback;
    void* input_callback_user_data;
    rcog_hub_attention_callback_t attention_callback;
    void* attention_callback_user_data;
    rcog_hub_memory_callback_t memory_callback;
    void* memory_callback_user_data;

    /* Statistics */
    rcog_hub_bridge_stats_t stats;            /**< Bridge statistics */
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

/* ============================================================================
 * Hub Event Callback (internal)
 * ============================================================================ */

/**
 * @brief Internal callback for cognitive hub events
 *
 * WHAT: Handle events from cognitive hub
 * WHY:  Dispatch to appropriate user callbacks
 * HOW:  Parse event type and payload, invoke registered callback
 *
 * @param event Event data from hub
 * @param user_data Bridge pointer
 * @return 0 on success, -1 on error
 */
static int rcog_hub_on_event(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_on_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    rcog_hub_bridge_t* bridge = (rcog_hub_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->stats.events_received++;
    bridge->stats.last_event_timestamp = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Dispatch based on event type */
    switch (event->event_type) {
        case COG_EVENT_INPUT_RECEIVED: {
            nimcp_mutex_lock(bridge->base.mutex);
            rcog_hub_input_callback_t callback = bridge->input_callback;
            void* cb_data = bridge->input_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload) {
                /* Parse input payload - assume text query with priority */
                const char* query = (const char*)event->payload;
                float priority = nimcp_clampf((float)event->priority / 3.0f, 0.0f, 1.0f);

                int result = callback(query, RCOG_GOAL_QUESTION_ANSWERING, priority, cb_data);

                if (result == 0) {
                    nimcp_mutex_lock(bridge->base.mutex);
                    bridge->stats.goals_from_input_events++;
                    bridge->stats.recursions_triggered++;
                    nimcp_mutex_unlock(bridge->base.mutex);
                }
            }
            break;
        }

        case COG_EVENT_ATTENTION_SHIFT: {
            nimcp_mutex_lock(bridge->base.mutex);
            rcog_hub_attention_callback_t callback = bridge->attention_callback;
            void* cb_data = bridge->attention_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload) {
                /* Parse attention payload */
                /* Simplified: assume payload contains two uint64_t IDs */
                const uint64_t* ids = (const uint64_t*)event->payload;
                uint64_t new_focus = ids[0];
                uint64_t old_focus = (event->payload_size >= sizeof(uint64_t) * 2) ? ids[1] : 0;
                float urgency = nimcp_clampf((float)event->priority / 3.0f, 0.0f, 1.0f);

                callback(new_focus, old_focus, urgency, cb_data);

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.priority_changes++;
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        }

        case COG_EVENT_MEMORY_ACCESS: {
            nimcp_mutex_lock(bridge->base.mutex);
            rcog_hub_memory_callback_t callback = bridge->memory_callback;
            void* cb_data = bridge->memory_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload) {
                /* Parse memory access payload */
                /* Simplified: assume payload contains memory_id and access_type */
                const uint64_t* data = (const uint64_t*)event->payload;
                uint64_t memory_id = data[0];
                uint32_t access_type = (event->payload_size >= sizeof(uint64_t) + sizeof(uint32_t))
                    ? ((const uint32_t*)(data + 1))[0] : 0;
                float relevance = 0.5f;  /* Default relevance */

                callback(memory_id, access_type, relevance, cb_data);

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.context_updates++;
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        }

        default:
            /* Unhandled event type - not an error */
            break;
    }

    return 0;
}

/**
 * @brief Internal query handler for rcog state queries
 *
 * WHAT: Handle queries from other modules about rcog state
 * WHY:  Allow inspection of recursive processing state
 * HOW:  Query engine state and return formatted result
 *
 * @param query Incoming query
 * @param result Output result
 * @param context Bridge pointer
 * @return 0 on success, -1 on error
 */
static int rcog_hub_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    if (!query || !result || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_query_handler: required parameter is NULL (query, result, context)");
        return -1;
    }

    rcog_hub_bridge_t* bridge = (rcog_hub_bridge_t*)context;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update query stats */
    bridge->stats.queries_handled++;

    /* Get engine reference */
    rcog_engine_t* engine = bridge->engine;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Initialize result */
    result->status = 0;
    result->result_data = NULL;
    result->result_size = 0;
    result->error_message[0] = '\0';

    /* Handle query based on type */
    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            /* Return engine status */
            if (engine) {
                rcog_engine_state_t state = rcog_engine_get_state(engine);
                const char* state_name = rcog_engine_state_name(state);

                size_t len = strlen(state_name) + 1;
                char* status_str = nimcp_malloc(len);
                if (status_str) {
                    strncpy(status_str, state_name, len);
                    result->result_data = status_str;
                    result->result_size = len;
                }
            } else {
                strncpy(result->error_message, "Engine not connected",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        case COG_QUERY_STATE: {
            /* Return detailed state */
            if (engine) {
                rcog_engine_stats_t stats;
                if (rcog_engine_get_stats(engine, &stats) == 0) {
                    /* Create state summary struct */
                    typedef struct {
                        uint32_t active_goals;
                        uint32_t pending_goals;
                        uint32_t max_depth_reached;
                        float avg_confidence;
                    } rcog_state_summary_t;

                    rcog_state_summary_t* summary = nimcp_malloc(sizeof(rcog_state_summary_t));
                    if (summary) {
                        summary->active_goals = stats.active_goals;
                        summary->pending_goals = stats.pending_goals;
                        summary->max_depth_reached = stats.max_depth_reached;
                        summary->avg_confidence = stats.avg_confidence;

                        result->result_data = summary;
                        result->result_size = sizeof(rcog_state_summary_t);
                    }
                }
            } else {
                strncpy(result->error_message, "Engine not connected",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        case COG_QUERY_METRICS: {
            /* Return performance metrics */
            if (engine) {
                rcog_engine_stats_t stats;
                if (rcog_engine_get_stats(engine, &stats) == 0) {
                    typedef struct {
                        uint64_t goals_completed;
                        uint64_t subtasks_completed;
                        float avg_processing_time_ms;
                        float avg_refinement_steps;
                    } rcog_metrics_t;

                    rcog_metrics_t* metrics = nimcp_malloc(sizeof(rcog_metrics_t));
                    if (metrics) {
                        metrics->goals_completed = stats.goals_completed;
                        metrics->subtasks_completed = stats.subtasks_completed;
                        metrics->avg_processing_time_ms = stats.avg_processing_time_ms;
                        metrics->avg_refinement_steps = stats.avg_refinement_steps;

                        result->result_data = metrics;
                        result->result_size = sizeof(rcog_metrics_t);
                    }
                }
            } else {
                strncpy(result->error_message, "Engine not connected",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        default:
            strncpy(result->error_message, "Unsupported query type",
                    sizeof(result->error_message) - 1);
            result->status = -1;
            break;
    }

    return result->status;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int rcog_hub_bridge_default_config(rcog_hub_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_default_config", 0.0f);


    config->module_id = RCOG_HUB_DEFAULT_MODULE_ID;
    config->auto_subscribe_input = true;
    config->auto_subscribe_memory = true;
    config->auto_subscribe_attention = true;
    config->publish_subtask_events = false;  /* Can be noisy */
    config->enable_query_handler = true;
    config->event_buffer_size = RCOG_HUB_MAX_EVENT_BUFFER;

    return 0;
}

rcog_hub_bridge_t* rcog_hub_bridge_create(
    const rcog_hub_bridge_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_create", 0.0f);


    rcog_hub_bridge_t* bridge = (rcog_hub_bridge_t*)nimcp_calloc(
        1, sizeof(rcog_hub_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        rcog_hub_bridge_default_config(&bridge->config);
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "rcog_hub") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_hub_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->hub = NULL;
    bridge->engine = NULL;
    bridge->connected = false;

    /* Initialize callbacks */
    bridge->input_callback = NULL;
    bridge->input_callback_user_data = NULL;
    bridge->attention_callback = NULL;
    bridge->attention_callback_user_data = NULL;
    bridge->memory_callback = NULL;
    bridge->memory_callback_user_data = NULL;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(rcog_hub_bridge_stats_t));

    bridge->initialized = true;

    return bridge;
}

void rcog_hub_bridge_destroy(rcog_hub_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from hub if connected */
    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_destroy", 0.0f);


    if (bridge->connected) {
        rcog_hub_bridge_disconnect(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int rcog_hub_bridge_connect(
    rcog_hub_bridge_t* bridge,
    cognitive_integration_hub_t hub,
    rcog_engine_t* engine
) {
    if (!bridge || !bridge->initialized || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_connect: required parameter is NULL (bridge, bridge->initialized, hub)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_connect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already connected */
    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_hub_bridge_connect: validation failed");
        return -1;
    }

    /* Store references */
    bridge->hub = hub;
    bridge->engine = engine;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register module with hub */
    int result = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_REASONING,
        RCOG_HUB_BRIDGE_NAME,
        bridge
    );

    if (result != 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->hub = NULL;
        bridge->engine = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_hub_bridge_connect: validation failed");
        return -1;
    }

    /* Subscribe to configured event types */
    if (bridge->config.auto_subscribe_input) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_INPUT_RECEIVED,
            rcog_hub_on_event,
            bridge
        );
        if (result != 0) {
            /* Non-fatal - continue with other subscriptions */
        }
    }

    if (bridge->config.auto_subscribe_memory) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_MEMORY_ACCESS,
            rcog_hub_on_event,
            bridge
        );
        if (result != 0) {
            /* Non-fatal */
        }
    }

    if (bridge->config.auto_subscribe_attention) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_ATTENTION_SHIFT,
            rcog_hub_on_event,
            bridge
        );
        if (result != 0) {
            /* Non-fatal */
        }
    }

    /* Register query handler if enabled */
    if (bridge->config.enable_query_handler) {
        result = cognitive_hub_register_query_handler(
            hub,
            bridge->config.module_id,
            rcog_hub_query_handler
        );
        if (result != 0) {
            /* Non-fatal */
        }
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_hub_bridge_disconnect(rcog_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_disconnect: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_disconnect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_disconnect: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unsubscribe from events */
    if (bridge->config.auto_subscribe_input) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_INPUT_RECEIVED);
    }
    if (bridge->config.auto_subscribe_memory) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_MEMORY_ACCESS);
    }
    if (bridge->config.auto_subscribe_attention) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_ATTENTION_SHIFT);
    }

    /* Unregister module from hub */
    cognitive_hub_unregister_module(hub, module_id);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub = NULL;
    bridge->engine = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_hub_bridge_set_engine(
    rcog_hub_bridge_t* bridge,
    rcog_engine_t* engine
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_set_engine: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_set_engine", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->engine = engine;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool rcog_hub_bridge_is_connected(rcog_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Cast away const for mutex lock (safe - only reading) */
    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_is_connected", 0.0f);


    rcog_hub_bridge_t* mutable_bridge = (rcog_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool connected = bridge->connected;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Event Callback Registration Implementation
 * ============================================================================ */

int rcog_hub_bridge_set_input_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_input_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_set_input_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_set_input_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->input_callback = callback;
    bridge->input_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_hub_bridge_set_attention_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_attention_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_set_attention_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_set_attention_callba", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_callback = callback;
    bridge->attention_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_hub_bridge_set_memory_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_memory_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_set_memory_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_set_memory_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->memory_callback = callback;
    bridge->memory_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Event Publication Implementation
 * ============================================================================ */

int rcog_hub_publish_recursion_start(
    rcog_hub_bridge_t* bridge,
    uint64_t goal_id,
    uint32_t goal_type,
    uint32_t max_depth,
    float priority
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_rcog_hub_publish_rec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_start: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    rcog_hub_recursion_start_payload_t payload;
    payload.goal_id = goal_id;
    payload.goal_type = goal_type;
    payload.max_depth = max_depth;
    payload.priority = nimcp_clampf(priority, 0.0f, 1.0f);
    payload.timestamp = get_timestamp_ms();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;  /* Use state change for recursion lifecycle */
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;  /* Convert to microseconds */
    event.priority = (priority > 0.66f) ? COG_PRIORITY_HIGH :
                     (priority > 0.33f) ? COG_PRIORITY_NORMAL : COG_PRIORITY_LOW;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.recursions_triggered++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int rcog_hub_publish_recursion_complete(
    rcog_hub_bridge_t* bridge,
    uint64_t goal_id,
    bool success,
    float final_confidence,
    uint32_t subtasks_total,
    uint32_t subtasks_completed,
    uint32_t max_depth_reached,
    uint64_t processing_time_ms
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_complete: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_rcog_hub_publish_rec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_complete: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    rcog_hub_recursion_complete_payload_t payload;
    payload.goal_id = goal_id;
    payload.success = success;
    payload.final_confidence = nimcp_clampf(final_confidence, 0.0f, 1.0f);
    payload.subtasks_total = subtasks_total;
    payload.subtasks_completed = subtasks_completed;
    payload.max_depth_reached = max_depth_reached;
    payload.processing_time_ms = processing_time_ms;
    payload.timestamp = get_timestamp_ms();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_OUTPUT_READY;  /* Output ready when recursion completes */
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;
    event.priority = success ? COG_PRIORITY_NORMAL : COG_PRIORITY_HIGH;  /* Failures are higher priority */
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_OUTPUT_READY, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int rcog_hub_publish_subtask_spawned(
    rcog_hub_bridge_t* bridge,
    uint64_t parent_goal_id,
    uint64_t subtask_id,
    uint32_t current_depth,
    uint32_t subtask_type,
    float priority
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_subtask_spawned: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_rcog_hub_publish_sub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if subtask events are enabled */
    if (!bridge->config.publish_subtask_events) {
        bridge->stats.subtasks_spawned++;  /* Still track locally */
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Success - just not published */
    }

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_subtask_spawned: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    rcog_hub_subtask_spawned_payload_t payload;
    payload.parent_goal_id = parent_goal_id;
    payload.subtask_id = subtask_id;
    payload.current_depth = current_depth;
    payload.subtask_type = subtask_type;
    payload.priority = nimcp_clampf(priority, 0.0f, 1.0f);
    payload.timestamp = get_timestamp_ms();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;
    event.priority = COG_PRIORITY_LOW;  /* Subtask events are lower priority */
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub (async to avoid blocking) */
    int result = cognitive_hub_publish_async(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.subtasks_spawned++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int rcog_hub_publish_recursion_event(
    rcog_hub_bridge_t* bridge,
    rcog_hub_event_type_t event_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_event: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_rcog_hub_publish_rec", 0.0f);


    if (event_type >= RCOG_HUB_EVENT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rcog_hub_publish_recursion_event: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_publish_recursion_event: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Map rcog event type to cognitive event type */
    cognitive_event_type_t cog_event_type;
    switch (event_type) {
        case RCOG_HUB_EVENT_RECURSION_START:
        case RCOG_HUB_EVENT_SUBTASK_SPAWNED:
        case RCOG_HUB_EVENT_DEPTH_INCREASED:
        case RCOG_HUB_EVENT_STRATEGY_CHANGED:
            cog_event_type = COG_EVENT_STATE_CHANGE;
            break;

        case RCOG_HUB_EVENT_RECURSION_COMPLETE:
        case RCOG_HUB_EVENT_SUBTASK_COMPLETE:
        case RCOG_HUB_EVENT_ANSWER_REFINED:
            cog_event_type = COG_EVENT_OUTPUT_READY;
            break;

        case RCOG_HUB_EVENT_CONFIDENCE_CHANGED:
            cog_event_type = COG_EVENT_LEARNING_COMPLETE;
            break;

        default:
            cog_event_type = COG_EVENT_STATE_CHANGE;
            break;
    }

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = cog_event_type;
    event.source_module_id = module_id;
    event.timestamp = get_timestamp_ms() * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)payload;  /* Hub will copy if async */
    event.payload_size = payload_size;

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, cog_event_type, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int rcog_hub_bridge_get_state(
    const rcog_hub_bridge_t* bridge,
    uint32_t* active_goals,
    uint32_t* current_depth,
    float* avg_confidence
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_get_state: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_get_state", 0.0f);


    rcog_hub_bridge_t* mutable_bridge = (rcog_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    rcog_engine_t* engine = bridge->engine;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    if (!engine) {
        /* No engine connected - return zeros */
        if (active_goals) *active_goals = 0;
        if (current_depth) *current_depth = 0;
        if (avg_confidence) *avg_confidence = 0.0f;
        return 0;
    }

    /* Get stats from engine */
    rcog_engine_stats_t stats;
    if (rcog_engine_get_stats(engine, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_hub_bridge_get_state: validation failed");
        return -1;
    }

    if (active_goals) *active_goals = stats.active_goals;
    if (current_depth) *current_depth = stats.max_depth_reached;
    if (avg_confidence) *avg_confidence = stats.avg_confidence;

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int rcog_hub_bridge_get_stats(
    const rcog_hub_bridge_t* bridge,
    rcog_hub_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_get_stats", 0.0f);


    rcog_hub_bridge_t* mutable_bridge = (rcog_hub_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int rcog_hub_bridge_reset_stats(rcog_hub_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_hub_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_hub_bridge_heartbeat("rcog_hub_bri_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_hub_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}


void rcog_hub_bridge_set_instance_health_agent(rcog_hub_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "rcog_hub_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int rcog_hub_bridge_training_begin(rcog_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_hub_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_hub_bridge_heartbeat_instance(bridge->health_agent, "rcog_hub_bridge_training_begin", 0.0f);
    return 0;
}

int rcog_hub_bridge_training_end(rcog_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_hub_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_hub_bridge_heartbeat_instance(bridge->health_agent, "rcog_hub_bridge_training_end", 1.0f);
    return 0;
}

int rcog_hub_bridge_training_step(rcog_hub_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_hub_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    rcog_hub_bridge_heartbeat_instance(bridge->health_agent, "rcog_hub_bridge_training_step", progress);
    return 0;
}
