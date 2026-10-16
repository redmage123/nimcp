/**
 * @file nimcp_surface_manifold.h
 * @brief Manifold Mathematics for Surface Optimization
 *
 * WHAT: Mathematical foundations for surface minimization via manifolds
 * WHY:  Physical networks are 2D manifolds (surfaces) not 1D wires.
 *       The true material cost is surface area, computed via:
 *       S = integral( sqrt(det(gamma)) d^2 sigma )
 *       where gamma is the induced metric tensor.
 * HOW:  Maps to string theory worldsheets via Nambu-Goto action,
 *       uses Strebel's theorem for boundary conditions.
 *
 * MATHEMATICAL BACKGROUND:
 * - Manifold M(G): 2D surface assigned to graph G
 * - Chart X_i(sigma): Local coordinate patch for link i
 * - sigma = (sigma^0, sigma^1): Longitudinal and azimuthal coordinates
 * - Metric tensor: gamma_ab = (dX/d sigma^a) . (dX/d sigma^b)
 * - Surface area: S = sum_i integral sqrt(det(gamma_i)) d^2 sigma
 * - Nambu-Goto action: Identical to S (string theory formulation)
 * - Strebel's theorem: Minimal surface is cylindrical away from junctions
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_MANIFOLD_H
#define NIMCP_SURFACE_MANIFOLD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// CONSTANTS
//=============================================================================

/** Pi constant */

/** Default number of integration points for Gaussian quadrature */
#define SURFACE_GAUSS_POINTS 16

/** Minimum Gaussian quadrature points for low-curvature surfaces */
#define SURFACE_GAUSS_POINTS_MIN 8

/** Maximum Gaussian quadrature points for high-curvature surfaces */
#define SURFACE_GAUSS_POINTS_MAX 64

/** Curvature threshold for adaptive quadrature (increases points above this) */
#define SURFACE_CURVATURE_THRESHOLD_HIGH 0.5f

/** Default resolution for manifold discretization */
#define SURFACE_MANIFOLD_RESOLUTION 32

//=============================================================================
// COORDINATE SYSTEM STRUCTURES
//=============================================================================

/**
 * @brief 2D local coordinate on manifold surface
 */
typedef struct surface_sigma_struct {
    float sigma0;               /**< Longitudinal coordinate (along link) */
    float sigma1;               /**< Azimuthal coordinate (around link) */
} surface_sigma_t;

/**
 * @brief Jacobian of coordinate transformation
 */
typedef struct surface_jacobian_struct {
    float dX_dsigma0[3];        /**< dX/d(sigma^0) - tangent along link */
    float dX_dsigma1[3];        /**< dX/d(sigma^1) - tangent around link */
} surface_jacobian_t;

//=============================================================================
// PARAMETRIC SURFACE FUNCTIONS
//=============================================================================

/**
 * @brief Function pointer for position on surface
 *
 * @param sigma Local coordinates (sigma^0, sigma^1)
 * @param coord Which coordinate to return (0=x, 1=y, 2=z)
 * @param user_data User context (e.g., link parameters)
 * @return Position coordinate value
 */
typedef float (*surface_position_fn)(
    const surface_sigma_t* sigma,
    uint8_t coord,
    void* user_data
);

/**
 * @brief Cylindrical surface parametrization
 *
 * Standard cylinder along z-axis with radius r:
 * X(sigma) = (r*cos(sigma^1), r*sin(sigma^1), sigma^0)
 */
typedef struct surface_cylinder_params_struct {
    float radius;               /**< Cylinder radius (w / 2*pi) */
    float length;               /**< Cylinder length */
    surface_vec3_t axis;        /**< Cylinder axis direction */
    surface_vec3_t origin;      /**< Cylinder origin */
} surface_cylinder_params_t;

/**
 * @brief Conical surface parametrization (varying diameter)
 */
typedef struct surface_cone_params_struct {
    float radius_start;         /**< Radius at start */
    float radius_end;           /**< Radius at end */
    float length;               /**< Cone length */
    surface_vec3_t axis;        /**< Cone axis direction */
    surface_vec3_t origin;      /**< Cone origin */
} surface_cone_params_t;

