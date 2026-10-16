/**
 * @file nimcp_omni_rcog_bridge.h
 * @brief Omnidirectional Inference to Recursive Cognition Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with recursive cognition
 * WHY:  Enable prediction-guided task decomposition and backward goal reasoning
 * HOW:  JEPA predictions inform decomposition, backward inference enables goal abduction
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTION-GUIDED TASK DECOMPOSITION:
 * -------------------------------------
 * Omnidirectional inference informs recursive task decomposition:
 *
 *   1. FORWARD PREDICTION + GOAL:
 *      - Predict outcomes of actions
 *      - Decompose into sequential subtasks
 *      - "If I do A, then B, then C → Goal achieved"
 *
 *   2. BACKWARD INFERENCE (MEANS-END ANALYSIS):
 *      - Given goal, infer required preconditions
 *      - Decompose into prerequisite subtasks
 *      - "Goal requires C, C requires B, B requires A"
 *
 *   3. HIERARCHICAL DECOMPOSITION:
 *      - Predictive hierarchy levels map to abstraction levels
 *      - High levels: Abstract goals
 *      - Low levels: Concrete actions
 *
 * PREDICTION ERROR AND DECOMPOSITION:
 * ------------------------------------
 *   PE Level        →  Decomposition Strategy
 *   ─────────────────────────────────────────────
 *   Low PE          →  Execute directly (no decomposition)
 *   Medium PE       →  Shallow decomposition
 *   High PE         →  Deep decomposition with subgoals
 *   Very High PE    →  Abort and re-plan
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Models prefrontal cortex (PFC) integration:
 * - Dorsolateral PFC: Working memory for subtask tracking
 * - Orbitofrontal cortex: Prediction-based decision making
 * - Anterior cingulate: Conflict monitoring (prediction errors)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_RCOG_BRIDGE_H
#define NIMCP_OMNI_RCOG_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_rcog_bridge omni_rcog_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct rcog_orchestrator rcog_orchestrator_t;
typedef struct rcog_engine rcog_engine_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-rcog bridge */
#define BIO_MODULE_OMNI_RCOG_BRIDGE            0x0E53

/** @brief Default PE threshold for direct execution */
#define OMNI_RCOG_DIRECT_EXEC_THRESHOLD        1.0f

/** @brief Default PE threshold for deep decomposition */
#define OMNI_RCOG_DEEP_DECOMP_THRESHOLD        5.0f

/** @brief Maximum decomposition depth adjustment */
#define OMNI_RCOG_MAX_DEPTH_ADJUSTMENT         3

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Decomposition strategy based on predictions
 */
typedef enum {
    OMNI_DECOMP_DIRECT = 0,          /**< Execute directly (low PE) */
    OMNI_DECOMP_SHALLOW,             /**< Shallow decomposition */
    OMNI_DECOMP_DEEP,                /**< Deep hierarchical decomposition */
    OMNI_DECOMP_BACKWARD,            /**< Backward goal decomposition */
    OMNI_DECOMP_BIDIRECTIONAL,       /**< Forward + backward combined */
    OMNI_DECOMP_ABORT                /**< Abort and re-plan (very high PE) */
} omni_decomp_strategy_t;

/**
 * @brief Goal inference mode
 */
typedef enum {
    OMNI_GOAL_FORWARD = 0,           /**< Infer goals from actions */
    OMNI_GOAL_BACKWARD,              /**< Infer preconditions from goals */
    OMNI_GOAL_HIERARCHICAL,          /**< Abstract to concrete */
    OMNI_GOAL_ASSOCIATIVE            /**< Hopfield-based goal retrieval */
} omni_goal_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni effects on recursive cognition
 */
typedef struct {
    omni_decomp_strategy_t strategy; /**< Suggested decomposition strategy */
    uint32_t suggested_depth;        /**< Suggested recursion depth */
    float execution_confidence;      /**< Confidence in direct execution */
    float* subgoal_predictions;      /**< Predicted subgoal states */
    uint32_t num_subgoals;           /**< Number of predicted subgoals */
    bool needs_backward_inference;   /**< Backward inference recommended */
} omni_to_rcog_effects_t;

/**
 * @brief Rcog effects on omni inference
 */
