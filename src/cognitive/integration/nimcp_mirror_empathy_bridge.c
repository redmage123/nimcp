/**
 * @file nimcp_mirror_empathy_bridge.c
 * @brief Mirror Neurons - Empathetic Response Cognitive Hub Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting mirror neurons to empathetic response via cognitive hub
 * WHY:  Enable social cognition pipeline from action observation to empathy
 * HOW:  Implements event subscription, publication, and empathy generation
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron system bridges action observation and understanding
 * - Emotional resonance creates shared affective states
 * - Empathy integrates cognitive and affective components
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_empathy_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_empathy_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_empathy_bridge_mesh_registry = NULL;

nimcp_error_t mirror_empathy_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_empathy_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_empathy_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_empathy_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_empathy_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_empathy_bridge_mesh_registry = registry;
    return err;
}

void mirror_empathy_bridge_mesh_unregister(void) {
    if (g_mirror_empathy_bridge_mesh_registry && g_mirror_empathy_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_empathy_bridge_mesh_registry, g_mirror_empathy_bridge_mesh_id);
        g_mirror_empathy_bridge_mesh_id = 0;
        g_mirror_empathy_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_empathy_bridge module (instance-level) */
static inline void mirror_empathy_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_empathy_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_empathy_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_empathy_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "MIRROR_EMPATHY_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MIRROR_EMPATHY_BRIDGE_NAME "MirrorEmpathy"

/* Agent state tracking structure */
typedef struct {
    uint32_t agent_id;
    float empathy_level;
    mirror_emotion_type_t last_emotion;
    float resonance_strength;
    uint64_t last_interaction_ms;
    bool is_active;
} agent_empathy_state_t;

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal mirror-empathy bridge structure
 */
struct mirror_empathy_bridge {
    bridge_base_t base;                       /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    mirror_empathy_config_t config;           /**< Bridge configuration */
    cognitive_integration_hub_t hub;          /**< Connected cognitive hub */

    /* State */
    bool initialized;                         /**< Initialization flag */
    bool registered;                          /**< Registration status */

    /* Agent tracking */
    agent_empathy_state_t* agents;            /**< Agent state array */
    uint32_t agent_count;                     /**< Number of tracked agents */

    /* Callbacks */
    mirror_empathy_action_callback_t action_callback;
    void* action_callback_user_data;
    mirror_empathy_resonance_callback_t resonance_callback;
    void* resonance_callback_user_data;
    mirror_empathy_response_callback_t response_callback;
    void* response_callback_user_data;

    /* Statistics */
    mirror_empathy_stats_t stats;             /**< Bridge statistics */
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

/**
 * @brief Find or create agent state entry
 */
static agent_empathy_state_t* find_or_create_agent(
    mirror_empathy_bridge_t* bridge,
    uint32_t agent_id
) {
    /* Search for existing agent */
    for (uint32_t i = 0; i < bridge->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->agent_count > 256) {
            mirror_empathy_bridge_heartbeat("mirror_empat_loop",
                             (float)(i + 1) / (float)bridge->agent_count);
        }

        if (bridge->agents[i].is_active && bridge->agents[i].agent_id == agent_id) {
            return &bridge->agents[i];
        }
    }

    /* Check capacity */
    if (bridge->agent_count >= bridge->config.agent_capacity) {
        /* Find oldest inactive or least recently used */
        uint64_t oldest_time = UINT64_MAX;
        uint32_t oldest_idx = 0;
        for (uint32_t i = 0; i < bridge->agent_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->agent_count > 256) {
                mirror_empathy_bridge_heartbeat("mirror_empat_loop",
                                 (float)(i + 1) / (float)bridge->agent_count);
            }

            if (!bridge->agents[i].is_active ||
                bridge->agents[i].last_interaction_ms < oldest_time) {
                oldest_time = bridge->agents[i].last_interaction_ms;
                oldest_idx = i;
            }
        }
        /* Reuse oldest slot */
        bridge->agents[oldest_idx].agent_id = agent_id;
        bridge->agents[oldest_idx].empathy_level = 0.0f;
        bridge->agents[oldest_idx].last_emotion = MIRROR_EMOTION_NEUTRAL;
        bridge->agents[oldest_idx].resonance_strength = 0.0f;
        bridge->agents[oldest_idx].last_interaction_ms = get_timestamp_ms();
        bridge->agents[oldest_idx].is_active = true;
        return &bridge->agents[oldest_idx];
    }

    /* Create new entry */
    agent_empathy_state_t* agent = &bridge->agents[bridge->agent_count];
    agent->agent_id = agent_id;
    agent->empathy_level = 0.0f;
    agent->last_emotion = MIRROR_EMOTION_NEUTRAL;
    agent->resonance_strength = 0.0f;
    agent->last_interaction_ms = get_timestamp_ms();
    agent->is_active = true;
    bridge->agent_count++;

    return agent;
}

