/**
 * @file nimcp_predictive_attention_bridge.c
 * @brief Predictive Processing - Attention Cognitive Hub Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting predictive processing to attention systems via the
 *       cognitive integration hub, enabling bidirectional communication.
 *
 * WHY: Predictive processing and attention are tightly coupled:
 *      - Prediction errors signal where attention is needed
 *      - Precision weighting determines attention allocation
 *      - Surprise/unexpected events automatically capture attention
 *      - Active inference: attention samples to minimize prediction error
 *
 * HOW: Implements event subscription, publication, and query handling for
 *      bidirectional predictive-attention integration.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_predictive_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_attention_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_predictive_attention_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_predictive_attention_bridge_mesh_registry = NULL;

nimcp_error_t predictive_attention_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_predictive_attention_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "predictive_attention_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "predictive_attention_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_predictive_attention_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_predictive_attention_bridge_mesh_registry = registry;
    return err;
}

void predictive_attention_bridge_mesh_unregister(void) {
    if (g_predictive_attention_bridge_mesh_registry && g_predictive_attention_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_predictive_attention_bridge_mesh_registry, g_predictive_attention_bridge_mesh_id);
        g_predictive_attention_bridge_mesh_id = 0;
        g_predictive_attention_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from predictive_attention_bridge module (instance-level) */
static inline void predictive_attention_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_predictive_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_predictive_attention_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_predictive_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "PREDICTIVE_ATTENTION_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define PRED_ATTN_BRIDGE_NAME "PredictiveAttention"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal predictive-attention bridge structure
 */
struct predictive_attention_bridge {
    bridge_base_t base;                            /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    predictive_attention_bridge_config_t config;  /**< Bridge configuration */
    cognitive_integration_hub_t hub;               /**< Connected cognitive hub */

    /* State */
    bool initialized;                              /**< Initialization flag */
    bool connected;                                /**< Connection status */

    /* Callbacks */
    pred_attn_prediction_callback_t prediction_callback;
    void* prediction_callback_user_data;
    pred_attn_attention_callback_t attention_callback;
    void* attention_callback_user_data;
    pred_attn_error_callback_t error_callback;
    void* error_callback_user_data;

    /* Statistics */
    predictive_attention_bridge_stats_t stats;    /**< Bridge statistics */

    /* Running averages for statistics */
    float error_sum;                               /**< Sum of error magnitudes */
    uint32_t error_count;                          /**< Count for average */
    float precision_sum;                           /**< Sum of precision values */
    uint32_t precision_count;                      /**< Count for average */
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    }
    return 0;
}

/**
 * @brief Update running average
 */
static void update_running_average(float* sum, uint32_t* count, float* avg, float new_value) {
    *sum += new_value;
    (*count)++;
    if (*count > 0) {
        *avg = *sum / (float)(*count);
    }
}

/* ============================================================================
 * Hub Event Callback (internal)
 * ============================================================================ */

/**
 * @brief Internal callback for cognitive hub events
 *
 * WHAT: Handle events from cognitive hub
 * WHY: Dispatch to appropriate user callbacks and update state
 * HOW: Parse event type and payload, invoke registered callback
 *
 * @param event Event data from hub
 * @param user_data Bridge pointer
 * @return 0 on success, -1 on error
 */
