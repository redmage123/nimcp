/**
 * @file nimcp_game_theory_executive_bridge.h
 * @brief Game Theory-Executive Integration Bridge for Cognitive Hub
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tier 2 Hub Bridge connecting game theory with executive function
 * WHY:  Strategic reasoning informs executive decisions; executive feedback
 *       guides strategy refinement; risk assessment bridges uncertainty with action
 * HOW:  Game theory provides strategy options and risk assessment, executive makes
 *       decisions, decision outcomes flow back to update strategic models
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral prefrontal cortex (dlPFC) integrates strategic planning
 * - Orbitofrontal cortex evaluates strategic outcomes
 * - Anterior cingulate cortex monitors strategy-action conflicts
 * - Insular cortex processes risk and uncertainty
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_DECISION_MADE for executive outcomes
 * - Subscribe: COG_EVENT_ATTENTION_SHIFT for priority changes
 * - Subscribe: COG_EVENT_QUERY for strategic analysis requests
 * - Subscribe: COG_EVENT_STATE_CHANGE for strategy updates
 * - Publish: COG_EVENT_OUTPUT_READY when strategy recommended
 * - Publish: COG_EVENT_PREDICTION for opponent model outputs
 * - Query: Expose strategic analysis to other modules
 *
 * KEY CONCEPTS:
 * - Game theory provides strategic analysis for executive decisions
 * - Executive function makes final decisions considering strategic advice
 * - Risk assessment bridges strategic uncertainty with action selection
 * - Opponent modeling informs executive planning
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GAME_THEORY_EXECUTIVE_BRIDGE_H
#define NIMCP_GAME_THEORY_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum concurrent strategies */
#define GT_EXEC_MAX_STRATEGIES              32

/** Default module ID */
#define GT_EXEC_DEFAULT_MODULE_ID           0x47544558  /* "GTEX" */

/** Maximum players in strategic model */
#define GT_EXEC_MAX_PLAYERS                 16

/** Maximum opponent models to track */
#define GT_EXEC_MAX_OPPONENT_MODELS         8

/** Maximum subscribed event types */
#define GT_EXEC_MAX_SUBSCRIPTIONS           16

/** Maximum event buffer size */
#define GT_EXEC_MAX_EVENT_BUFFER            64

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct game_theory_executive_bridge game_theory_executive_bridge_t;

struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    GT_EXEC_STATE_IDLE = 0,               /**< No active strategy evaluation */
    GT_EXEC_STATE_ANALYZING,              /**< Analyzing strategic options */
    GT_EXEC_STATE_RECOMMENDING,           /**< Recommending strategy */
    GT_EXEC_STATE_AWAITING_DECISION,      /**< Waiting for executive decision */
    GT_EXEC_STATE_UPDATING,               /**< Updating models from outcome */
    GT_EXEC_STATE_ERROR                   /**< Error state */
} game_theory_executive_state_t;

/**
 * @brief Strategy type enumeration
 */
typedef enum {
    GT_STRATEGY_DOMINANT = 0,             /**< Dominant strategy */
    GT_STRATEGY_NASH_EQUILIBRIUM,         /**< Nash equilibrium strategy */
    GT_STRATEGY_PARETO_OPTIMAL,           /**< Pareto optimal strategy */
    GT_STRATEGY_MINIMAX,                  /**< Minimax strategy */
    GT_STRATEGY_MIXED,                    /**< Mixed strategy */
    GT_STRATEGY_COUNT
} gt_strategy_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Game Theory-Executive bridge
 */
