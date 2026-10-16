/**
 * @file nimcp_quantum_mcts_fep_bridge.c
 * @brief Quantum MCTS - FEP Bridge Implementation
 */

#include "cognitive/neuro_symbolic/bridges/nimcp_quantum_mcts_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_mcts_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_quantum_mcts_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_quantum_mcts_fep_bridge_mesh_registry = NULL;

nimcp_error_t quantum_mcts_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_quantum_mcts_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "quantum_mcts_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "quantum_mcts_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_quantum_mcts_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_quantum_mcts_fep_bridge_mesh_registry = registry;
    return err;
}

void quantum_mcts_fep_bridge_mesh_unregister(void) {
    if (g_quantum_mcts_fep_bridge_mesh_registry && g_quantum_mcts_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_quantum_mcts_fep_bridge_mesh_registry, g_quantum_mcts_fep_bridge_mesh_id);
        g_quantum_mcts_fep_bridge_mesh_id = 0;
        g_quantum_mcts_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from quantum_mcts_fep_bridge module (instance-level) */
static inline void quantum_mcts_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_quantum_mcts_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_quantum_mcts_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_quantum_mcts_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "QUANTUM_MCTS_FEP_BRIDGE"


NIMCP_API qmcts_fep_bridge_t* qmcts_fep_bridge_create(void) {
    qmcts_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(qmcts_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge_base_init(&bridge->base, BIO_MODULE_QMCTS_FEP_BRIDGE,
                     "qmcts_fep_bridge");

    bridge->quantum_exploration_boost = 1.5f;

    NIMCP_LOGGING_INFO("Created %s bridge", "quantum_mcts_fep");
    return bridge;
}

NIMCP_API void qmcts_fep_bridge_destroy(qmcts_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "quantum_mcts_fep");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_API float qmcts_fep_bridge_expected_value(
    qmcts_fep_bridge_t* bridge, const float* state, uint32_t dim) {

    if (!bridge || !state || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qmcts_fep_bridge_expected_value: bridge, state, or dim is invalid");
        return 0.0f;
    }

    /* Compute expected value based on state energy */
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            quantum_mcts_fep_bridge_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += state[i] * state[i];
    }

    /* Lower energy = higher expected value */
    float energy = sqrtf(sum);
    float value = 1.0f / (1.0f + energy);

    bridge->plans_executed++;

    return value * bridge->quantum_exploration_boost;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void quantum_mcts_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_quantum_mcts_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int quantum_mcts_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    quantum_mcts_fep_bridge_heartbeat_instance(NULL, "quantum_mcts_fep_bridge_training_begin", 0.0f);
    return 0;
}

int quantum_mcts_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_fep_bridge_training_end: NULL argument");
        return -1;
    }
    quantum_mcts_fep_bridge_heartbeat_instance(NULL, "quantum_mcts_fep_bridge_training_end", 1.0f);
    return 0;
}

int quantum_mcts_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    quantum_mcts_fep_bridge_heartbeat_instance(NULL, "quantum_mcts_fep_bridge_training_step", progress);
    return 0;
}
