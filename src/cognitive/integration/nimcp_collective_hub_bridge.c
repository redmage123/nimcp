/**
 * @file nimcp_collective_hub_bridge.c
 * @brief Collective Cognition - Cognitive Integration Hub Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Implementation of the bridge connecting collective cognition to the
 *       cognitive integration hub for cross-module event communication.
 *
 * WHY: Enable collective cognition to participate in the system-wide event
 *      routing and receive events from other cognitive modules.
 *
 * HOW: Registers with hub, subscribes to relevant events, handles callbacks,
 *      and publishes collective state changes and consensus events.
 *
 * THREAD SAFETY: All public functions are thread-safe using internal mutex.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_collective_hub_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
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
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_hub_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_collective_hub_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_collective_hub_bridge_mesh_registry = NULL;

nimcp_error_t collective_hub_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_collective_hub_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "collective_hub_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "collective_hub_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_collective_hub_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_collective_hub_bridge_mesh_registry = registry;
    return err;
}

void collective_hub_bridge_mesh_unregister(void) {
    if (g_collective_hub_bridge_mesh_registry && g_collective_hub_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_collective_hub_bridge_mesh_registry, g_collective_hub_bridge_mesh_id);
        g_collective_hub_bridge_mesh_id = 0;
        g_collective_hub_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from collective_hub_bridge module (instance-level) */
static inline void collective_hub_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_collective_hub_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_collective_hub_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_collective_hub_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "COLLECTIVE_HUB_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define DEFAULT_PHI_THRESHOLD       0.05f
#define DEFAULT_COHERENCE_THRESHOLD 0.05f

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Cached collective state for change detection
 */
typedef struct {
    float phi;                  /**< Last known phi value */
    float coherence;            /**< Last known coherence value */
    uint32_t active_instances;  /**< Last known instance count */
    bool is_entrained;          /**< Last known entrainment state */
    uint64_t last_update_us;    /**< Timestamp of last update */
} cached_collective_state_t;

/**
 * @brief Collective-Hub bridge internal structure
 */
struct collective_hub_bridge {
    bridge_base_t base;                        /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    collective_hub_bridge_config_t config;     /**< Bridge configuration */
    cognitive_integration_hub_t hub;           /**< Connected hub */
    collective_cognition_t* collective;        /**< Connected collective */
    cached_collective_state_t cached_state;    /**< Cached state for change detection */
    collective_hub_bridge_stats_t stats;       /**< Bridge statistics */
    bool connected;                            /**< Connection status */
    bool initialized;                          /**< Initialization flag */
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 *
 * @return Current time in microseconds since epoch
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
 * @brief Handle social signal event (unlocked)
 *
 * @param bridge Bridge instance
 * @param event Event data
 * @return 0 on success, -1 on error
 */
static int handle_social_signal_unlocked(
    collective_hub_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!bridge->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_social_signal_unlocked: bridge->collective is NULL");
        return -1;
    }

    /* Update statistics */
    bridge->stats.social_signals_received++;

    /* Social signals can be integrated into collective state through
     * the collective cognition's shared intentionality subsystem.
     * For now, we track the event and let collective handle it. */

    return 0;
}

/**
 * @brief Handle state change event (unlocked)
 *
 * @param bridge Bridge instance
 * @param event Event data
 * @return 0 on success, -1 on error
 */
static int handle_state_change_unlocked(
    collective_hub_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!bridge->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_state_change_unlocked: bridge->collective is NULL");
        return -1;
    }

    /* Update statistics */
    bridge->stats.state_changes_received++;

    /* State changes from individual modules can affect collective state.
     * The collective cognition system may need to update its coordination
     * based on individual module state transitions. */

    return 0;
}

/**
 * @brief Handle decision made event (unlocked)
 *
 * @param bridge Bridge instance
 * @param event Event data
 * @return 0 on success, -1 on error
 */
