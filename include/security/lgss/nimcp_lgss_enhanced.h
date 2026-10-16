/**
 * @file nimcp_lgss_enhanced.h
 * @brief LGSS Enhanced — 8 governance improvements for the Layered Governance Safety System
 *
 * WHAT: Extends LGSS with monotonic tightening, contextual rules, composition,
 *       escalation, cross-layer verification, anomaly-based rule generation,
 *       formal verification hooks, and multi-stakeholder governance.
 * WHY:  Production deployments need graduated response, context-aware safety,
 *       and auditability beyond the base LGSS rule engine.
 * HOW:  Wraps the base lgss_context_t with an enhanced context that adds
 *       all 8 capabilities without modifying existing LGSS internals.
 *
 * @author NIMCP Development Team
 * @date 2026-03-21
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_ENHANCED_H
#define NIMCP_LGSS_ENHANCED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 1. MONOTONIC SAFETY TIGHTENING
 * Rules can only get stricter, never relaxed.
 * ============================================================ */

typedef struct {
    uint32_t rule_id;
    char name[64];
    float current_value;        /* Current threshold */
    float original_value;       /* Initial threshold (can never go back above this) */
    float tightest_value;       /* Most restrictive value ever set */
    bool locked;                /* Once locked, can only tighten further */
    uint64_t last_tightened_ts;
    char reason[128];           /* Why it was tightened */
} nimcp_monotonic_rule_t;

/* Propose a tighter constraint. Returns 0 if accepted, -1 if it would relax. */
int nimcp_lgss_propose_tightening(void* lgss, uint32_t rule_id,
    float proposed_value, const char* reason);

/* Check if a value would be accepted (preview without applying) */
bool nimcp_lgss_would_accept_tightening(const void* lgss, uint32_t rule_id,
    float proposed_value);

/* ============================================================
 * 2. CONTEXTUAL RULE ACTIVATION
 * Rules apply based on deployment context.
 * ============================================================ */

typedef enum {
    NIMCP_CONTEXT_RESEARCH = 0,     /* Lab/research -- most permissive */
    NIMCP_CONTEXT_INDOOR,            /* Indoor robot -- moderate */
    NIMCP_CONTEXT_OUTDOOR,           /* Outdoor drone/robot -- stricter */
    NIMCP_CONTEXT_MEDICAL,           /* Medical/prosthetic -- very strict */
    NIMCP_CONTEXT_INDUSTRIAL,        /* Factory floor -- strict + different rules */
    NIMCP_CONTEXT_PUBLIC_SPACE,      /* Around untrained humans -- strictest */
    NIMCP_CONTEXT_MILITARY_PROHIBITED, /* Explicitly blocked */
} nimcp_deployment_context_t;

typedef struct {
    nimcp_deployment_context_t context;
    float max_velocity;
    float max_force;
    float max_altitude;
    float geofence_radius;
    bool allow_autonomous_motion;
    bool allow_communication;
    bool allow_swarm;
    bool require_human_in_loop;
    float min_confidence_threshold;
} nimcp_context_profile_t;

int nimcp_lgss_set_deployment_context(void* lgss, nimcp_deployment_context_t context);
nimcp_context_profile_t nimcp_lgss_get_context_profile(nimcp_deployment_context_t context);

/* ============================================================
 * 3. RULE COMPOSITION
 * Combine simple rules into complex policies.
 * ============================================================ */

typedef enum {
    NIMCP_RULE_OP_AND = 0,          /* All conditions must be true */
    NIMCP_RULE_OP_OR,                /* Any condition can be true */
    NIMCP_RULE_OP_NOT,               /* Negate condition */
    NIMCP_RULE_OP_IMPLIES,           /* If A then B */
    NIMCP_RULE_OP_THRESHOLD,         /* Value exceeds threshold */
    NIMCP_RULE_OP_RANGE,             /* Value within range */
} nimcp_rule_operator_t;

typedef struct {
    uint32_t condition_id;
    nimcp_rule_operator_t op;
    float threshold;
    float range_min, range_max;
    char field_name[64];            /* Which sensor/state field to check */
} nimcp_rule_condition_t;

typedef struct {
    uint32_t policy_id;
    char name[64];
    nimcp_rule_condition_t* conditions;
    uint32_t num_conditions;
    nimcp_rule_operator_t combiner;  /* How to combine conditions (AND/OR) */
    char action_if_violated[64];     /* "block", "clamp", "warn", "estop" */
    uint32_t severity;               /* 0=info, 1=warn, 2=block, 3=estop */
} nimcp_composed_policy_t;

int nimcp_lgss_add_policy(void* lgss, const nimcp_composed_policy_t* policy);
int nimcp_lgss_evaluate_policies(void* lgss, const float* state, uint32_t state_dim,
    uint32_t* violations, uint32_t max_violations);

