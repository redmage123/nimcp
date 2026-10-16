/**
 * @file nimcp_financial_explanations_bridge.h
 * @brief Financial Explanations Bridge - Explainable AI for Trading Decisions
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for generating human-readable explanations of financial trading
 *       decisions and creating comprehensive audit trails for regulatory compliance.
 *
 * WHY:  Regulatory frameworks (MiFID II, SEC Rule 15c3-5, etc.) increasingly require
 *       explainability in algorithmic trading. This bridge provides:
 *       - Clear reasoning chains for every trading decision
 *       - Audit trails meeting regulatory documentation requirements
 *       - Confidence scores and uncertainty quantification
 *       - Linkage between decisions and supporting evidence
 *
 * HOW:  Trading decisions are analyzed through multiple explanation layers:
 *       1. SUMMARY: One-line human-readable description of the decision
 *       2. REASONING STEPS: Chain-of-thought breakdown of decision logic
 *       3. CONFIDENCE: Statistical confidence in the decision
 *       4. REGULATORY NOTES: Compliance considerations and disclosures
 *       Audit trails capture full decision context for post-hoc analysis.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial Explanations Bridge                          |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Trading Decision      |       |    Explanation Engine     |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | decision_type             |       | Summary Generation        |        |
 * |  | magnitude                 |       | Reasoning Chain Builder   |        |
 * |  | asset                     |       | Confidence Estimation     |        |
 * |  | rationale                 |       | Regulatory Note Generator |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |              Explanation Generation Pipeline              |            |
 * |  |  decision -> summary, steps[], confidence, reg_notes     |            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Audit Trail Creation    |       |     Compliance Output     |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Full decision context     |       | MiFID II compatible       |        |
 * |  | Timestamp + sequence      |       | SEC Rule 15c3-5 ready     |        |
 * |  | Input/output logging      |       | Human-reviewable format   |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_ethics_bridge.h
 * @see nimcp_financial_reasoning_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_EXPLANATIONS_BRIDGE_H
#define NIMCP_FINANCIAL_EXPLANATIONS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_EXPLANATIONS_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_EXPLANATIONS_BRIDGE_MAGIC      0x46455842  /* 'FEXB' */

/** Bio-async module ID for financial explanations bridge */
#define BIO_MODULE_FINANCIAL_EXPLANATIONS        0x039B

/** Maximum length for summary string */
#define FIN_EXPL_SUMMARY_LEN              1024

/** Maximum length for regulatory note */
#define FIN_EXPL_REGULATORY_LEN           512

/** Maximum length for rationale string */
#define FIN_EXPL_RATIONALE_LEN            256

/** Maximum length for asset identifier */
#define FIN_EXPL_ASSET_LEN                32

/** Maximum length for a single reasoning step */
#define FIN_EXPL_STEP_LEN                 256

/** Maximum number of reasoning steps */
#define FIN_EXPL_MAX_STEPS                32

/** Maximum length for audit trail entries */
#define FIN_EXPL_AUDIT_ENTRY_LEN          2048

/** Maximum audit trail history size */
#define FIN_EXPL_MAX_AUDIT_ENTRIES        1024

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_EXPL_ERROR_BASE              33400
#define FIN_EXPL_ERR_OK                  0
#define FIN_EXPL_ERR_NULL                (FIN_EXPL_ERROR_BASE + 1)
#define FIN_EXPL_ERR_INVALID_PARAM       (FIN_EXPL_ERROR_BASE + 2)
#define FIN_EXPL_ERR_NO_MEMORY           (FIN_EXPL_ERROR_BASE + 3)
#define FIN_EXPL_ERR_STATE               (FIN_EXPL_ERROR_BASE + 4)
#define FIN_EXPL_ERR_IMMUNE              (FIN_EXPL_ERROR_BASE + 5)
#define FIN_EXPL_ERR_BBB                 (FIN_EXPL_ERROR_BASE + 6)
#define FIN_EXPL_ERR_VALIDATION          (FIN_EXPL_ERROR_BASE + 7)
#define FIN_EXPL_ERR_EXPLANATION_FAILED  (FIN_EXPL_ERROR_BASE + 8)
#define FIN_EXPL_ERR_AUDIT_FAILED        (FIN_EXPL_ERROR_BASE + 9)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Decision types for explanation
 */