typedef struct {
    float* goal_embedding;           /**< Current goal as embedding */
    uint32_t goal_dim;               /**< Goal embedding dimension */
    uint32_t current_depth;          /**< Current recursion depth */
    float task_urgency;              /**< Task urgency (affects precision) */
    bool in_backtracking;            /**< Currently backtracking */
    float working_memory_load;       /**< WM load (affects capacity) */
} rcog_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* PE thresholds for strategy selection */
    float direct_exec_threshold;     /**< PE threshold for direct exec */
    float shallow_decomp_threshold;  /**< PE threshold for shallow */
    float deep_decomp_threshold;     /**< PE threshold for deep */
    float abort_threshold;           /**< PE threshold for abort */

    /* Decomposition parameters */
    uint32_t base_recursion_depth;   /**< Base recursion depth */
    uint32_t max_depth_adjustment;   /**< Max depth adjustment */
    bool enable_backward_decomp;     /**< Enable backward decomposition */
    bool enable_bidirectional;       /**< Enable bidirectional decomp */

    /* Goal inference */
    omni_goal_mode_t goal_mode;      /**< Goal inference mode */
    bool use_hopfield_goals;         /**< Use Hopfield for goal retrieval */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_rcog_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t direct_executions;      /**< Direct execution count */
    uint64_t shallow_decompositions; /**< Shallow decomp count */
    uint64_t deep_decompositions;    /**< Deep decomp count */
    uint64_t backward_inferences;    /**< Backward inference count */
    uint64_t aborts;                 /**< Abort count */
    float avg_pe_at_decomp;          /**< Average PE at decomposition */
    float avg_depth;                 /**< Average recursion depth */
} omni_rcog_stats_t;

/**
 * @brief Omni-rcog bridge structure
 */
struct omni_rcog_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_rcog_config_t config;       /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    rcog_orchestrator_t* orchestrator; /**< RCOG orchestrator */
    rcog_engine_t* engine;           /**< RCOG engine */

    /* Computed effects */
    omni_to_rcog_effects_t omni_effects;  /**< Omni → rcog */
    rcog_to_omni_effects_t rcog_effects;  /**< Rcog → omni */

    /* Statistics */
    omni_rcog_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_rcog_default_config(omni_rcog_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-rcog bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
omni_rcog_bridge_t* omni_rcog_bridge_create(const omni_rcog_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void omni_rcog_bridge_destroy(omni_rcog_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to bidirectional JEPA
 */
int omni_rcog_connect_jepa(omni_rcog_bridge_t* bridge,
                            jepa_bidirectional_t* jepa);

/**
 * @brief Connect to predictive hierarchy
 */
int omni_rcog_connect_pred_hier(omni_rcog_bridge_t* bridge,
                                 predictive_hierarchy_t* pred_hier);

/**
 * @brief Connect to RCOG orchestrator
 */
int omni_rcog_connect_orchestrator(omni_rcog_bridge_t* bridge,
                                    rcog_orchestrator_t* orchestrator);

/**
 * @brief Connect to RCOG engine
 */
int omni_rcog_connect_engine(omni_rcog_bridge_t* bridge,
                              rcog_engine_t* engine);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge
 */
int omni_rcog_update(omni_rcog_bridge_t* bridge);

/**
 * @brief Apply omni effects to rcog
 */
int omni_rcog_apply_to_rcog(omni_rcog_bridge_t* bridge);

/**
 * @brief Apply rcog effects to omni
 */
int omni_rcog_apply_to_omni(omni_rcog_bridge_t* bridge);

/* ============================================================================
 * Decomposition API
 * ============================================================================ */

/**
 * @brief Get recommended decomposition strategy
 *
 * @param bridge Bridge
 * @param strategy Output strategy
 * @return NIMCP_SUCCESS on success
 */
int omni_rcog_get_strategy(omni_rcog_bridge_t* bridge,
                            omni_decomp_strategy_t* strategy);

/**
 * @brief Get suggested recursion depth
 *
 * @param bridge Bridge
 * @return Suggested depth
 */
uint32_t omni_rcog_get_suggested_depth(const omni_rcog_bridge_t* bridge);

/**
 * @brief Check if backward inference is needed
 *
 * @param bridge Bridge
 * @return true if backward inference recommended
 */
bool omni_rcog_needs_backward(const omni_rcog_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get omni-to-rcog effects
 */
int omni_rcog_get_omni_effects(omni_rcog_bridge_t* bridge,
                                omni_to_rcog_effects_t* effects);

/**
 * @brief Get rcog-to-omni effects
 */
int omni_rcog_get_rcog_effects(omni_rcog_bridge_t* bridge,
                                rcog_to_omni_effects_t* effects);

/**
 * @brief Get statistics
 */
int omni_rcog_get_stats(omni_rcog_bridge_t* bridge,
                         omni_rcog_stats_t* stats);

/**
 * @brief Reset statistics
 */
int omni_rcog_reset_stats(omni_rcog_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_rcog_connect_bio_async(omni_rcog_bridge_t* bridge);
int omni_rcog_disconnect_bio_async(omni_rcog_bridge_t* bridge);
bool omni_rcog_is_bio_async_connected(const omni_rcog_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_rcog_strategy_to_string(omni_decomp_strategy_t strategy);
const char* omni_rcog_goal_mode_to_string(omni_goal_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_RCOG_BRIDGE_H */
