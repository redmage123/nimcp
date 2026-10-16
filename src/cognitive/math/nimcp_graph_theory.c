/**
 * @file nimcp_graph_theory.c
 * @brief Graph theory engine implementation
 */

#include "cognitive/math/nimcp_graph_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <float.h>

#define LOG_TAG "GRAPH_THEORY"

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

graph_t *gt_create(uint16_t num_vertices, bool directed, bool weighted) {
    if (num_vertices == 0 || num_vertices > GT_MAX_VERTICES) {
        LOG_ERROR("Vertex count %u out of range [1, %d]", num_vertices, GT_MAX_VERTICES);
        return NULL;
    }
    graph_t *g = (graph_t *)nimcp_calloc(1, sizeof(graph_t));
    if (!g) return NULL;

    g->num_vertices = num_vertices;
    g->num_edges = 0;
    g->directed = directed;
    g->weighted = weighted;

    /* Initialize adjacency matrix to zero (no edges) */
    memset(g->adj_matrix, 0, sizeof(g->adj_matrix));

    /* Allocate adjacency lists with initial capacity */
    for (uint16_t v = 0; v < num_vertices; v++) {
        g->adj_cap[v] = 16;
        g->adj_list[v] = (gt_adj_entry_t *)nimcp_calloc(g->adj_cap[v],
                                                         sizeof(gt_adj_entry_t));
        g->adj_count[v] = 0;
        if (!g->adj_list[v]) {
            for (uint16_t j = 0; j < v; j++) nimcp_free(g->adj_list[j]);
            nimcp_free(g);
            return NULL;
        }
    }

    LOG_INFO("Graph created: %u vertices, %s, %s",
             num_vertices, directed ? "directed" : "undirected",
             weighted ? "weighted" : "unweighted");
    return g;
}

void gt_destroy(graph_t *g) {
    if (!g) return;
    for (uint16_t v = 0; v < g->num_vertices; v++) {
        if (g->adj_list[v]) nimcp_free(g->adj_list[v]);
    }
    nimcp_free(g);
}

graph_t *gt_clone(const graph_t *g) {
    if (!g) return NULL;
    graph_t *c = gt_create(g->num_vertices, g->directed, g->weighted);
    if (!c) return NULL;
    memcpy(c->adj_matrix, g->adj_matrix, sizeof(g->adj_matrix));
    c->num_edges = g->num_edges;
    for (uint16_t v = 0; v < g->num_vertices; v++) {
        if (g->adj_count[v] > c->adj_cap[v]) {
            nimcp_free(c->adj_list[v]);
            c->adj_cap[v] = g->adj_count[v];
            c->adj_list[v] = (gt_adj_entry_t *)nimcp_calloc(c->adj_cap[v],
                                                             sizeof(gt_adj_entry_t));
        }
        c->adj_count[v] = g->adj_count[v];
        memcpy(c->adj_list[v], g->adj_list[v],
               g->adj_count[v] * sizeof(gt_adj_entry_t));
    }
    return c;
}

/* =========================================================================
 * Edge operations
 * ========================================================================= */

static bool adj_list_add(graph_t *g, uint16_t u, uint16_t v, double w) {
    if (g->adj_count[u] >= g->adj_cap[u]) {
        uint16_t new_cap = g->adj_cap[u] * 2;
        gt_adj_entry_t *new_list = (gt_adj_entry_t *)nimcp_calloc(
            new_cap, sizeof(gt_adj_entry_t));
        if (!new_list) return false;
        memcpy(new_list, g->adj_list[u],
               g->adj_count[u] * sizeof(gt_adj_entry_t));
        nimcp_free(g->adj_list[u]);
        g->adj_list[u] = new_list;
        g->adj_cap[u] = new_cap;
    }
    g->adj_list[u][g->adj_count[u]].to = v;
    g->adj_list[u][g->adj_count[u]].weight = w;
    g->adj_count[u]++;
    return true;
}

static void adj_list_remove(graph_t *g, uint16_t u, uint16_t v) {
    for (uint16_t i = 0; i < g->adj_count[u]; i++) {
        if (g->adj_list[u][i].to == v) {
            g->adj_list[u][i] = g->adj_list[u][g->adj_count[u] - 1];
            g->adj_count[u]--;
            return;
        }
    }
}

