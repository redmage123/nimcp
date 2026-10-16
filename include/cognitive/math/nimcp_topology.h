/**
 * @file nimcp_topology.h
 * @brief Computational topology engine: simplicial complexes, homology,
 *        Betti numbers, Euler characteristic, metric topology.
 *
 * Simplicial complexes up to dimension 3 (vertices, edges, triangles,
 * tetrahedra). Boundary operator. Betti numbers via rank-nullity on
 * boundary matrices. Euler characteristic. Metric space operations
 * (open/closed balls, interior, closure, boundary). Connected components.
 * Genus computation.
 */

#ifndef NIMCP_TOPOLOGY_H
#define NIMCP_TOPOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define TOPO_MAX_VERTICES      256
#define TOPO_MAX_EDGES         2048
#define TOPO_MAX_TRIANGLES     4096
#define TOPO_MAX_TETRAHEDRA    1024
#define TOPO_MAX_DIMENSION     3       /* up to tetrahedra */
#define TOPO_MAX_POINTS        256     /* for metric space point sets */

/* --------------------------------------------------------------------------
 * Simplex types
 * -------------------------------------------------------------------------- */

typedef struct {
    uint16_t v[2];                     /* sorted vertex indices */
} topo_edge_t;

typedef struct {
    uint16_t v[3];                     /* sorted vertex indices */
} topo_triangle_t;

typedef struct {
    uint16_t v[4];                     /* sorted vertex indices */
} topo_tetrahedron_t;

/* --------------------------------------------------------------------------
 * Simplicial complex
 * -------------------------------------------------------------------------- */

typedef struct {
    uint16_t         num_vertices;
    topo_edge_t     *edges;
    uint32_t         num_edges;
    topo_triangle_t *triangles;
    uint32_t         num_triangles;
    topo_tetrahedron_t *tetrahedra;
    uint32_t         num_tetrahedra;
} simplicial_complex_t;

/* --------------------------------------------------------------------------
 * Boundary matrix (sparse COO format)
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t *row;                      /* row indices */
    int32_t *col;                      /* col indices */
    int8_t  *val;                      /* +1 or -1   */
    uint32_t nnz;                      /* number of nonzeros */
    uint32_t rows;
    uint32_t cols;
} boundary_matrix_t;

/* --------------------------------------------------------------------------
 * Homology results
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t betti[TOPO_MAX_DIMENSION + 1]; /* beta_0, beta_1, beta_2, beta_3 */
    int32_t euler_characteristic;           /* V - E + F - T ...              */
    int32_t genus;                          /* (2 - chi) / 2 for closed surf  */
} homology_result_t;

/* --------------------------------------------------------------------------
 * Metric space point set
 * -------------------------------------------------------------------------- */

typedef enum {
    TOPO_METRIC_EUCLIDEAN,
    TOPO_METRIC_MANHATTAN,
    TOPO_METRIC_CHEBYSHEV,
    TOPO_METRIC_DISCRETE
} topo_metric_type_t;

typedef struct {
    double  coords[TOPO_MAX_POINTS][3]; /* points in R^3 */
    uint32_t num_points;
    uint32_t dim;                       /* spatial dimension (1,2,3) */
    topo_metric_type_t metric;
} topo_point_set_t;

/* --------------------------------------------------------------------------
 * Connected component result
 * -------------------------------------------------------------------------- */

typedef struct {
    int32_t  component_id[TOPO_MAX_VERTICES]; /* which component per vertex */
    uint32_t num_components;
} connected_components_t;

/* --------------------------------------------------------------------------
 * Topology engine
 * -------------------------------------------------------------------------- */

typedef struct {
    simplicial_complex_t *complex;     /* currently loaded complex */
} topology_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

topology_t *topo_create(void);
void        topo_destroy(topology_t *t);

/* --------------------------------------------------------------------------
 * Complex construction
 * -------------------------------------------------------------------------- */

simplicial_complex_t *topo_complex_create(uint16_t num_vertices);
void                  topo_complex_destroy(simplicial_complex_t *sc);

bool topo_complex_add_edge(simplicial_complex_t *sc, uint16_t v0, uint16_t v1);
bool topo_complex_add_triangle(simplicial_complex_t *sc,
                               uint16_t v0, uint16_t v1, uint16_t v2);
bool topo_complex_add_tetrahedron(simplicial_complex_t *sc,
                                  uint16_t v0, uint16_t v1,
                                  uint16_t v2, uint16_t v3);

/* --------------------------------------------------------------------------
 * Boundary operator
 * -------------------------------------------------------------------------- */

/** Build boundary matrix for dimension d (d=1: edges->vertices, etc.) */
boundary_matrix_t *topo_boundary_matrix(const simplicial_complex_t *sc,
                                        uint32_t d);
void               topo_boundary_matrix_destroy(boundary_matrix_t *bm);

/* --------------------------------------------------------------------------
 * Homology / Betti numbers
 * -------------------------------------------------------------------------- */

homology_result_t topo_compute_homology(const simplicial_complex_t *sc);
int32_t           topo_euler_characteristic(const simplicial_complex_t *sc);

/* --------------------------------------------------------------------------
 * Connected components
 * -------------------------------------------------------------------------- */

connected_components_t topo_connected_components(const simplicial_complex_t *sc);

/* --------------------------------------------------------------------------
 * Metric topology
 * -------------------------------------------------------------------------- */

/** Distance between two points in the point set. */
double topo_metric_distance(const topo_point_set_t *ps,
                            uint32_t i, uint32_t j);

/** Check if point idx is in open ball B(center, radius). */
bool topo_in_open_ball(const topo_point_set_t *ps, uint32_t center,
                       uint32_t idx, double radius);

/** Check if point idx is in closed ball. */
bool topo_in_closed_ball(const topo_point_set_t *ps, uint32_t center,
                         uint32_t idx, double radius);

/** Check if point idx is an interior point of subset (given as bool array). */
bool topo_is_interior_point(const topo_point_set_t *ps, const bool *subset,
                            uint32_t idx);

/** Check if point idx is a boundary point of subset. */
bool topo_is_boundary_point(const topo_point_set_t *ps, const bool *subset,
                            uint32_t idx);

/** Check if subset is bounded (max distance finite). */
bool topo_is_bounded(const topo_point_set_t *ps, const bool *subset);

/** Estimate diameter of a subset. */
double topo_diameter(const topo_point_set_t *ps, const bool *subset);

/** Check Lipschitz continuity between two point sets under mapping. */
double topo_lipschitz_constant(const topo_point_set_t *domain,
                               const topo_point_set_t *codomain,
                               const uint32_t *mapping);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOPOLOGY_H */
