//=============================================================================
// nimcp_entanglement.h - Entanglement Graph for Memory Association
//=============================================================================
/**
 * @file nimcp_entanglement.h
 * @brief Sparse graph for automatic memory association via resonance
 *
 * WHAT: Graph-based memory association with quantum-walk-inspired retrieval
 * WHY:  Memories are interconnected through shared features, temporal co-occurrence,
 *       causal relationships, and emotional associations - forming a rich associative fabric
 * HOW:  Sparse adjacency list graph with resonance-weighted edges, supporting
 *       both classical spreading activation and quantum walk search
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Memory Entanglement Model:
 *   +-----------------------------------------------------------------------+
 *   |  Biological memories are not isolated - they form an associative net |
 *   |                                                                       |
 *   |  Neural Basis:                                                        |
 *   |  - Hippocampal pattern completion activates related memories         |
 *   |  - Prefrontal cortex maintains context-dependent associations        |
 *   |  - Amygdala links emotionally salient memories together              |
 *   |  - Temporal lobe binds semantically similar concepts                 |
 *   |                                                                       |
 *   |  Graph Representation:                                                |
 *   |  - Nodes: Memory IDs (indices into memory store)                     |
 *   |  - Edges: Resonance-weighted connections between memories            |
 *   |  - Edge types capture different association mechanisms               |
 *   +-----------------------------------------------------------------------+
 *
 *   Edge Types and Their Neural Correlates:
 *   +-----------------------------------------------------------------------+
 *   |  SEMANTIC:     Content overlap (Prime signature Jaccard)             |
 *   |                Neural: Lateral temporal cortex pattern similarity    |
 *   |                                                                       |
 *   |  TEMPORAL:     Co-occurrence in time window                          |
 *   |                Neural: Hippocampal theta-locked binding              |
 *   |                                                                       |
 *   |  CAUSAL:       One memory predicts/causes another                    |
 *   |                Neural: Prefrontal-hippocampal prediction coding      |
 *   |                                                                       |
 *   |  ASSOCIATIVE:  Learned via repeated co-retrieval                     |
 *   |                Neural: Hebbian strengthening ("fire together...")    |
 *   |                                                                       |
 *   |  EMOTIONAL:    Shared emotional valence                              |
 *   |                Neural: Amygdala-mediated arousal tagging             |
 *   |                                                                       |
 *   |  CONTEXTUAL:   Same schema/situation/context                         |
 *   |                Neural: Medial prefrontal schema representation       |
 *   +-----------------------------------------------------------------------+
 *
 *   Quantum Walk for Memory Retrieval:
 *   +-----------------------------------------------------------------------+
 *   |  Classical spreading activation has limitations:                      |
 *   |  - Linear decay loses signal in dense graphs                          |
 *   |  - Can't exploit interference patterns                                |
 *   |                                                                       |
 *   |  Quantum walk provides:                                               |
 *   |  - Superposition: Search multiple paths simultaneously               |
 *   |  - Interference: Good paths reinforce, bad paths cancel              |
 *   |  - Quadratic speedup for unstructured search                         |
 *   |                                                                       |
 *   |  Algorithm:                                                           |
 *   |  1. Initialize: amplitude[start] = 1, others = 0                     |
 *   |  2. Evolution: amplitude spreads via weighted edges                   |
 *   |  3. Measurement: Collapse to nodes proportional to |amplitude|^2     |
 *   |                                                                       |
 *   |  Neural analog: Gamma-band synchronization search hypothesis         |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Edge add/remove: O(1) average (hash table)
 * - Neighbor lookup: O(degree)
 * - Quantum walk step: O(E) where E = edges
 * - Top-K retrieval: O(N log K) where N = nodes
 * - Memory: ~56 bytes per edge + hash table overhead
 *
 * INTEGRATION:
 * - Core: Resonance engine computes edge weights
 * - Middleware: Z-Ladder uses for spreading activation
 * - API: Memory query by association
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_ENTANGLEMENT_H
#define NIMCP_ENTANGLEMENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "nimcp_resonance.h"
#include "nimcp_prime_signature.h"
#include "nimcp_quaternion.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default initial capacity for node hash table */
#define ENTANGLE_DEFAULT_NODE_CAPACITY      1024

/** Default initial capacity for edge hash table */
#define ENTANGLE_DEFAULT_EDGE_CAPACITY      4096

/** Default threshold for auto-linking (resonance must exceed this) */
#define ENTANGLE_DEFAULT_LINK_THRESHOLD     0.5f

/** Default threshold for pruning weak edges */
#define ENTANGLE_DEFAULT_PRUNE_THRESHOLD    0.3f

/** Maximum edges per node before warning */
#define ENTANGLE_MAX_DEGREE_WARN            1000

/** Default decay factor for spreading activation (per hop) */
#define ENTANGLE_DEFAULT_DECAY              0.5f

/** Quantum walk amplitude threshold below which to ignore (for sparsity) */
#define ENTANGLE_AMPLITUDE_EPSILON          1e-8f

