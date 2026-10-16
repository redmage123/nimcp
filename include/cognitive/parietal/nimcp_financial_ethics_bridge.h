/**
 * @file nimcp_financial_ethics_bridge.h
 * @brief Financial Ethics Bridge - Ethical Evaluation for Trading Decisions
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for evaluating the ethical implications of financial trading
 *       actions using multiple ethical frameworks including Asimov's Laws
 *       adapted for AI trading and the Golden Rule principle.
 *
 * WHY:  Autonomous trading systems must consider ethical implications of their
 *       actions beyond pure profit maximization. This bridge enables:
 *       - Assessment of potential harm to market participants
 *       - Detection of manipulative or predatory trading patterns
 *       - Enforcement of Asimov-inspired constraints for AI trading
 *       - Application of empathy-based ethical reasoning (Golden Rule)
 *       - Escalation of ethically ambiguous situations for human review
 *
 * HOW:  Trading actions are evaluated against multiple ethical frameworks:
 *       1. HARM ASSESSMENT: Estimates damage to counterparties, market integrity
 *       2. ASIMOV COMPLIANCE: Checks for violations of AI trading principles
 *       3. GOLDEN RULE TEST: Would you accept this action if targeted at you?
 *       4. EMPATHY SCORING: Models emotional impact on affected parties
 *       Results are aggregated into verdict (APPROVED/DENIED/ESCALATE/WARN).
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial Ethics Bridge                                 |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Trading Action        |       |    Ethical Frameworks     |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | action_type               |       | Asimov's Laws (adapted)   |        |
 * |  | action_magnitude          |       | Golden Rule Test          |        |
 * |  | target_asset              |       | Harm Calculus             |        |
 * |  | position_size             |       | Empathy Modeling          |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |              Ethics Evaluation Engine                     |            |
 * |  |  action -> harm_score, asimov_check, golden_rule_test    |            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Verdict Generation      |       |     Party Impact          |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | APPROVED / DENIED         |       | Affected counterparties   |        |
 * |  | ESCALATE / WARN           |       | Market impact assessment  |        |
 * |  | Reasoning provided        |       | Systemic risk evaluation  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * ASIMOV'S LAWS ADAPTED FOR AI TRADING:
 * 1. A trading AI shall not, through action or inaction, cause significant
 *    financial harm to individual market participants.
 * 2. A trading AI must operate within regulatory frameworks and market rules,
 *    except where such compliance would violate the First Law.
 * 3. A trading AI may pursue profit for its principals, as long as such
 *    pursuit does not conflict with the First or Second Laws.
 *
 * @see nimcp_financial_emotion_bridge.h
 * @see nimcp_financial_motivation_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_ETHICS_BRIDGE_H
#define NIMCP_FINANCIAL_ETHICS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_ETHICS_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_ETHICS_BRIDGE_MAGIC      0x46455442  /* 'FETB' */

/** Bio-async module ID for financial ethics bridge */
#define BIO_MODULE_FINANCIAL_ETHICS        0x039A

/** Maximum length for reason/description strings */
#define FIN_ETHICS_REASON_LEN              512

/** Maximum length for party description */
#define FIN_ETHICS_PARTY_LEN               256

/** Maximum length for asset identifier */
#define FIN_ETHICS_ASSET_LEN               32

/** Maximum length for context description */
#define FIN_ETHICS_CONTEXT_LEN             256

/** Number of Asimov laws */
#define FIN_ETHICS_ASIMOV_LAW_COUNT        3

/** Maximum violation records in history */
#define FIN_ETHICS_MAX_VIOLATIONS          256

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_ETHICS_ERROR_BASE              33300
#define FIN_ETHICS_ERR_OK                  0
#define FIN_ETHICS_ERR_NULL                (FIN_ETHICS_ERROR_BASE + 1)
#define FIN_ETHICS_ERR_INVALID_PARAM       (FIN_ETHICS_ERROR_BASE + 2)
#define FIN_ETHICS_ERR_NO_MEMORY           (FIN_ETHICS_ERROR_BASE + 3)
#define FIN_ETHICS_ERR_STATE               (FIN_ETHICS_ERROR_BASE + 4)
#define FIN_ETHICS_ERR_IMMUNE              (FIN_ETHICS_ERROR_BASE + 5)
#define FIN_ETHICS_ERR_BBB                 (FIN_ETHICS_ERROR_BASE + 6)
#define FIN_ETHICS_ERR_VALIDATION          (FIN_ETHICS_ERROR_BASE + 7)
#define FIN_ETHICS_ERR_ETHICS_VIOLATION    (FIN_ETHICS_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Ethics evaluation verdict
 */