bool gt_add_edge(graph_t *g, uint16_t u, uint16_t v, double weight) {
    if (!g || u >= g->num_vertices || v >= g->num_vertices) return false;
    if (g->num_edges >= GT_MAX_EDGES) return false;

    double w = g->weighted ? weight : 1.0;
    g->adj_matrix[u][v] = w;
    adj_list_add(g, u, v, w);

    if (!g->directed) {
        g->adj_matrix[v][u] = w;
        adj_list_add(g, v, u, w);
    }
    g->num_edges++;
    return true;
}

bool gt_remove_edge(graph_t *g, uint16_t u, uint16_t v) {
    if (!g || u >= g->num_vertices || v >= g->num_vertices) return false;
    if (g->adj_matrix[u][v] == 0.0) return false;

    g->adj_matrix[u][v] = 0.0;
    adj_list_remove(g, u, v);
    if (!g->directed) {
        g->adj_matrix[v][u] = 0.0;
        adj_list_remove(g, v, u);
    }
    g->num_edges--;
    return true;
}

bool gt_has_edge(const graph_t *g, uint16_t u, uint16_t v) {
    if (!g || u >= g->num_vertices || v >= g->num_vertices) return false;
    return g->adj_matrix[u][v] != 0.0;
}

double gt_edge_weight(const graph_t *g, uint16_t u, uint16_t v) {
    if (!g || u >= g->num_vertices || v >= g->num_vertices) return 0.0;
    return g->adj_matrix[u][v];
}

/* =========================================================================
 * BFS / DFS
 * ========================================================================= */

gt_traversal_t gt_bfs(const graph_t *g, uint16_t source) {
    gt_traversal_t t;
    memset(&t, 0, sizeof(t));
    for (int i = 0; i < GT_MAX_VERTICES; i++) t.parent[i] = UINT16_MAX;
    if (!g || source >= g->num_vertices) return t;

    bool visited[GT_MAX_VERTICES] = {false};
    uint16_t queue[GT_MAX_VERTICES];
    uint32_t head = 0, tail = 0;

    visited[source] = true;
    queue[tail++] = source;
    t.parent[source] = UINT16_MAX;

    while (head < tail) {
        uint16_t v = queue[head++];
        t.order[t.count++] = v;
        for (uint16_t i = 0; i < g->adj_count[v]; i++) {
            uint16_t w = g->adj_list[v][i].to;
            if (!visited[w]) {
                visited[w] = true;
                t.parent[w] = v;
                queue[tail++] = w;
            }
        }
    }
    return t;
}

static void dfs_visit(const graph_t *g, uint16_t v, bool *visited,
                      gt_traversal_t *t) {
    visited[v] = true;
    t->order[t->count++] = v;
    for (uint16_t i = 0; i < g->adj_count[v]; i++) {
        uint16_t w = g->adj_list[v][i].to;
        if (!visited[w]) {
            t->parent[w] = v;
            dfs_visit(g, w, visited, t);
        }
    }
}

gt_traversal_t gt_dfs(const graph_t *g, uint16_t source) {
    gt_traversal_t t;
    memset(&t, 0, sizeof(t));
    for (int i = 0; i < GT_MAX_VERTICES; i++) t.parent[i] = UINT16_MAX;
    if (!g || source >= g->num_vertices) return t;

    bool visited[GT_MAX_VERTICES] = {false};
    dfs_visit(g, source, visited, &t);
    return t;
}

bool gt_topological_sort(const graph_t *g, uint16_t *order, uint32_t *count) {
    if (!g || !g->directed || !order || !count) return false;
    uint16_t n = g->num_vertices;

    /* Kahn's algorithm */
    uint16_t in_degree[GT_MAX_VERTICES] = {0};
    for (uint16_t v = 0; v < n; v++) {
        for (uint16_t i = 0; i < g->adj_count[v]; i++) {
            in_degree[g->adj_list[v][i].to]++;
        }
    }

    uint16_t queue[GT_MAX_VERTICES];
    uint32_t head = 0, tail = 0;
    for (uint16_t v = 0; v < n; v++) {
        if (in_degree[v] == 0) queue[tail++] = v;
    }

    *count = 0;
    while (head < tail) {
        uint16_t v = queue[head++];
        order[(*count)++] = v;
        for (uint16_t i = 0; i < g->adj_count[v]; i++) {
            uint16_t w = g->adj_list[v][i].to;
            if (--in_degree[w] == 0) queue[tail++] = w;
        }
    }

    return (*count == n); /* false if cycle detected */
}