/** Default quantum walk steps */
#define ENTANGLE_DEFAULT_WALK_STEPS         10

/** Maximum nodes in a single graph */
#define ENTANGLE_MAX_NODES                  (1ULL << 24)  // 16M nodes

/** Maximum edges in a single graph */
#define ENTANGLE_MAX_EDGES                  (1ULL << 26)  // 64M edges

/** Hash table load factor threshold for rehashing */
#define ENTANGLE_LOAD_FACTOR_THRESHOLD      0.75f

/** Numerical epsilon for floating-point comparisons */
#define ENTANGLE_EPSILON                    1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Edge types for entanglement relationships
 *
 * WHAT: Semantic category of memory association
 * WHY:  Different edge types have different biological mechanisms and retrieval semantics
 * HOW:  Enum value stored with each edge
 *
 * The edge type affects:
 * - How edges are created (auto-link criteria)
 * - How edges decay over time
 * - Which edges are traversed in different query contexts
 */
typedef enum {
    ENTANGLE_EDGE_SEMANTIC = 0,   /**< Content similarity (Jaccard on prime signatures) */
    ENTANGLE_EDGE_TEMPORAL,       /**< Co-occurrence within time window */
    ENTANGLE_EDGE_CAUSAL,         /**< Cause-effect relationship (directed) */
    ENTANGLE_EDGE_ASSOCIATIVE,    /**< Learned association via co-retrieval */
    ENTANGLE_EDGE_EMOTIONAL,      /**< Shared emotional valence (quaternion similarity) */
    ENTANGLE_EDGE_CONTEXTUAL,     /**< Same context/schema/situation */
    ENTANGLE_EDGE_TYPE_COUNT      /**< Number of edge types (for arrays) */
} entangle_edge_type_t;

/**
 * @brief Edge structure for entanglement graph
 *
 * WHAT: A weighted, typed connection between two memory nodes
 * WHY:  Captures the strength and nature of memory association
 * HOW:  Stores both endpoint IDs and resonance breakdown
 *
 * Memory layout: ~64 bytes per edge
 * - IDs: 16 bytes (2x uint64_t)
 * - Scores: 16 bytes (4x float)
 * - Metadata: 20 bytes (timestamp, weight, type, flags)
 * - Padding: ~12 bytes (alignment)
 */
typedef struct {
    uint64_t from_id;             /**< Source node ID */
    uint64_t to_id;               /**< Target node ID */
    float resonance_score;        /**< Combined R metric (0-1) */
    float prime_similarity;       /**< Jaccard coefficient on prime signatures */
    float quat_similarity;        /**< 1 - geodesic_distance on quaternions */
    float phase_coherence;        /**< Phase Locking Value between nodes */
    entangle_edge_type_t type;    /**< Edge semantics */
    uint64_t created_time_ms;     /**< Creation timestamp (milliseconds since epoch) */
    float weight;                 /**< Current edge strength (0-1), may decay */
    bool bidirectional;           /**< If true, implies reverse edge exists */
} entangle_edge_t;

/**
 * @brief Configuration for entanglement graph
 *
 * WHAT: Parameters controlling graph behavior
 * WHY:  Different applications need different trade-offs (memory vs speed vs precision)
 * HOW:  Set at creation time, some parameters modifiable afterward
 */
typedef struct {
    size_t initial_node_capacity;     /**< Hash table size for nodes */
    size_t initial_edge_capacity;     /**< Hash table size for edges */
    float auto_link_threshold;        /**< Minimum resonance for auto-linking */
    float prune_threshold;            /**< Remove edges below this weight */
    bool enable_bidirectional;        /**< Automatically create reverse edges */
    bool track_creation_time;         /**< Store timestamps on edges */
    resonance_config_t resonance_cfg; /**< Config for resonance computations */
} entangle_config_t;

/**
 * @brief Quantum walk state for spreading activation retrieval
 *
 * WHAT: Probability amplitudes over graph nodes during walk
 * WHY:  Quantum walk provides interference-based search
 * HOW:  Complex amplitudes that evolve according to graph structure
 *
 * Note: We use real-valued amplitudes (classical limit) for efficiency,
 * but the algorithm structure mirrors quantum walks.
 */
typedef struct {
    uint64_t start_node;          /**< Initial node (where walk started) */
    float* amplitudes;            /**< Probability amplitudes per node [num_nodes] */
    uint64_t* node_ids;           /**< Mapping from index to node ID [num_nodes] */
    size_t num_nodes;             /**< Number of nodes in walk state */
    uint32_t current_step;        /**< Steps taken so far */
    uint32_t max_steps;           /**< Maximum steps to take */
    float total_amplitude;        /**< Sum of |amplitude|^2 (should be ~1) */
    bool is_collapsed;            /**< Whether walk has been measured */
} quantum_walk_state_t;

