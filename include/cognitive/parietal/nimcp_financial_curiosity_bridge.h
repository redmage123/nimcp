//=============================================================================
// nimcp_financial_curiosity_bridge.h - Financial Curiosity-Driven Hypothesis Bridge
//=============================================================================
/**
 * @file nimcp_financial_curiosity_bridge.h
 * @brief Curiosity-driven exploration and hypothesis generation for financial markets
 *
 * WHAT: Implements curiosity-driven hypothesis generation for financial analysis,
 *       using information gain optimization to identify promising market hypotheses
 *       worth exploring while balancing exploration cost against expected value.
 *
 * WHY:  Financial markets contain hidden patterns that systematic exploration can
 *       uncover. Curiosity-driven approaches:
 *       - Generate diverse hypotheses about market behavior
 *       - Prioritize exploration based on expected information gain
 *       - Balance novelty-seeking with exploitation of known patterns
 *       - Enable discovery of non-obvious trading opportunities
 *
 * HOW:  The bridge maintains a hypothesis generation engine that:
 *       1. Analyzes market state (prices, volumes, timestamps)
 *       2. Generates candidate hypotheses about market dynamics
 *       3. Estimates information gain for each hypothesis
 *       4. Computes exploration cost vs. expected value
 *       5. Selects optimal exploration strategy
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |            FINANCIAL CURIOSITY BRIDGE                             |
 * +------------------------------------------------------------------+
 * |                                                                   |
 * |   +-------------------+    +-------------------+                  |
 * |   |  MARKET STATE     |    |  HYPOTHESIS       |                  |
 * |   |  INPUT            |    |  GENERATOR        |                  |
 * |   |                   |    |                   |                  |
 * |   | prices[]          |    | Pattern detection |                  |
 * |   | volumes[]         |    | Regime hypotheses |                  |
 * |   | num_assets        |    | Correlation hyps  |                  |
 * |   | timestamp_ms      |    | Anomaly hypotheses|                  |
 * |   +--------+----------+    +--------+----------+                  |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +------------------------------------------+                    |
 * |   |     EXPLORATION CANDIDATE EVALUATION      |                   |
 * |   |                                          |                    |
 * |   | information_gain: I(Y; H) - mutual info  |                   |
 * |   | exploration_cost: C(explore H)           |                   |
 * |   | expected_value: E[V | H confirmed]       |                   |
 * |   +------------------------------------------+                    |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +-------------------+    +-------------------+                  |
 * |   | SELECTION ENGINE  |    | BBB / IMMUNE      |                  |
 * |   | UCB/Thompson      |    | VALIDATION        |                  |
 * |   | sampling          |    +-------------------+                  |
 * |   +-------------------+                                           |
 * |                                                                   |
 * +------------------------------------------------------------------+
 * ```
 *
 * THEORETICAL FOUNDATION:
 * =======================
 * Curiosity as Intrinsic Motivation (Schmidhuber, 2010):
 *   Reward(H) = InformationGain(H) - Cost(H)
 *
 * Information Gain:
 *   IG(H) = H(Y) - H(Y | H tested)
 *         = Expected reduction in market uncertainty
 *
 * Selection Strategy (UCB-style):
 *   Score(H) = ExpectedValue(H) + c * sqrt(2 * ln(N) / n_H)
 *
 * Where:
 * - H is a hypothesis about market behavior
 * - Y is future market outcomes
 * - c is exploration-exploitation trade-off parameter
 * - N is total exploration count
 * - n_H is times hypothesis H explored
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_CURIOSITY_BRIDGE_H
#define NIMCP_FINANCIAL_CURIOSITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module identifier for financial curiosity bridge */
#define BIO_MODULE_FINANCIAL_CURIOSITY       0x03A4

/** Maximum length of hypothesis description */
#define FIN_CURIOSITY_MAX_HYPOTHESIS_LEN     256

/** Maximum number of exploration candidates */
#define FIN_CURIOSITY_MAX_CANDIDATES         64

/** Maximum number of assets in market state */
#define FIN_CURIOSITY_MAX_ASSETS             1024

