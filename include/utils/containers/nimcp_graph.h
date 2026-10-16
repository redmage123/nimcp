/**
 * @file nimcp_graph.h
 * @brief Network topology graph data structure using adjacency lists
 *
 * This module implements a graph structure optimized for P2P network topology
 * representation. It uses adjacency lists instead of matrices to efficiently
 * handle sparse connections typical in P2P networks. The implementation
 * supports dynamic topology changes, path finding, and component analysis.
 *
 * THREAD SAFETY: All public APIs are thread-safe via internal mutex
 * MEMORY MANAGEMENT: Uses nimcp_malloc/free for leak detection
 */

#ifndef NIMCP_GRAPH_H
#define NIMCP_GRAPH_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"

/* Constants */
/**
 * Maximum number of vertices (peers) supported in the graph
 * This limits the maximum network size but prevents unbounded growth
 * Increased to 8192 to support BRAIN_SIZE_LARGE (5000 neurons) + overhead
 */
#define NIMCP_MAX_VERTICES 8192

/**
 * Maximum number of edges (connections) supported in the graph
 * This provides an upper bound on memory usage
 * Increased to 2M to support large brains (1.5M neurons with sparse connectivity)
 */
#define NIMCP_MAX_EDGES 2097152

/**
 * Special value indicating an invalid vertex index
 * Used for error conditions and sentinel values
 */
#define NIMCP_INVALID_VERTEX UINT32_MAX

/* Types */

/**
 * Type definition for edge weights
 * Using float allows for various metrics (latency, bandwidth, reliability, etc.)
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef float nimcp_weight_t;

/**
 * @brief Edge node in adjacency list
 *
 * Represents a connection between peers in the network topology.
 * Forms a singly-linked list of edges for each vertex.
 */
typedef struct NimcpEdgeNode {
    uint32_t dest;              /**< Index of the destination vertex */
    nimcp_weight_t weight;      /**< Edge weight (can represent distance, latency, etc.) */
    uint32_t flags;             /**< Edge state flags (e.g., active, pending, failed) */
    uint64_t last_updated;      /**< Timestamp of last edge state update */
    struct NimcpEdgeNode* next; /**< Pointer to next edge in the adjacency list */
} NimcpEdgeNode;

/**
 * @brief Vertex properties
 *
 * Represents a peer in the network with its properties and connections.
 * Maintains the head of an adjacency list for efficient neighbor access.
 */
typedef struct {
    uint64_t peer_id;      /**< Unique identifier of the peer */
    float x, y, z;         /**< Logical network coordinates for topology mapping */
    uint32_t capabilities; /**< Bitmap of peer capabilities (routing, storage, etc.) */
    uint32_t state;        /**< Current state flags (active, inactive, etc.) */
    uint64_t last_updated; /**< Timestamp of last vertex state update */
    NimcpEdgeNode* edges;  /**< Head of the adjacency list for this vertex */
    uint32_t edge_count;   /**< Number of edges connected to this vertex */
} NimcpVertex;

/**
 * @brief Graph structure
 *
 * Main container for the network topology representation.
 * Uses adjacency lists for memory-efficient sparse graph storage.
 *
 * THREAD SAFETY: Protected by internal mutex (Monitor Pattern)
 * All public API functions acquire lock before modifying graph state.
 *
 * QUANTUM ACCELERATION: Optional quantum walk for O(√N) search speedup
 */
typedef struct {
    uint32_t vertex_count;    /**< Current number of vertices in the graph */
    uint32_t edge_count;      /**< Current total number of edges in the graph */
    NimcpVertex* vertices;    /**< Array of vertices representing peers */
    uint32_t* components;     /**< Array tracking connected components */
    uint32_t component_count; /**< Number of distinct connected components */
    nimcp_mutex_t lock;       /**< Mutex for thread-safe operations */

    /* Quantum walk acceleration */
    void* quantum_bridge;     /**< quantum_walk_bridge_t* for graph search acceleration */
    bool enable_quantum_walk; /**< Whether quantum walk is enabled (default: true) */
} NimcpGraph;

/**
 * @brief Path in the graph
 *
 * Represents a route between two peers in the network.
 * Used for routing and topology analysis.
 */
typedef struct {
    uint32_t* vertices;          /**< Array of vertex indices forming the path */
    uint32_t length;             /**< Number of vertices in the path */
    nimcp_weight_t total_weight; /**< Aggregate weight of all edges in the path */
} NimcpPath;

/* Function Prototypes */

/**
 * @brief Create a new empty graph
 *
 * Allocates and initializes a new graph structure.
 * Sets up internal data structures for vertex and edge storage.
 *
 * @return Pointer to new graph or NULL on allocation failure
 */
NimcpGraph* nimcp_graph_create(void);

/**
 * @brief Destroy a graph and free all resources
 *
 * Cleans up all allocated memory including vertices, edges,
 * and component tracking arrays.
 *
 * @param graph Pointer to graph to destroy
 */
void nimcp_graph_destroy(NimcpGraph* graph);

/**
 * @brief Add a new vertex to the graph
 *
 * Creates a new vertex representing a peer in the network.
 * Initializes the vertex with provided coordinates and capabilities.
 *
 * @param graph Target graph
 * @param peer_id Unique identifier for the peer
 * @param x X coordinate in network space
 * @param y Y coordinate in network space
 * @param z Z coordinate in network space
 * @param capabilities Bitmap of peer capabilities
 * @return Index of new vertex or NIMCP_INVALID_VERTEX on failure
 */
uint32_t nimcp_graph_add_vertex(NimcpGraph* graph, uint64_t peer_id, float x, float y, float z,
                                uint32_t capabilities);

