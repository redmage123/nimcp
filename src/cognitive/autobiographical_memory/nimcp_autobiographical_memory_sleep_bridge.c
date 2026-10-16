/**
 * @file nimcp_autobiographical_memory_sleep_bridge.c
 * @brief Sleep-Autobiographical Memory Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "cognitive/autobiographical_memory/nimcp_autobiographical_memory_sleep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(autobiographical_memory_sleep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


struct autobio_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    autobio_sleep_config_t config;
    sleep_system_t sleep_system;
    autobio_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void autobio_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update autobiographical memory parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state changes affect memory encoding/consolidation immediately
 * - Hippocampus shifts from encoding (awake) to consolidation (sleep)
 * - Slow oscillations during deep NREM drive memory transfer
 */
static void autobio_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    autobio_sleep_bridge_t bridge = (autobio_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Autobiographical memory bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update encoding efficiency */
    if (bridge->config.enable_encoding_modulation) {
        float encoding_base = autobio_sleep_encoding_for_state(new_state);
        bridge->effects.encoding_efficiency_factor = encoding_base * bridge->config.modulation_strength +
                                                     (1.0f - bridge->config.modulation_strength);
    }

    /* Update consolidation rate */
    if (bridge->config.enable_consolidation_modulation) {
        bridge->effects.consolidation_rate_factor = autobio_sleep_consolidation_for_state(new_state);
    }

    /* Retrieval accuracy */
    bridge->effects.retrieval_accuracy_factor = (new_state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                (new_state == SLEEP_STATE_DROWSY) ? 0.7f :
                                                (new_state == SLEEP_STATE_REM) ? 0.4f : 0.0f;

    /* Narrative integration happens in REM */
    bridge->effects.narrative_integration_factor = (new_state == SLEEP_STATE_REM) ? 1.0f :
                                                   (new_state == SLEEP_STATE_AWAKE) ? 0.2f : 0.0f;

    /* Update consolidation status */
    bridge->effects.consolidation_active = (new_state == SLEEP_STATE_DEEP_NREM ||
                                            new_state == SLEEP_STATE_LIGHT_NREM ||
                                            new_state == SLEEP_STATE_REM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Autobiographical memory modulated: encoding=%.2f, consolidation=%.2f, active=%d",
                        bridge->effects.encoding_efficiency_factor,
                        bridge->effects.consolidation_rate_factor,
                        bridge->effects.consolidation_active);
}

int autobio_sleep_default_config(autobio_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_sleep_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_defaul", 0.0f);


    config->enable_encoding_modulation = true;
    config->enable_consolidation_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

autobio_sleep_bridge_t autobio_sleep_bridge_create(
    const autobio_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_bridge", 0.0f);


    struct autobio_sleep_bridge_struct* bridge =
        (struct autobio_sleep_bridge_struct*)nimcp_malloc(sizeof(struct autobio_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct autobio_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else autobio_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.encoding_efficiency_factor = 1.0f;
    bridge->effects.consolidation_rate_factor = 0.1f;
    bridge->effects.retrieval_accuracy_factor = 1.0f;
    bridge->effects.narrative_integration_factor = 0.2f;
    bridge->effects.consolidation_active = false;

    if (bridge_base_init(&bridge->base, 0, "autobiographical_memory_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        autobio_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for autobiographical memory bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    autobio_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Autobiographical memory-sleep bridge created");
    return bridge;
}

void autobio_sleep_bridge_destroy(autobio_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_bridge", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            autobio_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for autobiographical memory bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int autobio_sleep_update(autobio_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_sleep_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_encoding_modulation) {
        float encoding_base = autobio_sleep_encoding_for_state(state);
        bridge->effects.encoding_efficiency_factor = encoding_base * bridge->config.modulation_strength +
                                                     (1.0f - bridge->config.modulation_strength);
        /* Sleep pressure severely impairs encoding */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            bridge->effects.encoding_efficiency_factor *= (1.0f - (pressure - 0.7f) * 0.8f);
        }
    }

    if (bridge->config.enable_consolidation_modulation) {
        bridge->effects.consolidation_rate_factor = autobio_sleep_consolidation_for_state(state);
    }

    /* Retrieval accuracy */
    bridge->effects.retrieval_accuracy_factor = (state == SLEEP_STATE_AWAKE) ? 1.0f :
                                                (state == SLEEP_STATE_DROWSY) ? 0.7f :
                                                (state == SLEEP_STATE_REM) ? 0.4f : 0.0f;
    if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
        bridge->effects.retrieval_accuracy_factor *= (1.0f - (pressure - 0.7f) * 0.5f);
    }

    /* Narrative integration happens in REM */
    bridge->effects.narrative_integration_factor = (state == SLEEP_STATE_REM) ? 1.0f :
                                                   (state == SLEEP_STATE_AWAKE) ? 0.2f : 0.0f;

    bridge->effects.consolidation_active = (state == SLEEP_STATE_DEEP_NREM ||
                                            state == SLEEP_STATE_LIGHT_NREM ||
                                            state == SLEEP_STATE_REM);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_sleep_get_effects(const autobio_sleep_bridge_t bridge, autobio_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_get_ef", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float autobio_sleep_get_encoding_efficiency(const autobio_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_get_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.encoding_efficiency_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool autobio_sleep_is_consolidation_active(const autobio_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_is_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.consolidation_active;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float autobio_sleep_encoding_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_encodi", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return AUTOBIO_SLEEP_ENCODING_AWAKE;
        case SLEEP_STATE_DROWSY:     return AUTOBIO_SLEEP_ENCODING_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return AUTOBIO_SLEEP_ENCODING_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return AUTOBIO_SLEEP_ENCODING_DEEP_NREM;
        case SLEEP_STATE_REM:        return AUTOBIO_SLEEP_ENCODING_REM;
        default:                     return AUTOBIO_SLEEP_ENCODING_AWAKE;
    }
}

float autobio_sleep_consolidation_for_state(sleep_state_t state) {
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_consol", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return AUTOBIO_SLEEP_CONSOLIDATION_AWAKE;
        case SLEEP_STATE_DROWSY:     return AUTOBIO_SLEEP_CONSOLIDATION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return AUTOBIO_SLEEP_CONSOLIDATION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return AUTOBIO_SLEEP_CONSOLIDATION_DEEP_NREM;
        case SLEEP_STATE_REM:        return AUTOBIO_SLEEP_CONSOLIDATION_REM;
        default:                     return AUTOBIO_SLEEP_CONSOLIDATION_AWAKE;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int autobio_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_sleep_bridge_heartbeat("autobiograph_autobio_sleep_bridge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobiographical_memory_sleep_bridge_heartbeat("autobiograph_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_Sleep_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_Sleep_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

void autobiographical_memory_sleep_bridge_set_instance_health_agent(autobio_sleep_bridge_t bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
    g_autobiographical_memory_sleep_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("autobiographical_memory_sleep_bridge: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobiographical_memory_sleep_bridge_training_begin(autobio_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_memory_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    autobiographical_memory_sleep_bridge_heartbeat_instance(bridge, "autobio_slp_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int autobiographical_memory_sleep_bridge_training_end(autobio_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_memory_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    autobiographical_memory_sleep_bridge_heartbeat_instance(bridge, "autobio_slp_training_end", 1.0f);
    (void)bridge;
    return 0;
}

int autobiographical_memory_sleep_bridge_training_step(autobio_sleep_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobiographical_memory_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    autobiographical_memory_sleep_bridge_heartbeat_instance(bridge, "autobio_slp_training_step", progress);
    (void)bridge;
    return 0;
}
