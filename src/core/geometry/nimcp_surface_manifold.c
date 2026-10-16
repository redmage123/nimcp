/**
 * @file nimcp_surface_manifold.c
 * @brief Manifold Mathematics Implementation
 *
 * Implementation of manifold mathematics for surface optimization.
 *
 * @version 1.0.0
 * @date 2026-01-14
 */

#include "core/geometry/nimcp_surface_manifold.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "SurfaceManifold"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_manifold)

//=============================================================================
// NUMERICAL STABILITY CONSTANTS
//=============================================================================

/**
 * Epsilon for vector magnitude checks (normalization, angle computation).
 * WHAT: Threshold below which vectors are considered degenerate/zero.
 * WHY:  SURFACE_MIN_CIRCUMFERENCE (1e-6) is for circumferences, not magnitudes.
 *       Vector magnitudes need tighter tolerance to avoid division instability.
 * HOW:  Use 1e-8f which is well above float denormals (~1e-38) but small enough
 *       to catch near-zero vectors before division causes NaN/Inf.
 */
#define SURFACE_VEC_MAGNITUDE_EPSILON NIMCP_EPSILON_ADAM

//=============================================================================
// VECTOR UTILITIES
//=============================================================================

float surface_vec3_dot(const surface_vec3_t* a, const surface_vec3_t* b)
{
    if (!a || !b) {
        LOG_WARN(LOG_MODULE, "surface_vec3_dot: NULL input (a=%p, b=%p)",
                 (const void*)a, (const void*)b);
        return 0.0f;
    }
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void surface_vec3_cross(
    const surface_vec3_t* a,
    const surface_vec3_t* b,
    surface_vec3_t* result)
{
    if (!a || !b || !result) {
        LOG_WARN(LOG_MODULE, "surface_vec3_cross: NULL input (a=%p, b=%p, result=%p)",
                 (const void*)a, (const void*)b, (void*)result);
        return;
    }

    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
}

float surface_vec3_magnitude(const surface_vec3_t* v)
{
    if (!v) {
        LOG_WARN(LOG_MODULE, "surface_vec3_magnitude: NULL input");
        return 0.0f;
    }
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

int surface_vec3_normalize(surface_vec3_t* v)
{
    if (!v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_vec3_normalize: v is NULL");
        return -1;
    }

    float mag = surface_vec3_magnitude(v);

    /* NUMERICAL STABILITY: Use dedicated magnitude epsilon, not circumference threshold.
     * Check against epsilon before division to avoid NaN/Inf. */
    if (mag < SURFACE_VEC_MAGNITUDE_EPSILON) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_vec3_normalize: validation failed");
        return -1;
    }

    /* Use (1/mag) multiplication instead of division for stability with very small mag.
     * This avoids division-by-zero-adjacent issues near epsilon. */
    float inv_mag = 1.0f / mag;
    v->x *= inv_mag;
    v->y *= inv_mag;
    v->z *= inv_mag;
    return 0;
}

float surface_vec3_angle(const surface_vec3_t* a, const surface_vec3_t* b)
{
    if (!a || !b) {
        LOG_WARN(LOG_MODULE, "surface_vec3_angle: NULL input (a=%p, b=%p)",
                 (const void*)a, (const void*)b);
        return 0.0f;
    }

    float mag_a = surface_vec3_magnitude(a);
    float mag_b = surface_vec3_magnitude(b);

    /* NUMERICAL STABILITY: Use dedicated magnitude epsilon for degenerate check */
    if (mag_a < SURFACE_VEC_MAGNITUDE_EPSILON || mag_b < SURFACE_VEC_MAGNITUDE_EPSILON) {
        LOG_DEBUG(LOG_MODULE, "surface_vec3_angle: degenerate vector (mag_a=%.2e, mag_b=%.2e)",
                  (double)mag_a, (double)mag_b);
        return 0.0f;
    }

    float dot = surface_vec3_dot(a, b);
    float cos_angle = dot / (mag_a * mag_b);

    /* NUMERICAL STABILITY: Check for NaN before clamping.
     * NaN can occur from 0/0, inf/inf, or corrupted inputs.
     * Return 0 (parallel vectors) as safe fallback. */
    if (isnan(cos_angle) || isinf(cos_angle)) {
        LOG_WARN(LOG_MODULE, "surface_vec3_angle: numerical instability (cos_angle=%f)", (double)cos_angle);
        return 0.0f;
    }

    /* NUMERICAL STABILITY: Use fminf/fmaxf intrinsics instead of if-statements.
     * These are branchless and may use CPU SIMD instructions.
     * Clamp to [-1, 1] to prevent acosf domain error. */
    cos_angle = fmaxf(-1.0f, fminf(1.0f, cos_angle));

    return acosf(cos_angle);
}

float surface_vec3_distance(const surface_vec3_t* a, const surface_vec3_t* b)
{
    if (!a || !b) {
        LOG_WARN(LOG_MODULE, "surface_vec3_distance: NULL input (a=%p, b=%p)",
                 (const void*)a, (const void*)b);
        return 0.0f;
    }

    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

//=============================================================================
// GEOMETRY CALCULATIONS
//=============================================================================

int surface_compute_chi(float circumference, float distance, float* chi)
{
    if (!chi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_chi: chi is NULL");
        return -1;
    }
    if (distance < SURFACE_MIN_CIRCUMFERENCE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_compute_chi: validation failed");
        return -1;
    }

    *chi = circumference / distance;
    return 0;
}

int surface_compute_rho(float w_prime, float w, float* rho)
{
    if (!rho) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_rho: rho is NULL");
        return -1;
    }
    if (w < SURFACE_MIN_CIRCUMFERENCE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_compute_rho: validation failed");
        return -1;
    }

    *rho = w_prime / w;
    return 0;
}

int surface_compute_lambda(float link_length, float circumference, float* lambda)
{
    if (!lambda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_lambda: lambda is NULL");
        return -1;
    }
    if (circumference < SURFACE_MIN_CIRCUMFERENCE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_compute_lambda: validation failed");
        return -1;
    }

    *lambda = link_length / circumference;
    return 0;
}

int surface_compute_solid_angle(
    const surface_vec3_t* directions,
    uint32_t num_directions,
    float* solid_angle)
{
    if (!directions || !solid_angle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_solid_angle: required parameter is NULL (directions, solid_angle)");
        return -1;
    }
    if (num_directions < 2) {
        *solid_angle = 0.0f;
        return 0;
    }

    /* Compute solid angle using spherical excess */
    float sum_angles = 0.0f;
    for (uint32_t i = 0; i < num_directions; i++) {
        uint32_t j = (i + 1) % num_directions;
        float angle = surface_vec3_angle(&directions[i], &directions[j]);
        sum_angles += angle;
    }

    /* For planar configurations, solid angle = 2*pi */
    /* Use spherical excess formula approximation */
    *solid_angle = sum_angles;
    return 0;
}

int surface_compute_steering_angle(
    float rho,
    float rho_threshold,
    float* steering_angle)
{
    if (!steering_angle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_steering_angle: steering_angle is NULL");
        return -1;
    }

    if (rho < rho_threshold) {
        /* Sprouting regime: orthogonal (no steering) */
        *steering_angle = 0.0f;
    } else {
        /* Branching regime: linear increase */
        float k = 2.5f;  /* Slope from paper fit */
        *steering_angle = k * (rho - rho_threshold);
        if (*steering_angle > M_PI) {
            *steering_angle = M_PI;
        }
    }

    return 0;
}

//=============================================================================
// MANIFOLD CREATION AND DESTRUCTION
//=============================================================================

surface_manifold_t* surface_manifold_create(
    uint32_t max_charts,
    uint32_t max_branch_points)
{
    surface_manifold_t* manifold = nimcp_malloc(sizeof(*manifold));
    if (!manifold) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "manifold is NULL");

        return NULL;

    }

    memset(manifold, 0, sizeof(*manifold));

    /* Allocate charts */
    manifold->charts = nimcp_malloc(max_charts * sizeof(surface_chart_t));
    if (!manifold->charts) {
        nimcp_free(manifold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_manifold_create: manifold->charts is NULL");
        return NULL;
    }
    memset(manifold->charts, 0, max_charts * sizeof(surface_chart_t));
    manifold->capacity_charts = max_charts;

    /* Allocate branch points */
    manifold->branch_points = nimcp_malloc(
        max_branch_points * sizeof(surface_branch_point_t)
    );
    if (!manifold->branch_points) {
        nimcp_free(manifold->charts);
        nimcp_free(manifold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_manifold_create: manifold->branch_points is NULL");
        return NULL;
    }
    memset(manifold->branch_points, 0,
           max_branch_points * sizeof(surface_branch_point_t));
    manifold->capacity_branch_points = max_branch_points;

    return manifold;
}

void surface_manifold_destroy(surface_manifold_t* manifold)
{
    if (!manifold) return;

    if (manifold->feynman) {
        surface_feynman_diagram_destroy(manifold->feynman);
    }

    if (manifold->branch_points) {
        nimcp_free(manifold->branch_points);
    }

    if (manifold->charts) {
        nimcp_free(manifold->charts);
    }

    nimcp_free(manifold);
}

int surface_manifold_reset(surface_manifold_t* manifold)
{
    if (!manifold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_manifold_reset: manifold is NULL");
        return -1;
    }

    if (manifold->charts) {
        memset(manifold->charts, 0,
               manifold->capacity_charts * sizeof(surface_chart_t));
    }
    manifold->num_charts = 0;

    if (manifold->branch_points) {
        memset(manifold->branch_points, 0,
               manifold->capacity_branch_points * sizeof(surface_branch_point_t));
    }
    manifold->num_branch_points = 0;

    if (manifold->feynman) {
        surface_feynman_diagram_destroy(manifold->feynman);
        manifold->feynman = NULL;
    }

    manifold->total_surface_area = 0.0f;
    manifold->total_wire_length = 0.0f;
    manifold->total_circumference = 0.0f;
    manifold->is_computed = false;
    manifold->is_optimized = false;
    manifold->computation_iterations = 0;

    return 0;
}

//=============================================================================
// CHART OPERATIONS
//=============================================================================

int surface_manifold_add_chart(
    surface_manifold_t* manifold,
    uint32_t link_id,
    surface_chart_type_t type,
    float circumference,
    uint32_t* chart_id)
{
    if (!manifold || !chart_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_manifold_add_chart: required parameter is NULL (manifold, chart_id)");
        return -1;
    }
    if (manifold->num_charts >= manifold->capacity_charts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_manifold_add_chart: capacity exceeded");
        return -1;
    }

    uint32_t id = manifold->num_charts;
    surface_chart_t* chart = &manifold->charts[id];

    chart->id = id;
    chart->link_id = link_id;
    chart->type = type;
    chart->circumference = circumference;

    /* Default coordinate bounds */
    chart->sigma0_min = 0.0f;
    chart->sigma0_max = 1.0f;
    chart->sigma1_min = 0.0f;
    chart->sigma1_max = 2.0f * M_PI;

    manifold->num_charts++;
    *chart_id = id;

    return 0;
}

int surface_manifold_connect_charts(
    surface_manifold_t* manifold,
    uint32_t chart1_id,
    uint32_t chart2_id,
    uint32_t boundary_index)
{
    if (!manifold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_manifold_connect_charts: manifold is NULL");
        return -1;
    }
    if (chart1_id >= manifold->num_charts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_manifold_connect_charts: capacity exceeded");
        return -1;
    }
    if (chart2_id >= manifold->num_charts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_manifold_connect_charts: capacity exceeded");
        return -1;
    }
    if (boundary_index >= 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "surface_manifold_connect_charts: capacity exceeded");
        return -1;
    }

    surface_chart_t* chart1 = &manifold->charts[chart1_id];
    chart1->boundary_chart_ids[boundary_index] = chart2_id;
    if (chart1->num_boundaries <= boundary_index) {
        chart1->num_boundaries = boundary_index + 1;
    }

    return 0;
}

