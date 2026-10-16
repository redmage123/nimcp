/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_hub_bridge.h - Information Geometry to Cognitive Hub Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_hub_bridge.h
 * @brief Bridge connecting Information Geometry with Cognitive Hub
 *
 * WHAT: Provides bidirectional integration between Information Geometry and
 *       the Cognitive Hub for geometry-informed high-level cognition.
 *
 * WHY:  Information geometry enhances cognitive hub operations:
 *       - Natural gradients optimize cognitive model learning
 *       - Manifold structure reveals cognitive state organization
 *       - KL divergence measures conceptual similarity
 *       - Geodesics enable smooth cognitive state transitions
 *       - Fisher information identifies critical cognitive parameters
 *
 * HOW:  Two-way integration:
 *       1. InfoGeo -> Hub: Geometric metrics inform cognitive processing
 *       2. Hub -> InfoGeo: Cognitive demands modulate geometry computation
 *       3. Manifold embeddings for cognitive state representation
 *       4. Natural gradient optimization for cognitive learning
 *
 * COGNITIVE INTEGRATION:
 * ```
 * INFORMATION GEOMETRY                    COGNITIVE HUB
 * -----------------------------------------------------------------------
 * Fisher Information                  ->  Parameter importance for reasoning
 * Natural Gradient                    ->  Optimal belief update direction
 * Manifold Embedding                  ->  Cognitive state representation
 * KL Divergence                       ->  Conceptual distance/similarity
 * Geodesic Path                       ->  Cognitive transition trajectory
 * Ricci Curvature                     ->  Reasoning complexity metric
 *
 * COGNITIVE HUB TO INFOGEO:
 * -----------------------------------------------------------------------
 * Working Memory State                ->  Distributions for Fisher
 * Task Context                        ->  Manifold region selection
 * Cognitive Load                      ->  Computation budget allocation
 * Goal State                          ->  Geodesic target specification
 * ```
 *
 * APPLICATIONS:
 * - Cognitive state space navigation via geodesics
 * - Conceptual similarity via KL divergence
 * - Optimal reasoning via natural gradient
 * - Cognitive complexity analysis via curvature
 * - Working memory organization via manifold structure
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_HUB_BRIDGE_H
#define NIMCP_INFOGEO_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_HUB_MODULE_NAME          "infogeo_hub_bridge"

/** Maximum cognitive dimensions */
#define INFOGEO_HUB_MAX_COG_DIM          64

/** Maximum concepts for similarity */
#define INFOGEO_HUB_MAX_CONCEPTS         128

/** Maximum cognitive states tracked */
#define INFOGEO_HUB_MAX_STATES           32

/** Default geodesic interpolation steps */
#define INFOGEO_HUB_GEODESIC_STEPS       20

/** Default complexity threshold */
#define INFOGEO_HUB_COMPLEXITY_THRESH    2.0f

/** Default conceptual similarity threshold */
#define INFOGEO_HUB_SIMILARITY_THRESH    0.8f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive operation type
 */
typedef enum {
    INFOGEO_HUB_OP_REASONING = 0,       /**< Logical reasoning */
    INFOGEO_HUB_OP_RETRIEVAL,           /**< Memory retrieval */
    INFOGEO_HUB_OP_PLANNING,            /**< Goal-directed planning */
    INFOGEO_HUB_OP_ANALOGY,             /**< Analogical reasoning */
    INFOGEO_HUB_OP_INTEGRATION          /**< Multi-modal integration */
} infogeo_hub_operation_t;

/**
 * @brief Cognitive state type
 */
typedef enum {
    INFOGEO_HUB_STATE_IDLE = 0,         /**< Idle/resting state */
    INFOGEO_HUB_STATE_PROCESSING,       /**< Active processing */
    INFOGEO_HUB_STATE_TRANSITIONING,    /**< State transition */
    INFOGEO_HUB_STATE_CONSOLIDATING     /**< Memory consolidation */
} infogeo_hub_state_type_t;