/**
 * @brief Pants surface parametrization (bifurcation)
 *
 * Three-legged surface for k=3 branch point
 */
typedef struct surface_pants_params_struct {
    float radius_trunk;         /**< Trunk radius */
    float radius_leg1;          /**< Leg 1 radius */
    float radius_leg2;          /**< Leg 2 radius */
    float trunk_length;         /**< Trunk length before split */
    float leg_length;           /**< Leg length after split */
    float branch_angle;         /**< Angle between legs */
    surface_vec3_t origin;      /**< Origin position */
    surface_vec3_t trunk_dir;   /**< Trunk direction */
} surface_pants_params_t;

/**
 * @brief Triple pants surface parametrization (trifurcation)
 *
 * Four-legged surface for k=4 branch point
 */
typedef struct surface_triple_pants_params_struct {
    float radius_trunk;         /**< Trunk radius */
    float radius_legs[3];       /**< Leg radii */
    float trunk_length;
    float leg_lengths[3];
    float branch_angles[3];     /**< Angles between legs */
    surface_vec3_t origin;
    surface_vec3_t trunk_dir;
} surface_triple_pants_params_t;

//=============================================================================
// MANIFOLD CONTEXT
//=============================================================================

/**
 * @brief Complete manifold for a network graph
 */
typedef struct surface_manifold_struct {
    /* Charts (one per link) */
    surface_chart_t* charts;
    uint32_t num_charts;
    uint32_t capacity_charts;

    /* Branch points */
    surface_branch_point_t* branch_points;
    uint32_t num_branch_points;
    uint32_t capacity_branch_points;

    /* Feynman diagram representation */
    surface_feynman_diagram_t* feynman;

    /* Total metrics */
    float total_surface_area;   /**< S_M(G) */
    float total_wire_length;    /**< Steiner equivalent */
    float total_circumference;  /**< Sum of link circumferences */

    /* Constraints */
    float min_circumference;    /**< w constraint */

    /* Computation state */
    bool is_computed;
    bool is_optimized;
    uint32_t computation_iterations;
} surface_manifold_t;

//=============================================================================
// MANIFOLD CREATION AND DESTRUCTION
//=============================================================================

/**
 * @brief Create empty manifold
 *
 * @param max_charts Maximum number of charts
 * @param max_branch_points Maximum number of branch points
 * @return Allocated manifold or NULL on failure
 */
surface_manifold_t* surface_manifold_create(
    uint32_t max_charts,
    uint32_t max_branch_points
);

/**
 * @brief Destroy manifold and free memory
 *
 * @param manifold Manifold to destroy
 */
void surface_manifold_destroy(surface_manifold_t* manifold);

/**
 * @brief Reset manifold to empty state
 *
 * @param manifold Manifold to reset
 * @return 0 on success, -1 on error
 */
int surface_manifold_reset(surface_manifold_t* manifold);

//=============================================================================
// CHART OPERATIONS
//=============================================================================

/**
 * @brief Add chart to manifold
 *
 * @param manifold Target manifold
 * @param link_id Associated link ID
 * @param type Chart type (cylindrical, pants, etc.)
 * @param circumference Minimum circumference constraint
 * @param chart_id Output: ID of created chart
 * @return 0 on success, -1 on error
 */
int surface_manifold_add_chart(
    surface_manifold_t* manifold,
    uint32_t link_id,
    surface_chart_type_t type,
    float circumference,
    uint32_t* chart_id
);

/**
 * @brief Connect two charts at their boundaries
 *
 * @param manifold Target manifold
 * @param chart1_id First chart
 * @param chart2_id Second chart
 * @param boundary_index Which boundary (0-3)
 * @return 0 on success, -1 on error
 */
int surface_manifold_connect_charts(
    surface_manifold_t* manifold,
    uint32_t chart1_id,
    uint32_t chart2_id,
    uint32_t boundary_index
);

