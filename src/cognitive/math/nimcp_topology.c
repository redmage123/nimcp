/**
 * @file nimcp_topology.c
 * @brief Computational topology engine implementation
 */

#include "cognitive/math/nimcp_topology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "TOPOLOGY"

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

topology_t *topo_create(void) {
    topology_t *t = (topology_t *)nimcp_calloc(1, sizeof(topology_t));
    if (!t) {
        LOG_ERROR("Failed to allocate topology_t");
        return NULL;
    }
    t->complex = NULL;
    LOG_INFO("Topology engine created");
    return t;
}

void topo_destroy(topology_t *t) {
    if (!t) return;
    if (t->complex) topo_complex_destroy(t->complex);
    nimcp_free(t);
    LOG_INFO("Topology engine destroyed");
}

/* =========================================================================
 * Complex construction
 * ========================================================================= */

simplicial_complex_t *topo_complex_create(uint16_t num_vertices) {
    if (num_vertices > TOPO_MAX_VERTICES) {
        LOG_ERROR("Vertex count %u exceeds max %d", num_vertices, TOPO_MAX_VERTICES);
        return NULL;
    }
    simplicial_complex_t *sc = (simplicial_complex_t *)nimcp_calloc(1, sizeof(simplicial_complex_t));
    if (!sc) return NULL;

    sc->num_vertices = num_vertices;
    sc->edges = (topo_edge_t *)nimcp_calloc(TOPO_MAX_EDGES, sizeof(topo_edge_t));
    sc->triangles = (topo_triangle_t *)nimcp_calloc(TOPO_MAX_TRIANGLES, sizeof(topo_triangle_t));
    sc->tetrahedra = (topo_tetrahedron_t *)nimcp_calloc(TOPO_MAX_TETRAHEDRA, sizeof(topo_tetrahedron_t));

    if (!sc->edges || !sc->triangles || !sc->tetrahedra) {
        if (sc->edges) nimcp_free(sc->edges);
        if (sc->triangles) nimcp_free(sc->triangles);
        if (sc->tetrahedra) nimcp_free(sc->tetrahedra);
        nimcp_free(sc);
        return NULL;
    }
    return sc;
}

void topo_complex_destroy(simplicial_complex_t *sc) {
    if (!sc) return;
    if (sc->edges) nimcp_free(sc->edges);
    if (sc->triangles) nimcp_free(sc->triangles);
    if (sc->tetrahedra) nimcp_free(sc->tetrahedra);
    nimcp_free(sc);
}

/** Sort two vertices for canonical edge ordering. */
static void sort2(uint16_t *a, uint16_t *b) {
    if (*a > *b) { uint16_t t = *a; *a = *b; *b = t; }
}

/** Sort three vertices. */
static void sort3(uint16_t *a, uint16_t *b, uint16_t *c) {
    sort2(a, b); sort2(b, c); sort2(a, b);
}

/** Sort four vertices. */
static void sort4(uint16_t *a, uint16_t *b, uint16_t *c, uint16_t *d) {
    sort2(a, b); sort2(c, d); sort2(a, c); sort2(b, d); sort2(b, c);
}

static bool has_edge(const simplicial_complex_t *sc, uint16_t v0, uint16_t v1) {
    for (uint32_t i = 0; i < sc->num_edges; i++) {
        if (sc->edges[i].v[0] == v0 && sc->edges[i].v[1] == v1) return true;
    }
    return false;
}

bool topo_complex_add_edge(simplicial_complex_t *sc, uint16_t v0, uint16_t v1) {
    if (!sc || v0 >= sc->num_vertices || v1 >= sc->num_vertices || v0 == v1) return false;
    sort2(&v0, &v1);
    if (has_edge(sc, v0, v1)) return true; /* already present */
    if (sc->num_edges >= TOPO_MAX_EDGES) return false;
    sc->edges[sc->num_edges].v[0] = v0;
    sc->edges[sc->num_edges].v[1] = v1;
    sc->num_edges++;
    return true;
}

