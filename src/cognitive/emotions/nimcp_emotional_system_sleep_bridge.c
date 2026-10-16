/**
 * @file nimcp_emotional_system_sleep_bridge.c
 * @brief Sleep-Emotional System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/emotions/nimcp_emotional_system_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotional_system_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotional_system_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotional_system_sleep_bridge_mesh_registry = NULL;

nimcp_error_t emotional_system_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotional_system_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotional_system_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotional_system_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotional_system_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotional_system_sleep_bridge_mesh_registry = registry;
    return err;
}

void emotional_system_sleep_bridge_mesh_unregister(void) {
    if (g_emotional_system_sleep_bridge_mesh_registry && g_emotional_system_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotional_system_sleep_bridge_mesh_registry, g_emotional_system_sleep_bridge_mesh_id);
        g_emotional_system_sleep_bridge_mesh_id = 0;
        g_emotional_system_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotional_system_sleep_bridge module (instance-level) */
static inline void emotional_system_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotional_system_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotional_system_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotional_system_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct emotional_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    emotional_sleep_config_t config;
    sleep_system_t sleep_system;
    emotional_sleep_effects_t effects;
    bool callback_registered;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/* Forward declarations */
static void emotional_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update emotional system parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect emotional regulation immediately
 * - Prefrontal-amygdala connectivity depends on arousal level
 * - Sleep deprivation disinhibits amygdala (60% increase in reactivity)
 */
static void emotional_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    emotional_sleep_bridge_t bridge = (emotional_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Emotional system bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update regulation capacity */
    if (bridge->config.enable_regulation_modulation) {
        float regulation_base = emotional_sleep_regulation_for_state(new_state);
        bridge->effects.regulation_capacity_factor = regulation_base * bridge->config.modulation_strength +
                                                     (1.0f - bridge->config.modulation_strength);
    }

    /* Update emotional reactivity */
    if (bridge->config.enable_reactivity_modulation) {
        bridge->effects.emotional_reactivity_factor = emotional_sleep_reactivity_for_state(new_state);
    }

    /* Positive affect decreases with drowsiness */
    bridge->effects.positive_affect_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (new_state == SLEEP_STATE_DROWSY) ? 0.7f :
                                             (new_state == SLEEP_STATE_REM) ? 0.5f : 0.3f;

    /* Negative affect increases with drowsiness */
    bridge->effects.negative_affect_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (new_state == SLEEP_STATE_DROWSY) ? 1.3f : 0.5f;

    /* Emotional processing active during sleep */
    bridge->effects.emotional_processing_active = (new_state == SLEEP_STATE_DEEP_NREM ||
                                                   new_state == SLEEP_STATE_REM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Emotional system modulated: regulation=%.2f, reactivity=%.2f, processing=%d",
                        bridge->effects.regulation_capacity_factor,
                        bridge->effects.emotional_reactivity_factor,
                        bridge->effects.emotional_processing_active);
}

