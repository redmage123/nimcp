/**
 * @file nimcp_graph_ternary.c
 * @brief Ternary Edge Weight Implementation for Network Graphs
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary edge weights {STRONG=+1, WEAK=0, ABSENT=-1} for graphs
 * WHY:  Memory-efficient edge classification with semantic meaning
 * HOW:  Adjacency lists with ternary weights and weighted path finding
 *
 * @author NIMCP Development Team
 */

#include "utils/containers/nimcp_graph_ternary.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graph_ternary)

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Map ternary weight to path cost
 */
static float weight_to_cost(const NimcpTernaryGraph* graph, trit_t weight) {
    switch (weight) {
        case TRIT_POSITIVE: return graph->strong_cost;
        case TRIT_UNKNOWN:  return graph->weak_cost;
        case TRIT_NEGATIVE: return graph->absent_cost;
        default:            return graph->weak_cost;
    }
}

/**
 * @brief Find edge in adjacency list
 */
static NimcpTernaryEdge* find_edge(
    NimcpTernaryVertex* vertex,
    uint32_t dest
) {
    NimcpTernaryEdge* edge = vertex->edges;
    while (edge) {
        if (edge->dest == dest) return edge;
        edge = edge->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_edge: validation failed");
    return NULL;
}

/**
 * @brief Update edge counts for vertex
 */
static void update_vertex_counts(NimcpTernaryVertex* vertex) {
    vertex->strong_count = 0;
    vertex->weak_count = 0;
    vertex->absent_count = 0;
    vertex->edge_count = 0;

    NimcpTernaryEdge* edge = vertex->edges;
    while (edge) {
        vertex->edge_count++;
        switch (edge->weight) {
            case TRIT_POSITIVE: vertex->strong_count++; break;
            case TRIT_UNKNOWN:  vertex->weak_count++;   break;
            case TRIT_NEGATIVE: vertex->absent_count++; break;
            default: break;
        }
        edge = edge->next;
    }
}

/**
 * @brief Update global graph statistics
 */
static void update_graph_stats(NimcpTernaryGraph* graph) {
    graph->total_strong = 0;
    graph->total_weak = 0;
    graph->total_absent = 0;
    graph->edge_count = 0;

    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        graph->total_strong += graph->vertices[i].strong_count;
        graph->total_weak += graph->vertices[i].weak_count;
        graph->total_absent += graph->vertices[i].absent_count;
        graph->edge_count += graph->vertices[i].edge_count;
    }

    /* Divide by 2 for undirected edges */
    graph->total_strong /= 2;
    graph->total_weak /= 2;
    graph->total_absent /= 2;
    graph->edge_count /= 2;
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

NimcpTernaryGraph* nimcp_ternary_graph_create(void) {
    NimcpTernaryGraph* graph = nimcp_malloc(sizeof(NimcpTernaryGraph));
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "graph is NULL");

        return NULL;

    }

    memset(graph, 0, sizeof(NimcpTernaryGraph));
    graph->magic = GRAPH_TERNARY_MAGIC;

    /* Allocate vertex array */
    graph->vertices = nimcp_malloc(NIMCP_MAX_VERTICES * sizeof(NimcpTernaryVertex));
    if (!graph->vertices) {
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ternary_graph_create: graph->vertices is NULL");
        return NULL;
    }
    memset(graph->vertices, 0, NIMCP_MAX_VERTICES * sizeof(NimcpTernaryVertex));

    /* Allocate components array */
    graph->components = nimcp_malloc(NIMCP_MAX_VERTICES * sizeof(uint32_t));
    if (!graph->components) {
        nimcp_free(graph->vertices);
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ternary_graph_create: graph->components is NULL");
        return NULL;
    }
    memset(graph->components, 0, NIMCP_MAX_VERTICES * sizeof(uint32_t));

    /* Initialize mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    if (nimcp_mutex_init(&graph->lock, &attr) != NIMCP_SUCCESS) {
        nimcp_free(graph->components);
        nimcp_free(graph->vertices);
        nimcp_free(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_ternary_graph_create: validation failed");
        return NULL;
    }

    /* Set default path costs */
    graph->strong_cost = 0.5f;
    graph->weak_cost = 1.0f;
    graph->absent_cost = 10.0f;
    graph->path_mode = GRAPH_PATH_AVOID_ABSENT;

    return graph;
}

