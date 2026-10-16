/**
 * @file nimcp_imagination_reasoning_bridge.c
 * @brief Imagination-Reasoning Cognitive Integration Hub Bridge Implementation
 * @version 1.1.0
 * @date 2026-01-08
 *
 * WHAT: Implementation of the bridge connecting imagination and reasoning modules
 *       through the cognitive integration hub for bidirectional event-driven communication.
 *
 * WHY: Enable imagination and reasoning to work together for:
 *      - Counterfactual reasoning: Analyzing "what-if" scenarios
 *      - Creative inference: Making novel logical leaps from imagined premises
 *      - Simulation-guided reasoning: Using mental simulation to inform logic
 *      - Scenario planning: Combining imaginative exploration with structured analysis
 *
 * HOW: Registers with the cognitive hub, subscribes to relevant events,
 *      handles event callbacks, and provides specialized operations.
 *
 * THREAD SAFETY: All public functions are thread-safe using internal mutex.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(imagination_reasoning_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "IMAGINATION_REASONING_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define BRIDGE_NAME "ImaginationReasoning"
#define DEFAULT_COUNTERFACTUAL_WEIGHT   0.5f
#define DEFAULT_SIMULATION_WEIGHT       0.5f
#define DEFAULT_CREATIVE_WEIGHT         0.3f
#define DEFAULT_MAX_SCENARIOS           64

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Active scenario tracking
 */
typedef struct {
    uint64_t scenario_id;               /**< Unique identifier */
    imagination_scenario_type_t type;   /**< Scenario type */
    float complexity;                   /**< Complexity level */
    uint64_t start_time_us;             /**< Processing start time */
    bool active;                        /**< Is slot active */
} active_scenario_t;

/**
 * @brief Imagination-Reasoning bridge internal structure
 */
struct imagination_reasoning_bridge {
    bridge_base_t base;                        /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    imagination_reasoning_config_t config;     /**< Bridge configuration */
    cognitive_integration_hub_t hub;           /**< Connected hub */
    imagination_engine_t* imagination;         /**< Imagination engine ref */
    reasoning_engine_t* reasoning;             /**< Reasoning engine ref */

    /* State */
    imagination_reasoning_state_t state;       /**< Current bridge state */
    active_scenario_t* active_scenarios;       /**< Active scenario tracking */
    uint32_t active_scenario_count;            /**< Number of active scenarios */
    uint64_t next_scenario_id;                 /**< Next scenario ID counter */

    /* Callbacks */
    counterfactual_result_callback_t counterfactual_callback;
    void* counterfactual_callback_data;
    simulation_result_callback_t simulation_callback;
    void* simulation_callback_data;
    creative_inference_callback_t creative_callback;
    void* creative_callback_data;
    insight_callback_t insight_callback;
    void* insight_callback_data;

    /* Statistics */
    imagination_reasoning_stats_t stats;       /**< Bridge statistics */

    /* Synchronization */
    bool connected;                            /**< Connection status */
    bool initialized;                          /**< Initialization flag */
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
        return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
    }
    return 0;
}

/**
 * @brief Update running average
 */
static float update_average(float old_avg, float new_value, uint32_t count) {
    if (count == 0) return new_value;
    return old_avg + (new_value - old_avg) / (float)(count + 1);
}

/**
 * @brief Find free scenario slot (unlocked)
 */
static int find_free_scenario_slot_unlocked(imagination_reasoning_bridge_t* bridge) {
    if (!bridge->active_scenarios) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_scenario_slot_unlocked: bridge->active_scenarios is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->config.max_concurrent_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_concurrent_scenarios > 256) {
            imagination_reasoning_bridge_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)bridge->config.max_concurrent_scenarios);
        }

        if (!bridge->active_scenarios[i].active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_scenario_slot_unlocked: bridge->active_scenarios is NULL");
    return -1;
}

/**
 * @brief Find scenario by ID (unlocked)
 */
static int find_scenario_by_id_unlocked(imagination_reasoning_bridge_t* bridge,
                                        uint64_t scenario_id) {
    if (!bridge->active_scenarios) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_scenario_slot_unlocked: bridge->active_scenarios is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->config.max_concurrent_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_concurrent_scenarios > 256) {
            imagination_reasoning_bridge_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)bridge->config.max_concurrent_scenarios);
        }

        if (bridge->active_scenarios[i].active &&
            bridge->active_scenarios[i].scenario_id == scenario_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_free_scenario_slot_unlocked: operation failed");
    return -1;
}

