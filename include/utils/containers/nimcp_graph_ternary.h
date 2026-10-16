//=============================================================================
// nimcp_graph_ternary.h - Ternary Edge Weights for Network Graphs
//=============================================================================
/**
 * @file nimcp_graph_ternary.h
 * @brief Ternary edge weight support for network topology graphs
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary edge weights {STRONG=+1, WEAK=0, ABSENT=-1} for graphs
 * WHY:  Memory-efficient edge classification with semantic meaning
 * HOW:  Extend NimcpGraph with ternary weight operations
 *
 * EDGE SEMANTICS:
 * | Value | Name   | Meaning                              |
 * |-------|--------|--------------------------------------|
 * | +1    | STRONG | Strong/reliable connection           |
 * |  0    | WEAK   | Weak/unreliable connection           |
 * | -1    | ABSENT | No connection / blocked path         |
 *
 * USE CASES:
 * - Neural network topology (excitatory/inhibitory/absent)
 * - P2P network reliability (good/degraded/failed)
 * - Trust networks (trusted/neutral/untrusted)
 *
 * USAGE:
 * ```c
 * // Create graph with ternary weights
 * NimcpTernaryGraph* tgraph = nimcp_ternary_graph_create();
 *
 * // Add vertices
 * uint32_t v0 = nimcp_ternary_graph_add_vertex(tgraph, 0x1234, 0, 0, 0, 0);
 * uint32_t v1 = nimcp_ternary_graph_add_vertex(tgraph, 0x5678, 1, 0, 0, 0);
 *
 * // Add ternary edge
 * nimcp_ternary_graph_add_edge(tgraph, v0, v1, TRIT_POSITIVE);
 *
 * // Path finding considers edge weights
 * NimcpPath* path = nimcp_ternary_graph_shortest_path(tgraph, v0, v1);
 *
 * // Cleanup
 * nimcp_ternary_graph_destroy(tgraph);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GRAPH_TERNARY_H
#define NIMCP_GRAPH_TERNARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/containers/nimcp_graph.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Edge weight semantics */
#define GRAPH_EDGE_STRONG   TRIT_POSITIVE   /**< Strong/reliable (+1) */
#define GRAPH_EDGE_WEAK     TRIT_UNKNOWN    /**< Weak/neutral (0) */
#define GRAPH_EDGE_ABSENT   TRIT_NEGATIVE   /**< Absent/blocked (-1) */

/** Path finding modes */
#define GRAPH_PATH_PREFER_STRONG  0   /**< Prefer strong edges */
#define GRAPH_PATH_AVOID_WEAK     1   /**< Avoid weak edges */
#define GRAPH_PATH_AVOID_ABSENT   2   /**< Avoid absent edges (default) */

/** Magic number for validation */
#define GRAPH_TERNARY_MAGIC 0x47545454  /* "GTTT" */

//=============================================================================
// Ternary Edge Structure
//=============================================================================

/**
 * @brief Ternary-weighted edge
 *
 * WHAT: Edge with ternary weight classification
 * WHY:  Semantic edge categorization
 * HOW:  Replace float weight with trit classification
 */
typedef struct NimcpTernaryEdge {
    uint32_t dest;                    /**< Destination vertex index */
    trit_t weight;                    /**< Ternary weight {-1, 0, +1} */
    uint32_t flags;                   /**< Edge state flags */
    uint64_t last_updated;            /**< Timestamp of last update */
    struct NimcpTernaryEdge* next;    /**< Next edge in adjacency list */
} NimcpTernaryEdge;

//=============================================================================
// Ternary Graph Structure
//=============================================================================

/**
 * @brief Ternary vertex with adjacency list of ternary edges
 */
typedef struct {
    uint64_t peer_id;                 /**< Unique peer identifier */
    float x, y, z;                    /**< Logical coordinates */
    uint32_t capabilities;            /**< Capability bitmap */
    uint32_t state;                   /**< Current state flags */
    uint64_t last_updated;            /**< Timestamp of last update */
    NimcpTernaryEdge* edges;          /**< Head of ternary adjacency list */
    uint32_t edge_count;              /**< Total edges */
    uint32_t strong_count;            /**< Count of +1 edges */
    uint32_t weak_count;              /**< Count of 0 edges */
    uint32_t absent_count;            /**< Count of -1 edges (for tracking blocked) */
} NimcpTernaryVertex;

/**
 * @brief Graph with ternary edge weights
 *
 * WHAT: Extended graph structure with ternary edge classification
 * WHY:  Support semantic edge weights for topology analysis
 * HOW:  Replace float weights with trit values
 */
