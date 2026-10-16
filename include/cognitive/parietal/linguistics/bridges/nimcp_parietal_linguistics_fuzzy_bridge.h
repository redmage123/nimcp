/**
 * @file nimcp_parietal_linguistics_fuzzy_bridge.h
 * @brief Fuzzy Logic Integration Bridge for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 *
 * WHAT: Integrates NIMCP fuzzy logic system with parietal linguistics modules
 *       for spatial preposition semantics, hedges, and graded membership
 *
 * WHY:  Spatial prepositions like "near", "far", "left" have inherently fuzzy
 *       semantics - there's no crisp boundary between "near" and "far". Fuzzy
 *       membership functions naturally model this continuous gradation.
 *
 * BIOLOGICAL BASIS:
 * - Parietal neurons encode spatial relations with graded activation
 * - Weber-Fechner law: perceived distances follow logarithmic scaling
 * - Angular gyrus integrates spatial and linguistic representations
 *
 * MESH INTEGRATION:
 * - Implements linguistics_mesh_handler_t for mesh participation
 * - Contributes fuzzy spatial beliefs with precision weighting
 * - Participates in FEP convergence for spatial language understanding
 *
 * FUZZY PREPOSITION SEMANTICS:
 * - "near" → Gaussian MF centered at 0, σ varies with context
 * - "far" → Z-shaped MF transitioning at context-dependent distance
 * - "very near" → μ²(x) concentration (FUZZY_HEDGE_VERY)
 * - "somewhat far" → √μ(x) dilation (FUZZY_HEDGE_SOMEWHAT)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_FUZZY_BRIDGE_H
#define NIMCP_PARIETAL_LINGUISTICS_FUZZY_BRIDGE_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID for fuzzy bridge */
#define BIO_MODULE_LING_FUZZY_BRIDGE     0x8400

/** Maximum spatial prepositions supported */
#define LING_FUZZY_MAX_PREPOSITIONS      64

/** Maximum hedges per preposition */
#define LING_FUZZY_MAX_HEDGES            8

/** Default precision for fuzzy bridge */
#define LING_FUZZY_DEFAULT_PRECISION     0.75f

/** Precision floor to prevent division by zero */
#define LING_FUZZY_PRECISION_FLOOR       0.01f

/** Precision ceiling for highly certain fuzzy evaluations */
#define LING_FUZZY_PRECISION_CEILING     0.99f

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

#define LING_FUZZY_ERR_OK                0
#define LING_FUZZY_ERR_NULL              -1
#define LING_FUZZY_ERR_INVALID_PREP      -2
#define LING_FUZZY_ERR_INVALID_HEDGE     -3
#define LING_FUZZY_ERR_MF_CREATE         -4
#define LING_FUZZY_ERR_REGISTRY_FULL     -5
#define LING_FUZZY_ERR_NOT_INIT          -6
#define LING_FUZZY_ERR_MESH_REGISTER     -7
#define LING_FUZZY_ERR_BBB_REJECTED      -8

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for fuzzy bridge */
typedef struct ling_fuzzy_bridge ling_fuzzy_bridge_t;

/**
 * @brief Spatial dimension for fuzzy evaluation
 */
typedef enum {
    LING_FUZZY_DIM_DISTANCE,            /**< Distance in meters */
    LING_FUZZY_DIM_ANGLE,               /**< Angle in radians */
    LING_FUZZY_DIM_HEIGHT,              /**< Vertical distance */
    LING_FUZZY_DIM_PROPORTION,          /**< Proportion [0,1] */
    LING_FUZZY_DIM_COUNT
} ling_fuzzy_dimension_t;

/**
 * @brief Fuzzy preposition definition
 */
typedef struct {
    spatial_preposition_t preposition;  /**< Preposition type */
    char name[32];                      /**< Human-readable name */

    /* Membership functions per dimension */
    fuzzy_mf_t distance_mf;             /**< Distance MF (for proximity preps) */
    fuzzy_mf_t angle_mf;                /**< Angle MF (for directional preps) */
    fuzzy_mf_t height_mf;               /**< Height MF (for vertical preps) */

    /* Context modulation */
    float context_scale;                /**< Scale factor from context */
    float salience;                     /**< Preposition salience [0,1] */

    /* Flags */
    bool has_distance;                  /**< Uses distance dimension */
    bool has_angle;                     /**< Uses angle dimension */
    bool has_height;                    /**< Uses height dimension */
    bool is_symmetric;                  /**< Symmetric relation (e.g., "near") */
} ling_fuzzy_preposition_t;

/**
 * @brief Fuzzy evaluation result
 */