/**
 * @brief Conceptual distance metric
 */
typedef enum {
    INFOGEO_HUB_DIST_KL = 0,            /**< KL divergence */
    INFOGEO_HUB_DIST_GEODESIC,          /**< Geodesic distance */
    INFOGEO_HUB_DIST_FISHER,            /**< Fisher-Rao distance */
    INFOGEO_HUB_DIST_COSINE             /**< Cosine in embedding space */
} infogeo_hub_distance_t;

/**
 * @brief Optimization target for cognitive learning
 */
typedef enum {
    INFOGEO_HUB_OPT_ACCURACY = 0,       /**< Maximize accuracy */
    INFOGEO_HUB_OPT_EFFICIENCY,         /**< Maximize efficiency */
    INFOGEO_HUB_OPT_ROBUSTNESS,         /**< Maximize robustness */
    INFOGEO_HUB_OPT_BALANCED            /**< Balanced optimization */
} infogeo_hub_opt_target_t;

/**
 * @brief Cognitive complexity level
 */
typedef enum {
    INFOGEO_HUB_COMPLEXITY_LOW = 0,     /**< Low complexity (flat manifold) */
    INFOGEO_HUB_COMPLEXITY_MEDIUM,      /**< Medium complexity */
    INFOGEO_HUB_COMPLEXITY_HIGH,        /**< High complexity (curved) */
    INFOGEO_HUB_COMPLEXITY_EXTREME      /**< Extreme complexity */
} infogeo_hub_complexity_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-Hub bridge
 */
typedef struct {
    /** Cognitive embedding settings */
    uint32_t cognitive_dim;              /**< Cognitive embedding dimension */
    uint32_t manifold_latent_dim;        /**< Latent manifold dimension */
    bool enable_adaptive_embedding;      /**< Adapt embedding over time */
    float embedding_learning_rate;       /**< Embedding update rate */

    /** Distance settings */
    infogeo_hub_distance_t distance_metric; /**< Primary distance metric */
    float similarity_threshold;          /**< Threshold for similarity */
    bool enable_geodesic_cache;          /**< Cache geodesic computations */

    /** Optimization settings */
    infogeo_hub_opt_target_t opt_target; /**< Optimization objective */
    float natural_gradient_lr;           /**< Natural gradient learning rate */
    bool enable_natural_gradient;        /**< Use natural gradient */
    float gradient_clip;                 /**< Gradient clipping threshold */

    /** Complexity settings */
    float complexity_threshold;          /**< Curvature for high complexity */
    bool enable_complexity_adaptation;   /**< Adapt to complexity */
    float complexity_budget;             /**< Processing budget for complex */

    /** Transition settings */
    uint32_t geodesic_steps;            /**< Steps for state transition */
    bool enable_smooth_transitions;      /**< Smooth cognitive transitions */
    float transition_speed;              /**< Base transition speed */

    /** General settings */
    float update_interval_ms;           /**< Bridge update interval */
    bool enable_logging;                 /**< Enable logging */
} infogeo_hub_config_t;

/**
 * @brief Cognitive state embedding
 */
typedef struct {
    uint32_t state_id;                  /**< State identifier */
    char state_label[64];               /**< Human-readable label */
    float* embedding;                   /**< Manifold embedding */
    uint32_t embedding_dim;             /**< Embedding dimensionality */
    float* distribution;                /**< Associated probability dist */
    uint32_t dist_dim;                  /**< Distribution dimensionality */
    float curvature;                    /**< Local curvature at state */
    float stability;                    /**< State stability [0,1] */
} infogeo_cog_state_t;

/**
 * @brief Conceptual similarity result
 */
typedef struct {
    uint32_t concept_a_id;              /**< First concept ID */
    uint32_t concept_b_id;              /**< Second concept ID */
    float kl_distance;                  /**< KL divergence */
    float geodesic_distance;            /**< Geodesic distance */
    float fisher_distance;              /**< Fisher-Rao distance */
    float cosine_similarity;            /**< Cosine similarity */
    float combined_similarity;          /**< Weighted combination */
    bool are_similar;                   /**< Above similarity threshold */
} infogeo_similarity_t;

