/**
 * @file nimcp_mental_health_sleep_bridge.c
 * @brief Sleep-Mental Health Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/mental_health/nimcp_mental_health_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mental_health_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mental_health_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mental_health_sleep_bridge_mesh_registry = NULL;

nimcp_error_t mental_health_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mental_health_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mental_health_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mental_health_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mental_health_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mental_health_sleep_bridge_mesh_registry = registry;
    return err;
}

void mental_health_sleep_bridge_mesh_unregister(void) {
    if (g_mental_health_sleep_bridge_mesh_registry && g_mental_health_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mental_health_sleep_bridge_mesh_registry, g_mental_health_sleep_bridge_mesh_id);
        g_mental_health_sleep_bridge_mesh_id = 0;
        g_mental_health_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mental_health_sleep_bridge module (instance-level) */
static inline void mental_health_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mental_health_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mental_health_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mental_health_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



struct mental_health_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    mental_health_sleep_config_t config;
    sleep_system_t sleep_system;
    mental_health_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void mental_health_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update mental health parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect psychiatric stability immediately
 * - Sleep deprivation rapidly increases disorder risk
 * - NREM sleep actively restores emotional regulation circuits
 */
static void mental_health_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    mental_health_sleep_bridge_t bridge = (mental_health_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Mental health bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update psychiatric stability factor */
    if (bridge->config.enable_stability_modulation) {
        float stability_base = mental_health_sleep_stability_for_state(new_state);
        bridge->effects.psychiatric_stability_factor = stability_base * bridge->config.modulation_strength +
                                                       (1.0f - bridge->config.modulation_strength);
    }

    /* Update disorder risk */
    if (bridge->config.enable_risk_modulation) {
        bridge->effects.disorder_risk_factor = mental_health_sleep_disorder_risk_for_state(new_state);
    }

    /* Emotional regulation improves during sleep */
    bridge->effects.emotional_regulation_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                   (new_state == SLEEP_STATE_DROWSY) ? 0.7f :
                                                   (new_state == SLEEP_STATE_DEEP_NREM) ? 1.2f :
                                                   (new_state == SLEEP_STATE_LIGHT_NREM) ? 1.1f : 0.9f;

    /* Reality testing degrades with sleep deprivation */
    bridge->effects.reality_testing_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (new_state == SLEEP_STATE_DROWSY) ? 0.8f : 1.0f;

    /* Update restoration status */
    bridge->effects.restoration_active = (new_state == SLEEP_STATE_DEEP_NREM ||
                                          new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mental health modulated: stability=%.2f, risk=%.2f, restoration=%d",
                        bridge->effects.psychiatric_stability_factor,
                        bridge->effects.disorder_risk_factor,
                        bridge->effects.restoration_active);
}

int mental_health_sleep_default_config(mental_health_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    config->enable_stability_modulation = true;
    config->enable_risk_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

mental_health_sleep_bridge_t mental_health_sleep_bridge_create(
    const mental_health_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_create", 0.0f);


    struct mental_health_sleep_bridge_struct* bridge =
        (struct mental_health_sleep_bridge_struct*)nimcp_malloc(sizeof(struct mental_health_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct mental_health_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else mental_health_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.psychiatric_stability_factor = 1.0f;
    bridge->effects.disorder_risk_factor = 1.0f;
    bridge->effects.emotional_regulation_factor = 1.0f;
    bridge->effects.reality_testing_factor = 1.0f;
    bridge->effects.restoration_active = false;

    if (bridge_base_init(&bridge->base, 0, "mental_health_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        mental_health_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for mental health bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    mental_health_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Mental health-sleep bridge created");
    return bridge;
}

void mental_health_sleep_bridge_destroy(mental_health_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_destroy", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            mental_health_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for mental health bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int mental_health_sleep_update(mental_health_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_stability_modulation) {
        float stability_base = mental_health_sleep_stability_for_state(state);
        bridge->effects.psychiatric_stability_factor = stability_base * bridge->config.modulation_strength +
                                                       (1.0f - bridge->config.modulation_strength);
        /* High sleep pressure reduces stability */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.psychiatric_stability_factor *= (1.0f - (pressure - 0.7f) * 0.7f);
        }
    }

    if (bridge->config.enable_risk_modulation) {
        float risk_base = mental_health_sleep_disorder_risk_for_state(state);
        bridge->effects.disorder_risk_factor = risk_base;
        /* High sleep pressure increases disorder risk */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.disorder_risk_factor *= (1.0f + (pressure - 0.7f) * 0.5f);
        }
    }

    /* Emotional regulation improves during sleep */
    bridge->effects.emotional_regulation_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                   (state == SLEEP_STATE_DROWSY) ? 0.7f :
                                                   (state == SLEEP_STATE_DEEP_NREM) ? 1.2f :
                                                   (state == SLEEP_STATE_LIGHT_NREM) ? 1.1f : 0.9f;

    /* Reality testing degrades with sleep deprivation */
    bridge->effects.reality_testing_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                             (state == SLEEP_STATE_DROWSY) ? 0.8f : 1.0f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.8f) {
        bridge->effects.reality_testing_factor *= 0.6f;  /* Severe impairment */
    }

    bridge->effects.restoration_active = (state == SLEEP_STATE_DEEP_NREM ||
                                          state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_sleep_get_effects(const mental_health_sleep_bridge_t bridge, mental_health_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float mental_health_sleep_get_stability(const mental_health_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.psychiatric_stability_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool mental_health_sleep_is_restoration_active(const mental_health_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.restoration_active;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float mental_health_sleep_stability_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MENTAL_HEALTH_SLEEP_STABILITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return MENTAL_HEALTH_SLEEP_STABILITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MENTAL_HEALTH_SLEEP_STABILITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MENTAL_HEALTH_SLEEP_STABILITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return MENTAL_HEALTH_SLEEP_STABILITY_REM;
        default:                     return MENTAL_HEALTH_SLEEP_STABILITY_AWAKE;
    }
}

float mental_health_sleep_disorder_risk_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_mental_health_sleep_", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return MENTAL_HEALTH_SLEEP_DISORDER_RISK_AWAKE;
        case SLEEP_STATE_DROWSY:     return MENTAL_HEALTH_SLEEP_DISORDER_RISK_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:  return MENTAL_HEALTH_SLEEP_DISORDER_RISK_NREM;
        case SLEEP_STATE_REM:        return MENTAL_HEALTH_SLEEP_DISORDER_RISK_REM;
        default:                     return MENTAL_HEALTH_SLEEP_DISORDER_RISK_AWAKE;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int mental_health_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    mental_health_sleep_bridge_heartbeat("mental_healt_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mental_Health_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mental_health_sleep_bridge_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mental_Health_Sleep_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mental_Health_Sleep_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mental_health_sleep_bridge_set_instance_health_agent(mental_health_sleep_bridge_t bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "mental_health_sleep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mental_health_sleep_bridge_training_begin(mental_health_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    mental_health_sleep_bridge_heartbeat_instance(bridge->health_agent, "mental_health_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int mental_health_sleep_bridge_training_end(mental_health_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    mental_health_sleep_bridge_heartbeat_instance(bridge->health_agent, "mental_health_sleep_bridge_training_end", 1.0f);
    return 0;
}

int mental_health_sleep_bridge_training_step(mental_health_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mental_health_sleep_bridge_heartbeat_instance(bridge->health_agent, "mental_health_sleep_bridge_training_step", progress);
    return 0;
}