typedef struct {
    uint32_t magic;                   /**< Validation: GRAPH_TERNARY_MAGIC */
    uint32_t vertex_count;            /**< Number of vertices */
    uint32_t edge_count;              /**< Total edge count */
    NimcpTernaryVertex* vertices;     /**< Array of vertices */
    uint32_t* components;             /**< Connected components */
    uint32_t component_count;         /**< Number of components */
    nimcp_mutex_t lock;               /**< Thread-safety mutex */

    /* Edge statistics */
    uint32_t total_strong;            /**< Total +1 edges */
    uint32_t total_weak;              /**< Total 0 edges */
    uint32_t total_absent;            /**< Total -1 edges */

    /* Path finding configuration */
    int path_mode;                    /**< Path finding mode */
    float strong_cost;                /**< Cost for +1 edges (default: 0.5) */
    float weak_cost;                  /**< Cost for 0 edges (default: 1.0) */
    float absent_cost;                /**< Cost for -1 edges (default: 10.0) */
} NimcpTernaryGraph;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ternary graph
 *
 * WHAT: Allocate and initialize new ternary graph
 * WHY:  Entry point for ternary graph operations
 * HOW:  Allocate structure and initialize mutex
 *
 * @return New graph or NULL on failure
 */
NimcpTernaryGraph* nimcp_ternary_graph_create(void);

/**
 * @brief Destroy ternary graph
 *
 * WHAT: Free all graph memory
 * WHY:  Clean resource release
 * HOW:  Free vertices, edges, components
 *
 * @param graph Graph to destroy
 */
void nimcp_ternary_graph_destroy(NimcpTernaryGraph* graph);

/**
 * @brief Create ternary graph from existing NimcpGraph
 *
 * WHAT: Convert standard graph to ternary
 * WHY:  Upgrade existing graphs with ternary semantics
 * HOW:  Quantize float weights to ternary
 *
 * @param graph Source graph
 * @param strong_threshold Threshold for STRONG (weight > this)
 * @param weak_threshold Threshold for WEAK (weight > this but <= strong)
 * @return Ternary graph or NULL on failure
 */
NimcpTernaryGraph* nimcp_ternary_graph_from_graph(
    const NimcpGraph* graph,
    float strong_threshold,
    float weak_threshold
);

//=============================================================================
// Vertex Operations
//=============================================================================

/**
 * @brief Add vertex to ternary graph
 *
 * WHAT: Create new vertex with given properties
 * WHY:  Build graph topology
 * HOW:  Allocate vertex in array
 *
 * @param graph Target graph
 * @param peer_id Unique peer identifier
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param capabilities Capability bitmap
 * @return Vertex index or NIMCP_INVALID_VERTEX on failure
 */
uint32_t nimcp_ternary_graph_add_vertex(
    NimcpTernaryGraph* graph,
    uint64_t peer_id,
    float x, float y, float z,
    uint32_t capabilities
);

/**
 * @brief Remove vertex from ternary graph
 *
 * WHAT: Remove vertex and all its edges
 * WHY:  Handle node departure
 * HOW:  Remove from adjacency lists, update components
 *
 * @param graph Target graph
 * @param vertex_idx Vertex to remove
 * @return true on success
 */
bool nimcp_ternary_graph_remove_vertex(
    NimcpTernaryGraph* graph,
    uint32_t vertex_idx
);

/**
 * @brief Find vertex by peer ID
 *
 * @param graph Target graph
 * @param peer_id Peer ID to find
 * @return Vertex index or NIMCP_INVALID_VERTEX if not found
 */
uint32_t nimcp_ternary_graph_find_vertex(
    const NimcpTernaryGraph* graph,
    uint64_t peer_id
);

//=============================================================================
// Edge Operations
//=============================================================================

/**
 * @brief Add ternary edge between vertices
 *
 * WHAT: Create edge with ternary weight
 * WHY:  Build connectivity with semantic classification
 * HOW:  Add to adjacency lists of both vertices
 *
 * @param graph Target graph
 * @param from Source vertex
 * @param to Destination vertex
 * @param weight Ternary weight {-1, 0, +1}
 * @return true on success
 */
bool nimcp_ternary_graph_add_edge(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t weight
);

/**
 * @brief Remove edge between vertices
 *
 * @param graph Target graph
 * @param from Source vertex
 * @param to Destination vertex
 * @return true on success
 */
bool nimcp_ternary_graph_remove_edge(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
);

/**
 * @brief Get ternary edge weight
 *
 * @param graph Target graph
 * @param from Source vertex
 * @param to Destination vertex
 * @param weight Output weight
 * @return true if edge exists
 */
bool nimcp_ternary_graph_get_edge_weight(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t* weight
);

/**
 * @brief Set ternary edge weight
 *
 * WHAT: Update existing edge weight
 * WHY:  Modify edge classification
 * HOW:  Find edge and update weight field
 *
 * @param graph Target graph
 * @param from Source vertex
 * @param to Destination vertex
 * @param weight New ternary weight
 * @return true on success
 */
bool nimcp_ternary_graph_set_edge_weight(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t weight
);

/**
 * @brief Check if edge exists
 *
 * @param graph Target graph
 * @param from Source vertex
 * @param to Destination vertex
 * @return true if edge exists
 */
bool nimcp_ternary_graph_has_edge(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
);

//=============================================================================
// Path Finding
//=============================================================================

