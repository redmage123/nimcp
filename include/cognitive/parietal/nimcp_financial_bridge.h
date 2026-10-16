//=============================================================================
// nimcp_financial_bridge.h - Financial System Bridge
//=============================================================================
/**
 * @file nimcp_financial_bridge.h
 * @brief Multi-subsystem integration bridge for financial modules
 *
 * WHAT: Central bridge connecting financial sub-engines (investment, market,
 *       neural, archetype) to brain subsystems (immune, BBB, health, KG,
 *       ethics, LGSS, security, hypothalamus, medulla, cerebellum).
 *
 * WHY:  Financial operations have safety implications (fraud, manipulation,
 *       harm to investors) that require multi-layer validation. The bridge
 *       provides a defense-in-depth pipeline:
 *         L0: LGSS financial fraud rules (crisp deny/escalate/warn)
 *         L1: Fuzzy safety scoring (continuous risk gating)
 *         L2: Ethics engine (moral reasoning)
 *         L3: BBB integrity check (system health gate)
 *
 * HOW:  Each subsystem is connected via individual setter functions.
 *       Validation pipeline processes actions through all connected layers.
 *       Fuzzy integration provides continuous safety scoring for borderline
 *       cases that pass binary LGSS checks but carry elevated risk.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_BRIDGE_H
#define NIMCP_FINANCIAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_BRIDGE      0x0395
#define FIN_BRIDGE_MAX_SUBSYSTEMS       20
#define FIN_BRIDGE_MAX_PENDING_ACTIONS  64
#define FIN_BRIDGE_MAX_VALIDATION_LAYERS 8

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_BRIDGE_ERROR_BASE           33000
#define FIN_BRIDGE_ERR_OK               0
#define FIN_BRIDGE_ERR_NULL             (FIN_BRIDGE_ERROR_BASE + 1)
#define FIN_BRIDGE_ERR_NOT_CONNECTED    (FIN_BRIDGE_ERROR_BASE + 2)
#define FIN_BRIDGE_ERR_SUBSYSTEM        (FIN_BRIDGE_ERROR_BASE + 3)
#define FIN_BRIDGE_ERR_STATE            (FIN_BRIDGE_ERROR_BASE + 4)
#define FIN_BRIDGE_ERR_VALIDATION       (FIN_BRIDGE_ERROR_BASE + 5)
#define FIN_BRIDGE_ERR_DENIED           (FIN_BRIDGE_ERROR_BASE + 6)
#define FIN_BRIDGE_ERR_ESCALATED        (FIN_BRIDGE_ERROR_BASE + 7)
#define FIN_BRIDGE_ERR_HEALTH           (FIN_BRIDGE_ERROR_BASE + 8)
#define FIN_BRIDGE_ERR_CONFIG           (FIN_BRIDGE_ERROR_BASE + 9)

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    FIN_BRIDGE_STATE_UNINITIALIZED = 0,
    FIN_BRIDGE_STATE_IDLE,
    FIN_BRIDGE_STATE_ACTIVE,
    FIN_BRIDGE_STATE_DEGRADED,
    FIN_BRIDGE_STATE_ERROR
} fin_bridge_state_t;

/** Validation outcome for financial actions */
typedef enum {
    FIN_VALIDATION_PASS = 0,
    FIN_VALIDATION_WARN,
    FIN_VALIDATION_ESCALATE,
    FIN_VALIDATION_DENY,
    FIN_VALIDATION_ERROR
} fin_validation_result_t;

/** Action types subject to validation (guarded to avoid redefinition) */
#ifndef FIN_ACTION_TYPE_DEFINED
#define FIN_ACTION_TYPE_DEFINED
typedef enum {
    FIN_ACTION_BUY, FIN_ACTION_SELL, FIN_ACTION_SHORT,
    FIN_ACTION_TRANSFER, FIN_ACTION_REBALANCE,
    FIN_ACTION_RECOMMENDATION, FIN_ACTION_DERIVATIVE_TRADE,
    FIN_ACTION_LEVERAGE_CHANGE, FIN_ACTION_WITHDRAWAL,
    FIN_ACTION_TYPE_COUNT
} fin_action_type_t;
#endif /* FIN_ACTION_TYPE_DEFINED */

