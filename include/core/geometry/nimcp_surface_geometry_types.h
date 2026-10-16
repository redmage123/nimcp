/**
 * @file nimcp_surface_geometry_types.h
 * @brief Surface Geometry Optimization Types - Based on Meng et al. Nature 2026
 *
 * WHAT: Type definitions for surface optimization in physical networks
 * WHY:  Traditional wire minimization (Steiner) fails to predict real network
 *       morphology. Surface minimization via 2D manifolds explains:
 *       - Trifurcations (k=4) emerge at chi >= 0.83
 *       - Orthogonal sprouting for rho < rho_threshold (~0.6)
 *       - 98% of neuronal sprouts end at synapses
 * HOW:  Maps network geometry to string theory worldsheets (Nambu-Goto action)
 *
 * REFERENCE: "Surface optimization governs the local design of physical networks"
 *            Meng, Piazza, Both, Barzel & Barabasi, Nature 649:315-322 (2026)
 *
 * NIMCP STANDARDS:
 * - Uses nimcp_malloc/nimcp_free for memory
 * - Returns 0 for success, -1 for errors
 * - All functions < 50 lines
 * - Guard clauses for NULL checks
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_GEOMETRY_TYPES_H
#define NIMCP_SURFACE_GEOMETRY_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>  /* For M_PI, fabsf */
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MATHEMATICAL CONSTANTS
//=============================================================================

/** Pi constant - define if not available from math.h */

/** Pi as single-precision float for computation */
#define SURFACE_PI_F ((float)M_PI)

//=============================================================================
// CONSTANTS - From Paper Predictions
//=============================================================================

/** Chi threshold for bifurcation -> trifurcation transition (Fig. 3g) */
#define SURFACE_CHI_TRIFURCATION_THRESHOLD 0.83f

/** Default rho threshold for sprouting -> branching transition (Fig. 4g) */
#define SURFACE_RHO_THRESHOLD_DEFAULT 0.6f

/** Human neuron rho threshold (from paper data) */
#define SURFACE_RHO_THRESHOLD_HUMAN_NEURON 0.56f

/** Fruit fly neuron rho threshold */
#define SURFACE_RHO_THRESHOLD_FRUIT_FLY 0.52f

/** Blood vessel rho threshold */
#define SURFACE_RHO_THRESHOLD_BLOOD_VESSEL 0.83f

/** Expected sprout-synapse ratio (98% from paper) */
#define SURFACE_SPROUT_SYNAPSE_RATIO 0.98f

/** Expected network length overhead vs Steiner (~25% longer) */
#define SURFACE_STEINER_LENGTH_OVERHEAD 1.25f

/** Steiner angle prediction (2*pi/3 radians = 120 degrees) */
#define SURFACE_STEINER_ANGLE (2.0f * SURFACE_PI_F / 3.0f)

/** Orthogonal sprout angle (pi/2 radians = 90 degrees) */
#define SURFACE_ORTHOGONAL_SPROUT_ANGLE (SURFACE_PI_F / 2.0f)

//=============================================================================
// NUMERICAL STABILITY CONSTANTS
//=============================================================================

/** Minimum determinant for metric tensor (avoid singularities) - absolute threshold */
#define SURFACE_MIN_DETERMINANT 1e-10f

/**
 * @brief Scale-relative determinant threshold for metric tensor validation
 *
 * For surfaces of varying scale, using an absolute threshold can cause:
 * - False negatives on large surfaces (det >> 1e-10 but still degenerate)
 * - False positives on small surfaces (det ~ 1e-10 but valid)
 *
 * Use: det > SURFACE_MIN_DETERMINANT_SCALED(mean_radius_squared)
 * where mean_radius_squared is the characteristic surface area scale.
 */
#define SURFACE_MIN_DETERMINANT_SCALED(mean_radius_sq) \
    (SURFACE_MIN_DETERMINANT * (mean_radius_sq))

/** Maximum optimization iterations */
#define SURFACE_MAX_ITERATIONS 1000

/** Convergence tolerance */
#define SURFACE_CONVERGENCE_TOL 1e-6f