typedef struct {
    uint32_t module_id;                   /**< Module ID for hub registration */
    bool enable_logging;                  /**< Enable debug logging */
    float strategic_weight;               /**< Weight for strategic analysis [0,1] */
    float risk_assessment_weight;         /**< Weight for risk assessment [0,1] */
    float decision_integration_weight;    /**< Weight for decision integration [0,1] */
    uint32_t max_strategies;              /**< Max strategies to evaluate */
    float risk_tolerance;                 /**< Risk tolerance [0,1] */
    float time_pressure_factor;           /**< Time pressure influence [0,1] */
    bool auto_subscribe_decision;         /**< Subscribe to DECISION_MADE */
    bool auto_subscribe_attention;        /**< Subscribe to ATTENTION_SHIFT */
    bool auto_subscribe_query;            /**< Subscribe to QUERY events */
    bool auto_subscribe_state_change;     /**< Subscribe to STATE_CHANGE */
    bool enable_mixed_strategies;         /**< Enable mixed strategy analysis */
    bool enable_learning;                 /**< Enable strategy learning */
    bool enable_query_handler;            /**< Register query handler */
    uint32_t event_buffer_size;           /**< Size of internal event buffer */
    float opponent_model_decay;           /**< Decay rate for opponent models [0,1] */
} game_theory_executive_config_t;

/**
 * @brief Strategic recommendation
 */
typedef struct {
    uint64_t recommendation_id;           /**< Unique recommendation ID */
    gt_strategy_type_t strategy_type;     /**< Type of strategy */
    float expected_utility;               /**< Expected utility [0,1] */
    float confidence;                     /**< Recommendation confidence [0,1] */
    float risk_level;                     /**< Associated risk [0,1] */
    uint32_t action_index;                /**< Recommended action index */
} gt_strategic_recommendation_t;

/**
 * @brief Executive decision notification
 */
typedef struct {
    uint64_t decision_id;                 /**< Decision identifier */
    uint64_t recommendation_id;           /**< Original recommendation ID */
    uint32_t action_taken;                /**< Action actually taken */
    float outcome_utility;                /**< Realized outcome utility */
    bool followed_recommendation;         /**< Whether recommendation was followed */
} gt_decision_outcome_t;

/**
 * @brief Risk assessment result
 */
typedef struct {
    uint64_t action_id;                   /**< Action being assessed */
    float overall_risk;                   /**< Overall risk score [0,1] */
    float strategic_risk;                 /**< Strategic component of risk */
    float execution_risk;                 /**< Execution difficulty risk */
    float opportunity_cost;               /**< Opportunity cost of action */
    char risk_factors[256];               /**< Description of risk factors */
    uint64_t timestamp;                   /**< Assessment timestamp */
} gt_exec_risk_assessment_t;

/**
 * @brief Opponent model for strategic predictions
 */
typedef struct {
    uint32_t opponent_id;                 /**< Opponent identifier */
    float cooperation_tendency;           /**< Tendency to cooperate [0,1] */
    float aggression_level;               /**< Aggression level [0,1] */
    float predictability;                 /**< How predictable opponent is [0,1] */
    float strategy_probs[GT_EXEC_MAX_STRATEGIES]; /**< Probability distribution */
    uint32_t num_strategies;              /**< Number of possible strategies */
    uint64_t last_update;                 /**< Last model update timestamp */
    uint64_t interaction_count;           /**< Number of interactions observed */
} gt_exec_opponent_model_t;

/**
 * @brief Strategic situation descriptor for analysis requests
 */
typedef struct {
    uint64_t situation_id;                /**< Unique situation identifier */
    uint32_t situation_type;              /**< Type of strategic situation */
    uint32_t num_actions;                 /**< Number of available actions */
    uint32_t num_outcomes;                /**< Number of possible outcomes */
    const float* utilities;               /**< Utility matrix (actions x outcomes) */
    float urgency;                        /**< Decision urgency [0,1] */
    uint64_t deadline_ms;                 /**< Decision deadline (0 = none) */
    void* context;                        /**< User context */
} gt_exec_situation_t;

/**
 * @brief Statistics for Game Theory-Executive bridge
 */
