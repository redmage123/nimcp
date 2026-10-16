/**
 * @file nimcp_self_model_sleep_bridge.c
 * @brief Sleep-Self Model Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/self_model/nimcp_self_model_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_model_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_model_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_model_sleep_bridge_mesh_registry = NULL;

nimcp_error_t self_model_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_model_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_model_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_model_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_model_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_model_sleep_bridge_mesh_registry = registry;
    return err;
}

void self_model_sleep_bridge_mesh_unregister(void) {
    if (g_self_model_sleep_bridge_mesh_registry && g_self_model_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_model_sleep_bridge_mesh_registry, g_self_model_sleep_bridge_mesh_id);
        g_self_model_sleep_bridge_mesh_id = 0;
        g_self_model_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_model_sleep_bridge module (instance-level) */
static inline void self_model_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_model_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_model_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_model_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct self_model_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    self_model_sleep_config_t config;
    sleep_system_t sleep_system;
    self_model_sleep_effects_t effects;
    bool callback_registered;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(self_model_sleep_bridge, struct self_model_sleep_bridge_struct)

/* Forward declarations */
static void self_model_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update self-model parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect self-awareness immediately
 * - Consciousness level depends on arousal state
 * - Metacognition requires wakefulness
 */
static void self_model_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    self_model_sleep_bridge_t bridge = (self_model_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Self-model bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update self-awareness */
    if (bridge->config.enable_awareness_modulation) {
        float awareness_base = self_model_sleep_awareness_for_state(new_state);
        bridge->effects.self_awareness_factor = awareness_base * bridge->config.modulation_strength +
                                                (1.0f - bridge->config.modulation_strength);
    }

    /* Update self-reflection */
    if (bridge->config.enable_reflection_modulation) {
        bridge->effects.self_reflection_factor = self_model_sleep_reflection_for_state(new_state);
    }

    /* Metacognition requires full consciousness */
    bridge->effects.metacognition_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                           (new_state == SLEEP_STATE_DROWSY) ? 0.4f : 0.0f;

    /* Self-monitoring accuracy */
    bridge->effects.self_monitoring_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (new_state == SLEEP_STATE_DROWSY) ? 0.5f :
                                             (new_state == SLEEP_STATE_REM) ? 0.2f : 0.0f;

    /* Update offline status */
    bridge->effects.self_awareness_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                              new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Self-model modulated: awareness=%.2f, reflection=%.2f, offline=%d",
                        bridge->effects.self_awareness_factor,
                        bridge->effects.self_reflection_factor,
                        bridge->effects.self_awareness_offline);
}

int self_model_sleep_default_config(self_model_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_def", 0.0f);


    config->enable_awareness_modulation = true;
    config->enable_reflection_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

self_model_sleep_bridge_t self_model_sleep_bridge_create(
    const self_model_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_create", 0.0f);


    struct self_model_sleep_bridge_struct* bridge =
        (struct self_model_sleep_bridge_struct*)nimcp_malloc(sizeof(struct self_model_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct self_model_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else self_model_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.self_awareness_factor = 1.0f;
    bridge->effects.metacognition_factor = 1.0f;
    bridge->effects.self_reflection_factor = 1.0f;
    bridge->effects.self_monitoring_factor = 1.0f;
    bridge->effects.self_awareness_offline = false;

    if (bridge_base_init(&bridge->base, 0, "self_model_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        self_model_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for self-model bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    self_model_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Self-model-sleep bridge created");
    return bridge;
}

void self_model_sleep_bridge_destroy(self_model_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_destroy", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            self_model_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for self-model bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int self_model_sleep_update(self_model_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_upd", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "self_model_sleep_update");
    BRIDGE_LGSS_GATE(bridge, "self_model_sleep_update");

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_awareness_modulation) {
        float awareness_base = self_model_sleep_awareness_for_state(state);
        bridge->effects.self_awareness_factor = awareness_base * bridge->config.modulation_strength +
                                                (1.0f - bridge->config.modulation_strength);
        /* Sleep pressure impairs self-awareness */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.self_awareness_factor *= (1.0f - (pressure - 0.7f) * 0.5f);
        }
    }

    if (bridge->config.enable_reflection_modulation) {
        bridge->effects.self_reflection_factor = self_model_sleep_reflection_for_state(state);
        /* Sleep pressure reduces reflection */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.self_reflection_factor *= (1.0f - (pressure - 0.7f) * 0.6f);
        }
    }

    /* Metacognition requires full consciousness */
    bridge->effects.metacognition_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                           (state == SLEEP_STATE_DROWSY) ? 0.4f : 0.0f;

    /* Self-monitoring accuracy */
    bridge->effects.self_monitoring_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (state == SLEEP_STATE_DROWSY) ? 0.5f :
                                             (state == SLEEP_STATE_REM) ? 0.2f : 0.0f;

    bridge->effects.self_awareness_offline = (state == SLEEP_STATE_DEEP_NREM ||
                                              state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int self_model_sleep_get_effects(const self_model_sleep_bridge_t bridge, self_model_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float self_model_sleep_get_awareness(const self_model_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.self_awareness_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool self_model_sleep_is_offline(const self_model_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_is_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.self_awareness_offline;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float self_model_sleep_awareness_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_awa", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return SELF_MODEL_SLEEP_AWARENESS_AWAKE;
        case SLEEP_STATE_DROWSY:     return SELF_MODEL_SLEEP_AWARENESS_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SELF_MODEL_SLEEP_AWARENESS_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SELF_MODEL_SLEEP_AWARENESS_DEEP_NREM;
        case SLEEP_STATE_REM:        return SELF_MODEL_SLEEP_AWARENESS_REM;
        default:                     return SELF_MODEL_SLEEP_AWARENESS_AWAKE;
    }
}

float self_model_sleep_reflection_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_self_model_sleep_ref", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return SELF_MODEL_SLEEP_REFLECTION_AWAKE;
        case SLEEP_STATE_DROWSY:     return SELF_MODEL_SLEEP_REFLECTION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:  return SELF_MODEL_SLEEP_REFLECTION_NREM;
        case SLEEP_STATE_REM:        return SELF_MODEL_SLEEP_REFLECTION_REM;
        default:                     return SELF_MODEL_SLEEP_REFLECTION_AWAKE;
    }
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about self-model sleep bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int self_model_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    self_model_sleep_bridge_heartbeat("self_model_s_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Model_Sleep_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_model_sleep_bridge_heartbeat("self_model_s_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Self-model sleep bridge self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Model_Sleep_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Self-model sleep bridge has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Model_Sleep_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Self-model sleep bridge has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_model_sleep_bridge_set_instance_health_agent(self_model_sleep_bridge_t bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "self_model_sleep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_model_sleep_bridge_training_begin(self_model_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    self_model_sleep_bridge_heartbeat_instance(bridge->health_agent, "self_model_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int self_model_sleep_bridge_training_end(self_model_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    self_model_sleep_bridge_heartbeat_instance(bridge->health_agent, "self_model_sleep_bridge_training_end", 1.0f);
    return 0;
}

int self_model_sleep_bridge_training_step(self_model_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_model_sleep_bridge_heartbeat_instance(bridge->health_agent, "self_model_sleep_bridge_training_step", progress);
    return 0;
}
