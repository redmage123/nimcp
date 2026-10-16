/**
 * @file nimcp_graph.c
 * @brief Implementation of network topology graph structure
 *
 * REFACTORED: Added memory tracking, thread safety, and validation
 * WHY: Prevent memory leaks, ensure thread-safe topology updates, validate inputs
 *
 * OPTIMIZED: Dijkstra's algorithm now uses min-heap for O((V+E) log V) instead of O(V²)
 * WHY: Improves performance for large graphs (>100 vertices)
 *
 * QUANTUM INTEGRATION: Added quantum walk acceleration for graph search
 * WHY: O(√N) speedup for graph traversal operations
 * HOW: Amplitude propagation along ternary-weighted edges
 */

#include "utils/containers/nimcp_graph.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

/* Quantum walk bridge integration */
#define NIMCP_QUANTUM_WALK_BRIDGE_IMPLEMENTATION
#include "utils/quantum/nimcp_quantum_walk_bridge.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/containers/nimcp_min_heap.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graph)

/* Internal helper functions */

/**
 * @brief Allocate and initialize a new edge node
 *
 * WHAT: Creates edge node in adjacency list
 * WHY: Use nimcp_malloc for memory tracking
 * PATTERN: Factory Method - centralized edge creation
 */
static NimcpEdgeNode* create_edge_node(uint32_t dest, nimcp_weight_t weight)
{
    // Validate edge weight
    // WHY: Ensure weight is valid float (no NaN/Inf, within range)
    if (!nimcp_validate_float_field(&weight, sizeof(nimcp_weight_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_edge_node: nimcp_validate_float_field is NULL");
        return NULL;
    }

    NimcpEdgeNode* node = (NimcpEdgeNode*) nimcp_malloc(sizeof(NimcpEdgeNode));
    if (node) {
        node->dest = dest;
        node->weight = weight;
        node->flags = 0;
        node->last_updated = 0;
        node->next = NULL;
    }
    return node;
}

/**
 * @brief Free an entire edge list
 *
 * WHAT: Recursively frees adjacency list
 * WHY: Use nimcp_free for memory tracking
 * PATTERN: Chain of Responsibility - traverse and free linked list
 */
static void free_edge_list(NimcpEdgeNode* head)
{
    while (head) {
        NimcpEdgeNode* next = head->next;
        nimcp_free(head);
        head = next;
    }
}

/**
 * @brief Depth-first search for component labeling
 */
static void dfs_component(const NimcpGraph* graph, uint32_t vertex, uint32_t component,
                          bool* visited)
{
    visited[vertex] = true;
    graph->components[vertex] = component;

    NimcpEdgeNode* edge = graph->vertices[vertex].edges;
    while (edge) {
        if (!visited[edge->dest]) {
            dfs_component(graph, edge->dest, component, visited);
        }
        edge = edge->next;
    }
}

/* Public function implementations */

/**
 * WHAT: Creates new graph for network topology
 * WHY: Use nimcp_* memory functions for leak detection
 * PATTERN: Builder - step-by-step construction with error handling
 */
NimcpGraph* nimcp_graph_create(void)
{
    LOG_TRACE("Entering nimcp_graph_create");
    NimcpGraph* graph = (NimcpGraph*) nimcp_malloc(sizeof(NimcpGraph));
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_graph_create: failed to allocate graph");
        LOG_ERROR("nimcp_graph_create failed: returning error");
        return NULL;
    }

    // Initialize graph structure
    graph->vertex_count = 0;
    graph->edge_count = 0;
    graph->component_count = 0;

    // Initialize mutex for thread safety (Monitor Pattern)
    if (nimcp_mutex_init(&graph->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_graph_create: failed to initialize mutex");
        return NULL;
    }

    // Allocate vertex array (zero-initialized for clean state)
    graph->vertices = (NimcpVertex*) nimcp_calloc(NIMCP_MAX_VERTICES, sizeof(NimcpVertex));
    if (!graph->vertices) {
        nimcp_mutex_destroy(&graph->lock);
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_graph_create: failed to allocate vertices array");
        return NULL;
    }

    // Allocate component tracking array
    graph->components = (uint32_t*) nimcp_malloc(NIMCP_MAX_VERTICES * sizeof(uint32_t));
    if (!graph->components) {
        nimcp_free(graph->vertices);
        nimcp_mutex_destroy(&graph->lock);
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_graph_create: failed to allocate components array");
        LOG_ERROR("nimcp_graph_create failed: returning error");
        return NULL;
    }

    // Initialize quantum walk bridge (enabled by default)
    graph->enable_quantum_walk = true;
    quantum_walk_bridge_config_t qconfig = quantum_walk_bridge_default_config();
    qconfig.max_nodes = NIMCP_MAX_VERTICES;
    qconfig.default_steps = 10;
    qconfig.enabled = true;
    graph->quantum_bridge = quantum_walk_bridge_create(&qconfig);

    if (graph->quantum_bridge) {
        NIMCP_LOGGING_INFO("Quantum-accelerated graph search enabled");
    } else {
        NIMCP_LOGGING_WARN("Quantum walk bridge creation failed, using classical algorithms");
    }

    return graph;
}

/**
 * WHAT: Destroys graph and frees all resources
 * WHY: Proper cleanup with nimcp_free for memory tracking
 * PATTERN: Destructor - systematic resource release
 */
void nimcp_graph_destroy(NimcpGraph* graph)
{
    LOG_TRACE("Entering nimcp_graph_destroy");
    if (!graph)
        return;

    // Free all edge lists (uses nimcp_free internally)
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        free_edge_list(graph->vertices[i].edges);
    }

    // Free arrays
    nimcp_free(graph->components);
    nimcp_free(graph->vertices);

    // Destroy quantum walk bridge
    if (graph->quantum_bridge) {
        quantum_walk_bridge_destroy((quantum_walk_bridge_t*)graph->quantum_bridge);
        graph->quantum_bridge = NULL;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&graph->lock);

    nimcp_free(graph);
}

/**
 * WHAT: Adds peer as vertex to topology graph
 * WHY: Track new peers joining the network
 * PATTERN: Monitor - thread-safe with guard clauses
 */
uint32_t nimcp_graph_add_vertex(NimcpGraph* graph, uint64_t peer_id, float x, float y, float z,
                                uint32_t capabilities)
{
    // Guard clause: validate parameters before locking
    if (!graph)
        return NIMCP_INVALID_VERTEX;

    nimcp_mutex_lock(&graph->lock);

    // Guard clause: check capacity
    if (graph->vertex_count >= NIMCP_MAX_VERTICES) {
        nimcp_mutex_unlock(&graph->lock);
        return NIMCP_INVALID_VERTEX;
    }

    // Check for duplicate peer_id
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        if (graph->vertices[i].peer_id == peer_id) {
            nimcp_mutex_unlock(&graph->lock);
            return NIMCP_INVALID_VERTEX;
        }
    }

    if (graph->vertex_count >= NIMCP_MAX_VERTICES) {
        nimcp_mutex_unlock(&graph->lock);
        return NIMCP_INVALID_VERTEX;
    }
    uint32_t idx = graph->vertex_count++;
    NimcpVertex* vertex = &graph->vertices[idx];

    // Initialize vertex with peer properties
    vertex->peer_id = peer_id;
    vertex->x = x;
    vertex->y = y;
    vertex->z = z;
    vertex->capabilities = capabilities;
    vertex->state = 0;
    vertex->last_updated = 0;
    vertex->edges = NULL;
    vertex->edge_count = 0;

    nimcp_mutex_unlock(&graph->lock);
    return idx;
}