typedef struct {
    uint64_t total_events;                /**< Total events processed */
    uint64_t strategic_decisions;         /**< Strategic decisions made */
    uint64_t risk_assessments;            /**< Risk assessments completed */
    uint64_t executive_overrides;         /**< Times executive overrode recommendation */
    uint64_t strategies_analyzed;         /**< Total strategies analyzed */
    uint64_t recommendations_made;        /**< Recommendations provided */
    uint64_t recommendations_followed;    /**< Recommendations followed */
    uint64_t decisions_received;          /**< Decision outcomes received */
    uint64_t events_received;             /**< Events from hub */
    uint64_t events_published;            /**< Events published to hub */
    uint64_t queries_handled;             /**< Queries handled */
    uint64_t opponent_model_requests;     /**< Opponent model requests */
    float avg_expected_utility;           /**< Average expected utility */
    float avg_realized_utility;           /**< Average realized utility */
    float avg_risk_score;                 /**< Average risk score */
    float recommendation_accuracy;        /**< Recommendation accuracy rate */
    uint64_t last_event_timestamp;        /**< Timestamp of last event */
} game_theory_executive_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_default_config(game_theory_executive_config_t* config);

/**
 * @brief Create Game Theory-Executive bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
game_theory_executive_bridge_t* game_theory_executive_bridge_create(
    const game_theory_executive_config_t* config
);

/**
 * @brief Destroy Game Theory-Executive bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void game_theory_executive_bridge_destroy(game_theory_executive_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to cognitive hub
 *
 * @param bridge Game Theory-Executive bridge
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_connect(
    game_theory_executive_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Disconnect bridge from cognitive hub
 *
 * @param bridge Game Theory-Executive bridge
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_disconnect(game_theory_executive_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Game Theory-Executive bridge
 * @return true if connected, false otherwise
 */
bool game_theory_executive_bridge_is_connected(const game_theory_executive_bridge_t* bridge);

/* ============================================================================
 * Strategy API
 * ============================================================================ */

/**
 * @brief Analyze strategic options
 *
 * @param bridge Game Theory-Executive bridge
 * @param num_actions Number of available actions
 * @param utilities Utility matrix (row-major, num_actions x num_outcomes)
 * @param num_outcomes Number of possible outcomes per action
 * @return 0 on success, -1 on error
 */
int game_theory_executive_analyze_options(
    game_theory_executive_bridge_t* bridge,
    uint32_t num_actions,
    const float* utilities,
    uint32_t num_outcomes
);

/**
 * @brief Get strategic recommendation
 *
 * @param bridge Game Theory-Executive bridge
 * @param recommendation_out Output recommendation
 * @return 0 on success, -1 on error
 */
int game_theory_executive_get_recommendation(
    game_theory_executive_bridge_t* bridge,
    gt_strategic_recommendation_t* recommendation_out
);

/**
 * @brief Notify of executive decision outcome
 *
 * @param bridge Game Theory-Executive bridge
 * @param outcome Decision outcome
 * @return 0 on success, -1 on error
 */
int game_theory_executive_notify_outcome(
    game_theory_executive_bridge_t* bridge,
    const gt_decision_outcome_t* outcome
);

/**
 * @brief Publish recommendation to hub
 *
 * @param bridge Game Theory-Executive bridge
 * @param recommendation Recommendation to publish
 * @return 0 on success, -1 on error
 */
int game_theory_executive_publish_recommendation(
    game_theory_executive_bridge_t* bridge,
    const gt_strategic_recommendation_t* recommendation
);

/* ============================================================================
 * Risk Assessment API
 * ============================================================================ */

/**
 * @brief Request risk assessment for an action
 *
 * WHAT: Assess strategic risks of a proposed executive action
 * WHY:  Help executive understand risks before committing
 * HOW:  Evaluate strategic, execution, and opportunity risks
 *
 * @param bridge Game Theory-Executive bridge
 * @param action_id Action identifier
 * @param context Action context (situation-specific, may be NULL)
 * @param assessment Output: Risk assessment result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int game_theory_executive_request_risk_assessment(
    game_theory_executive_bridge_t* bridge,
    uint64_t action_id,
    const void* context,
    gt_exec_risk_assessment_t* assessment
);

/* ============================================================================
 * Decision Notification API
 * ============================================================================ */