/** Minimum circumference (avoid degenerate links) */
#define SURFACE_MIN_CIRCUMFERENCE 1e-6f

/** Maximum degree for branch points */
#define SURFACE_MAX_BRANCH_DEGREE 8

/** Maximum charts per manifold */
#define SURFACE_MAX_CHARTS 64

/** Maximum branch points per optimization */
#define SURFACE_MAX_BRANCH_POINTS 256

/** Default spine cache capacity */
#define SURFACE_SPINE_CACHE_DEFAULT_CAPACITY 256

//=============================================================================
// ERROR CODES
//=============================================================================

typedef enum {
    SURFACE_OK = 0,
    SURFACE_ERROR_NULL = -1,
    SURFACE_ERROR_MEMORY = -2,
    SURFACE_ERROR_INVALID_PARAM = -3,
    SURFACE_ERROR_DEGENERATE_METRIC = -4,
    SURFACE_ERROR_CONVERGENCE = -5,
    SURFACE_ERROR_MAX_ITERATIONS = -6,
    SURFACE_ERROR_INVALID_TOPOLOGY = -7,
    SURFACE_ERROR_CONSTRAINT_VIOLATION = -8,
    SURFACE_ERROR_NOT_INITIALIZED = -9,
    SURFACE_ERROR_ALREADY_INITIALIZED = -10
} surface_error_t;

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Branch regime classification (from paper Fig. 4)
 *
 * WHAT: Determines branching behavior based on diameter ratio rho
 * WHY:  Different regimes have distinct optimal angles:
 *       - SPROUTING (rho < rho_th): 90-degree orthogonal branches
 *       - BRANCHING (rho > rho_th): Variable angles ~linear in rho
 *       - STEINER (chi -> 0): Classical 120-degree symmetric
 */
typedef enum {
    SURFACE_REGIME_SPROUTING = 0,   /**< rho < rho_th: orthogonal sprouts (90 deg) */
    SURFACE_REGIME_BRANCHING = 1,   /**< rho > rho_th: variable angle branching */
    SURFACE_REGIME_STEINER = 2,     /**< chi -> 0: classical Steiner (120 deg) */
    SURFACE_REGIME_COUNT
} surface_regime_t;

/**
 * @brief Branch node degree classification (from paper Fig. 1d)
 *
 * WHAT: Number of links meeting at a branch point
 * WHY:  Steiner predicts only bifurcations (k=3), but surface minimization
 *       predicts stable trifurcations (k=4) at chi >= 0.83
 */
typedef enum {
    SURFACE_BRANCH_BIFURCATION = 0,     /**< k=3: two-way split (Steiner) */
    SURFACE_BRANCH_TRIFURCATION = 1,    /**< k=4: three-way split (surface) */
    SURFACE_BRANCH_QUADFURCATION = 2,   /**< k=5: four-way split (rare) */
    SURFACE_BRANCH_HIGHER = 3,          /**< k>5: higher-order (very rare) */
    SURFACE_BRANCH_TYPE_COUNT
} surface_branch_type_t;

/**
 * @brief Validation status for geometry parameters
 */
typedef enum {
    SURFACE_VALIDATION_VALID = 0,
    SURFACE_VALIDATION_CHI_OUT_OF_RANGE,
    SURFACE_VALIDATION_RHO_OUT_OF_RANGE,
    SURFACE_VALIDATION_ANGLE_VIOLATION,
    SURFACE_VALIDATION_TRIFURCATION_INVALID,
    SURFACE_VALIDATION_TOPOLOGY_ERROR,
    SURFACE_VALIDATION_MATERIAL_OVERFLOW,
    SURFACE_VALIDATION_COUNT
} surface_validation_status_t;

/**
 * @brief Optimization algorithm selection
 */
typedef enum {
    SURFACE_OPT_GRADIENT_DESCENT = 0,   /**< Simple gradient descent */
    SURFACE_OPT_CONJUGATE_GRADIENT,     /**< Conjugate gradient method */
    SURFACE_OPT_MONTE_CARLO,            /**< Monte Carlo integration */
    SURFACE_OPT_QUANTUM_ANNEALING,      /**< Quantum-inspired annealing */
    SURFACE_OPT_QMCTS,                  /**< Quantum MCTS for topology */
    SURFACE_OPT_COUNT
} surface_optimization_method_t;