bool topo_complex_add_triangle(simplicial_complex_t *sc,
                               uint16_t v0, uint16_t v1, uint16_t v2) {
    if (!sc) return false;
    sort3(&v0, &v1, &v2);

    /* Add sub-edges if missing */
    topo_complex_add_edge(sc, v0, v1);
    topo_complex_add_edge(sc, v0, v2);
    topo_complex_add_edge(sc, v1, v2);

    /* Check for duplicate */
    for (uint32_t i = 0; i < sc->num_triangles; i++) {
        if (sc->triangles[i].v[0] == v0 && sc->triangles[i].v[1] == v1 &&
            sc->triangles[i].v[2] == v2) return true;
    }
    if (sc->num_triangles >= TOPO_MAX_TRIANGLES) return false;
    sc->triangles[sc->num_triangles].v[0] = v0;
    sc->triangles[sc->num_triangles].v[1] = v1;
    sc->triangles[sc->num_triangles].v[2] = v2;
    sc->num_triangles++;
    return true;
}

bool topo_complex_add_tetrahedron(simplicial_complex_t *sc,
                                  uint16_t v0, uint16_t v1,
                                  uint16_t v2, uint16_t v3) {
    if (!sc) return false;
    sort4(&v0, &v1, &v2, &v3);

    /* Add sub-faces */
    topo_complex_add_triangle(sc, v0, v1, v2);
    topo_complex_add_triangle(sc, v0, v1, v3);
    topo_complex_add_triangle(sc, v0, v2, v3);
    topo_complex_add_triangle(sc, v1, v2, v3);

    if (sc->num_tetrahedra >= TOPO_MAX_TETRAHEDRA) return false;
    sc->tetrahedra[sc->num_tetrahedra].v[0] = v0;
    sc->tetrahedra[sc->num_tetrahedra].v[1] = v1;
    sc->tetrahedra[sc->num_tetrahedra].v[2] = v2;
    sc->tetrahedra[sc->num_tetrahedra].v[3] = v3;
    sc->num_tetrahedra++;
    return true;
}

/* =========================================================================
 * Boundary matrix
 * ========================================================================= */

static int edge_index(const simplicial_complex_t *sc, uint16_t v0, uint16_t v1) {
    for (uint32_t i = 0; i < sc->num_edges; i++) {
        if (sc->edges[i].v[0] == v0 && sc->edges[i].v[1] == v1) return (int)i;
    }
    return -1;
}

static int tri_index(const simplicial_complex_t *sc, uint16_t v0, uint16_t v1, uint16_t v2) {
    for (uint32_t i = 0; i < sc->num_triangles; i++) {
        if (sc->triangles[i].v[0] == v0 && sc->triangles[i].v[1] == v1 &&
            sc->triangles[i].v[2] == v2) return (int)i;
    }
    return -1;
}

