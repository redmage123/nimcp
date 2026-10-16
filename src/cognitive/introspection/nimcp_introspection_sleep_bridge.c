/**
 * @file nimcp_introspection_sleep_bridge.c
 * @brief Sleep-Introspection Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/introspection/nimcp_introspection_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(introspection_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_introspection_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_introspection_sleep_bridge_mesh_registry = NULL;

nimcp_error_t introspection_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_introspection_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "introspection_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "introspection_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_introspection_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_introspection_sleep_bridge_mesh_registry = registry;
    return err;
}

void introspection_sleep_bridge_mesh_unregister(void) {
    if (g_introspection_sleep_bridge_mesh_registry && g_introspection_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_introspection_sleep_bridge_mesh_registry, g_introspection_sleep_bridge_mesh_id);
        g_introspection_sleep_bridge_mesh_id = 0;
        g_introspection_sleep_bridge_mesh_registry = NULL;
    }
}


static inline void introspection_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_introspection_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_introspection_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_introspection_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct introspection_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    introspection_sleep_config_t config;
    sleep_system_t sleep_system;
    introspection_sleep_effects_t effects;
    bool callback_registered;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/* Forward declarations */
static void introspection_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update introspection parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect consciousness level immediately
 * - Metacognition requires cortical arousal
 * - Integrated information (Phi) drops during NREM sleep
 */
static void introspection_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    introspection_sleep_bridge_t bridge = (introspection_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Introspection bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update metacognitive accuracy */
    if (bridge->config.enable_metacognition_modulation) {
        float metacog_base = introspection_sleep_metacognition_for_state(new_state);
        bridge->effects.metacognitive_accuracy_factor = metacog_base * bridge->config.modulation_strength +
                                                        (1.0f - bridge->config.modulation_strength);
    }

    /* Update consciousness level */
    if (bridge->config.enable_consciousness_modulation) {
        bridge->effects.consciousness_level_factor = introspection_sleep_consciousness_for_state(new_state);
    }

    /* Introspective access */
    bridge->effects.introspective_access_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                  (new_state == SLEEP_STATE_DROWSY) ? 0.5f :
                                                  (new_state == SLEEP_STATE_REM) ? 0.3f : 0.0f;

    /* Uncertainty awareness */
    bridge->effects.uncertainty_awareness_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                   (new_state == SLEEP_STATE_DROWSY) ? 0.6f : 0.1f;

    /* Update offline status */
    bridge->effects.introspection_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                             new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Introspection modulated: metacog=%.2f, consciousness=%.2f, offline=%d",
                        bridge->effects.metacognitive_accuracy_factor,
                        bridge->effects.consciousness_level_factor,
                        bridge->effects.introspection_offline);
}