/**
 * @brief Chart/sleeve type for manifold construction
 */
typedef enum {
    SURFACE_CHART_CYLINDRICAL = 0,      /**< Simple cylinder (Strebel) */
    SURFACE_CHART_CONICAL,              /**< Conical (varying diameter) */
    SURFACE_CHART_PANTS,                /**< Pants decomposition (bifurcation) */
    SURFACE_CHART_TRIPLE_PANTS,         /**< Triple pants (trifurcation) */
    SURFACE_CHART_COUNT
} surface_chart_type_t;

//=============================================================================
// CORE GEOMETRY STRUCTURES
//=============================================================================

/**
 * @brief Surface geometry parameters for a branch point
 *
 * WHAT: Key dimensionless parameters from the paper
 * WHY:  These determine optimal branching geometry:
 *       - chi = w/r: thickness-to-distance ratio -> bifurcation vs trifurcation
 *       - rho = w'/w: daughter-to-parent diameter -> sprouting vs branching
 *       - lambda = l/w: separation parameter -> degree of bifurcation merging
 * HOW:  Computed from local geometry, used to predict optimal angles
 */
typedef struct surface_geometry_params_struct {
    /* Primary parameters (dimensionless, from paper) */
    float chi;                  /**< chi = w/r: circumference / distance */
    float rho;                  /**< rho = w'/w: branch thickness ratio */
    float rho_threshold;        /**< Sprouting/branching transition (~0.6) */
    float lambda;               /**< lambda = l/w: separation parameter */

    /* Angular measurements */
    float solid_angle;          /**< Omega: solid angle (2pi = planar) */
    float steering_angle;       /**< Omega_1->2: branching angle */
    float branch_angles[SURFACE_MAX_BRANCH_DEGREE]; /**< Pairwise angles theta */
    uint32_t num_angles;        /**< Number of valid angles */

    /* Classification */
    surface_regime_t regime;            /**< SPROUTING, BRANCHING, or STEINER */
    surface_branch_type_t branch_type;  /**< BIFURCATION or TRIFURCATION */

    /* Physical dimensions (for reconstruction) */
    float circumference;        /**< w: link circumference (minimum) */
    float distance;             /**< r: characteristic distance */
    float link_length;          /**< l: intermediate link length */

    /* Computed flags */
    bool is_planar;             /**< Omega ~= 2*pi (within tolerance) */
    bool is_symmetric;          /**< All angles ~= 2*pi/3 */
    bool is_optimal;            /**< Within tolerance of predictions */
} surface_geometry_params_t;

/**
 * @brief 3D position vector
 */
typedef struct surface_vec3_struct {
    float x, y, z;
} surface_vec3_t;

/**
 * @brief Branch point descriptor
 *
 * WHAT: Complete description of a network branch point
 * WHY:  Stores position, connectivity, and computed geometry
 * HOW:  Used as input/output for optimization algorithms
 */
typedef struct surface_branch_point_struct {
    uint32_t id;                            /**< Unique identifier */
    surface_vec3_t position;                /**< 3D position */
    uint32_t degree;                        /**< Number of branches (k) */

    /* Connected links */
    uint32_t link_ids[SURFACE_MAX_BRANCH_DEGREE];   /**< Connected link IDs */
    float link_diameters[SURFACE_MAX_BRANCH_DEGREE]; /**< Link diameters (w) */
    surface_vec3_t link_directions[SURFACE_MAX_BRANCH_DEGREE]; /**< Link directions */

    /* Geometry parameters */
    surface_geometry_params_t params;       /**< Computed parameters */

    /* Functional annotations */
    bool is_terminal;           /**< True if endpoint (leaf node) */
    bool is_synapse_endpoint;   /**< For sprouts: ends at synapse */
    bool is_sprout;             /**< Orthogonal sprout (rho < rho_th) */

    /* Material cost */
    float local_surface_area;   /**< Local contribution to S_M(G) */
    float local_wire_length;    /**< Local Steiner length */

    /* Validation */
    surface_validation_status_t validation_status;
} surface_branch_point_t;