/**
 * @brief Calculate empathy intensity based on resonance and context
 */
static float calculate_empathy_intensity(
    const mirror_empathy_bridge_t* bridge,
    const agent_empathy_state_t* agent,
    mirror_emotion_type_t emotion,
    float arousal
) {
    /* Base empathy from configuration weights */
    float base_empathy = bridge->config.empathy_generation_weight;

    /* Modulate by emotional arousal */
    float arousal_factor = 0.5f + (arousal * 0.5f);

    /* Modulate by existing rapport (resonance history) */
    float rapport_factor = 0.7f + (agent->resonance_strength * 0.3f);

    /* Negative emotions typically elicit stronger empathy */
    float emotion_factor = 1.0f;
    switch (emotion) {
        case MIRROR_EMOTION_SADNESS:
        case MIRROR_EMOTION_FEAR:
            emotion_factor = 1.2f;
            break;
        case MIRROR_EMOTION_JOY:
            emotion_factor = 0.9f;
            break;
        case MIRROR_EMOTION_ANGER:
            emotion_factor = 0.8f;  /* Slightly reduced for anger */
            break;
        default:
            emotion_factor = 1.0f;
            break;
    }

    float intensity = base_empathy * arousal_factor * rapport_factor * emotion_factor;
    return nimcp_clampf(intensity, 0.0f, 1.0f);
}

/**
 * @brief Generate response suggestion based on empathy context
 */
static void generate_response_suggestion(
    mirror_emotion_type_t emotion,
    float empathy_intensity,
    char* suggestion,
    size_t suggestion_size
) {
    const char* base_response = "";

    switch (emotion) {
        case MIRROR_EMOTION_SADNESS:
            if (empathy_intensity > 0.7f) {
                base_response = "Express deep concern and offer support";
            } else {
                base_response = "Acknowledge their sadness with understanding";
            }
            break;
        case MIRROR_EMOTION_FEAR:
            if (empathy_intensity > 0.7f) {
                base_response = "Provide reassurance and stay close";
            } else {
                base_response = "Acknowledge their concern calmly";
            }
            break;
        case MIRROR_EMOTION_JOY:
            if (empathy_intensity > 0.7f) {
                base_response = "Share in their joy enthusiastically";
            } else {
                base_response = "Express happiness for them";
            }
            break;
        case MIRROR_EMOTION_ANGER:
            if (empathy_intensity > 0.7f) {
                base_response = "Validate feelings while remaining calm";
            } else {
                base_response = "Give space and acknowledge frustration";
            }
            break;
        case MIRROR_EMOTION_SURPRISE:
            base_response = "Share in the unexpected moment";
            break;
        case MIRROR_EMOTION_DISGUST:
            base_response = "Acknowledge their reaction respectfully";
            break;
        default:
            base_response = "Maintain attentive presence";
            break;
    }

    strncpy(suggestion, base_response, suggestion_size - 1);
    suggestion[suggestion_size - 1] = '\0';
}

/* ============================================================================
 * Hub Event Callback (internal)
 * ============================================================================ */

/**
 * @brief Internal callback for cognitive hub events
 */