//=============================================================================
// METRIC TENSOR COMPUTATION
//=============================================================================

int surface_compute_metric_tensor(
    surface_position_fn position_fn,
    const surface_sigma_t* sigma,
    void* user_data,
    surface_metric_tensor_t* metric)
{
    if (!position_fn || !sigma || !metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_metric_tensor: required parameter is NULL (position_fn, sigma, metric)");
        return -1;
    }

    const float h = 1e-4f;  /* Finite difference step */

    /* Compute Jacobian numerically */
    surface_jacobian_t jacobian;

    /* dX/d(sigma^0) */
    surface_sigma_t sigma_plus = {sigma->sigma0 + h, sigma->sigma1};
    surface_sigma_t sigma_minus = {sigma->sigma0 - h, sigma->sigma1};

    for (int i = 0; i < 3; i++) {
        float plus = position_fn(&sigma_plus, i, user_data);
        float minus = position_fn(&sigma_minus, i, user_data);
        jacobian.dX_dsigma0[i] = (plus - minus) / (2.0f * h);
    }

    /* dX/d(sigma^1) */
    sigma_plus.sigma0 = sigma->sigma0;
    sigma_plus.sigma1 = sigma->sigma1 + h;
    sigma_minus.sigma0 = sigma->sigma0;
    sigma_minus.sigma1 = sigma->sigma1 - h;

    for (int i = 0; i < 3; i++) {
        float plus = position_fn(&sigma_plus, i, user_data);
        float minus = position_fn(&sigma_minus, i, user_data);
        jacobian.dX_dsigma1[i] = (plus - minus) / (2.0f * h);
    }

    /* Compute metric tensor: gamma_ab = (dX/dsigma^a) . (dX/dsigma^b) */
    metric->gamma[0][0] = 0.0f;
    metric->gamma[0][1] = 0.0f;
    metric->gamma[1][0] = 0.0f;
    metric->gamma[1][1] = 0.0f;

    for (int i = 0; i < 3; i++) {
        metric->gamma[0][0] += jacobian.dX_dsigma0[i] * jacobian.dX_dsigma0[i];
        metric->gamma[0][1] += jacobian.dX_dsigma0[i] * jacobian.dX_dsigma1[i];
        metric->gamma[1][0] += jacobian.dX_dsigma1[i] * jacobian.dX_dsigma0[i];
        metric->gamma[1][1] += jacobian.dX_dsigma1[i] * jacobian.dX_dsigma1[i];
    }

    /* Compute determinant */
    metric->determinant = surface_metric_determinant(metric);
    metric->is_valid = (fabsf(metric->determinant) > SURFACE_MIN_DETERMINANT);

    /* Compute inverse if valid */
    if (metric->is_valid) {
        surface_metric_inverse(metric, metric->inverse);
    }

    return 0;
}

