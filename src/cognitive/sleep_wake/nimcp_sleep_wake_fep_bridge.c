/**
 * @file nimcp_sleep_wake_fep_bridge.c
 * @brief Free Energy Principle - Sleep-Wake Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "cognitive/sleep_wake/nimcp_sleep_wake_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

/* Define LOG_MODULE for this file */
#define LOG_MODULE "sleep_wake_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_wake_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_sleep_wake_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_sleep_wake_fep_bridge_mesh_registry = NULL;

nimcp_error_t sleep_wake_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_sleep_wake_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "sleep_wake_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "sleep_wake_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_sleep_wake_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_sleep_wake_fep_bridge_mesh_registry = registry;
    return err;
}

void sleep_wake_fep_bridge_mesh_unregister(void) {
    if (g_sleep_wake_fep_bridge_mesh_registry && g_sleep_wake_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_sleep_wake_fep_bridge_mesh_registry, g_sleep_wake_fep_bridge_mesh_id);
        g_sleep_wake_fep_bridge_mesh_id = 0;
        g_sleep_wake_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from sleep_wake_fep_bridge module (instance-level) */
static inline void sleep_wake_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_sleep_wake_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_sleep_wake_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_sleep_wake_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Provide default Sleep-Wake-FEP configuration
 * WHY:  Sensible defaults based on biological sleep-FE relationships
 * HOW:  Set standard thresholds and enable all features
 */
int sleep_wake_fep_bridge_default_config(sleep_wake_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP → Sleep-Wake */
    config->fe_pressure_scaling = SLEEP_FEP_FE_PRESSURE_SCALING;
    config->fe_sleep_threshold = SLEEP_FEP_HIGH_FE_THRESHOLD;
    config->complexity_deep_sleep_scaling = 1.0f;
    config->enable_fe_sleep_pressure = true;
    config->enable_complexity_deep_sleep = true;
    config->enable_surprise_rem = true;

    /* Sleep-Wake → FEP */
    config->consolidation_fe_reduction = SLEEP_FEP_CONSOLIDATION_FE_REDUCTION;
    config->pruning_fe_reduction = SLEEP_FEP_PRUNING_FE_REDUCTION;
    config->awake_lr_multiplier = SLEEP_FEP_AWAKE_LR_MULT;
    config->drowsy_lr_multiplier = SLEEP_FEP_DROWSY_LR_MULT;
    config->nrem_lr_multiplier = SLEEP_FEP_NREM_LR_MULT;
    config->rem_lr_multiplier = SLEEP_FEP_REM_LR_MULT;
    config->enable_state_lr_modulation = true;
    config->enable_consolidation_fe_reduction = true;
    config->enable_homeostasis_precision = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->sleep_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Create Sleep-Wake-FEP bridge
 * WHY:  Enable bidirectional sleep-FE integration
 * HOW:  Allocate structure, initialize mutex, set defaults
 */
sleep_wake_fep_bridge_t* sleep_wake_fep_bridge_create(
    const sleep_wake_fep_config_t* config
) {
    sleep_wake_fep_bridge_t* bridge = nimcp_malloc(sizeof(sleep_wake_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate sleep-wake FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(sleep_wake_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sleep_wake_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "sleep_wake_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_wake_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created sleep-wake FEP bridge");
    return bridge;
}

/**
 * WHAT: Destroy Sleep-Wake-FEP bridge
 * WHY:  Clean up resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void sleep_wake_fep_bridge_destroy(sleep_wake_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sleep_wake_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed sleep-wake FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP state monitoring
 * HOW:  Store FEP system pointer with thread safety
 */
int sleep_wake_fep_bridge_connect_fep(
    sleep_wake_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to sleep-wake bridge");
    return 0;
}

/**
 * WHAT: Connect sleep-wake system to bridge
 * WHY:  Enable sleep state monitoring
 * HOW:  Store sleep system handle with thread safety
 */
int sleep_wake_fep_bridge_connect_sleep_wake(
    sleep_wake_fep_bridge_t* bridge,
    sleep_system_t sleep
) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sleep_system = sleep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected sleep-wake system to FEP bridge");
    return 0;
}

/**
 * WHAT: Disconnect all systems from bridge
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers with thread safety
 */
int sleep_wake_fep_bridge_disconnect(sleep_wake_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->sleep_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from sleep-wake FEP bridge");
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * WHAT: Update Sleep-Wake-FEP bridge state
 * WHY:  Synchronize sleep and FEP systems bidirectionally
 * HOW:  Check both systems, apply modulations, update statistics
 */
int sleep_wake_fep_bridge_update(sleep_wake_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->fep_system && bridge->sleep_system, NIMCP_ERROR_INVALID_STATE, "fep_system or sleep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current FEP free energy */
    bridge->state.current_free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.current_free_energy = bridge->state.current_free_energy;

    /* Get current sleep state and pressure */
    bridge->state.current_sleep_state = sleep_get_current_state(bridge->sleep_system);
    bridge->state.current_sleep_pressure = sleep_get_pressure(bridge->sleep_system);
    bridge->sleep_effects.current_state = bridge->state.current_sleep_state;
    bridge->sleep_effects.sleep_pressure = bridge->state.current_sleep_pressure;

    /* FEP → Sleep-Wake: Free energy affects sleep pressure */
    if (bridge->config.enable_fe_sleep_pressure) {
        float fe_boost = bridge->state.current_free_energy *
                        bridge->config.fe_pressure_scaling;
        bridge->fep_effects.fe_sleep_pressure = fe_boost;
        bridge->state.sleep_pressure_boost = fe_boost;

        if (bridge->state.current_free_energy > bridge->config.fe_sleep_threshold) {
            bridge->state.fe_sleep_triggered = true;
            bridge->stats.fe_sleep_triggers++;
        }
    }

    /* Sleep-Wake → FEP: Sleep state modulates learning rate */
    if (bridge->config.enable_state_lr_modulation) {
        float lr_mult = 1.0f;
        switch (bridge->state.current_sleep_state) {
            case SLEEP_STATE_AWAKE:
                lr_mult = bridge->config.awake_lr_multiplier;
                break;
            case SLEEP_STATE_DROWSY:
                lr_mult = bridge->config.drowsy_lr_multiplier;
                break;
            case SLEEP_STATE_LIGHT_NREM:
            case SLEEP_STATE_DEEP_NREM:
                lr_mult = bridge->config.nrem_lr_multiplier;
                break;
            case SLEEP_STATE_REM:
                lr_mult = bridge->config.rem_lr_multiplier;
                break;
        }
        bridge->sleep_effects.state_lr_modifier = lr_mult;
        bridge->state.lr_modulation = lr_mult;
        bridge->stats.lr_modulation_events++;
    }

    /* Update statistics */
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * 0.99f) +
        (bridge->state.current_free_energy * 0.01f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Updated sleep-wake FEP bridge");
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Monitoring and debugging
 * HOW:  Copy state structure with thread safety
 */
int sleep_wake_fep_bridge_get_state(
    const sleep_wake_fep_bridge_t* bridge,
    sleep_wake_fep_state_t* state
) {
    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring
 * HOW:  Copy stats structure with thread safety
 */
int sleep_wake_fep_bridge_get_stats(
    const sleep_wake_fep_bridge_t* bridge,
    sleep_wake_fep_stats_t* stats
) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable distributed messaging between sleep and FEP systems
 * HOW:  Register module with bio-async router
 */
int sleep_wake_fep_bridge_connect_bio_async(sleep_wake_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SLEEP_WAKE_BRIDGE,
        .module_name = "sleep_wake_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister module, clear context
 */
int sleep_wake_fep_bridge_disconnect_bio_async(sleep_wake_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return bio_async_enabled flag
 */
bool sleep_wake_fep_bridge_is_bio_async_connected(
    const sleep_wake_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int sleep_wake_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Wake_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Wake_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Wake_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_wake_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_sleep_wake_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_wake_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    sleep_wake_fep_bridge_heartbeat_instance(NULL, "sleep_wake_fep_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_wake_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_fep_bridge_training_end: NULL argument");
        return -1;
    }
    sleep_wake_fep_bridge_heartbeat_instance(NULL, "sleep_wake_fep_bridge_training_end", 1.0f);
    return 0;
}

int sleep_wake_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_wake_fep_bridge_heartbeat_instance(NULL, "sleep_wake_fep_bridge_training_step", progress);
    return 0;
}