static int mirror_empathy_on_event(
    const cognitive_event_data_t* event,
    void* user_data
) {
    if (!event || !user_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_on_event: required parameter is NULL (event, user_data)");
        return -1;
    }

    mirror_empathy_bridge_t* bridge = (mirror_empathy_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->stats.events_received++;
    bridge->stats.total_events++;
    bridge->stats.last_event_timestamp = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Dispatch based on event type */
    switch (event->event_type) {
        case COG_EVENT_SOCIAL_SIGNAL: {
            /* Social signal indicates observed action or social cue */
            if (event->payload && event->payload_size >= sizeof(uint32_t)) {
                uint32_t agent_id = *((const uint32_t*)event->payload);

                nimcp_mutex_lock(bridge->base.mutex);

                agent_empathy_state_t* agent = find_or_create_agent(bridge, agent_id);
                agent->last_interaction_ms = get_timestamp_ms();
                bridge->stats.actions_mirrored++;

                /* Invoke action callback if registered */
                mirror_empathy_action_callback_t callback = bridge->action_callback;
                void* cb_data = bridge->action_callback_user_data;

                nimcp_mutex_unlock(bridge->base.mutex);

                if (callback) {
                    mirror_empathy_action_t action;
                    memset(&action, 0, sizeof(action));
                    action.agent_id = agent_id;
                    action.action_type = MIRROR_ACTION_GESTURE;
                    action.understanding_confidence = 0.7f;
                    action.goal_inference_confidence = 0.5f;
                    action.timestamp = get_timestamp_ms();
                    strncpy(action.action_description, "Social signal observed",
                            sizeof(action.action_description) - 1);

                    callback(&action, cb_data);
                }
            }
            break;
        }

        case COG_EVENT_STATE_CHANGE: {
            /* State change might indicate emotional shift */
            if (event->payload && event->payload_size >= sizeof(uint32_t) * 2) {
                const uint32_t* data = (const uint32_t*)event->payload;
                uint32_t agent_id = data[0];
                uint32_t state_value = data[1];

                nimcp_mutex_lock(bridge->base.mutex);

                agent_empathy_state_t* agent = find_or_create_agent(bridge, agent_id);
                agent->last_interaction_ms = get_timestamp_ms();

                /* Interpret state as emotion (simplified mapping) */
                mirror_emotion_type_t emotion = (mirror_emotion_type_t)(state_value % MIRROR_EMOTION_COUNT);
                agent->last_emotion = emotion;
                bridge->stats.emotions_resonated++;

                /* Calculate resonance strength */
                float resonance = bridge->config.emotional_resonance_weight * 0.8f;
                agent->resonance_strength = nimcp_clampf(
                    agent->resonance_strength * 0.7f + resonance * 0.3f,
                    0.0f, 1.0f
                );

                /* Update average */
                bridge->stats.avg_resonance_strength =
                    (bridge->stats.avg_resonance_strength * 0.9f) + (resonance * 0.1f);

                /* Invoke resonance callback if registered */
                mirror_empathy_resonance_callback_t callback = bridge->resonance_callback;
                void* cb_data = bridge->resonance_callback_user_data;

                nimcp_mutex_unlock(bridge->base.mutex);

                if (callback) {
                    mirror_empathy_resonance_t res;
                    memset(&res, 0, sizeof(res));
                    res.agent_id = agent_id;
                    res.emotion_type = emotion;
                    res.valence = (emotion == MIRROR_EMOTION_JOY ||
                                   emotion == MIRROR_EMOTION_SURPRISE) ? 0.5f : -0.3f;
                    res.arousal = 0.6f;
                    res.resonance_strength = resonance;
                    res.timestamp = get_timestamp_ms();

                    callback(&res, cb_data);
                }
            }
            break;
        }

        case COG_EVENT_INPUT_RECEIVED: {
            /* Input might be social stimulus requiring empathetic processing */
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->stats.social_insights++;
            nimcp_mutex_unlock(bridge->base.mutex);
            break;
        }

        case COG_EVENT_OUTPUT_READY: {
            /* Output ready from another module - might trigger response */
            break;
        }

        case COG_EVENT_EMOTION_UPDATE: {
            /* Direct emotion update from emotional system */
            if (event->payload && event->payload_size >= sizeof(uint32_t) + sizeof(float)) {
                const uint8_t* payload = (const uint8_t*)event->payload;
                uint32_t agent_id = *((const uint32_t*)payload);
                float intensity = *((const float*)(payload + sizeof(uint32_t)));

                nimcp_mutex_lock(bridge->base.mutex);

                agent_empathy_state_t* agent = find_or_create_agent(bridge, agent_id);
                agent->empathy_level = nimcp_clampf(
                    agent->empathy_level * 0.8f + intensity * 0.2f,
                    0.0f, 1.0f
                );
                agent->last_interaction_ms = get_timestamp_ms();

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

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int mirror_empathy_bridge_default_config(mirror_empathy_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_default_config", 0.0f);


    config->module_id = MIRROR_EMPATHY_DEFAULT_MODULE_ID;
    config->enable_logging = false;
    config->action_understanding_weight = 0.7f;
    config->emotional_resonance_weight = 0.8f;
    config->empathy_generation_weight = 0.75f;
    config->auto_subscribe_social = true;
    config->auto_subscribe_state = true;
    config->auto_subscribe_input = true;
    config->publish_predictions = true;
    config->event_buffer_size = MIRROR_EMPATHY_MAX_EVENT_BUFFER;
    config->empathy_threshold = 0.3f;
    config->agent_capacity = MIRROR_EMPATHY_MAX_AGENTS;

    return 0;
}

mirror_empathy_bridge_t* mirror_empathy_bridge_create(
    const mirror_empathy_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_create", 0.0f);


    mirror_empathy_bridge_t* bridge = (mirror_empathy_bridge_t*)nimcp_calloc(
        1, sizeof(mirror_empathy_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_empathy_bridge_default_config(&bridge->config);
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "mirror_empathy") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_empathy_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate agent tracking array */
    bridge->agents = (agent_empathy_state_t*)nimcp_calloc(
        bridge->config.agent_capacity, sizeof(agent_empathy_state_t));
    if (!bridge->agents) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_empathy_bridge_create: bridge->agents is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->hub = NULL;
    bridge->registered = false;
    bridge->agent_count = 0;

    /* Initialize callbacks */
    bridge->action_callback = NULL;
    bridge->action_callback_user_data = NULL;
    bridge->resonance_callback = NULL;
    bridge->resonance_callback_user_data = NULL;
    bridge->response_callback = NULL;
    bridge->response_callback_user_data = NULL;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(mirror_empathy_stats_t));

    bridge->initialized = true;

    return bridge;
}

void mirror_empathy_bridge_destroy(mirror_empathy_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Unregister from hub if registered */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_destroy", 0.0f);


    if (bridge->registered) {
        mirror_empathy_bridge_unregister_from_hub(bridge);
    }

    /* Free agent tracking array */
    if (bridge->agents) {
        nimcp_free(bridge->agents);
        bridge->agents = NULL;
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
 * Hub Registration Implementation
 * ============================================================================ */

int mirror_empathy_bridge_register_with_hub(
    mirror_empathy_bridge_t* bridge,
    cognitive_integration_hub_t hub
) {
    if (!bridge || !bridge->initialized || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_bridge_register_with_hub: required parameter is NULL (bridge, bridge->initialized, hub)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_register_with_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_empathy_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Store hub reference */
    bridge->hub = hub;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register module with hub */
    int result = cognitive_hub_register_module(
        hub,
        bridge->config.module_id,
        COG_CATEGORY_SOCIAL,
        MIRROR_EMPATHY_BRIDGE_NAME,
        bridge
    );

    if (result != 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->hub = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_empathy_bridge_register_with_hub: validation failed");
        return -1;
    }

    /* Subscribe to configured event types */
    if (bridge->config.auto_subscribe_social) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_SOCIAL_SIGNAL,
            mirror_empathy_on_event,
            bridge
        );
        /* Non-fatal if subscription fails */
    }

    if (bridge->config.auto_subscribe_state) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_STATE_CHANGE,
            mirror_empathy_on_event,
            bridge
        );
    }

    if (bridge->config.auto_subscribe_input) {
        result = cognitive_hub_subscribe(
            hub,
            bridge->config.module_id,
            COG_EVENT_INPUT_RECEIVED,
            mirror_empathy_on_event,
            bridge
        );
    }

    /* Also subscribe to emotion updates */
    result = cognitive_hub_subscribe(
        hub,
        bridge->config.module_id,
        COG_EVENT_EMOTION_UPDATE,
        mirror_empathy_on_event,
        bridge
    );

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->registered = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mirror_empathy_bridge_unregister_from_hub(mirror_empathy_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_bridge_unregister_from_hub: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_unregister_from_hub", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_bridge_unregister_from_hub: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unsubscribe from events */
    if (bridge->config.auto_subscribe_social) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_SOCIAL_SIGNAL);
    }
    if (bridge->config.auto_subscribe_state) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_STATE_CHANGE);
    }
    if (bridge->config.auto_subscribe_input) {
        cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_INPUT_RECEIVED);
    }
    cognitive_hub_unsubscribe(hub, module_id, COG_EVENT_EMOTION_UPDATE);

    /* Unregister module from hub */
    cognitive_hub_unregister_module(hub, module_id);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub = NULL;
    bridge->registered = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool mirror_empathy_bridge_is_registered(mirror_empathy_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }

    /* Cast away const for mutex lock (safe - only reading) */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_is_registered", 0.0f);


    mirror_empathy_bridge_t* mutable_bridge = (mirror_empathy_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return registered;
}

/* ============================================================================
 * Event Publication Implementation
 * ============================================================================ */

int mirror_empathy_publish_mirrored_action(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_action_t* action
) {
    if (!bridge || !bridge->initialized || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_mirrored_action: required parameter is NULL (bridge, bridge->initialized, action)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_mirrored_action: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Update stats */
    bridge->stats.actions_mirrored++;
    bridge->stats.events_published++;

    /* Update agent state */
    agent_empathy_state_t* agent = find_or_create_agent(bridge, action->agent_id);
    agent->last_interaction_ms = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_SOCIAL_SIGNAL;
    event.source_module_id = module_id;
    event.timestamp = action->timestamp * 1000;  /* Convert to microseconds */
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)action;
    event.payload_size = sizeof(mirror_empathy_action_t);

    /* Publish to hub */
    return cognitive_hub_publish(hub, module_id, COG_EVENT_SOCIAL_SIGNAL, &event);
}

int mirror_empathy_publish_emotional_resonance(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_resonance_t* resonance
) {
    if (!bridge || !bridge->initialized || !resonance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_emotional_resonance: required parameter is NULL (bridge, bridge->initialized, resonance)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_emotional_resonance: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Update stats */
    bridge->stats.emotions_resonated++;
    bridge->stats.events_published++;

    /* Update agent state */
    agent_empathy_state_t* agent = find_or_create_agent(bridge, resonance->agent_id);
    agent->last_emotion = resonance->emotion_type;
    agent->resonance_strength = resonance->resonance_strength;
    agent->last_interaction_ms = get_timestamp_ms();

    /* Update average resonance */
    bridge->stats.avg_resonance_strength =
        (bridge->stats.avg_resonance_strength * 0.9f) +
        (resonance->resonance_strength * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create event data */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = module_id;
    event.timestamp = resonance->timestamp * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)resonance;
    event.payload_size = sizeof(mirror_empathy_resonance_t);

    /* Publish to hub */
    return cognitive_hub_publish(hub, module_id, COG_EVENT_STATE_CHANGE, &event);
}

int mirror_empathy_request_empathetic_response(
    mirror_empathy_bridge_t* bridge,
    uint32_t target_agent_id,
    mirror_emotion_type_t context_emotion,
    mirror_empathy_response_t* response_out
) {
    if (!bridge || !bridge->initialized || !response_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_request_empathetic_response: required parameter is NULL (bridge, bridge->initialized, response_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_reque", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_request_empathetic_response: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Get or create agent state */
    agent_empathy_state_t* agent = find_or_create_agent(bridge, target_agent_id);

    /* Calculate empathy intensity */
    float arousal = 0.6f;  /* Default arousal */
    float empathy_intensity = calculate_empathy_intensity(
        bridge, agent, context_emotion, arousal);

    /* Check threshold */
    if (empathy_intensity < bridge->config.empathy_threshold) {
        /* Below threshold - still provide response but note low empathy */
        empathy_intensity = bridge->config.empathy_threshold;
    }

    /* Generate response */
    memset(response_out, 0, sizeof(mirror_empathy_response_t));
    response_out->target_agent_id = target_agent_id;
    response_out->perceived_emotion = context_emotion;
    response_out->empathy_intensity = empathy_intensity;
    response_out->compassion_level = empathy_intensity * 0.9f;
    response_out->helping_motivation = (empathy_intensity > 0.6f);
    response_out->timestamp = get_timestamp_ms();

    generate_response_suggestion(
        context_emotion,
        empathy_intensity,
        response_out->response_suggestion,
        sizeof(response_out->response_suggestion)
    );

    /* Update agent state */
    agent->empathy_level = empathy_intensity;
    agent->last_emotion = context_emotion;
    agent->last_interaction_ms = response_out->timestamp;

    /* Update stats */
    bridge->stats.empathetic_responses++;
    bridge->stats.events_published++;
    bridge->stats.avg_empathy_intensity =
        (bridge->stats.avg_empathy_intensity * 0.9f) +
        (empathy_intensity * 0.1f);

    /* Invoke response callback if registered */
    mirror_empathy_response_callback_t callback = bridge->response_callback;
    void* cb_data = bridge->response_callback_user_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (callback) {
        callback(response_out, cb_data);
    }

    /* Publish empathetic response event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = module_id;
    event.timestamp = response_out->timestamp * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = response_out;
    event.payload_size = sizeof(mirror_empathy_response_t);

    cognitive_hub_publish(hub, module_id, COG_EVENT_OUTPUT_READY, &event);

    return 0;
}

int mirror_empathy_notify_action_intention(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_intention_t* intention
) {
    if (!bridge || !bridge->initialized || !intention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_notify_action_intention: required parameter is NULL (bridge, bridge->initialized, intention)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_notif", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_notify_action_intention: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    /* Check if predictions are enabled */
    if (!bridge->config.publish_predictions) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Success but not published */
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Update stats */
    bridge->stats.intentions_predicted++;
    bridge->stats.events_published++;

    /* Update agent state */
    agent_empathy_state_t* agent = find_or_create_agent(bridge, intention->agent_id);
    agent->last_interaction_ms = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create prediction event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_DECISION_MADE;  /* Use decision for predictions */
    event.source_module_id = module_id;
    event.timestamp = intention->timestamp * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)intention;
    event.payload_size = sizeof(mirror_empathy_intention_t);

    /* Publish to hub */
    return cognitive_hub_publish(hub, module_id, COG_EVENT_DECISION_MADE, &event);
}

int mirror_empathy_publish_social_understanding(
    mirror_empathy_bridge_t* bridge,
    const mirror_empathy_social_t* understanding
) {
    if (!bridge || !bridge->initialized || !understanding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_social_understanding: required parameter is NULL (bridge, bridge->initialized, understanding)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_publi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered || !bridge->hub) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_publish_social_understanding: required parameter is NULL (bridge->registered, bridge->hub)");
        return -1;
    }

    cognitive_integration_hub_t hub = bridge->hub;
    uint32_t module_id = bridge->config.module_id;

    /* Update stats */
    bridge->stats.social_insights++;
    bridge->stats.events_published++;

    /* Update agent state */
    agent_empathy_state_t* agent = find_or_create_agent(bridge, understanding->agent_id);
    agent->resonance_strength = understanding->rapport_level;
    agent->last_interaction_ms = get_timestamp_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Create social understanding event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_SOCIAL_SIGNAL;
    event.source_module_id = module_id;
    event.timestamp = understanding->timestamp * 1000;
    event.priority = COG_PRIORITY_NORMAL;
    event.payload = (void*)understanding;
    event.payload_size = sizeof(mirror_empathy_social_t);

    /* Publish to hub */
    return cognitive_hub_publish(hub, module_id, COG_EVENT_SOCIAL_SIGNAL, &event);
}

/* ============================================================================
 * Callback Registration Implementation
 * ============================================================================ */

int mirror_empathy_set_action_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_action_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_set_action_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_set_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->action_callback = callback;
    bridge->action_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mirror_empathy_set_resonance_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_resonance_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_set_resonance_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_set_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->resonance_callback = callback;
    bridge->resonance_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mirror_empathy_set_response_callback(
    mirror_empathy_bridge_t* bridge,
    mirror_empathy_response_callback_t callback,
    void* user_data
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_set_response_callback: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_set_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->response_callback = callback;
    bridge->response_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int mirror_empathy_get_agent_state(
    const mirror_empathy_bridge_t* bridge,
    uint32_t agent_id,
    float* empathy_level,
    mirror_emotion_type_t* last_emotion
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_get_agent_state: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_mirror_empathy_get_a", 0.0f);


    mirror_empathy_bridge_t* mutable_bridge = (mirror_empathy_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    /* Search for agent */
    const agent_empathy_state_t* agent = NULL;
    for (uint32_t i = 0; i < bridge->agent_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->agent_count > 256) {
            mirror_empathy_bridge_heartbeat("mirror_empat_loop",
                             (float)(i + 1) / (float)bridge->agent_count);
        }

        if (bridge->agents[i].is_active && bridge->agents[i].agent_id == agent_id) {
            agent = &bridge->agents[i];
            break;
        }
    }

    if (!agent) {
        nimcp_mutex_unlock(mutable_bridge->base.mutex);
        /* Return defaults for unknown agent */
        if (empathy_level) *empathy_level = 0.0f;
        if (last_emotion) *last_emotion = MIRROR_EMOTION_NEUTRAL;
        return 0;
    }

    if (empathy_level) *empathy_level = agent->empathy_level;
    if (last_emotion) *last_emotion = agent->last_emotion;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int mirror_empathy_bridge_get_stats(
    const mirror_empathy_bridge_t* bridge,
    mirror_empathy_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    /* Cast away const for mutex lock */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_get_stats", 0.0f);


    mirror_empathy_bridge_t* mutable_bridge = (mirror_empathy_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int mirror_empathy_bridge_reset_stats(mirror_empathy_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_empathy_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_bridge_heartbeat("mirror_empat_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mirror_empathy_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* mirror_empathy_action_type_to_string(mirror_action_type_t action_type) {
    switch (action_type) {
        case MIRROR_ACTION_GRASP:    return "GRASP";
        case MIRROR_ACTION_FACIAL:   return "FACIAL";
        case MIRROR_ACTION_GESTURE:  return "GESTURE";
        case MIRROR_ACTION_POSTURAL: return "POSTURAL";
        case MIRROR_ACTION_VOCAL:    return "VOCAL";
        default:                     return "UNKNOWN";
    }
}

const char* mirror_empathy_emotion_type_to_string(mirror_emotion_type_t emotion_type) {
    switch (emotion_type) {
        case MIRROR_EMOTION_JOY:      return "JOY";
        case MIRROR_EMOTION_SADNESS:  return "SADNESS";
        case MIRROR_EMOTION_FEAR:     return "FEAR";
        case MIRROR_EMOTION_ANGER:    return "ANGER";
        case MIRROR_EMOTION_SURPRISE: return "SURPRISE";
        case MIRROR_EMOTION_DISGUST:  return "DISGUST";
        case MIRROR_EMOTION_NEUTRAL:  return "NEUTRAL";
        default:                      return "UNKNOWN";
    }
}

const char* mirror_empathy_event_type_to_string(mirror_empathy_event_type_t event_type) {
    switch (event_type) {
        case MIRROR_EMPATHY_EVENT_ACTION_MIRRORED:
            return "ACTION_MIRRORED";
        case MIRROR_EMPATHY_EVENT_EMOTION_RESONATED:
            return "EMOTION_RESONATED";
        case MIRROR_EMPATHY_EVENT_EMPATHY_GENERATED:
            return "EMPATHY_GENERATED";
        case MIRROR_EMPATHY_EVENT_INTENTION_PREDICTED:
            return "INTENTION_PREDICTED";
        case MIRROR_EMPATHY_EVENT_SOCIAL_UNDERSTOOD:
            return "SOCIAL_UNDERSTOOD";
        case MIRROR_EMPATHY_EVENT_COMPASSION_ACTIVATED:
            return "COMPASSION_ACTIVATED";
        case MIRROR_EMPATHY_EVENT_BOND_STRENGTHENED:
            return "BOND_STRENGTHENED";
        default:
            return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mirror_empathy_bridge_set_instance_health_agent(mirror_empathy_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "mirror_empathy_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mirror_empathy_bridge_training_begin(mirror_empathy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_empathy_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_empathy_bridge_training_end(mirror_empathy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_empathy_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_bridge_training_end", 1.0f);
    return 0;
}

int mirror_empathy_bridge_training_step(mirror_empathy_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_empathy_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_bridge_training_step", progress);
    return 0;
}