/** Maximum hypothesis types supported */
#define FIN_CURIOSITY_MAX_HYPOTHESIS_TYPES   16

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_CURIOSITY_ERROR_BASE             37000
#define FIN_CURIOSITY_ERR_OK                 0
#define FIN_CURIOSITY_ERR_NULL               (FIN_CURIOSITY_ERROR_BASE + 1)
#define FIN_CURIOSITY_ERR_INVALID_PARAM      (FIN_CURIOSITY_ERROR_BASE + 2)
#define FIN_CURIOSITY_ERR_NO_MEMORY          (FIN_CURIOSITY_ERROR_BASE + 3)
#define FIN_CURIOSITY_ERR_NOT_INITIALIZED    (FIN_CURIOSITY_ERROR_BASE + 4)
#define FIN_CURIOSITY_ERR_GENERATION         (FIN_CURIOSITY_ERROR_BASE + 5)
#define FIN_CURIOSITY_ERR_SUBSYSTEM          (FIN_CURIOSITY_ERROR_BASE + 6)
#define FIN_CURIOSITY_ERR_VALIDATION         (FIN_CURIOSITY_ERROR_BASE + 7)
#define FIN_CURIOSITY_ERR_NO_CANDIDATES      (FIN_CURIOSITY_ERROR_BASE + 8)
#define FIN_CURIOSITY_ERR_SELECTION          (FIN_CURIOSITY_ERROR_BASE + 9)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_CURIOSITY_STATE_UNINITIALIZED = 0,
    FIN_CURIOSITY_STATE_IDLE,
    FIN_CURIOSITY_STATE_GENERATING,
    FIN_CURIOSITY_STATE_SELECTING,
    FIN_CURIOSITY_STATE_EXPLORING,
    FIN_CURIOSITY_STATE_ERROR
} fin_curiosity_op_state_t;

/** Hypothesis types for market exploration */
typedef enum {
    FIN_HYPOTHESIS_NONE = 0,
    FIN_HYPOTHESIS_TREND,           /**< Trend continuation/reversal */
    FIN_HYPOTHESIS_MEAN_REVERSION,  /**< Price mean reversion */
    FIN_HYPOTHESIS_MOMENTUM,        /**< Momentum factor persistence */
    FIN_HYPOTHESIS_CORRELATION,     /**< Cross-asset correlation change */
    FIN_HYPOTHESIS_REGIME_CHANGE,   /**< Market regime transition */
    FIN_HYPOTHESIS_VOLATILITY,      /**< Volatility clustering/breakout */
    FIN_HYPOTHESIS_SEASONALITY,     /**< Calendar/seasonal effect */
    FIN_HYPOTHESIS_ANOMALY,         /**< Statistical anomaly */
    FIN_HYPOTHESIS_LIQUIDITY,       /**< Liquidity-driven move */
    FIN_HYPOTHESIS_SENTIMENT,       /**< Sentiment shift */
    FIN_HYPOTHESIS_CUSTOM,          /**< User-defined hypothesis */
    FIN_HYPOTHESIS_COUNT
} fin_hypothesis_type_t;

/** Exploration selection strategy */
typedef enum {
    FIN_SELECTION_UCB = 0,          /**< Upper Confidence Bound */
    FIN_SELECTION_THOMPSON,         /**< Thompson Sampling */
    FIN_SELECTION_GREEDY,           /**< Greedy (highest expected value) */
    FIN_SELECTION_EPSILON_GREEDY,   /**< Epsilon-greedy exploration */
    FIN_SELECTION_SOFTMAX,          /**< Softmax/Boltzmann selection */
    FIN_SELECTION_COUNT
} fin_selection_strategy_t;

//=============================================================================
// Data Structures (as specified by user)
//=============================================================================

/**
 * @brief Exploration candidate hypothesis
 *
 * Represents a hypothesis about market behavior that could be explored,
 * with associated information gain, cost, and expected value metrics.
 */
typedef struct {
    char hypothesis[256];           /**< Human-readable hypothesis description */
    float information_gain;         /**< Expected information gain [0-1] */
    float exploration_cost;         /**< Cost to test this hypothesis [0-1] */
    float expected_value;           /**< Expected value if confirmed */
} fin_exploration_candidate_t;

/**
 * @brief Market state snapshot for hypothesis generation
 *
 * Contains current market data used to generate exploration hypotheses.
 */
typedef struct {
    float* prices;                  /**< Current prices array [num_assets] */
    float* volumes;                 /**< Current volumes array [num_assets] */
    uint32_t num_assets;            /**< Number of assets in arrays */
    uint64_t timestamp_ms;          /**< Current timestamp in milliseconds */
} fin_market_state_t;