/**
 * @brief Result from quantum walk collapse (measurement)
 *
 * WHAT: A node selected probabilistically from walk state
 * WHY:  Returns most likely relevant nodes based on graph structure
 */
typedef struct {
    uint64_t node_id;             /**< Selected node ID */
    float probability;            /**< |amplitude|^2 at collapse time */
    uint32_t steps_taken;         /**< Walk length to reach this result */
} quantum_walk_result_t;

/**
 * @brief Spreading activation state for classical retrieval
 *
 * WHAT: Activation levels over graph nodes
 * WHY:  Classical spreading activation is simpler and often sufficient
 * HOW:  Initial activation spreads to neighbors with decay
 */
typedef struct {
    float* activations;           /**< Activation level per node [num_nodes] */
    uint64_t* node_ids;           /**< Mapping from index to node ID [num_nodes] */
    size_t num_nodes;             /**< Number of nodes tracked */
    float decay;                  /**< Decay factor per hop */
    uint32_t max_hops;            /**< Maximum spreading distance */
    float threshold;              /**< Minimum activation to propagate */
} spreading_state_t;

/**
 * @brief Statistics for entanglement graph
 *
 * WHAT: Operational metrics for monitoring and optimization
 * WHY:  Track graph health, identify bottlenecks
 */
typedef struct {
    size_t num_nodes;             /**< Number of unique nodes in graph */
    size_t num_edges;             /**< Total edge count */
    size_t max_degree;            /**< Maximum node degree */
    float avg_degree;             /**< Average node degree */
    float avg_weight;             /**< Average edge weight */
    float min_weight;             /**< Minimum edge weight */
    float max_weight;             /**< Maximum edge weight */
    uint64_t edges_by_type[ENTANGLE_EDGE_TYPE_COUNT]; /**< Count per edge type */
    uint64_t total_lookups;       /**< Edge lookup count */
    uint64_t total_walks;         /**< Quantum walks performed */
    uint64_t total_spreads;       /**< Spreading activations performed */
    size_t memory_bytes;          /**< Approximate memory usage */
} entangle_stats_t;

/**
 * @brief Neighbor result with resonance details
 *
 * WHAT: A neighbor node with its edge information
 * WHY:  Queries often need both neighbor ID and connection strength
 */
typedef struct {
    uint64_t neighbor_id;         /**< Neighbor node ID */
    entangle_edge_t edge;         /**< Full edge information */
} entangle_neighbor_t;

/**
 * @brief Opaque graph structure
 *
 * Internal implementation uses:
 * - Hash table for nodes
 * - Hash table for edges (keyed by (from_id, to_id))
 * - Adjacency lists for efficient neighbor traversal
 * - Read-write lock for thread safety
 */
typedef struct entangle_graph_struct* entangle_graph_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default entanglement graph configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - initial_node_capacity: 1024
 *         - initial_edge_capacity: 4096
 *         - auto_link_threshold: 0.5
 *         - prune_threshold: 0.3
 *         - enable_bidirectional: true
 *         - track_creation_time: true
 *
 * Performance: ~5ns
 *
 * Example:
 *   entangle_config_t config = entangle_config_default();
 *   config.auto_link_threshold = 0.7f;  // More selective linking
 *   entangle_graph_t graph = entangle_graph_create(&config);
 */
NIMCP_EXPORT entangle_config_t entangle_config_default(void);

/**
 * @brief Validate entanglement configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - Capacities must be > 0
 * - Thresholds must be in [0, 1]
 * - Prune threshold should be < auto_link threshold
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool entangle_config_validate(const entangle_config_t* config);

//=============================================================================
// Graph Management Functions
//=============================================================================

/**
 * @brief Create a new entanglement graph
 *
 * WHAT: Allocates and initializes empty graph
 * WHY:  Entry point for graph-based memory association
 * HOW:  Creates hash tables for nodes and edges, initializes locks
 *
 * @param config Configuration (NULL for defaults)
 * @return Opaque graph handle, or NULL on failure
 *
 * Performance: O(capacity) for hash table initialization
 * Memory: ~200KB for default capacity
 *
 * Thread safety: The returned graph is thread-safe for concurrent use
 *
 * Example:
 *   entangle_graph_t graph = entangle_graph_create(NULL);
 *   if (!graph) {
 *       fprintf(stderr, "Failed to create graph: %s\n", entangle_get_last_error());
 *   }
 */
NIMCP_EXPORT entangle_graph_t entangle_graph_create(const entangle_config_t* config);

/**
 * @brief Destroy entanglement graph and free all resources
 *
 * WHAT: Deallocates graph and all edges
 * WHY:  Resource cleanup
 * HOW:  Frees all hash table entries, adjacency lists, and locks
 *
 * @param graph Graph to destroy (NULL safe)
 *
 * Performance: O(N + E) where N = nodes, E = edges
 *
 * Warning: Any walk or spreading state referencing this graph becomes invalid
 *
 * Example:
 *   entangle_graph_destroy(graph);
 *   graph = NULL;  // Good practice
 */