/**
 * @brief Notify game theory of executive decision
 *
 * WHAT: Inform game theory that executive made a decision
 * WHY:  Enable learning and strategy adjustment
 * HOW:  Update internal models based on decision
 *
 * @param bridge Game Theory-Executive bridge
 * @param decision_id Unique decision identifier
 * @param recommendation_id Associated recommendation (0 if none)
 * @param action_taken Action that was taken
 * @param followed_recommendation Whether recommendation was followed
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int game_theory_executive_notify_decision_made(
    game_theory_executive_bridge_t* bridge,
    uint64_t decision_id,
    uint64_t recommendation_id,
    uint32_t action_taken,
    bool followed_recommendation
);

/* ============================================================================
 * Opponent Modeling API
 * ============================================================================ */

/**
 * @brief Request opponent model for strategic planning
 *
 * WHAT: Get prediction model for an opponent/agent
 * WHY:  Enable executive to consider opponent behavior in planning
 * HOW:  Query or construct opponent behavior model
 *
 * @param bridge Game Theory-Executive bridge
 * @param opponent_id Opponent identifier
 * @param context Strategic context (may be NULL)
 * @param model Output: Opponent model
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int game_theory_executive_request_opponent_model(
    game_theory_executive_bridge_t* bridge,
    uint32_t opponent_id,
    const void* context,
    gt_exec_opponent_model_t* model
);

/**
 * @brief Update opponent model with new observation
 *
 * WHAT: Update opponent behavior model with observed action
 * WHY:  Improve prediction accuracy over time
 * HOW:  Bayesian update of strategy probabilities
 *
 * @param bridge Game Theory-Executive bridge
 * @param opponent_id Opponent identifier
 * @param observed_strategy Strategy index that was observed
 * @param outcome Outcome of the interaction
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int game_theory_executive_update_opponent_model(
    game_theory_executive_bridge_t* bridge,
    uint32_t opponent_id,
    uint32_t observed_strategy,
    float outcome
);

/* ============================================================================
 * Situation Analysis API
 * ============================================================================ */

/**
 * @brief Request strategic analysis for a situation
 *
 * WHAT: Ask game theory to analyze a decision situation
 * WHY:  Get strategic advice for executive decision making
 * HOW:  Analyze situation using game-theoretic methods
 *
 * @param bridge Game Theory-Executive bridge
 * @param situation Situation to analyze
 * @param recommendation Output: Strategic recommendation
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(actions * outcomes)
 * THREAD-SAFE: Yes
 */
int game_theory_executive_request_strategic_analysis(
    game_theory_executive_bridge_t* bridge,
    const gt_exec_situation_t* situation,
    gt_strategic_recommendation_t* recommendation
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 *
 * @param bridge Game Theory-Executive bridge
 * @return Current state
 */
game_theory_executive_state_t game_theory_executive_bridge_get_state(
    const game_theory_executive_bridge_t* bridge
);

/**
 * @brief Get module ID
 *
 * @param bridge Game Theory-Executive bridge
 * @return Module ID, or 0 on error
 */
uint32_t game_theory_executive_bridge_get_module_id(
    const game_theory_executive_bridge_t* bridge
);

/**
 * @brief Get pending recommendations count
 *
 * @param bridge Game Theory-Executive bridge
 * @return Number of pending recommendations
 */
uint32_t game_theory_executive_bridge_get_pending_count(
    const game_theory_executive_bridge_t* bridge
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Game Theory-Executive bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_get_stats(
    const game_theory_executive_bridge_t* bridge,
    game_theory_executive_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Game Theory-Executive bridge
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_reset_stats(game_theory_executive_bridge_t* bridge);

/**
 * @brief Force update bridge state
 *
 * @param bridge Game Theory-Executive bridge
 * @return 0 on success, -1 on error
 */
int game_theory_executive_bridge_force_update(game_theory_executive_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_EXECUTIVE_BRIDGE_H */