/**
 * @brief Handle counterfactual result event (unlocked)
 */
static int handle_counterfactual_result_unlocked(
    imagination_reasoning_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!event->payload || event->payload_size < sizeof(counterfactual_result_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_counterfactual_result_unlocked: event->payload is NULL");
        return -1;
    }

    const counterfactual_result_t* result = (const counterfactual_result_t*)event->payload;

    /* Update statistics */
    bridge->stats.counterfactual_queries++;
    bridge->stats.avg_counterfactual_confidence = update_average(
        bridge->stats.avg_counterfactual_confidence,
        result->confidence,
        bridge->stats.counterfactual_queries
    );

    /* Invoke callback if registered */
    if (bridge->counterfactual_callback) {
        counterfactual_result_callback_t callback = bridge->counterfactual_callback;
        void* user_data = bridge->counterfactual_callback_data;

        /* Release lock for callback */
        nimcp_mutex_unlock(bridge->base.mutex);
        callback(result, user_data);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    return 0;
}

/**
 * @brief Handle simulation result event (unlocked)
 */
static int handle_simulation_result_unlocked(
    imagination_reasoning_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!event->payload || event->payload_size < sizeof(simulation_result_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_simulation_result_unlocked: event->payload is NULL");
        return -1;
    }

    const simulation_result_t* result = (const simulation_result_t*)event->payload;

    /* Update statistics */
    bridge->stats.simulation_results++;

    /* Invoke callback if registered */
    if (bridge->simulation_callback) {
        simulation_result_callback_t callback = bridge->simulation_callback;
        void* user_data = bridge->simulation_callback_data;

        /* Release lock for callback */
        nimcp_mutex_unlock(bridge->base.mutex);
        callback(result, user_data);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    return 0;
}

/**
 * @brief Handle creative inference result event (unlocked)
 */
static int handle_creative_result_unlocked(
    imagination_reasoning_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!event->payload || event->payload_size < sizeof(creative_inference_result_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_creative_result_unlocked: event->payload is NULL");
        return -1;
    }

    const creative_inference_result_t* result =
        (const creative_inference_result_t*)event->payload;

    /* Update statistics */
    bridge->stats.creative_inferences++;
    bridge->stats.avg_creative_novelty = update_average(
        bridge->stats.avg_creative_novelty,
        result->novelty,
        bridge->stats.creative_inferences
    );

    /* Invoke callback if registered */
    if (bridge->creative_callback) {
        creative_inference_callback_t callback = bridge->creative_callback;
        void* user_data = bridge->creative_callback_data;

        /* Release lock for callback */
        nimcp_mutex_unlock(bridge->base.mutex);
        callback(result, user_data);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    return 0;
}

/**
 * @brief Handle insight feedback event (unlocked)
 */
static int handle_insight_feedback_unlocked(
    imagination_reasoning_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!event->payload || event->payload_size < sizeof(imagination_insight_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_insight_feedback_unlocked: event->payload is NULL");
        return -1;
    }

    const imagination_insight_t* insight = (const imagination_insight_t*)event->payload;

    /* Update statistics */
    bridge->stats.insights_shared++;

    /* Invoke callback if registered */
    if (bridge->insight_callback) {
        insight_callback_t callback = bridge->insight_callback;
        void* user_data = bridge->insight_callback_data;

        /* Release lock for callback */
        nimcp_mutex_unlock(bridge->base.mutex);
        callback(insight, user_data);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    return 0;
}

/**
 * @brief Query handler for bridge state queries
 */
static int imagination_reasoning_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    imagination_reasoning_bridge_t* bridge = (imagination_reasoning_bridge_t*)context;

    if (!bridge || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_query_handler: required parameter is NULL (bridge, query, result)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        result->status = -1;
        strncpy(result->error_message, "Bridge not connected",
                sizeof(result->error_message) - 1);
        bridge->stats.query_errors++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_query_handler: bridge->connected is NULL");
        return -1;
    }

    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            result->status = 0;
            result->result_data = NULL;
            result->result_size = 0;
            break;
        }

        case COG_QUERY_STATE: {
            /* Allocate and return bridge state */
            imagination_reasoning_state_t* state_ptr = nimcp_malloc(
                sizeof(imagination_reasoning_state_t)
            );
            if (!state_ptr) {
                result->status = -1;
                strncpy(result->error_message, "Memory allocation failed",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_reasoning_query_handler: state_ptr is NULL");
                return -1;
            }

            *state_ptr = bridge->state;
            result->status = 0;
            result->result_data = state_ptr;
            result->result_size = sizeof(imagination_reasoning_state_t);
            break;
        }

        case COG_QUERY_METRICS: {
            /* Allocate and return statistics */
            imagination_reasoning_stats_t* stats_ptr = nimcp_malloc(
                sizeof(imagination_reasoning_stats_t)
            );
            if (!stats_ptr) {
                result->status = -1;
                strncpy(result->error_message, "Memory allocation failed",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_reasoning_query_handler: stats_ptr is NULL");
                return -1;
            }

            *stats_ptr = bridge->stats;
            result->status = 0;
            result->result_data = stats_ptr;
            result->result_size = sizeof(imagination_reasoning_stats_t);
            break;
        }

        default:
            result->status = -1;
            strncpy(result->error_message, "Unsupported query type",
                    sizeof(result->error_message) - 1);
            bridge->stats.query_errors++;
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_query_handler: operation failed");
            return -1;
    }

    bridge->stats.queries_handled++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int imagination_reasoning_bridge_default_config(imagination_reasoning_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__default_config", 0.0f);


    config->module_id = IMAG_REASON_DEFAULT_MODULE_ID;
    config->max_concurrent_scenarios = DEFAULT_MAX_SCENARIOS;
    config->imagination_weight = 0.5f;
    config->reasoning_weight = 0.5f;
    config->auto_subscribe_input = true;
    config->auto_subscribe_attention = true;
    config->enable_counterfactual = true;
    config->enable_prospective = true;
    config->enable_logging = false;
    config->counterfactual_weight = DEFAULT_COUNTERFACTUAL_WEIGHT;
    config->simulation_reasoning_weight = DEFAULT_SIMULATION_WEIGHT;
    config->creative_inference_weight = DEFAULT_CREATIVE_WEIGHT;
    config->enable_query_handling = true;
    config->event_priority = COG_PRIORITY_NORMAL;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

imagination_reasoning_bridge_t* imagination_reasoning_bridge_create(
    const imagination_reasoning_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__create", 0.0f);


    imagination_reasoning_bridge_t* bridge = nimcp_calloc(
        1, sizeof(imagination_reasoning_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        imagination_reasoning_bridge_default_config(&bridge->config);
    }

    /* Validate module ID */
    if (bridge->config.module_id == 0) {
        bridge->config.module_id = IMAG_REASON_DEFAULT_MODULE_ID;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "imagination_reasoning") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "imagination_reasoning_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate scenario tracking */
    uint32_t max_scenarios = bridge->config.max_concurrent_scenarios;
    if (max_scenarios == 0) max_scenarios = DEFAULT_MAX_SCENARIOS;

    bridge->active_scenarios = nimcp_calloc(max_scenarios, sizeof(active_scenario_t));
    if (!bridge->active_scenarios) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_reasoning_bridge_create: bridge->active_scenarios is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state = IMAG_REASON_STATE_IDLE;
    bridge->active_scenario_count = 0;
    bridge->next_scenario_id = 1;

    /* Initialize callbacks */
    bridge->counterfactual_callback = NULL;
    bridge->counterfactual_callback_data = NULL;
    bridge->simulation_callback = NULL;
    bridge->simulation_callback_data = NULL;
    bridge->creative_callback = NULL;
    bridge->creative_callback_data = NULL;
    bridge->insight_callback = NULL;
    bridge->insight_callback_data = NULL;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(imagination_reasoning_stats_t));

    bridge->hub = NULL;
    bridge->imagination = NULL;
    bridge->reasoning = NULL;
    bridge->connected = false;
    bridge->initialized = true;

    return bridge;
}

void imagination_reasoning_bridge_destroy(imagination_reasoning_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect if still connected */
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__destroy", 0.0f);


    if (bridge->connected) {
        imagination_reasoning_bridge_disconnect(bridge);
    }

    /* Free scenario tracking */
    if (bridge->active_scenarios) {
        nimcp_free(bridge->active_scenarios);
        bridge->active_scenarios = NULL;
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    bridge->initialized = false;

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Hub Registration API
 * ============================================================================ */

int imagination_reasoning_bridge_register_with_hub(
    imagination_reasoning_bridge_t* bridge,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_register_with_hub: required parameter is NULL (bridge, hub)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_register_with_hub: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__register_with_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_bridge_register_with_hub: validation failed");
        return -1;  /* Already connected */
    }

    /* Register module with hub */
    int ret = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_REASONING,
        BRIDGE_NAME,
        bridge
    );
    if (ret != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to output ready events (for simulation results) */
    ret = cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_OUTPUT_READY,
        (cognitive_event_callback_t)imagination_reasoning_on_event,
        bridge
    );
    if (ret != 0) {
        cognitive_hub_unregister_module(hub, bridge->config.module_id);
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to state change events (for insights) */
    ret = cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_STATE_CHANGE,
        (cognitive_event_callback_t)imagination_reasoning_on_event,
        bridge
    );
    if (ret != 0) {
        cognitive_hub_unsubscribe(hub, bridge->config.module_id, COG_EVENT_OUTPUT_READY);
        cognitive_hub_unregister_module(hub, bridge->config.module_id);
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to learning complete events (for creative inferences) */
    ret = cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_LEARNING_COMPLETE,
        (cognitive_event_callback_t)imagination_reasoning_on_event,
        bridge
    );
    if (ret != 0) {
        cognitive_hub_unsubscribe(hub, bridge->config.module_id, COG_EVENT_STATE_CHANGE);
        cognitive_hub_unsubscribe(hub, bridge->config.module_id, COG_EVENT_OUTPUT_READY);
        cognitive_hub_unregister_module(hub, bridge->config.module_id);
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to input received (if configured) */
    if (bridge->config.auto_subscribe_input) {
        ret = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_INPUT_RECEIVED,
            (cognitive_event_callback_t)imagination_reasoning_on_event,
            bridge
        );
        /* Non-fatal if fails */
    }

    /* Subscribe to attention shift (if configured) */
    if (bridge->config.auto_subscribe_attention) {
        ret = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_ATTENTION_SHIFT,
            (cognitive_event_callback_t)imagination_reasoning_on_event,
            bridge
        );
        /* Non-fatal if fails */
    }

    /* Register query handler if enabled */
    if (bridge->config.enable_query_handling) {
        ret = cognitive_hub_register_query_handler(
            hub,
            bridge->config.module_id,
            imagination_reasoning_query_handler
        );
        /* Non-fatal if fails */
    }

    /* Store reference */
    bridge->hub = hub;
    bridge->connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_bridge_unregister_from_hub(
    imagination_reasoning_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__unregister_from_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_unregister_from_hub: bridge->connected is NULL");
        return -1;
    }

    /* Unsubscribe from events */
    cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                              COG_EVENT_LEARNING_COMPLETE);
    cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                              COG_EVENT_STATE_CHANGE);
    cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                              COG_EVENT_OUTPUT_READY);

    if (bridge->config.auto_subscribe_input) {
        cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                                  COG_EVENT_INPUT_RECEIVED);
    }
    if (bridge->config.auto_subscribe_attention) {
        cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                                  COG_EVENT_ATTENTION_SHIFT);
    }

    /* Unregister module */
    cognitive_hub_unregister_module(bridge->hub, bridge->config.module_id);

    /* Clear references */
    bridge->hub = NULL;
    bridge->connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_bridge_connect(
    imagination_reasoning_bridge_t* bridge,
    cognitive_integration_hub_t hub
) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__connect", 0.0f);


    return imagination_reasoning_bridge_register_with_hub(bridge, hub);
}