boundary_matrix_t *topo_boundary_matrix(const simplicial_complex_t *sc, uint32_t d) {
    if (!sc) return NULL;

    boundary_matrix_t *bm = (boundary_matrix_t *)nimcp_calloc(1, sizeof(boundary_matrix_t));
    if (!bm) return NULL;

    if (d == 1) {
        /* d_1: edges -> vertices. Each edge [v0,v1] gives +1 at v1, -1 at v0 */
        bm->rows = sc->num_vertices;
        bm->cols = sc->num_edges;
        uint32_t max_nnz = 2 * sc->num_edges;
        bm->row = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->col = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->val = (int8_t *)nimcp_calloc(max_nnz, sizeof(int8_t));
        if (!bm->row || !bm->col || !bm->val) {
            topo_boundary_matrix_destroy(bm);
            return NULL;
        }
        bm->nnz = 0;
        for (uint32_t j = 0; j < sc->num_edges; j++) {
            bm->row[bm->nnz] = sc->edges[j].v[0]; bm->col[bm->nnz] = (int32_t)j;
            bm->val[bm->nnz] = -1; bm->nnz++;
            bm->row[bm->nnz] = sc->edges[j].v[1]; bm->col[bm->nnz] = (int32_t)j;
            bm->val[bm->nnz] = +1; bm->nnz++;
        }
    } else if (d == 2) {
        /* d_2: triangles -> edges */
        bm->rows = sc->num_edges;
        bm->cols = sc->num_triangles;
        uint32_t max_nnz = 3 * sc->num_triangles;
        bm->row = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->col = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->val = (int8_t *)nimcp_calloc(max_nnz, sizeof(int8_t));
        if (!bm->row || !bm->col || !bm->val) {
            topo_boundary_matrix_destroy(bm);
            return NULL;
        }
        bm->nnz = 0;
        for (uint32_t j = 0; j < sc->num_triangles; j++) {
            uint16_t v0 = sc->triangles[j].v[0];
            uint16_t v1 = sc->triangles[j].v[1];
            uint16_t v2 = sc->triangles[j].v[2];
            /* Boundary = [v1,v2] - [v0,v2] + [v0,v1] */
            int ei;
            ei = edge_index(sc, v1, v2);
            if (ei >= 0) { bm->row[bm->nnz] = ei; bm->col[bm->nnz] = (int32_t)j; bm->val[bm->nnz] = +1; bm->nnz++; }
            ei = edge_index(sc, v0, v2);
            if (ei >= 0) { bm->row[bm->nnz] = ei; bm->col[bm->nnz] = (int32_t)j; bm->val[bm->nnz] = -1; bm->nnz++; }
            ei = edge_index(sc, v0, v1);
            if (ei >= 0) { bm->row[bm->nnz] = ei; bm->col[bm->nnz] = (int32_t)j; bm->val[bm->nnz] = +1; bm->nnz++; }
        }
    } else if (d == 3) {
        /* d_3: tetrahedra -> triangles */
        bm->rows = sc->num_triangles;
        bm->cols = sc->num_tetrahedra;
        uint32_t max_nnz = 4 * sc->num_tetrahedra;
        bm->row = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->col = (int32_t *)nimcp_calloc(max_nnz, sizeof(int32_t));
        bm->val = (int8_t *)nimcp_calloc(max_nnz, sizeof(int8_t));
        if (!bm->row || !bm->col || !bm->val) {
            topo_boundary_matrix_destroy(bm);
            return NULL;
        }
        bm->nnz = 0;
        for (uint32_t j = 0; j < sc->num_tetrahedra; j++) {
            uint16_t v[4];
            for (int k = 0; k < 4; k++) v[k] = sc->tetrahedra[j].v[k];
            /* Face i = omit vertex i, sign = (-1)^i */
            uint16_t faces[4][3] = {
                {v[1], v[2], v[3]}, {v[0], v[2], v[3]},
                {v[0], v[1], v[3]}, {v[0], v[1], v[2]}
            };
            int8_t signs[4] = {+1, -1, +1, -1};
            for (int k = 0; k < 4; k++) {
                int ti = tri_index(sc, faces[k][0], faces[k][1], faces[k][2]);
                if (ti >= 0) {
                    bm->row[bm->nnz] = ti; bm->col[bm->nnz] = (int32_t)j;
                    bm->val[bm->nnz] = signs[k]; bm->nnz++;
                }
            }
        }
    } else {
        nimcp_free(bm);
        return NULL;
    }

    return bm;
}

void topo_boundary_matrix_destroy(boundary_matrix_t *bm) {
    if (!bm) return;
    if (bm->row) nimcp_free(bm->row);
    if (bm->col) nimcp_free(bm->col);
    if (bm->val) nimcp_free(bm->val);
    nimcp_free(bm);
}

/* =========================================================================
 * Homology via rank computation (Gaussian elimination over Z/2Z)
 * ========================================================================= */

/** Compute rank of a boundary matrix over Z/2Z (mod 2). */
static uint32_t matrix_rank_mod2(const boundary_matrix_t *bm) {
    if (!bm || bm->rows == 0 || bm->cols == 0) return 0;

    /* Dense matrix over Z/2 */
    uint32_t rows = bm->rows;
    uint32_t cols = bm->cols;
    uint8_t *mat = (uint8_t *)nimcp_calloc(rows * cols, sizeof(uint8_t));
    if (!mat) return 0;

    for (uint32_t k = 0; k < bm->nnz; k++) {
        int32_t r = bm->row[k];
        int32_t c = bm->col[k];
        if (r >= 0 && r < (int32_t)rows && c >= 0 && c < (int32_t)cols) {
            mat[r * cols + c] = (mat[r * cols + c] + 1) % 2;
        }
    }

    /* Gaussian elimination mod 2 */
    uint32_t rank = 0;
    for (uint32_t col = 0; col < cols && rank < rows; col++) {
        /* Find pivot */
        uint32_t pivot = rows;
        for (uint32_t r = rank; r < rows; r++) {
            if (mat[r * cols + col]) { pivot = r; break; }
        }
        if (pivot == rows) continue;

        /* Swap rows */
        if (pivot != rank) {
            for (uint32_t c = 0; c < cols; c++) {
                uint8_t tmp = mat[rank * cols + c];
                mat[rank * cols + c] = mat[pivot * cols + c];
                mat[pivot * cols + c] = tmp;
            }
        }

        /* Eliminate */
        for (uint32_t r = 0; r < rows; r++) {
            if (r != rank && mat[r * cols + col]) {
                for (uint32_t c = 0; c < cols; c++) {
                    mat[r * cols + c] ^= mat[rank * cols + c];
                }
            }
        }
        rank++;
    }

    nimcp_free(mat);
    return rank;
}