typedef enum {
    FIN_DECISION_BUY = 0,           /**< Buy/long position */
    FIN_DECISION_SELL,              /**< Sell/close long */
    FIN_DECISION_HOLD,              /**< Hold current position */
    FIN_DECISION_SHORT,             /**< Short sale */
    FIN_DECISION_COVER,             /**< Cover short position */
    FIN_DECISION_REBALANCE,         /**< Portfolio rebalancing */
    FIN_DECISION_HEDGE,             /**< Hedging operation */
    FIN_DECISION_EXIT,              /**< Exit all positions */
    FIN_DECISION_SCALE_IN,          /**< Scale into position */
    FIN_DECISION_SCALE_OUT,         /**< Scale out of position */
    FIN_DECISION_STOP_LOSS,         /**< Stop loss triggered */
    FIN_DECISION_TAKE_PROFIT,       /**< Take profit triggered */
    FIN_DECISION_COUNT
} fin_decision_type_t;

/**
 * @brief Explanation detail level
 */
typedef enum {
    FIN_EXPL_LEVEL_BRIEF = 0,       /**< One-line summary only */
    FIN_EXPL_LEVEL_STANDARD,        /**< Summary + key reasoning */
    FIN_EXPL_LEVEL_DETAILED,        /**< Full reasoning chain */
    FIN_EXPL_LEVEL_REGULATORY       /**< Maximum detail for compliance */
} fin_explanation_level_t;

/**
 * @brief Audit trail type
 */
typedef enum {
    FIN_AUDIT_DECISION = 0,         /**< Trading decision audit */
    FIN_AUDIT_ORDER,                /**< Order submission audit */
    FIN_AUDIT_EXECUTION,            /**< Execution audit */
    FIN_AUDIT_CANCELLATION,         /**< Order cancellation audit */
    FIN_AUDIT_MODIFICATION,         /**< Order modification audit */
    FIN_AUDIT_RISK_CHECK,           /**< Risk check audit */
    FIN_AUDIT_COMPLIANCE            /**< Compliance check audit */
} fin_audit_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_EXPL_STATE_UNINITIALIZED = 0,
    FIN_EXPL_STATE_INITIALIZED,
    FIN_EXPL_STATE_ACTIVE,
    FIN_EXPL_STATE_DEGRADED,
    FIN_EXPL_STATE_ERROR
} fin_explanations_bridge_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Trading decision record for explanation
 */
typedef struct {
    int decision_type;                      /**< Decision type (fin_decision_type_t) */
    float magnitude;                        /**< Decision magnitude/size [0,1] */
    char asset[FIN_EXPL_ASSET_LEN];         /**< Target asset identifier */
    char rationale[FIN_EXPL_RATIONALE_LEN]; /**< Brief rationale from decision maker */
} fin_decision_record_t;

/**
 * @brief Generated explanation for a decision
 */
typedef struct {
    char summary[FIN_EXPL_SUMMARY_LEN];     /**< Human-readable summary */
    char* reasoning_steps;                   /**< Concatenated reasoning steps (heap) */
    uint32_t num_steps;                      /**< Number of reasoning steps */
    float confidence;                        /**< Confidence in explanation [0,1] */
    char regulatory_note[FIN_EXPL_REGULATORY_LEN]; /**< Regulatory disclosure note */
} fin_explanation_t;

/**
 * @brief Audit trail entry
 */
typedef struct {
    uint64_t timestamp_ns;                   /**< Nanosecond timestamp */
    uint64_t sequence_num;                   /**< Global sequence number */
    fin_audit_type_t audit_type;             /**< Type of audit entry */
    fin_decision_record_t decision;          /**< Decision being audited */
    char explanation_summary[FIN_EXPL_SUMMARY_LEN]; /**< Explanation summary */
    float confidence;                        /**< Decision confidence */
    char context[FIN_EXPL_AUDIT_ENTRY_LEN];  /**< Full context JSON */
    bool compliant;                          /**< Compliance check passed */
} fin_audit_entry_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t explanations_generated;         /**< Total explanations generated */
    uint64_t audit_trails_created;           /**< Total audit trails created */
    uint64_t immune_checks;                  /**< Immune system checks */
    uint64_t bbb_validations;                /**< BBB validations performed */
    uint64_t kg_messages_sent;               /**< KG messages published */
    uint64_t health_heartbeats;              /**< Health heartbeats sent */
} fin_explanations_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Explanation settings */
    fin_explanation_level_t default_level;   /**< Default explanation detail level */
    bool include_confidence;                 /**< Include confidence scores */
    bool include_regulatory_notes;           /**< Include regulatory notes */
    uint32_t max_reasoning_steps;            /**< Max reasoning steps to generate */

    /* Audit trail settings */
    bool enable_audit_trail;                 /**< Enable audit trail creation */
    uint32_t max_audit_entries;              /**< Max entries in circular buffer */
    bool persist_audit_trail;                /**< Persist to disk (future) */

    /* Regulatory compliance */
    bool mifid2_mode;                        /**< Enable MiFID II compliance mode */
    bool sec_mode;                           /**< Enable SEC compliance mode */
    bool include_timestamps;                 /**< Include nanosecond timestamps */

    /* Integration settings */
    bool enable_immune_integration;          /**< Enable immune system */
    bool enable_bbb_validation;              /**< Enable BBB validation */
    bool enable_kg_messaging;                /**< Enable KG messaging */
    bool enable_health_monitoring;           /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                    /**< Verbose debug output */
} fin_explanations_config_t;

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
 * @brief Opaque financial explanations bridge handle
 */