/**
 * @brief Cognitive transition path
 */
typedef struct {
    uint32_t start_state_id;            /**< Starting state */
    uint32_t end_state_id;              /**< Target state */
    float* path_embeddings;             /**< Embeddings along path */
    uint32_t num_steps;                 /**< Number of steps */
    uint32_t embedding_dim;             /**< Embedding dimensionality */
    float total_geodesic_length;        /**< Total path length */
    float avg_curvature;                /**< Average curvature along path */
    float estimated_time_ms;            /**< Estimated transition time */
    float progress;                     /**< Current progress [0,1] */
} infogeo_transition_path_t;

/**
 * @brief Cognitive complexity analysis
 */
typedef struct {
    uint32_t region_id;                 /**< Cognitive region */
    infogeo_hub_operation_t operation;  /**< Operation type */
    float ricci_curvature;              /**< Ricci scalar curvature */
    float intrinsic_dimension;          /**< Intrinsic dimensionality */
    infogeo_hub_complexity_t level;     /**< Complexity level */
    float estimated_compute_cost;       /**< Estimated computation cost */
    float recommended_budget;           /**< Recommended processing budget */
} infogeo_complexity_analysis_t;

/**
 * @brief Natural gradient cognitive update
 */
typedef struct {
    float* natural_gradient;            /**< Natural gradient direction */
    float* standard_gradient;           /**< Standard gradient */
    uint32_t dim;                       /**< Dimensionality */
    float speedup_ratio;                /**< Natural/standard speedup */
    float fisher_condition;             /**< Fisher matrix condition */
    float learning_rate_used;           /**< Actual learning rate */
    infogeo_hub_opt_target_t target;    /**< Optimization target */
    bool converged;                     /**< Whether update converged */
} infogeo_cog_update_t;

/**
 * @brief Working memory geometric state
 */
