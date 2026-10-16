/**
 * @file nimcp_rcog_imagination_bridge.h
 * @brief Imagination Engine Integration Bridge for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting recursive cognition with imagination engine
 * WHY:  Imagination enables hypothetical reasoning, mental rehearsal, and prospective simulation
 * HOW:  Full bridge pattern with simulation requests and imagination-guided processing
 *
 * BIOLOGICAL BASIS:
 * The default mode network (imagination) supports recursive problem-solving:
 * - Mental simulation predicts subtask outcomes before execution
 * - Counterfactual reasoning explores alternative decomposition strategies
 * - Prospective simulation guides answer refinement
 * - Creative imagination generates novel decomposition approaches
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * | RECURSIVE COGNITION  |                    |  IMAGINATION ENGINE  |
 * |                      |                    |                      |
 * | - Orchestrator       |<-- simulate ------>| - Scenario Manager   |
 * |   (decomposition)    |    subtasks        | - World Model        |
 * | - Delegation Pool    |                    | - Latent Space       |
 * |   (execution)        |<-- rehearse ------>| - Visual Generation  |
 * | - Answer Refiner     |    results         | - Prospective Sim    |
 * |   (refinement)       |                    |                      |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (hypothetical reasoning)
 * ```
 *
 * IMAGINATION MODES:
 * - DIRECTED: Goal-directed subtask simulation
 * - COUNTERFACTUAL: "What if" decomposition analysis
 * - PROSPECTIVE: Predict answer quality before refinement
 * - CREATIVE: Generate novel decomposition strategies
 */

#ifndef NIMCP_RCOG_IMAGINATION_BRIDGE_H
#define NIMCP_RCOG_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_engine;
struct rcog_orchestrator;
struct rcog_delegation_pool;
struct rcog_answer_refiner;
struct rcog_subtask;
struct rcog_decomposition;
struct imagination_engine;
struct imagination_scenario;
struct imagination_goal;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum simulated decompositions per request */
#define RCOG_IMAG_MAX_SIMULATED_DECOMPOSITIONS  4

/** Maximum rehearsed subtasks per batch */
#define RCOG_IMAG_MAX_REHEARSED_SUBTASKS        8

/** Default simulation confidence threshold */
#define RCOG_IMAG_DEFAULT_SIMULATION_THRESHOLD  0.6f

/** Default imagination depth for recursive simulation */
#define RCOG_IMAG_DEFAULT_IMAGINATION_DEPTH     3

/*=============================================================================
 * IMAGINATION MODES
 *===========================================================================*/

/**
 * @brief Imagination modes for recursive cognition
 */
typedef enum {
    RCOG_IMAG_MODE_DIRECTED = 0,     /**< Goal-directed subtask simulation */
    RCOG_IMAG_MODE_COUNTERFACTUAL,   /**< What-if decomposition analysis */
    RCOG_IMAG_MODE_PROSPECTIVE,      /**< Predict answer quality */
    RCOG_IMAG_MODE_CREATIVE          /**< Generate novel strategies */
} rcog_imagination_mode_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Simulation result for a single decomposition
 */
typedef struct {
    uint32_t decomposition_id;           /**< Decomposition being simulated */
    float predicted_success_rate;        /**< Predicted success rate [0.0-1.0] */
    float predicted_completion_time_ms;  /**< Predicted completion time */
    float predicted_confidence;          /**< Predicted answer confidence */
    float resource_efficiency;           /**< Predicted resource efficiency */
    uint32_t predicted_depth;            /**< Predicted recursion depth */
    bool has_deadlock_risk;              /**< Predicted deadlock risk */
    const char* risk_description;        /**< Description of risks */
} rcog_simulation_result_t;

/**
 * @brief Rehearsal result for a single subtask
 */
typedef struct {
    uint64_t subtask_id;                 /**< Subtask being rehearsed */
    float predicted_success;             /**< Predicted success probability */
    float predicted_duration_ms;         /**< Predicted duration */
    float predicted_confidence;          /**< Predicted result confidence */
    bool should_execute;                 /**< Whether to actually execute */
    float imagined_result_quality;       /**< Quality of imagined result */
} rcog_rehearsal_result_t;