void nimcp_ternary_graph_destroy(NimcpTernaryGraph* graph) {
    if (!graph) return;
    if (graph->magic != GRAPH_TERNARY_MAGIC) return;

    /* Free all edges */
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        NimcpTernaryEdge* edge = graph->vertices[i].edges;
        while (edge) {
            NimcpTernaryEdge* next = edge->next;
            nimcp_free(edge);
            edge = next;
        }
    }

    nimcp_mutex_destroy(&graph->lock);
    nimcp_free(graph->components);
    nimcp_free(graph->vertices);
    graph->magic = 0;
    nimcp_free(graph);
}

NimcpTernaryGraph* nimcp_ternary_graph_from_graph(
    const NimcpGraph* graph,
    float strong_threshold,
    float weak_threshold
) {
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;

    }

    NimcpTernaryGraph* tgraph = nimcp_ternary_graph_create();
    if (!tgraph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tgraph is NULL");

        return NULL;

    }

    /* Copy vertices */
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        const NimcpVertex* v = &graph->vertices[i];
        nimcp_ternary_graph_add_vertex(
            tgraph, v->peer_id, v->x, v->y, v->z, v->capabilities
        );
    }

    /* Copy edges with weight quantization */
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        NimcpEdgeNode* edge = graph->vertices[i].edges;
        while (edge) {
            /* Only add each edge once (i < dest) */
            if (i < edge->dest) {
                trit_t weight;
                if (edge->weight > strong_threshold) {
                    weight = TRIT_POSITIVE;
                } else if (edge->weight > weak_threshold) {
                    weight = TRIT_UNKNOWN;
                } else {
                    weight = TRIT_NEGATIVE;
                }
                nimcp_ternary_graph_add_edge(tgraph, i, edge->dest, weight);
            }
            edge = edge->next;
        }
    }

    return tgraph;
}

/*=============================================================================
 * Vertex Operations
 *===========================================================================*/

uint32_t nimcp_ternary_graph_add_vertex(
    NimcpTernaryGraph* graph,
    uint64_t peer_id,
    float x, float y, float z,
    uint32_t capabilities
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        return NIMCP_INVALID_VERTEX;
    }

    nimcp_mutex_lock(&graph->lock);

    if (graph->vertex_count >= NIMCP_MAX_VERTICES) {
        nimcp_mutex_unlock(&graph->lock);
        return NIMCP_INVALID_VERTEX;
    }

    uint32_t idx = graph->vertex_count++;
    NimcpTernaryVertex* v = &graph->vertices[idx];

    v->peer_id = peer_id;
    v->x = x;
    v->y = y;
    v->z = z;
    v->capabilities = capabilities;
    v->state = 0;
    v->last_updated = 0;
    v->edges = NULL;
    v->edge_count = 0;
    v->strong_count = 0;
    v->weak_count = 0;
    v->absent_count = 0;

    nimcp_mutex_unlock(&graph->lock);
    return idx;
}

