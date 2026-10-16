/**
 * @file nimcp_theory_of_mind_sleep_bridge.c
 * @brief Sleep-Theory of Mind Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/theory_of_mind/nimcp_theory_of_mind_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(theory_of_mind_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_theory_of_mind_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_theory_of_mind_sleep_bridge_mesh_registry = NULL;

nimcp_error_t theory_of_mind_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_theory_of_mind_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "theory_of_mind_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "theory_of_mind_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_theory_of_mind_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_theory_of_mind_sleep_bridge_mesh_registry = registry;
    return err;
}

void theory_of_mind_sleep_bridge_mesh_unregister(void) {
    if (g_theory_of_mind_sleep_bridge_mesh_registry && g_theory_of_mind_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_theory_of_mind_sleep_bridge_mesh_registry, g_theory_of_mind_sleep_bridge_mesh_id);
        g_theory_of_mind_sleep_bridge_mesh_id = 0;
        g_theory_of_mind_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from theory_of_mind_sleep_bridge module (instance-level) */
static inline void theory_of_mind_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_theory_of_mind_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_theory_of_mind_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_theory_of_mind_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct tom_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    tom_sleep_config_t config;
    sleep_system_t sleep_system;
    tom_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void tom_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update theory of mind parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect social cognition immediately
 * - mPFC and TPJ activity depends on arousal level
 * - Sleep deprivation increases egocentric bias
 */
static void tom_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    tom_sleep_bridge_t bridge = (tom_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("ToM bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update mentalizing accuracy */
    if (bridge->config.enable_mentalizing_modulation) {
        float mental_base = tom_sleep_mentalizing_for_state(new_state);
        bridge->effects.mentalizing_accuracy_factor = mental_base * bridge->config.modulation_strength +
                                                      (1.0f - bridge->config.modulation_strength);
    }

    /* Update empathy */
    if (bridge->config.enable_empathy_modulation) {
        bridge->effects.empathy_factor = tom_sleep_empathy_for_state(new_state);
    }

    /* Perspective-taking ability */
    bridge->effects.perspective_taking_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                (new_state == SLEEP_STATE_DROWSY) ? 0.5f :
                                                (new_state == SLEEP_STATE_REM) ? 0.7f : 0.1f;

    /* Egocentric bias increases with fatigue */
    bridge->effects.egocentric_bias_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (new_state == SLEEP_STATE_DROWSY) ? 1.5f : 1.0f;

    /* Update offline status */
    bridge->effects.social_cognition_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                                new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("ToM modulated: mentalizing=%.2f, empathy=%.2f, offline=%d",
                        bridge->effects.mentalizing_accuracy_factor,
                        bridge->effects.empathy_factor,
                        bridge->effects.social_cognition_offline);
}

int tom_sleep_default_config(tom_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_default_co", 0.0f);


    config->enable_mentalizing_modulation = true;
    config->enable_empathy_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

tom_sleep_bridge_t tom_sleep_bridge_create(
    const tom_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_bridge_cre", 0.0f);


    struct tom_sleep_bridge_struct* bridge =
        (struct tom_sleep_bridge_struct*)nimcp_malloc(sizeof(struct tom_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct tom_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else tom_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.mentalizing_accuracy_factor = 1.0f;
    bridge->effects.empathy_factor = 1.0f;
    bridge->effects.perspective_taking_factor = 1.0f;
    bridge->effects.egocentric_bias_factor = 1.0f;
    bridge->effects.social_cognition_offline = false;

    if (bridge_base_init(&bridge->base, 0, "theory_of_mind_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        tom_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for ToM bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    tom_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Theory of Mind-sleep bridge created");
    return bridge;
}

void tom_sleep_bridge_destroy(tom_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_bridge_des", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            tom_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for ToM bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int tom_sleep_update(tom_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_mentalizing_modulation) {
        float mental_base = tom_sleep_mentalizing_for_state(state);
        bridge->effects.mentalizing_accuracy_factor = mental_base * bridge->config.modulation_strength +
                                                      (1.0f - bridge->config.modulation_strength);
        /* Sleep pressure further impairs mentalizing */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.mentalizing_accuracy_factor *= (1.0f - (pressure - 0.7f) * 0.6f);
        }
    }

    if (bridge->config.enable_empathy_modulation) {
        bridge->effects.empathy_factor = tom_sleep_empathy_for_state(state);
        /* Sleep pressure reduces empathy */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.empathy_factor *= (1.0f - (pressure - 0.7f) * 0.4f);
        }
    }

    /* Perspective-taking ability */
    bridge->effects.perspective_taking_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                (state == SLEEP_STATE_DROWSY) ? 0.5f :
                                                (state == SLEEP_STATE_REM) ? 0.7f : 0.1f;

    /* Egocentric bias increases with fatigue */
    bridge->effects.egocentric_bias_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (state == SLEEP_STATE_DROWSY) ? 1.5f : 1.0f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.8f) {
        bridge->effects.egocentric_bias_factor *= 1.8f;  /* Severe egocentric bias */
    }

    bridge->effects.social_cognition_offline = (state == SLEEP_STATE_DEEP_NREM ||
                                                state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_sleep_get_effects(const tom_sleep_bridge_t bridge, tom_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_get_effect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float tom_sleep_get_mentalizing_accuracy(const tom_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_get_mental", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.mentalizing_accuracy_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool tom_sleep_is_offline(const tom_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_is_offline", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.social_cognition_offline;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float tom_sleep_mentalizing_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_mentalizin", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return TOM_SLEEP_MENTALIZING_AWAKE;
        case SLEEP_STATE_DROWSY:     return TOM_SLEEP_MENTALIZING_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return TOM_SLEEP_MENTALIZING_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return TOM_SLEEP_MENTALIZING_DEEP_NREM;
        case SLEEP_STATE_REM:        return TOM_SLEEP_MENTALIZING_REM;
        default:                     return TOM_SLEEP_MENTALIZING_AWAKE;
    }
}

float tom_sleep_empathy_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_empathy_fo", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return TOM_SLEEP_EMPATHY_AWAKE;
        case SLEEP_STATE_DROWSY:     return TOM_SLEEP_EMPATHY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:  return TOM_SLEEP_EMPATHY_NREM;
        case SLEEP_STATE_REM:        return TOM_SLEEP_EMPATHY_REM;
        default:                     return TOM_SLEEP_EMPATHY_AWAKE;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int tom_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_tom_sleep_bridge_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Theory_Of_Mind_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                theory_of_mind_sleep_bridge_heartbeat("theory_of_mi_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Theory_Of_Mind_Sleep_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Theory_Of_Mind_Sleep_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void theory_of_mind_sleep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_theory_of_mind_sleep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int theory_of_mind_sleep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mind_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int theory_of_mind_sleep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mind_sleep_bridge_training_end", 1.0f);
    return 0;
}

int theory_of_mind_sleep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    theory_of_mind_sleep_bridge_heartbeat("theory_of_mind_sleep_bridge_training_step", progress);
    return 0;
}
