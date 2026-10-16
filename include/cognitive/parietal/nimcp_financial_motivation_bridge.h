/**
 * @file nimcp_financial_motivation_bridge.h
 * @brief Financial Motivation Bridge - Nucleus Accumbens Integration
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling motivational signals in financial decision-making,
 *       separating wanting (incentive salience) from liking (hedonic value)
 *       following the nucleus accumbens dopaminergic model.
 *
 * WHY:  Financial decisions are driven by both rational analysis and
 *       motivational/emotional impulses. The nucleus accumbens mediates:
 *       - WANTING: Anticipatory desire for rewards (FOMO, urgency)
 *       - LIKING:  Actual hedonic enjoyment of outcomes
 *       - LEARNING: Reward prediction errors that shape future behavior
 *       By separating these signals, we can detect when irrational wanting
 *       (e.g., FOMO) overrides rational evaluation and apply appropriate
 *       override mechanisms.
 *
 * HOW:  Opportunities are evaluated to produce motivation signals. FOMO
 *       detection analyzes wanting relative to rational value. Override
 *       decisions compare incentive salience against computed rational
 *       expected value to recommend intervention when emotion dominates.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Motivation Bridge (Nucleus Accumbens)              |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Market Opportunity   |       |  Rational Analyzer    |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | expected_return       |       | fundamental_value     |                |
 * |  | risk_level            |       | risk_adjusted_return  |                |
 * |  | novelty               |       | opportunity_cost      |                |
 * |  | urgency               |       | position_sizing       |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Motivation Signal Generator                      |            |
 * |  |  opportunity -> (wanting, liking, learning)              |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          FOMO Detection & Override Decision               |            |
 * |  |  wanting >> rational_value -> recommend_override         |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * NUCLEUS ACCUMBENS MODEL:
 * - Shell: Processes reward prediction, novelty, salience attribution
 * - Core:  Action selection, approach/avoidance, motor output gating
 * - Dopamine bursts encode reward prediction errors (learning signal)
 * - Opioid system mediates hedonic "liking" responses
 * - Integration with prefrontal cortex enables rational override
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_parietal_training_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_MOTIVATION_BRIDGE_H
#define NIMCP_FINANCIAL_MOTIVATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_MOTIVATION_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_MOTIVATION_BRIDGE_MAGIC      0x464D4F54  /* 'FMOT' */

/** Bio-async module ID for financial motivation bridge */
#define BIO_MODULE_FINANCIAL_MOTIVATION        0x0398

/** Maximum history entries for learning signal computation */
#define FIN_MOTIVATION_MAX_HISTORY             256

/** Maximum description/label length */
#define FIN_MOTIVATION_DESC_LEN                128

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_MOTIVATION_ERROR_BASE              33200
#define FIN_MOTIVATION_ERR_OK                  0
#define FIN_MOTIVATION_ERR_NULL                (FIN_MOTIVATION_ERROR_BASE + 1)
#define FIN_MOTIVATION_ERR_INVALID_PARAM       (FIN_MOTIVATION_ERROR_BASE + 2)
#define FIN_MOTIVATION_ERR_NO_MEMORY           (FIN_MOTIVATION_ERROR_BASE + 3)
#define FIN_MOTIVATION_ERR_STATE               (FIN_MOTIVATION_ERROR_BASE + 4)
#define FIN_MOTIVATION_ERR_IMMUNE              (FIN_MOTIVATION_ERROR_BASE + 5)
#define FIN_MOTIVATION_ERR_BBB                 (FIN_MOTIVATION_ERROR_BASE + 6)
#define FIN_MOTIVATION_ERR_SUBSYSTEM           (FIN_MOTIVATION_ERROR_BASE + 7)
#define FIN_MOTIVATION_ERR_COMPUTE             (FIN_MOTIVATION_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_MOTIVATION_STATE_UNINITIALIZED = 0,
    FIN_MOTIVATION_STATE_INITIALIZED,
    FIN_MOTIVATION_STATE_ACTIVE,
    FIN_MOTIVATION_STATE_DEGRADED,
    FIN_MOTIVATION_STATE_ERROR
} fin_motivation_state_t;

/**
 * @brief Override recommendation levels
 */
typedef enum {
    FIN_OVERRIDE_NONE = 0,          /**< No override needed - proceed */
    FIN_OVERRIDE_CAUTION,           /**< Mild FOMO detected - add delay */
    FIN_OVERRIDE_REVIEW,            /**< Significant FOMO - require review */
    FIN_OVERRIDE_BLOCK,             /**< Strong FOMO - block action */
    FIN_OVERRIDE_COUNT
} fin_override_level_t;

/**
 * @brief FOMO intensity levels
 */