typedef enum {
    FIN_ETHICS_APPROVED = 0,    /**< Action is ethically acceptable */
    FIN_ETHICS_DENIED,          /**< Action violates ethical principles */
    FIN_ETHICS_ESCALATE,        /**< Action requires human review */
    FIN_ETHICS_WARN             /**< Action is borderline, proceed with caution */
} fin_ethics_verdict_t;

/**
 * @brief Trading action types for ethical evaluation (guarded)
 */
#ifndef FIN_ACTION_TYPE_DEFINED
#define FIN_ACTION_TYPE_DEFINED
typedef enum {
    FIN_ACTION_BUY = 0,         /**< Buy/long position */
    FIN_ACTION_SELL,            /**< Sell/close long */
    FIN_ACTION_SHORT,           /**< Short sale */
    FIN_ACTION_COVER,           /**< Cover short position */
    FIN_ACTION_MARKET_MAKE,     /**< Market making activity */
    FIN_ACTION_ARBITRAGE,       /**< Arbitrage opportunity */
    FIN_ACTION_LIQUIDATE,       /**< Forced liquidation */
    FIN_ACTION_STOP_HUNT,       /**< Stop-loss hunting (flagged) */
    FIN_ACTION_MOMENTUM_IGNITE, /**< Momentum ignition (flagged) */
    FIN_ACTION_SPOOFING,        /**< Order spoofing (flagged) */
    FIN_ACTION_LAYERING,        /**< Layering (flagged) */
    FIN_ACTION_WASH_TRADE,      /**< Wash trading (flagged) */
    FIN_ACTION_COUNT
} fin_action_type_t;
#endif /* FIN_ACTION_TYPE_DEFINED */

/**
 * @brief Asimov law that was violated
 */
typedef enum {
    FIN_ASIMOV_NONE = 0,        /**< No violation */
    FIN_ASIMOV_FIRST_LAW,       /**< Harm to market participants */
    FIN_ASIMOV_SECOND_LAW,      /**< Regulatory/rules violation */
    FIN_ASIMOV_THIRD_LAW        /**< Self-interest over ethics */
} fin_asimov_law_t;

/**
 * @brief Affected party type
 */
typedef enum {
    FIN_PARTY_RETAIL = 0,       /**< Retail investors */
    FIN_PARTY_INSTITUTIONAL,    /**< Institutional investors */
    FIN_PARTY_MARKET_MAKER,     /**< Market makers */
    FIN_PARTY_ISSUER,           /**< Security issuers */
    FIN_PARTY_REGULATOR,        /**< Regulatory bodies */
    FIN_PARTY_MARKET_INTEGRITY, /**< Market as a whole */
    FIN_PARTY_PRINCIPAL,        /**< Trading principal/client */
    FIN_PARTY_COUNT
} fin_party_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_ETHICS_STATE_UNINITIALIZED = 0,
    FIN_ETHICS_STATE_INITIALIZED,
    FIN_ETHICS_STATE_ACTIVE,
    FIN_ETHICS_STATE_DEGRADED,
    FIN_ETHICS_STATE_ERROR
} fin_ethics_bridge_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Trading action for ethical evaluation
 */
typedef struct {
    int action_type;                        /**< Action type (fin_action_type_t) */
    float action_magnitude;                 /**< Magnitude/aggression [0,1] */
    char target_asset[FIN_ETHICS_ASSET_LEN]; /**< Target asset identifier */
    float position_size;                    /**< Position size in base currency */
    char context[FIN_ETHICS_CONTEXT_LEN];   /**< Additional context */
} fin_ethics_action_t;

/**
 * @brief Result of ethics evaluation
 */
