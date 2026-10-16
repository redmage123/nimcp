/**
 * @file nimcp_evolutionary_proof_logic_bridge.c
 * @brief Evolutionary Proof - Logic Bridge Implementation
 */

#include "cognitive/neuro_symbolic/bridges/nimcp_evolutionary_proof_logic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(evolutionary_proof_logic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_evolutionary_proof_logic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_evolutionary_proof_logic_bridge_mesh_registry = NULL;

nimcp_error_t evolutionary_proof_logic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_evolutionary_proof_logic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "evolutionary_proof_logic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "evolutionary_proof_logic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_evolutionary_proof_logic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_evolutionary_proof_logic_bridge_mesh_registry = registry;
    return err;
}

void evolutionary_proof_logic_bridge_mesh_unregister(void) {
    if (g_evolutionary_proof_logic_bridge_mesh_registry && g_evolutionary_proof_logic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_evolutionary_proof_logic_bridge_mesh_registry, g_evolutionary_proof_logic_bridge_mesh_id);
        g_evolutionary_proof_logic_bridge_mesh_id = 0;
        g_evolutionary_proof_logic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from evolutionary_proof_logic_bridge module (instance-level) */
static inline void evolutionary_proof_logic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_evolutionary_proof_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_evolutionary_proof_logic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_evolutionary_proof_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "EVOLUTIONARY_PROOF_LOGIC_BRIDGE"


NIMCP_API evoproof_logic_bridge_t* evoproof_logic_bridge_create(void) {
    evoproof_logic_bridge_t* bridge = nimcp_calloc(1, sizeof(evoproof_logic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge_base_init(&bridge->base, BIO_MODULE_EVOPROOF_LOGIC_BRIDGE,
                     "evoproof_logic_bridge");

    bridge->enable_axiom_expansion = true;
    bridge->enable_lemma_caching = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "evolutionary_proof_logic");
    return bridge;
}

NIMCP_API void evoproof_logic_bridge_destroy(evoproof_logic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "evolutionary_proof_logic");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void evolutionary_proof_logic_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_evolutionary_proof_logic_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int evolutionary_proof_logic_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_logic_bridge_training_begin: NULL argument");
        return -1;
    }
    evolutionary_proof_logic_bridge_heartbeat_instance(NULL, "evolutionary_proof_logic_bridge_training_begin", 0.0f);
    return 0;
}

int evolutionary_proof_logic_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_logic_bridge_training_end: NULL argument");
        return -1;
    }
    evolutionary_proof_logic_bridge_heartbeat_instance(NULL, "evolutionary_proof_logic_bridge_training_end", 1.0f);
    return 0;
}

int evolutionary_proof_logic_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_logic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    evolutionary_proof_logic_bridge_heartbeat_instance(NULL, "evolutionary_proof_logic_bridge_training_step", progress);
    return 0;
}
