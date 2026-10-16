/**
 * @file nimcp_surface_geometry.c
 * @brief Surface Geometry Optimization - Core Implementation
 *
 * Implementation of surface optimization algorithms based on
 * Meng et al. Nature 2026 paper predictions.
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_manifold.h"
#include "core/geometry/nimcp_surface_optimization.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(surface_geometry)

//=============================================================================
// INTERNAL CONTEXT STRUCTURE
//=============================================================================

struct surface_geometry_ctx_struct {
    /* Configuration */
    surface_geometry_config_t config;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Manifold for computations */
    surface_manifold_t* manifold;

    /* Statistics */
    surface_geometry_stats_t stats;

    /* Layer communication buffers */
    surface_layer_comm_t layer_comm;

    /* State flags */
    bool initialized;
    bool bio_async_connected;
    bool quantum_connected;
    bool immune_connected;

    /* Metabolic modulation factor */
    float metabolic_quality;
};

//=============================================================================
// CONFIGURATION
//=============================================================================

int surface_geometry_default_config(surface_geometry_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Paper-derived thresholds */
    config->chi_trifurcation_threshold = SURFACE_CHI_TRIFURCATION_THRESHOLD;
    config->rho_threshold = SURFACE_RHO_THRESHOLD_DEFAULT;
    config->angle_tolerance = 0.1f * M_PI;  /* 10% tolerance */
    config->convergence_tolerance = SURFACE_CONVERGENCE_TOL;

    /* Optimization settings */
    config->method = SURFACE_OPT_GRADIENT_DESCENT;
    config->max_iterations = SURFACE_MAX_ITERATIONS;
    config->monte_carlo_samples = 10000;

    /* Material constraints */
    config->min_circumference = SURFACE_MIN_CIRCUMFERENCE;
    config->material_budget = 0.0f;  /* No limit by default */
    config->enforce_budget = false;

    /* Metabolic modulation */
    config->enable_metabolic = true;
    config->metabolic_quality_min = 0.5f;

    /* Integration flags */
    config->enable_bio_async = true;
    config->bio_async_buffer_size = 256;
    config->enable_quantum = false;  /* Opt-in */
    config->quantum_samples = 1000;

    /* Debug/validation */
    config->validate_predictions = true;
    config->verbose = false;

    return 0;
}

//=============================================================================
// CONTEXT CREATION AND DESTRUCTION
//=============================================================================

surface_geometry_ctx_t* surface_geometry_create(
    const surface_geometry_config_t* config)
{
    surface_geometry_ctx_t* ctx = nimcp_malloc(sizeof(*ctx));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_geometry_create: failed to allocate context");
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));

    /* Apply configuration */
    if (config) {
        memcpy(&ctx->config, config, sizeof(ctx->config));
    } else {
        surface_geometry_default_config(&ctx->config);
    }

    /* Create mutex */
    ctx->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!ctx->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_geometry_create: failed to allocate mutex");
        nimcp_free(ctx);
        return NULL;
    }
    if (nimcp_mutex_init(ctx->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "surface_geometry_create: failed to init mutex");
        nimcp_free(ctx->mutex);
        nimcp_free(ctx);
        return NULL;
    }

    /* Create manifold */
    ctx->manifold = surface_manifold_create(
        SURFACE_MAX_CHARTS,
        SURFACE_MAX_BRANCH_POINTS
    );
    if (!ctx->manifold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "surface_geometry_create: failed to create manifold");
        nimcp_mutex_free(ctx->mutex);
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize state */
    ctx->metabolic_quality = 1.0f;
    ctx->initialized = true;

    return ctx;
}

void surface_geometry_destroy(surface_geometry_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->manifold) {
        surface_manifold_destroy(ctx->manifold);
    }

    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

int surface_geometry_reset(surface_geometry_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_reset: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->manifold) {
        surface_manifold_reset(ctx->manifold);
    }

    memset(&ctx->stats, 0, sizeof(ctx->stats));
    memset(&ctx->layer_comm, 0, sizeof(ctx->layer_comm));
    ctx->metabolic_quality = 1.0f;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int surface_geometry_get_config(
    const surface_geometry_ctx_t* ctx,
    surface_geometry_config_t* config)
{
    if (!ctx || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_get_config: ctx or config is NULL");
        return -1;
    }

    memcpy(config, &ctx->config, sizeof(*config));
    return 0;
}

