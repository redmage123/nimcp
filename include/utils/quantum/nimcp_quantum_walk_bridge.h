/**
 * @file nimcp_quantum_walk_bridge.h
 * @brief Quantum walk integration for graph-based algorithms
 *
 * WHAT: Integrates ternary quantum walk with graph utilities
 * WHY:  O(√N) speedup for graph search and traversal
 * HOW:  Amplitude propagation along ternary-weighted edges
 *
 * BIOLOGICAL INSPIRATION:
 * - Neural signal propagation in connectome
 * - Spreading activation in semantic networks
 * - Inhibitory/excitatory balance in cortical circuits
 */

#ifndef NIMCP_QUANTUM_WALK_BRIDGE_H
#define NIMCP_QUANTUM_WALK_BRIDGE_H

#include "utils/quantum/nimcp_quantum_walk_ternary.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct quantum_walk_bridge quantum_walk_bridge_t;

typedef struct {
    bool enabled;
    uint32_t max_nodes;
    uint32_t default_steps;
    float interference_threshold;
    bool track_path;
} quantum_walk_bridge_config_t;

typedef struct {
    uint64_t walks_performed;
    uint64_t nodes_visited;
    float avg_steps;
    float avg_final_amplitude;
    float avg_interference;
} quantum_walk_bridge_stats_t;

//=============================================================================
// API
//=============================================================================

quantum_walk_bridge_config_t quantum_walk_bridge_default_config(void);

quantum_walk_bridge_t* quantum_walk_bridge_create(
    const quantum_walk_bridge_config_t* config
);

void quantum_walk_bridge_destroy(quantum_walk_bridge_t* bridge);

bool quantum_walk_bridge_is_enabled(const quantum_walk_bridge_t* bridge);

void quantum_walk_bridge_set_enabled(quantum_walk_bridge_t* bridge, bool enabled);

/**
 * WHAT: Initialize walker on graph
 */
int quantum_walk_bridge_init(
    quantum_walk_bridge_t* bridge,
    uint32_t n_nodes
);

/**
 * WHAT: Add ternary edge to graph
 * @param weight -1 (inhibitory), 0 (none), +1 (excitatory)
 */
int quantum_walk_bridge_add_edge(
    quantum_walk_bridge_t* bridge,
    uint32_t from,
    uint32_t to,
    int8_t weight
);

/**
 * WHAT: Set starting node for walk
 */
int quantum_walk_bridge_set_start(
    quantum_walk_bridge_t* bridge,
    uint32_t node
);

/**
 * WHAT: Perform quantum walk steps
 */
int quantum_walk_bridge_step(
    quantum_walk_bridge_t* bridge,
    uint32_t n_steps
);

/**
 * WHAT: Measure walker position (collapse to node)
 */
int quantum_walk_bridge_measure(
    quantum_walk_bridge_t* bridge,
    uint32_t* node_out,
    float* probability_out
);

/**
 * WHAT: Get amplitude at specific node
 */
float quantum_walk_bridge_get_amplitude(
    quantum_walk_bridge_t* bridge,
    uint32_t node
);

/**
 * WHAT: Get all node amplitudes
 */
int quantum_walk_bridge_get_distribution(
    quantum_walk_bridge_t* bridge,
    float* amplitudes,
    uint32_t n_nodes
);

/**
 * WHAT: Search for target node using quantum walk
 */
bool quantum_walk_bridge_search(
    quantum_walk_bridge_t* bridge,
    uint32_t target,
    uint32_t max_steps
);

/**
 * WHAT: Reset walker state
 */
void quantum_walk_bridge_reset(quantum_walk_bridge_t* bridge);

int quantum_walk_bridge_get_stats(
    const quantum_walk_bridge_t* bridge,
    quantum_walk_bridge_stats_t* stats
);

void quantum_walk_bridge_reset_stats(quantum_walk_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_QUANTUM_WALK_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

struct quantum_walk_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    quantum_walk_bridge_config_t config;
    trit_walker_graph_t* walker;
    trit_matrix_t* adjacency;           /**< Ternary adjacency matrix for graph */
    quantum_walk_bridge_stats_t stats;
    uint32_t n_nodes;
    uint32_t start_node;
    bool initialized;
};

quantum_walk_bridge_config_t quantum_walk_bridge_default_config(void) {
    return (quantum_walk_bridge_config_t){
        .enabled = true,
        .max_nodes = 1000,
        .default_steps = 10,
        .interference_threshold = 0.1f,
        .track_path = true
    };
}

quantum_walk_bridge_t* quantum_walk_bridge_create(
    const quantum_walk_bridge_config_t* config
) {
    quantum_walk_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : quantum_walk_bridge_default_config();

    return bridge;
}

void quantum_walk_bridge_destroy(quantum_walk_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->walker) trit_walker_graph_destroy(bridge->walker);
    if (bridge->adjacency) trit_matrix_destroy(bridge->adjacency);
    nimcp_free(bridge);
}