//=============================================================================
// METRIC TENSOR COMPUTATION
//=============================================================================

/**
 * @brief Compute metric tensor at a point
 *
 * gamma_ab = (dX/d sigma^a) . (dX/d sigma^b)
 *
 * @param position_fn Function returning position X(sigma)
 * @param sigma Local coordinates
 * @param user_data Context for position function
 * @param metric Output: computed metric tensor
 * @return 0 on success, -1 on error
 */
int surface_compute_metric_tensor(
    surface_position_fn position_fn,
    const surface_sigma_t* sigma,
    void* user_data,
    surface_metric_tensor_t* metric
);

/**
 * @brief Compute metric tensor for cylindrical surface
 *
 * For cylinder: gamma = [[1, 0], [0, r^2]]
 *
 * @param params Cylinder parameters
 * @param sigma Local coordinates
 * @param metric Output: metric tensor
 * @return 0 on success, -1 on error
 */
int surface_compute_metric_cylinder(
    const surface_cylinder_params_t* params,
    const surface_sigma_t* sigma,
    surface_metric_tensor_t* metric
);

/**
 * @brief Compute metric tensor for conical surface
 *
 * @param params Cone parameters
 * @param sigma Local coordinates
 * @param metric Output: metric tensor
 * @return 0 on success, -1 on error
 */
int surface_compute_metric_cone(
    const surface_cone_params_t* params,
    const surface_sigma_t* sigma,
    surface_metric_tensor_t* metric
);

/**
 * @brief Compute determinant of 2x2 metric tensor
 *
 * @param metric Input metric tensor
 * @return Determinant value
 */
float surface_metric_determinant(const surface_metric_tensor_t* metric);

/**
 * @brief Compute inverse of 2x2 metric tensor
 *
 * @param metric Input metric tensor
 * @param inverse Output: inverse metric
 * @return 0 on success, -1 if singular
 */
int surface_metric_inverse(
    const surface_metric_tensor_t* metric,
    float inverse[2][2]
);

//=============================================================================
// SURFACE AREA COMPUTATION
//=============================================================================

/**
 * @brief Compute surface area of a chart via numerical integration
 *
 * S = integral sqrt(det(gamma)) d sigma^0 d sigma^1
 *
 * Uses Gaussian quadrature for numerical integration.
 *
 * @param chart Chart to compute area for
 * @param position_fn Position function X(sigma)
 * @param user_data Context for position function
 * @param area Output: computed surface area
 * @return 0 on success, -1 on error
 */
int surface_compute_chart_area(
    const surface_chart_t* chart,
    surface_position_fn position_fn,
    void* user_data,
    float* area
);

/**
 * @brief Compute surface area of cylindrical link
 *
 * For cylinder: S = 2*pi*r*L
 *
 * @param params Cylinder parameters
 * @param area Output: surface area
 * @return 0 on success, -1 on error
 */
int surface_compute_cylinder_area(
    const surface_cylinder_params_t* params,
    float* area
);

/**
 * @brief Compute surface area of conical link
 *
 * @param params Cone parameters
 * @param area Output: surface area
 * @return 0 on success, -1 on error
 */
int surface_compute_cone_area(
    const surface_cone_params_t* params,
    float* area
);

/**
 * @brief Compute total surface area of manifold (Equation 1)
 *
 * S_M(G) = sum_i integral sqrt(det(gamma_i)) d^2 sigma
 *
 * @param manifold Manifold to compute
 * @param total_area Output: total surface area
 * @return 0 on success, -1 on error
 */
int surface_compute_manifold_area(
    surface_manifold_t* manifold,
    float* total_area
);

//=============================================================================
// NAMBU-GOTO ACTION (STRING THEORY)
//=============================================================================

/**
 * @brief Compute Nambu-Goto action for manifold
 *
 * The Nambu-Goto action is formally identical to the surface area:
 * S_NG = T * integral sqrt(-det(gamma)) d^2 sigma
 *
 * where T is the string tension. For our purposes T=1.
 *
 * @param manifold Input manifold
 * @param action Output: Nambu-Goto action
 * @return 0 on success, -1 on error
 */