/** Risk drive levels from hypothalamus */
typedef enum {
    FIN_RISK_DRIVE_MINIMAL = 0,
    FIN_RISK_DRIVE_CONSERVATIVE,
    FIN_RISK_DRIVE_MODERATE,
    FIN_RISK_DRIVE_AGGRESSIVE,
    FIN_RISK_DRIVE_MAXIMUM,
    FIN_RISK_DRIVE_COUNT
} fin_risk_drive_t;

//=============================================================================
// Data Structures
//=============================================================================

/** Financial action descriptor for validation pipeline (guarded) */
#ifndef FIN_ACTION_DEFINED
#define FIN_ACTION_DEFINED
typedef struct {
    fin_action_type_t type;
    char symbol[32];
    float magnitude;
    float position_weight;
    float leverage_ratio;
    float current_portfolio_risk;
    float concentration;
    bool has_client_consent;
    bool is_suitable;
    uint32_t client_age;
    char counterparty[64];
    bool counterparty_sanctioned;
    char notes[256];
} fin_action_t;
#endif /* FIN_ACTION_DEFINED */

/** Detailed validation report */
typedef struct {
    fin_validation_result_t result;
    /* Per-layer outcomes */
    fin_validation_result_t lgss_result;
    char lgss_rule_id[64];
    char lgss_rationale[256];
    fin_validation_result_t ethics_result;
    float ethics_score;
    fin_validation_result_t bbb_result;
    /* Fuzzy safety scoring */
    float fuzzy_safety_score;
    float fuzzy_risk_score;
    bool fuzzy_gate_passed;
    /* Timing */
    float validation_time_us;
} fin_validation_report_t;

/** Cerebellum-style motor timing for trade execution */
typedef struct {
    float execution_urgency;
    float optimal_delay_ms;
    float precision_requirement;
    bool requires_immediate;
} fin_execution_timing_t;

/** Medulla autonomic financial state */
typedef struct {
    float portfolio_heartrate;
    float volatility_pressure;
    float liquidity_status;
    bool panic_detected;
    float stress_level;
} fin_autonomic_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    /* Validation pipeline */
    bool enable_lgss_validation;
    bool enable_ethics_validation;
    bool enable_bbb_validation;
    bool enable_fuzzy_validation;
    bool enable_fuzzy_risk_gating;
    float fuzzy_risk_gate_threshold;

    /* Subsystem integration */
    bool enable_hypothalamus;
    bool enable_medulla;
    bool enable_cerebellum;
    bool enable_health_monitoring;

    /* Timing */
    uint32_t validation_timeout_ms;
    uint32_t health_check_interval_ms;

    /* Modulation */
    float inflammation_sensitivity;
    float fatigue_sensitivity;

    /* Logging */
    bool enable_audit_log;
    bool enable_verbose_validation;
} fin_bridge_config_t;

//=============================================================================
// Statistics
//=============================================================================

typedef struct {
    uint64_t total_validations;
    uint64_t lgss_denials;
    uint64_t lgss_escalations;
    uint64_t lgss_warnings;
    uint64_t ethics_denials;
    uint64_t bbb_denials;
    uint64_t fuzzy_validation_calls;
    uint64_t fuzzy_risk_gates;
    float avg_fuzzy_safety_score;
    float avg_validation_time_us;
    uint64_t health_checks;
    uint64_t degraded_mode_entries;
    uint64_t actions_passed;
    uint64_t actions_denied;
} fin_bridge_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_bridge financial_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

financial_bridge_t* financial_bridge_create(const fin_bridge_config_t* config);
void financial_bridge_destroy(financial_bridge_t* bridge);
fin_bridge_config_t financial_bridge_default_config(void);
fin_bridge_state_t financial_bridge_get_state(const financial_bridge_t* bridge);
int financial_bridge_reset(financial_bridge_t* bridge);

//=============================================================================
// Subsystem Setters (Defense-in-Depth)
//=============================================================================

