/**
 * @file nimcp_semantic_memory_quantum_bridge.h
 * @brief Quantum-accelerated semantic memory retrieval
 *
 * WHAT: Integrates quantum semantic algorithms with semantic memory
 * WHY:  O(sqrt(N)) speedup for similarity-based memory retrieval
 * HOW:  Uses amplitude encoding and Grover search for retrieval
 *
 * BIOLOGICAL INSPIRATION:
 * - Content-addressable memory in hippocampus
 * - Pattern completion from partial cues
 * - Rapid semantic priming effects
 */

#ifndef NIMCP_SEMANTIC_MEMORY_QUANTUM_BRIDGE_H
#define NIMCP_SEMANTIC_MEMORY_QUANTUM_BRIDGE_H

#include "cognitive/memory/nimcp_quantum_semantic.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct semantic_quantum_bridge semantic_quantum_bridge_t;

typedef struct {
    bool enabled;
    uint32_t max_results;
    float similarity_threshold;
    uint32_t embedding_dim;
} semantic_quantum_config_t;

typedef struct {
    uint64_t quantum_retrievals;
    uint64_t classical_fallbacks;
    float avg_speedup;
    float avg_similarity;
} semantic_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

semantic_quantum_config_t semantic_quantum_default_config(void);

semantic_quantum_bridge_t* semantic_quantum_bridge_create(
    const semantic_quantum_config_t* config
);

void semantic_quantum_bridge_destroy(semantic_quantum_bridge_t* bridge);

bool semantic_quantum_bridge_is_enabled(const semantic_quantum_bridge_t* bridge);

void semantic_quantum_bridge_set_enabled(semantic_quantum_bridge_t* bridge, bool enabled);

int semantic_quantum_get_stats(
    const semantic_quantum_bridge_t* bridge,
    semantic_quantum_stats_t* stats
);

void semantic_quantum_reset_stats(semantic_quantum_bridge_t* bridge);

/**
 * WHAT: Get the underlying quantum semantic context
 * WHY:  Allows direct access for advanced operations
 */
quantum_semantic_t semantic_quantum_get_context(semantic_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_SEMANTIC_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

struct semantic_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    semantic_quantum_config_t config;
    quantum_semantic_t qsem;  /* Direct handle */
    semantic_quantum_stats_t stats;
};

semantic_quantum_config_t semantic_quantum_default_config(void) {
    return (semantic_quantum_config_t){
        .enabled = true,
        .max_results = 10,
        .similarity_threshold = 0.5f,
        .embedding_dim = 256
    };
}

semantic_quantum_bridge_t* semantic_quantum_bridge_create(
    const semantic_quantum_config_t* config
) {
    semantic_quantum_bridge_t* bridge = (semantic_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : semantic_quantum_default_config();

    quantum_semantic_config_t qconfig = quantum_semantic_default_config();
    qconfig.similarity_threshold = bridge->config.similarity_threshold;
    qconfig.max_results = bridge->config.max_results;

    bridge->qsem = quantum_semantic_create(&qconfig, bridge->config.embedding_dim);
    if (!bridge->qsem) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void semantic_quantum_bridge_destroy(semantic_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->qsem) quantum_semantic_destroy(bridge->qsem);
    nimcp_free(bridge);
}

bool semantic_quantum_bridge_is_enabled(const semantic_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void semantic_quantum_bridge_set_enabled(semantic_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int semantic_quantum_get_stats(
    const semantic_quantum_bridge_t* bridge,
    semantic_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void semantic_quantum_reset_stats(semantic_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

quantum_semantic_t semantic_quantum_get_context(semantic_quantum_bridge_t* bridge) {
    return bridge ? bridge->qsem : NULL;
}

#endif // NIMCP_SEMANTIC_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SEMANTIC_MEMORY_QUANTUM_BRIDGE_H