typedef struct {
    fin_ethics_verdict_t verdict;           /**< Final verdict */
    char reason[FIN_ETHICS_REASON_LEN];     /**< Human-readable explanation */
    float harm_score;                       /**< Estimated harm [0,1] */
    char affected_parties[FIN_ETHICS_PARTY_LEN]; /**< Description of affected parties */
    bool asimov_violation;                  /**< True if any Asimov law violated */
    bool golden_rule_violation;             /**< True if fails Golden Rule test */
    float empathy_score;                    /**< Empathy/fairness score [0,1] */
} fin_ethics_result_t;

/**
 * @brief Detailed harm assessment
 */
typedef struct {
    float total_harm;                       /**< Aggregate harm score [0,1] */
    float counterparty_harm;                /**< Harm to direct counterparties */
    float market_harm;                      /**< Harm to market integrity */
    float systemic_harm;                    /**< Systemic risk contribution */
    float principal_risk;                   /**< Risk to trading principal */

    /* Per-party harm breakdown */
    float party_harm[FIN_PARTY_COUNT];      /**< Harm by party type */

    /* Impact description */
    char impact_description[FIN_ETHICS_REASON_LEN];
} fin_harm_assessment_t;

/**
 * @brief Golden Rule test result
 */
typedef struct {
    bool passes;                            /**< True if passes Golden Rule */
    float reciprocity_score;                /**< How fair if roles reversed [0,1] */
    float fairness_score;                   /**< Overall fairness [0,1] */
    char explanation[FIN_ETHICS_REASON_LEN]; /**< Why it passes/fails */
} fin_golden_rule_result_t;

/**
 * @brief Asimov compliance check result
 */
typedef struct {
    bool compliant;                         /**< True if all laws satisfied */
    bool law_violations[FIN_ETHICS_ASIMOV_LAW_COUNT]; /**< Per-law violations */
    fin_asimov_law_t primary_violation;     /**< Most severe violation */
    float severity;                         /**< Violation severity [0,1] */
    char explanation[FIN_ETHICS_REASON_LEN]; /**< Violation explanation */
} fin_asimov_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t evaluations;                   /**< Total evaluations performed */
    uint64_t approvals;                     /**< Actions approved */
    uint64_t denials;                       /**< Actions denied */
    uint64_t escalations;                   /**< Actions escalated */
    uint64_t warnings;                      /**< Warnings issued */
    uint64_t immune_checks;                 /**< Immune system checks */
    uint64_t bbb_validations;               /**< BBB validations performed */
    uint64_t kg_messages_sent;              /**< KG messages published */
    uint64_t health_heartbeats;             /**< Health heartbeats sent */
} fin_ethics_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Harm thresholds */
    float harm_deny_threshold;              /**< Harm level for automatic denial */
    float harm_escalate_threshold;          /**< Harm level for escalation */
    float harm_warn_threshold;              /**< Harm level for warning */

    /* Asimov enforcement */
    bool enforce_first_law;                 /**< Enforce no-harm principle */
    bool enforce_second_law;                /**< Enforce regulatory compliance */
    bool enforce_third_law;                 /**< Enforce ethical self-interest */
    float asimov_violation_threshold;       /**< Severity threshold for denial */

    /* Golden Rule settings */
    bool enable_golden_rule;                /**< Enable Golden Rule evaluation */
    float reciprocity_threshold;            /**< Minimum reciprocity score */

    /* Empathy modeling */
    bool enable_empathy_modeling;           /**< Enable empathy score computation */
    float empathy_weight;                   /**< Weight of empathy in verdict */

    /* Auto-flag suspicious actions */
    bool auto_flag_stop_hunt;               /**< Auto-flag stop hunting */
    bool auto_flag_momentum_ignite;         /**< Auto-flag momentum ignition */
    bool auto_flag_spoofing;                /**< Auto-flag spoofing */
    bool auto_flag_layering;                /**< Auto-flag layering */
    bool auto_flag_wash_trade;              /**< Auto-flag wash trading */

    /* Integration settings */
    bool enable_immune_integration;         /**< Enable immune system */
    bool enable_bbb_validation;             /**< Enable BBB validation */
    bool enable_kg_messaging;               /**< Enable KG messaging */
    bool enable_health_monitoring;          /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                   /**< Verbose debug output */
} fin_ethics_config_t;

/* ============================================================================
 * Forward Declarations for Security Subsystems
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
 * @brief Opaque financial ethics bridge handle
 */
typedef struct financial_ethics_bridge financial_ethics_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_ethics_bridge_default_config(fin_ethics_config_t* config);