/**
 * @brief Bridge statistics for monitoring and diagnostics
 */
typedef struct {
    uint64_t hypotheses_generated;  /**< Total hypotheses generated */
    uint64_t explorations_selected; /**< Explorations selected */
    uint64_t immune_checks;         /**< Immune system validations */
    uint64_t bbb_validations;       /**< Blood-brain barrier validations */
    uint64_t kg_messages_sent;      /**< Knowledge graph messages sent */
    uint64_t health_heartbeats;     /**< Health agent heartbeats */
} fin_curiosity_bridge_stats_t;

//=============================================================================
// Extended Data Structures
//=============================================================================

/**
 * @brief Extended exploration candidate with metadata
 */
typedef struct {
    fin_exploration_candidate_t base;  /**< Base candidate data */
    fin_hypothesis_type_t type;        /**< Hypothesis type category */
    float confidence;                  /**< Confidence in hypothesis validity */
    float novelty;                     /**< Novelty score [0-1] */
    uint32_t exploration_count;        /**< Times this hypothesis explored */
    uint64_t last_explored_ms;         /**< Last exploration timestamp */
    float prior_success_rate;          /**< Historical success rate */
    uint32_t target_asset_idx;         /**< Primary asset index (-1 for market-wide) */
} fin_extended_candidate_t;

/**
 * @brief Hypothesis generation result
 */
typedef struct {
    fin_extended_candidate_t* candidates;  /**< Generated candidates array */
    uint32_t num_candidates;               /**< Number of candidates generated */
    uint32_t max_candidates;               /**< Capacity of candidates array */
    float total_information_gain;          /**< Sum of all candidate IG */
    float avg_exploration_cost;            /**< Average exploration cost */
    uint64_t generation_time_us;           /**< Generation time in microseconds */
} fin_hypothesis_result_t;

/**
 * @brief Selection result with rationale
 */
typedef struct {
    fin_extended_candidate_t selected;     /**< Selected candidate */
    uint32_t selected_index;               /**< Index in candidates array */
    float selection_score;                 /**< Score used for selection */
    float exploration_bonus;               /**< Exploration term contribution */
    float exploitation_score;              /**< Exploitation term contribution */
    char rationale[256];                   /**< Human-readable selection rationale */
} fin_selection_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration structure
 */
typedef struct {
    /* Hypothesis generation settings */
    uint32_t max_hypotheses_per_cycle;    /**< Max hypotheses to generate */
    float min_information_gain;           /**< Minimum IG threshold */
    float max_exploration_cost;           /**< Maximum acceptable cost */
    bool enable_all_hypothesis_types;     /**< Enable all hypothesis types */
    uint32_t enabled_types_mask;          /**< Bitmask of enabled types */

    /* Selection strategy settings */
    fin_selection_strategy_t strategy;    /**< Selection strategy */
    float exploration_coefficient;        /**< UCB exploration parameter c */
    float epsilon;                        /**< Epsilon for epsilon-greedy */
    float temperature;                    /**< Softmax temperature */

    /* Modulation sensitivity */
    float inflammation_sensitivity;       /**< Sensitivity to inflammation [0-2] */
    float fatigue_sensitivity;            /**< Sensitivity to fatigue [0-2] */
    float curiosity_boost;                /**< Base curiosity multiplier [0.5-2.0] */

    /* Security settings */
    bool enable_bbb_validation;           /**< Enable blood-brain barrier checks */
    bool enable_immune_validation;        /**< Enable immune system checks */
} fin_curiosity_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct financial_curiosity_bridge financial_curiosity_bridge_t;

//=============================================================================
// Forward Declarations for Subsystems
//=============================================================================

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration with sensible values
 */
fin_curiosity_config_t financial_curiosity_bridge_default_config(void);

/**
 * @brief Create financial curiosity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
financial_curiosity_bridge_t* financial_curiosity_bridge_create(
    const fin_curiosity_config_t* config);

/**
 * @brief Destroy bridge and free resources
 * @param bridge Bridge handle
 */
void financial_curiosity_bridge_destroy(financial_curiosity_bridge_t* bridge);