bool nimcp_ternary_graph_remove_vertex(
    NimcpTernaryGraph* graph,
    uint32_t vertex_idx
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_remove_vertex: graph is NULL");
        return false;
    }
    if (vertex_idx >= graph->vertex_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_remove_vertex: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    /* Remove all edges to/from this vertex */
    NimcpTernaryVertex* v = &graph->vertices[vertex_idx];

    /* Free outgoing edges */
    NimcpTernaryEdge* edge = v->edges;
    while (edge) {
        /* Remove corresponding edge from neighbor */
        NimcpTernaryVertex* neighbor = &graph->vertices[edge->dest];
        NimcpTernaryEdge* prev = NULL;
        NimcpTernaryEdge* curr = neighbor->edges;
        while (curr) {
            if (curr->dest == vertex_idx) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    neighbor->edges = curr->next;
                }
                nimcp_free(curr);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        update_vertex_counts(neighbor);

        NimcpTernaryEdge* next = edge->next;
        nimcp_free(edge);
        edge = next;
    }

    /* Move last vertex to fill gap */
    uint32_t last_idx = graph->vertex_count - 1;
    if (vertex_idx != last_idx) {
        graph->vertices[vertex_idx] = graph->vertices[last_idx];

        /* Update edges pointing to moved vertex */
        NimcpTernaryEdge* e = graph->vertices[vertex_idx].edges;
        while (e) {
            NimcpTernaryEdge* neighbor_edge = find_edge(
                &graph->vertices[e->dest], last_idx
            );
            if (neighbor_edge) {
                neighbor_edge->dest = vertex_idx;
            }
            e = e->next;
        }
    }

    graph->vertex_count--;
    update_graph_stats(graph);

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

uint32_t nimcp_ternary_graph_find_vertex(
    const NimcpTernaryGraph* graph,
    uint64_t peer_id
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        return NIMCP_INVALID_VERTEX;
    }

    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        if (graph->vertices[i].peer_id == peer_id) {
            return i;
        }
    }

    return NIMCP_INVALID_VERTEX;
}

/*=============================================================================
 * Edge Operations
 *===========================================================================*/

