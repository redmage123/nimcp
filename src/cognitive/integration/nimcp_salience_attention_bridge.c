/**
 * @file nimcp_salience_attention_bridge.c
 * @brief Salience-Attention Cognitive Integration Hub Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting salience detection to attention allocation
 * WHY:  Enable bidirectional communication between salience and attention
 * HOW:  Implements event subscription, publication, and query handling
 *
 * BIOLOGICAL BASIS:
 * - Models bottom-up attention capture by salient stimuli (superior colliculus)
 * - Models top-down attention modulation of salience sensitivity (prefrontal cortex)
 * - Priority maps integrate bottom-up salience with top-down goals (parietal cortex)
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_salience_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/salience/nimcp_salience.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(salience_attention_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_salience_attention_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_salience_attention_bridge_mesh_registry = NULL;

nimcp_error_t salience_attention_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_salience_attention_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "salience_attention_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "salience_attention_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_salience_attention_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_salience_attention_bridge_mesh_registry = registry;
    return err;
}

void salience_attention_bridge_mesh_unregister(void) {
    if (g_salience_attention_bridge_mesh_registry && g_salience_attention_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_salience_attention_bridge_mesh_registry, g_salience_attention_bridge_mesh_id);
        g_salience_attention_bridge_mesh_id = 0;
        g_salience_attention_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from salience_attention_bridge module (instance-level) */
static inline void salience_attention_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_salience_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_salience_attention_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_salience_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SALIENCE_ATTENTION_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define SALIENCE_ATTENTION_BRIDGE_NAME "SalienceAttention"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal salience-attention bridge structure
 */