int surface_compute_metric_cylinder(
    const surface_cylinder_params_t* params,
    const surface_sigma_t* sigma,
    surface_metric_tensor_t* metric)
{
    if (!params || !sigma || !metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_metric_cylinder: required parameter is NULL (params, sigma, metric)");
        return -1;
    }

    /* For cylinder: gamma = [[1, 0], [0, r^2]] */
    float r = params->radius;

    metric->gamma[0][0] = 1.0f;
    metric->gamma[0][1] = 0.0f;
    metric->gamma[1][0] = 0.0f;
    metric->gamma[1][1] = r * r;

    metric->determinant = r * r;
    metric->is_valid = (r > SURFACE_MIN_CIRCUMFERENCE);

    if (metric->is_valid) {
        metric->inverse[0][0] = 1.0f;
        metric->inverse[0][1] = 0.0f;
        metric->inverse[1][0] = 0.0f;
        metric->inverse[1][1] = 1.0f / (r * r);
    }

    return 0;
}

int surface_compute_metric_cone(
    const surface_cone_params_t* params,
    const surface_sigma_t* sigma,
    surface_metric_tensor_t* metric)
{
    if (!params || !sigma || !metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_metric_cone: required parameter is NULL (params, sigma, metric)");
        return -1;
    }

    /* For cone with varying radius */
    float t = sigma->sigma0 / params->length;  /* Normalized position */
    float r = params->radius_start + t * (params->radius_end - params->radius_start);
    float dr_dt = (params->radius_end - params->radius_start) / params->length;

    /* gamma_00 = 1 + (dr/dt)^2 */
    metric->gamma[0][0] = 1.0f + dr_dt * dr_dt;
    metric->gamma[0][1] = 0.0f;
    metric->gamma[1][0] = 0.0f;
    metric->gamma[1][1] = r * r;

    metric->determinant = metric->gamma[0][0] * metric->gamma[1][1];
    metric->is_valid = (fabsf(metric->determinant) > SURFACE_MIN_DETERMINANT);

    if (metric->is_valid) {
        surface_metric_inverse(metric, metric->inverse);
    }

    return 0;
}