static int pred_attn_hub_on_event(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pred_attn_hub_on_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    predictive_attention_bridge_t* bridge = (predictive_attention_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->stats.events_received++;
    bridge->stats.total_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Dispatch based on event type */
    switch (event->event_type) {
        case COG_EVENT_INPUT_RECEIVED: {
            /* Handle prediction input events */
            nimcp_mutex_lock(bridge->base.mutex);
            pred_attn_prediction_callback_t callback = bridge->prediction_callback;
            void* cb_data = bridge->prediction_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(float) * 2) {
                /* Parse prediction payload: [prediction, confidence, location] */
                const float* data = (const float*)event->payload;
                float prediction = data[0];
                float confidence = nimcp_clampf(data[1], 0.0f, 1.0f);
                uint64_t location = 0;
                if (event->payload_size >= sizeof(float) * 2 + sizeof(uint64_t)) {
                    location = *(const uint64_t*)(data + 2);
                }

                callback(prediction, location, confidence, cb_data);

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.focus_predictions++;
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        }

        case COG_EVENT_ATTENTION_SHIFT: {
            /* Handle attention shift events */
            nimcp_mutex_lock(bridge->base.mutex);
            pred_attn_attention_callback_t callback = bridge->attention_callback;
            void* cb_data = bridge->attention_callback_user_data;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(uint64_t) * 2) {
                /* Parse attention shift payload */
                const uint64_t* ids = (const uint64_t*)event->payload;
                uint64_t new_focus = ids[0];
                uint64_t old_focus = ids[1];
                float urgency = nimcp_clampf((float)event->priority / 3.0f, 0.0f, 1.0f);

                int result = callback(old_focus, new_focus, urgency, cb_data);

                if (result == 0) {
                    nimcp_mutex_lock(bridge->base.mutex);
                    bridge->stats.surprise_shifts++;
                    nimcp_mutex_unlock(bridge->base.mutex);
                }
            }
            break;
        }

        case COG_EVENT_STATE_CHANGE: {
            /* Handle state change events (may contain errors or precision updates) */
            nimcp_mutex_lock(bridge->base.mutex);
            pred_attn_error_callback_t callback = bridge->error_callback;
            void* cb_data = bridge->error_callback_user_data;
            float threshold = bridge->config.error_attention_threshold;
            nimcp_mutex_unlock(bridge->base.mutex);

            if (callback && event->payload && event->payload_size >= sizeof(float) + sizeof(uint64_t)) {
                /* Parse error payload */
                const float* error_mag = (const float*)event->payload;
                const uint64_t* location = (const uint64_t*)(error_mag + 1);

                if (*error_mag >= threshold) {
                    callback(*error_mag, *location, cb_data);

                    nimcp_mutex_lock(bridge->base.mutex);
                    bridge->stats.prediction_errors++;
                    update_running_average(&bridge->error_sum, &bridge->error_count,
                                         &bridge->stats.avg_error_magnitude, *error_mag);
                    nimcp_mutex_unlock(bridge->base.mutex);
                }
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
 * @brief Internal query handler for bridge state queries
 *
 * WHAT: Handle queries from other modules about bridge state
 * WHY: Allow inspection of predictive-attention integration state
 * HOW: Query internal state and return formatted result
 *
 * @param query Incoming query
 * @param result Output result
 * @param context Bridge pointer
 * @return 0 on success, -1 on error
 */
static int pred_attn_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    if (!query || !result || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pred_attn_query_handler: required parameter is NULL (query, result, context)");
        return -1;
    }

    predictive_attention_bridge_t* bridge = (predictive_attention_bridge_t*)context;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update query stats */
    bridge->stats.queries_handled++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Initialize result */
    result->status = 0;
    result->result_data = NULL;
    result->result_size = 0;
    result->error_message[0] = '\0';

    /* Handle query based on type */
    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            /* Return connection status */
            nimcp_mutex_lock(bridge->base.mutex);
            bool connected = bridge->connected;
            nimcp_mutex_unlock(bridge->base.mutex);

            const char* status_str = connected ? "connected" : "disconnected";
            size_t len = strlen(status_str) + 1;
            char* status_copy = nimcp_malloc(len);
            if (status_copy) {
                strncpy(status_copy, status_str, len);
                result->result_data = status_copy;
                result->result_size = len;
            }
            break;
        }

        case COG_QUERY_STATE: {
            /* Return detailed state */
            predictive_attention_bridge_stats_t* stats_copy =
                nimcp_malloc(sizeof(predictive_attention_bridge_stats_t));
            if (stats_copy) {
                nimcp_mutex_lock(bridge->base.mutex);
                *stats_copy = bridge->stats;
                nimcp_mutex_unlock(bridge->base.mutex);

                result->result_data = stats_copy;
                result->result_size = sizeof(predictive_attention_bridge_stats_t);
            }
            break;
        }

        case COG_QUERY_METRICS: {
            /* Return key metrics */
            typedef struct {
                float avg_error;
                float avg_precision;
                uint32_t total_events;
                uint32_t error_count;
            } pred_attn_metrics_t;

            pred_attn_metrics_t* metrics = nimcp_malloc(sizeof(pred_attn_metrics_t));
            if (metrics) {
                nimcp_mutex_lock(bridge->base.mutex);
                metrics->avg_error = bridge->stats.avg_error_magnitude;
                metrics->avg_precision = bridge->stats.avg_precision;
                metrics->total_events = bridge->stats.total_events;
                metrics->error_count = bridge->stats.prediction_errors;
                nimcp_mutex_unlock(bridge->base.mutex);

                result->result_data = metrics;
                result->result_size = sizeof(pred_attn_metrics_t);
            }
            break;
        }

        default:
            strncpy(result->error_message, "Unsupported query type",
                    sizeof(result->error_message) - 1);
            result->status = -1;

            nimcp_mutex_lock(bridge->base.mutex);
            bridge->stats.query_errors++;
            nimcp_mutex_unlock(bridge->base.mutex);
            break;
    }

    return result->status;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int predictive_attention_bridge_default_config(
    predictive_attention_bridge_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_default_config", 0.0f);


    config->enable_logging = false;
    config->prediction_error_weight = 0.6f;
    config->surprise_attention_weight = PRED_ATTN_DEFAULT_SURPRISE_WEIGHT;
    config->precision_weight = PRED_ATTN_DEFAULT_PRECISION_WEIGHT;
    config->error_attention_threshold = PRED_ATTN_DEFAULT_ERROR_THRESHOLD;
    config->enable_auto_publish = true;
    config->enable_prediction_subscription = true;
    config->enable_attention_subscription = true;
    config->enable_error_subscription = true;
    config->enable_state_subscription = true;
    config->enable_query_handling = true;
    config->event_priority = COG_PRIORITY_NORMAL;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

predictive_attention_bridge_t* predictive_attention_bridge_create(
    const predictive_attention_bridge_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_create", 0.0f);


    predictive_attention_bridge_t* bridge = (predictive_attention_bridge_t*)nimcp_calloc(
        1, sizeof(predictive_attention_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        predictive_attention_bridge_default_config(&bridge->config);
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "predictive_attention") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "predictive_attention_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->hub = NULL;
    bridge->connected = false;

    /* Initialize callbacks */
    bridge->prediction_callback = NULL;
    bridge->prediction_callback_user_data = NULL;
    bridge->attention_callback = NULL;
    bridge->attention_callback_user_data = NULL;
    bridge->error_callback = NULL;
    bridge->error_callback_user_data = NULL;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(predictive_attention_bridge_stats_t));

    /* Initialize running averages */
    bridge->error_sum = 0.0f;
    bridge->error_count = 0;
    bridge->precision_sum = 0.0f;
    bridge->precision_count = 0;

    bridge->initialized = true;

    return bridge;
}

void predictive_attention_bridge_destroy(predictive_attention_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from hub if connected */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_destroy", 0.0f);


    if (bridge->connected) {
        predictive_attention_bridge_unregister_from_hub(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int predictive_attention_bridge_register_with_hub(
    predictive_attention_bridge_t* bridge,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !bridge->initialized || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_bridge_register_with_hub: required parameter is NULL (bridge, bridge->initialized, hub)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_register_with_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already connected */
    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_attention_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Store hub reference */
    bridge->hub = hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register module with hub */
    int result = cognitive_hub_register_module(
        hub,
        PRED_ATTN_BRIDGE_MODULE_ID,
        COG_CATEGORY_PERCEPTION,
        PRED_ATTN_BRIDGE_MODULE_NAME,
        bridge
    );

    if (result != 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->hub = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_attention_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to configured event types */
    if (bridge->config.enable_prediction_subscription) {
        result = cognitive_hub_subscribe(
            hub,
            PRED_ATTN_BRIDGE_MODULE_ID,
            COG_EVENT_INPUT_RECEIVED,
            pred_attn_hub_on_event,
            bridge
        );
        /* Non-fatal if fails */
    }

    if (bridge->config.enable_attention_subscription) {
        result = cognitive_hub_subscribe(
            hub,
            PRED_ATTN_BRIDGE_MODULE_ID,
            COG_EVENT_ATTENTION_SHIFT,
            pred_attn_hub_on_event,
            bridge
        );
        /* Non-fatal */
    }

    if (bridge->config.enable_error_subscription || bridge->config.enable_state_subscription) {
        result = cognitive_hub_subscribe(
            hub,
            PRED_ATTN_BRIDGE_MODULE_ID,
            COG_EVENT_STATE_CHANGE,
            pred_attn_hub_on_event,
            bridge
        );
        /* Non-fatal */
    }

    /* Register query handler if enabled */
    if (bridge->config.enable_query_handling) {
        result = cognitive_hub_register_query_handler(
            hub,
            PRED_ATTN_BRIDGE_MODULE_ID,
            pred_attn_query_handler
        );
        /* Non-fatal */
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_attention_bridge_unregister_from_hub(
    predictive_attention_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_bridge_unregister_from_hub: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_unregister_from_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_bridge_unregister_from_hub: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unsubscribe from events */
    if (bridge->config.enable_prediction_subscription) {
        cognitive_hub_unsubscribe(hub, PRED_ATTN_BRIDGE_MODULE_ID, COG_EVENT_INPUT_RECEIVED);
    }
    if (bridge->config.enable_attention_subscription) {
        cognitive_hub_unsubscribe(hub, PRED_ATTN_BRIDGE_MODULE_ID, COG_EVENT_ATTENTION_SHIFT);
    }
    if (bridge->config.enable_error_subscription || bridge->config.enable_state_subscription) {
        cognitive_hub_unsubscribe(hub, PRED_ATTN_BRIDGE_MODULE_ID, COG_EVENT_STATE_CHANGE);
    }

    /* Unregister module from hub */
    cognitive_hub_unregister_module(hub, PRED_ATTN_BRIDGE_MODULE_ID);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool predictive_attention_bridge_is_connected(
    const predictive_attention_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Cast away const for mutex lock (safe - only reading) */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_is_connected", 0.0f);


    predictive_attention_bridge_t* mutable_bridge = (predictive_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool connected = bridge->connected;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Predictive -> Attention Operations Implementation
 * ============================================================================ */

int predictive_attention_publish_prediction_error(
    predictive_attention_bridge_t* bridge,
    float error,
    uint64_t location
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_publish_prediction_error: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_publish_prediction_error: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    float threshold = bridge->config.error_attention_threshold;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Clamp error to valid range */
    error = nimcp_clampf(error, 0.0f, 1.0f);

    /* Create payload */
    struct {
        float error_magnitude;
        uint64_t location;
        uint64_t timestamp;
    } payload;

    payload.error_magnitude = error;
    payload.location = location;
    payload.timestamp = get_timestamp_us();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = PRED_ATTN_BRIDGE_MODULE_ID;
    event.timestamp = payload.timestamp;
    event.priority = (error > 0.66f) ? COG_PRIORITY_HIGH :
                     (error > 0.33f) ? COG_PRIORITY_NORMAL : COG_PRIORITY_LOW;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, PRED_ATTN_BRIDGE_MODULE_ID,
                                       COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.prediction_errors++;
        update_running_average(&bridge->error_sum, &bridge->error_count,
                             &bridge->stats.avg_error_magnitude, error);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int predictive_attention_request_attention_to_error(
    predictive_attention_bridge_t* bridge,
    const pred_attn_error_data_t* error_data
) {
    if (!bridge || !bridge->initialized || !error_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_request_attention_to_error: required parameter is NULL (bridge, bridge->initialized, error_data)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_request_attention_to_error: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create attention shift request event */
    struct {
        uint64_t focus_id;
        uint64_t old_focus;
        float urgency;
        uint64_t timestamp;
    } payload;

    payload.focus_id = error_data->error_location;
    payload.old_focus = 0;  /* Unknown previous focus */
    payload.urgency = nimcp_clampf(error_data->error_magnitude, 0.0f, 1.0f);
    payload.timestamp = get_timestamp_us();

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_ATTENTION_SHIFT;
    event.source_module_id = PRED_ATTN_BRIDGE_MODULE_ID;
    event.timestamp = payload.timestamp;
    event.priority = (payload.urgency > 0.7f) ? COG_PRIORITY_HIGH : COG_PRIORITY_NORMAL;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, PRED_ATTN_BRIDGE_MODULE_ID,
                                       COG_EVENT_ATTENTION_SHIFT, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.attention_requests++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int predictive_attention_publish_precision_estimate(
    predictive_attention_bridge_t* bridge,
    const pred_attn_precision_data_t* precision_data
) {
    if (!bridge || !bridge->initialized || !precision_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_publish_precision_estimate: required parameter is NULL (bridge, bridge->initialized, precision_data)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_publish_precision_estimate: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = PRED_ATTN_BRIDGE_MODULE_ID;
    event.timestamp = precision_data->timestamp_us > 0 ? precision_data->timestamp_us : get_timestamp_us();
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)precision_data;
    event.payload_size = sizeof(pred_attn_precision_data_t);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, PRED_ATTN_BRIDGE_MODULE_ID,
                                       COG_EVENT_STATE_CHANGE, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.precision_updates++;
        update_running_average(&bridge->precision_sum, &bridge->precision_count,
                             &bridge->stats.avg_precision, precision_data->precision_new);
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

/* ============================================================================
 * Attention -> Predictive Operations Implementation
 * ============================================================================ */

int predictive_attention_notify_attended_prediction(
    predictive_attention_bridge_t* bridge,
    const pred_attn_prediction_data_t* prediction
) {
    if (!bridge || !bridge->initialized || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_notify_attended_prediction: required parameter is NULL (bridge, bridge->initialized, prediction)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_notify_attended_prediction: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = PRED_ATTN_BRIDGE_MODULE_ID;
    event.timestamp = prediction->timestamp_us > 0 ? prediction->timestamp_us : get_timestamp_us();
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)prediction;
    event.payload_size = sizeof(pred_attn_prediction_data_t);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, PRED_ATTN_BRIDGE_MODULE_ID,
                                       COG_EVENT_OUTPUT_READY, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        bridge->stats.focus_predictions++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int predictive_attention_request_prediction_for_focus(
    predictive_attention_bridge_t* bridge,
    const pred_attn_focus_request_t* focus
) {
    if (!bridge || !bridge->initialized || !focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_request_prediction_for_focus: required parameter is NULL (bridge, bridge->initialized, focus)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_request_prediction_for_focus: required parameter is NULL (bridge->connected, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = PRED_ATTN_BRIDGE_MODULE_ID;
    event.timestamp = focus->timestamp_us > 0 ? focus->timestamp_us : get_timestamp_us();
    event.priority = (focus->urgency > 0.7f) ? COG_PRIORITY_HIGH : COG_PRIORITY_NORMAL;
    event.payload = (void*)focus;
    event.payload_size = sizeof(pred_attn_focus_request_t);

    /* Publish to hub */
    int result = cognitive_hub_publish(hub, PRED_ATTN_BRIDGE_MODULE_ID,
                                       COG_EVENT_INPUT_RECEIVED, &event);

    if (result == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.events_published++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

/* ============================================================================
 * Event Callback Registration Implementation
 * ============================================================================ */

int predictive_attention_set_prediction_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_prediction_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_set_prediction_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->prediction_callback = callback;
    bridge->prediction_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_attention_set_attention_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_attention_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_set_attention_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_callback = callback;
    bridge->attention_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_attention_set_error_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_error_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_set_error_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_predictive_attention", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->error_callback = callback;
    bridge->error_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int predictive_attention_bridge_get_stats(
    const predictive_attention_bridge_t* bridge,
    predictive_attention_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_get_stats", 0.0f);


    predictive_attention_bridge_t* mutable_bridge = (predictive_attention_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int predictive_attention_bridge_reset_stats(
    predictive_attention_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_attention_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_bridge_heartbeat("predictive_a_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(predictive_attention_bridge_stats_t));
    bridge->error_sum = 0.0f;
    bridge->error_count = 0;
    bridge->precision_sum = 0.0f;
    bridge->precision_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* pred_attn_event_subtype_to_string(pred_attn_event_subtype_t subtype) {
    switch (subtype) {
        case PRED_ATTN_EVENT_PREDICTION_ERROR:
            return "PREDICTION_ERROR";
        case PRED_ATTN_EVENT_ATTENTION_REQUEST:
            return "ATTENTION_REQUEST";
        case PRED_ATTN_EVENT_PRECISION_UPDATE:
            return "PRECISION_UPDATE";
        case PRED_ATTN_EVENT_ATTENDED_PREDICTION:
            return "ATTENDED_PREDICTION";
        case PRED_ATTN_EVENT_FOCUS_PREDICTION_REQUEST:
            return "FOCUS_PREDICTION_REQUEST";
        case PRED_ATTN_EVENT_SURPRISE_DETECTED:
            return "SURPRISE_DETECTED";
        case PRED_ATTN_EVENT_ATTENTION_SHIFTED:
            return "ATTENTION_SHIFTED";
        case PRED_ATTN_EVENT_SAMPLING_STARTED:
            return "SAMPLING_STARTED";
        case PRED_ATTN_EVENT_SAMPLING_COMPLETED:
            return "SAMPLING_COMPLETED";
        default:
            return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void predictive_attention_bridge_set_instance_health_agent(predictive_attention_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "predictive_attention_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int predictive_attention_bridge_training_begin(predictive_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_bridge_training_begin: NULL argument");
        return -1;
    }
    predictive_attention_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_bridge_training_begin", 0.0f);
    return 0;
}

int predictive_attention_bridge_training_end(predictive_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_bridge_training_end: NULL argument");
        return -1;
    }
    predictive_attention_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_bridge_training_end", 1.0f);
    return 0;
}

int predictive_attention_bridge_training_step(predictive_attention_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    predictive_attention_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_bridge_training_step", progress);
    return 0;
}