bool quantum_walk_bridge_is_enabled(const quantum_walk_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void quantum_walk_bridge_set_enabled(quantum_walk_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int quantum_walk_bridge_init(
    quantum_walk_bridge_t* bridge,
    uint32_t n_nodes
) {
    if (!bridge) return -1;

    /* Clean up existing state */
    if (bridge->walker) {
        trit_walker_graph_destroy(bridge->walker);
        bridge->walker = NULL;
    }
    if (bridge->adjacency) {
        trit_matrix_destroy(bridge->adjacency);
        bridge->adjacency = NULL;
    }

    /* Create ternary adjacency matrix */
    bridge->adjacency = trit_matrix_create(n_nodes, n_nodes, TERNARY_PACK_NONE);
    if (!bridge->adjacency) return -1;

    /* Initialize all edges to UNKNOWN (no edge) */
    trit_matrix_fill(bridge->adjacency, TRIT_UNKNOWN);

    bridge->n_nodes = n_nodes;
    bridge->walker = NULL;  /* Walker created after edges added */
    bridge->initialized = true;
    return 0;
}

int quantum_walk_bridge_add_edge(
    quantum_walk_bridge_t* bridge,
    uint32_t from,
    uint32_t to,
    int8_t weight
) {
    if (!bridge || !bridge->adjacency) return -1;
    if (from >= bridge->n_nodes || to >= bridge->n_nodes) return -1;

    /* Clamp weight to ternary value */
    trit_t trit_weight = TRIT_UNKNOWN;
    if (weight < 0) {
        trit_weight = TRIT_NEGATIVE;  /* Inhibitory */
    } else if (weight > 0) {
        trit_weight = TRIT_POSITIVE;  /* Excitatory */
    }

    /* Set edge in adjacency matrix */
    trit_matrix_set(bridge->adjacency, from, to, trit_weight);

    return 0;
}

int quantum_walk_bridge_set_start(
    quantum_walk_bridge_t* bridge,
    uint32_t node
) {
    if (!bridge || !bridge->adjacency) return -1;
    if (node >= bridge->n_nodes) return -1;

    /* Create walker from adjacency matrix if not yet created */
    if (!bridge->walker) {
        bridge->walker = trit_walker_graph_create(bridge->adjacency);
        if (!bridge->walker) return -1;
    }

    trit_walker_graph_init(bridge->walker, node);
    bridge->start_node = node;
    return 0;
}

int quantum_walk_bridge_step(
    quantum_walk_bridge_t* bridge,
    uint32_t n_steps
) {
    if (!bridge || !bridge->walker) return -1;

    trit_walker_graph_run(bridge->walker, n_steps);

    bridge->stats.walks_performed++;
    bridge->stats.avg_steps =
        (bridge->stats.avg_steps * (bridge->stats.walks_performed - 1) + n_steps)
        / bridge->stats.walks_performed;

    return 0;
}

int quantum_walk_bridge_measure(
    quantum_walk_bridge_t* bridge,
    uint32_t* node_out,
    float* probability_out
) {
    if (!bridge || !bridge->walker || !node_out) return -1;

    float max_amp = 0.0f;
    uint32_t node = trit_walker_graph_max_node(bridge->walker, &max_amp);
    *node_out = node;

    if (probability_out) {
        *probability_out = max_amp * max_amp;  /* Probability is amplitude squared */
    }

    bridge->stats.nodes_visited++;
    return 0;
}

float quantum_walk_bridge_get_amplitude(
    quantum_walk_bridge_t* bridge,
    uint32_t node
) {
    if (!bridge || !bridge->walker) return 0.0f;
    return trit_walker_graph_get_amplitude(bridge->walker, node);
}

int quantum_walk_bridge_get_distribution(
    quantum_walk_bridge_t* bridge,
    float* amplitudes,
    uint32_t n_nodes
) {
    if (!bridge || !bridge->walker || !amplitudes) return -1;

    for (uint32_t i = 0; i < n_nodes && i < bridge->n_nodes; i++) {
        amplitudes[i] = trit_walker_graph_get_amplitude(bridge->walker, i);
    }

    return 0;
}

bool quantum_walk_bridge_search(
    quantum_walk_bridge_t* bridge,
    uint32_t target,
    uint32_t max_steps
) {
    if (!bridge) return false;
    if (!bridge->walker && bridge->adjacency) {
        /* Create walker if not yet created */
        bridge->walker = trit_walker_graph_create(bridge->adjacency);
        if (!bridge->walker) return false;
    }
    if (!bridge->walker) return false;

    uint32_t steps_taken = 0;
    return trit_walker_graph_search(
        bridge->walker,
        bridge->start_node,
        target,
        max_steps,
        &steps_taken
    );
}

void quantum_walk_bridge_reset(quantum_walk_bridge_t* bridge) {
    if (bridge && bridge->walker) {
        trit_walker_graph_init(bridge->walker, bridge->start_node);
    }
}

int quantum_walk_bridge_get_stats(
    const quantum_walk_bridge_t* bridge,
    quantum_walk_bridge_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void quantum_walk_bridge_reset_stats(quantum_walk_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_QUANTUM_WALK_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUANTUM_WALK_BRIDGE_H