typedef struct {
    float membership;                   /**< Membership degree [0,1] */
    float confidence;                   /**< Evaluation confidence [0,1] */
    float precision;                    /**< Precision for mesh (1/σ²) */

    spatial_preposition_t preposition;  /**< Evaluated preposition */
    fuzzy_hedge_t hedge_applied;        /**< Applied hedge */

    /* Component memberships */
    float distance_membership;          /**< Distance component */
    float angle_membership;             /**< Angle component */
    float height_membership;            /**< Height component */

    /* Debug info */
    float crisp_distance;               /**< Input distance */
    float crisp_angle;                  /**< Input angle */
    float crisp_height;                 /**< Input height */
} ling_fuzzy_result_t;

/**
 * @brief Fuzzy bridge configuration
 */
typedef struct {
    /* Default MF parameters */
    float default_near_sigma;           /**< σ for "near" Gaussian (default: 1.0m) */
    float default_far_threshold;        /**< Threshold for "far" (default: 5.0m) */
    float default_angle_sigma;          /**< σ for angular MFs (default: π/6) */

    /* Precision settings */
    float base_precision;               /**< Base precision (default: 0.75) */
    float precision_decay;              /**< Decay with uncertainty (default: 0.9) */

    /* Mesh integration */
    bool enable_mesh;                   /**< Register with mesh (default: true) */
    float mesh_learning_rate;           /**< FEP update rate (default: 0.1) */

    /* Infrastructure */
    bool enable_bbb;                    /**< Enable BBB validation (default: true) */
    bool enable_health;                 /**< Enable health monitoring (default: true) */
    bool enable_logging;                /**< Enable structured logging (default: true) */

    /* Context modulation */
    float indoor_scale;                 /**< Scale for indoor context (default: 0.5) */
    float outdoor_scale;                /**< Scale for outdoor context (default: 2.0) */
} ling_fuzzy_bridge_config_t;

/**
 * @brief Fuzzy bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;         /**< Total fuzzy evaluations */
    uint64_t hedge_applications;        /**< Hedge applications */
    uint64_t mesh_contributions;        /**< Mesh belief contributions */
    uint64_t mesh_updates;              /**< FEP belief updates */

    float avg_membership;               /**< Average membership degree */
    float avg_precision;                /**< Average precision */
    float avg_latency_us;               /**< Average latency in μs */

    uint64_t bbb_rejections;            /**< BBB validation failures */
    uint64_t exceptions;                /**< Exceptions thrown */
} ling_fuzzy_bridge_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default fuzzy bridge configuration
 *
 * @return Configuration with sensible defaults
 */
ling_fuzzy_bridge_config_t ling_fuzzy_bridge_default_config(void);

/**
 * @brief Create fuzzy bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
ling_fuzzy_bridge_t* ling_fuzzy_bridge_create(
    const ling_fuzzy_bridge_config_t* config
);

/**
 * @brief Destroy fuzzy bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void ling_fuzzy_bridge_destroy(ling_fuzzy_bridge_t* bridge);

/**
 * @brief Register with linguistics mesh coordinator
 *
 * @param bridge Fuzzy bridge
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int ling_fuzzy_bridge_register_mesh(
    ling_fuzzy_bridge_t* bridge,
    linguistics_mesh_t* mesh
);

/* ============================================================================
 * FUZZY EVALUATION API
 * ============================================================================ */

/**
 * @brief Evaluate spatial preposition membership
 *
 * Computes fuzzy membership for a preposition given crisp spatial values.
 *
 * @param bridge Fuzzy bridge
 * @param preposition Preposition to evaluate
 * @param distance Distance in meters (or NAN if not applicable)
 * @param angle Angle in radians (or NAN if not applicable)
 * @param height Height in meters (or NAN if not applicable)
 * @param result Output evaluation result
 * @return 0 on success
 */
int ling_fuzzy_evaluate_preposition(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    float distance,
    float angle,
    float height,
    ling_fuzzy_result_t* result
);

/**
 * @brief Apply linguistic hedge to membership
 *
 * @param bridge Fuzzy bridge
 * @param membership Input membership [0,1]
 * @param hedge Hedge to apply
 * @param result Output modified membership
 * @return 0 on success
 */
int ling_fuzzy_apply_hedge(
    ling_fuzzy_bridge_t* bridge,
    float membership,
    fuzzy_hedge_t hedge,
    float* result
);

/**
 * @brief Evaluate hedged preposition ("very near", "somewhat left")
 *
 * @param bridge Fuzzy bridge
 * @param preposition Base preposition
 * @param hedge Linguistic hedge
 * @param distance Distance in meters
 * @param angle Angle in radians
 * @param height Height in meters
 * @param result Output evaluation result
 * @return 0 on success
 */
int ling_fuzzy_evaluate_hedged(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    fuzzy_hedge_t hedge,
    float distance,
    float angle,
    float height,
    ling_fuzzy_result_t* result
);