int imagination_reasoning_bridge_disconnect(imagination_reasoning_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__disconnect", 0.0f);


    return imagination_reasoning_bridge_unregister_from_hub(bridge);
}

bool imagination_reasoning_bridge_is_connected(
    const imagination_reasoning_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__is_connected", 0.0f);


    nimcp_mutex_lock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);
    bool connected = bridge->connected;
    nimcp_mutex_unlock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);

    return connected;
}

/* ============================================================================
 * Module Connection API
 * ============================================================================ */

int imagination_reasoning_bridge_set_imagination(
    imagination_reasoning_bridge_t* bridge,
    imagination_engine_t* engine
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_set_imagination: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__set_imagination", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->imagination = engine;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_bridge_set_reasoning(
    imagination_reasoning_bridge_t* bridge,
    reasoning_engine_t* engine
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_set_reasoning: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__set_reasoning", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->reasoning = engine;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Counterfactual Analysis API
 * ============================================================================ */

int imagination_reasoning_request_counterfactual_analysis(
    imagination_reasoning_bridge_t* bridge,
    const counterfactual_scenario_t* scenario
) {
    if (!bridge || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_request_counterfactual_analysis: required parameter is NULL (bridge, scenario)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_request_counterfactual_analysis: bridge->connected is NULL");
        return -1;
    }

    /* Track the scenario */
    int slot = find_free_scenario_slot_unlocked(bridge);
    if (slot >= 0) {
        bridge->active_scenarios[slot].scenario_id = scenario->scenario_id;
        bridge->active_scenarios[slot].type = IMAG_SCENARIO_COUNTERFACTUAL;
        bridge->active_scenarios[slot].complexity = scenario->complexity;
        bridge->active_scenarios[slot].start_time_us = get_timestamp_us();
        bridge->active_scenarios[slot].active = true;
        bridge->active_scenario_count++;
    }

    /* Update state */
    bridge->state = IMAG_REASON_STATE_ANALYZING;

    /* Create event payload */
    typedef struct {
        imag_reason_event_type_t subtype;
        counterfactual_scenario_t scenario;
    } counterfactual_request_payload_t;

    counterfactual_request_payload_t payload;
    payload.subtype = IMAG_REASON_EVENT_COUNTERFACTUAL_REQUEST;
    payload.scenario = *scenario;

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_INPUT_RECEIVED;  /* Request is an input */
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Cache hub reference before releasing lock */
    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Release lock before publishing to avoid deadlock from synchronous callbacks */
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        hub,
        module_id,
        COG_EVENT_INPUT_RECEIVED,
        &event
    );

    /* Relock to update stats */
    nimcp_mutex_lock(bridge->base.mutex);
    if (ret == 0) {
        bridge->stats.events_published++;
        bridge->stats.counterfactual_queries++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int imagination_reasoning_set_counterfactual_callback(
    imagination_reasoning_bridge_t* bridge,
    counterfactual_result_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_set_counterfactual_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->counterfactual_callback = callback;
    bridge->counterfactual_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Simulation Result API
 * ============================================================================ */

int imagination_reasoning_publish_simulation_result(
    imagination_reasoning_bridge_t* bridge,
    const simulation_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_simulation_result: required parameter is NULL (bridge, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_simulation_result: bridge->connected is NULL");
        return -1;
    }

    /* Create event payload */
    typedef struct {
        imag_reason_event_type_t subtype;
        simulation_result_t result;
    } simulation_payload_t;

    simulation_payload_t payload;
    payload.subtype = IMAG_REASON_EVENT_SIMULATION_RESULT;
    payload.result = *result;

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Cache hub reference before releasing lock */
    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Release lock before publishing to avoid deadlock from synchronous callbacks */
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        hub,
        module_id,
        COG_EVENT_OUTPUT_READY,
        &event
    );

    /* Relock to update stats */
    nimcp_mutex_lock(bridge->base.mutex);
    if (ret == 0) {
        bridge->stats.events_published++;
        bridge->stats.simulation_results++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int imagination_reasoning_set_simulation_callback(
    imagination_reasoning_bridge_t* bridge,
    simulation_result_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_set_simulation_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->simulation_callback = callback;
    bridge->simulation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Creative Inference API
 * ============================================================================ */

int imagination_reasoning_request_creative_inference(
    imagination_reasoning_bridge_t* bridge,
    const creative_inference_request_t* request
) {
    if (!bridge || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_request_creative_inference: required parameter is NULL (bridge, request)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_request_creative_inference: bridge->connected is NULL");
        return -1;
    }

    /* Update state */
    bridge->state = IMAG_REASON_STATE_INTEGRATING;

    /* Create event payload */
    typedef struct {
        imag_reason_event_type_t subtype;
        creative_inference_request_t request;
    } creative_request_payload_t;

    creative_request_payload_t payload;
    payload.subtype = IMAG_REASON_EVENT_CREATIVE_REQUEST;
    payload.request = *request;

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_LEARNING_COMPLETE;  /* Creative processing */
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Cache hub reference before releasing lock */
    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Release lock before publishing to avoid deadlock from synchronous callbacks */
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        hub,
        module_id,
        COG_EVENT_LEARNING_COMPLETE,
        &event
    );

    /* Relock to update stats */
    nimcp_mutex_lock(bridge->base.mutex);
    if (ret == 0) {
        bridge->stats.events_published++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int imagination_reasoning_set_creative_callback(
    imagination_reasoning_bridge_t* bridge,
    creative_inference_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_set_creative_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->creative_callback = callback;
    bridge->creative_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Insight Publication API
 * ============================================================================ */

int imagination_reasoning_publish_insight(
    imagination_reasoning_bridge_t* bridge,
    const imagination_insight_t* insight
) {
    if (!bridge || !insight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_insight: required parameter is NULL (bridge, insight)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_insight: bridge->connected is NULL");
        return -1;
    }

    /* Create event payload */
    typedef struct {
        imag_reason_event_type_t subtype;
        imagination_insight_t insight;
    } insight_payload_t;

    insight_payload_t payload;
    payload.subtype = IMAG_REASON_EVENT_INSIGHT_PUBLISHED;
    payload.insight = *insight;

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = &payload;
    event.payload_size = sizeof(payload);

    /* Cache hub reference before releasing lock */
    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Release lock before publishing to avoid deadlock from synchronous callbacks */
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        hub,
        module_id,
        COG_EVENT_STATE_CHANGE,
        &event
    );

    /* Relock to update stats */
    nimcp_mutex_lock(bridge->base.mutex);
    if (ret == 0) {
        bridge->stats.events_published++;
        bridge->stats.insights_shared++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int imagination_reasoning_set_insight_callback(
    imagination_reasoning_bridge_t* bridge,
    insight_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_set_insight_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->insight_callback = callback;
    bridge->insight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Scenario Generation API
 * ============================================================================ */

int imagination_reasoning_generate_scenario(
    imagination_reasoning_bridge_t* bridge,
    imagination_scenario_type_t type,
    float complexity,
    imagination_scenario_t* scenario_out
) {
    if (!bridge || !scenario_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_generate_scenario: required parameter is NULL (bridge, scenario_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    if (type >= IMAG_SCENARIO_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "imagination_reasoning_generate_scenario: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find free slot */
    int slot = find_free_scenario_slot_unlocked(bridge);
    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_reasoning_generate_scenario: validation failed");
        return -1;  /* No free slots */
    }

    /* Generate scenario ID */
    uint64_t scenario_id = bridge->next_scenario_id++;

    /* Create scenario */
    scenario_out->scenario_id = scenario_id;
    scenario_out->type = type;
    scenario_out->plausibility = 0.5f;  /* Default */
    scenario_out->relevance = 0.5f;     /* Default */
    scenario_out->complexity = nimcp_clampf(complexity, 0.0f, 1.0f);
    scenario_out->creation_time = get_timestamp_us();

    /* Track scenario */
    bridge->active_scenarios[slot].scenario_id = scenario_id;
    bridge->active_scenarios[slot].type = type;
    bridge->active_scenarios[slot].complexity = complexity;
    bridge->active_scenarios[slot].start_time_us = scenario_out->creation_time;
    bridge->active_scenarios[slot].active = true;
    bridge->active_scenario_count++;

    /* Update state */
    bridge->state = IMAG_REASON_STATE_IMAGINING;

    /* Update statistics */
    bridge->stats.scenarios_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_analyze_scenario(
    imagination_reasoning_bridge_t* bridge,
    const imagination_scenario_t* scenario,
    imagination_analysis_result_t* result_out
) {
    if (!bridge || !scenario || !result_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_analyze_scenario: required parameter is NULL (bridge, scenario, result_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update state */
    bridge->state = IMAG_REASON_STATE_ANALYZING;

    /* Simulate analysis (in real implementation, would invoke reasoning engine) */
    result_out->scenario_id = scenario->scenario_id;
    result_out->logical_consistency = 0.8f - (scenario->complexity * 0.2f);
    result_out->utility_estimate = scenario->relevance * scenario->plausibility;
    result_out->confidence = 0.7f;
    result_out->feasible = (result_out->utility_estimate > 0.3f);

    /* Update statistics */
    bridge->stats.scenarios_analyzed++;
    bridge->stats.avg_plausibility = update_average(
        bridge->stats.avg_plausibility,
        scenario->plausibility,
        bridge->stats.scenarios_analyzed
    );
    bridge->stats.avg_utility = update_average(
        bridge->stats.avg_utility,
        result_out->utility_estimate,
        bridge->stats.scenarios_analyzed
    );

    if (result_out->feasible) {
        bridge->stats.scenarios_accepted++;
    }

    /* Clear scenario from tracking */
    int slot = find_scenario_by_id_unlocked(bridge, scenario->scenario_id);
    if (slot >= 0) {
        uint64_t end_time = get_timestamp_us();
        float analysis_time_ms = (float)(end_time - bridge->active_scenarios[slot].start_time_us) / 1000.0f;
        bridge->stats.avg_analysis_time_ms = update_average(
            bridge->stats.avg_analysis_time_ms,
            analysis_time_ms,
            bridge->stats.scenarios_analyzed
        );

        bridge->active_scenarios[slot].active = false;
        bridge->active_scenario_count--;
    }

    /* Update state */
    if (bridge->active_scenario_count == 0) {
        bridge->state = IMAG_REASON_STATE_IDLE;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_publish_result(
    imagination_reasoning_bridge_t* bridge,
    const imagination_analysis_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_result: required parameter is NULL (bridge, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_publish_result: bridge->connected is NULL");
        return -1;
    }

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = (void*)result;
    event.payload_size = sizeof(imagination_analysis_result_t);

    /* Cache hub reference before releasing lock */
    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Release lock before publishing to avoid deadlock from synchronous callbacks */
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        hub,
        module_id,
        COG_EVENT_OUTPUT_READY,
        &event
    );

    /* Relock to update stats */
    nimcp_mutex_lock(bridge->base.mutex);
    if (ret == 0) {
        bridge->stats.events_published++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

/* ============================================================================
 * Event Handling API
 * ============================================================================ */

int imagination_reasoning_on_event(
    const void* event_ptr,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__imagination_reasonin", 0.0f);


    const cognitive_event_data_t* event = (const cognitive_event_data_t*)event_ptr;
    imagination_reasoning_bridge_t* bridge = (imagination_reasoning_bridge_t*)user_data;

    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_on_event: required parameter is NULL (bridge, event)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_on_event: bridge->connected is NULL");
        return -1;
    }

    /* Update received count */
    bridge->stats.events_received++;
    bridge->stats.total_events++;

    int result = 0;

    /* Check if this is a bridge-specific event with subtype */
    if (event->payload && event->payload_size >= sizeof(imag_reason_event_type_t)) {
        imag_reason_event_type_t* subtype = (imag_reason_event_type_t*)event->payload;

        switch (*subtype) {
            case IMAG_REASON_EVENT_COUNTERFACTUAL_RESULT:
                result = handle_counterfactual_result_unlocked(bridge, event);
                break;

            case IMAG_REASON_EVENT_SIMULATION_RESULT:
            case IMAG_REASON_EVENT_SIMULATION_PROCESSED:
                result = handle_simulation_result_unlocked(bridge, event);
                break;

            case IMAG_REASON_EVENT_CREATIVE_RESULT:
                result = handle_creative_result_unlocked(bridge, event);
                break;

            case IMAG_REASON_EVENT_INSIGHT_FEEDBACK:
                result = handle_insight_feedback_unlocked(bridge, event);
                break;

            default:
                /* Handle generic event types */
                break;
        }
    }

    /* Handle by cognitive event type if not a bridge-specific event */
    switch (event->event_type) {
        case COG_EVENT_OUTPUT_READY:
            /* Could be simulation results from imagination */
            break;

        case COG_EVENT_STATE_CHANGE:
            /* State updates from either module */
            break;

        case COG_EVENT_LEARNING_COMPLETE:
            /* Creative inference completed */
            break;

        case COG_EVENT_INPUT_RECEIVED:
            /* New reasoning problem */
            break;

        case COG_EVENT_ATTENTION_SHIFT:
            /* Attention changed - may need to reprioritize scenarios */
            break;

        default:
            /* Ignore unhandled event types */
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

imagination_reasoning_state_t imagination_reasoning_bridge_get_state(
    const imagination_reasoning_bridge_t* bridge
) {
    if (!bridge) {
        return IMAG_REASON_STATE_ERROR;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__get_state", 0.0f);


    nimcp_mutex_lock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);
    imagination_reasoning_state_t state = bridge->state;
    nimcp_mutex_unlock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);

    return state;
}

uint32_t imagination_reasoning_bridge_get_module_id(
    const imagination_reasoning_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__get_module_id", 0.0f);


    return bridge->config.module_id;
}

uint32_t imagination_reasoning_bridge_get_active_scenarios(
    const imagination_reasoning_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__get_active_scenarios", 0.0f);


    nimcp_mutex_lock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);
    uint32_t count = bridge->active_scenario_count;
    nimcp_mutex_unlock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);

    return count;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int imagination_reasoning_bridge_get_stats(
    const imagination_reasoning_bridge_t* bridge,
    imagination_reasoning_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__get_stats", 0.0f);


    nimcp_mutex_lock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((imagination_reasoning_bridge_t*)bridge)->base.mutex);

    return 0;
}

int imagination_reasoning_bridge_reset_stats(imagination_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reasoning_bridge_reset_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(imagination_reasoning_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_reasoning_bridge_force_update(imagination_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_bridge_heartbeat("imagination__force_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update state based on active scenarios */
    if (bridge->active_scenario_count == 0) {
        bridge->state = IMAG_REASON_STATE_IDLE;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

const char* imag_reason_event_type_to_string(imag_reason_event_type_t event_type) {
    switch (event_type) {
        case IMAG_REASON_EVENT_COUNTERFACTUAL_REQUEST:
            return "COUNTERFACTUAL_REQUEST";
        case IMAG_REASON_EVENT_COUNTERFACTUAL_RESULT:
            return "COUNTERFACTUAL_RESULT";
        case IMAG_REASON_EVENT_SIMULATION_RESULT:
            return "SIMULATION_RESULT";
        case IMAG_REASON_EVENT_SIMULATION_PROCESSED:
            return "SIMULATION_PROCESSED";
        case IMAG_REASON_EVENT_CREATIVE_REQUEST:
            return "CREATIVE_REQUEST";
        case IMAG_REASON_EVENT_CREATIVE_RESULT:
            return "CREATIVE_RESULT";
        case IMAG_REASON_EVENT_INSIGHT_PUBLISHED:
            return "INSIGHT_PUBLISHED";
        case IMAG_REASON_EVENT_INSIGHT_FEEDBACK:
            return "INSIGHT_FEEDBACK";
        case IMAG_REASON_EVENT_IMAGINATION_STATE:
            return "IMAGINATION_STATE";
        case IMAG_REASON_EVENT_REASONING_STATE:
            return "REASONING_STATE";
        default:
            return "UNKNOWN";
    }
}


void imagination_reasoning_bridge_set_instance_health_agent(imagination_reasoning_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "imagination_reasoning_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int imagination_reasoning_bridge_training_begin(imagination_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_bridge_training_begin: NULL argument");
        return -1;
    }
    imagination_reasoning_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_bridge_training_begin", 0.0f);
    return 0;
}

int imagination_reasoning_bridge_training_end(imagination_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_bridge_training_end: NULL argument");
        return -1;
    }
    imagination_reasoning_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_bridge_training_end", 1.0f);
    return 0;
}

int imagination_reasoning_bridge_training_step(imagination_reasoning_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    imagination_reasoning_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_bridge_training_step", progress);
    return 0;
}
