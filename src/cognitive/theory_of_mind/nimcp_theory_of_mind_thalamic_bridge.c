/**
 * @file nimcp_theory_of_mind_thalamic_bridge.c
 * @brief Theory of Mind-Thalamic Bridge - Health Agent Integration
 *
 * NOTE: The actual bridge implementation lives in cognitive/tom/nimcp_tom_thalamic_bridge.c.
 * This file provides only the health agent setter for the theory_of_mind_thalamic_bridge
 * module identity, used by Phase 8 system-wide health monitoring.
 */

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(theory_of_mind_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_theory_of_mind_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_theory_of_mind_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t theory_of_mind_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_theory_of_mind_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "theory_of_mind_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "theory_of_mind_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_theory_of_mind_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_theory_of_mind_thalamic_bridge_mesh_registry = registry;
    return err;
}

void theory_of_mind_thalamic_bridge_mesh_unregister(void) {
    if (g_theory_of_mind_thalamic_bridge_mesh_registry && g_theory_of_mind_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_theory_of_mind_thalamic_bridge_mesh_registry, g_theory_of_mind_thalamic_bridge_mesh_id);
        g_theory_of_mind_thalamic_bridge_mesh_id = 0;
        g_theory_of_mind_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from theory_of_mind_thalamic_bridge module (instance-level) */
static inline void theory_of_mind_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_theory_of_mind_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_theory_of_mind_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_theory_of_mind_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "THEORY_OF_MIND_THALAMIC_BRIDGE"

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void theory_of_mind_thalamic_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_theory_of_mind_thalamic_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int theory_of_mind_thalamic_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    theory_of_mind_thalamic_bridge_heartbeat("theory_of_mind_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int theory_of_mind_thalamic_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    theory_of_mind_thalamic_bridge_heartbeat("theory_of_mind_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int theory_of_mind_thalamic_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    theory_of_mind_thalamic_bridge_heartbeat("theory_of_mind_thalamic_bridge_training_step", progress);
    return 0;
}