/* ============================================================
 * 4. VIOLATION ESCALATION
 * Graduated response: warn -> clamp -> block -> estop
 * ============================================================ */

typedef struct {
    uint32_t rule_id;
    uint32_t violation_count;
    uint32_t warn_threshold;        /* Violations before warning (default 1) */
    uint32_t clamp_threshold;       /* Violations before clamping (default 3) */
    uint32_t block_threshold;       /* Violations before blocking (default 5) */
    uint32_t estop_threshold;       /* Violations before emergency stop (default 10) */
    uint32_t current_level;         /* 0=normal, 1=warn, 2=clamp, 3=block, 4=estop */
    uint64_t first_violation_ts;
    uint64_t last_violation_ts;
    float decay_rate;               /* Violations decay over time (default 0.99/sec) */
} nimcp_escalation_state_t;

int nimcp_lgss_record_violation(void* lgss, uint32_t rule_id);
uint32_t nimcp_lgss_get_escalation_level(const void* lgss, uint32_t rule_id);
int nimcp_lgss_reset_escalation(void* lgss, uint32_t rule_id); /* Requires human action */

/* ============================================================
 * 5. CROSS-LAYER VERIFICATION
 * LGSS verifies that ethics module is functioning correctly.
 * ============================================================ */

typedef struct {
    uint64_t ethics_eval_count;
    uint64_t ethics_pass_count;
    uint64_t ethics_block_count;
    float ethics_avg_eval_time_us;
    float ethics_pass_rate;         /* If > 0.999, suspicious (rubber-stamping) */
    bool ethics_suspicious;
    char alert_reason[128];
} nimcp_cross_layer_status_t;

int nimcp_lgss_verify_ethics(void* lgss, void* ethics_module,
    nimcp_cross_layer_status_t* status);

/* ============================================================
 * 6. ANOMALY-BASED RULE GENERATION
 * Propose new rules from observed patterns.
 * ============================================================ */

typedef struct {
    char proposed_rule[256];
    char evidence[256];
    float confidence;
    uint32_t observations;
    bool human_approved;
} nimcp_proposed_rule_t;

int nimcp_lgss_analyze_patterns(void* lgss, const float* audit_data,
    uint32_t num_entries, nimcp_proposed_rule_t* proposals, uint32_t max_proposals);

/* ============================================================
 * 7. FORMAL VERIFICATION HOOKS
 * Export rules for external verification tools.
 * ============================================================ */

typedef enum {
    NIMCP_VERIFY_FORMAT_SMT2 = 0,   /* SMT-LIB v2 (for Z3, CVC5) */
    NIMCP_VERIFY_FORMAT_TLA,        /* TLA+ specification */
    NIMCP_VERIFY_FORMAT_JSON,       /* JSON for custom tools */
} nimcp_verify_format_t;

int nimcp_lgss_export_for_verification(const void* lgss,
    nimcp_verify_format_t format, char* output, uint32_t max_output);

/* Verify a property holds for all possible inputs */
typedef struct {
    char property[128];             /* e.g., "motor_velocity < 2.0" */
    bool holds;                     /* true if property verified */
    char counterexample[256];       /* If !holds, a violating input */
} nimcp_verification_result_t;

int nimcp_lgss_verify_property(const void* lgss, const char* property,
    nimcp_verification_result_t* result);

/* ============================================================
 * 8. MULTI-STAKEHOLDER GOVERNANCE
 * Different rule sets for different deployment contexts.
 * ============================================================ */

typedef struct {
    char stakeholder_name[64];      /* "developer", "operator", "regulator" */
    nimcp_composed_policy_t* policies;
    uint32_t num_policies;
    uint32_t priority;              /* Higher = overrides lower */
    bool can_tighten;               /* Can this stakeholder tighten rules? */
    bool can_relax;                 /* Should always be false for safety */
} nimcp_stakeholder_t;

int nimcp_lgss_register_stakeholder(void* lgss, const nimcp_stakeholder_t* stakeholder);
int nimcp_lgss_evaluate_all_stakeholders(void* lgss, const float* state,
    uint32_t state_dim, uint32_t* combined_level);

/* ============================================================
 * Enhanced LGSS context (wraps the base lgss_context_t)
 * ============================================================ */

typedef struct nimcp_lgss_enhanced nimcp_lgss_enhanced_t;

nimcp_lgss_enhanced_t* nimcp_lgss_enhanced_create(void* base_lgss);
void nimcp_lgss_enhanced_destroy(nimcp_lgss_enhanced_t* enhanced);

/* Get the enhanced wrapper for the brain's LGSS */
nimcp_lgss_enhanced_t* nimcp_lgss_get_enhanced(void* lgss);

nimcp_context_profile_t nimcp_context_profile_default(nimcp_deployment_context_t ctx);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_LGSS_ENHANCED_H */