/**
 * @brief Remove a vertex from the graph
 *
 * Removes a peer and all its connections from the network topology.
 * Updates connected components after removal.
 *
 * @param graph Target graph
 * @param vertex_idx Index of vertex to remove
 * @return true on success, false on invalid index or graph
 */
bool nimcp_graph_remove_vertex(NimcpGraph* graph, uint32_t vertex_idx);

/**
 * @brief Add an edge between vertices
 *
 * Creates a new connection between two peers in the network.
 * Updates adjacency lists for both vertices.
 *
 * @param graph Target graph
 * @param from Source vertex index
 * @param to Destination vertex index
 * @param weight Edge weight/cost metric
 * @return true on success, false on invalid indices or full graph
 */
bool nimcp_graph_add_edge(NimcpGraph* graph, uint32_t from, uint32_t to, nimcp_weight_t weight);

/**
 * @brief Remove an edge from the graph
 *
 * Removes a connection between two peers.
 * Updates adjacency lists and connected components.
 *
 * @param graph Target graph
 * @param from Source vertex index
 * @param to Destination vertex index
 * @return true on success, false if edge doesn't exist
 */
bool nimcp_graph_remove_edge(NimcpGraph* graph, uint32_t from, uint32_t to);

/**
 * @brief Find shortest path between vertices
 *
 * Implements Dijkstra's algorithm to find optimal route between peers.
 * Considers edge weights in path calculation.
 *
 * @param graph Source graph
 * @param from Start vertex index
 * @param to End vertex index
 * @return Path object or NULL if no path exists
 */
NimcpPath* nimcp_graph_shortest_path(const NimcpGraph* graph, uint32_t from, uint32_t to);

/**
 * @brief Update vertex coordinates
 *
 * Updates the logical network position of a peer.
 * Used for topology optimization and visualization.
 *
 * @param graph Target graph
 * @param vertex_idx Vertex to update
 * @param x New X coordinate
 * @param y New Y coordinate
 * @param z New Z coordinate
 * @return true on success, false on invalid index
 */
bool nimcp_graph_update_coordinates(NimcpGraph* graph, uint32_t vertex_idx, float x, float y,
                                    float z);

/**
 * @brief Find vertex by peer ID
 *
 * Locates a peer's vertex in the graph by its unique identifier.
 *
 * @param graph Target graph
 * @param peer_id Peer ID to find
 * @return Vertex index or NIMCP_INVALID_VERTEX if not found
 */
uint32_t nimcp_graph_find_vertex(const NimcpGraph* graph, uint64_t peer_id);

/**
 * @brief Update connected components
 *
 * Identifies and labels connected subgraphs in the network.
 * Uses depth-first search for component discovery.
 *
 * @param graph Target graph
 * @return Number of distinct components found
 */
uint32_t nimcp_graph_update_components(NimcpGraph* graph);

/**
 * @brief Get neighbors of a vertex
 *
 * Retrieves all directly connected peers of a given vertex.
 * Traverses adjacency list to build neighbor array.
 *
 * @param graph Source graph
 * @param vertex_idx Vertex index
 * @param neighbors Array to store neighbor indices
 * @param max_neighbors Size of neighbors array
 * @return Number of neighbors written to array
 */
uint32_t nimcp_graph_get_neighbors(const NimcpGraph* graph, uint32_t vertex_idx,
                                   uint32_t* neighbors, uint32_t max_neighbors);

/**
 * @brief Get edge weight between vertices
 *
 * Retrieves the weight/cost metric for a connection between peers.
 * Searches adjacency list for the specified edge.
 *
 * @param graph Source graph
 * @param from Source vertex
 * @param to Destination vertex
 * @param weight Pointer to store retrieved weight
 * @return true if edge exists, false otherwise
 */
bool nimcp_graph_get_edge_weight(const NimcpGraph* graph, uint32_t from, uint32_t to,
                                 nimcp_weight_t* weight);

/**
 * @brief Find path using quantum walk acceleration
 *
 * Hybrid quantum-classical pathfinding algorithm.
 * Uses quantum walk for O(√N) node discovery, then classical Dijkstra for exact path.
 * Falls back to classical algorithm if quantum walk is disabled or unavailable.
 *
 * @param graph Source graph
 * @param from Start vertex index
 * @param to End vertex index
 * @return Path object or NULL if no path exists
 */
NimcpPath* nimcp_graph_quantum_path(NimcpGraph* graph, uint32_t from, uint32_t to);

/**
 * @brief Get the number of vertices in the graph
 *
 * WHAT: Returns the current vertex count
 * WHY:  Accessor for graph size information
 * HOW:  Returns graph->vertex_count directly
 *
 * @param graph Source graph
 * @return Number of vertices, or 0 if graph is NULL
 */
uint32_t nimcp_graph_vertex_count(const NimcpGraph* graph);

/**
 * @brief Get the number of edges in the graph
 *
 * WHAT: Returns the total edge count
 * WHY:  Accessor for graph connectivity information
 * HOW:  Returns graph->edge_count directly
 *
 * @param graph Source graph
 * @return Number of edges, or 0 if graph is NULL
 */
uint32_t nimcp_graph_edge_count(const NimcpGraph* graph);

/**
 * @brief Get the degree (number of edges) of a vertex
 *
 * WHAT: Returns the number of edges connected to a vertex
 * WHY:  Used for centrality calculations and topology analysis
 * HOW:  Returns vertices[vertex_idx].edge_count
 *
 * @param graph Source graph
 * @param vertex_idx Index of the vertex
 * @return Degree of the vertex, or 0 if invalid
 */
uint32_t nimcp_graph_degree(const NimcpGraph* graph, uint32_t vertex_idx);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_GRAPH_H */