float surface_metric_determinant(const surface_metric_tensor_t* metric)
{
    if (!metric) return 0.0f;
    return metric->gamma[0][0] * metric->gamma[1][1] -
           metric->gamma[0][1] * metric->gamma[1][0];
}

int surface_metric_inverse(
    const surface_metric_tensor_t* metric,
    float inverse[2][2])
{
    if (!metric || !inverse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_metric_inverse: required parameter is NULL (metric, inverse)");
        return -1;
    }

    float det = metric->gamma[0][0] * metric->gamma[1][1] -
                metric->gamma[0][1] * metric->gamma[1][0];

    if (fabsf(det) < SURFACE_MIN_DETERMINANT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_metric_inverse: validation failed");
        return -1;  /* Singular matrix */
    }

    float inv_det = 1.0f / det;

    inverse[0][0] = metric->gamma[1][1] * inv_det;
    inverse[0][1] = -metric->gamma[0][1] * inv_det;
    inverse[1][0] = -metric->gamma[1][0] * inv_det;
    inverse[1][1] = metric->gamma[0][0] * inv_det;

    return 0;
}

//=============================================================================
// SURFACE AREA COMPUTATION
//=============================================================================

int surface_compute_chart_area(
    const surface_chart_t* chart,
    surface_position_fn position_fn,
    void* user_data,
    float* area)
{
    if (!chart || !position_fn || !area) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_chart_area: required parameter is NULL (chart, position_fn, area)");
        return -1;
    }

    /* Use numerical integration */
    float total_area = 0.0f;
    const int n = 16;  /* Grid resolution */

    float d_sigma0 = (chart->sigma0_max - chart->sigma0_min) / n;
    float d_sigma1 = (chart->sigma1_max - chart->sigma1_min) / n;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            surface_sigma_t sigma = {
                chart->sigma0_min + (i + 0.5f) * d_sigma0,
                chart->sigma1_min + (j + 0.5f) * d_sigma1
            };

            surface_metric_tensor_t metric;
            if (surface_compute_metric_tensor(position_fn, &sigma,
                                               user_data, &metric) == 0) {
                if (metric.is_valid) {
                    total_area += sqrtf(fabsf(metric.determinant)) *
                                  d_sigma0 * d_sigma1;
                }
            }
        }
    }

    *area = total_area;
    return 0;
}