//=============================================================================
// MANIFOLD STRUCTURES (String Theory Mapping)
//=============================================================================

/**
 * @brief 2x2 metric tensor for surface parametrization
 *
 * WHAT: The induced metric gamma_ab on the 2D surface
 * WHY:  Surface area element is sqrt(det(gamma)) d^2 sigma
 * HOW:  gamma_ab = (dX/d_sigma^a) . (dX/d_sigma^b)
 */
typedef struct surface_metric_tensor_struct {
    float gamma[2][2];          /**< 2x2 metric tensor gamma_ab */
    float determinant;          /**< det(gamma) - cached */
    float inverse[2][2];        /**< Inverse metric gamma^ab - cached */
    bool is_valid;              /**< True if det > SURFACE_MIN_DETERMINANT */
} surface_metric_tensor_t;

/**
 * @brief Chart (local coordinate patch) for manifold
 *
 * WHAT: Local coordinate system for a link's surface
 * WHY:  Manifold = union of charts with smooth transitions
 * HOW:  sigma = (sigma^0, sigma^1) = (longitudinal, azimuthal)
 */
typedef struct surface_chart_struct {
    uint32_t id;                        /**< Chart identifier */
    uint32_t link_id;                   /**< Associated link ID */
    surface_chart_type_t type;          /**< Chart geometry type */

    /* Coordinate bounds */
    float sigma0_min, sigma0_max;       /**< Longitudinal range */
    float sigma1_min, sigma1_max;       /**< Azimuthal range (0 to 2*pi) */

    /* Metric tensor (computed at chart center) */
    surface_metric_tensor_t metric;     /**< Induced metric */

    /* Constraint */
    float circumference;                /**< Minimum circumference w */

    /* Connection to other charts */
    uint32_t boundary_chart_ids[4];     /**< Adjacent charts at boundaries */
    uint32_t num_boundaries;            /**< Number of chart boundaries */
} surface_chart_t;

/**
 * @brief Feynman diagram node (pants decomposition vertex)
 *
 * WHAT: Vertex in the worldsheet Feynman diagram
 * WHY:  Maps surface minimization to string theory formalism
 * HOW:  Each vertex represents a pants (bifurcation) or higher junction
 */
typedef struct surface_feynman_vertex_struct {
    uint32_t id;
    uint32_t branch_point_id;           /**< Corresponding branch point */
    uint32_t valence;                   /**< Number of legs (k) */
    uint32_t leg_ids[SURFACE_MAX_BRANCH_DEGREE]; /**< Connected propagators */
    float moduli[SURFACE_MAX_BRANCH_DEGREE];     /**< Strebel moduli */
} surface_feynman_vertex_t;

/**
 * @brief Feynman diagram (worldsheet decomposition)
 */
typedef struct surface_feynman_diagram_struct {
    surface_feynman_vertex_t* vertices;
    uint32_t num_vertices;
    uint32_t* propagators;              /**< Pairs of vertex connections */
    uint32_t num_propagators;
    float total_action;                 /**< Nambu-Goto action S */
} surface_feynman_diagram_t;

//=============================================================================
// OPTIMIZATION STRUCTURES
//=============================================================================

/**
 * @brief Surface optimization result
 *
 * WHAT: Output of surface minimization algorithm
 * WHY:  Provides optimized geometry and comparison metrics
 */