static int handle_decision_unlocked(
    collective_hub_bridge_t* bridge,
    const cognitive_event_data_t* event
) {
    if (!bridge->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_decision_unlocked: bridge->collective is NULL");
        return -1;
    }

    /* Update statistics */
    bridge->stats.decisions_received++;

    /* Decisions can be propagated through the collective for
     * coordination and consensus building. */

    return 0;
}

/**
 * @brief Query handler for collective state queries
 *
 * @param query Incoming query
 * @param result Output result
 * @param context Bridge instance
 * @return 0 on success, -1 on error
 */
static int collective_query_handler(
    const cognitive_query_t* query,
    cognitive_query_result_t* result,
    void* context
) {
    collective_hub_bridge_t* bridge = (collective_hub_bridge_t*)context;

    if (!bridge || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_query_handler: required parameter is NULL (bridge, query, result)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->collective) {
        result->status = -1;
        strncpy(result->error_message, "Bridge not connected",
                sizeof(result->error_message) - 1);
        bridge->stats.query_errors++;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_query_handler: required parameter is NULL (bridge->connected, bridge->collective)");
        return -1;
    }

    switch (query->query_type) {
        case COG_QUERY_STATUS: {
            /* Return connection status */
            result->status = 0;
            result->result_data = NULL;
            result->result_size = 0;
            break;
        }

        case COG_QUERY_STATE: {
            /* Allocate and return collective state */
            collective_cognition_state_t* state = nimcp_malloc(
                sizeof(collective_cognition_state_t)
            );
            if (!state) {
                result->status = -1;
                strncpy(result->error_message, "Memory allocation failed",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_query_handler: state is NULL");
                return -1;
            }

            int ret = collective_cognition_get_state(bridge->collective, state);
            if (ret != 0) {
                nimcp_free(state);
                state = NULL;
                result->status = -1;
                strncpy(result->error_message, "Failed to get collective state",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_query_handler: validation failed");
                return -1;
            }

            result->status = 0;
            result->result_data = state;
            result->result_size = sizeof(collective_cognition_state_t);
            break;
        }

        case COG_QUERY_METRICS: {
            /* Allocate and return statistics */
            collective_cognition_stats_t* stats = nimcp_malloc(
                sizeof(collective_cognition_stats_t)
            );
            if (!stats) {
                result->status = -1;
                strncpy(result->error_message, "Memory allocation failed",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_query_handler: stats is NULL");
                return -1;
            }

            int ret = collective_cognition_get_stats(bridge->collective, stats);
            if (ret != 0) {
                nimcp_free(stats);
                stats = NULL;
                result->status = -1;
                strncpy(result->error_message, "Failed to get collective stats",
                        sizeof(result->error_message) - 1);
                bridge->stats.query_errors++;
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_query_handler: validation failed");
                return -1;
            }

            result->status = 0;
            result->result_data = stats;
            result->result_size = sizeof(collective_cognition_stats_t);
            break;
        }

        default:
            result->status = -1;
            strncpy(result->error_message, "Unsupported query type",
                    sizeof(result->error_message) - 1);
            bridge->stats.query_errors++;
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_query_handler: operation failed");
            return -1;
    }

    bridge->stats.queries_handled++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int collective_hub_bridge_default_config(collective_hub_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_default_config", 0.0f);


    config->module_id = COLLECTIVE_HUB_MODULE_ID;
    config->phi_change_threshold = DEFAULT_PHI_THRESHOLD;
    config->coherence_change_threshold = DEFAULT_COHERENCE_THRESHOLD;
    config->enable_auto_publish = true;
    config->enable_social_subscription = true;
    config->enable_state_subscription = true;
    config->enable_decision_subscription = true;
    config->enable_query_handling = true;
    config->event_priority = COG_PRIORITY_NORMAL;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

collective_hub_bridge_t* collective_hub_bridge_create(
    const collective_hub_bridge_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_create", 0.0f);


    collective_hub_bridge_t* bridge = nimcp_calloc(1, sizeof(collective_hub_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        collective_hub_bridge_default_config(&bridge->config);
    }

    /* Validate module ID */
    if (bridge->config.module_id == 0) {
        bridge->config.module_id = COLLECTIVE_HUB_MODULE_ID;
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "collective_hub") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_hub_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    memset(&bridge->cached_state, 0, sizeof(cached_collective_state_t));
    memset(&bridge->stats, 0, sizeof(collective_hub_bridge_stats_t));

    bridge->hub = NULL;
    bridge->collective = NULL;
    bridge->connected = false;
    bridge->initialized = true;

    return bridge;
}

void collective_hub_bridge_destroy(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect if still connected */
    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_destroy", 0.0f);


    if (bridge->connected) {
        collective_hub_bridge_disconnect(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    bridge->initialized = false;

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int collective_hub_bridge_connect(
    collective_hub_bridge_t* bridge,
    cognitive_integration_hub_t hub,
    collective_cognition_t* collective
) {
    if (!bridge || !hub || !collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_connect: required parameter is NULL (bridge, hub, collective)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_connect: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_connect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
        return -1;  /* Already connected */
    }

    /* Register module with hub */
    int ret = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_SOCIAL,
        COLLECTIVE_HUB_MODULE_NAME,
        bridge
    );
    if (ret != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
        return -1;
    }

    /* Subscribe to social signals */
    if (bridge->config.enable_social_subscription) {
        ret = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_SOCIAL_SIGNAL,
            (cognitive_event_callback_t)collective_hub_on_event,
            bridge
        );
        if (ret != 0) {
            cognitive_hub_unregister_module(hub, bridge->config.module_id);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
            return -1;
        }
    }

    /* Subscribe to state changes */
    if (bridge->config.enable_state_subscription) {
        ret = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_STATE_CHANGE,
            (cognitive_event_callback_t)collective_hub_on_event,
            bridge
        );
        if (ret != 0) {
            if (bridge->config.enable_social_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_SOCIAL_SIGNAL);
            }
            cognitive_hub_unregister_module(hub, bridge->config.module_id);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
            return -1;
        }
    }

    /* Subscribe to decisions */
    if (bridge->config.enable_decision_subscription) {
        ret = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_DECISION_MADE,
            (cognitive_event_callback_t)collective_hub_on_event,
            bridge
        );
        if (ret != 0) {
            if (bridge->config.enable_state_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_STATE_CHANGE);
            }
            if (bridge->config.enable_social_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_SOCIAL_SIGNAL);
            }
            cognitive_hub_unregister_module(hub, bridge->config.module_id);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
            return -1;
        }
    }

    /* Register query handler */
    if (bridge->config.enable_query_handling) {
        ret = cognitive_hub_register_query_handler(
            hub,
            bridge->config.module_id,
            collective_query_handler
        );
        if (ret != 0) {
            /* Cleanup subscriptions */
            if (bridge->config.enable_decision_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_DECISION_MADE);
            }
            if (bridge->config.enable_state_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_STATE_CHANGE);
            }
            if (bridge->config.enable_social_subscription) {
                cognitive_hub_unsubscribe(hub, bridge->config.module_id,
                                          COG_EVENT_SOCIAL_SIGNAL);
            }
            cognitive_hub_unregister_module(hub, bridge->config.module_id);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_connect: validation failed");
            return -1;
        }
    }

    /* Store references */
    bridge->hub = hub;
    bridge->collective = collective;
    bridge->connected = true;

    /* Initialize cached state from collective */
    collective_cognition_state_t state;
    if (collective_cognition_get_state(collective, &state) == 0) {
        bridge->cached_state.phi = state.phi.phi_total;
        bridge->cached_state.coherence = state.attention_coherence;
        bridge->cached_state.active_instances = state.active_instances;
        bridge->cached_state.is_entrained = state.hyperscanning.is_entrained;
        bridge->cached_state.last_update_us = get_timestamp_us();
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_hub_bridge_disconnect(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_disconnect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_disconnect: bridge->connected is NULL");
        return -1;
    }

    /* Unsubscribe from all events */
    if (bridge->config.enable_decision_subscription) {
        cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                                  COG_EVENT_DECISION_MADE);
    }
    if (bridge->config.enable_state_subscription) {
        cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                                  COG_EVENT_STATE_CHANGE);
    }
    if (bridge->config.enable_social_subscription) {
        cognitive_hub_unsubscribe(bridge->hub, bridge->config.module_id,
                                  COG_EVENT_SOCIAL_SIGNAL);
    }

    /* Unregister module */
    cognitive_hub_unregister_module(bridge->hub, bridge->config.module_id);

    /* Clear references */
    bridge->hub = NULL;
    bridge->collective = NULL;
    bridge->connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool collective_hub_bridge_is_connected(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_is_connected", 0.0f);


    nimcp_mutex_lock(((collective_hub_bridge_t*)bridge)->base.mutex);
    bool connected = bridge->connected;
    nimcp_mutex_unlock(((collective_hub_bridge_t*)bridge)->base.mutex);

    return connected;
}