int surface_compute_cylinder_area(
    const surface_cylinder_params_t* params,
    float* area)
{
    if (!params || !area) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_cylinder_area: required parameter is NULL (params, area)");
        return -1;
    }

    /* For cylinder: S = 2*pi*r*L */
    *area = 2.0f * M_PI * params->radius * params->length;
    return 0;
}

int surface_compute_cone_area(
    const surface_cone_params_t* params,
    float* area)
{
    if (!params || !area) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_cone_area: required parameter is NULL (params, area)");
        return -1;
    }

    /* Lateral surface area of truncated cone */
    float r1 = params->radius_start;
    float r2 = params->radius_end;
    float L = params->length;

    float slant = sqrtf(L * L + (r2 - r1) * (r2 - r1));
    *area = M_PI * (r1 + r2) * slant;

    return 0;
}

int surface_compute_manifold_area(
    surface_manifold_t* manifold,
    float* total_area)
{
    if (!manifold || !total_area) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_manifold_area: required parameter is NULL (manifold, total_area)");
        return -1;
    }

    float area = 0.0f;

    /* Sum area contributions from all branch points */
    for (uint32_t i = 0; i < manifold->num_branch_points; i++) {
        const surface_branch_point_t* bp = &manifold->branch_points[i];

        /* Compute cylindrical area for each link */
        for (uint32_t j = 0; j < bp->degree && j < SURFACE_MAX_BRANCH_DEGREE; j++) {
            float radius = bp->link_diameters[j] / (2.0f * M_PI);
            float length = surface_vec3_magnitude(&bp->link_directions[j]);

            /* Each link is counted at both endpoints, so divide by 2 */
            area += M_PI * radius * length;
        }
    }

    /* If no branch points, use charts */
    if (manifold->num_branch_points == 0) {
        for (uint32_t i = 0; i < manifold->num_charts; i++) {
            const surface_chart_t* chart = &manifold->charts[i];

            if (chart->type == SURFACE_CHART_CYLINDRICAL) {
                float radius = chart->circumference / (2.0f * M_PI);
                float length = chart->sigma0_max - chart->sigma0_min;
                area += 2.0f * M_PI * radius * length;
            }
        }
    }

    manifold->total_surface_area = area;
    manifold->is_computed = true;
    *total_area = area;

    return 0;
}