NIMCP_EXPORT void entangle_graph_destroy(entangle_graph_t graph);

/**
 * @brief Remove all edges from graph (keep structure)
 *
 * WHAT: Clears all edges while keeping graph allocated
 * WHY:  Reset graph without reallocation overhead
 * HOW:  Clears hash tables but keeps allocated capacity
 *
 * @param graph Graph to clear
 * @return true on success, false if graph is NULL
 *
 * Performance: O(E) where E = current edges
 *
 * Example:
 *   entangle_graph_clear(graph);  // Fresh start
 */
NIMCP_EXPORT bool entangle_graph_clear(entangle_graph_t graph);

/**
 * @brief Check if node exists in graph
 *
 * WHAT: Tests whether a node ID is present
 * WHY:  Validation before operations
 *
 * @param graph Graph to query
 * @param node_id Node ID to check
 * @return true if node exists (has at least one edge), false otherwise
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT bool entangle_node_exists(entangle_graph_t graph, uint64_t node_id);

//=============================================================================
// Edge Operations
//=============================================================================

/**
 * @brief Add edge to graph
 *
 * WHAT: Creates connection between two memory nodes
 * WHY:  Build association structure for retrieval
 * HOW:  Inserts edge in hash table and updates adjacency lists
 *
 * @param graph Graph to modify
 * @param edge Edge to add (copied into graph)
 * @return true on success, false on error (NULL, duplicate, invalid)
 *
 * Performance: O(1) average, O(N) worst case (rehash)
 *
 * Notes:
 * - If bidirectional, automatically creates reverse edge
 * - If edge already exists, returns false (use update instead)
 * - Implicitly creates nodes if they don't exist
 *
 * Example:
 *   entangle_edge_t edge = {
 *       .from_id = mem1_id,
 *       .to_id = mem2_id,
 *       .resonance_score = 0.85f,
 *       .type = ENTANGLE_EDGE_SEMANTIC,
 *       .weight = 0.85f,
 *       .bidirectional = true
 *   };
 *   if (!entangle_add_edge(graph, &edge)) {
 *       // Handle error
 *   }
 */
NIMCP_EXPORT bool entangle_add_edge(entangle_graph_t graph, const entangle_edge_t* edge);

/**
 * @brief Remove edge from graph
 *
 * WHAT: Deletes connection between two nodes
 * WHY:  Prune weak or outdated associations
 * HOW:  Removes from hash table and adjacency lists
 *
 * @param graph Graph to modify
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @return true if edge existed and was removed, false otherwise
 *
 * Performance: O(1) average
 *
 * Notes:
 * - Does NOT automatically remove reverse edge (even if bidirectional)
 * - May leave orphan nodes (nodes with no edges)
 */
NIMCP_EXPORT bool entangle_remove_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id);

/**
 * @brief Update existing edge
 *
 * WHAT: Modifies edge properties (weight, score, etc.)
 * WHY:  Strengthen/weaken associations based on co-retrieval
 * HOW:  Looks up edge and updates in place
 *
 * @param graph Graph to modify
 * @param edge Updated edge (from_id and to_id identify edge)
 * @return true if edge existed and was updated, false otherwise
 *
 * Performance: O(1) average
 *
 * Notes:
 * - from_id and to_id cannot be changed (identifies the edge)
 * - If bidirectional flag changed from false to true, creates reverse edge
 */
NIMCP_EXPORT bool entangle_update_edge(entangle_graph_t graph, const entangle_edge_t* edge);

/**
 * @brief Retrieve edge by endpoint IDs
 *
 * WHAT: Looks up edge data
 * WHY:  Check connection strength, edge properties
 *
 * @param graph Graph to query
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @param edge Output edge data (caller-allocated)
 * @return true if edge exists, false otherwise
 *
 * Performance: O(1) average
 *
 * Example:
 *   entangle_edge_t edge;
 *   if (entangle_get_edge(graph, mem1, mem2, &edge)) {
 *       printf("Edge weight: %.3f\n", edge.weight);
 *   }
 */
NIMCP_EXPORT bool entangle_get_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, entangle_edge_t* edge);

/**
 * @brief Check if edge exists
 *
 * WHAT: Fast existence check without retrieving data
 * WHY:  Avoid allocation when only checking presence
 *
 * @param graph Graph to query
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @return true if edge exists, false otherwise
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT bool entangle_has_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id);

/**
 * @brief Strengthen edge by delta
 *
 * WHAT: Increases edge weight (Hebbian learning)
 * WHY:  Co-retrieved memories should be more strongly linked
 *
 * @param graph Graph to modify
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @param delta Amount to increase weight (clamped to [0, 1])
 * @return New weight after strengthening, or -1.0f if edge doesn't exist
 *
 * Performance: O(1) average
 *
 * Example:
 *   // Memories retrieved together - strengthen link
 *   float new_weight = entangle_strengthen_edge(graph, mem1, mem2, 0.1f);
 */