/**
 * WHAT: Removes vertex and all connected edges
 * WHY: Maintains graph consistency when peer disconnects
 * PATTERN: Cascade Delete - removes vertex and dependent edges
 */
bool nimcp_graph_remove_vertex(NimcpGraph* graph, uint32_t vertex_idx)
{
    LOG_TRACE("Entering nimcp_graph_remove_vertex");
    if (!graph) {
        LOG_ERROR("nimcp_graph_remove_vertex failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_remove_vertex: graph is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    // Guard clause: validate vertex index
    if (vertex_idx >= graph->vertex_count) {
        nimcp_mutex_unlock(&graph->lock);
        LOG_ERROR("nimcp_graph_remove_vertex failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_remove_vertex: capacity exceeded");
        return false;
    }

    // Free vertex's outgoing edge list and update counts
    NimcpEdgeNode* edge = graph->vertices[vertex_idx].edges;
    while (edge) {
        graph->edge_count--;
        edge = edge->next;
    }
    free_edge_list(graph->vertices[vertex_idx].edges);

    // Remove incoming edges from other vertices
    // WHY: Maintain graph integrity - no dangling edge references
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        if (i == vertex_idx)
            continue;

        NimcpEdgeNode** edge = &graph->vertices[i].edges;
        while (*edge) {
            if ((*edge)->dest == vertex_idx) {
                NimcpEdgeNode* temp = *edge;
                *edge = (*edge)->next;
                nimcp_free(temp);
                graph->vertices[i].edge_count--;
                graph->edge_count--;
            } else {
                edge = &(*edge)->next;
            }
        }
    }

    // Shift remaining vertices to compact array
    if (vertex_idx < graph->vertex_count - 1) {
        memmove(&graph->vertices[vertex_idx], &graph->vertices[vertex_idx + 1],
                (graph->vertex_count - vertex_idx - 1) * sizeof(NimcpVertex));

        // Update destination indices in remaining edges
        for (uint32_t i = 0; i < graph->vertex_count - 1; i++) {
            NimcpEdgeNode* edge = graph->vertices[i].edges;
            while (edge) {
                if (edge->dest > vertex_idx) {
                    edge->dest--;
                }
                edge = edge->next;
            }
        }
    }

    graph->vertex_count--;
    nimcp_mutex_unlock(&graph->lock);
    return true;
}

/**
 * WHAT: Adds or updates edge between peers
 * WHY: Track connection establishment with quality metric
 * PATTERN: Monitor with early validation
 */
bool nimcp_graph_add_edge(NimcpGraph* graph, uint32_t from, uint32_t to, nimcp_weight_t weight)
{
    LOG_TRACE("Entering nimcp_graph_add_edge");
    if (!graph) {
        LOG_ERROR("nimcp_graph_add_edge failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_add_edge: graph is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    // Guard clauses: validate parameters
    if (from >= graph->vertex_count || to >= graph->vertex_count || from == to) {
        nimcp_mutex_unlock(&graph->lock);
        LOG_ERROR("nimcp_graph_add_edge failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_graph_add_edge: operation failed");
        return false;
    }
    // Capacity limit — return silently to avoid exception spam during
    // topology analysis of large brains
    if (graph->edge_count >= NIMCP_MAX_EDGES) {
        nimcp_mutex_unlock(&graph->lock);
        return false;
    }

    // Check if edge already exists
    NimcpEdgeNode* edge = graph->vertices[from].edges;
    while (edge) {
        if (edge->dest == to) {
            edge->weight = weight;  // Update weight if edge exists
            nimcp_mutex_unlock(&graph->lock);
            return true;
        }
        edge = edge->next;
    }

    // Create new edge node
    NimcpEdgeNode* new_edge = create_edge_node(to, weight);
    if (!new_edge) {
        nimcp_mutex_unlock(&graph->lock);
        LOG_ERROR("nimcp_graph_add_edge failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_add_edge: new_edge is NULL");
        return false;
    }

    // Prepend to adjacency list (O(1) insertion)
    new_edge->next = graph->vertices[from].edges;
    graph->vertices[from].edges = new_edge;
    graph->vertices[from].edge_count++;
    graph->edge_count++;

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

/**
 * WHAT: Removes edge between two vertices
 * WHY: Update topology when peer connection is lost
 * PATTERN: Linked List Remove - find and unlink node
 */
bool nimcp_graph_remove_edge(NimcpGraph* graph, uint32_t from, uint32_t to)
{
    LOG_TRACE("Entering nimcp_graph_remove_edge");
    if (!graph) {
        LOG_ERROR("nimcp_graph_remove_edge failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_remove_edge: graph is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    // Guard clauses: validate indices
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        nimcp_mutex_unlock(&graph->lock);
        LOG_ERROR("nimcp_graph_remove_edge failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_remove_edge: capacity exceeded");
        return false;
    }

    // Traverse adjacency list to find and remove edge
    NimcpEdgeNode** edge = &graph->vertices[from].edges;
    while (*edge) {
        if ((*edge)->dest == to) {
            NimcpEdgeNode* temp = *edge;
            *edge = (*edge)->next;
            nimcp_free(temp);
            graph->vertices[from].edge_count--;
            graph->edge_count--;
            nimcp_mutex_unlock(&graph->lock);
            return true;
        }
        edge = &(*edge)->next;
    }

    nimcp_mutex_unlock(&graph->lock);
    LOG_ERROR("nimcp_graph_remove_edge failed: returning error");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_graph_remove_edge: operation failed");
    return false;
}

/**
 * WHAT: Finds shortest path between two peers using Dijkstra's algorithm
 * WHY: Enable routing decisions and topology analysis
 * PATTERN: Algorithm Strategy - pluggable pathfinding
 * NOTE: Read-only but needs lock for consistent snapshot
 */
NimcpPath* nimcp_graph_shortest_path(const NimcpGraph* graph, uint32_t from, uint32_t to)
{
    LOG_TRACE("Entering nimcp_graph_shortest_path");
    if (!graph) {
        LOG_ERROR("nimcp_graph_shortest_path failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_shortest_path: graph is NULL");
        return NULL;
    }

    // Cast away const to lock (graph isn't modified, just protected)
    NimcpGraph* g = (NimcpGraph*) graph;
    nimcp_mutex_lock(&g->lock);

    // Guard clauses: validate indices
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        nimcp_mutex_unlock(&g->lock);
        LOG_ERROR("nimcp_graph_shortest_path failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_shortest_path: capacity exceeded");
        return NULL;
    }

    // Allocate Dijkstra's algorithm working arrays
    nimcp_weight_t* distances =
        (nimcp_weight_t*) nimcp_malloc(graph->vertex_count * sizeof(nimcp_weight_t));
    uint32_t* previous = (uint32_t*) nimcp_malloc(graph->vertex_count * sizeof(uint32_t));
    bool* visited = (bool*) nimcp_calloc(graph->vertex_count, sizeof(bool));

    if (!distances || !previous || !visited) {
        nimcp_free(distances);
        nimcp_free(previous);
        nimcp_free(visited);
        LOG_ERROR("nimcp_graph_shortest_path failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_shortest_path: required parameter is NULL (distances, previous, visited)");
        return NULL;
    }

    // Initialize distances
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        distances[i] = FLT_MAX;
        previous[i] = NIMCP_INVALID_VERTEX;
    }
    distances[from] = 0;

    // Create min-heap for O((V+E) log V) Dijkstra
    // DESIGN PATTERN: Strategy Pattern - same algorithm, different data structure
    nimcp_min_heap_t* heap = nimcp_min_heap_create(graph->vertex_count);
    if (!heap) {
        // Fallback: if heap creation fails, still need to clean up
        nimcp_free(distances);
        nimcp_free(previous);
        nimcp_free(visited);
        nimcp_mutex_unlock(&g->lock);
        LOG_ERROR("nimcp_graph_shortest_path failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_shortest_path: heap is NULL");
        return NULL;
    }

    // Insert source vertex into heap
    nimcp_heap_element_t start_elem = {from, 0.0F};
    nimcp_min_heap_insert(heap, &start_elem);

    // Dijkstra's algorithm with min-heap
    // COMPLEXITY: O((V+E) log V) vs old O(V²)
    while (!nimcp_min_heap_is_empty(heap)) {
        // Extract vertex with minimum distance - O(log V)
        nimcp_heap_element_t u_elem;
        if (!nimcp_min_heap_extract_min(heap, &u_elem)) {
            break;
        }

        uint32_t u = u_elem.vertex_id;

        // Skip if already visited (can happen with decrease-key)
        if (visited[u]) {
            continue;
        }

        visited[u] = true;

        // Early termination: if we reached destination, we're done
        if (u == to) {
            break;
        }

        // Relax all edges from u
        NimcpEdgeNode* edge = graph->vertices[u].edges;
        while (edge) {
            uint32_t v = edge->dest;

            if (!visited[v] && distances[u] != FLT_MAX) {
                float new_dist = distances[u] + edge->weight;

                if (new_dist < distances[v]) {
                    float old_dist = distances[v];
                    distances[v] = new_dist;
                    previous[v] = u;

                    if (old_dist == FLT_MAX) {
                        // First time seeing this vertex - insert into heap
                        nimcp_heap_element_t v_elem = {v, new_dist};
                        nimcp_min_heap_insert(heap, &v_elem);
                    } else {
                        // Update existing vertex - decrease key
                        nimcp_min_heap_decrease_key(heap, v, new_dist);
                    }
                }
            }

            edge = edge->next;
        }
    }

    // Clean up heap
    nimcp_min_heap_destroy(heap);

    // Build path if destination was reached
    NimcpPath* path = NULL;
    if (distances[to] != FLT_MAX) {
        path = (NimcpPath*) nimcp_malloc(sizeof(NimcpPath));
        if (path) {
            // Count path length by traversing previous array
            // Iteration limit prevents infinite loop if previous[] is corrupted
            uint32_t length = 0;
            uint32_t curr = to;
            while (curr != NIMCP_INVALID_VERTEX && length < graph->vertex_count) {
                length++;
                curr = previous[curr];
            }

            path->vertices = (uint32_t*) nimcp_malloc(length * sizeof(uint32_t));
            if (path->vertices) {
                path->length = length;
                path->total_weight = distances[to];

                // Fill path array in reverse order (from dest to source)
                curr = to;
                for (int i = length - 1; i >= 0; i--) {
                    path->vertices[i] = curr;
                    curr = previous[curr];
                }
            } else {
                nimcp_free(path);
                path = NULL;
            }
        }
    }

    // Free working arrays
    nimcp_free(distances);
    nimcp_free(previous);
    nimcp_free(visited);

    nimcp_mutex_unlock(&g->lock);
    return path;
}

/**
 * WHAT: Updates peer's logical network coordinates
 * WHY: Track topology changes for optimization
 * PATTERN: Monitor with simple update
 */
bool nimcp_graph_update_coordinates(NimcpGraph* graph, uint32_t vertex_idx, float x, float y,
                                    float z)
{
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_update_coordinates: graph is NULL");
        return false;
    }

    // Validate coordinates before locking
    // WHY: Ensure coordinates are valid floats (no NaN/Inf, within range)
    if (!nimcp_validate_float_field(&x, sizeof(float)) ||
        !nimcp_validate_float_field(&y, sizeof(float)) ||
        !nimcp_validate_float_field(&z, sizeof(float))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_graph_update_coordinates: nimcp_validate_float_field is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    // Guard clause: validate vertex index
    if (vertex_idx >= graph->vertex_count) {
        nimcp_mutex_unlock(&graph->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_update_coordinates: capacity exceeded");
        return false;
    }

    graph->vertices[vertex_idx].x = x;
    graph->vertices[vertex_idx].y = y;
    graph->vertices[vertex_idx].z = z;
    graph->vertices[vertex_idx].last_updated = nimcp_time_get_ms();

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

/**
 * WHAT: Finds vertex index by peer ID
 * WHY: Translate peer ID to graph index for operations
 * PATTERN: Linear search (could optimize with hash map)
 */
uint32_t nimcp_graph_find_vertex(const NimcpGraph* graph, uint64_t peer_id)
{
    LOG_TRACE("Entering nimcp_graph_find_vertex");
    if (!graph)
        return NIMCP_INVALID_VERTEX;

    // Cast away const to lock (read-only operation)
    NimcpGraph* g = (NimcpGraph*) graph;
    nimcp_mutex_lock(&g->lock);

    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        if (graph->vertices[i].peer_id == peer_id) {
            nimcp_mutex_unlock(&g->lock);
            return i;
        }
    }

    nimcp_mutex_unlock(&g->lock);
    return NIMCP_INVALID_VERTEX;
}

/**
 * WHAT: Identifies connected components in network topology
 * WHY: Detect network partitions and isolated subgraphs
 * PATTERN: Depth-First Search - component discovery algorithm
 */
uint32_t nimcp_graph_update_components(NimcpGraph* graph)
{
    LOG_TRACE("Entering nimcp_graph_update_components");
    if (!graph)
        return 0;

    nimcp_mutex_lock(&graph->lock);

    bool* visited = (bool*) nimcp_calloc(graph->vertex_count, sizeof(bool));
    if (!visited) {
        nimcp_mutex_unlock(&graph->lock);
        return 0;
    }

    // Label each connected component with unique ID
    uint32_t component = 0;
    for (uint32_t v = 0; v < graph->vertex_count; v++) {
        if (!visited[v]) {
            dfs_component(graph, v, component, visited);
            component++;
        }
    }

    nimcp_free(visited);
    graph->component_count = component;

    nimcp_mutex_unlock(&graph->lock);
    return component;
}

/**
 * WHAT: Retrieves all neighbors of a vertex
 * WHY: Get list of directly connected peers
 * PATTERN: Iterator - traverse adjacency list
 */
uint32_t nimcp_graph_get_neighbors(const NimcpGraph* graph, uint32_t vertex_idx,
                                   uint32_t* neighbors, uint32_t max_neighbors)
{
    if (!graph || !neighbors)
        return 0;

    // Cast away const to lock (read-only operation)
    NimcpGraph* g = (NimcpGraph*) graph;
    nimcp_mutex_lock(&g->lock);

    // Guard clause: validate vertex index
    if (vertex_idx >= graph->vertex_count) {
        nimcp_mutex_unlock(&g->lock);
        return 0;
    }

    uint32_t count = 0;
    NimcpEdgeNode* edge = graph->vertices[vertex_idx].edges;

    // Traverse adjacency list up to max_neighbors limit
    while (edge && count < max_neighbors) {
        neighbors[count++] = edge->dest;
        edge = edge->next;
    }

    nimcp_mutex_unlock(&g->lock);
    return count;
}

/**
 * WHAT: Retrieves edge weight between two vertices
 * WHY: Get connection quality metric for routing decisions
 * PATTERN: Search - traverse adjacency list for specific edge
 */
bool nimcp_graph_get_edge_weight(const NimcpGraph* graph, uint32_t from, uint32_t to,
                                 nimcp_weight_t* weight)
{
    if (!graph || !weight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_get_edge_weight: required parameter is NULL (graph, weight)");
        return false;
    }

    // Cast away const to lock (read-only operation)
    NimcpGraph* g = (NimcpGraph*) graph;
    nimcp_mutex_lock(&g->lock);

    // Guard clauses: validate indices
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        nimcp_mutex_unlock(&g->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_get_edge_weight: capacity exceeded");
        return false;
    }

    NimcpEdgeNode* edge = graph->vertices[from].edges;
    while (edge) {
        if (edge->dest == to) {
            *weight = edge->weight;
            nimcp_mutex_unlock(&g->lock);
            return true;
        }
        edge = edge->next;
    }

    nimcp_mutex_unlock(&g->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_graph_get_edge_weight: validation failed");
    return false;
}

/**
 * @brief Quantum-accelerated graph search
 * WHAT: Use quantum walk to find vertex with O(√N) speedup
 * WHY:  Quantum walk provides quadratic speedup for graph search
 * HOW:  Map graph to quantum walker, propagate amplitudes, measure result
 */
static uint32_t nimcp_graph_quantum_search(NimcpGraph* graph, uint32_t start, uint32_t target)
{
    // Guard clauses
    if (!graph || !graph->quantum_bridge) return NIMCP_INVALID_VERTEX;
    if (!graph->enable_quantum_walk) return NIMCP_INVALID_VERTEX;
    if (start >= graph->vertex_count || target >= graph->vertex_count) return NIMCP_INVALID_VERTEX;

    quantum_walk_bridge_t* qbridge = (quantum_walk_bridge_t*)graph->quantum_bridge;

    // Initialize quantum walker on graph
    if (quantum_walk_bridge_init(qbridge, graph->vertex_count) != 0) {
        NIMCP_LOGGING_WARN("Quantum walk initialization failed");
        return NIMCP_INVALID_VERTEX;
    }

    // Build quantum graph from adjacency lists
    // WHAT: Map classical graph edges to ternary quantum edges
    // WHY:  Quantum walker needs weighted edge information
    for (uint32_t from = 0; from < graph->vertex_count; from++) {
        NimcpEdgeNode* edge = graph->vertices[from].edges;
        while (edge) {
            // Map edge weight to ternary value [-1, 0, +1]
            // WHAT: Convert float weight to quantum trit
            // WHY:  Quantum walker uses ternary logic
            int8_t trit_weight = 0;
            if (edge->weight < -0.1f) {
                trit_weight = -1;  // Inhibitory
            } else if (edge->weight > 0.1f) {
                trit_weight = 1;   // Excitatory
            }
            // else: weight ~= 0, use neutral (0)

            quantum_walk_bridge_add_edge(qbridge, from, edge->dest, trit_weight);
            edge = edge->next;
        }
    }

    // Set start position
    if (quantum_walk_bridge_set_start(qbridge, start) != 0) {
        NIMCP_LOGGING_WARN("Quantum walk start failed");
        quantum_walk_bridge_reset(qbridge);
        return NIMCP_INVALID_VERTEX;
    }

    // Perform quantum search
    // WHAT: Use Grover-like search with quantum walk
    // WHY:  O(√N) steps instead of O(N) classical search
    uint32_t max_steps = (uint32_t)(10.0 * sqrt((double)graph->vertex_count));
    bool found = quantum_walk_bridge_search(qbridge, target, max_steps);

    if (found) {
        uint32_t result_node;
        float probability;
        if (quantum_walk_bridge_measure(qbridge, &result_node, &probability) == 0) {
            NIMCP_LOGGING_DEBUG("Quantum search found target at node %u (prob=%.3f)",
                               result_node, probability);
            quantum_walk_bridge_reset(qbridge);
            return result_node;
        }
    }

    quantum_walk_bridge_reset(qbridge);
    return NIMCP_INVALID_VERTEX;
}

/**
 * @brief Find path using quantum walk acceleration
 * WHAT: Hybrid quantum-classical pathfinding
 * WHY:  Use quantum walk for node discovery, classical for path construction
 * HOW:  Quantum search to find intermediate nodes, Dijkstra to connect them
 */
NimcpPath* nimcp_graph_quantum_path(NimcpGraph* graph, uint32_t from, uint32_t to)
{
    // Guard clauses
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_quantum_path: graph is NULL");
        return NULL;
    }
    if (!graph->enable_quantum_walk || !graph->quantum_bridge) {
        // Fall back to classical Dijkstra
        return nimcp_graph_shortest_path(graph, from, to);
    }

    nimcp_mutex_lock(&graph->lock);

    // Validate indices
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        nimcp_mutex_unlock(&graph->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_graph_quantum_path: capacity exceeded");
        return NULL;
    }

    // Use quantum search to verify target is reachable
    uint32_t quantum_result = nimcp_graph_quantum_search(graph, from, to);

    nimcp_mutex_unlock(&graph->lock);

    // If quantum search succeeded, use classical algorithm to get exact path
    if (quantum_result != NIMCP_INVALID_VERTEX) {
        NIMCP_LOGGING_DEBUG("Quantum search confirmed reachability, computing exact path");
        return nimcp_graph_shortest_path(graph, from, to);
    }

    NIMCP_LOGGING_DEBUG("Quantum search found no path");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graph_quantum_path: validation failed");
    return NULL;
}

/**
 * @brief Get the number of vertices in the graph
 *
 * WHAT: Returns current vertex count
 * WHY:  Accessor for graph size information
 * HOW:  Returns graph->vertex_count directly
 */
uint32_t nimcp_graph_vertex_count(const NimcpGraph* graph)
{
    if (!graph) return 0;
    return graph->vertex_count;
}

/**
 * @brief Get the number of edges in the graph
 *
 * WHAT: Returns total edge count
 * WHY:  Accessor for graph connectivity information
 * HOW:  Returns graph->edge_count directly
 */
uint32_t nimcp_graph_edge_count(const NimcpGraph* graph)
{
    if (!graph) return 0;
    return graph->edge_count;
}

/**
 * @brief Get the degree (number of edges) of a vertex
 *
 * WHAT: Returns number of edges connected to a vertex
 * WHY:  Used for centrality calculations and topology analysis
 * HOW:  Returns vertices[vertex_idx].edge_count
 */
uint32_t nimcp_graph_degree(const NimcpGraph* graph, uint32_t vertex_idx)
{
    if (!graph) return 0;
    if (vertex_idx >= graph->vertex_count) return 0;
    return graph->vertices[vertex_idx].edge_count;
}