typedef struct financial_explanations_bridge financial_explanations_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_explanations_bridge_default_config(fin_explanations_config_t* config);

/**
 * @brief Create financial explanations bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_explanations_bridge_t* financial_explanations_bridge_create(
    const fin_explanations_config_t* config
);

/**
 * @brief Destroy financial explanations bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_explanations_bridge_destroy(financial_explanations_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_reset(financial_explanations_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_explanations_bridge_set_immune(financial_explanations_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_explanations_bridge_set_bbb(financial_explanations_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_explanations_bridge_set_health_agent(financial_explanations_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_explanations_bridge_set_kg_wiring(financial_explanations_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_explanations_bridge_set_logger(financial_explanations_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_explanations_bridge_set_security(financial_explanations_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_explanations_bridge_set_ethics(financial_explanations_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_explanations_bridge_set_lgss(financial_explanations_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_explanations_bridge_set_coordinator(financial_explanations_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_explanations_bridge_set_bio_router(financial_explanations_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Explanation API
 * ============================================================================ */

/**
 * @brief Generate explanation for a trading decision
 *
 * Creates a human-readable explanation with reasoning chain for the given
 * trading decision. The explanation includes confidence scoring and
 * regulatory notes when applicable.
 *
 * @param bridge Bridge handle
 * @param decision Trading decision to explain
 * @param level Detail level for explanation
 * @param explanation Output explanation (caller must free reasoning_steps)
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_explain_decision(
    financial_explanations_bridge_t* bridge,
    const fin_decision_record_t* decision,
    fin_explanation_level_t level,
    fin_explanation_t* explanation
);

/**
 * @brief Create audit trail entry for a trade
 *
 * Records a complete audit trail entry for regulatory compliance,
 * including timestamps, decision context, and explanation.
 *
 * @param bridge Bridge handle
 * @param decision Trading decision to audit
 * @param explanation Explanation for the decision (can be NULL)
 * @param audit_type Type of audit entry
 * @param entry Output audit entry
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_audit_trail(
    financial_explanations_bridge_t* bridge,
    const fin_decision_record_t* decision,
    const fin_explanation_t* explanation,
    fin_audit_type_t audit_type,
    fin_audit_entry_t* entry
);

/**
 * @brief Free explanation resources
 *
 * Frees the reasoning_steps buffer allocated by explain_decision.
 *
 * @param explanation Explanation to free (NULL safe)
 */
void financial_explanations_bridge_free_explanation(fin_explanation_t* explanation);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_explanations_bridge_state_t financial_explanations_bridge_get_state(
    const financial_explanations_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_get_stats(
    const financial_explanations_bridge_t* bridge,
    fin_explanations_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_explanations_bridge_reset_stats(financial_explanations_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_explanations_bridge_get_last_error(void);

/**
 * @brief Get audit trail entry count
 *
 * @param bridge Bridge handle
 * @return Number of entries in audit trail buffer
 */
uint64_t financial_explanations_bridge_get_audit_count(
    const financial_explanations_bridge_t* bridge
);

/**
 * @brief Get audit trail entry by index
 *
 * @param bridge Bridge handle
 * @param index Entry index (0 = oldest)
 * @param entry Output entry
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_get_audit_entry(
    const financial_explanations_bridge_t* bridge,
    uint64_t index,
    fin_audit_entry_t* entry
);

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
int financial_explanations_bridge_heartbeat(
    financial_explanations_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get decision type name
 *
 * @param decision_type Decision type
 * @return String name (static)
 */
const char* fin_expl_decision_name(fin_decision_type_t decision_type);

/**
 * @brief Get explanation level name
 *
 * @param level Explanation level
 * @return String name (static)
 */
const char* fin_expl_level_name(fin_explanation_level_t level);

/**
 * @brief Get audit type name
 *
 * @param audit_type Audit type
 * @return String name (static)
 */
const char* fin_expl_audit_name(fin_audit_type_t audit_type);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_expl_state_name(fin_explanations_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_explanations_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_training_begin(financial_explanations_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_training_end(financial_explanations_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_explanations_bridge_training_step(financial_explanations_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_EXPLANATIONS_BRIDGE_H */