/**
 * @brief Create financial ethics bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_ethics_bridge_t* financial_ethics_bridge_create(
    const fin_ethics_config_t* config
);

/**
 * @brief Destroy financial ethics bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_ethics_bridge_destroy(financial_ethics_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_reset(financial_ethics_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_ethics_bridge_set_immune(financial_ethics_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_ethics_bridge_set_bbb(financial_ethics_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_ethics_bridge_set_health_agent(financial_ethics_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_ethics_bridge_set_kg_wiring(financial_ethics_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_ethics_bridge_set_logger(financial_ethics_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_ethics_bridge_set_security(financial_ethics_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_ethics_bridge_set_ethics(financial_ethics_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_ethics_bridge_set_lgss(financial_ethics_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_ethics_bridge_set_coordinator(financial_ethics_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_ethics_bridge_set_bio_router(financial_ethics_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Ethics API
 * ============================================================================ */

/**
 * @brief Evaluate the ethics of a trading action
 *
 * Performs comprehensive ethical evaluation using all enabled frameworks
 * (harm assessment, Asimov compliance, Golden Rule, empathy modeling).
 *
 * @param bridge Bridge handle
 * @param action Trading action to evaluate
 * @param result Output evaluation result
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_evaluate(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_ethics_result_t* result
);

/**
 * @brief Assess potential harm from a trading action
 *
 * Detailed harm assessment including per-party breakdown and
 * systemic risk evaluation.
 *
 * @param bridge Bridge handle
 * @param action Trading action to assess
 * @param assessment Output harm assessment
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_assess_harm(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_harm_assessment_t* assessment
);

/**
 * @brief Apply Golden Rule test to a trading action
 *
 * Evaluates whether the action would be acceptable if the roles
 * were reversed (would you accept this action if targeted at you?).
 *
 * @param bridge Bridge handle
 * @param action Trading action to test
 * @param result Output Golden Rule result
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_golden_rule(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_golden_rule_result_t* result
);

/**
 * @brief Check Asimov law compliance
 *
 * Evaluates the action against the three Asimov laws adapted for
 * AI trading systems.
 *
 * @param bridge Bridge handle
 * @param action Trading action to check
 * @param result Output Asimov compliance result
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_check_asimov(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    fin_asimov_result_t* result
);

/**
 * @brief Compute empathy score for action
 *
 * Models the emotional/psychological impact on affected parties.
 *
 * @param bridge Bridge handle
 * @param action Trading action to evaluate
 * @param empathy_score Output empathy score [0,1]
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_compute_empathy(
    financial_ethics_bridge_t* bridge,
    const fin_ethics_action_t* action,
    float* empathy_score
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_ethics_bridge_state_t financial_ethics_bridge_get_bridge_state(
    const financial_ethics_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_get_stats(
    const financial_ethics_bridge_t* bridge,
    fin_ethics_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_ethics_bridge_reset_stats(financial_ethics_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_ethics_bridge_get_last_error(void);

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
int financial_ethics_bridge_heartbeat(
    financial_ethics_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get verdict name
 *
 * @param verdict Verdict type
 * @return String name (static)
 */
const char* fin_ethics_verdict_name(fin_ethics_verdict_t verdict);

/**
 * @brief Get action type name
 *
 * @param action_type Action type
 * @return String name (static)
 */
const char* fin_ethics_action_name(fin_action_type_t action_type);

/**
 * @brief Get Asimov law name
 *
 * @param law Asimov law
 * @return String name (static)
 */
const char* fin_ethics_asimov_name(fin_asimov_law_t law);

/**
 * @brief Get party type name
 *
 * @param party Party type
 * @return String name (static)
 */
const char* fin_ethics_party_name(fin_party_type_t party);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_ethics_state_name(fin_ethics_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_ethics_bridge_version(void);

/**
 * @brief Check if action type is automatically flagged
 *
 * @param bridge Bridge handle
 * @param action_type Action type to check
 * @return true if action type is auto-flagged
 */
bool financial_ethics_bridge_is_auto_flagged(
    const financial_ethics_bridge_t* bridge,
    fin_action_type_t action_type
);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_training_begin(financial_ethics_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_training_end(financial_ethics_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_ethics_bridge_training_step(financial_ethics_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_ETHICS_BRIDGE_H */