/**
 * @brief Get current bridge state
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_curiosity_op_state_t financial_curiosity_bridge_get_state(
    const financial_curiosity_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_reset(financial_curiosity_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/**
 * @brief Set immune system for validation
 * @param bridge Bridge handle
 * @param immune Immune system handle (NULL to disable)
 * @return 0 on success
 */
int financial_curiosity_bridge_set_immune(financial_curiosity_bridge_t* bridge,
                                           void* immune);

/**
 * @brief Set blood-brain barrier for data validation
 * @param bridge Bridge handle
 * @param bbb BBB handle (NULL to disable)
 * @return 0 on success
 */
int financial_curiosity_bridge_set_bbb(financial_curiosity_bridge_t* bridge,
                                        bbb_system_t bbb);

/**
 * @brief Enable/disable BBB validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_curiosity_bridge_enable_bbb_validation(
    financial_curiosity_bridge_t* bridge, bool enable);

/**
 * @brief Enable/disable immune validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_curiosity_bridge_enable_immune_validation(
    financial_curiosity_bridge_t* bridge, bool enable);

/**
 * @brief Set KG wiring for inter-module communication
 * @param bridge Bridge handle
 * @param kg KG wiring handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_kg_wiring(financial_curiosity_bridge_t* bridge,
                                              void* kg);

/**
 * @brief Set health agent for heartbeat monitoring
 * @param bridge Bridge handle
 * @param health_agent Health agent handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_health_agent(financial_curiosity_bridge_t* bridge,
                                                 void* health_agent);

/**
 * @brief Set logger for debug/trace output
 * @param bridge Bridge handle
 * @param logger Logger handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_logger(financial_curiosity_bridge_t* bridge,
                                           void* logger);

/**
 * @brief Set security handle
 * @param bridge Bridge handle
 * @param security Security handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_security(financial_curiosity_bridge_t* bridge,
                                             void* security);

/**
 * @brief Set ethics engine handle
 * @param bridge Bridge handle
 * @param ethics Ethics engine handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_ethics(financial_curiosity_bridge_t* bridge,
                                           ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 * @param bridge Bridge handle
 * @param lgss LGSS handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_lgss(financial_curiosity_bridge_t* bridge,
                                         const void* lgss);

/**
 * @brief Set cycle coordinator handle
 * @param bridge Bridge handle
 * @param coordinator Cycle coordinator handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_coordinator(financial_curiosity_bridge_t* bridge,
                                                brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 * @param bridge Bridge handle
 * @param bio_router Bio router handle
 * @return 0 on success
 */
int financial_curiosity_bridge_set_bio_router(financial_curiosity_bridge_t* bridge,
                                               void* bio_router);

//=============================================================================
// Core Curiosity API
//=============================================================================

/**
 * @brief Generate market hypotheses to test
 *
 * Analyzes current market state and generates candidate hypotheses
 * about market behavior that could be explored for information gain.
 *
 * @param bridge Bridge handle
 * @param market_state Current market state (prices, volumes, timestamp)
 * @param result Output hypothesis generation result (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_generate_hypotheses(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_hypothesis_result_t* result);

/**
 * @brief Select best hypothesis to explore
 *
 * Given generated candidates, selects the optimal hypothesis to explore
 * based on the configured selection strategy (UCB, Thompson, etc.).
 *
 * @param bridge Bridge handle
 * @param candidates Array of exploration candidates
 * @param num_candidates Number of candidates
 * @param selection Output selection result (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_select_exploration(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* candidates,
    uint32_t num_candidates,
    fin_selection_result_t* selection);

/**
 * @brief Combined generate and select (convenience function)
 *
 * Generates hypotheses from market state and immediately selects
 * the best one for exploration.
 *
 * @param bridge Bridge handle
 * @param market_state Current market state
 * @param selection Output selection result
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_explore(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_selection_result_t* selection);

/**
 * @brief Update hypothesis after exploration outcome
 *
 * Updates the bridge's internal state based on exploration outcome,
 * adjusting future selection probabilities.
 *
 * @param bridge Bridge handle
 * @param hypothesis The explored hypothesis
 * @param outcome_value Observed outcome value
 * @param was_confirmed Whether hypothesis was confirmed
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_update_outcome(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* hypothesis,
    float outcome_value,
    bool was_confirmed);

//=============================================================================
// Advanced Generation API
//=============================================================================

/**
 * @brief Generate hypotheses of specific type
 *
 * @param bridge Bridge handle
 * @param market_state Current market state
 * @param type Hypothesis type to generate
 * @param candidates Output candidates array (caller allocates)
 * @param max_candidates Maximum candidates to generate
 * @param num_generated Output: actual number generated
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_generate_typed(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    fin_hypothesis_type_t type,
    fin_extended_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_generated);

/**
 * @brief Generate cross-asset correlation hypotheses
 *
 * @param bridge Bridge handle
 * @param market_state Current market state
 * @param asset_i First asset index
 * @param asset_j Second asset index
 * @param candidate Output candidate
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_generate_correlation_hypothesis(
    financial_curiosity_bridge_t* bridge,
    const fin_market_state_t* market_state,
    uint32_t asset_i,
    uint32_t asset_j,
    fin_extended_candidate_t* candidate);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Allocate market state structure
 * @param num_assets Number of assets
 * @return Allocated structure or NULL on error
 */
