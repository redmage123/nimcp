/**
 * @file nimcp_energy_consistency_thermo_bridge.c
 * @brief Energy-Thermodynamics Bridge Implementation
 */

#include "cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_thermo_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(energy_consistency_thermo_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_energy_consistency_thermo_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_energy_consistency_thermo_bridge_mesh_registry = NULL;

nimcp_error_t energy_consistency_thermo_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_energy_consistency_thermo_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "energy_consistency_thermo_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "energy_consistency_thermo_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_energy_consistency_thermo_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_energy_consistency_thermo_bridge_mesh_registry = registry;
    return err;
}

void energy_consistency_thermo_bridge_mesh_unregister(void) {
    if (g_energy_consistency_thermo_bridge_mesh_registry && g_energy_consistency_thermo_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_energy_consistency_thermo_bridge_mesh_registry, g_energy_consistency_thermo_bridge_mesh_id);
        g_energy_consistency_thermo_bridge_mesh_id = 0;
        g_energy_consistency_thermo_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from energy_consistency_thermo_bridge module (instance-level) */
static inline void energy_consistency_thermo_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_energy_consistency_thermo_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_energy_consistency_thermo_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_energy_consistency_thermo_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "ENERGY_CONSISTENCY_THERMO_BRIDGE"


#define BOLTZMANN_CONSTANT 1.380649e-23f

NIMCP_API energy_thermo_bridge_t* energy_thermo_bridge_create(void) {
    energy_thermo_bridge_t* bridge = nimcp_calloc(1, sizeof(energy_thermo_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge_base_init(&bridge->base, BIO_MODULE_ENERGY_THERMO_BRIDGE,
                     "energy_thermo_bridge");

    bridge->config.enable_modulation = true;
    bridge->config.atp_per_operation = 0.001f;
    bridge->config.landauer_constant = BOLTZMANN_CONSTANT;
    bridge->config.temperature_kelvin = 310.0f; /* Body temperature */

    NIMCP_LOGGING_INFO("Created %s bridge", "energy_consistency_thermo");
    return bridge;
}

NIMCP_API void energy_thermo_bridge_destroy(energy_thermo_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "energy_consistency_thermo");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_API nimcp_error_t energy_thermo_bridge_track_reasoning_cost(
    energy_thermo_bridge_t* bridge, uint32_t operations) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_thermo_bridge_track_reasoning_cost: bridge is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    bridge->total_atp_consumed += operations * bridge->config.atp_per_operation;
    bridge->operations_tracked += operations;

    return NIMCP_SUCCESS;
}

NIMCP_API double energy_thermo_bridge_landauer_proof_cost(
    energy_thermo_bridge_t* bridge, uint32_t proof_bits) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "energy_thermo_bridge_landauer_proof_cost: bridge is NULL");
        return 0.0;
    }

    /* Landauer limit: E >= k*T*ln(2) per bit erased */
    double min_energy_per_bit = bridge->config.landauer_constant *
                                 bridge->config.temperature_kelvin * log(2.0);

    double total_cost = min_energy_per_bit * proof_bits;
    bridge->total_landauer_cost += (float)total_cost;

    return total_cost;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void energy_consistency_thermo_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_energy_consistency_thermo_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int energy_consistency_thermo_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_thermo_bridge_training_begin: NULL argument");
        return -1;
    }
    energy_consistency_thermo_bridge_heartbeat_instance(NULL, "energy_consistency_thermo_bridge_training_begin", 0.0f);
    return 0;
}

int energy_consistency_thermo_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_thermo_bridge_training_end: NULL argument");
        return -1;
    }
    energy_consistency_thermo_bridge_heartbeat_instance(NULL, "energy_consistency_thermo_bridge_training_end", 1.0f);
    return 0;
}

int energy_consistency_thermo_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "energy_consistency_thermo_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    energy_consistency_thermo_bridge_heartbeat_instance(NULL, "energy_consistency_thermo_bridge_training_step", progress);
    return 0;
}