NIMCP_EXPORT float entangle_strengthen_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, float delta);

/**
 * @brief Weaken edge by delta
 *
 * WHAT: Decreases edge weight (forgetting/decay)
 * WHY:  Unused associations should decay over time
 *
 * @param graph Graph to modify
 * @param from_id Source node ID
 * @param to_id Target node ID
 * @param delta Amount to decrease weight (clamped to [0, 1])
 * @return New weight after weakening, or -1.0f if edge doesn't exist
 *
 * Performance: O(1) average
 */
NIMCP_EXPORT float entangle_weaken_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, float delta);

//=============================================================================
// Neighbor Query Functions
//=============================================================================

/**
 * @brief Get all neighbors of a node
 *
 * WHAT: Returns all nodes connected to given node (in or out)
 * WHY:  Basic traversal operation for spreading activation
 *
 * @param graph Graph to query
 * @param node_id Node to get neighbors of
 * @param neighbors Output array (caller-allocated)
 * @param max_neighbors Maximum neighbors to return
 * @param count Output: actual number of neighbors returned
 * @return true on success, false on error
 *
 * Performance: O(degree)
 *
 * Example:
 *   entangle_neighbor_t neighbors[100];
 *   size_t count;
 *   if (entangle_get_neighbors(graph, node_id, neighbors, 100, &count)) {
 *       for (size_t i = 0; i < count; i++) {
 *           printf("Neighbor %lu: weight=%.3f\n",
 *                  neighbors[i].neighbor_id, neighbors[i].edge.weight);
 *       }
 *   }
 */
NIMCP_EXPORT bool entangle_get_neighbors(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_neighbor_t* neighbors,
    size_t max_neighbors,
    size_t* count);

/**
 * @brief Get outgoing edges from a node
 *
 * WHAT: Returns edges where node is the source
 * WHY:  Directed traversal from a starting point
 *
 * @param graph Graph to query
 * @param node_id Source node
 * @param edges Output array (caller-allocated)
 * @param max_edges Maximum edges to return
 * @param count Output: actual edge count
 * @return true on success, false on error
 *
 * Performance: O(out_degree)
 */
NIMCP_EXPORT bool entangle_get_outgoing(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_t* edges,
    size_t max_edges,
    size_t* count);

/**
 * @brief Get incoming edges to a node
 *
 * WHAT: Returns edges where node is the target
 * WHY:  "What memories lead to this one?"
 *
 * @param graph Graph to query
 * @param node_id Target node
 * @param edges Output array (caller-allocated)
 * @param max_edges Maximum edges to return
 * @param count Output: actual edge count
 * @return true on success, false on error
 *
 * Performance: O(in_degree)
 */
NIMCP_EXPORT bool entangle_get_incoming(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_t* edges,
    size_t max_edges,
    size_t* count);

/**
 * @brief Get strongest K connections from a node
 *
 * WHAT: Returns top-K neighbors by edge weight
 * WHY:  Retrieval typically wants strongest associations
 *
 * @param graph Graph to query
 * @param node_id Node to query from
 * @param k Number of strongest neighbors to return
 * @param neighbors Output array (caller-allocated, size >= k)
 * @param count Output: actual count returned (<= k)
 * @return true on success, false on error
 *
 * Performance: O(degree + k log k) for partial sort
 *
 * Example:
 *   entangle_neighbor_t top5[5];
 *   size_t count;
 *   entangle_get_strongest(graph, query_id, 5, top5, &count);
 */
NIMCP_EXPORT bool entangle_get_strongest(
    entangle_graph_t graph,
    uint64_t node_id,
    size_t k,
    entangle_neighbor_t* neighbors,
    size_t* count);

/**
 * @brief Get neighbors filtered by edge type
 *
 * WHAT: Returns neighbors connected by specific edge type
 * WHY:  Retrieval may want only semantic or only temporal associations
 *
 * @param graph Graph to query
 * @param node_id Node to query from
 * @param type Edge type to filter by
 * @param neighbors Output array (caller-allocated)
 * @param max_neighbors Maximum to return
 * @param count Output: actual count returned
 * @return true on success, false on error
 *
 * Performance: O(degree)
 */
NIMCP_EXPORT bool entangle_get_neighbors_by_type(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_type_t type,
    entangle_neighbor_t* neighbors,
    size_t max_neighbors,
    size_t* count);

//=============================================================================
// Auto-Entanglement Functions
//=============================================================================

/**
 * @brief Compute resonance between two memories for potential linking
 *
 * WHAT: Calculates resonance score without creating edge
 * WHY:  Preview resonance before committing to graph modification
 *
 * @param graph Graph (for configuration)
 * @param query Query memory parameters
 * @param target Target memory parameters
 * @param result Output resonance result
 * @return true on success, false on error
 *
 * Performance: ~100-200ns (depends on components enabled)
 *
 * Note: Uses graph's resonance config for component weights
 */