homology_result_t topo_compute_homology(const simplicial_complex_t *sc) {
    homology_result_t h;
    memset(&h, 0, sizeof(h));
    if (!sc) return h;

    /* Count simplices per dimension */
    uint32_t c[4];
    c[0] = sc->num_vertices;
    c[1] = sc->num_edges;
    c[2] = sc->num_triangles;
    c[3] = sc->num_tetrahedra;

    /* Build boundary matrices and compute ranks */
    uint32_t rank_d[4] = {0, 0, 0, 0}; /* rank of boundary_d */

    for (uint32_t d = 1; d <= 3; d++) {
        if (c[d] == 0) continue;
        boundary_matrix_t *bm = topo_boundary_matrix(sc, d);
        if (bm) {
            rank_d[d] = matrix_rank_mod2(bm);
            topo_boundary_matrix_destroy(bm);
        }
    }

    /* Betti_k = dim(ker d_k) - dim(im d_{k+1})
     *         = (c_k - rank d_k) - rank d_{k+1}
     * where rank d_0 = 0 by convention */
    for (uint32_t k = 0; k <= 3; k++) {
        uint32_t ker_dim = c[k] - rank_d[k];
        uint32_t im_dim = (k + 1 <= 3) ? rank_d[k + 1] : 0;
        h.betti[k] = (int32_t)ker_dim - (int32_t)im_dim;
        if (h.betti[k] < 0) h.betti[k] = 0;
    }

    /* Euler characteristic = sum (-1)^k * c_k */
    h.euler_characteristic = (int32_t)c[0] - (int32_t)c[1] +
                             (int32_t)c[2] - (int32_t)c[3];

    /* Genus for closed orientable surface: g = (2 - chi) / 2 */
    h.genus = (2 - h.euler_characteristic) / 2;

    return h;
}

int32_t topo_euler_characteristic(const simplicial_complex_t *sc) {
    if (!sc) return 0;
    return (int32_t)sc->num_vertices - (int32_t)sc->num_edges +
           (int32_t)sc->num_triangles - (int32_t)sc->num_tetrahedra;
}

/* =========================================================================
 * Connected components (BFS on 1-skeleton)
 * ========================================================================= */

connected_components_t topo_connected_components(const simplicial_complex_t *sc) {
    connected_components_t result;
    memset(&result, 0, sizeof(result));
    if (!sc) return result;

    for (uint32_t i = 0; i < TOPO_MAX_VERTICES; i++) {
        result.component_id[i] = -1;
    }

    uint16_t queue[TOPO_MAX_VERTICES];
    int32_t comp_id = 0;

    for (uint16_t start = 0; start < sc->num_vertices; start++) {
        if (result.component_id[start] >= 0) continue;

        /* BFS from start */
        uint32_t head = 0, tail = 0;
        queue[tail++] = start;
        result.component_id[start] = comp_id;

        while (head < tail) {
            uint16_t v = queue[head++];
            /* Find neighbors via edges */
            for (uint32_t e = 0; e < sc->num_edges; e++) {
                uint16_t other = UINT16_MAX;
                if (sc->edges[e].v[0] == v) other = sc->edges[e].v[1];
                else if (sc->edges[e].v[1] == v) other = sc->edges[e].v[0];
                if (other != UINT16_MAX && result.component_id[other] < 0) {
                    result.component_id[other] = comp_id;
                    queue[tail++] = other;
                }
            }
        }
        comp_id++;
    }
    result.num_components = (uint32_t)comp_id;
    return result;
}

/* =========================================================================
 * Metric topology
 * ========================================================================= */

