/**
 * @file nimcp_graph_theory.h
 * @brief Graph theory engine: representations, traversals, shortest paths,
 *        MST, coloring, planarity, max flow, connectivity.
 *
 * Dual representation (adjacency matrix + adjacency list). BFS, DFS,
 * topological sort. Dijkstra, Bellman-Ford. Kruskal, Prim. Greedy + backtracking
 * coloring. Euler formula planarity check. Edmonds-Karp max flow. Connected
 * components, bridges, articulation points. Bipartiteness, degree sequence,
 * complement, subgraph isomorphism.
 */

#ifndef NIMCP_GRAPH_THEORY_H
#define NIMCP_GRAPH_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define GT_MAX_VERTICES    256
#define GT_MAX_EDGES       1024
#define GT_INF_WEIGHT      1.0e18
#define GT_MAX_COLORS      64

/* --------------------------------------------------------------------------
 * Graph representation
 * -------------------------------------------------------------------------- */

typedef struct {
    uint16_t to;
    double   weight;
} gt_adj_entry_t;

typedef struct {
    /* Adjacency matrix (dense) */
    double   adj_matrix[GT_MAX_VERTICES][GT_MAX_VERTICES]; /* 0 = no edge, else weight */

    /* Adjacency list (sparse) */
    gt_adj_entry_t *adj_list[GT_MAX_VERTICES]; /* dynamic arrays per vertex */
    uint16_t        adj_count[GT_MAX_VERTICES]; /* number of neighbors      */
    uint16_t        adj_cap[GT_MAX_VERTICES];   /* capacity of adj_list[v]  */

    uint16_t num_vertices;
    uint32_t num_edges;
    bool     directed;
    bool     weighted;
} graph_t;

/* --------------------------------------------------------------------------
 * Edge (for Kruskal, etc.)
 * -------------------------------------------------------------------------- */

typedef struct {
    uint16_t u, v;
    double   weight;
} gt_edge_t;

/* --------------------------------------------------------------------------
 * Traversal / path results
 * -------------------------------------------------------------------------- */

typedef struct {
    uint16_t order[GT_MAX_VERTICES];   /* visit order */
    uint16_t parent[GT_MAX_VERTICES];  /* BFS/DFS tree parent (UINT16_MAX=root) */
    uint32_t count;                    /* number of vertices visited */
} gt_traversal_t;

typedef struct {
    double   dist[GT_MAX_VERTICES];    /* shortest distance from source */
    uint16_t prev[GT_MAX_VERTICES];    /* predecessor on shortest path  */
    bool     reachable[GT_MAX_VERTICES];
    bool     has_negative_cycle;       /* Bellman-Ford only */
} gt_shortest_path_t;

typedef struct {
    gt_edge_t edges[GT_MAX_VERTICES];
    uint32_t  num_edges;
    double    total_weight;
} gt_mst_result_t;

/* --------------------------------------------------------------------------
 * Coloring
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t  color[GT_MAX_VERTICES];   /* color assigned to each vertex */
    uint32_t num_colors;               /* chromatic number (or upper bound) */
    bool     optimal;                  /* true if proven chromatic number */
} gt_coloring_t;

/* --------------------------------------------------------------------------
 * Max flow
 * -------------------------------------------------------------------------- */

typedef struct {
    double   max_flow;
    double   flow[GT_MAX_VERTICES][GT_MAX_VERTICES]; /* flow on each edge */
} gt_flow_result_t;

/* --------------------------------------------------------------------------
 * Connectivity
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t  component[GT_MAX_VERTICES];
    uint32_t num_components;
} gt_components_t;

typedef struct {
    bool     is_bridge[GT_MAX_EDGES];
    uint32_t bridge_count;
    gt_edge_t bridges[GT_MAX_EDGES];
} gt_bridges_t;

typedef struct {
    bool     is_articulation[GT_MAX_VERTICES];
    uint32_t count;
    uint16_t points[GT_MAX_VERTICES];
} gt_articulation_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

graph_t *gt_create(uint16_t num_vertices, bool directed, bool weighted);
void     gt_destroy(graph_t *g);
graph_t *gt_clone(const graph_t *g);

/* --------------------------------------------------------------------------
 * Edge operations
 * -------------------------------------------------------------------------- */

bool gt_add_edge(graph_t *g, uint16_t u, uint16_t v, double weight);
bool gt_remove_edge(graph_t *g, uint16_t u, uint16_t v);
bool gt_has_edge(const graph_t *g, uint16_t u, uint16_t v);
double gt_edge_weight(const graph_t *g, uint16_t u, uint16_t v);

/* --------------------------------------------------------------------------
 * Traversals
 * -------------------------------------------------------------------------- */

gt_traversal_t gt_bfs(const graph_t *g, uint16_t source);
gt_traversal_t gt_dfs(const graph_t *g, uint16_t source);
bool gt_topological_sort(const graph_t *g, uint16_t *order, uint32_t *count);

/* --------------------------------------------------------------------------
 * Shortest paths
 * -------------------------------------------------------------------------- */

gt_shortest_path_t gt_dijkstra(const graph_t *g, uint16_t source);
gt_shortest_path_t gt_bellman_ford(const graph_t *g, uint16_t source);

/* --------------------------------------------------------------------------
 * Minimum spanning tree
 * -------------------------------------------------------------------------- */

gt_mst_result_t gt_kruskal(const graph_t *g);
gt_mst_result_t gt_prim(const graph_t *g, uint16_t start);

/* --------------------------------------------------------------------------
 * Coloring
 * -------------------------------------------------------------------------- */

gt_coloring_t gt_greedy_coloring(const graph_t *g);
gt_coloring_t gt_chromatic_number(const graph_t *g); /* backtracking exact */

/* --------------------------------------------------------------------------
 * Max flow (Edmonds-Karp)
 * -------------------------------------------------------------------------- */

gt_flow_result_t gt_max_flow(const graph_t *g, uint16_t source, uint16_t sink);

/* --------------------------------------------------------------------------
 * Connectivity
 * -------------------------------------------------------------------------- */

gt_components_t   gt_connected_components(const graph_t *g);
gt_bridges_t      gt_find_bridges(const graph_t *g);
gt_articulation_t gt_find_articulation_points(const graph_t *g);
bool              gt_is_bipartite(const graph_t *g);

/* --------------------------------------------------------------------------
 * Properties
 * -------------------------------------------------------------------------- */

uint16_t gt_degree(const graph_t *g, uint16_t v);
bool     gt_check_handshaking(const graph_t *g);

/** Euler formula planarity check: E <= 3V - 6 (necessary condition). */
bool gt_is_planar_euler(const graph_t *g);

/** Graph complement (unweighted). */
graph_t *gt_complement(const graph_t *g);

/** Brute-force subgraph isomorphism for small graphs. */
bool gt_subgraph_isomorphism(const graph_t *g, const graph_t *pattern,
                             uint16_t *mapping);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_THEORY_H */