/**
 * @brief Effects flowing from recursive cognition to imagination
 *
 * WHAT: Requests for mental simulation and hypothetical reasoning
 * WHY:  Predict outcomes before committing resources
 */
typedef struct {
    /* Simulation requests */
    bool request_decomposition_simulation; /**< Request decomposition simulation */
    uint32_t num_decompositions;           /**< Number to simulate */
    float simulation_detail_level;         /**< Level of detail [0.0-1.0] */

    /* Rehearsal requests */
    bool request_subtask_rehearsal;        /**< Request subtask rehearsal */
    uint32_t num_subtasks_to_rehearse;     /**< Number of subtasks */
    float rehearsal_depth;                 /**< Depth of mental rehearsal */

    /* Counterfactual requests */
    bool request_counterfactual;           /**< Request what-if analysis */
    uint32_t alternative_count;            /**< Number of alternatives */

    /* Answer imagination */
    bool request_answer_prediction;        /**< Request answer quality prediction */
    float current_confidence;              /**< Current answer confidence */

    /* Goal context */
    rcog_goal_type_t goal_type;            /**< Type of goal being processed */
    uint32_t current_depth;                /**< Current recursion depth */
    float urgency;                         /**< Processing urgency [0.0-1.0] */
} rcog_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to recursive cognition
 *
 * WHAT: Simulation results and imagination-guided recommendations
 * WHY:  Guide recursive processing based on predicted outcomes
 */
typedef struct {
    /* Simulation results */
    bool simulation_complete;              /**< Simulation is complete */
    uint32_t num_simulation_results;       /**< Number of results */
    rcog_simulation_result_t* simulation_results; /**< Array of results */
    uint32_t recommended_decomposition;    /**< Recommended decomposition index */

    /* Rehearsal results */
    bool rehearsal_complete;               /**< Rehearsal is complete */
    uint32_t num_rehearsal_results;        /**< Number of results */
    rcog_rehearsal_result_t* rehearsal_results; /**< Array of results */
    uint32_t subtasks_to_skip;             /**< Bitmap of subtasks to skip */

    /* Counterfactual insights */
    bool counterfactual_complete;          /**< Counterfactual analysis complete */
    float alternative_benefit;             /**< Benefit of best alternative */
    uint32_t recommended_alternative;      /**< Recommended alternative index */

    /* Answer prediction */
    float predicted_final_confidence;      /**< Predicted final confidence */
    uint32_t predicted_steps_remaining;    /**< Predicted refinement steps */
    bool early_termination_recommended;    /**< Recommend early termination */

    /* Creative suggestions */
    bool has_creative_suggestion;          /**< Has novel strategy suggestion */
    const char* creative_strategy;         /**< Description of novel strategy */
    float novelty_score;                   /**< Novelty of suggestion [0.0-1.0] */

    /* Resource guidance */
    float recommended_parallelism;         /**< Recommended parallelism level */
    float recommended_depth_limit;         /**< Recommended depth limit */
} imagination_to_rcog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Imagination bridge configuration
 */
typedef struct {
    /* Simulation parameters */
    float simulation_threshold;            /**< Min confidence for simulation */
    uint32_t max_simulation_depth;         /**< Max depth of simulation */
    float simulation_timeout_ms;           /**< Timeout for simulation */

    /* Rehearsal parameters */
    bool enable_automatic_rehearsal;       /**< Auto-rehearse before execution */
    float rehearsal_threshold;             /**< Threshold for rehearsal */
    uint32_t max_rehearsals_per_batch;     /**< Max rehearsals per batch */

    /* Mode selection */
    rcog_imagination_mode_t default_mode;  /**< Default imagination mode */
    bool enable_creative_mode;             /**< Enable creative suggestions */

    /* Resource limits */
    float max_imagination_cpu_fraction;    /**< Max CPU for imagination */
    uint32_t max_concurrent_scenarios;     /**< Max concurrent scenarios */
} rcog_imagination_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Imagination bridge opaque handle
 */