bool nimcp_ternary_graph_add_edge(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t weight
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_add_edge: graph is NULL");
        return false;
    }
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_add_edge: capacity exceeded");
        return false;
    }
    if (from == to) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_add_edge: validation failed");
        return false;
    }
    if (!TRIT_IS_VALID(weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_add_edge: TRIT_IS_VALID is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    /* Check if edge already exists */
    if (find_edge(&graph->vertices[from], to)) {
        /* Update existing edge weight */
        NimcpTernaryEdge* edge = find_edge(&graph->vertices[from], to);
        edge->weight = weight;
        edge = find_edge(&graph->vertices[to], from);
        if (edge) edge->weight = weight;
        update_vertex_counts(&graph->vertices[from]);
        update_vertex_counts(&graph->vertices[to]);
        update_graph_stats(graph);
        nimcp_mutex_unlock(&graph->lock);
        return true;
    }

    /* Create new edges (bidirectional) */
    NimcpTernaryEdge* edge1 = nimcp_malloc(sizeof(NimcpTernaryEdge));
    NimcpTernaryEdge* edge2 = nimcp_malloc(sizeof(NimcpTernaryEdge));

    if (!edge1 || !edge2) {
        if (edge1) nimcp_free(edge1);
        if (edge2) nimcp_free(edge2);
        nimcp_mutex_unlock(&graph->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_add_edge: validation failed");
        return false;
    }

    /* Initialize forward edge */
    edge1->dest = to;
    edge1->weight = weight;
    edge1->flags = 0;
    edge1->last_updated = 0;
    edge1->next = graph->vertices[from].edges;
    graph->vertices[from].edges = edge1;

    /* Initialize reverse edge */
    edge2->dest = from;
    edge2->weight = weight;
    edge2->flags = 0;
    edge2->last_updated = 0;
    edge2->next = graph->vertices[to].edges;
    graph->vertices[to].edges = edge2;

    update_vertex_counts(&graph->vertices[from]);
    update_vertex_counts(&graph->vertices[to]);
    update_graph_stats(graph);

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

bool nimcp_ternary_graph_remove_edge(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_remove_edge: graph is NULL");
        return false;
    }
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_remove_edge: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    /* Remove from -> to */
    NimcpTernaryEdge* prev = NULL;
    NimcpTernaryEdge* curr = graph->vertices[from].edges;
    while (curr) {
        if (curr->dest == to) {
            if (prev) {
                prev->next = curr->next;
            } else {
                graph->vertices[from].edges = curr->next;
            }
            nimcp_free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    /* Remove to -> from */
    prev = NULL;
    curr = graph->vertices[to].edges;
    while (curr) {
        if (curr->dest == from) {
            if (prev) {
                prev->next = curr->next;
            } else {
                graph->vertices[to].edges = curr->next;
            }
            nimcp_free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    update_vertex_counts(&graph->vertices[from]);
    update_vertex_counts(&graph->vertices[to]);
    update_graph_stats(graph);

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

bool nimcp_ternary_graph_get_edge_weight(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t* weight
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_get_edge_weight: graph is NULL");
        return false;
    }
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_get_edge_weight: capacity exceeded");
        return false;
    }
    if (!weight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_get_edge_weight: weight is NULL");
        return false;
    }

    NimcpTernaryEdge* edge = find_edge(
        (NimcpTernaryVertex*)&graph->vertices[from], to
    );

    if (edge) {
        *weight = edge->weight;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_get_edge_weight: validation failed");
    return false;
}

bool nimcp_ternary_graph_set_edge_weight(
    NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to,
    trit_t weight
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_set_edge_weight: graph is NULL");
        return false;
    }
    if (!TRIT_IS_VALID(weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_set_edge_weight: TRIT_IS_VALID is NULL");
        return false;
    }

    nimcp_mutex_lock(&graph->lock);

    NimcpTernaryEdge* edge1 = find_edge(&graph->vertices[from], to);
    NimcpTernaryEdge* edge2 = find_edge(&graph->vertices[to], from);

    if (!edge1 || !edge2) {
        nimcp_mutex_unlock(&graph->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_set_edge_weight: required parameter is NULL (edge1, edge2)");
        return false;
    }

    edge1->weight = weight;
    edge2->weight = weight;

    update_vertex_counts(&graph->vertices[from]);
    update_vertex_counts(&graph->vertices[to]);
    update_graph_stats(graph);

    nimcp_mutex_unlock(&graph->lock);
    return true;
}

bool nimcp_ternary_graph_has_edge(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        return false;
    }
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        return false;
    }

    return find_edge((NimcpTernaryVertex*)&graph->vertices[from], to) != NULL;
}

/*=============================================================================
 * Path Finding
 *===========================================================================*/

NimcpPath* nimcp_ternary_graph_shortest_path(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_shortest_path: graph is NULL");
        return NULL;
    }
    if (from >= graph->vertex_count || to >= graph->vertex_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_shortest_path: capacity exceeded");
        return NULL;
    }
    if (from == to) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_shortest_path: validation failed");
        return NULL;
    }

    uint32_t n = graph->vertex_count;

    /* Allocate Dijkstra arrays */
    float* dist = nimcp_malloc(n * sizeof(float));
    uint32_t* prev = nimcp_malloc(n * sizeof(uint32_t));
    bool* visited = nimcp_malloc(n * sizeof(bool));

    if (!dist || !prev || !visited) {
        if (dist) nimcp_free(dist);
        if (prev) nimcp_free(prev);
        if (visited) nimcp_free(visited);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ternary_graph_shortest_path: validation failed");
        return NULL;
    }

    /* Initialize */
    for (uint32_t i = 0; i < n; i++) {
        dist[i] = FLT_MAX;
        prev[i] = NIMCP_INVALID_VERTEX;
        visited[i] = false;
    }
    dist[from] = 0.0f;

    /* Dijkstra's algorithm */
    for (uint32_t iter = 0; iter < n; iter++) {
        /* Find unvisited vertex with minimum distance */
        uint32_t u = NIMCP_INVALID_VERTEX;
        float min_dist = FLT_MAX;
        for (uint32_t i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < min_dist) {
                min_dist = dist[i];
                u = i;
            }
        }

        if (u == NIMCP_INVALID_VERTEX || u == to) break;
        visited[u] = true;

        /* Relax edges */
        NimcpTernaryEdge* edge = graph->vertices[u].edges;
        while (edge) {
            uint32_t v = edge->dest;
            float cost = weight_to_cost(graph, edge->weight);
            float alt = dist[u] + cost;

            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
            }
            edge = edge->next;
        }
    }

    /* Check if path exists */
    if (prev[to] == NIMCP_INVALID_VERTEX && from != to) {
        nimcp_free(dist);
        nimcp_free(prev);
        nimcp_free(visited);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_shortest_path: validation failed");
        return NULL;
    }

    /* Build path */
    uint32_t path_len = 0;
    uint32_t curr = to;
    while (curr != NIMCP_INVALID_VERTEX) {
        path_len++;
        curr = prev[curr];
    }

    NimcpPath* path = nimcp_malloc(sizeof(NimcpPath));
    if (!path) {
        nimcp_free(dist);
        nimcp_free(prev);
        nimcp_free(visited);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ternary_graph_shortest_path: path is NULL");
        return NULL;
    }

    path->vertices = nimcp_malloc(path_len * sizeof(uint32_t));
    if (!path->vertices) {
        nimcp_free(path);
        nimcp_free(dist);
        nimcp_free(prev);
        nimcp_free(visited);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ternary_graph_shortest_path: path->vertices is NULL");
        return NULL;
    }

    path->length = path_len;
    path->total_weight = dist[to];

    /* Fill path in reverse */
    curr = to;
    for (uint32_t i = path_len; i > 0 && curr != NIMCP_INVALID_VERTEX; i--) {
        path->vertices[i - 1] = curr;
        curr = prev[curr];
    }

    nimcp_free(dist);
    nimcp_free(prev);
    nimcp_free(visited);

    return path;
}

NimcpPath* nimcp_ternary_graph_strong_path(
    const NimcpTernaryGraph* graph,
    uint32_t from,
    uint32_t to
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_strong_path: graph is NULL");
        return NULL;
    }

    /* Temporarily modify costs to strongly prefer strong edges */
    NimcpTernaryGraph* mutable_graph = (NimcpTernaryGraph*)graph;
    float old_weak = mutable_graph->weak_cost;
    float old_absent = mutable_graph->absent_cost;

    mutable_graph->weak_cost = FLT_MAX / 2;
    mutable_graph->absent_cost = FLT_MAX / 2;

    NimcpPath* path = nimcp_ternary_graph_shortest_path(graph, from, to);

    /* Restore costs */
    mutable_graph->weak_cost = old_weak;
    mutable_graph->absent_cost = old_absent;

    /* Verify path uses only strong edges */
    if (path && path->total_weight >= FLT_MAX / 4) {
        /* Path includes non-strong edges */
        nimcp_free(path->vertices);
        nimcp_free(path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_ternary_graph_strong_path: capacity exceeded");
        return NULL;
    }

    return path;
}

void nimcp_ternary_graph_set_path_costs(
    NimcpTernaryGraph* graph,
    float strong_cost,
    float weak_cost,
    float absent_cost
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) return;

    graph->strong_cost = strong_cost;
    graph->weak_cost = weak_cost;
    graph->absent_cost = absent_cost;
}