NIMCP_EXPORT bool entangle_compute_resonance(
    entangle_graph_t graph,
    const resonance_query_t* query,
    const resonance_target_t* target,
    resonance_result_t* result);

/**
 * @brief Automatically link memories if resonance exceeds threshold
 *
 * WHAT: Compute resonance and create edge if strong enough
 * WHY:  Automatic association building based on content similarity
 *
 * @param graph Graph to modify
 * @param from_id Source memory ID
 * @param to_id Target memory ID
 * @param query Query parameters (from memory)
 * @param target Target parameters (to memory)
 * @param type Edge type to assign if linked
 * @param edge_out Output: created edge (NULL if not linked, or on error)
 * @return true if edge was created, false if below threshold or error
 *
 * Performance: O(1) average + resonance computation
 *
 * Example:
 *   resonance_query_t q = { .signature = &sig1, .quaternion = q1 };
 *   resonance_target_t t = { .signature = &sig2, .quaternion = q2 };
 *   entangle_edge_t edge;
 *   if (entangle_auto_link(graph, mem1, mem2, &q, &t, ENTANGLE_EDGE_SEMANTIC, &edge)) {
 *       printf("Created edge with resonance %.3f\n", edge.resonance_score);
 *   }
 */
NIMCP_EXPORT bool entangle_auto_link(
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    const resonance_query_t* query,
    const resonance_target_t* target,
    entangle_edge_type_t type,
    entangle_edge_t* edge_out);

/**
 * @brief Batch auto-link: try to link one source to multiple targets
 *
 * WHAT: Efficiently link one memory to many candidates
 * WHY:  New memory should be linked to all relevant existing memories
 *
 * @param graph Graph to modify
 * @param from_id Source memory ID
 * @param query Query parameters for source
 * @param targets Array of target parameters
 * @param target_ids Array of target memory IDs
 * @param num_targets Number of targets
 * @param type Edge type for created edges
 * @return Number of edges created
 *
 * Performance: O(num_targets * resonance_cost)
 */
NIMCP_EXPORT size_t entangle_auto_link_batch(
    entangle_graph_t graph,
    uint64_t from_id,
    const resonance_query_t* query,
    const resonance_target_t* targets,
    const uint64_t* target_ids,
    size_t num_targets,
    entangle_edge_type_t type);

/**
 * @brief Prune edges below weight threshold
 *
 * WHAT: Remove weak edges from graph
 * WHY:  Garbage collection for decayed associations
 *
 * @param graph Graph to prune
 * @param threshold Remove edges with weight < threshold (0 = use config default)
 * @return Number of edges removed
 *
 * Performance: O(E) where E = total edges
 *
 * Example:
 *   // Remove edges below 0.2 weight
 *   size_t removed = entangle_prune_weak(graph, 0.2f);
 *   printf("Pruned %zu weak edges\n", removed);
 */
NIMCP_EXPORT size_t entangle_prune_weak(entangle_graph_t graph, float threshold);

/**
 * @brief Decay all edge weights by factor
 *
 * WHAT: Multiply all weights by decay factor
 * WHY:  Time-based forgetting of unused associations
 *
 * @param graph Graph to modify
 * @param decay_factor Multiplicative factor (0 < factor <= 1)
 * @return Number of edges affected
 *
 * Performance: O(E)
 *
 * Example:
 *   // Apply 10% decay to all edges
 *   entangle_decay_all(graph, 0.9f);
 */
NIMCP_EXPORT size_t entangle_decay_all(entangle_graph_t graph, float decay_factor);

//=============================================================================
// Quantum Walk Functions
//=============================================================================

/**
 * @brief Initialize quantum walk state from starting node
 *
 * WHAT: Creates walk state with all amplitude at start node
 * WHY:  Begin quantum-inspired search from query node
 *
 * @param graph Graph to walk on
 * @param start_node Starting node ID (must exist in graph)
 * @param max_steps Maximum walk steps
 * @return Walk state handle, or NULL on error
 *
 * Performance: O(N) where N = nodes in graph
 * Memory: O(N) for amplitude array
 *
 * Thread safety: Walk state is NOT thread-safe (use one per thread)
 *
 * Example:
 *   quantum_walk_state_t* walk = quantum_walk_init(graph, query_id, 20);
 *   if (!walk) {
 *       fprintf(stderr, "Failed to init walk: %s\n", entangle_get_last_error());
 *   }
 */
NIMCP_EXPORT quantum_walk_state_t* quantum_walk_init(
    entangle_graph_t graph,
    uint64_t start_node,
    uint32_t max_steps);

/**
 * @brief Execute one step of quantum walk
 *
 * WHAT: Evolve amplitudes according to graph structure
 * WHY:  Each step spreads probability through the graph
 *
 * ALGORITHM:
 *   For each node i with amplitude[i] > epsilon:
 *     For each edge (i, j) with weight w:
 *       amplitude_new[j] += amplitude[i] * w / out_degree(i)
 *   Normalize so sum(|amplitude|^2) = 1
 *
 * @param graph Graph being walked
 * @param state Walk state to evolve
 * @return true on success, false if already at max_steps or error
 *
 * Performance: O(E) where E = edges
 *
 * Example:
 *   for (int i = 0; i < 10; i++) {
 *       entangle_quantum_walk_step(graph, walk);
 *   }
 */
