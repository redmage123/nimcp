/**
 * @file nimcp_surface_geometry.h
 * @brief Surface Geometry Optimization Module - Core API
 *
 * WHAT: Surface optimization for physical network branching geometry
 * WHY:  Implements predictions from Meng et al. Nature 2026:
 *       - Trifurcations (k=4) emerge when chi >= 0.83
 *       - Orthogonal sprouting (90 deg) for rho < rho_threshold
 *       - 98% of sprouts end at synapses (dendritic spines)
 *       - Networks are ~25% longer than Steiner predictions
 * HOW:  Maps network to 2D manifolds, computes surface area via
 *       Nambu-Goto action, optimizes geometry under circumference constraints
 *
 * INTEGRATION:
 * - Bio-Async: Broadcasts geometry events, receives modulation requests
 * - Quantum: Uses QMC/QMCTS for optimization landscape exploration
 * - Immune: Reports geometry anomalies as antigens
 * - KG: Registers as geometry module in wiring diagram
 * - Brain: Initialization via factory pattern
 *
 * USAGE:
 *   // Create context with configuration
 *   surface_geometry_config_t config;
 *   surface_geometry_default_config(&config);
 *   surface_geometry_ctx_t* ctx = surface_geometry_create(&config);
 *
 *   // Compute parameters for a branch point
 *   surface_geometry_params_t params;
 *   surface_compute_branch_params(ctx, branch_point, &params);
 *
 *   // Optimize 4-terminal network
 *   float terminals[4][3] = {...};
 *   surface_optimization_result_t result;
 *   surface_optimize_network(ctx, terminals, 4, min_w, &result);
 *
 *   // Cleanup
 *   surface_geometry_destroy(ctx);
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_GEOMETRY_H
#define NIMCP_SURFACE_GEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/geometry/nimcp_surface_manifold.h"
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct surface_geometry_ctx_struct surface_geometry_ctx_t;

//=============================================================================
// CONTEXT CREATION AND DESTRUCTION
//=============================================================================

/**
 * @brief Initialize configuration with default values
 *
 * Sets paper-derived defaults:
 * - chi_trifurcation_threshold = 0.83
 * - rho_threshold = 0.6
 * - enable_bio_async = true
 * - enable_quantum = false (opt-in)
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_geometry_default_config(surface_geometry_config_t* config);

/**
 * @brief Create surface geometry context
 *
 * @param config Configuration (NULL for defaults)
 * @return Created context or NULL on failure
 */
surface_geometry_ctx_t* surface_geometry_create(
    const surface_geometry_config_t* config
);

/**
 * @brief Destroy surface geometry context
 *
 * @param ctx Context to destroy
 */
void surface_geometry_destroy(surface_geometry_ctx_t* ctx);

/**
 * @brief Reset context to initial state
 *
 * @param ctx Context to reset
 * @return 0 on success, -1 on error
 */
int surface_geometry_reset(surface_geometry_ctx_t* ctx);

/**
 * @brief Get current configuration
 *
 * @param ctx Context
 * @param config Output: current configuration
 * @return 0 on success, -1 on error
 */