/*=============================================================================
 * Neighbor Operations
 *===========================================================================*/

uint32_t nimcp_ternary_graph_get_neighbors_by_weight(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    trit_t weight_filter,
    uint32_t* neighbors,
    uint32_t max_neighbors
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) return 0;
    if (vertex_idx >= graph->vertex_count) return 0;
    if (!neighbors || max_neighbors == 0) return 0;

    uint32_t count = 0;
    NimcpTernaryEdge* edge = graph->vertices[vertex_idx].edges;

    while (edge && count < max_neighbors) {
        /* Check weight filter (-2 means all) */
        if (weight_filter == -2 || edge->weight == weight_filter) {
            neighbors[count++] = edge->dest;
        }
        edge = edge->next;
    }

    return count;
}

uint32_t nimcp_ternary_graph_get_neighbors(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* neighbors,
    uint32_t max_neighbors
) {
    return nimcp_ternary_graph_get_neighbors_by_weight(
        graph, vertex_idx, -2, neighbors, max_neighbors
    );
}

uint32_t nimcp_ternary_graph_get_strong_neighbors(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* neighbors,
    uint32_t max_neighbors
) {
    return nimcp_ternary_graph_get_neighbors_by_weight(
        graph, vertex_idx, TRIT_POSITIVE, neighbors, max_neighbors
    );
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

void nimcp_ternary_graph_stats(
    const NimcpTernaryGraph* graph,
    uint32_t* total_strong,
    uint32_t* total_weak,
    uint32_t* total_absent
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        if (total_strong) *total_strong = 0;
        if (total_weak) *total_weak = 0;
        if (total_absent) *total_absent = 0;
        return;
    }

    if (total_strong) *total_strong = graph->total_strong;
    if (total_weak) *total_weak = graph->total_weak;
    if (total_absent) *total_absent = graph->total_absent;
}