//=============================================================================
// NAMBU-GOTO ACTION
//=============================================================================

int surface_compute_nambu_goto_action(
    const surface_manifold_t* manifold,
    float* action)
{
    if (!manifold || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_nambu_goto_action: required parameter is NULL (manifold, action)");
        return -1;
    }

    /* Nambu-Goto action is identical to surface area (T=1) */
    *action = manifold->total_surface_area;
    return 0;
}

int surface_apply_strebel_constraint(
    surface_chart_t* chart,
    const float boundary_circumferences[4])
{
    if (!chart || !boundary_circumferences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_apply_strebel_constraint: required parameter is NULL (chart, boundary_circumferences)");
        return -1;
    }

    /* Strebel's theorem: In the absence of boundary conditions,
     * the minimal surface is exactly cylindrical.
     * Store boundary circumferences for use in optimization. */
    chart->circumference = boundary_circumferences[0];

    return 0;
}

//=============================================================================
// FEYNMAN DIAGRAM MAPPING
//=============================================================================

int surface_map_branch_to_feynman(
    const surface_branch_point_t* branch,
    surface_feynman_vertex_t* vertex)
{
    if (!branch || !vertex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_map_branch_to_feynman: required parameter is NULL (branch, vertex)");
        return -1;
    }

    memset(vertex, 0, sizeof(*vertex));

    vertex->id = branch->id;
    vertex->branch_point_id = branch->id;
    vertex->valence = branch->degree;

    /* Map link IDs to leg IDs */
    for (uint32_t i = 0; i < branch->degree && i < SURFACE_MAX_BRANCH_DEGREE; i++) {
        vertex->leg_ids[i] = branch->link_ids[i];
        vertex->moduli[i] = branch->link_diameters[i];
    }

    return 0;
}