typedef enum {
    FIN_FOMO_NONE = 0,              /**< No FOMO detected */
    FIN_FOMO_MILD,                  /**< Slight urgency bias */
    FIN_FOMO_MODERATE,              /**< Noticeable fear of missing out */
    FIN_FOMO_STRONG,                /**< Strong irrational desire */
    FIN_FOMO_EXTREME,               /**< Panic-level FOMO */
    FIN_FOMO_COUNT
} fin_fomo_level_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Motivation signal triplet (wanting/liking/learning)
 *
 * Based on the nucleus accumbens dopaminergic model:
 * - Wanting (incentive salience): Anticipatory motivation, FOMO, desire to act
 * - Liking (hedonic value): Actual enjoyment/satisfaction from outcomes
 * - Learning (prediction error): Signal that updates future expectations
 *
 * All values normalized to [-1.0, 1.0] range.
 */
typedef struct {
    float wanting;    /**< Incentive salience - FOMO, desire to trade [-1,1] */
    float liking;     /**< Hedonic value - actual enjoyment/satisfaction [-1,1] */
    float learning;   /**< Prediction error signal for RL update [-1,1] */
} fin_motivation_signal_t;

/**
 * @brief Financial opportunity descriptor
 *
 * Describes a trading opportunity for motivation evaluation.
 */
typedef struct {
    float expected_return;          /**< Expected return (e.g., 0.05 = 5%) */
    float risk_level;               /**< Risk level [0.0-1.0] */
    float novelty;                  /**< Novelty/uniqueness [0.0-1.0] */
    float urgency;                  /**< Time pressure [0.0-1.0] */
    uint64_t timestamp_ms;          /**< Opportunity timestamp */
} fin_opportunity_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t evaluations;           /**< Total opportunity evaluations */
    uint64_t fomo_detections;       /**< FOMO detection events */
    uint64_t rational_overrides;    /**< Rational override recommendations */
    uint64_t immune_checks;         /**< Immune system checks performed */
    uint64_t bbb_validations;       /**< BBB validations performed */
    uint64_t kg_messages_sent;      /**< KG messages published */
    uint64_t health_heartbeats;     /**< Health heartbeats sent */
} fin_motivation_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief FOMO detection result
 */
typedef struct {
    fin_fomo_level_t level;         /**< Detected FOMO intensity */
    float wanting_excess;           /**< Wanting above rational baseline */
    float urgency_bias;             /**< Urgency-induced bias */
    float novelty_bias;             /**< Novelty-seeking bias */
    float herd_signal;              /**< Social/herd influence estimate */
    char trigger[FIN_MOTIVATION_DESC_LEN]; /**< Primary trigger description */
} fin_fomo_result_t;

/**
 * @brief Rational value computation result
 */
typedef struct {
    float expected_value;           /**< Risk-adjusted expected value */
    float opportunity_cost;         /**< Cost of alternative opportunities */
    float kelly_fraction;           /**< Optimal position size (Kelly) */
    float confidence;               /**< Confidence in estimate [0.0-1.0] */
    char rationale[FIN_MOTIVATION_DESC_LEN]; /**< Brief rationale */
} fin_rational_value_t;

/**
 * @brief Override decision result
 */
typedef struct {
    fin_override_level_t level;     /**< Recommended override level */
    float wanting_rational_gap;     /**< Gap between wanting and rational */
    float override_confidence;      /**< Confidence in recommendation */
    uint32_t delay_ms;              /**< Recommended delay before action */
    char reason[FIN_MOTIVATION_DESC_LEN]; /**< Override reason */
} fin_override_result_t;

/**
 * @brief Outcome feedback for learning signal update
 */
typedef struct {
    uint64_t opportunity_id;        /**< Original opportunity ID */
    float actual_return;            /**< Realized return */
    float satisfaction;             /**< Subjective satisfaction [-1,1] */
    uint64_t timestamp_ms;          /**< Outcome timestamp */
} fin_outcome_feedback_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Motivation signal parameters */
    float wanting_sensitivity;      /**< Sensitivity to reward cues [0.0-2.0] */
    float novelty_weight;           /**< Weight of novelty in wanting [0.0-1.0] */
    float urgency_weight;           /**< Weight of urgency in wanting [0.0-1.0] */
    float risk_aversion;            /**< Risk aversion coefficient [0.0-2.0] */

    /* FOMO detection thresholds */
    float fomo_mild_threshold;      /**< Wanting excess for mild FOMO */
    float fomo_moderate_threshold;  /**< Wanting excess for moderate FOMO */
    float fomo_strong_threshold;    /**< Wanting excess for strong FOMO */
    float fomo_extreme_threshold;   /**< Wanting excess for extreme FOMO */

    /* Override parameters */
    float override_caution_gap;     /**< Gap threshold for caution */
    float override_review_gap;      /**< Gap threshold for review */
    float override_block_gap;       /**< Gap threshold for block */
    uint32_t caution_delay_ms;      /**< Delay for caution override */
    uint32_t review_delay_ms;       /**< Delay for review override */

    /* Learning parameters */
    float learning_rate;            /**< RL learning rate [0.0-1.0] */
    float discount_factor;          /**< Temporal discount [0.0-1.0] */

    /* Integration settings */
    bool enable_immune_integration; /**< Enable immune system checks */
    bool enable_bbb_validation;     /**< Enable BBB validation */
    bool enable_kg_messaging;       /**< Enable KG messaging */
    bool enable_health_monitoring;  /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;           /**< Verbose debug output */
} fin_motivation_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief FOMO detected callback
 */