void nimcp_ternary_graph_vertex_degrees(
    const NimcpTernaryGraph* graph,
    uint32_t vertex_idx,
    uint32_t* strong_degree,
    uint32_t* weak_degree,
    uint32_t* absent_degree
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC ||
        vertex_idx >= graph->vertex_count) {
        if (strong_degree) *strong_degree = 0;
        if (weak_degree) *weak_degree = 0;
        if (absent_degree) *absent_degree = 0;
        return;
    }

    const NimcpTernaryVertex* v = &graph->vertices[vertex_idx];
    if (strong_degree) *strong_degree = v->strong_count;
    if (weak_degree) *weak_degree = v->weak_count;
    if (absent_degree) *absent_degree = v->absent_count;
}

uint32_t nimcp_ternary_graph_update_components(NimcpTernaryGraph* graph) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) return 0;

    nimcp_mutex_lock(&graph->lock);

    uint32_t n = graph->vertex_count;

    /* Reset components */
    for (uint32_t i = 0; i < n; i++) {
        graph->components[i] = UINT32_MAX;
    }

    uint32_t component = 0;
    uint32_t* stack = nimcp_malloc(n * sizeof(uint32_t));
    if (!stack) {
        nimcp_mutex_unlock(&graph->lock);
        return 0;
    }

    for (uint32_t start = 0; start < n; start++) {
        if (graph->components[start] != UINT32_MAX) continue;

        /* DFS from this vertex */
        uint32_t stack_top = 0;
        stack[stack_top++] = start;

        while (stack_top > 0) {
            uint32_t v = stack[--stack_top];
            if (graph->components[v] != UINT32_MAX) continue;

            graph->components[v] = component;

            /* Add unvisited neighbors */
            NimcpTernaryEdge* edge = graph->vertices[v].edges;
            while (edge) {
                if (graph->components[edge->dest] == UINT32_MAX) {
                    stack[stack_top++] = edge->dest;
                }
                edge = edge->next;
            }
        }

        component++;
    }

    nimcp_free(stack);
    graph->component_count = component;

    nimcp_mutex_unlock(&graph->lock);
    return component;
}

/*=============================================================================
 * Conversion
 *===========================================================================*/

trit_matrix_t* nimcp_ternary_graph_to_matrix(
    const NimcpTernaryGraph* graph,
    ternary_pack_mode_t pack_mode
) {
    if (!graph || graph->magic != GRAPH_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_to_matrix: graph is NULL");
        return NULL;
    }

    uint32_t n = graph->vertex_count;
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ternary_graph_to_matrix: n is zero");
        return NULL;
    }

    trit_matrix_t* mat = trit_matrix_create(n, n, pack_mode);
    if (!mat) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mat is NULL");

        return NULL;

    }

    /* Initialize to all zeros (no edges) */
    trit_matrix_fill(mat, TRIT_UNKNOWN);

    /* Copy edges */
    for (uint32_t i = 0; i < n; i++) {
        NimcpTernaryEdge* edge = graph->vertices[i].edges;
        while (edge) {
            trit_matrix_set(mat, i, edge->dest, edge->weight);
            edge = edge->next;
        }
    }

    return mat;
}

NimcpTernaryGraph* nimcp_ternary_graph_from_matrix(
    const trit_matrix_t* adjacency
) {
    if (!adjacency || adjacency->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_from_matrix: adjacency is NULL");
        return NULL;
    }
    if (adjacency->rows != adjacency->cols) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ternary_graph_from_matrix: validation failed");
        return NULL;
    }

    uint32_t n = adjacency->rows;

    NimcpTernaryGraph* graph = nimcp_ternary_graph_create();
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;

    }

    /* Add vertices */
    for (uint32_t i = 0; i < n; i++) {
        nimcp_ternary_graph_add_vertex(graph, i, 0, 0, 0, 0);
    }

    /* Add edges (only upper triangle to avoid duplicates) */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            trit_t w = trit_matrix_get(adjacency, i, j);
            if (w != TRIT_UNKNOWN) {
                nimcp_ternary_graph_add_edge(graph, i, j, w);
            }
        }
    }

    return graph;
}