int emotional_sleep_default_config(emotional_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_defa", 0.0f);


    config->enable_regulation_modulation = true;
    config->enable_reactivity_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

emotional_sleep_bridge_t emotional_sleep_bridge_create(
    const emotional_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_brid", 0.0f);


    struct emotional_sleep_bridge_struct* bridge =
        (struct emotional_sleep_bridge_struct*)nimcp_malloc(sizeof(struct emotional_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct emotional_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else emotional_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.regulation_capacity_factor = 1.0f;
    bridge->effects.emotional_reactivity_factor = 1.0f;
    bridge->effects.positive_affect_factor = 1.0f;
    bridge->effects.negative_affect_factor = 1.0f;
    bridge->effects.emotional_processing_active = false;

    if (bridge_base_init(&bridge->base, 0, "emotional_system_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        emotional_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for emotional system bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    emotional_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Emotional system-sleep bridge created");
    return bridge;
}

void emotional_sleep_bridge_destroy(emotional_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_brid", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            emotional_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for emotional system bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int emotional_sleep_update(emotional_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_upda", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_regulation_modulation) {
        float regulation_base = emotional_sleep_regulation_for_state(state);
        bridge->effects.regulation_capacity_factor = regulation_base * bridge->config.modulation_strength +
                                                     (1.0f - bridge->config.modulation_strength);
        /* Sleep pressure severely impairs regulation */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.regulation_capacity_factor *= (1.0f - (pressure - 0.7f) * 0.8f);
        }
    }

    if (bridge->config.enable_reactivity_modulation) {
        float reactivity_base = emotional_sleep_reactivity_for_state(state);
        bridge->effects.emotional_reactivity_factor = reactivity_base;
        /* Sleep pressure dramatically increases reactivity */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.emotional_reactivity_factor *= (1.0f + (pressure - 0.7f) * 0.6f);
        }
    }

    /* Positive affect decreases with drowsiness */
    bridge->effects.positive_affect_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (state == SLEEP_STATE_DROWSY) ? 0.7f :
                                             (state == SLEEP_STATE_REM) ? 0.5f : 0.3f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
        bridge->effects.positive_affect_factor *= (1.0f - (pressure - 0.7f) * 0.5f);
    }

    /* Negative affect increases with drowsiness */
    bridge->effects.negative_affect_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (state == SLEEP_STATE_DROWSY) ? 1.3f : 0.5f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
        bridge->effects.negative_affect_factor *= (1.0f + (pressure - 0.7f) * 0.6f);
    }

    bridge->effects.emotional_processing_active = (state == SLEEP_STATE_DEEP_NREM ||
                                                   state == SLEEP_STATE_REM);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_sleep_get_effects(const emotional_sleep_bridge_t bridge, emotional_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float emotional_sleep_get_regulation_capacity(const emotional_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.regulation_capacity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool emotional_sleep_is_processing_active(const emotional_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_is_p", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.emotional_processing_active;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float emotional_sleep_regulation_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_regu", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return EMOTIONAL_SLEEP_REGULATION_AWAKE;
        case SLEEP_STATE_DROWSY:     return EMOTIONAL_SLEEP_REGULATION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return EMOTIONAL_SLEEP_REGULATION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return EMOTIONAL_SLEEP_REGULATION_DEEP_NREM;
        case SLEEP_STATE_REM:        return EMOTIONAL_SLEEP_REGULATION_REM;
        default:                     return EMOTIONAL_SLEEP_REGULATION_AWAKE;
    }
}

float emotional_sleep_reactivity_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_emotional_sleep_reac", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return EMOTIONAL_SLEEP_REACTIVITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return EMOTIONAL_SLEEP_REACTIVITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:  return EMOTIONAL_SLEEP_REACTIVITY_NREM;
        case SLEEP_STATE_REM:        return EMOTIONAL_SLEEP_REACTIVITY_REM;
        default:                     return EMOTIONAL_SLEEP_REACTIVITY_AWAKE;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_system_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_sleep_bridge_heartbeat("emotional_sy_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_System_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_system_sleep_bridge_heartbeat("emotional_sy_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_System_Sleep_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_System_Sleep_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotional_system_sleep_bridge_set_instance_health_agent(emotional_sleep_bridge_t bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotional_system_sleep_bridge_training_begin(emotional_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    emotional_system_sleep_bridge_heartbeat_instance(bridge->health_agent, "emotional_sy_training_begin", 0.0f);
    return 0;
}

int emotional_system_sleep_bridge_training_end(emotional_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    emotional_system_sleep_bridge_heartbeat_instance(bridge->health_agent, "emotional_sy_training_end", 1.0f);
    return 0;
}

int emotional_system_sleep_bridge_training_step(emotional_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    emotional_system_sleep_bridge_heartbeat_instance(bridge->health_agent, "emotional_sy_training_step", progress);
    return 0;
}