typedef struct rcog_imagination_bridge rcog_imagination_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create imagination bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
rcog_imagination_bridge_t* rcog_imagination_bridge_create(
    const rcog_imagination_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
rcog_imagination_bridge_t* rcog_imagination_bridge_create_default(void);

/**
 * @brief Destroy imagination bridge
 * @param bridge Bridge handle (NULL safe)
 */
void rcog_imagination_bridge_destroy(rcog_imagination_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
rcog_imagination_bridge_config_t rcog_imagination_bridge_default_config(void);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to imagination engine
 * @param bridge Bridge handle
 * @param imagination Imagination engine handle
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_connect(
    rcog_imagination_bridge_t* bridge,
    struct imagination_engine* imagination
);

/**
 * @brief Connect bridge to recursive cognition engine
 * @param bridge Bridge handle
 * @param engine Recursive cognition engine handle
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_connect_engine(
    rcog_imagination_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected to both systems
 */
bool rcog_imagination_bridge_is_connected(const rcog_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_update(
    rcog_imagination_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * SIMULATION
 *===========================================================================*/

/**
 * @brief Simulate decomposition outcomes
 *
 * Uses imagination to predict which decomposition strategy will yield
 * best results without actually executing.
 *
 * @param bridge Bridge handle
 * @param goal Goal being processed
 * @param decompositions Array of candidate decompositions
 * @param num_decompositions Number of candidates
 * @param results Output array of simulation results
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_simulate_decompositions(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    struct rcog_decomposition** decompositions,
    size_t num_decompositions,
    rcog_simulation_result_t* results
);

/**
 * @brief Rehearse subtask execution
 *
 * Mental rehearsal predicts subtask outcome without consuming resources.
 *
 * @param bridge Bridge handle
 * @param subtasks Array of subtasks to rehearse
 * @param num_subtasks Number of subtasks
 * @param results Output array of rehearsal results
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_rehearse_subtasks(
    rcog_imagination_bridge_t* bridge,
    struct rcog_subtask** subtasks,
    size_t num_subtasks,
    rcog_rehearsal_result_t* results
);

/**
 * @brief Perform counterfactual analysis
 *
 * Analyze "what if we had decomposed differently?" scenarios.
 *
 * @param bridge Bridge handle
 * @param actual Actual decomposition used
 * @param alternatives Array of alternative decompositions
 * @param num_alternatives Number of alternatives
 * @param benefit_scores Output array of benefit scores
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_counterfactual_analysis(
    rcog_imagination_bridge_t* bridge,
    const struct rcog_decomposition* actual,
    struct rcog_decomposition** alternatives,
    size_t num_alternatives,
    float* benefit_scores
);

/**
 * @brief Predict answer quality before full computation
 *
 * Quick approximate answer via imagination for early termination.
 *
 * @param bridge Bridge handle
 * @param goal Goal being processed
 * @param predicted_confidence Output predicted confidence
 * @param predicted_steps Output predicted remaining steps
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_predict_answer(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    float* predicted_confidence,
    uint32_t* predicted_steps
);

/*=============================================================================
 * CREATIVE GENERATION
 *===========================================================================*/

/**
 * @brief Request creative decomposition strategy
 *
 * Uses imagination's creative mode to generate novel approaches.
 *
 * @param bridge Bridge handle
 * @param goal Goal to decompose
 * @param constraint Constraints on strategy
 * @param strategy Output strategy description
 * @param novelty_score Output novelty score
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_generate_creative_strategy(
    rcog_imagination_bridge_t* bridge,
    const rcog_goal_t* goal,
    const char* constraint,
    char** strategy,
    float* novelty_score
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from rcog to imagination
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_get_outgoing_effects(
    const rcog_imagination_bridge_t* bridge,
    rcog_to_imagination_effects_t* effects
);

/**
 * @brief Get current effects from imagination to rcog
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_get_incoming_effects(
    const rcog_imagination_bridge_t* bridge,
    imagination_to_rcog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t simulations_requested;
    uint64_t simulations_completed;
    uint64_t rehearsals_requested;
    uint64_t rehearsals_completed;
    uint64_t counterfactuals_analyzed;
    uint64_t creative_strategies_generated;
    float avg_simulation_accuracy;
    float avg_rehearsal_accuracy;
    uint64_t subtasks_skipped_by_rehearsal;
    float total_time_saved_ms;
} rcog_imagination_bridge_stats_t;

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_imagination_bridge_get_stats(
    const rcog_imagination_bridge_t* bridge,
    rcog_imagination_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void rcog_imagination_bridge_reset_stats(rcog_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_IMAGINATION_BRIDGE_H */