int introspection_sleep_default_config(introspection_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    config->enable_metacognition_modulation = true;
    config->enable_consciousness_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

introspection_sleep_bridge_t introspection_sleep_bridge_create(
    const introspection_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_create", 0.0f);


    struct introspection_sleep_bridge_struct* bridge =
        (struct introspection_sleep_bridge_struct*)nimcp_malloc(sizeof(struct introspection_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct introspection_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else introspection_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.metacognitive_accuracy_factor = 1.0f;
    bridge->effects.consciousness_level_factor = 1.0f;
    bridge->effects.introspective_access_factor = 1.0f;
    bridge->effects.uncertainty_awareness_factor = 1.0f;
    bridge->effects.introspection_offline = false;

    if (bridge_base_init(&bridge->base, 0, "introspection_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        introspection_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for introspection bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    introspection_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Introspection-sleep bridge created");
    return bridge;
}

void introspection_sleep_bridge_destroy(introspection_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_destroy", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            introspection_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for introspection bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int introspection_sleep_update(introspection_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_metacognition_modulation) {
        float metacog_base = introspection_sleep_metacognition_for_state(state);
        bridge->effects.metacognitive_accuracy_factor = metacog_base * bridge->config.modulation_strength +
                                                        (1.0f - bridge->config.modulation_strength);
        /* Sleep pressure severely impairs metacognition */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.metacognitive_accuracy_factor *= (1.0f - (pressure - 0.7f) * 0.7f);
        }
    }

    if (bridge->config.enable_consciousness_modulation) {
        bridge->effects.consciousness_level_factor = introspection_sleep_consciousness_for_state(state);
        /* Sleep pressure reduces consciousness */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.consciousness_level_factor *= (1.0f - (pressure - 0.7f) * 0.5f);
        }
    }

    /* Introspective access */
    bridge->effects.introspective_access_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                  (state == SLEEP_STATE_DROWSY) ? 0.5f :
                                                  (state == SLEEP_STATE_REM) ? 0.3f : 0.0f;

    /* Uncertainty awareness */
    bridge->effects.uncertainty_awareness_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                   (state == SLEEP_STATE_DROWSY) ? 0.6f : 0.1f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.8f) {
        bridge->effects.uncertainty_awareness_factor *= 0.4f;  /* Severe impairment */
    }

    bridge->effects.introspection_offline = (state == SLEEP_STATE_DEEP_NREM ||
                                             state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_sleep_get_effects(const introspection_sleep_bridge_t bridge, introspection_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float introspection_sleep_get_metacognitive_accuracy(const introspection_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.metacognitive_accuracy_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool introspection_sleep_is_offline(const introspection_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.introspection_offline;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float introspection_sleep_metacognition_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return INTROSPECTION_SLEEP_METACOG_AWAKE;
        case SLEEP_STATE_DROWSY:     return INTROSPECTION_SLEEP_METACOG_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return INTROSPECTION_SLEEP_METACOG_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return INTROSPECTION_SLEEP_METACOG_DEEP_NREM;
        case SLEEP_STATE_REM:        return INTROSPECTION_SLEEP_METACOG_REM;
        default:                     return INTROSPECTION_SLEEP_METACOG_AWAKE;
    }
}

float introspection_sleep_consciousness_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_introspection_sleep_", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return INTROSPECTION_SLEEP_CONSCIOUSNESS_AWAKE;
        case SLEEP_STATE_DROWSY:     return INTROSPECTION_SLEEP_CONSCIOUSNESS_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return INTROSPECTION_SLEEP_CONSCIOUSNESS_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return INTROSPECTION_SLEEP_CONSCIOUSNESS_DEEP_NREM;
        case SLEEP_STATE_REM:        return INTROSPECTION_SLEEP_CONSCIOUSNESS_REM;
        default:                     return INTROSPECTION_SLEEP_CONSCIOUSNESS_AWAKE;
    }
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about introspection sleep bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int introspection_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    introspection_sleep_bridge_heartbeat("introspectio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Introspection_Sleep_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                introspection_sleep_bridge_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Introspection sleep bridge self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Introspection_Sleep_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Introspection sleep bridge has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Introspection_Sleep_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Introspection sleep bridge has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void introspection_sleep_bridge_set_instance_health_agent(
    introspection_sleep_bridge_t bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int introspection_sleep_bridge_training_begin(introspection_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    introspection_sleep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_sleep_training_begin", 0.0f);
    bridge->effects.metacognitive_accuracy_factor = 0.5f;
    NIMCP_LOGGING_INFO("[INTRO_SLEEP] Training begin: counters reset, baseline state initialized");
    return 0;
}

int introspection_sleep_bridge_training_step(introspection_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    introspection_sleep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_sleep_training_step", progress);
    float lr = bridge->config.modulation_strength;
    float adaptation = lr * (1.0f - progress) * 0.1f;
    bridge->config.modulation_strength = lr + adaptation;
    if (bridge->config.modulation_strength > 1.0f) bridge->config.modulation_strength = 1.0f;
    if (bridge->config.modulation_strength < 0.001f) bridge->config.modulation_strength = 0.001f;
    bridge->effects.metacognitive_accuracy_factor =
        bridge->effects.metacognitive_accuracy_factor * 0.99f + progress * 0.01f;
    return 0;
}

int introspection_sleep_bridge_training_end(introspection_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    introspection_sleep_bridge_heartbeat_instance(bridge->health_agent,
        "intro_sleep_training_end", 1.0f);
    if (bridge->effects.metacognitive_accuracy_factor < 0.0f)
        bridge->effects.metacognitive_accuracy_factor = 0.0f;
    if (bridge->effects.metacognitive_accuracy_factor > 1.0f)
        bridge->effects.metacognitive_accuracy_factor = 1.0f;
    NIMCP_LOGGING_INFO("[INTRO_SLEEP] Training end: metacog_accuracy=%.3f",
        bridge->effects.metacognitive_accuracy_factor);
    return 0;
}