/* =========================================================================
 * Shortest paths
 * ========================================================================= */

gt_shortest_path_t gt_dijkstra(const graph_t *g, uint16_t source) {
    gt_shortest_path_t sp;
    memset(&sp, 0, sizeof(sp));
    for (int i = 0; i < GT_MAX_VERTICES; i++) {
        sp.dist[i] = GT_INF_WEIGHT;
        sp.prev[i] = UINT16_MAX;
    }
    if (!g || source >= g->num_vertices) return sp;

    bool visited[GT_MAX_VERTICES] = {false};
    sp.dist[source] = 0.0;
    sp.reachable[source] = true;

    for (uint16_t iter = 0; iter < g->num_vertices; iter++) {
        /* Find unvisited vertex with min dist */
        double min_d = GT_INF_WEIGHT;
        uint16_t u = UINT16_MAX;
        for (uint16_t v = 0; v < g->num_vertices; v++) {
            if (!visited[v] && sp.dist[v] < min_d) {
                min_d = sp.dist[v];
                u = v;
            }
        }
        if (u == UINT16_MAX) break;
        visited[u] = true;

        for (uint16_t i = 0; i < g->adj_count[u]; i++) {
            uint16_t v = g->adj_list[u][i].to;
            double w = g->adj_list[u][i].weight;
            double alt = sp.dist[u] + w;
            if (alt < sp.dist[v]) {
                sp.dist[v] = alt;
                sp.prev[v] = u;
                sp.reachable[v] = true;
            }
        }
    }
    return sp;
}