struct salience_attention_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    salience_attention_config_t config;      /**< Bridge configuration */
    cognitive_integration_hub_t hub;          /**< Connected cognitive hub */
    salience_evaluator_t salience;            /**< Connected salience evaluator */
    multihead_attention_t attention;          /**< Connected attention module */

    /* State */
    bool initialized;                         /**< Initialization flag */
    bool registered;                          /**< Hub registration status */

    /* Callbacks */
    sa_salience_callback_t salience_callback;
    void* salience_callback_user_data;
    sa_attention_callback_t attention_callback;
    void* attention_callback_user_data;
    sa_priority_callback_t priority_callback;
    void* priority_callback_user_data;
    sa_eval_request_callback_t eval_callback;
    void* eval_callback_user_data;

    /* Cached state */
    uint64_t current_focus_id;                /**< Current attention focus */
    float current_focus_strength;             /**< Current focus strength */
    float peak_salience;                      /**< Peak salience observed */
    uint32_t detection_count;                 /**< Salience detections */

    /* Statistics */
    salience_attention_stats_t stats;         /**< Bridge statistics */
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
static int salience_attention_on_event(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_on_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    salience_attention_bridge_t* bridge = (salience_attention_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->stats.events_received++;
    bridge->stats.total_events++;
    bridge->stats.last_event_timestamp = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Dispatch based on event type */
    switch (event->event_type) {
        case COG_EVENT_ATTENTION_SHIFT: {
            /* Handle attention shift events */
            nimcp_mutex_lock(bridge->base.mutex);
            sa_attention_callback_t callback = bridge->attention_callback;
            void* cb_data = bridge->attention_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(attention_focus_t)) {
                const attention_focus_t* focus = (const attention_focus_t*)event->payload;

                /* Update cached state */
                nimcp_mutex_lock(bridge->base.mutex);
                bridge->current_focus_id = focus->focus_id;
                bridge->current_focus_strength = focus->focus_strength;
                bridge->stats.attention_shifts++;
                nimcp_mutex_unlock(bridge->base.mutex);

                callback(focus, cb_data);
            }
            break;
        }

        case COG_EVENT_STATE_CHANGE: {
            /* Handle state change events - could be salience updates */
            nimcp_mutex_lock(bridge->base.mutex);
            sa_salience_callback_t callback = bridge->salience_callback;
            void* cb_data = bridge->salience_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(salient_item_t)) {
                const salient_item_t* item = (const salient_item_t*)event->payload;

                /* Check if this is from salience module (check source) */
                /* Update cached state */
                nimcp_mutex_lock(bridge->base.mutex);
                if (item->salience_score > bridge->peak_salience) {
                    bridge->peak_salience = item->salience_score;
                }
                bridge->detection_count++;
                bridge->stats.salience_detections++;

                /* Update average */
                float total = bridge->stats.avg_salience_score * (bridge->stats.salience_detections - 1);
                bridge->stats.avg_salience_score = (total + item->salience_score) / bridge->stats.salience_detections;
                nimcp_mutex_unlock(bridge->base.mutex);

                int result = callback(item, cb_data);

                if (result == 0) {
                    nimcp_mutex_lock(bridge->base.mutex);
                    bridge->stats.salience_detections++;
                    nimcp_mutex_unlock(bridge->base.mutex);
                }
            }
            break;
        }

        case COG_EVENT_DECISION_MADE: {
            /* Handle priority update events (mapped to decision) */
            nimcp_mutex_lock(bridge->base.mutex);
            sa_priority_callback_t callback = bridge->priority_callback;
            void* cb_data = bridge->priority_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(priority_update_payload_t)) {
                const priority_update_payload_t* update = (const priority_update_payload_t*)event->payload;

                callback(update->priorities, update->num_priorities, cb_data);

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.priority_updates++;
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        }

        case COG_EVENT_INPUT_RECEIVED: {
            /* Handle evaluation request events */
            nimcp_mutex_lock(bridge->base.mutex);
            sa_eval_request_callback_t callback = bridge->eval_callback;
            void* cb_data = bridge->eval_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(salience_eval_request_t)) {
                const salience_eval_request_t* request = (const salience_eval_request_t*)event->payload;

                callback(request, cb_data);

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.evaluation_requests++;
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
 * @brief Internal query handler for salience-attention state queries
 *
 * @param query Incoming query
 * @param result Output result
 * @param context Bridge pointer
 * @return 0 on success, -1 on error
 */
static int salience_attention_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    if (!query || !result || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_query_handler: required parameter is NULL (query, result, context)");
        return -1;
    }

    salience_attention_bridge_t* bridge = (salience_attention_bridge_t*)context;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update query stats */
    bridge->stats.queries_handled++;

    /* Get module references */
    salience_evaluator_t salience = bridge->salience;
    multihead_attention_t attention = bridge->attention;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Initialize result */
    result->status = 0;
    result->result_data = NULL;
    result->result_size = 0;
    result->error_message[0] = '\0';

    /* Handle query based on type */
    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            /* Return bridge status */
            const char* status = bridge->registered ? "registered" : "not_registered";
            size_t len = strlen(status) + 1;
            char* status_str = nimcp_malloc(len);
            if (status_str) {
                strncpy(status_str, status, len);
                result->result_data = status_str;
                result->result_size = len;
            }
            break;
        }

        case COG_QUERY_STATE: {
            /* Return detailed state */
            typedef struct {
                uint64_t current_focus;
                float focus_strength;
                float peak_salience;
                uint32_t detection_count;
            } sa_state_summary_t;

            sa_state_summary_t* summary = nimcp_malloc(sizeof(sa_state_summary_t));
            if (summary) {
                nimcp_mutex_lock(bridge->base.mutex);
                summary->current_focus = bridge->current_focus_id;
                summary->focus_strength = bridge->current_focus_strength;
                summary->peak_salience = bridge->peak_salience;
                summary->detection_count = bridge->detection_count;
                nimcp_mutex_unlock(bridge->base.mutex);

                result->result_data = summary;
                result->result_size = sizeof(sa_state_summary_t);
            }
            break;
        }

        case COG_QUERY_ATTENTION: {
            /* Return attention state */
            if (attention) {
                attention_stats_t att_stats;
                if (multihead_attention_get_stats(attention, &att_stats)) {
                    typedef struct {
                        uint64_t forward_calls;
                        float avg_attention_entropy;
                        float avg_gate_activation;
                        uint32_t active_heads;
                    } attention_summary_t;

                    attention_summary_t* att_summary = nimcp_malloc(sizeof(attention_summary_t));
                    if (att_summary) {
                        att_summary->forward_calls = att_stats.forward_calls;
                        att_summary->avg_attention_entropy = att_stats.avg_attention_entropy;
                        att_summary->avg_gate_activation = att_stats.avg_gate_activation;
                        att_summary->active_heads = att_stats.active_heads;

                        result->result_data = att_summary;
                        result->result_size = sizeof(attention_summary_t);
                    }
                }
            } else {
                strncpy(result->error_message, "Attention module not connected",
                        sizeof(result->error_message) - 1);
                result->status = -1;
            }
            break;
        }

        case COG_QUERY_METRICS: {
            /* Return performance metrics */
            salience_attention_stats_t* metrics = nimcp_malloc(sizeof(salience_attention_stats_t));
            if (metrics) {
                nimcp_mutex_lock(bridge->base.mutex);
                *metrics = bridge->stats;
                nimcp_mutex_unlock(bridge->base.mutex);

                result->result_data = metrics;
                result->result_size = sizeof(salience_attention_stats_t);
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

int salience_attention_bridge_default_config(salience_attention_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_default_config", 0.0f);


    config->module_id = SALIENCE_ATTENTION_DEFAULT_MODULE_ID;
    config->enable_logging = false;
    config->salience_threshold = SALIENCE_ATTENTION_DEFAULT_THRESHOLD;
    config->attention_shift_weight = 0.8f;
    config->priority_weight = 0.5f;
    config->auto_subscribe_attention = true;
    config->auto_subscribe_state = true;
    config->enable_query_handler = true;

    return 0;
}

salience_attention_bridge_t* salience_attention_bridge_create(
    const salience_attention_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_create", 0.0f);


    salience_attention_bridge_t* bridge = (salience_attention_bridge_t*)nimcp_calloc(
        1, sizeof(salience_attention_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        salience_attention_bridge_default_config(&bridge->config);
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "salience_attention") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "salience_attention_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->hub = NULL;
    bridge->salience = NULL;
    bridge->attention = NULL;
    bridge->registered = false;

    /* Initialize callbacks */
    bridge->salience_callback = NULL;
    bridge->salience_callback_user_data = NULL;
    bridge->attention_callback = NULL;
    bridge->attention_callback_user_data = NULL;
    bridge->priority_callback = NULL;
    bridge->priority_callback_user_data = NULL;
    bridge->eval_callback = NULL;
    bridge->eval_callback_user_data = NULL;

    /* Initialize cached state */
    bridge->current_focus_id = 0;
    bridge->current_focus_strength = 0.0f;
    bridge->peak_salience = 0.0f;
    bridge->detection_count = 0;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(salience_attention_stats_t));

    bridge->initialized = true;

    return bridge;
}

void salience_attention_bridge_destroy(salience_attention_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Unregister from hub if registered */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_destroy", 0.0f);


    if (bridge->registered) {
        salience_attention_bridge_unregister_from_hub(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Hub Registration Implementation
 * ============================================================================ */

int salience_attention_bridge_register_with_hub(
    salience_attention_bridge_t* bridge,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !bridge->initialized || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_register_with_hub: required parameter is NULL (bridge, bridge->initialized, hub)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_register_with_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_attention_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Store hub reference */
    bridge->hub = hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register module with hub */
    int result = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_PERCEPTION,  /* Salience is perception-related */
        SALIENCE_ATTENTION_BRIDGE_NAME,
        bridge
    );

    if (result != 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->hub = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_attention_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to configured event types */
    if (bridge->config.auto_subscribe_attention) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_ATTENTION_SHIFT,
            salience_attention_on_event,
            bridge
        );
        /* Non-fatal if subscription fails */
    }

    if (bridge->config.auto_subscribe_state) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_STATE_CHANGE,
            salience_attention_on_event,
            bridge
        );
        /* Non-fatal */
    }

    /* Also subscribe to decision events for priority updates */
    cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_DECISION_MADE,
        salience_attention_on_event,
        bridge
    );

    /* Subscribe to input events for evaluation requests */
    cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_INPUT_RECEIVED,
        salience_attention_on_event,
        bridge
    );

    /* Register query handler if enabled */
    if (bridge->config.enable_query_handler) {
        result = cognitive_hub_register_query_handler(
            hub,
            bridge->config.module_id,
            salience_attention_query_handler
        );
        /* Non-fatal */
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->registered = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_unregister_from_hub(
    salience_attention_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_unregister_from_hub: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_unregister_from_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_unregister_from_hub: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unsubscribe from events */
    if (bridge->config.auto_subscribe_attention) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_ATTENTION_SHIFT);
    }
    if (bridge->config.auto_subscribe_state) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_STATE_CHANGE);
    }
    cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_DECISION_MADE);
    cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_INPUT_RECEIVED);

    /* Unregister module from hub */
    cognitive_hub_unregister_module(hub, module_id);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub = NULL;
    bridge->registered = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool salience_attention_bridge_is_registered(
    const salience_attention_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_is_registered", 0.0f);


    salience_attention_bridge_t* mutable_bridge = (salience_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return registered;
}

/* ============================================================================
 * Module Connection Implementation
 * ============================================================================ */

int salience_attention_bridge_set_salience(
    salience_attention_bridge_t* bridge,
    salience_evaluator_t evaluator
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_salience: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_salience", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->salience = evaluator;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_set_attention(
    salience_attention_bridge_t* bridge,
    multihead_attention_t attention
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_attention: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention = attention;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Event Callback Registration Implementation
 * ============================================================================ */

int salience_attention_bridge_set_salience_callback(
    salience_attention_bridge_t* bridge,
    sa_salience_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_salience_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_salience_callbac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->salience_callback = callback;
    bridge->salience_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_set_attention_callback(
    salience_attention_bridge_t* bridge,
    sa_attention_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_attention_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_attention_callba", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_callback = callback;
    bridge->attention_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_set_priority_callback(
    salience_attention_bridge_t* bridge,
    sa_priority_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_priority_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_priority_callbac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->priority_callback = callback;
    bridge->priority_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_set_eval_callback(
    salience_attention_bridge_t* bridge,
    sa_eval_request_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_eval_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_eval_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->eval_callback = callback;
    bridge->eval_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Event Publication Implementation
 * ============================================================================ */

int salience_attention_publish_salience_detection(
    salience_attention_bridge_t* bridge,
    const salient_item_t* item,
    float score
) {
    if (!bridge || !bridge->initialized || !item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_publish_salience_detection: required parameter is NULL (bridge, bridge->initialized, item)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_salience_attention_p", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_publish_salience_detection: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;
    float threshold = bridge->config.salience_threshold;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    salience_detection_payload_t payload;
    payload.item = *item;
    payload.item.salience_score = nimcp_clampf(score, 0.0f, 1.0f);
    payload.captured_attention = (score >= threshold);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;  /* Salience is a state change */
    event.source_module_id = module_id;
    event.timestamp = get_timestamp_ms() * 1000;  /* Convert to microseconds */
    event.priority = (score > 0.8f) ? COG_PRIORITY_HIGH :
                     (score > 0.5f) ? COG_PRIORITY_NORMAL : COG_PRIORITY_LOW;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.salience_detections++;

        /* Update peak salience */
        if (score > bridge->peak_salience) {
            bridge->peak_salience = score;
        }
        bridge->detection_count++;

        /* Update average */
        uint64_t count = bridge->stats.salience_detections;
        float total = bridge->stats.avg_salience_score * (count - 1);
        bridge->stats.avg_salience_score = (total + score) / (count > 0 ? count : 1);

        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int salience_attention_request_attention_shift(
    salience_attention_bridge_t* bridge,
    const attention_target_t* target
) {
    if (!bridge || !bridge->initialized || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_request_attention_shift: required parameter is NULL (bridge, bridge->initialized, target)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_salience_attention_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_request_attention_shift: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;
    float shift_weight = bridge->config.attention_shift_weight;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    attention_shift_payload_t payload;
    payload.target = *target;
    payload.shift_weight = nimcp_clampf(shift_weight, 0.0f, 1.0f);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_ATTENTION_SHIFT;
    event.source_module_id = module_id;
    event.timestamp = get_timestamp_ms() * 1000;
    event.priority = (target->urgency > 0.8f) ? COG_PRIORITY_HIGH :
                     (target->urgency > 0.5f) ? COG_PRIORITY_NORMAL : COG_PRIORITY_LOW;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_ATTENTION_SHIFT, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.attention_shifts++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int salience_attention_publish_priority_update(
    salience_attention_bridge_t* bridge,
    const attention_priority_t* priorities,
    uint32_t num_priorities
) {
    if (!bridge || !bridge->initialized || !priorities || num_priorities == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_publish_priority_update: required parameter is NULL (bridge, bridge->initialized, priorities)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_salience_attention_p", 0.0f);


    if (num_priorities > SALIENCE_ATTENTION_MAX_PRIORITIES) {
        num_priorities = SALIENCE_ATTENTION_MAX_PRIORITIES;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_publish_priority_update: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create payload */
    priority_update_payload_t payload;
    memcpy(payload.priorities, priorities, num_priorities * sizeof(attention_priority_t));
    payload.num_priorities = num_priorities;
    payload.timestamp = get_timestamp_ms();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_DECISION_MADE;  /* Priority updates are decisions */
    event.source_module_id = module_id;
    event.timestamp = payload.timestamp * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_DECISION_MADE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.priority_updates++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int salience_attention_notify_attention_focus(
    salience_attention_bridge_t* bridge,
    const attention_focus_t* focus
) {
    if (!bridge || !bridge->initialized || !focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_notify_attention_focus: required parameter is NULL (bridge, bridge->initialized, focus)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_salience_attention_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_notify_attention_focus: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Update cached state */
    bridge->current_focus_id = focus->focus_id;
    bridge->current_focus_strength = focus->focus_strength;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_ATTENTION_SHIFT;
    event.source_module_id = module_id;
    event.timestamp = get_timestamp_ms() * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)focus;
    event.payload_size = sizeof(attention_focus_t);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_ATTENTION_SHIFT, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.focus_notifications++;

        /* Update average attention strength */
        uint64_t count = bridge->stats.focus_notifications;
        float total = bridge->stats.avg_attention_strength * (count - 1);
        bridge->stats.avg_attention_strength = (total + focus->focus_strength) / (count > 0 ? count : 1);

        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int salience_attention_request_salience_evaluation(
    salience_attention_bridge_t* bridge,
    const salience_eval_request_t* request
) {
    if (!bridge || !bridge->initialized || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_request_salience_evaluation: required parameter is NULL (bridge, bridge->initialized, request)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_salience_attention_r", 0.0f);


    if (request->num_items == 0 || request->num_items > SALIENCE_ATTENTION_MAX_ITEMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_attention_request_salience_evaluation: request->num_items is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_request_salience_evaluation: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_INPUT_RECEIVED;  /* Evaluation request is input */
    event.source_module_id = module_id;
    event.timestamp = get_timestamp_ms() * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)request;
    event.payload_size = sizeof(salience_eval_request_t);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, module_id, COG_EVENT_INPUT_RECEIVED, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.evaluation_requests++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int salience_attention_bridge_get_salience_state(
    const salience_attention_bridge_t* bridge,
    float* avg_salience,
    float* peak_salience,
    uint32_t* detection_count
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_get_salience_state: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_get_salience_state", 0.0f);


    salience_attention_bridge_t* mutable_bridge = (salience_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    if (avg_salience) *avg_salience = bridge->stats.avg_salience_score;
    if (peak_salience) *peak_salience = bridge->peak_salience;
    if (detection_count) *detection_count = bridge->detection_count;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_get_attention_state(
    const salience_attention_bridge_t* bridge,
    uint64_t* current_focus,
    float* focus_strength,
    uint32_t* num_targets
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_get_attention_state: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_get_attention_state", 0.0f);


    salience_attention_bridge_t* mutable_bridge = (salience_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    if (current_focus) *current_focus = bridge->current_focus_id;
    if (focus_strength) *focus_strength = bridge->current_focus_strength;
    if (num_targets) *num_targets = (uint32_t)bridge->stats.attention_shifts;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int salience_attention_bridge_get_stats(
    const salience_attention_bridge_t* bridge,
    salience_attention_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_get_stats", 0.0f);


    salience_attention_bridge_t* mutable_bridge = (salience_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_reset_stats(salience_attention_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(salience_attention_stats_t));
    bridge->peak_salience = 0.0f;
    bridge->detection_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int salience_attention_bridge_set_threshold(
    salience_attention_bridge_t* bridge,
    float threshold
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_threshold: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_threshold", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.salience_threshold = nimcp_clampf(threshold, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int salience_attention_bridge_set_shift_weight(
    salience_attention_bridge_t* bridge,
    float weight
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_attention_bridge_set_shift_weight: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_bridge_heartbeat("salience_att_set_shift_weight", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.attention_shift_weight = nimcp_clampf(weight, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_attention_bridge_set_instance_health_agent(salience_attention_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "salience_attention_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_attention_bridge_training_begin(salience_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_attention_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_bridge_training_begin", 0.0f);
    return 0;
}

int salience_attention_bridge_training_end(salience_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_bridge_training_end: NULL argument");
        return -1;
    }
    salience_attention_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_bridge_training_end", 1.0f);
    return 0;
}

int salience_attention_bridge_training_step(salience_attention_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_attention_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_bridge_training_step", progress);
    return 0;
}
