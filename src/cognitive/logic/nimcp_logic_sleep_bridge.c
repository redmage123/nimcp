/**
 * @file nimcp_logic_sleep_bridge.c
 * @brief Sleep-Logic Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "cognitive/logic/nimcp_logic_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(logic_sleep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_logic_sleep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_logic_sleep_bridge_mesh_registry = NULL;

nimcp_error_t logic_sleep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_logic_sleep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "logic_sleep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "logic_sleep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_logic_sleep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_logic_sleep_bridge_mesh_registry = registry;
    return err;
}

void logic_sleep_bridge_mesh_unregister(void) {
    if (g_logic_sleep_bridge_mesh_registry && g_logic_sleep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_logic_sleep_bridge_mesh_registry, g_logic_sleep_bridge_mesh_id);
        g_logic_sleep_bridge_mesh_id = 0;
        g_logic_sleep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from logic_sleep_bridge module (instance-level) */
static inline void logic_sleep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_logic_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_logic_sleep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_logic_sleep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* Forward declarations */
static void logic_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/* ============================================================================
 * Sleep State Change Callback
 * ============================================================================ */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update logical reasoning parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Logical reasoning degrades rapidly with drowsiness
 * - NREM sleep shifts to consolidation mode (procedural rule strengthening)
 * - REM sleep enables associative rather than deductive reasoning
 */