NIMCP_EXPORT bool entangle_quantum_walk_step(entangle_graph_t graph, quantum_walk_state_t* state);

/**
 * @brief Run N steps of quantum walk
 *
 * WHAT: Convenience function for multi-step evolution
 * WHY:  Most retrieval needs several steps
 *
 * @param graph Graph being walked
 * @param state Walk state
 * @param steps Number of steps to take
 * @return Actual steps taken (may be less if hit max_steps)
 *
 * Performance: O(steps * E)
 *
 * Example:
 *   uint32_t taken = quantum_walk_run(graph, walk, 10);
 */
NIMCP_EXPORT uint32_t quantum_walk_run(entangle_graph_t graph, quantum_walk_state_t* state, uint32_t steps);

/**
 * @brief Collapse walk state to select a node
 *
 * WHAT: "Measure" the walk state, selecting node probabilistically
 * WHY:  Retrieve a memory based on walk distribution
 *
 * ALGORITHM:
 *   Sample random value in [0, 1)
 *   Return node i where sum(|amplitude[0..i]|^2) > random
 *
 * @param state Walk state (marked as collapsed after this)
 * @param result Output: selected node and probability
 * @return true on success, false if already collapsed or error
 *
 * Performance: O(N) worst case
 *
 * Notes:
 * - Walk state becomes invalid after collapse
 * - Call quantum_walk_init to restart
 *
 * Example:
 *   quantum_walk_result_t result;
 *   if (quantum_walk_collapse(walk, &result)) {
 *       printf("Retrieved memory %lu with prob %.3f\n",
 *              result.node_id, result.probability);
 *   }
 */
NIMCP_EXPORT bool quantum_walk_collapse(quantum_walk_state_t* state, quantum_walk_result_t* result);

/**
 * @brief Get top-K nodes by probability without collapsing
 *
 * WHAT: Return K highest probability nodes from walk state
 * WHY:  Retrieve multiple relevant memories
 *
 * @param state Walk state (NOT collapsed, can continue walking after)
 * @param k Number of top nodes to return
 * @param results Output array (caller-allocated, size >= k)
 * @param count Output: actual count returned (<= k)
 * @return true on success, false on error
 *
 * Performance: O(N + K log K)
 *
 * Notes:
 * - Does NOT collapse walk - can continue stepping
 * - Probabilities are |amplitude|^2
 *
 * Example:
 *   quantum_walk_result_t top10[10];
 *   size_t count;
 *   quantum_walk_get_top_k(walk, 10, top10, &count);
 */
NIMCP_EXPORT bool quantum_walk_get_top_k(
    quantum_walk_state_t* state,
    size_t k,
    quantum_walk_result_t* results,
    size_t* count);

/**
 * @brief Get probability of specific node in walk state
 *
 * WHAT: Check how likely a specific node would be selected
 * WHY:  Targeted queries - "how related is X to query?"
 *
 * @param state Walk state
 * @param node_id Node to query
 * @return Probability (|amplitude|^2), or -1.0f if node not in state
 *
 * Performance: O(N) worst case (linear search), O(1) with index
 */
NIMCP_EXPORT float quantum_walk_get_probability(quantum_walk_state_t* state, uint64_t node_id);

/**
 * @brief Destroy quantum walk state
 *
 * WHAT: Free walk state memory
 * WHY:  Resource cleanup
 *
 * @param state Walk state to destroy (NULL safe)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void entangle_quantum_walk_destroy(quantum_walk_state_t* state);

//=============================================================================
// Classical Spreading Activation Functions
//=============================================================================

/**
 * @brief Perform spreading activation from starting nodes
 *
 * WHAT: Classical activation spreading with decay
 * WHY:  Simpler and faster than quantum walk for many applications
 *
 * ALGORITHM:
 *   activation[start] = 1.0
 *   For hop = 1 to max_hops:
 *     For each active node i:
 *       For each edge (i, j) with weight w:
 *         activation_new[j] += activation[i] * w * decay
 *
 * @param graph Graph to traverse
 * @param start_nodes Array of starting node IDs
 * @param start_activations Initial activation levels (NULL = all 1.0)
 * @param num_starts Number of starting nodes
 * @param decay Decay factor per hop (0 < decay < 1)
 * @param max_hops Maximum spreading distance
 * @param threshold Minimum activation to continue spreading
 * @param results Output array of (node_id, activation) pairs
 * @param max_results Maximum results to return
 * @param result_count Output: actual results returned
 * @return true on success, false on error
 *
 * Performance: O(V + E * max_hops) worst case
 *
 * Example:
 *   uint64_t starts[] = {query_id};
 *   entangle_neighbor_t results[100];
 *   size_t count;
 *   entangle_spread_activation(graph, starts, NULL, 1, 0.5f, 3, 0.01f,
 *                              results, 100, &count);
 */