fin_market_state_t* financial_curiosity_market_state_create(uint32_t num_assets);

/**
 * @brief Free market state structure
 * @param state State to free
 */
void financial_curiosity_market_state_destroy(fin_market_state_t* state);

/**
 * @brief Allocate hypothesis result structure
 * @param max_candidates Maximum candidates capacity
 * @return Allocated structure or NULL on error
 */
fin_hypothesis_result_t* financial_curiosity_result_create(uint32_t max_candidates);

/**
 * @brief Free hypothesis result structure
 * @param result Result to free
 */
void financial_curiosity_result_destroy(fin_hypothesis_result_t* result);

/**
 * @brief Compute information gain for hypothesis
 *
 * @param bridge Bridge handle
 * @param hypothesis Hypothesis to evaluate
 * @param market_state Current market state
 * @param information_gain Output information gain
 * @return 0 on success, error code on failure
 */
int financial_curiosity_bridge_compute_information_gain(
    financial_curiosity_bridge_t* bridge,
    const fin_extended_candidate_t* hypothesis,
    const fin_market_state_t* market_state,
    float* information_gain);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Set inflammation level (affects exploration conservatism)
 * @param bridge Bridge handle
 * @param level Inflammation level [0-1]
 * @return 0 on success
 */
int financial_curiosity_bridge_set_inflammation(
    financial_curiosity_bridge_t* bridge, float level);

/**
 * @brief Set fatigue level (affects generation quality)
 * @param bridge Bridge handle
 * @param level Fatigue level [0-1]
 * @return 0 on success
 */
int financial_curiosity_bridge_set_fatigue(
    financial_curiosity_bridge_t* bridge, float level);

/**
 * @brief Set curiosity boost multiplier
 * @param bridge Bridge handle
 * @param boost Curiosity boost [0.5-2.0]
 * @return 0 on success
 */
int financial_curiosity_bridge_set_curiosity_boost(
    financial_curiosity_bridge_t* bridge, float boost);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics (caller allocates)
 * @return 0 on success
 */
int financial_curiosity_bridge_get_stats(
    const financial_curiosity_bridge_t* bridge,
    fin_curiosity_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 * @param bridge Bridge handle
 */
void financial_curiosity_bridge_reset_stats(financial_curiosity_bridge_t* bridge);

/**
 * @brief Get last error message
 * @return Thread-local error message string
 */
const char* financial_curiosity_bridge_get_last_error(void);

//=============================================================================
// Health Integration
//=============================================================================

/**
 * @brief Set global health agent for module-level heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void financial_curiosity_bridge_set_health_agent_global(void* agent);

//=============================================================================
// String Conversion Utilities
//=============================================================================

/**
 * @brief Get string name for operational state
 * @param state Operational state
 * @return Static string name
 */
const char* fin_curiosity_state_name(fin_curiosity_op_state_t state);

/**
 * @brief Get string name for hypothesis type
 * @param type Hypothesis type
 * @return Static string name
 */
const char* fin_curiosity_hypothesis_type_name(fin_hypothesis_type_t type);

/**
 * @brief Get string name for selection strategy
 * @param strategy Selection strategy
 * @return Static string name
 */
const char* fin_curiosity_selection_strategy_name(fin_selection_strategy_t strategy);

/**
 * @brief Get bridge version string
 * @return Version string
 */
const char* financial_curiosity_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_CURIOSITY_BRIDGE_H */