static void logic_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    logic_sleep_bridge_t* bridge = (logic_sleep_bridge_t*)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Logic bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update inference capacity */
    if (bridge->config.enable_inference_modulation) {
        bridge->effects.inference_capacity_factor =
            logic_sleep_inference_for_state(new_state);
    }

    /* Update deduction accuracy */
    if (bridge->config.enable_accuracy_modulation) {
        bridge->effects.deduction_accuracy_factor =
            logic_sleep_accuracy_for_state(new_state);
    }

    /* Update consistency checking */
    if (bridge->config.enable_consistency_modulation) {
        bridge->effects.consistency_check_factor =
            logic_sleep_consistency_for_state(new_state);
    }

    /* Set functional modes */
    bridge->effects.logic_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                      new_state == SLEEP_STATE_LIGHT_NREM);

    bridge->effects.consolidation_mode = (new_state == SLEEP_STATE_LIGHT_NREM ||
                                          new_state == SLEEP_STATE_DEEP_NREM);

    bridge->effects.creative_mode = (new_state == SLEEP_STATE_REM);

    /* NREM consolidation rate */
    if (bridge->config.enable_nrem_consolidation) {
        if (new_state == SLEEP_STATE_DEEP_NREM) {
            bridge->effects.rule_consolidation_rate = LOGIC_SLEEP_CONSOLIDATION_DEEP;
        } else if (new_state == SLEEP_STATE_LIGHT_NREM) {
            bridge->effects.rule_consolidation_rate = LOGIC_SLEEP_CONSOLIDATION_NREM;
        } else {
            bridge->effects.rule_consolidation_rate = 0.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Logic modulated: inference=%.2f, accuracy=%.2f, consistency=%.2f, offline=%d",
                        bridge->effects.inference_capacity_factor,
                        bridge->effects.deduction_accuracy_factor,
                        bridge->effects.consistency_check_factor,
                        bridge->effects.logic_offline);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * WHAT: Get default configuration
 * WHY:  Provide sensible defaults for typical use
 * HOW:  Initialize all config fields with evidence-based values
 */
int logic_sleep_default_config(logic_sleep_config_t* config)
{
    /* Guard clause */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Feature enables */
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_default_", 0.0f);


    config->enable_inference_modulation = true;
    config->enable_accuracy_modulation = true;
    config->enable_consistency_modulation = true;
    config->enable_nrem_consolidation = true;
    config->enable_rem_creativity = true;

    /* Sensitivity */
    config->modulation_strength = 1.0f;
    config->sleep_pressure_threshold = 0.6f;  /* Start degrading at 60% pressure */

    return 0;
}

/**
 * WHAT: Create logic-sleep bridge
 * WHY:  Initialize integration between sleep and logic systems
 * HOW:  Allocate, configure, register callback, get initial state
 */
logic_sleep_bridge_t* logic_sleep_bridge_create(
    const logic_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    /* Guard clauses */
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("Cannot create logic-sleep bridge: NULL sleep system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__create", 0.0f);


    logic_sleep_bridge_t* bridge =
        (logic_sleep_bridge_t*)nimcp_malloc(sizeof(logic_sleep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate logic-sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_sleep_bridge_create: failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(logic_sleep_bridge_t));

    /* Initialize configuration */
    if (config) {
        bridge->config = *config;
    } else {
        logic_sleep_default_config(&bridge->config);
    }

    /* Store sleep system handle */
    bridge->sleep_system = sleep_system;

    /* Initialize effects to default (awake) state */
    bridge->effects.inference_capacity_factor = 1.0f;
    bridge->effects.deduction_accuracy_factor = 1.0f;
    bridge->effects.consistency_check_factor = 1.0f;
    bridge->effects.logic_offline = false;
    bridge->effects.consolidation_mode = false;
    bridge->effects.creative_mode = false;
    bridge->effects.rule_consolidation_rate = 0.0f;
    bridge->effects.pressure_inference_penalty = 0.0f;
    bridge->effects.pressure_accuracy_penalty = 0.0f;

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "logic_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for logic-sleep bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        logic_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for logic bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    logic_on_sleep_state_change(initial_state, bridge);

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Logic-sleep bridge created");
    return bridge;
}

/**
 * WHAT: Destroy logic-sleep bridge
 * WHY:  Clean up resources and prevent memory leaks
 * HOW:  Unregister callback, destroy mutex, free memory
 */
void logic_sleep_bridge_destroy(logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) return;

    /* Unregister callback if it was registered */
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__destroy", 0.0f);


    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            logic_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for logic bridge");
        }
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        logic_sleep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Update and Query API Implementation
 * ============================================================================ */

/**
 * WHAT: Update logic-sleep bridge state
 * WHY:  Recompute effects based on current sleep state and pressure
 * HOW:  Query sleep system, apply modulation formulas
 */
int logic_sleep_update(logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current sleep state and pressure */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Update inference capacity */
    if (bridge->config.enable_inference_modulation) {
        bridge->effects.inference_capacity_factor =
            logic_sleep_inference_for_state(state);

        /* Sleep pressure impairs inference even when awake */
        if (state == SLEEP_STATE_AWAKE && pressure > bridge->config.sleep_pressure_threshold) {
            float pressure_excess = (pressure - bridge->config.sleep_pressure_threshold) /
                                   (1.0f - bridge->config.sleep_pressure_threshold);
            float penalty = 0.5f * pressure_excess;  /* Up to 50% reduction */
            bridge->effects.pressure_inference_penalty = penalty;
            bridge->effects.inference_capacity_factor *= (1.0f - penalty);
        } else {
            bridge->effects.pressure_inference_penalty = 0.0f;
        }

        /* Apply modulation strength */
        bridge->effects.inference_capacity_factor =
            1.0f - (1.0f - bridge->effects.inference_capacity_factor) *
            bridge->config.modulation_strength;
    }

    /* Update deduction accuracy */
    if (bridge->config.enable_accuracy_modulation) {
        bridge->effects.deduction_accuracy_factor =
            logic_sleep_accuracy_for_state(state);

        /* Sleep pressure reduces accuracy when awake */
        if (state == SLEEP_STATE_AWAKE && pressure > bridge->config.sleep_pressure_threshold) {
            float pressure_excess = (pressure - bridge->config.sleep_pressure_threshold) /
                                   (1.0f - bridge->config.sleep_pressure_threshold);
            float penalty = 0.3f * pressure_excess;  /* Up to 30% reduction */
            bridge->effects.pressure_accuracy_penalty = penalty;
            bridge->effects.deduction_accuracy_factor *= (1.0f - penalty);
        } else {
            bridge->effects.pressure_accuracy_penalty = 0.0f;
        }

        /* Apply modulation strength */
        bridge->effects.deduction_accuracy_factor =
            1.0f - (1.0f - bridge->effects.deduction_accuracy_factor) *
            bridge->config.modulation_strength;
    }

    /* Update consistency checking */
    if (bridge->config.enable_consistency_modulation) {
        bridge->effects.consistency_check_factor =
            logic_sleep_consistency_for_state(state);

        /* Apply modulation strength */
        bridge->effects.consistency_check_factor =
            1.0f - (1.0f - bridge->effects.consistency_check_factor) *
            bridge->config.modulation_strength;
    }

    /* Update functional modes */
    bridge->effects.logic_offline = (state == SLEEP_STATE_DEEP_NREM ||
                                      state == SLEEP_STATE_LIGHT_NREM);

    bridge->effects.consolidation_mode = (state == SLEEP_STATE_LIGHT_NREM ||
                                          state == SLEEP_STATE_DEEP_NREM);

    bridge->effects.creative_mode = (state == SLEEP_STATE_REM);

    /* Update NREM consolidation rate */
    if (bridge->config.enable_nrem_consolidation) {
        if (state == SLEEP_STATE_DEEP_NREM) {
            bridge->effects.rule_consolidation_rate = LOGIC_SLEEP_CONSOLIDATION_DEEP;
        } else if (state == SLEEP_STATE_LIGHT_NREM) {
            bridge->effects.rule_consolidation_rate = LOGIC_SLEEP_CONSOLIDATION_NREM;
        } else {
            bridge->effects.rule_consolidation_rate = 0.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current sleep effects on logic
 * WHY:  Provide effects structure to logic module
 * HOW:  Thread-safe copy of effects
 */
int logic_sleep_get_effects(
    const logic_sleep_bridge_t* bridge,
    logic_sleep_effects_t* effects)
{
    /* Guard clauses */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_get_effe", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get inference capacity factor
 * WHY:  Quick access to most critical parameter
 * HOW:  Thread-safe read
 */
float logic_sleep_get_inference_capacity(logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_get_infe", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.inference_capacity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/**
 * WHAT: Check if logic is offline
 * WHY:  Allow logic module to skip processing during deep sleep
 * HOW:  Thread-safe read of offline flag
 */
bool logic_sleep_is_offline(logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_is_offli", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.logic_offline;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/**
 * WHAT: Check if in consolidation mode
 * WHY:  Signal NREM consolidation to logic system
 * HOW:  Thread-safe read of consolidation flag
 */
bool logic_sleep_is_consolidation_mode(logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_is_conso", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.consolidation_mode;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Sleep State Mapping Functions
 * ============================================================================ */

/**
 * WHAT: Map sleep state to inference capacity
 * WHY:  Encapsulate state-to-performance mapping
 * HOW:  Switch on state, return constant
 */
float logic_sleep_inference_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_inferenc", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return LOGIC_SLEEP_INFERENCE_AWAKE;
        case SLEEP_STATE_DROWSY:     return LOGIC_SLEEP_INFERENCE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return LOGIC_SLEEP_INFERENCE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return LOGIC_SLEEP_INFERENCE_DEEP_NREM;
        case SLEEP_STATE_REM:        return LOGIC_SLEEP_INFERENCE_REM;
        default:                     return LOGIC_SLEEP_INFERENCE_AWAKE;
    }
}

/**
 * WHAT: Map sleep state to deduction accuracy
 * WHY:  Encapsulate state-to-accuracy mapping
 * HOW:  Switch on state, return constant
 */
float logic_sleep_accuracy_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_accuracy", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return LOGIC_SLEEP_ACCURACY_AWAKE;
        case SLEEP_STATE_DROWSY:     return LOGIC_SLEEP_ACCURACY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return LOGIC_SLEEP_ACCURACY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return LOGIC_SLEEP_ACCURACY_DEEP_NREM;
        case SLEEP_STATE_REM:        return LOGIC_SLEEP_ACCURACY_REM;
        default:                     return LOGIC_SLEEP_ACCURACY_AWAKE;
    }
}

/**
 * WHAT: Map sleep state to consistency checking
 * WHY:  Encapsulate state-to-consistency mapping
 * HOW:  Switch on state, return constant
 */
float logic_sleep_consistency_for_state(sleep_state_t state)
{
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_consiste", 0.0f);


    switch (state) {
        case SLEEP_STATE_AWAKE:      return LOGIC_SLEEP_CONSISTENCY_AWAKE;
        case SLEEP_STATE_DROWSY:     return LOGIC_SLEEP_CONSISTENCY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:  return LOGIC_SLEEP_CONSISTENCY_NREM;
        case SLEEP_STATE_REM:        return LOGIC_SLEEP_CONSISTENCY_REM;
        default:                     return LOGIC_SLEEP_CONSISTENCY_AWAKE;
    }
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed logic signals
 * HOW:  Register with bio_router using BIO_MODULE_KNOWLEDGE_SYMBOLIC_LOGIC
 */
int logic_sleep_connect_bio_async(logic_sleep_bridge_t* bridge)
{
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Register with bio-async router */
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_connect_", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_KNOWLEDGE_SYMBOLIC_LOGIC,
        .module_name = "logic_sleep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Logic-sleep bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Not an error if router unavailable */
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int logic_sleep_disconnect_bio_async(logic_sleep_bridge_t* bridge)
{
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Not connected */

    /* Unregister from bio-async router */
    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_disconne", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Logic-sleep bridge disconnected from bio-async router");

    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Allow conditional bio-async usage
 * HOW:  Return bio_async_enabled flag
 */
bool logic_sleep_is_bio_async_connected(const logic_sleep_bridge_t* bridge)
{
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__logic_sleep_is_bio_a", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int logic_sleep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    logic_sleep_bridge_heartbeat("logic_sleep__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Logic_Sleep_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                logic_sleep_bridge_heartbeat("logic_sleep__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Logic_Sleep_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Logic_Sleep_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void logic_sleep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_logic_sleep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int logic_sleep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_sleep_bridge_training_begin: NULL argument");
        return -1;
    }
    logic_sleep_bridge_heartbeat_instance(NULL, "logic_sleep_bridge_training_begin", 0.0f);
    return 0;
}

int logic_sleep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_sleep_bridge_training_end: NULL argument");
        return -1;
    }
    logic_sleep_bridge_heartbeat_instance(NULL, "logic_sleep_bridge_training_end", 1.0f);
    return 0;
}

int logic_sleep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_sleep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    logic_sleep_bridge_heartbeat_instance(NULL, "logic_sleep_bridge_training_step", progress);
    return 0;
}