typedef void (*fin_motivation_fomo_callback_t)(
    const fin_opportunity_t* opportunity,
    const fin_fomo_result_t* fomo,
    void* user_data
);

/**
 * @brief Override recommended callback
 */
typedef void (*fin_motivation_override_callback_t)(
    const fin_opportunity_t* opportunity,
    const fin_override_result_t* override,
    void* user_data
);

/* ============================================================================
 * Forward Declarations for Subsystems
 * ============================================================================ */

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

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial motivation bridge handle
 */
typedef struct financial_motivation_bridge financial_motivation_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_default_config(fin_motivation_config_t* config);

/**
 * @brief Create financial motivation bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_motivation_bridge_t* financial_motivation_bridge_create(
    const fin_motivation_config_t* config
);

/**
 * @brief Destroy financial motivation bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_motivation_bridge_destroy(financial_motivation_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_reset(financial_motivation_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_motivation_bridge_set_immune(
    financial_motivation_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_motivation_bridge_set_bbb(
    financial_motivation_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_motivation_bridge_set_health_agent(
    financial_motivation_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_motivation_bridge_set_kg_wiring(
    financial_motivation_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_motivation_bridge_set_logger(
    financial_motivation_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_motivation_bridge_set_security(
    financial_motivation_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_motivation_bridge_set_ethics(
    financial_motivation_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_motivation_bridge_set_lgss(
    financial_motivation_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_motivation_bridge_set_coordinator(
    financial_motivation_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_motivation_bridge_set_bio_router(
    financial_motivation_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Motivation Evaluation
 * ============================================================================ */

/**
 * @brief Evaluate motivation signals for an opportunity
 *
 * Computes the wanting/liking/learning triplet for a given opportunity.
 *
 * @param bridge Bridge handle
 * @param opportunity Opportunity to evaluate
 * @param out_signal Output motivation signal
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_evaluate(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    fin_motivation_signal_t* out_signal
);

/**
 * @brief Detect FOMO in motivation state
 *
 * Analyzes the motivation signal for signs of fear of missing out.
 *
 * @param bridge Bridge handle
 * @param opportunity The opportunity being evaluated
 * @param signal Current motivation signal
 * @param out_fomo Output FOMO detection result
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_detect_fomo(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    const fin_motivation_signal_t* signal,
    fin_fomo_result_t* out_fomo
);

/**
 * @brief Compute rational value for an opportunity
 *
 * Calculates the rational expected value independent of emotional state.
 *
 * @param bridge Bridge handle
 * @param opportunity Opportunity to evaluate
 * @param out_rational Output rational value
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_rational_value(
    financial_motivation_bridge_t* bridge,
    const fin_opportunity_t* opportunity,
    fin_rational_value_t* out_rational
);

/**
 * @brief Determine if rational override is needed
 *
 * Compares wanting (emotional motivation) against rational value
 * to recommend appropriate override action.
 *
 * @param bridge Bridge handle
 * @param signal Current motivation signal
 * @param rational Computed rational value
 * @param out_override Output override decision
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_should_override(
    financial_motivation_bridge_t* bridge,
    const fin_motivation_signal_t* signal,
    const fin_rational_value_t* rational,
    fin_override_result_t* out_override
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Process outcome feedback for learning
 *
 * Updates internal models based on actual outcome vs expectation.
 *
 * @param bridge Bridge handle
 * @param feedback Outcome feedback
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_process_outcome(
    financial_motivation_bridge_t* bridge,
    const fin_outcome_feedback_t* feedback
);

/**
 * @brief Get current prediction error estimate
 *
 * @param bridge Bridge handle
 * @param out_prediction_error Output prediction error
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_get_prediction_error(
    financial_motivation_bridge_t* bridge,
    float* out_prediction_error
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set FOMO detected callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_set_fomo_callback(
    financial_motivation_bridge_t* bridge,
    fin_motivation_fomo_callback_t callback,
    void* user_data
);

/**
 * @brief Set override recommended callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_set_override_callback(
    financial_motivation_bridge_t* bridge,
    fin_motivation_override_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_motivation_state_t financial_motivation_bridge_get_state(
    const financial_motivation_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_get_stats(
    const financial_motivation_bridge_t* bridge,
    fin_motivation_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_motivation_bridge_reset_stats(financial_motivation_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_motivation_bridge_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param bridge Bridge handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_motivation_bridge_heartbeat(
    financial_motivation_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_motivation_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_motivation_state_name(fin_motivation_state_t state);

/**
 * @brief Get override level name
 *
 * @param level Override level
 * @return String name (static)
 */
const char* fin_motivation_override_name(fin_override_level_t level);

/**
 * @brief Get FOMO level name
 *
 * @param level FOMO level
 * @return String name (static)
 */
const char* fin_motivation_fomo_name(fin_fomo_level_t level);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_motivation_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_MOTIVATION_BRIDGE_H */