/* ============================================================================
 * Event Callback API
 * ============================================================================ */

int collective_hub_on_event(
    const void* event_ptr,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_collective_hub_on_ev", 0.0f);


    const cognitive_event_data_t* event = (const cognitive_event_data_t*)event_ptr;
    collective_hub_bridge_t* bridge = (collective_hub_bridge_t*)user_data;

    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_on_event: required parameter is NULL (bridge, event)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_on_event: bridge->connected is NULL");
        return -1;
    }

    /* Update received count */
    bridge->stats.events_received++;

    int result = 0;

    /* Route event based on type */
    switch (event->event_type) {
        case COG_EVENT_SOCIAL_SIGNAL:
            result = handle_social_signal_unlocked(bridge, event);
            break;

        case COG_EVENT_STATE_CHANGE:
            result = handle_state_change_unlocked(bridge, event);
            break;

        case COG_EVENT_DECISION_MADE:
            result = handle_decision_unlocked(bridge, event);
            break;

        default:
            /* Ignore unhandled event types */
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Publishing API
 * ============================================================================ */

int collective_hub_publish_consensus(
    collective_hub_bridge_t* bridge,
    const collective_consensus_data_t* consensus_data
) {
    if (!bridge || !consensus_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_publish_consensus: required parameter is NULL (bridge, consensus_data)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_collective_hub_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_publish_consensus: bridge->connected is NULL");
        return -1;
    }

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_CONSOLIDATION;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = (void*)consensus_data;
    event.payload_size = sizeof(collective_consensus_data_t);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        bridge->hub,
        bridge->config.module_id,
        COG_EVENT_CONSOLIDATION,
        &event
    );

    if (ret == 0) {
        bridge->stats.events_published++;
        bridge->stats.consensus_reached++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int collective_hub_publish_state_change(
    collective_hub_bridge_t* bridge,
    const collective_state_change_t* state_change
) {
    if (!bridge || !state_change) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_publish_state_change: required parameter is NULL (bridge, state_change)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_collective_hub_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_publish_state_change: bridge->connected is NULL");
        return -1;
    }

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = get_timestamp_us();
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = (void*)state_change;
    event.payload_size = sizeof(collective_state_change_t);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        bridge->hub,
        bridge->config.module_id,
        COG_EVENT_STATE_CHANGE,
        &event
    );

    if (ret == 0) {
        bridge->stats.events_published++;

        /* Track phi updates specifically */
        if (state_change->subtype == COLLECTIVE_EVENT_PHI_CHANGED) {
            bridge->stats.phi_updates++;
        }

        /* Update average coherence */
        bridge->stats.avg_coherence = update_average(
            bridge->stats.avg_coherence,
            state_change->coherence_new,
            bridge->stats.events_published
        );
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

int collective_hub_publish_event(
    collective_hub_bridge_t* bridge,
    collective_event_subtype_t subtype,
    const void* data,
    size_t data_size
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_collective_hub_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_publish_event: bridge->connected is NULL");
        return -1;
    }

    /* Determine event type based on subtype */
    cognitive_event_type_t event_type;
    switch (subtype) {
        case COLLECTIVE_EVENT_CONSENSUS_REACHED:
            event_type = COG_EVENT_CONSOLIDATION;
            break;

        case COLLECTIVE_EVENT_PHI_CHANGED:
        case COLLECTIVE_EVENT_COHERENCE_CHANGED:
        case COLLECTIVE_EVENT_ENTRAINMENT_ACHIEVED:
        case COLLECTIVE_EVENT_INSTANCE_JOINED:
        case COLLECTIVE_EVENT_INSTANCE_LEFT:
            event_type = COG_EVENT_STATE_CHANGE;
            break;

        case COLLECTIVE_EVENT_GOAL_PROPOSED:
        case COLLECTIVE_EVENT_GOAL_ACCEPTED:
        case COLLECTIVE_EVENT_GOAL_COMPLETED:
            event_type = COG_EVENT_DECISION_MADE;
            break;

        case COLLECTIVE_EVENT_LOAD_REBALANCED:
            event_type = COG_EVENT_STATE_CHANGE;
            break;

        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_publish_event: operation failed");
            return -1;
    }

    /* Create wrapper structure to include subtype */
    struct {
        collective_event_subtype_t subtype;
        uint64_t timestamp_us;
        size_t data_size;
        /* data follows */
    } event_wrapper;

    event_wrapper.subtype = subtype;
    event_wrapper.timestamp_us = get_timestamp_us();
    event_wrapper.data_size = data_size;

    /* Create event data */
    cognitive_event_data_t event;
    event.event_type = event_type;
    event.source_module_id = bridge->config.module_id;
    event.timestamp = event_wrapper.timestamp_us;
    event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
    event.payload = (void*)&event_wrapper;
    event.payload_size = sizeof(event_wrapper);

    /* Publish to hub */
    int ret = cognitive_hub_publish(
        bridge->hub,
        bridge->config.module_id,
        event_type,
        &event
    );

    if (ret == 0) {
        bridge->stats.events_published++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return ret;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int collective_hub_bridge_update(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected || !bridge->collective) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_update: required parameter is NULL (bridge->connected, bridge->collective)");
        return -1;
    }

    if (!bridge->config.enable_auto_publish) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get current collective state */
    collective_cognition_state_t state;
    int ret = collective_cognition_get_state(bridge->collective, &state);
    if (ret != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_hub_bridge_update: validation failed");
        return -1;
    }

    /* Check for phi change */
    float phi_delta = fabsf(state.phi.phi_total - bridge->cached_state.phi);
    bool phi_changed = phi_delta >= bridge->config.phi_change_threshold;

    /* Check for coherence change */
    float coherence_delta = fabsf(state.attention_coherence - bridge->cached_state.coherence);
    bool coherence_changed = coherence_delta >= bridge->config.coherence_change_threshold;

    /* Check for entrainment change */
    bool entrainment_changed = state.hyperscanning.is_entrained != bridge->cached_state.is_entrained;

    /* Check for instance count change */
    bool instances_changed = state.active_instances != bridge->cached_state.active_instances;

    /* Publish events if changes detected */
    if (phi_changed || coherence_changed || entrainment_changed || instances_changed) {
        collective_state_change_t change;

        /* Determine primary change type */
        if (phi_changed) {
            change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;
        } else if (coherence_changed) {
            change.subtype = COLLECTIVE_EVENT_COHERENCE_CHANGED;
        } else if (entrainment_changed) {
            change.subtype = COLLECTIVE_EVENT_ENTRAINMENT_ACHIEVED;
        } else if (instances_changed) {
            if (state.active_instances > bridge->cached_state.active_instances) {
                change.subtype = COLLECTIVE_EVENT_INSTANCE_JOINED;
            } else {
                change.subtype = COLLECTIVE_EVENT_INSTANCE_LEFT;
            }
        } else {
            change.subtype = COLLECTIVE_EVENT_PHI_CHANGED;
        }

        change.phi_old = bridge->cached_state.phi;
        change.phi_new = state.phi.phi_total;
        change.coherence_old = bridge->cached_state.coherence;
        change.coherence_new = state.attention_coherence;
        change.active_instances = state.active_instances;
        change.is_entrained = state.hyperscanning.is_entrained;
        change.timestamp_us = get_timestamp_us();

        /* Create event data */
        cognitive_event_data_t event;
        event.event_type = COG_EVENT_STATE_CHANGE;
        event.source_module_id = bridge->config.module_id;
        event.timestamp = change.timestamp_us;
        event.priority = (cognitive_event_priority_t)bridge->config.event_priority;
        event.payload = &change;
        event.payload_size = sizeof(collective_state_change_t);

        /* Publish synchronously (already holding mutex) */
        ret = cognitive_hub_publish(
            bridge->hub,
            bridge->config.module_id,
            COG_EVENT_STATE_CHANGE,
            &event
        );

        if (ret == 0) {
            bridge->stats.events_published++;
            if (phi_changed) {
                bridge->stats.phi_updates++;
            }
            bridge->stats.avg_coherence = update_average(
                bridge->stats.avg_coherence,
                state.attention_coherence,
                bridge->stats.events_published
            );
        }
    }

    /* Update cached state */
    bridge->cached_state.phi = state.phi.phi_total;
    bridge->cached_state.coherence = state.attention_coherence;
    bridge->cached_state.active_instances = state.active_instances;
    bridge->cached_state.is_entrained = state.hyperscanning.is_entrained;
    bridge->cached_state.last_update_us = get_timestamp_us();

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int collective_hub_bridge_get_stats(
    const collective_hub_bridge_t* bridge,
    collective_hub_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_get_stats", 0.0f);


    nimcp_mutex_lock(((collective_hub_bridge_t*)bridge)->base.mutex);

    *stats = bridge->stats;

    nimcp_mutex_unlock(((collective_hub_bridge_t*)bridge)->base.mutex);

    return 0;
}

int collective_hub_bridge_reset_stats(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_hub_bridge_reset_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_hub_bridge_heartbeat("collective_h_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(collective_hub_bridge_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

const char* collective_event_subtype_to_string(collective_event_subtype_t subtype) {
    switch (subtype) {
        case COLLECTIVE_EVENT_CONSENSUS_REACHED:
            return "CONSENSUS_REACHED";
        case COLLECTIVE_EVENT_PHI_CHANGED:
            return "PHI_CHANGED";
        case COLLECTIVE_EVENT_COHERENCE_CHANGED:
            return "COHERENCE_CHANGED";
        case COLLECTIVE_EVENT_ENTRAINMENT_ACHIEVED:
            return "ENTRAINMENT_ACHIEVED";
        case COLLECTIVE_EVENT_INSTANCE_JOINED:
            return "INSTANCE_JOINED";
        case COLLECTIVE_EVENT_INSTANCE_LEFT:
            return "INSTANCE_LEFT";
        case COLLECTIVE_EVENT_GOAL_PROPOSED:
            return "GOAL_PROPOSED";
        case COLLECTIVE_EVENT_GOAL_ACCEPTED:
            return "GOAL_ACCEPTED";
        case COLLECTIVE_EVENT_GOAL_COMPLETED:
            return "GOAL_COMPLETED";
        case COLLECTIVE_EVENT_LOAD_REBALANCED:
            return "LOAD_REBALANCED";
        default:
            return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void collective_hub_bridge_set_instance_health_agent(collective_hub_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "collective_hub_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int collective_hub_bridge_training_begin(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_hub_bridge_training_begin: NULL argument");
        return -1;
    }
    collective_hub_bridge_heartbeat_instance(bridge->health_agent, "collective_hub_bridge_training_begin", 0.0f);
    return 0;
}

int collective_hub_bridge_training_end(collective_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_hub_bridge_training_end: NULL argument");
        return -1;
    }
    collective_hub_bridge_heartbeat_instance(bridge->health_agent, "collective_hub_bridge_training_end", 1.0f);
    return 0;
}

int collective_hub_bridge_training_step(collective_hub_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_hub_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_hub_bridge_heartbeat_instance(bridge->health_agent, "collective_hub_bridge_training_step", progress);
    return 0;
}