int surface_compute_nambu_goto_action(
    const surface_manifold_t* manifold,
    float* action
);

/**
 * @brief Apply Strebel constraint to chart
 *
 * Strebel's theorem: In the absence of boundary conditions,
 * the minimal surface is exactly cylindrical.
 * With boundary conditions, we get quadratic differentials.
 *
 * @param chart Chart to constrain
 * @param boundary_circumferences Circumferences at boundaries [4]
 * @return 0 on success, -1 on error
 */
int surface_apply_strebel_constraint(
    surface_chart_t* chart,
    const float boundary_circumferences[4]
);

//=============================================================================
// FEYNMAN DIAGRAM MAPPING
//=============================================================================

/**
 * @brief Map branch point to Feynman vertex
 *
 * @param branch Branch point
 * @param vertex Output: Feynman vertex
 * @return 0 on success, -1 on error
 */
int surface_map_branch_to_feynman(
    const surface_branch_point_t* branch,
    surface_feynman_vertex_t* vertex
);

/**
 * @brief Construct Feynman diagram from manifold
 *
 * Maps surface minimization problem to string theory
 * worldsheet formulation (pants decomposition).
 *
 * @param manifold Input manifold
 * @param diagram Output: Feynman diagram
 * @return 0 on success, -1 on error
 */
int surface_construct_feynman_diagram(
    const surface_manifold_t* manifold,
    surface_feynman_diagram_t* diagram
);

/**
 * @brief Destroy Feynman diagram
 *
 * @param diagram Diagram to destroy
 */
void surface_feynman_diagram_destroy(surface_feynman_diagram_t* diagram);

//=============================================================================
// GEOMETRY CALCULATIONS
//=============================================================================

/**
 * @brief Compute chi parameter: chi = w/r
 *
 * @param circumference Link circumference w
 * @param distance Characteristic distance r
 * @param chi Output: chi value
 * @return 0 on success, -1 on error
 */
int surface_compute_chi(float circumference, float distance, float* chi);

/**
 * @brief Compute rho parameter: rho = w'/w
 *
 * @param w_prime Daughter link circumference
 * @param w Parent link circumference
 * @param rho Output: rho value
 * @return 0 on success, -1 on error
 */
int surface_compute_rho(float w_prime, float w, float* rho);

/**
 * @brief Compute lambda parameter: lambda = l/w
 *
 * @param link_length Intermediate link length l
 * @param circumference Link circumference w
 * @param lambda Output: lambda value
 * @return 0 on success, -1 on error
 */
int surface_compute_lambda(float link_length, float circumference, float* lambda);

/**
 * @brief Compute solid angle from branch directions
 *
 * Omega = 4*pi*sin^2(sum of half-angles / 2)
 *
 * @param directions Array of direction vectors
 * @param num_directions Number of directions
 * @param solid_angle Output: solid angle
 * @return 0 on success, -1 on error
 */
int surface_compute_solid_angle(
    const surface_vec3_t* directions,
    uint32_t num_directions,
    float* solid_angle
);

/**
 * @brief Compute steering angle from paper Fig. 4g
 *
 * For rho < rho_th: Omega = 0 (orthogonal sprout)
 * For rho > rho_th: Omega = k * (rho - rho_th) (linear)
 *
 * @param rho Branch thickness ratio
 * @param rho_threshold Sprouting/branching threshold
 * @param steering_angle Output: steering angle
 * @return 0 on success, -1 on error
 */
int surface_compute_steering_angle(
    float rho,
    float rho_threshold,
    float* steering_angle
);

//=============================================================================
// VECTOR UTILITIES
//=============================================================================

/**
 * @brief Compute dot product of two 3D vectors
 */
float surface_vec3_dot(const surface_vec3_t* a, const surface_vec3_t* b);

/**
 * @brief Compute cross product of two 3D vectors
 */