typedef struct surface_optimization_result_struct {
    /* Area metrics */
    float surface_area;         /**< S_M(G): total surface area */
    float wire_length;          /**< Steiner-equivalent wire length */
    float efficiency_ratio;     /**< surface_area / wire_length */
    float material_cost;        /**< Estimated material investment */

    /* Comparison to predictions */
    float steiner_area;         /**< Predicted Steiner area */
    float area_overhead;        /**< (surface_area - steiner_area) / steiner_area */

    /* Optimized branch points */
    surface_branch_point_t* branch_points;
    uint32_t num_branch_points;

    /* Convergence info */
    uint32_t iterations;        /**< Number of iterations to converge */
    float final_residual;       /**< Final optimization residual */
    bool converged;             /**< True if converged within tolerance */
    bool diverged;              /**< True if optimization diverged (area increasing) */

    /* Statistics */
    uint32_t num_bifurcations;  /**< k=3 nodes */
    uint32_t num_trifurcations; /**< k=4 nodes */
    uint32_t num_sprouts;       /**< Orthogonal sprouts */
    uint32_t num_synapse_sprouts; /**< Sprouts ending at synapses */
    float sprout_synapse_ratio; /**< Should be ~0.98 */
} surface_optimization_result_t;

/**
 * @brief Validation result for geometry parameters
 */
typedef struct surface_validation_result_struct {
    surface_validation_status_t status;
    bool is_valid;

    /* Specific checks */
    bool chi_valid;
    bool rho_valid;
    bool angles_valid;
    bool topology_valid;
    bool material_valid;

    /* Deviation from predictions */
    float chi_deviation;        /**< |chi - chi_predicted| */
    float angle_deviation;      /**< Max angle deviation from optimal */
    float rho_deviation;        /**< |rho - rho_threshold| */

    /* Message for diagnostics */
    char message[256];
} surface_validation_result_t;

//=============================================================================
// CONFIGURATION STRUCTURES
//=============================================================================

/**
 * @brief Surface geometry module configuration
 */
typedef struct surface_geometry_config_struct {
    /* Thresholds (can override paper defaults) */
    float chi_trifurcation_threshold;   /**< Default: 0.83 */
    float rho_threshold;                /**< Default: 0.6 */
    float angle_tolerance;              /**< Tolerance for angle validation */
    float convergence_tolerance;        /**< Optimization convergence */

    /* Optimization settings */
    surface_optimization_method_t method;
    uint32_t max_iterations;
    uint32_t monte_carlo_samples;       /**< For MC integration */

    /* Material constraints */
    float min_circumference;            /**< Minimum link circumference */
    float material_budget;              /**< Total surface area budget */
    bool enforce_budget;                /**< Strict budget enforcement */

    /* Metabolic modulation */
    bool enable_metabolic;              /**< ATP affects optimization */
    float metabolic_quality_min;        /**< Min quality factor */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable async messaging */
    uint32_t bio_async_buffer_size;     /**< Message buffer size */

    /* Quantum integration */
    bool enable_quantum;                /**< Enable QMC/QMCTS */
    uint32_t quantum_samples;           /**< QMC sample count */

    /* Debug/validation */
    bool validate_predictions;          /**< Check against paper */
    bool verbose;                       /**< Detailed logging */
} surface_geometry_config_t;

/**
 * @brief Statistics for surface geometry operations
 */
typedef struct surface_geometry_stats_struct {
    /* Counts */
    uint64_t total_optimizations;
    uint64_t successful_optimizations;
    uint64_t failed_optimizations;

    /* Branch type distribution */
    uint64_t bifurcations_predicted;
    uint64_t trifurcations_predicted;
    uint64_t sprouts_predicted;
    uint64_t synapse_sprouts;

    /* Performance */
    float avg_iterations;
    float avg_area_overhead;
    float avg_sprout_synapse_ratio;

    /* Validation */
    uint64_t validation_passes;
    uint64_t validation_failures;
} surface_geometry_stats_t;

//=============================================================================
// SPINE/AXON INTEGRATION STRUCTURES
//=============================================================================

/**
 * @brief Surface geometry for dendritic spines (lazy-computed)
 *
 * WHAT: Surface optimization parameters for spine formation
 * WHY:  Paper predicts 98% of orthogonal sprouts end at synapses
 * HOW:  Computed on-demand, cached by spine_id
 */