/**
 * @brief Find shortest path using ternary weights
 *
 * WHAT: Dijkstra's algorithm with ternary edge costs
 * WHY:  Route through preferred edge types
 * HOW:  Map ternary weights to costs, run Dijkstra
 *
 * COST MAPPING (default):
 * - STRONG (+1): cost = 0.5 (preferred)
 * - WEAK (0): cost = 1.0 (neutral)
 * - ABSENT (-1): cost = 10.0 (avoid if possible)
 *
 * @param graph Source graph
 * @param from Start vertex
 * @param to End vertex
 * @return Path or NULL if no path exists
 */
NimcpPath* nimcp_ternary_graph_shortest_path(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
);

/**
 * @brief Find path using only strong edges
 *
 * WHAT: Path through strong edges only
 * WHY:  Most reliable routing
 * HOW:  Ignore weak and absent edges
 *
 * @param graph Source graph
 * @param from Start vertex
 * @param to End vertex
 * @return Path or NULL if no strong-only path exists
 */
NimcpPath* nimcp_ternary_graph_strong_path(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
);

/**
 * @brief Set path finding cost parameters
 *
 * WHAT: Configure cost mapping for path finding
 * WHY:  Customize routing preferences
 * HOW:  Set cost multipliers for each edge type
 *
 * @param graph Target graph
 * @param strong_cost Cost for +1 edges (default: 0.5)
 * @param weak_cost Cost for 0 edges (default: 1.0)
 * @param absent_cost Cost for -1 edges (default: 10.0)
 */
void nimcp_ternary_graph_set_path_costs(
    NimcpTernaryGraph* graph,
    float strong_cost,
    float weak_cost,
    float absent_cost
);

//=============================================================================
// Neighbor Operations
//=============================================================================

/**
 * @brief Get neighbors with specific edge weight
 *
 * WHAT: List neighbors connected by specific weight type
 * WHY:  Filter connections by quality
 * HOW:  Scan adjacency list with weight filter
 *
 * @param graph Source graph
 * @param vertex_idx Source vertex
 * @param weight_filter Filter for edge weight (or -2 for all)
 * @param neighbors Output array
 * @param max_neighbors Maximum to return
 * @return Number of neighbors found
 */
uint32_t nimcp_ternary_graph_get_neighbors_by_weight(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    trit_t weight_filter,
    uint32_t* neighbors,
    uint32_t max_neighbors
);

/**
 * @brief Get all neighbors
 *
 * @param graph Source graph
 * @param vertex_idx Source vertex
 * @param neighbors Output array
 * @param max_neighbors Maximum to return
 * @return Number of neighbors
 */
uint32_t nimcp_ternary_graph_get_neighbors(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* neighbors,
    uint32_t max_neighbors
);

/**
 * @brief Get strong neighbors only
 *
 * @param graph Source graph
 * @param vertex_idx Source vertex
 * @param neighbors Output array
 * @param max_neighbors Maximum to return
 * @return Number of strong neighbors
 */
uint32_t nimcp_ternary_graph_get_strong_neighbors(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* neighbors,
    uint32_t max_neighbors
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get graph statistics
 *
 * @param graph Source graph
 * @param total_strong Output count of +1 edges
 * @param total_weak Output count of 0 edges
 * @param total_absent Output count of -1 edges
 */
void nimcp_ternary_graph_stats(
    const NimcpTernaryGraph* graph,
    uint32_t* total_strong,
    uint32_t* total_weak,
    uint32_t* total_absent
);

/**
 * @brief Get vertex degree by edge type
 *
 * @param graph Source graph
 * @param vertex_idx Target vertex
 * @param strong_degree Output count of +1 edges
 * @param weak_degree Output count of 0 edges
 * @param absent_degree Output count of -1 edges
 */
void nimcp_ternary_graph_vertex_degrees(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* strong_degree,
    uint32_t* weak_degree,
    uint32_t* absent_degree
);

/**
 * @brief Update connected components
 *
 * WHAT: Recompute connected components
 * WHY:  Track graph connectivity
 * HOW:  DFS traversal marking components
 *
 * @param graph Target graph
 * @return Number of components
 */
uint32_t nimcp_ternary_graph_update_components(NimcpTernaryGraph* graph);

//=============================================================================
// Conversion
//=============================================================================

/**
 * @brief Convert to adjacency matrix
 *
 * WHAT: Export graph as ternary adjacency matrix
 * WHY:  Interface with matrix-based algorithms
 * HOW:  Build matrix from adjacency lists
 *
 * @param graph Source graph
 * @param pack_mode Ternary packing mode
 * @return Ternary adjacency matrix or NULL on failure
 */
trit_matrix_t* nimcp_ternary_graph_to_matrix(
    const NimcpTernaryGraph* graph,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Create ternary graph from adjacency matrix
 *
 * WHAT: Build graph from ternary adjacency
 * WHY:  Import from matrix-based representation
 * HOW:  Iterate matrix and create edges
 *
 * @param adjacency Ternary adjacency matrix
 * @return Graph or NULL on failure
 */
NimcpTernaryGraph* nimcp_ternary_graph_from_matrix(
    const trit_matrix_t* adjacency
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_TERNARY_H */