int surface_construct_feynman_diagram(
    const surface_manifold_t* manifold,
    surface_feynman_diagram_t* diagram)
{
    if (!manifold || !diagram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_construct_feynman_diagram: required parameter is NULL (manifold, diagram)");
        return -1;
    }

    memset(diagram, 0, sizeof(*diagram));

    if (manifold->num_branch_points == 0) {
        return 0;  /* Empty diagram */
    }

    /* Allocate vertices */
    diagram->vertices = nimcp_malloc(
        manifold->num_branch_points * sizeof(surface_feynman_vertex_t)
    );
    if (!diagram->vertices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_construct_feynman_diagram: diagram->vertices is NULL");
        return -1;
    }

    /* Map each branch point to a vertex */
    for (uint32_t i = 0; i < manifold->num_branch_points; i++) {
        surface_map_branch_to_feynman(
            &manifold->branch_points[i],
            &diagram->vertices[i]
        );
    }
    diagram->num_vertices = manifold->num_branch_points;

    /* Total action */
    diagram->total_action = manifold->total_surface_area;

    return 0;
}

void surface_feynman_diagram_destroy(surface_feynman_diagram_t* diagram)
{
    if (!diagram) return;

    if (diagram->vertices) {
        nimcp_free(diagram->vertices);
    }

    if (diagram->propagators) {
        nimcp_free(diagram->propagators);
    }

    memset(diagram, 0, sizeof(*diagram));
}

//=============================================================================
// GAUSSIAN QUADRATURE
//=============================================================================

int surface_gauss_init(
    surface_gauss_quadrature_t* quad,
    uint32_t num_points)
{
    if (!quad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_gauss_init: quad is NULL");
        return -1;
    }
    if (num_points > SURFACE_GAUSS_POINTS) {
        num_points = SURFACE_GAUSS_POINTS;
    }

    memset(quad, 0, sizeof(*quad));
    quad->num_points = num_points;

    /* Standard Gauss-Legendre points and weights for [0,1] */
    if (num_points >= 2) {
        quad->points[0] = 0.21132486540518711774542560974902f;
        quad->points[1] = 0.78867513459481288225457439025098f;
        quad->weights[0] = 0.5f;
        quad->weights[1] = 0.5f;
    }

    if (num_points >= 4) {
        quad->points[2] = 0.06943184420297371238802675555359f;
        quad->points[3] = 0.93056815579702628761197324444640f;
        quad->weights[2] = 0.17392742256872692868653197461099f;
        quad->weights[3] = 0.17392742256872692868653197461099f;
    }

    return 0;
}

int surface_integrate_2d(
    surface_integrand_fn integrand,
    float a0, float b0,
    float a1, float b1,
    void* user_data,
    float* result)
{
    if (!integrand || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_integrate_2d: required parameter is NULL (integrand, result)");
        return -1;
    }

    const int n = 8;  /* Quadrature points per dimension */
    float total = 0.0f;

    float h0 = (b0 - a0) / n;
    float h1 = (b1 - a1) / n;

    for (int i = 0; i < n; i++) {
        float sigma0 = a0 + (i + 0.5f) * h0;
        for (int j = 0; j < n; j++) {
            float sigma1 = a1 + (j + 0.5f) * h1;
            total += integrand(sigma0, sigma1, user_data);
        }
    }

    *result = total * h0 * h1;
    return 0;
}

//=============================================================================
// ADAPTIVE QUADRATURE
//=============================================================================

int surface_adaptive_quadrature_default_config(
    surface_adaptive_quadrature_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_adaptive_quadrature_default_config: config is NULL");
        return -1;
    }

    config->enabled = true;
    config->min_points = SURFACE_GAUSS_POINTS_MIN;
    config->max_points = SURFACE_GAUSS_POINTS_MAX;
    config->curvature_threshold = SURFACE_CURVATURE_THRESHOLD_HIGH;
    config->target_accuracy = 1e-4f;

    return 0;
}