typedef struct {
    float* wm_distribution;             /**< Working memory distribution */
    uint32_t wm_dim;                    /**< WM dimensionality */
    float wm_entropy;                   /**< WM entropy */
    float* fisher_diagonal;             /**< Fisher info for WM parameters */
    float wm_curvature;                 /**< WM manifold curvature */
    float capacity_utilization;         /**< WM capacity used [0,1] */
    uint32_t num_items;                 /**< Items in WM */
} infogeo_wm_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t similarity_computations;   /**< Similarity computations */
    uint64_t transitions_completed;     /**< Completed state transitions */
    uint64_t natural_grad_updates;      /**< Natural gradient updates */
    uint64_t complexity_analyses;       /**< Complexity analyses */
    float avg_similarity;               /**< Average concept similarity */
    float avg_transition_time_ms;       /**< Average transition time */
    float avg_complexity;               /**< Average complexity level */
    float avg_speedup_ratio;            /**< Average natural grad speedup */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_hub_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_hub_bridge_struct infogeo_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_default_config(infogeo_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-Hub bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_hub_bridge_t* infogeo_hub_bridge_create(
    const infogeo_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_hub_bridge_destroy(infogeo_hub_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_reset(infogeo_hub_bridge_t* bridge);

//=============================================================================
// Cognitive State API
//=============================================================================

/**
 * @brief Register cognitive state
 *
 * WHAT: Registers a cognitive state with its embedding
 * WHY:  Enables geometric operations on cognitive states
 * HOW:  Stores embedding and computes local geometry
 *
 * @param bridge Bridge handle
 * @param state Cognitive state to register
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_register_state(
    infogeo_hub_bridge_t* bridge,
    const infogeo_cog_state_t* state
);

/**
 * @brief Get cognitive state
 *
 * @param bridge Bridge handle
 * @param state_id State identifier
 * @param state Output cognitive state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_get_state(
    const infogeo_hub_bridge_t* bridge,
    uint32_t state_id,
    infogeo_cog_state_t* state
);

/**
 * @brief Update cognitive state embedding
 *
 * WHAT: Updates embedding for existing cognitive state
 * WHY:  States evolve as learning occurs
 * HOW:  Recomputes embedding and local geometry
 *
 * @param bridge Bridge handle
 * @param state_id State identifier
 * @param new_embedding Updated embedding
 * @param dim Embedding dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_update_state(
    infogeo_hub_bridge_t* bridge,
    uint32_t state_id,
    const float* new_embedding,
    uint32_t dim
);

//=============================================================================
// Conceptual Similarity API
//=============================================================================

/**
 * @brief Compute conceptual similarity
 *
 * WHAT: Computes similarity between concepts using geometry
 * WHY:  Principled similarity for cognitive operations
 * HOW:  Uses configured distance metric (KL, geodesic, etc.)
 *
 * @param bridge Bridge handle
 * @param concept_a First concept distribution
 * @param concept_b Second concept distribution
 * @param dim Distribution dimensionality
 * @param similarity Output similarity result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_compute_similarity(
    infogeo_hub_bridge_t* bridge,
    const float* concept_a,
    const float* concept_b,
    uint32_t dim,
    infogeo_similarity_t* similarity
);

/**
 * @brief Find most similar concept
 *
 * WHAT: Finds concept most similar to query
 * WHY:  Memory retrieval, analogy finding
 * HOW:  Computes similarity to all registered concepts
 *
 * @param bridge Bridge handle
 * @param query Query concept distribution
 * @param dim Distribution dimensionality
 * @param concepts Array of concept IDs to search
 * @param num_concepts Number of concepts
 * @param best_match Output best matching concept ID
 * @param similarity Output similarity score
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_find_similar(
    infogeo_hub_bridge_t* bridge,
    const float* query,
    uint32_t dim,
    const uint32_t* concepts,
    uint32_t num_concepts,
    uint32_t* best_match,
    float* similarity
);

//=============================================================================
// Cognitive Transition API
//=============================================================================

/**
 * @brief Plan cognitive state transition
 *
 * WHAT: Plans geodesic path between cognitive states
 * WHY:  Smooth, optimal cognitive transitions
 * HOW:  Computes geodesic on cognitive manifold
 *
 * @param bridge Bridge handle
 * @param start_state_id Starting state ID
 * @param end_state_id Target state ID
 * @param path Output transition path
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_plan_transition(
    infogeo_hub_bridge_t* bridge,
    uint32_t start_state_id,
    uint32_t end_state_id,
    infogeo_transition_path_t* path
);

/**
 * @brief Step cognitive transition
 *
 * WHAT: Advances cognitive transition by one step
 * WHY:  Gradual state transition along geodesic
 * HOW:  Moves to next point on geodesic path
 *
 * @param bridge Bridge handle
 * @param path Transition path to step
 * @param current_embedding Output current embedding
 * @return 1 if complete, 0 if in progress, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_step_transition(
    infogeo_hub_bridge_t* bridge,
    infogeo_transition_path_t* path,
    float* current_embedding
);

/**
 * @brief Get transition progress
 *
 * @param bridge Bridge handle
 * @return Progress [0,1], or -1.0 if not transitioning
 */
NIMCP_EXPORT float infogeo_hub_get_transition_progress(
    const infogeo_hub_bridge_t* bridge
);

//=============================================================================
// Complexity Analysis API
//=============================================================================

/**
 * @brief Analyze cognitive complexity
 *
 * WHAT: Analyzes complexity of cognitive operation region
 * WHY:  Allocate resources based on difficulty
 * HOW:  Computes curvature, intrinsic dimension
 *
 * @param bridge Bridge handle
 * @param region_id Cognitive region
 * @param operation Operation type
 * @param analysis Output complexity analysis
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_analyze_complexity(
    infogeo_hub_bridge_t* bridge,
    uint32_t region_id,
    infogeo_hub_operation_t operation,
    infogeo_complexity_analysis_t* analysis
);

/**
 * @brief Get recommended processing budget
 *
 * WHAT: Gets recommended budget based on complexity
 * WHY:  Adaptive resource allocation
 * HOW:  Maps complexity to processing budget
 *
 * @param bridge Bridge handle
 * @param region_id Cognitive region
 * @return Recommended budget [0,1], or -1.0 on error
 */
NIMCP_EXPORT float infogeo_hub_get_recommended_budget(
    const infogeo_hub_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// Natural Gradient Cognitive Learning API
//=============================================================================

/**
 * @brief Compute natural gradient for cognitive update
 *
 * WHAT: Computes natural gradient for cognitive parameter update
 * WHY:  Optimal learning in probability space
 * HOW:  Multiplies gradient by inverse Fisher
 *
 * @param bridge Bridge handle
 * @param gradient Standard gradient
 * @param grad_dim Gradient dimensionality
 * @param update Output cognitive update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_natural_gradient(
    infogeo_hub_bridge_t* bridge,
    const float* gradient,
    uint32_t grad_dim,
    infogeo_cog_update_t* update
);

/**
 * @brief Apply natural gradient update
 *
 * WHAT: Applies natural gradient update to cognitive parameters
 * WHY:  Single call for parameter update
 * HOW:  Computes natural gradient and applies
 *
 * @param bridge Bridge handle
 * @param parameters Parameters to update (modified in place)
 * @param gradient Standard gradient
 * @param dim Dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_apply_update(
    infogeo_hub_bridge_t* bridge,
    float* parameters,
    const float* gradient,
    uint32_t dim
);

//=============================================================================
// Working Memory Geometry API
//=============================================================================

/**
 * @brief Update working memory geometric state
 *
 * WHAT: Updates geometric representation of working memory
 * WHY:  Geometry-aware WM processing
 * HOW:  Computes Fisher, curvature for WM distribution
 *
 * @param bridge Bridge handle
 * @param wm_distribution Working memory distribution
 * @param wm_dim WM dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_update_wm_state(
    infogeo_hub_bridge_t* bridge,
    const float* wm_distribution,
    uint32_t wm_dim
);

/**
 * @brief Get working memory geometric state
 *
 * @param bridge Bridge handle
 * @param wm_state Output WM geometric state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_get_wm_state(
    const infogeo_hub_bridge_t* bridge,
    infogeo_wm_state_t* wm_state
);

/**
 * @brief Compute WM item importance via Fisher
 *
 * WHAT: Computes importance of WM items via Fisher info
 * WHY:  Principled importance weighting for WM
 * HOW:  Fisher diagonal gives parameter importance
 *
 * @param bridge Bridge handle
 * @param importance Output importance scores
 * @param num_items Number of WM items
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_wm_importance(
    const infogeo_hub_bridge_t* bridge,
    float* importance,
    uint32_t num_items
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Update embeddings, transition progress
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_update(
    infogeo_hub_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_hub_get_stats(
    const infogeo_hub_bridge_t* bridge,
    infogeo_hub_stats_t* stats
);

/**
 * @brief Get number of registered cognitive states
 *
 * @param bridge Bridge handle
 * @return Number of registered states
 */
NIMCP_EXPORT uint32_t infogeo_hub_state_count(
    const infogeo_hub_bridge_t* bridge
);

/**
 * @brief Check if transitioning between states
 *
 * @param bridge Bridge handle
 * @return true if in transition
 */
NIMCP_EXPORT bool infogeo_hub_is_transitioning(
    const infogeo_hub_bridge_t* bridge
);

/**
 * @brief Get current complexity level
 *
 * @param bridge Bridge handle
 * @return Current complexity level
 */
NIMCP_EXPORT infogeo_hub_complexity_t infogeo_hub_get_complexity(
    const infogeo_hub_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_HUB_BRIDGE_H */