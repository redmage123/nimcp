/**
 * @file nimcp_hypergraph_kg_bridge.c
 * @brief Hypergraph - Knowledge Graph Bridge Implementation
 */

#include "cognitive/neuro_symbolic/bridges/nimcp_hypergraph_kg_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(hypergraph_kg_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "HYPERGRAPH_KG_BRIDGE"


NIMCP_API hypergraph_kg_bridge_t* hypergraph_kg_bridge_create(void) {
    hypergraph_kg_bridge_t* bridge = nimcp_calloc(1, sizeof(hypergraph_kg_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge_base_init(&bridge->base, BIO_MODULE_HYPERGRAPH_KG_BRIDGE,
                     "hypergraph_kg_bridge");

    bridge->enable_bidirectional_sync = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "hypergraph_kg");
    return bridge;
}

NIMCP_API void hypergraph_kg_bridge_destroy(hypergraph_kg_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypergraph_kg");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}


void hypergraph_kg_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hypergraph_kg_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hypergraph_kg_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_kg_bridge_training_begin: NULL argument");
        return -1;
    }
    hypergraph_kg_bridge_heartbeat_instance(NULL, "hypergraph_kg_bridge_training_begin", 0.0f);
    return 0;
}

int hypergraph_kg_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_kg_bridge_training_end: NULL argument");
        return -1;
    }
    hypergraph_kg_bridge_heartbeat_instance(NULL, "hypergraph_kg_bridge_training_end", 1.0f);
    return 0;
}

int hypergraph_kg_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_kg_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hypergraph_kg_bridge_heartbeat_instance(NULL, "hypergraph_kg_bridge_training_step", progress);
    return 0;
}