NIMCP_EXPORT bool entangle_spread_activation(
    entangle_graph_t graph,
    const uint64_t* start_nodes,
    const float* start_activations,
    size_t num_starts,
    float decay,
    uint32_t max_hops,
    float threshold,
    entangle_neighbor_t* results,
    size_t max_results,
    size_t* result_count);

/**
 * @brief Cascaded retrieval with multiple passes
 *
 * WHAT: Multi-pass spreading with intermediate pruning
 * WHY:  Better signal-to-noise for complex queries
 *
 * ALGORITHM:
 *   1. Spread from start nodes
 *   2. Keep top-K activated nodes
 *   3. Spread from those K nodes
 *   4. Repeat for cascade_depth iterations
 *   5. Return final activation pattern
 *
 * @param graph Graph to traverse
 * @param start_node Starting node ID
 * @param cascade_depth Number of cascade passes
 * @param top_k Keep this many nodes each pass
 * @param decay Decay factor per hop
 * @param results Output results
 * @param max_results Maximum results
 * @param result_count Output: actual count
 * @return true on success, false on error
 *
 * Performance: O(cascade_depth * top_k * E)
 */
NIMCP_EXPORT bool entangle_cascade(
    entangle_graph_t graph,
    uint64_t start_node,
    uint32_t cascade_depth,
    size_t top_k,
    float decay,
    entangle_neighbor_t* results,
    size_t max_results,
    size_t* result_count);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get graph statistics
 *
 * WHAT: Returns operational metrics and structure info
 * WHY:  Monitoring, debugging, optimization
 *
 * @param graph Graph to query
 * @param stats Output statistics (caller-allocated)
 * @return true on success, false if graph is NULL
 *
 * Performance: O(N) for degree computation
 */
NIMCP_EXPORT bool entangle_get_stats(entangle_graph_t graph, entangle_stats_t* stats);

/**
 * @brief Get degree of specific node
 *
 * WHAT: Returns in-degree, out-degree, or total degree
 * WHY:  Hub detection, normalization
 *
 * @param graph Graph to query
 * @param node_id Node to query
 * @param in_degree Output: incoming edge count (NULL to skip)
 * @param out_degree Output: outgoing edge count (NULL to skip)
 * @return Total degree (in + out), or 0 if node doesn't exist
 *
 * Performance: O(1) if tracked, O(degree) otherwise
 */
NIMCP_EXPORT size_t entangle_node_degree(
    entangle_graph_t graph,
    uint64_t node_id,
    size_t* in_degree,
    size_t* out_degree);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears lookup/walk/spread counters
 * WHY:  Start fresh measurement period
 *
 * @param graph Graph to reset stats for
 */
NIMCP_EXPORT void entangle_reset_stats(entangle_graph_t graph);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get last error message
 *
 * WHAT: Returns description of last error
 * WHY:  Debugging failed operations
 *
 * @return Error string, or NULL if no error
 *
 * Thread safety: Error messages are thread-local
 */
NIMCP_EXPORT const char* entangle_get_last_error(void);

/**
 * @brief Get edge type name as string
 *
 * WHAT: Convert edge type enum to human-readable string
 * WHY:  Debugging, logging
 *
 * @param type Edge type
 * @return Static string name (e.g., "SEMANTIC", "TEMPORAL")
 */
NIMCP_EXPORT const char* entangle_edge_type_name(entangle_edge_type_t type);

/**
 * @brief Print edge to stdout for debugging
 *
 * @param edge Edge to print
 */
NIMCP_EXPORT void entangle_edge_print(const entangle_edge_t* edge);

/**
 * @brief Print graph summary to stdout
 *
 * @param graph Graph to summarize
 */
NIMCP_EXPORT void entangle_graph_print_summary(entangle_graph_t graph);

/**
 * @brief Validate graph internal consistency
 *
 * WHAT: Checks hash table integrity, adjacency list consistency
 * WHY:  Debug/test tool for graph corruption detection
 *
 * @param graph Graph to validate
 * @return true if consistent, false if corruption detected
 *
 * Performance: O(N + E)
 */
NIMCP_EXPORT bool entangle_graph_validate(entangle_graph_t graph);

/**
 * @brief Compact graph memory (defragmentation)
 *
 * WHAT: Shrinks hash tables to fit current content
 * WHY:  Reduce memory after large prune operations
 *
 * @param graph Graph to compact
 * @return Bytes freed
 *
 * Performance: O(N + E)
 */
NIMCP_EXPORT size_t entangle_graph_compact(entangle_graph_t graph);

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Utility for edge timestamps
 * WHY:  Consistent timestamp source
 *
 * @return Milliseconds since epoch (approx)
 */
NIMCP_EXPORT uint64_t entangle_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ENTANGLEMENT_H