static double point_distance(const topo_point_set_t *ps, uint32_t i, uint32_t j) {
    if (!ps || i >= ps->num_points || j >= ps->num_points) return 0.0;
    double sum = 0.0;
    switch (ps->metric) {
        case TOPO_METRIC_EUCLIDEAN:
            for (uint32_t d = 0; d < ps->dim; d++) {
                double diff = ps->coords[i][d] - ps->coords[j][d];
                sum += diff * diff;
            }
            return sqrt(sum);
        case TOPO_METRIC_MANHATTAN:
            for (uint32_t d = 0; d < ps->dim; d++)
                sum += fabs(ps->coords[i][d] - ps->coords[j][d]);
            return sum;
        case TOPO_METRIC_CHEBYSHEV: {
            double mx = 0.0;
            for (uint32_t d = 0; d < ps->dim; d++) {
                double diff = fabs(ps->coords[i][d] - ps->coords[j][d]);
                if (diff > mx) mx = diff;
            }
            return mx;
        }
        case TOPO_METRIC_DISCRETE:
            for (uint32_t d = 0; d < ps->dim; d++)
                if (fabs(ps->coords[i][d] - ps->coords[j][d]) > 1e-15) return 1.0;
            return 0.0;
        default:
            return 0.0;
    }
}

double topo_metric_distance(const topo_point_set_t *ps, uint32_t i, uint32_t j) {
    return point_distance(ps, i, j);
}

bool topo_in_open_ball(const topo_point_set_t *ps, uint32_t center,
                       uint32_t idx, double radius) {
    return point_distance(ps, center, idx) < radius;
}

bool topo_in_closed_ball(const topo_point_set_t *ps, uint32_t center,
                         uint32_t idx, double radius) {
    return point_distance(ps, center, idx) <= radius + 1e-15;
}

bool topo_is_interior_point(const topo_point_set_t *ps, const bool *subset,
                            uint32_t idx) {
    if (!ps || !subset || idx >= ps->num_points || !subset[idx]) return false;
    /* Find min distance to a non-subset point */
    double min_dist = 1e30;
    for (uint32_t j = 0; j < ps->num_points; j++) {
        if (j == idx || subset[j]) continue;
        double d = point_distance(ps, idx, j);
        if (d < min_dist) min_dist = d;
    }
    /* Interior if there exists an open ball around idx contained in subset */
    return (min_dist > 1e-12);
}

bool topo_is_boundary_point(const topo_point_set_t *ps, const bool *subset,
                            uint32_t idx) {
    if (!ps || !subset || idx >= ps->num_points) return false;
    /* Boundary point: every open ball around idx contains points in subset
       AND points not in subset. Approximate by checking nearby. */
    bool has_in = false, has_out = false;
    for (uint32_t j = 0; j < ps->num_points; j++) {
        if (j == idx) continue;
        if (subset[j]) has_in = true;
        else has_out = true;
        if (has_in && has_out) return true;
    }
    return false;
}

bool topo_is_bounded(const topo_point_set_t *ps, const bool *subset) {
    if (!ps || !subset) return true;
    double max_dist = 0.0;
    for (uint32_t i = 0; i < ps->num_points; i++) {
        if (!subset[i]) continue;
        for (uint32_t j = i + 1; j < ps->num_points; j++) {
            if (!subset[j]) continue;
            double d = point_distance(ps, i, j);
            if (d > max_dist) max_dist = d;
        }
    }
    return isfinite(max_dist);
}

double topo_diameter(const topo_point_set_t *ps, const bool *subset) {
    if (!ps || !subset) return 0.0;
    double max_dist = 0.0;
    for (uint32_t i = 0; i < ps->num_points; i++) {
        if (!subset[i]) continue;
        for (uint32_t j = i + 1; j < ps->num_points; j++) {
            if (!subset[j]) continue;
            double d = point_distance(ps, i, j);
            if (d > max_dist) max_dist = d;
        }
    }
    return max_dist;
}

double topo_lipschitz_constant(const topo_point_set_t *domain,
                               const topo_point_set_t *codomain,
                               const uint32_t *mapping) {
    if (!domain || !codomain || !mapping) return -1.0;
    double max_L = 0.0;
    for (uint32_t i = 0; i < domain->num_points; i++) {
        for (uint32_t j = i + 1; j < domain->num_points; j++) {
            double dx = point_distance(domain, i, j);
            if (dx < 1e-15) continue;
            double dy = point_distance(codomain, mapping[i], mapping[j]);
            double L = dy / dx;
            if (L > max_L) max_L = L;
        }
    }
    return max_L;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for topology results
 * (homotopy equivalence, manifold properties, continuity proofs).
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
void topology_wave14_kg_emit(
    struct brain_struct* brain,
    const char* theorem_label,
    float confidence)
{
    if (!brain) return;
    wave14_math_emit_proof(brain, "topology", theorem_label, confidence);
}