uint32_t surface_adaptive_quadrature_points(
    float curvature,
    const surface_adaptive_quadrature_config_t* config)
{
    if (!config || !config->enabled) {
        return SURFACE_GAUSS_POINTS;  /* Default */
    }

    /* Scale points based on curvature */
    float abs_curv = fabsf(curvature);

    if (abs_curv < config->curvature_threshold * 0.1f) {
        /* Low curvature: use minimum points */
        return config->min_points;
    } else if (abs_curv > config->curvature_threshold) {
        /* High curvature: scale up to maximum */
        float scale = abs_curv / config->curvature_threshold;
        uint32_t points = (uint32_t)(SURFACE_GAUSS_POINTS * scale);
        if (points > config->max_points) {
            points = config->max_points;
        }
        return points;
    } else {
        /* Medium curvature: use default */
        return SURFACE_GAUSS_POINTS;
    }
}

int surface_integrate_2d_adaptive(
    surface_integrand_fn integrand,
    float a0, float b0,
    float a1, float b1,
    void* user_data,
    const surface_adaptive_quadrature_config_t* config,
    float* result)
{
    if (!integrand || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_integrate_2d_adaptive: required parameter is NULL (integrand, result)");
        return -1;
    }

    /* Use default config if none provided */
    surface_adaptive_quadrature_config_t default_config;
    if (!config) {
        surface_adaptive_quadrature_default_config(&default_config);
        config = &default_config;
    }

    /* Two-level adaptive integration:
     * 1. Coarse pass to estimate curvature
     * 2. Fine pass with adapted resolution */

    /* Coarse pass with minimum points */
    uint32_t n_coarse = config->min_points;
    float h0_coarse = (b0 - a0) / n_coarse;
    float h1_coarse = (b1 - a1) / n_coarse;

    float coarse_total = 0.0f;
    float max_variation = 0.0f;
    float prev_value = 0.0f;

    for (uint32_t i = 0; i < n_coarse; i++) {
        float sigma0 = a0 + (i + 0.5f) * h0_coarse;
        for (uint32_t j = 0; j < n_coarse; j++) {
            float sigma1 = a1 + (j + 0.5f) * h1_coarse;
            float value = integrand(sigma0, sigma1, user_data);
            coarse_total += value;

            /* Track variation as proxy for curvature */
            if (i > 0 || j > 0) {
                float variation = fabsf(value - prev_value);
                if (variation > max_variation) {
                    max_variation = variation;
                }
            }
            prev_value = value;
        }
    }

    /* Estimate curvature from variation */
    float estimated_curvature = max_variation / (h0_coarse + h1_coarse);

    /* Determine adaptive resolution */
    uint32_t n_fine = surface_adaptive_quadrature_points(estimated_curvature, config);

    /* If no refinement needed, return coarse result */
    if (n_fine <= n_coarse) {
        *result = coarse_total * h0_coarse * h1_coarse;
        return 0;
    }

    /* Fine pass with adapted resolution */
    float h0_fine = (b0 - a0) / n_fine;
    float h1_fine = (b1 - a1) / n_fine;
    float fine_total = 0.0f;

    for (uint32_t i = 0; i < n_fine; i++) {
        float sigma0 = a0 + (i + 0.5f) * h0_fine;
        for (uint32_t j = 0; j < n_fine; j++) {
            float sigma1 = a1 + (j + 0.5f) * h1_fine;
            fine_total += integrand(sigma0, sigma1, user_data);
        }
    }

    *result = fine_total * h0_fine * h1_fine;

    /* Log if significant refinement was needed */
    if (n_fine > SURFACE_GAUSS_POINTS * 2) {
        LOG_DEBUG(LOG_MODULE, "Adaptive quadrature: curvature=%.3f, points=%u->%u",
                  (double)estimated_curvature, n_coarse, n_fine);
    }

    return 0;
}