gt_shortest_path_t gt_bellman_ford(const graph_t *g, uint16_t source) {
    gt_shortest_path_t sp;
    memset(&sp, 0, sizeof(sp));
    for (int i = 0; i < GT_MAX_VERTICES; i++) {
        sp.dist[i] = GT_INF_WEIGHT;
        sp.prev[i] = UINT16_MAX;
    }
    if (!g || source >= g->num_vertices) return sp;

    sp.dist[source] = 0.0;
    sp.reachable[source] = true;
    uint16_t n = g->num_vertices;

    /* Relax all edges n-1 times */
    for (uint16_t iter = 0; iter < n - 1; iter++) {
        bool changed = false;
        for (uint16_t u = 0; u < n; u++) {
            if (sp.dist[u] >= GT_INF_WEIGHT) continue;
            for (uint16_t i = 0; i < g->adj_count[u]; i++) {
                uint16_t v = g->adj_list[u][i].to;
                double w = g->adj_list[u][i].weight;
                double alt = sp.dist[u] + w;
                if (alt < sp.dist[v]) {
                    sp.dist[v] = alt;
                    sp.prev[v] = u;
                    sp.reachable[v] = true;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    /* Check for negative cycles */
    sp.has_negative_cycle = false;
    for (uint16_t u = 0; u < n; u++) {
        if (sp.dist[u] >= GT_INF_WEIGHT) continue;
        for (uint16_t i = 0; i < g->adj_count[u]; i++) {
            uint16_t v = g->adj_list[u][i].to;
            double w = g->adj_list[u][i].weight;
            if (sp.dist[u] + w < sp.dist[v]) {
                sp.has_negative_cycle = true;
                return sp;
            }
        }
    }
    return sp;
}

/* =========================================================================
 * MST: Kruskal with Union-Find
 * ========================================================================= */

static uint16_t uf_parent[GT_MAX_VERTICES];
static uint16_t uf_rank_arr[GT_MAX_VERTICES];

static void uf_init(uint16_t n) {
    for (uint16_t i = 0; i < n; i++) { uf_parent[i] = i; uf_rank_arr[i] = 0; }
}

static uint16_t uf_find(uint16_t x) {
    while (uf_parent[x] != x) {
        uf_parent[x] = uf_parent[uf_parent[x]]; /* path compression */
        x = uf_parent[x];
    }
    return x;
}

static bool uf_union(uint16_t a, uint16_t b) {
    uint16_t ra = uf_find(a), rb = uf_find(b);
    if (ra == rb) return false;
    if (uf_rank_arr[ra] < uf_rank_arr[rb]) { uint16_t t = ra; ra = rb; rb = t; }
    uf_parent[rb] = ra;
    if (uf_rank_arr[ra] == uf_rank_arr[rb]) uf_rank_arr[ra]++;
    return true;
}

/* Simple insertion sort for edges */
static void edge_sort(gt_edge_t *edges, uint32_t n) {
    for (uint32_t i = 1; i < n; i++) {
        gt_edge_t key = edges[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && edges[j].weight > key.weight) {
            edges[j + 1] = edges[j];
            j--;
        }
        edges[j + 1] = key;
    }
}

gt_mst_result_t gt_kruskal(const graph_t *g) {
    gt_mst_result_t mst;
    memset(&mst, 0, sizeof(mst));
    if (!g || g->directed) return mst;

    /* Collect edges */
    gt_edge_t edges[GT_MAX_EDGES];
    uint32_t edge_count = 0;
    for (uint16_t u = 0; u < g->num_vertices; u++) {
        for (uint16_t i = 0; i < g->adj_count[u]; i++) {
            uint16_t v = g->adj_list[u][i].to;
            if (u < v && edge_count < GT_MAX_EDGES) {
                edges[edge_count].u = u;
                edges[edge_count].v = v;
                edges[edge_count].weight = g->adj_list[u][i].weight;
                edge_count++;
            }
        }
    }

    edge_sort(edges, edge_count);
    uf_init(g->num_vertices);

    for (uint32_t i = 0; i < edge_count; i++) {
        if (uf_union(edges[i].u, edges[i].v)) {
            mst.edges[mst.num_edges] = edges[i];
            mst.total_weight += edges[i].weight;
            mst.num_edges++;
            if (mst.num_edges == (uint32_t)(g->num_vertices - 1)) break;
        }
    }
    return mst;
}

gt_mst_result_t gt_prim(const graph_t *g, uint16_t start) {
    gt_mst_result_t mst;
    memset(&mst, 0, sizeof(mst));
    if (!g || g->directed || start >= g->num_vertices) return mst;

    bool in_mst[GT_MAX_VERTICES] = {false};
    double key[GT_MAX_VERTICES];
    uint16_t parent[GT_MAX_VERTICES];
    for (int i = 0; i < GT_MAX_VERTICES; i++) {
        key[i] = GT_INF_WEIGHT;
        parent[i] = UINT16_MAX;
    }
    key[start] = 0.0;

    for (uint16_t iter = 0; iter < g->num_vertices; iter++) {
        double min_k = GT_INF_WEIGHT;
        uint16_t u = UINT16_MAX;
        for (uint16_t v = 0; v < g->num_vertices; v++) {
            if (!in_mst[v] && key[v] < min_k) { min_k = key[v]; u = v; }
        }
        if (u == UINT16_MAX) break;
        in_mst[u] = true;

        if (parent[u] != UINT16_MAX) {
            mst.edges[mst.num_edges].u = parent[u];
            mst.edges[mst.num_edges].v = u;
            mst.edges[mst.num_edges].weight = min_k;
            mst.total_weight += min_k;
            mst.num_edges++;
        }

        for (uint16_t i = 0; i < g->adj_count[u]; i++) {
            uint16_t v = g->adj_list[u][i].to;
            double w = g->adj_list[u][i].weight;
            if (!in_mst[v] && w < key[v]) {
                key[v] = w;
                parent[v] = u;
            }
        }
    }
    return mst;
}

/* =========================================================================
 * Coloring
 * ========================================================================= */

gt_coloring_t gt_greedy_coloring(const graph_t *g) {
    gt_coloring_t c;
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < GT_MAX_VERTICES; i++) c.color[i] = -1;
    if (!g) return c;

    bool used[GT_MAX_COLORS];
    for (uint16_t v = 0; v < g->num_vertices; v++) {
        memset(used, 0, sizeof(used));
        for (uint16_t i = 0; i < g->adj_count[v]; i++) {
            int32_t nc = c.color[g->adj_list[v][i].to];
            if (nc >= 0 && nc < GT_MAX_COLORS) used[nc] = true;
        }
        for (int32_t col = 0; col < GT_MAX_COLORS; col++) {
            if (!used[col]) { c.color[v] = col; break; }
        }
        if (c.color[v] + 1 > (int32_t)c.num_colors) {
            c.num_colors = (uint32_t)(c.color[v] + 1);
        }
    }
    c.optimal = false;
    return c;
}

static bool chromatic_bt(const graph_t *g, int32_t *color, uint16_t v,
                         uint32_t k) {
    if (v == g->num_vertices) return true;

    for (uint32_t c = 0; c < k; c++) {
        bool ok = true;
        for (uint16_t i = 0; i < g->adj_count[v] && ok; i++) {
            uint16_t w = g->adj_list[v][i].to;
            if (w < v && color[w] == (int32_t)c) ok = false;
        }
        if (ok) {
            color[v] = (int32_t)c;
            if (chromatic_bt(g, color, v + 1, k)) return true;
            color[v] = -1;
        }
    }
    return false;
}

gt_coloring_t gt_chromatic_number(const graph_t *g) {
    gt_coloring_t c;
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < GT_MAX_VERTICES; i++) c.color[i] = -1;
    if (!g) return c;

    /* Binary search on k, or sequential for small graphs */
    int32_t color_buf[GT_MAX_VERTICES];
    for (uint32_t k = 1; k <= g->num_vertices && k <= GT_MAX_COLORS; k++) {
        for (int i = 0; i < GT_MAX_VERTICES; i++) color_buf[i] = -1;
        if (chromatic_bt(g, color_buf, 0, k)) {
            memcpy(c.color, color_buf, sizeof(color_buf));
            c.num_colors = k;
            c.optimal = true;
            return c;
        }
    }
    return c;
}

/* =========================================================================
 * Max flow: Edmonds-Karp (BFS-based Ford-Fulkerson)
 * ========================================================================= */

gt_flow_result_t gt_max_flow(const graph_t *g, uint16_t source, uint16_t sink) {
    gt_flow_result_t result;
    memset(&result, 0, sizeof(result));
    if (!g || source >= g->num_vertices || sink >= g->num_vertices) return result;
    if (source == sink) return result;

    /* Residual capacity */
    double cap[GT_MAX_VERTICES][GT_MAX_VERTICES];
    memset(cap, 0, sizeof(cap));
    for (uint16_t u = 0; u < g->num_vertices; u++) {
        for (uint16_t i = 0; i < g->adj_count[u]; i++) {
            uint16_t v = g->adj_list[u][i].to;
            cap[u][v] += g->adj_list[u][i].weight;
        }
    }

    uint16_t parent[GT_MAX_VERTICES];
    uint16_t queue[GT_MAX_VERTICES];

    while (true) {
        /* BFS to find augmenting path */
        memset(parent, 0xFF, sizeof(parent));
        bool visited[GT_MAX_VERTICES] = {false};
        uint32_t head = 0, tail = 0;
        visited[source] = true;
        queue[tail++] = source;

        while (head < tail && !visited[sink]) {
            uint16_t u = queue[head++];
            for (uint16_t v = 0; v < g->num_vertices; v++) {
                if (!visited[v] && cap[u][v] > 1e-12) {
                    visited[v] = true;
                    parent[v] = u;
                    queue[tail++] = v;
                }
            }
        }

        if (!visited[sink]) break;

        /* Find bottleneck */
        double path_flow = GT_INF_WEIGHT;
        for (uint16_t v = sink; v != source; v = parent[v]) {
            uint16_t u = parent[v];
            if (cap[u][v] < path_flow) path_flow = cap[u][v];
        }

        /* Update residual */
        for (uint16_t v = sink; v != source; v = parent[v]) {
            uint16_t u = parent[v];
            cap[u][v] -= path_flow;
            cap[v][u] += path_flow;
            result.flow[u][v] += path_flow;
            result.flow[v][u] -= path_flow;
        }
        result.max_flow += path_flow;
    }

    return result;
}

/* =========================================================================
 * Connectivity: components, bridges, articulation points
 * ========================================================================= */

gt_components_t gt_connected_components(const graph_t *g) {
    gt_components_t c;
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < GT_MAX_VERTICES; i++) c.component[i] = -1;
    if (!g) return c;

    bool visited[GT_MAX_VERTICES] = {false};
    uint16_t queue[GT_MAX_VERTICES];

    for (uint16_t start = 0; start < g->num_vertices; start++) {
        if (visited[start]) continue;
        uint32_t head = 0, tail = 0;
        visited[start] = true;
        queue[tail++] = start;
        c.component[start] = (int32_t)c.num_components;

        while (head < tail) {
            uint16_t v = queue[head++];
            for (uint16_t i = 0; i < g->adj_count[v]; i++) {
                uint16_t w = g->adj_list[v][i].to;
                if (!visited[w]) {
                    visited[w] = true;
                    c.component[w] = (int32_t)c.num_components;
                    queue[tail++] = w;
                }
            }
        }
        c.num_components++;
    }
    return c;
}

/* Bridge / articulation point detection via DFS discovery times */
static int bridge_timer;

static void bridge_dfs(const graph_t *g, uint16_t u, uint16_t parent,
                       int *disc, int *low, bool *visited,
                       gt_bridges_t *bridges) {
    visited[u] = true;
    disc[u] = low[u] = bridge_timer++;

    for (uint16_t i = 0; i < g->adj_count[u]; i++) {
        uint16_t v = g->adj_list[u][i].to;
        if (!visited[v]) {
            bridge_dfs(g, v, u, disc, low, visited, bridges);
            if (low[v] < low[u]) low[u] = low[v];
            if (low[v] > disc[u]) {
                /* u-v is a bridge */
                if (bridges->bridge_count < GT_MAX_EDGES) {
                    bridges->bridges[bridges->bridge_count].u = u;
                    bridges->bridges[bridges->bridge_count].v = v;
                    bridges->bridges[bridges->bridge_count].weight = g->adj_matrix[u][v];
                    bridges->bridge_count++;
                }
            }
        } else if (v != parent) {
            if (disc[v] < low[u]) low[u] = disc[v];
        }
    }
}

gt_bridges_t gt_find_bridges(const graph_t *g) {
    gt_bridges_t b;
    memset(&b, 0, sizeof(b));
    if (!g || g->directed) return b;

    int disc[GT_MAX_VERTICES], low[GT_MAX_VERTICES];
    bool visited[GT_MAX_VERTICES] = {false};
    memset(disc, -1, sizeof(disc));
    memset(low, -1, sizeof(low));
    bridge_timer = 0;

    for (uint16_t v = 0; v < g->num_vertices; v++) {
        if (!visited[v]) {
            bridge_dfs(g, v, UINT16_MAX, disc, low, visited, &b);
        }
    }
    return b;
}

static void ap_dfs(const graph_t *g, uint16_t u, uint16_t parent,
                   int *disc, int *low, bool *visited,
                   gt_articulation_t *ap) {
    visited[u] = true;
    disc[u] = low[u] = bridge_timer++;
    uint32_t children = 0;

    for (uint16_t i = 0; i < g->adj_count[u]; i++) {
        uint16_t v = g->adj_list[u][i].to;
        if (!visited[v]) {
            children++;
            ap_dfs(g, v, u, disc, low, visited, ap);
            if (low[v] < low[u]) low[u] = low[v];

            /* Articulation point conditions */
            if (parent == UINT16_MAX && children > 1) {
                if (!ap->is_articulation[u]) {
                    ap->is_articulation[u] = true;
                    ap->points[ap->count++] = u;
                }
            }
            if (parent != UINT16_MAX && low[v] >= disc[u]) {
                if (!ap->is_articulation[u]) {
                    ap->is_articulation[u] = true;
                    ap->points[ap->count++] = u;
                }
            }
        } else if (v != parent) {
            if (disc[v] < low[u]) low[u] = disc[v];
        }
    }
}

gt_articulation_t gt_find_articulation_points(const graph_t *g) {
    gt_articulation_t ap;
    memset(&ap, 0, sizeof(ap));
    if (!g || g->directed) return ap;

    int disc[GT_MAX_VERTICES], low[GT_MAX_VERTICES];
    bool visited[GT_MAX_VERTICES] = {false};
    memset(disc, -1, sizeof(disc));
    memset(low, -1, sizeof(low));
    bridge_timer = 0;

    for (uint16_t v = 0; v < g->num_vertices; v++) {
        if (!visited[v]) ap_dfs(g, v, UINT16_MAX, disc, low, visited, &ap);
    }
    return ap;
}

bool gt_is_bipartite(const graph_t *g) {
    if (!g) return false;
    int32_t color[GT_MAX_VERTICES];
    memset(color, -1, sizeof(color));
    uint16_t queue[GT_MAX_VERTICES];

    for (uint16_t start = 0; start < g->num_vertices; start++) {
        if (color[start] >= 0) continue;
        color[start] = 0;
        uint32_t head = 0, tail = 0;
        queue[tail++] = start;

        while (head < tail) {
            uint16_t v = queue[head++];
            for (uint16_t i = 0; i < g->adj_count[v]; i++) {
                uint16_t w = g->adj_list[v][i].to;
                if (color[w] < 0) {
                    color[w] = 1 - color[v];
                    queue[tail++] = w;
                } else if (color[w] == color[v]) {
                    return false;
                }
            }
        }
    }
    return true;
}

/* =========================================================================
 * Properties
 * ========================================================================= */

uint16_t gt_degree(const graph_t *g, uint16_t v) {
    if (!g || v >= g->num_vertices) return 0;
    return g->adj_count[v];
}

bool gt_check_handshaking(const graph_t *g) {
    if (!g || g->directed) return false;
    uint32_t sum = 0;
    for (uint16_t v = 0; v < g->num_vertices; v++) {
        sum += g->adj_count[v];
    }
    return (sum == 2 * g->num_edges);
}

bool gt_is_planar_euler(const graph_t *g) {
    if (!g) return false;
    /* Necessary condition: E <= 3V - 6 for simple planar graphs (V >= 3) */
    if (g->num_vertices < 3) return true;
    return (g->num_edges <= 3 * (uint32_t)g->num_vertices - 6);
}

graph_t *gt_complement(const graph_t *g) {
    if (!g) return NULL;
    graph_t *c = gt_create(g->num_vertices, g->directed, false);
    if (!c) return NULL;
    for (uint16_t u = 0; u < g->num_vertices; u++) {
        for (uint16_t v = (g->directed ? 0 : u + 1); v < g->num_vertices; v++) {
            if (u == v) continue;
            if (!gt_has_edge(g, u, v)) {
                gt_add_edge(c, u, v, 1.0);
            }
        }
    }
    return c;
}

bool gt_subgraph_isomorphism(const graph_t *g, const graph_t *pattern,
                             uint16_t *mapping) {
    if (!g || !pattern || !mapping) return false;
    if (pattern->num_vertices > g->num_vertices) return false;

    uint16_t pn = pattern->num_vertices;
    uint16_t gn = g->num_vertices;
    bool used[GT_MAX_VERTICES] = {false};

    /* Backtracking: try to map pattern vertex k to graph vertex mapping[k] */
    /* Stack-based to avoid deep recursion */
    int32_t assignment[GT_MAX_VERTICES];
    memset(assignment, -1, sizeof(assignment));

    uint16_t k = 0;
    assignment[0] = -1;

    while (true) {
        /* Try next candidate for position k */
        int32_t start = assignment[k] + 1;
        bool found = false;

        for (int32_t cand = start; cand < (int32_t)gn; cand++) {
            if (used[cand]) continue;

            /* Check edge constraints: for all i < k mapped, if pattern has
               edge (i,k) then g must have edge (mapping[i], cand) */
            bool ok = true;
            for (uint16_t i = 0; i < k && ok; i++) {
                bool pe = (pattern->adj_matrix[i][k] != 0.0) ||
                          (pattern->adj_matrix[k][i] != 0.0);
                bool ge = (g->adj_matrix[assignment[i]][cand] != 0.0) ||
                          (g->adj_matrix[cand][assignment[i]] != 0.0);
                if (pe && !ge) ok = false;
            }
            if (ok) {
                assignment[k] = cand;
                used[cand] = true;
                found = true;
                break;
            }
        }

        if (found) {
            if (k == pn - 1) {
                /* Complete mapping found */
                for (uint16_t i = 0; i < pn; i++) {
                    mapping[i] = (uint16_t)assignment[i];
                }
                return true;
            }
            k++;
            assignment[k] = -1;
        } else {
            /* Backtrack */
            if (k == 0) return false;
            if (assignment[k] >= 0) used[assignment[k]] = false;
            assignment[k] = -1;
            k--;
            used[assignment[k]] = false;
        }
    }
}