/**
 * @brief Find best matching preposition for given spatial values
 *
 * @param bridge Fuzzy bridge
 * @param distance Distance in meters
 * @param angle Angle in radians
 * @param height Height in meters
 * @param preposition Output best matching preposition
 * @param membership Output membership degree
 * @return 0 on success
 */
int ling_fuzzy_select_preposition(
    ling_fuzzy_bridge_t* bridge,
    float distance,
    float angle,
    float height,
    spatial_preposition_t* preposition,
    float* membership
);

/* ============================================================================
 * MEMBERSHIP FUNCTION MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Get membership function for preposition
 *
 * @param bridge Fuzzy bridge
 * @param preposition Preposition
 * @param dimension Which dimension's MF to get
 * @param mf Output membership function
 * @return 0 on success
 */
int ling_fuzzy_get_preposition_mf(
    const ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    ling_fuzzy_dimension_t dimension,
    fuzzy_mf_t* mf
);

/**
 * @brief Set membership function for preposition
 *
 * @param bridge Fuzzy bridge
 * @param preposition Preposition
 * @param dimension Which dimension
 * @param mf New membership function
 * @return 0 on success
 */
int ling_fuzzy_set_preposition_mf(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    ling_fuzzy_dimension_t dimension,
    const fuzzy_mf_t* mf
);

/**
 * @brief Set context scale for all prepositions
 *
 * @param bridge Fuzzy bridge
 * @param scale Scale factor (1.0 = default, <1.0 = indoor, >1.0 = outdoor)
 * @return 0 on success
 */
int ling_fuzzy_set_context_scale(
    ling_fuzzy_bridge_t* bridge,
    float scale
);

/* ============================================================================
 * MESH HANDLER INTERFACE
 * ============================================================================ */

/**
 * @brief Mesh process callback - produce fuzzy belief for request
 *
 * Implements linguistics_mesh_handler_t::process
 *
 * @param ctx Bridge context
 * @param request Linguistics request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int ling_fuzzy_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Mesh update callback - FEP belief update from neighbors
 *
 * Implements linguistics_mesh_handler_t::update
 * Update rule: μ' = μ - lr × Π × ε
 *
 * @param ctx Bridge context
 * @param neighbors Neighbor beliefs
 * @param count Number of neighbors
 * @param updated Output updated belief
 * @return 0 on success
 */
int ling_fuzzy_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
);

/**
 * @brief Get current precision for mesh weighting
 *
 * Implements linguistics_mesh_handler_t::get_precision
 *
 * @param ctx Bridge context
 * @return Precision Π ∈ [PRECISION_FLOOR, PRECISION_CEILING]
 */
float ling_fuzzy_mesh_get_precision(void* ctx);

/**
 * @brief Get mesh handler interface
 *
 * @param bridge Fuzzy bridge
 * @param handler Output handler struct
 * @return 0 on success
 */
int ling_fuzzy_get_mesh_handler(
    ling_fuzzy_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * INFERENCE API
 * ============================================================================ */

/**
 * @brief Perform fuzzy inference for spatial relation
 *
 * Given a natural language spatial phrase, performs full fuzzy inference
 * to determine the spatial semantics.
 *
 * @param bridge Fuzzy bridge
 * @param phrase Spatial phrase (e.g., "very near the table")
 * @param semantics Output spatial semantics
 * @return 0 on success
 */
int ling_fuzzy_infer_spatial(
    ling_fuzzy_bridge_t* bridge,
    const char* phrase,
    spatial_semantics_t* semantics
);

/**
 * @brief Defuzzify membership to crisp distance
 *
 * @param bridge Fuzzy bridge
 * @param preposition Preposition
 * @param membership Target membership degree
 * @param distance Output crisp distance
 * @return 0 on success
 */
int ling_fuzzy_defuzzify_distance(
    ling_fuzzy_bridge_t* bridge,
    spatial_preposition_t preposition,
    float membership,
    float* distance
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Fuzzy bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int ling_fuzzy_bridge_get_stats(
    const ling_fuzzy_bridge_t* bridge,
    ling_fuzzy_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Fuzzy bridge
 */
void ling_fuzzy_bridge_reset_stats(ling_fuzzy_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* ling_fuzzy_bridge_get_last_error(void);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Get preposition name string
 *
 * @param preposition Preposition type
 * @return Static string name
 */
const char* ling_fuzzy_preposition_name(spatial_preposition_t preposition);

/**
 * @brief Get hedge name string
 *
 * @param hedge Hedge type
 * @return Static string name
 */
const char* ling_fuzzy_hedge_name(fuzzy_hedge_t hedge);

/**
 * @brief Parse hedge from string
 *
 * @param str Hedge string (e.g., "very", "somewhat")
 * @param hedge Output hedge type
 * @return 0 on success, -1 if unknown
 */
int ling_fuzzy_parse_hedge(const char* str, fuzzy_hedge_t* hedge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_FUZZY_BRIDGE_H */