int surface_geometry_get_config(
    const surface_geometry_ctx_t* ctx,
    surface_geometry_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param ctx Context
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int surface_geometry_set_config(
    surface_geometry_ctx_t* ctx,
    const surface_geometry_config_t* config
);

//=============================================================================
// PARAMETER COMPUTATION (Paper Equations)
//=============================================================================

/**
 * @brief Determine branching regime from rho
 *
 * WHAT: Classify as SPROUTING (rho < rho_th) or BRANCHING (rho >= rho_th)
 * WHY:  Different regimes have different optimal angles
 *
 * @param rho Branch thickness ratio (w'/w)
 * @param rho_threshold Transition threshold (~0.6)
 * @param regime Output: determined regime
 * @return 0 on success, -1 on error
 */
int surface_determine_regime(
    float rho,
    float rho_threshold,
    surface_regime_t* regime
);

/**
 * @brief Predict branch type from chi parameter
 *
 * WHAT: Predict BIFURCATION (k=3) or TRIFURCATION (k=4)
 * WHY:  Paper predicts trifurcations emerge at chi >= 0.83
 *
 * @param chi Thickness/distance ratio (w/r)
 * @param branch_type Output: predicted type
 * @return 0 on success, -1 on error
 */
int surface_predict_branch_type(
    float chi,
    surface_branch_type_t* branch_type
);

/**
 * @brief Compute optimal branching angle
 *
 * WHAT: Surface-minimizing angle for branch configuration
 * WHY:  Paper Fig. 4g shows:
 *       - rho < rho_th: 90 degrees (orthogonal sprout)
 *       - rho > rho_th: ~k*(rho - rho_th) linear increase
 *       - rho = 1: ~60 degrees (Steiner-like)
 *
 * @param params Geometry parameters
 * @param steering_angle Output: optimal steering angle
 * @return 0 on success, -1 on error
 */
int surface_compute_optimal_angle(
    const surface_geometry_params_t* params,
    float* steering_angle
);

/**
 * @brief Compute all parameters for a branch point
 *
 * @param ctx Context
 * @param branch Branch point with position, links, diameters
 * @param params Output: computed parameters
 * @return 0 on success, -1 on error
 */
int surface_compute_branch_params(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch,
    surface_geometry_params_t* params
);

//=============================================================================
// SURFACE AREA COMPUTATION
//=============================================================================

/**
 * @brief Compute surface area for network (Equation 1)
 *
 * S_M(G) = sum_i integral sqrt(det(gamma_i)) d^2 sigma
 *
 * @param ctx Context
 * @param branch_points Array of branch points
 * @param num_points Number of branch points
 * @param min_circumference Minimum link circumference (w)
 * @param total_area Output: total surface area
 * @return 0 on success, -1 on error
 */
int surface_compute_area(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* total_area
);

/**
 * @brief Compute Steiner wire length for comparison
 *
 * @param branch_points Array of branch points
 * @param num_points Number of branch points
 * @param wire_length Output: total wire length
 * @return 0 on success, -1 on error
 */
int surface_compute_steiner_length(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float* wire_length
);

//=============================================================================
// NETWORK OPTIMIZATION
//=============================================================================

/**
 * @brief Optimize branching geometry for terminal nodes
 *
 * Main optimization function: given terminal positions and
 * minimum circumference, find surface-minimizing tree topology.
 *
 * @param ctx Context
 * @param terminals Terminal positions [n][3]
 * @param num_terminals Number of terminals
 * @param min_circumference Minimum link circumference
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_optimize_network(
    surface_geometry_ctx_t* ctx,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result
);

/**
 * @brief Optimize 4-terminal tetrahedral configuration (Fig. 3)
 *
 * Special case for 4 terminals (tetrahedron from paper).
 * Predicts transition from 2 bifurcations to 1 trifurcation
 * at chi ~= 0.83.
 *
 * @param ctx Context
 * @param terminals 4 terminal positions [4][3]
 * @param min_circumference Minimum link circumference
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_optimize_tetrahedron(
    surface_geometry_ctx_t* ctx,
    const float terminals[4][3],
    float min_circumference,
    surface_optimization_result_t* result
);

/**
 * @brief Free optimization result resources
 *
 * @param result Result to free
 */
void surface_optimization_result_free(surface_optimization_result_t* result);

//=============================================================================
// VALIDATION
//=============================================================================

/**
 * @brief Validate geometry against paper predictions
 *
 * Checks:
 * - Chi in valid range
 * - Rho in valid range
 * - Angles match predictions for regime
 * - Branch type matches chi prediction
 *
 * @param ctx Context
 * @param params Parameters to validate
 * @param result Output: validation result
 * @return 0 on success, -1 on error
 */
int surface_validate_geometry(
    surface_geometry_ctx_t* ctx,
    const surface_geometry_params_t* params,
    surface_validation_result_t* result
);

/**
 * @brief Validate branch point configuration
 *
 * @param ctx Context
 * @param branch Branch point to validate
 * @param result Output: validation result
 * @return 0 on success, -1 on error
 */
int surface_validate_branch(
    surface_geometry_ctx_t* ctx,
    const surface_branch_point_t* branch,
    surface_validation_result_t* result
);

//=============================================================================
// SPINE/AXON INTEGRATION
//=============================================================================

/**
 * @brief Compute surface geometry for dendritic spine
 *
 * @param ctx Context
 * @param parent_diameter Parent dendrite diameter
 * @param spine_diameter Spine neck diameter
 * @param spine_position Spine position (relative to parent)
 * @param result Output: spine surface geometry
 * @return 0 on success, -1 on error
 */
int surface_compute_spine_geometry(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    float spine_diameter,
    const surface_vec3_t* spine_position,
    spine_surface_geometry_t* result
);

/**
 * @brief Predict if spine should be orthogonal sprout
 *
 * Returns true if rho < rho_threshold, indicating
 * optimal angle is 90 degrees (orthogonal sprout).
 *
 * @param ctx Context
 * @param parent_diameter Parent diameter
 * @param spine_diameter Spine diameter
 * @param is_sprout Output: true if sprout
 * @return 0 on success, -1 on error
 */
int surface_predict_spine_sprout(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    float spine_diameter,
    bool* is_sprout
);

/**
 * @brief Compute surface geometry for axon branch
 *
 * @param ctx Context
 * @param parent_diameter Parent axon diameter
 * @param child_diameters Child branch diameters
 * @param num_children Number of children
 * @param result Output: branch surface geometry
 * @return 0 on success, -1 on error
 */
int surface_compute_axon_branch_geometry(
    surface_geometry_ctx_t* ctx,
    float parent_diameter,
    const float* child_diameters,
    uint32_t num_children,
    axon_branch_surface_geometry_t* result
);

//=============================================================================
// SPINE SURFACE CACHE
//=============================================================================

/**
 * @brief Create spine surface cache
 *
 * @param capacity Maximum cached entries
 * @return Created cache or NULL on failure
 */
spine_surface_cache_t* surface_spine_cache_create(uint32_t capacity);

/**
 * @brief Destroy spine surface cache
 *
 * @param cache Cache to destroy
 */
void surface_spine_cache_destroy(spine_surface_cache_t* cache);

/**
 * @brief Get cached geometry for spine
 *
 * @param cache Cache to query
 * @param spine_id Spine identifier
 * @param geometry Output: cached geometry (if found)
 * @return 0 if found, -1 if not cached
 */
int surface_spine_cache_get(
    spine_surface_cache_t* cache,
    uint32_t spine_id,
    spine_surface_geometry_t* geometry
);

/**
 * @brief Store geometry in spine cache
 *
 * @param cache Cache to update
 * @param spine_id Spine identifier
 * @param geometry Geometry to cache
 * @return 0 on success, -1 on error
 */
int surface_spine_cache_put(
    spine_surface_cache_t* cache,
    uint32_t spine_id,
    const spine_surface_geometry_t* geometry
);

/**
 * @brief Invalidate cache entry
 *
 * @param cache Cache to update
 * @param spine_id Spine to invalidate
 * @return 0 on success, -1 on error
 */
int surface_spine_cache_invalidate(
    spine_surface_cache_t* cache,
    uint32_t spine_id
);

/**
 * @brief Clear entire cache
 *
 * @param cache Cache to clear
 * @return 0 on success, -1 on error
 */
int surface_spine_cache_clear(spine_surface_cache_t* cache);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get surface geometry statistics
 *
 * @param ctx Context
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int surface_geometry_get_stats(
    const surface_geometry_ctx_t* ctx,
    surface_geometry_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param ctx Context
 * @return 0 on success, -1 on error
 */
int surface_geometry_reset_stats(surface_geometry_ctx_t* ctx);

//=============================================================================
// REGION STATISTICS
//=============================================================================

/**
 * @brief Compute region-level statistics
 *
 * @param branch_points Branch points in region
 * @param num_points Number of points
 * @param region_id Region identifier
 * @param stats Output: region statistics
 * @return 0 on success, -1 on error
 */
int surface_compute_region_stats(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    uint32_t region_id,
    surface_region_stats_t* stats
);

//=============================================================================
// LAYER COMMUNICATION
//=============================================================================

/**
 * @brief Send data downstream (Core -> Neural -> Region -> Cognitive)
 *
 * @param ctx Context
 * @param source_layer Source layer (1-4)
 * @param data Data to send
 * @param data_size Data size in bytes
 * @return 0 on success, -1 on error
 */
int surface_layer_send_downstream(
    surface_geometry_ctx_t* ctx,
    uint32_t source_layer,
    const void* data,
    size_t data_size
);

/**
 * @brief Send feedback upstream (Cognitive -> Region -> Neural -> Core)
 *
 * @param ctx Context
 * @param source_layer Source layer (4-1)
 * @param feedback Feedback data
 * @param feedback_size Feedback size in bytes
 * @return 0 on success, -1 on error
 */
int surface_layer_send_upstream(
    surface_geometry_ctx_t* ctx,
    uint32_t source_layer,
    const void* feedback,
    size_t feedback_size
);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get human-readable name for regime
 */
const char* surface_regime_name(surface_regime_t regime);

/**
 * @brief Get human-readable name for branch type
 */
const char* surface_branch_type_name(surface_branch_type_t type);

/**
 * @brief Get human-readable name for validation status
 */
const char* surface_validation_status_name(surface_validation_status_t status);

/**
 * @brief Get error string for surface error code
 */
const char* surface_error_string(surface_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_GEOMETRY_H */