int surface_geometry_set_config(
    surface_geometry_ctx_t* ctx,
    const surface_geometry_config_t* config)
{
    if (!ctx || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_set_config: ctx or config is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    memcpy(&ctx->config, config, sizeof(*config));
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// PARAMETER COMPUTATION (Paper Equations)
//=============================================================================

int surface_determine_regime(
    float rho,
    float rho_threshold,
    surface_regime_t* regime)
{
    if (!regime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_determine_regime: regime is NULL");
        return -1;
    }

    if (rho < 0.0f || rho_threshold < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_determine_regime: negative rho or threshold");
        return -1;
    }

    if (rho < rho_threshold) {
        *regime = SURFACE_REGIME_SPROUTING;
    } else {
        *regime = SURFACE_REGIME_BRANCHING;
    }

    return 0;
}

int surface_predict_branch_type(
    float chi,
    surface_branch_type_t* branch_type)
{
    if (!branch_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_predict_branch_type: branch_type is NULL");
        return -1;
    }

    if (chi < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_predict_branch_type: negative chi");
        return -1;
    }

    /* Paper prediction: trifurcations emerge at chi >= 0.83 */
    if (chi < SURFACE_CHI_TRIFURCATION_THRESHOLD) {
        *branch_type = SURFACE_BRANCH_BIFURCATION;
    } else {
        *branch_type = SURFACE_BRANCH_TRIFURCATION;
    }

    return 0;
}

int surface_compute_optimal_angle(
    const surface_geometry_params_t* params,
    float* optimal_angle)
{
    if (!params || !optimal_angle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_optimal_angle: params or optimal_angle is NULL");
        return -1;
    }

    float rho = params->rho;
    float rho_th = params->rho_threshold;

    /* Paper Fig. 4g: bimodal angle behavior */
    if (rho < rho_th) {
        /* Sprouting regime: orthogonal branching (90 degrees) */
        *optimal_angle = M_PI / 2.0f;  /* 90 degrees */
    } else {
        /* Branching regime: angle increases from 90 to ~120 degrees */
        /* Linear interpolation from pi/2 to 2*pi/3 as rho goes from rho_th to 1 */
        float t = (rho - rho_th) / (1.0f - rho_th);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        /* Interpolate from 90 to 120 degrees */
        float angle_start = M_PI / 2.0f;    /* 90 degrees */
        float angle_end = 2.0f * M_PI / 3.0f; /* 120 degrees (Steiner) */
        *optimal_angle = angle_start + t * (angle_end - angle_start);
    }

    return 0;
}

int surface_compute_branch_params(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch,
    surface_geometry_params_t* params)
{
    if (!ctx || !branch || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_branch_params: required parameter is NULL (ctx, branch, params)");
        return -1;
    }

    memset(params, 0, sizeof(*params));

    /* Compute characteristic distance (average link length) */
    float total_dist = 0.0f;
    for (uint32_t i = 0; i < branch->degree && i < SURFACE_MAX_BRANCH_DEGREE; i++) {
        float dist = surface_vec3_magnitude(&branch->link_directions[i]);
        total_dist += dist;
    }
    float avg_dist = (branch->degree > 0) ? (total_dist / branch->degree) : 1.0f;
    params->distance = avg_dist;

    /* Find parent (largest diameter) and compute rho */
    float max_diam = 0.0f;
    uint32_t parent_idx = 0;
    for (uint32_t i = 0; i < branch->degree && i < SURFACE_MAX_BRANCH_DEGREE; i++) {
        if (branch->link_diameters[i] > max_diam) {
            max_diam = branch->link_diameters[i];
            parent_idx = i;
        }
    }

    float parent_circ = max_diam * M_PI;  /* w = pi * d */
    params->circumference = parent_circ;

    /* Compute chi = w/r */
    if (avg_dist > SURFACE_MIN_CIRCUMFERENCE) {
        surface_compute_chi(parent_circ, avg_dist, &params->chi);
    } else {
        params->chi = 0.0f;
    }

    /* Compute rho for each child */
    float sum_rho = 0.0f;
    uint32_t child_count = 0;
    for (uint32_t i = 0; i < branch->degree && i < SURFACE_MAX_BRANCH_DEGREE; i++) {
        if (i != parent_idx) {
            float child_circ = branch->link_diameters[i] * M_PI;
            float rho_i;
            if (surface_compute_rho(child_circ, parent_circ, &rho_i) == 0) {
                sum_rho += rho_i;
                child_count++;
            }
        }
    }
    params->rho = (child_count > 0) ? (sum_rho / child_count) : 1.0f;
    params->rho_threshold = ctx->config.rho_threshold;

    /* Determine regime and branch type */
    surface_determine_regime(params->rho, params->rho_threshold, &params->regime);
    surface_predict_branch_type(params->chi, &params->branch_type);

    /* Compute solid angle */
    surface_compute_solid_angle(
        branch->link_directions,
        branch->degree,
        &params->solid_angle
    );

    /* Compute steering angle */
    surface_compute_optimal_angle(params, &params->steering_angle);

    /* Compute pairwise angles */
    params->num_angles = 0;
    for (uint32_t i = 0; i < branch->degree && i < SURFACE_MAX_BRANCH_DEGREE; i++) {
        for (uint32_t j = i + 1; j < branch->degree && j < SURFACE_MAX_BRANCH_DEGREE; j++) {
            if (params->num_angles < SURFACE_MAX_BRANCH_DEGREE) {
                params->branch_angles[params->num_angles] = surface_vec3_angle(
                    &branch->link_directions[i],
                    &branch->link_directions[j]
                );
                params->num_angles++;
            }
        }
    }

    /* Set flags */
    params->is_planar = SURFACE_IS_PLANAR(params->solid_angle);
    params->is_symmetric = true;
    for (uint32_t i = 0; i < params->num_angles; i++) {
        if (!SURFACE_IS_STEINER_SYMMETRIC(params->branch_angles[i])) {
            params->is_symmetric = false;
            break;
        }
    }
    params->is_optimal = true;  /* Assume optimal until validation fails */

    return 0;
}

//=============================================================================
// SURFACE AREA COMPUTATION
//=============================================================================

int surface_compute_area(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* total_area)
{
    if (!ctx || !branch_points || !total_area) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_area: required parameter is NULL (ctx, branch_points, total_area)");
        return -1;
    }
    if (num_points == 0) {
        *total_area = 0.0f;
        return 0;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Reset manifold */
    surface_manifold_reset(ctx->manifold);
    ctx->manifold->min_circumference = min_circumference;

    /* Add branch points to manifold */
    for (uint32_t i = 0; i < num_points; i++) {
        /* Copy branch point */
        if (ctx->manifold->num_branch_points < ctx->manifold->capacity_branch_points) {
            memcpy(&ctx->manifold->branch_points[ctx->manifold->num_branch_points],
                   &branch_points[i],
                   sizeof(surface_branch_point_t));
            ctx->manifold->num_branch_points++;
        }
    }

    /* Compute total area */
    int result = surface_compute_manifold_area(ctx->manifold, total_area);

    /* Update statistics */
    if (result == 0) {
        ctx->stats.total_optimizations++;
        ctx->stats.successful_optimizations++;
    } else {
        ctx->stats.failed_optimizations++;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return result;
}

int surface_compute_steiner_length(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float* wire_length)
{
    if (!branch_points || !wire_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_steiner_length: required parameter is NULL (branch_points, wire_length)");
        return -1;
    }

    float total_length = 0.0f;

    /* Sum all link lengths (approximation) */
    for (uint32_t i = 0; i < num_points; i++) {
        for (uint32_t j = 0; j < branch_points[i].degree && j < SURFACE_MAX_BRANCH_DEGREE; j++) {
            float len = surface_vec3_magnitude(&branch_points[i].link_directions[j]);
            /* Each link counted twice (from both endpoints), so divide by 2 */
            total_length += len * 0.5f;
        }
    }

    *wire_length = total_length;
    return 0;
}

//=============================================================================
// NETWORK OPTIMIZATION
//=============================================================================

int surface_optimize_network(
    surface_geometry_ctx_t* ctx,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result)
{
    if (!ctx || !terminals || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimize_network: required parameter is NULL (ctx, terminals, result)");
        return -1;
    }
    if (num_terminals < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_optimize_network: validation failed");
        return -1;
    }

    /* Special case for 4 terminals (tetrahedral) */
    if (num_terminals == 4) {
        return surface_optimize_tetrahedron(ctx, terminals, min_circumference, result);
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Apply metabolic modulation to optimization quality */
    float quality = ctx->metabolic_quality;
    uint32_t effective_iterations = (uint32_t)(ctx->config.max_iterations * quality);

    /* Create optimizer */
    surface_optimizer_t* optimizer = surface_optimizer_create(
        ctx->config.method,
        NULL  /* Use default method config */
    );
    if (!optimizer) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimize_network: optimizer is NULL");
        return -1;
    }

    /* Initialize with terminals */
    if (surface_optimizer_init(optimizer, terminals, num_terminals, min_circumference) != 0) {
        surface_optimizer_destroy(optimizer);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "surface_optimize_network: validation failed");
        return -1;
    }

    /* Run optimization */
    int ret = surface_optimizer_run(optimizer, result);

    /* Validate if enabled */
    if (ret == 0 && ctx->config.validate_predictions) {
        for (uint32_t i = 0; i < result->num_branch_points; i++) {
            surface_validation_result_t validation;
            surface_validate_geometry(ctx, &result->branch_points[i].params, &validation);
            if (!validation.is_valid) {
                result->branch_points[i].params.is_optimal = false;
            }
        }
    }

    /* Update statistics */
    ctx->stats.total_optimizations++;
    if (result->converged) {
        ctx->stats.successful_optimizations++;
    } else {
        ctx->stats.failed_optimizations++;
    }

    ctx->stats.bifurcations_predicted += result->num_bifurcations;
    ctx->stats.trifurcations_predicted += result->num_trifurcations;
    ctx->stats.sprouts_predicted += result->num_sprouts;
    ctx->stats.synapse_sprouts += result->num_synapse_sprouts;

    surface_optimizer_destroy(optimizer);
    nimcp_mutex_unlock(ctx->mutex);

    return ret;
}

int surface_optimize_tetrahedron(
    surface_geometry_ctx_t* ctx,
    const float terminals[4][3],
    float min_circumference,
    surface_optimization_result_t* result)
{
    if (!ctx || !terminals || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_optimize_tetrahedron: required parameter is NULL (ctx, terminals, result)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Compute chi for this configuration */
    float chi;
    surface_tetrahedron_chi(terminals, min_circumference, &chi);

    /* Call internal optimization */
    int ret = surface_optimize_tetrahedron_internal(
        terminals,
        min_circumference,
        ctx->config.method,
        NULL,
        result
    );

    if (ret == 0) {
        /* Compute lambda (separation parameter) */
        float lambda;
        if (surface_tetrahedron_lambda(result->branch_points,
                                        result->num_branch_points,
                                        min_circumference,
                                        &lambda) == 0) {
            /* Store in first branch point's params for reference */
            if (result->num_branch_points > 0) {
                result->branch_points[0].params.lambda = lambda;
                result->branch_points[0].params.chi = chi;
            }
        }

        /* Count branch types */
        result->num_bifurcations = 0;
        result->num_trifurcations = 0;
        for (uint32_t i = 0; i < result->num_branch_points; i++) {
            if (result->branch_points[i].degree == 3) {
                result->num_bifurcations++;
            } else if (result->branch_points[i].degree == 4) {
                result->num_trifurcations++;
            }
        }

        /* Update stats */
        ctx->stats.total_optimizations++;
        ctx->stats.successful_optimizations++;
        ctx->stats.bifurcations_predicted += result->num_bifurcations;
        ctx->stats.trifurcations_predicted += result->num_trifurcations;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return ret;
}

void surface_optimization_result_free(surface_optimization_result_t* result)
{
    if (!result) return;

    if (result->branch_points) {
        for (uint32_t i = 0; i < result->num_branch_points; i++) {
            /* Free any per-branch allocated memory if needed */
        }
        nimcp_free(result->branch_points);
        result->branch_points = NULL;
    }

    result->num_branch_points = 0;
}

//=============================================================================
// VALIDATION
//=============================================================================

int surface_validate_geometry(
    surface_geometry_ctx_t* ctx,
    const surface_geometry_params_t* params,
    surface_validation_result_t* result)
{
    if (!ctx || !params || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_validate_geometry: required parameter is NULL (ctx, params, result)");
        return -1;
    }

    memset(result, 0, sizeof(*result));
    result->is_valid = true;

    /* Validate chi range */
    result->chi_valid = (params->chi >= 0.0f && params->chi <= 10.0f);
    if (!result->chi_valid) {
        result->is_valid = false;
        result->status = SURFACE_VALIDATION_CHI_OUT_OF_RANGE;
        snprintf(result->message, sizeof(result->message),
                 "Chi out of range: %.3f", params->chi);
        return 0;
    }

    /* Validate rho range */
    result->rho_valid = (params->rho >= 0.0f && params->rho <= 2.0f);
    if (!result->rho_valid) {
        result->is_valid = false;
        result->status = SURFACE_VALIDATION_RHO_OUT_OF_RANGE;
        snprintf(result->message, sizeof(result->message),
                 "Rho out of range: %.3f", params->rho);
        return 0;
    }

    /* Validate branch type vs chi */
    if (params->branch_type == SURFACE_BRANCH_TRIFURCATION &&
        params->chi < SURFACE_CHI_TRIFURCATION_THRESHOLD - 0.1f) {
        result->is_valid = false;
        result->status = SURFACE_VALIDATION_TRIFURCATION_INVALID;
        snprintf(result->message, sizeof(result->message),
                 "Trifurcation invalid at chi=%.3f (threshold=%.3f)",
                 params->chi, SURFACE_CHI_TRIFURCATION_THRESHOLD);
        return 0;
    }

    /* Validate angles */
    result->angles_valid = true;
    float expected_angle;
    surface_compute_optimal_angle(params, &expected_angle);

    result->angle_deviation = fabsf(params->steering_angle - expected_angle);
    if (result->angle_deviation > ctx->config.angle_tolerance) {
        result->angles_valid = false;
        result->is_valid = false;
        result->status = SURFACE_VALIDATION_ANGLE_VIOLATION;
        snprintf(result->message, sizeof(result->message),
                 "Angle deviation %.3f exceeds tolerance %.3f",
                 result->angle_deviation, ctx->config.angle_tolerance);
        return 0;
    }

    /* All validations passed */
    result->status = SURFACE_VALIDATION_VALID;
    result->topology_valid = true;
    result->material_valid = true;
    snprintf(result->message, sizeof(result->message), "Geometry valid");

    /* Update validation stats */
    ctx->stats.validation_passes++;

    return 0;
}

int surface_validate_branch(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch,
    surface_validation_result_t* result)
{
    if (!ctx || !branch || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_validate_branch: required parameter is NULL (ctx, branch, result)");
        return -1;
    }

    /* Compute parameters if not already computed */
    surface_geometry_params_t params;
    surface_compute_branch_params(ctx, branch, &params);

    return surface_validate_geometry(ctx, &params, result);
}

//=============================================================================
// SPINE/AXON INTEGRATION
//=============================================================================

int surface_compute_spine_geometry(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    float spine_diameter,
    const surface_vec3_t* spine_position,
    spine_surface_geometry_t* result)
{
    if (!ctx || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_spine_geometry: required parameter is NULL (ctx, result)");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Compute rho = w'/w (spine/parent ratio) */
    float parent_circ = parent_diameter * M_PI;
    float spine_circ = spine_diameter * M_PI;
    float rho;
    surface_compute_rho(spine_circ, parent_circ, &rho);

    result->params.rho = rho;
    result->params.rho_threshold = ctx->config.rho_threshold;

    /* Determine regime */
    surface_determine_regime(rho, ctx->config.rho_threshold, &result->params.regime);

    /* Compute optimal angle */
    surface_compute_optimal_angle(&result->params, &result->optimal_angle);

    /* Check if this is a sprout (orthogonal) */
    result->is_sprout = (result->params.regime == SURFACE_REGIME_SPROUTING);

    /* Paper prediction: 98% of sprouts end at synapses */
    result->ends_at_synapse = result->is_sprout;  /* Assume true for sprouts */

    /* Compute approximate material cost */
    if (spine_position) {
        float spine_length = surface_vec3_magnitude(spine_position);
        result->material_cost = spine_circ * spine_length;
    }

    result->is_cached = true;

    /* Update statistics */
    if (result->is_sprout) {
        ctx->stats.sprouts_predicted++;
        if (result->ends_at_synapse) {
            ctx->stats.synapse_sprouts++;
        }
    }

    return 0;
}

int surface_predict_spine_sprout(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    float spine_diameter,
    bool* is_sprout)
{
    if (!ctx || !is_sprout) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_predict_spine_sprout: required parameter is NULL (ctx, is_sprout)");
        return -1;
    }

    float parent_circ = parent_diameter * M_PI;
    float spine_circ = spine_diameter * M_PI;
    float rho;
    surface_compute_rho(spine_circ, parent_circ, &rho);

    *is_sprout = (rho < ctx->config.rho_threshold);
    return 0;
}

int surface_compute_axon_branch_geometry(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    const float* child_diameters,
    uint32_t num_children,
    axon_branch_surface_geometry_t* result)
{
    if (!ctx || !child_diameters || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_axon_branch_geometry: required parameter is NULL (ctx, child_diameters, result)");
        return -1;
    }
    if (num_children == 0 || num_children > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_compute_axon_branch_geometry: num_children is zero");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    float parent_circ = parent_diameter * M_PI;
    result->degree = num_children + 1;  /* Include parent */

    /* Compute average rho */
    float sum_rho = 0.0f;
    for (uint32_t i = 0; i < num_children; i++) {
        float child_circ = child_diameters[i] * M_PI;
        float rho_i;
        surface_compute_rho(child_circ, parent_circ, &rho_i);
        sum_rho += rho_i;

        /* Compute optimal angle for this child */
        surface_geometry_params_t child_params = {
            .rho = rho_i,
            .rho_threshold = ctx->config.rho_threshold
        };
        surface_compute_optimal_angle(&child_params, &result->branch_angles[i]);
    }

    result->params.rho = sum_rho / num_children;
    result->params.rho_threshold = ctx->config.rho_threshold;

    /* Determine branch type from parent diameter and characteristic distance */
    /* Approximate chi using average child distance */
    float avg_child_diam = 0.0f;
    for (uint32_t i = 0; i < num_children; i++) {
        avg_child_diam += child_diameters[i];
    }
    avg_child_diam /= num_children;

    result->params.chi = parent_circ / (avg_child_diam * 2.0f);  /* Rough estimate */
    surface_predict_branch_type(result->params.chi, &result->branch_type);

    /* Material cost estimate */
    result->material_cost = parent_circ * 1.0f;  /* Placeholder */

    return 0;
}

//=============================================================================
// SPINE SURFACE CACHE
//=============================================================================

spine_surface_cache_t* surface_spine_cache_create(uint32_t capacity)
{
    spine_surface_cache_t* cache = nimcp_malloc(sizeof(*cache));
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_spine_cache_create: cache is NULL");
        return NULL;
    }

    memset(cache, 0, sizeof(*cache));

    cache->cache = nimcp_malloc(capacity * sizeof(spine_surface_geometry_t*));
    if (!cache->cache) {
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_spine_cache_create: cache->cache is NULL");
        return NULL;
    }

    cache->spine_ids = nimcp_malloc(capacity * sizeof(uint32_t));
    if (!cache->spine_ids) {
        nimcp_free(cache->cache);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_spine_cache_create: cache->spine_ids is NULL");
        return NULL;
    }

    memset(cache->cache, 0, capacity * sizeof(spine_surface_geometry_t*));
    memset(cache->spine_ids, 0, capacity * sizeof(uint32_t));

    cache->capacity = capacity;
    cache->num_cached = 0;
    cache->dirty = false;

    return cache;
}

void surface_spine_cache_destroy(spine_surface_cache_t* cache)
{
    if (!cache) return;

    if (cache->cache) {
        for (uint32_t i = 0; i < cache->num_cached; i++) {
            if (cache->cache[i]) {
                nimcp_free(cache->cache[i]);
            }
        }
        nimcp_free(cache->cache);
    }

    if (cache->spine_ids) {
        nimcp_free(cache->spine_ids);
    }

    nimcp_free(cache);
}

int surface_spine_cache_get(
    spine_surface_cache_t* cache,
    uint32_t spine_id,
    spine_surface_geometry_t* geometry)
{
    if (!cache || !geometry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_spine_cache_get: required parameter is NULL (cache, geometry)");
        return -1;
    }

    /* Linear search (could use hash map for larger caches) */
    for (uint32_t i = 0; i < cache->num_cached; i++) {
        if (cache->spine_ids[i] == spine_id && cache->cache[i]) {
            memcpy(geometry, cache->cache[i], sizeof(*geometry));
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_spine_cache_get: validation failed");
    return -1;  /* Not found */
}

int surface_spine_cache_put(
    spine_surface_cache_t* cache,
    uint32_t spine_id,
    const spine_surface_geometry_t* geometry)
{
    if (!cache || !geometry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_spine_cache_put: required parameter is NULL (cache, geometry)");
        return -1;
    }

    /* Check if already cached */
    for (uint32_t i = 0; i < cache->num_cached; i++) {
        if (cache->spine_ids[i] == spine_id) {
            /* Update existing entry */
            memcpy(cache->cache[i], geometry, sizeof(*geometry));
            return 0;
        }
    }

    /* Add new entry */
    if (cache->num_cached >= cache->capacity) {
        /* Cache full - evict oldest (simple FIFO) */
        if (cache->cache[0]) {
            nimcp_free(cache->cache[0]);
        }
        memmove(&cache->cache[0], &cache->cache[1],
                (cache->capacity - 1) * sizeof(spine_surface_geometry_t*));
        memmove(&cache->spine_ids[0], &cache->spine_ids[1],
                (cache->capacity - 1) * sizeof(uint32_t));
        cache->num_cached--;
    }

    /* Allocate and copy */
    cache->cache[cache->num_cached] = nimcp_malloc(sizeof(spine_surface_geometry_t));
    if (!cache->cache[cache->num_cached]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "surface_spine_cache_put: cache->cache is NULL");
        return -1;
    }

    memcpy(cache->cache[cache->num_cached], geometry, sizeof(*geometry));
    cache->spine_ids[cache->num_cached] = spine_id;
    cache->num_cached++;

    return 0;
}

int surface_spine_cache_invalidate(
    spine_surface_cache_t* cache,
    uint32_t spine_id)
{
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_spine_cache_invalidate: cache is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < cache->num_cached; i++) {
        if (cache->spine_ids[i] == spine_id) {
            if (cache->cache[i]) {
                nimcp_free(cache->cache[i]);
            }
            /* Shift remaining entries */
            memmove(&cache->cache[i], &cache->cache[i + 1],
                    (cache->num_cached - i - 1) * sizeof(spine_surface_geometry_t*));
            memmove(&cache->spine_ids[i], &cache->spine_ids[i + 1],
                    (cache->num_cached - i - 1) * sizeof(uint32_t));
            cache->num_cached--;
            return 0;
        }
    }

    return 0;  /* Not found is OK */
}

int surface_spine_cache_clear(spine_surface_cache_t* cache)
{
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_spine_cache_clear: cache is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < cache->num_cached; i++) {
        if (cache->cache[i]) {
            nimcp_free(cache->cache[i]);
            cache->cache[i] = NULL;
        }
    }

    cache->num_cached = 0;
    cache->dirty = false;

    return 0;
}

//=============================================================================
// STATISTICS
//=============================================================================

int surface_geometry_get_stats(
    const surface_geometry_ctx_t* ctx,
    surface_geometry_stats_t* stats)
{
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    memcpy(stats, &ctx->stats, sizeof(*stats));

    /* Compute derived statistics */
    if (stats->total_optimizations > 0) {
        stats->avg_iterations = (float)stats->successful_optimizations /
                                (float)stats->total_optimizations;
    }

    if (stats->sprouts_predicted > 0) {
        stats->avg_sprout_synapse_ratio =
            (float)stats->synapse_sprouts / (float)stats->sprouts_predicted;
    }

    return 0;
}

int surface_geometry_reset_stats(surface_geometry_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_geometry_reset_stats: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// REGION STATISTICS
//=============================================================================

int surface_compute_region_stats(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    uint32_t region_id,
    surface_region_stats_t* stats)
{
    if (!branch_points || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_compute_region_stats: required parameter is NULL (branch_points, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(*stats));
    stats->region_id = region_id;
    stats->num_branch_points = num_points;

    float sum_chi = 0.0f;
    float sum_rho = 0.0f;

    for (uint32_t i = 0; i < num_points; i++) {
        const surface_branch_point_t* bp = &branch_points[i];

        /* Count branch types */
        if (bp->degree == 3) {
            stats->num_bifurcations++;
        } else if (bp->degree == 4) {
            stats->num_trifurcations++;
        }

        /* Count sprouts */
        if (bp->is_sprout) {
            stats->num_sprouts++;
            if (bp->is_synapse_endpoint) {
                stats->num_synapse_sprouts++;
            }
        }

        /* Sum surface area */
        stats->total_surface_area += bp->local_surface_area;

        /* Sum parameters for averages */
        sum_chi += bp->params.chi;
        sum_rho += bp->params.rho;
    }

    /* Compute averages */
    if (num_points > 0) {
        stats->mean_chi = sum_chi / num_points;
        stats->mean_rho = sum_rho / num_points;
    }

    /* Compute sprout-synapse ratio */
    if (stats->num_sprouts > 0) {
        stats->sprout_synapse_ratio =
            (float)stats->num_synapse_sprouts / (float)stats->num_sprouts;
    }

    return 0;
}

//=============================================================================
// LAYER COMMUNICATION
//=============================================================================

int surface_layer_send_downstream(
    surface_geometry_ctx_t* ctx,
    uint32_t source_layer,
    const void* data,
    size_t data_size)
{
    if (!ctx || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_layer_send_downstream: required parameter is NULL (ctx, data)");
        return -1;
    }
    if (source_layer < 1 || source_layer > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_layer_send_downstream: validation failed");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Store in layer communication buffer */
    switch (source_layer) {
        case 1:
            /* Core -> Neural: computed parameters */
            if (data_size == sizeof(surface_geometry_params_t)) {
                memcpy(&ctx->layer_comm.computed_params, data, data_size);
            }
            break;
        case 2:
            /* Neural -> Region: spine/axon updates */
            /* Would broadcast via bio-async in full implementation */
            break;
        case 3:
            /* Region -> Cognitive: region stats */
            if (data_size == sizeof(surface_region_stats_t)) {
                memcpy(&ctx->layer_comm.region_stats, data, data_size);
            }
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int surface_layer_send_upstream(
    surface_geometry_ctx_t* ctx,
    uint32_t source_layer,
    const void* feedback,
    size_t feedback_size)
{
    if (!ctx || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "surface_layer_send_upstream: required parameter is NULL (ctx, feedback)");
        return -1;
    }
    if (source_layer < 1 || source_layer > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "surface_layer_send_upstream: validation failed");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Process feedback */
    switch (source_layer) {
        case 4:
            /* Cognitive -> Region: budget/threshold adjustments */
            if (feedback_size >= sizeof(float) * 2) {
                const float* adj = (const float*)feedback;
                ctx->layer_comm.material_budget_adjustment = adj[0];
                ctx->layer_comm.rho_threshold_adjustment = adj[1];

                /* Apply rho threshold adjustment */
                ctx->config.rho_threshold += adj[1];
                ctx->config.rho_threshold = SURFACE_CLAMP(
                    ctx->config.rho_threshold, 0.3f, 0.9f
                );
            }
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

const char* surface_regime_name(surface_regime_t regime)
{
    switch (regime) {
        case SURFACE_REGIME_SPROUTING: return "SPROUTING";
        case SURFACE_REGIME_BRANCHING: return "BRANCHING";
        case SURFACE_REGIME_STEINER:   return "STEINER";
        default: return "UNKNOWN";
    }
}

const char* surface_branch_type_name(surface_branch_type_t type)
{
    switch (type) {
        case SURFACE_BRANCH_BIFURCATION:    return "BIFURCATION";
        case SURFACE_BRANCH_TRIFURCATION:   return "TRIFURCATION";
        case SURFACE_BRANCH_QUADFURCATION:  return "QUADFURCATION";
        case SURFACE_BRANCH_HIGHER:         return "HIGHER";
        default: return "UNKNOWN";
    }
}

const char* surface_validation_status_name(surface_validation_status_t status)
{
    switch (status) {
        case SURFACE_VALIDATION_VALID:              return "VALID";
        case SURFACE_VALIDATION_CHI_OUT_OF_RANGE:   return "CHI_OUT_OF_RANGE";
        case SURFACE_VALIDATION_RHO_OUT_OF_RANGE:   return "RHO_OUT_OF_RANGE";
        case SURFACE_VALIDATION_ANGLE_VIOLATION:    return "ANGLE_VIOLATION";
        case SURFACE_VALIDATION_TRIFURCATION_INVALID: return "TRIFURCATION_INVALID";
        case SURFACE_VALIDATION_TOPOLOGY_ERROR:     return "TOPOLOGY_ERROR";
        case SURFACE_VALIDATION_MATERIAL_OVERFLOW:  return "MATERIAL_OVERFLOW";
        default: return "UNKNOWN";
    }
}

const char* surface_error_string(surface_error_t error)
{
    switch (error) {
        case SURFACE_OK:                    return "OK";
        case SURFACE_ERROR_NULL:            return "NULL parameter";
        case SURFACE_ERROR_MEMORY:          return "Memory allocation failed";
        case SURFACE_ERROR_INVALID_PARAM:   return "Invalid parameter";
        case SURFACE_ERROR_DEGENERATE_METRIC: return "Degenerate metric tensor";
        case SURFACE_ERROR_CONVERGENCE:     return "Failed to converge";
        case SURFACE_ERROR_MAX_ITERATIONS:  return "Maximum iterations exceeded";
        case SURFACE_ERROR_INVALID_TOPOLOGY: return "Invalid topology";
        case SURFACE_ERROR_CONSTRAINT_VIOLATION: return "Constraint violation";
        case SURFACE_ERROR_NOT_INITIALIZED: return "Not initialized";
        case SURFACE_ERROR_ALREADY_INITIALIZED: return "Already initialized";
        default: return "Unknown error";
    }
}