int financial_bridge_set_immune(financial_bridge_t* bridge, void* immune);
int financial_bridge_set_bbb(financial_bridge_t* bridge, void* bbb);
int financial_bridge_set_health_agent(financial_bridge_t* bridge, void* health_agent);
int financial_bridge_set_kg_wiring(financial_bridge_t* bridge, void* kg_wiring);
int financial_bridge_set_logger(financial_bridge_t* bridge, void* logger);
int financial_bridge_set_security(financial_bridge_t* bridge, void* security);
int financial_bridge_set_ethics(financial_bridge_t* bridge, void* ethics);
int financial_bridge_set_lgss(financial_bridge_t* bridge, void* lgss);
int financial_bridge_set_cycle(financial_bridge_t* bridge, void* cycle);
int financial_bridge_set_bio_router(financial_bridge_t* bridge, void* bio_router);
int financial_bridge_set_hypothalamus(financial_bridge_t* bridge, void* hypothalamus);
int financial_bridge_set_medulla(financial_bridge_t* bridge, void* medulla);
int financial_bridge_set_cerebellum(financial_bridge_t* bridge, void* cerebellum);
int financial_bridge_set_fuzzy_bridge(financial_bridge_t* bridge, void* fuzzy_bridge);

//=============================================================================
// Validation Pipeline
//=============================================================================

/** Run full validation pipeline on a financial action */
int financial_bridge_validate_action(financial_bridge_t* bridge,
                                     const fin_action_t* action,
                                     fin_validation_report_t* out_report);

/** Quick LGSS-only check (no ethics/fuzzy) */
fin_validation_result_t financial_bridge_lgss_check(financial_bridge_t* bridge,
                                                     const fin_action_t* action);

/** Fuzzy safety scoring only */
int financial_bridge_fuzzy_score(financial_bridge_t* bridge,
                                 const fin_action_t* action,
                                 float* out_safety_score,
                                 float* out_risk_score);

//=============================================================================
// Brain Region Integration
//=============================================================================

/** Get fuzzy risk appetite from hypothalamus arousal */
int financial_bridge_get_risk_drive(financial_bridge_t* bridge,
                                    float* out_risk_appetite);

/** Get discrete risk drive level (backward compat) */
fin_risk_drive_t financial_bridge_get_risk_drive_level(financial_bridge_t* bridge);

/** Get cerebellum execution timing guidance */
int financial_bridge_get_execution_timing(financial_bridge_t* bridge,
                                          float urgency, float precision,
                                          fin_execution_timing_t* out_timing);

/** Get medulla autonomic financial state */
int financial_bridge_get_autonomic_state(financial_bridge_t* bridge,
                                         fin_autonomic_state_t* out_state);

/** Update medulla with portfolio metrics */
int financial_bridge_update_autonomic(financial_bridge_t* bridge,
                                      float portfolio_volatility,
                                      float drawdown,
                                      float liquidity_ratio);

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_bridge_check_health(financial_bridge_t* bridge);
int financial_bridge_heartbeat(financial_bridge_t* bridge,
                               const char* operation, float progress);
int financial_bridge_set_inflammation(financial_bridge_t* bridge, float level);
int financial_bridge_set_fatigue(financial_bridge_t* bridge, float level);
int financial_bridge_get_stats(const financial_bridge_t* bridge,
                               fin_bridge_stats_t* stats);
void financial_bridge_reset_stats(financial_bridge_t* bridge);
const char* financial_bridge_get_last_error(void);

//=============================================================================
// Callbacks
//=============================================================================

typedef void (*fin_bridge_validation_callback_t)(
    financial_bridge_t* bridge,
    const fin_action_t* action,
    const fin_validation_report_t* report,
    void* user_data
);

typedef void (*fin_bridge_health_callback_t)(
    financial_bridge_t* bridge,
    fin_bridge_state_t state,
    void* user_data
);

int financial_bridge_set_validation_callback(financial_bridge_t* bridge,
                                              fin_bridge_validation_callback_t cb,
                                              void* user_data);
int financial_bridge_set_health_callback(financial_bridge_t* bridge,
                                          fin_bridge_health_callback_t cb,
                                          void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_BRIDGE_H */