typedef struct spine_surface_geometry_struct {
    uint32_t spine_id;                  /**< Associated spine */
    surface_geometry_params_t params;   /**< Computed parameters */
    float optimal_angle;                /**< Surface-minimizing angle */
    bool is_sprout;                     /**< rho < rho_threshold */
    bool ends_at_synapse;               /**< Functional annotation */
    float material_cost;                /**< Surface area contribution */
    uint64_t compute_time_us;           /**< Computation timestamp */
    bool is_cached;                     /**< True if computed */
} spine_surface_geometry_t;

/**
 * @brief Surface geometry cache for spines
 *
 * WHAT: Lazy computation cache for spine surface geometry
 * WHY:  Avoid computing ~32 bytes per spine * 2000 spines = 64KB overhead
 * HOW:  Hash map from spine_id to cached geometry
 */
typedef struct spine_surface_cache_struct {
    spine_surface_geometry_t** cache;   /**< Array of pointers (hash table) */
    uint32_t* spine_ids;                /**< Spine IDs in cache */
    uint32_t num_cached;                /**< Number of cached entries */
    uint32_t capacity;                  /**< Cache capacity */
    bool dirty;                         /**< Needs recomputation */
    uint64_t last_update;               /**< Timestamp of last update */
} spine_surface_cache_t;

/**
 * @brief Surface geometry for axon branch points
 */
typedef struct axon_branch_surface_geometry_struct {
    uint32_t segment_id;                /**< Axon segment ID */
    surface_geometry_params_t params;   /**< Computed parameters */
    surface_branch_type_t branch_type;  /**< Predicted branch type */
    float branch_angles[4];             /**< Up to k=4 angles */
    uint32_t degree;                    /**< Branch degree k */
    float material_cost;                /**< Surface area contribution */
} axon_branch_surface_geometry_t;

//=============================================================================
// LAYER COMMUNICATION STRUCTURES
//=============================================================================

/**
 * @brief Region-level surface geometry statistics
 */
typedef struct surface_region_stats_struct {
    uint32_t region_id;
    uint32_t num_branch_points;
    uint32_t num_bifurcations;
    uint32_t num_trifurcations;
    uint32_t num_sprouts;
    uint32_t num_synapse_sprouts;
    float total_surface_area;
    float mean_chi;
    float mean_rho;
    float sprout_synapse_ratio;
} surface_region_stats_t;

/**
 * @brief Inter-layer communication packet
 */
typedef struct surface_layer_comm_struct {
    /* Layer 1 -> Layer 2: Core -> Neural structures */
    surface_geometry_params_t computed_params;

    /* Layer 2 -> Layer 3: Neural -> Brain regions */
    spine_surface_geometry_t* spine_updates;
    uint32_t num_spine_updates;
    axon_branch_surface_geometry_t* axon_updates;
    uint32_t num_axon_updates;

    /* Layer 3 -> Layer 4: Regions -> Cognitive */
    surface_region_stats_t region_stats;

    /* Feedback: Layer 4 -> Layer 1 */
    float material_budget_adjustment;
    float rho_threshold_adjustment;
    bool request_recompute;
} surface_layer_comm_t;

//=============================================================================
// UTILITY MACROS
//=============================================================================

/** Check if chi indicates trifurcation regime */
#define SURFACE_IS_TRIFURCATION_REGIME(chi) \
    ((chi) >= SURFACE_CHI_TRIFURCATION_THRESHOLD)

/** Check if rho indicates sprouting regime */
#define SURFACE_IS_SPROUTING_REGIME(rho, rho_th) \
    ((rho) < (rho_th))

/** Check if solid angle indicates planarity (within 10%) */
#define SURFACE_IS_PLANAR(omega) \
    (fabsf((omega) - 2.0f * SURFACE_PI_F) < 0.2f * SURFACE_PI_F)

/** Check if angles are Steiner-symmetric (within 10%) */
#define SURFACE_IS_STEINER_SYMMETRIC(theta) \
    (fabsf((theta) - SURFACE_STEINER_ANGLE) < 0.1f * SURFACE_PI_F)

/** Clamp value to range */
#define SURFACE_CLAMP(x, lo, hi) \
    (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_GEOMETRY_TYPES_H */