void surface_vec3_cross(
    const surface_vec3_t* a,
    const surface_vec3_t* b,
    surface_vec3_t* result
);

/**
 * @brief Compute magnitude of 3D vector
 */
float surface_vec3_magnitude(const surface_vec3_t* v);

/**
 * @brief Normalize 3D vector to unit length
 */
int surface_vec3_normalize(surface_vec3_t* v);

/**
 * @brief Compute angle between two 3D vectors
 */
float surface_vec3_angle(const surface_vec3_t* a, const surface_vec3_t* b);

/**
 * @brief Compute distance between two 3D points
 */
float surface_vec3_distance(const surface_vec3_t* a, const surface_vec3_t* b);

//=============================================================================
// NUMERICAL INTEGRATION
//=============================================================================

/**
 * @brief Gaussian quadrature points and weights
 */
typedef struct surface_gauss_quadrature_struct {
    float points[SURFACE_GAUSS_POINTS_MAX];
    float weights[SURFACE_GAUSS_POINTS_MAX];
    uint32_t num_points;
} surface_gauss_quadrature_t;

/**
 * @brief Adaptive quadrature configuration
 *
 * Enables automatic adjustment of quadrature resolution based on
 * surface curvature. Sharp features require more integration points
 * for accurate area computation.
 */
typedef struct surface_adaptive_quadrature_config_struct {
    bool enabled;                   /**< Enable adaptive quadrature */
    uint32_t min_points;            /**< Minimum quadrature points */
    uint32_t max_points;            /**< Maximum quadrature points */
    float curvature_threshold;      /**< Curvature threshold for refinement */
    float target_accuracy;          /**< Target relative accuracy */
} surface_adaptive_quadrature_config_t;

/**
 * @brief Initialize Gaussian quadrature for integration
 *
 * @param quad Output: quadrature structure
 * @param num_points Number of quadrature points
 * @return 0 on success, -1 on error
 */
int surface_gauss_init(
    surface_gauss_quadrature_t* quad,
    uint32_t num_points
);

/**
 * @brief 2D numerical integration using Gaussian quadrature
 *
 * @param integrand Function to integrate
 * @param a0 Lower bound for sigma^0
 * @param b0 Upper bound for sigma^0
 * @param a1 Lower bound for sigma^1
 * @param b1 Upper bound for sigma^1
 * @param user_data Context for integrand
 * @param result Output: integral value
 * @return 0 on success, -1 on error
 */
typedef float (*surface_integrand_fn)(
    float sigma0,
    float sigma1,
    void* user_data
);

int surface_integrate_2d(
    surface_integrand_fn integrand,
    float a0, float b0,
    float a1, float b1,
    void* user_data,
    float* result
);

/**
 * @brief Initialize adaptive quadrature configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_adaptive_quadrature_default_config(
    surface_adaptive_quadrature_config_t* config
);

/**
 * @brief Compute adaptive quadrature points based on curvature
 *
 * @param curvature Estimated surface curvature at region
 * @param config Adaptive quadrature configuration
 * @return Number of quadrature points to use
 */
uint32_t surface_adaptive_quadrature_points(
    float curvature,
    const surface_adaptive_quadrature_config_t* config
);

/**
 * @brief 2D numerical integration using adaptive Gaussian quadrature
 *
 * Automatically adjusts quadrature resolution based on local curvature.
 *
 * @param integrand Function to integrate
 * @param a0 Lower bound for sigma^0
 * @param b0 Upper bound for sigma^0
 * @param a1 Lower bound for sigma^1
 * @param b1 Upper bound for sigma^1
 * @param user_data Context for integrand
 * @param config Adaptive quadrature configuration
 * @param result Output: integral value
 * @return 0 on success, -1 on error
 */
int surface_integrate_2d_adaptive(
    surface_integrand_fn integrand,
    float a0, float b0,
    float a1, float b1,
    void* user_data,
    const surface_adaptive_quadrature_config_t* config,
    float* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_MANIFOLD_H */
