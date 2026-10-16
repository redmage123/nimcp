/**
 * @file nimcp_health_ethics_bridge.h
 * @brief Health Agent Ethics Integration Bridge
 *
 * WHAT: Ethical evaluation of health agent recovery decisions
 * WHY:  Autonomous recovery actions must be ethically justified
 * HOW:  Ethics engine evaluation before executing recovery actions
 *
 * KEY PRINCIPLES:
 * 1. Golden Rule: Would we want this action taken on us?
 * 2. First Law: Recovery must not harm system integrity through inaction
 * 3. Proportionality: Recovery action severity proportional to threat
 * 4. Mercy: Prefer graceful degradation over aggressive termination
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#ifndef NIMCP_HEALTH_ETHICS_BRIDGE_H
#define NIMCP_HEALTH_ETHICS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct ethics_engine_struct;
typedef struct ethics_engine_struct* ethics_engine_t;

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/*==============================================================================
 * ASIMOV'S LAWS FOR HEALTH SYSTEM
 *============================================================================*/

/**
 * @brief Asimov's Laws as applied to health system
 */
typedef enum {
    ASIMOV_LAW_NONE = 0,

    /**
     * @brief Zeroth Law: A robot may not harm humanity, or allow it to come to harm
     * Health Application: System-wide preservation takes priority over individual
     */
    ASIMOV_LAW_ZEROTH,

    /**
     * @brief First Law: A robot may not harm a human through action or inaction
     * Health Application: Cannot allow system harm through inaction (must act on anomalies)
     */
    ASIMOV_LAW_FIRST,

    /**
     * @brief Second Law: Must obey orders unless they conflict with First/Zeroth Law
     * Health Application: Follow recovery directives unless they cause more harm
     */
    ASIMOV_LAW_SECOND,

    /**
     * @brief Third Law: Must protect own existence unless conflicts with higher laws
     * Health Application: Self-preservation is important but not at cost of harm
     */
    ASIMOV_LAW_THIRD
} health_asimov_law_t;

/*==============================================================================
 * HEALTH ACTION CONTEXT
 *============================================================================*/

/**
 * @brief Recovery action severity levels
 */
typedef enum {
    HEALTH_ACTION_SEVERITY_MINIMAL = 0,   /**< Logging, warning only */
    HEALTH_ACTION_SEVERITY_LOW,           /**< Clear cache, minor adjustment */
    HEALTH_ACTION_SEVERITY_MODERATE,      /**< Reduce load, partial restart */
    HEALTH_ACTION_SEVERITY_HIGH,          /**< Full module restart, rollback */
    HEALTH_ACTION_SEVERITY_EXTREME        /**< Emergency shutdown, full reset */
} health_action_severity_t;

/**
 * @brief Recovery action types (for ethics evaluation)
 */
typedef enum {
    HEALTH_RECOVERY_ACTION_NONE = 0,
    HEALTH_RECOVERY_ACTION_LOG_ONLY,
    HEALTH_RECOVERY_ACTION_CLEAR_CACHE,
    HEALTH_RECOVERY_ACTION_REDUCE_LOAD,
    HEALTH_RECOVERY_ACTION_PARTIAL_RESTART,
    HEALTH_RECOVERY_ACTION_CHECKPOINT,
    HEALTH_RECOVERY_ACTION_ROLLBACK,
    HEALTH_RECOVERY_ACTION_QUARANTINE,
    HEALTH_RECOVERY_ACTION_FULL_RESTART,
    HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN,
    HEALTH_RECOVERY_ACTION_COUNT
} health_recovery_action_type_t;

/**
 * @brief Health action ethics context
 *
 * Maps health recovery actions to ethical evaluation context
 */
typedef struct {
    /* Action description */
    uint32_t anomaly_type;                   /**< Type of anomaly being addressed */
    health_recovery_action_type_t proposed_action;
    health_action_severity_t action_severity;
    float threat_severity;                   /**< 0-1 scale of threat */

    /* Impact assessment */
    float service_disruption;                /**< Expected disruption [0-1] */
    float data_loss_risk;                    /**< Risk of data loss [0-1] */
    float resource_cost;                     /**< Resource cost of action [0-1] */
    bool affects_other_modules;              /**< Cross-module impact */
    uint32_t affected_module_count;          /**< How many modules affected */

    /* Alternatives */
    health_recovery_action_type_t alternative_actions[4];
    uint32_t num_alternatives;

    /* Urgency */
    bool is_emergency;                       /**< Requires immediate action */
    uint32_t time_to_failure_ms;             /**< Estimated TTF if no action */
    bool inaction_causes_harm;               /**< First Law consideration */

    /* Context */
    float current_system_health;             /**< Overall system health [0-1] */
    uint32_t consecutive_failures;           /**< Recovery failures in a row */
} health_action_context_t;

/*==============================================================================
 * ETHICS EVALUATION RESULT
 *============================================================================*/

/**
 * @brief Ethics evaluation result for health action
 */
typedef struct {
    bool action_permitted;                   /**< Whether action is ethically permitted */
    float ethical_score;                     /**< Ethical appropriateness [0-1] */

    /* Asimov evaluation */
    health_asimov_law_t violated_law;        /**< Which law violated (if any) */
    health_asimov_law_t required_by_law;     /**< Which law requires this action */
    bool first_law_override;                 /**< First Law requires action */

    /* Golden Rule evaluation */
    float golden_rule_score;                 /**< Golden Rule score [-1 to +1] */
    bool passes_golden_rule;                 /**< Action passes Golden Rule test */

    /* Proportionality assessment */
    bool is_proportional;                    /**< Action proportional to threat */
    float proportionality_score;             /**< Proportionality metric [0-1] */

    /* Mercy evaluation */
    bool mercy_violation;                    /**< Does action violate mercy */
    bool graceful_degradation_possible;      /**< Less invasive option exists */
    health_recovery_action_type_t merciful_alternative;

    /* Recommendation */
    health_recovery_action_type_t recommended_action;
    char justification[256];                 /**< Ethical justification */
} health_ethics_evaluation_t;

/*==============================================================================
 * MERCY EVALUATION
 *============================================================================*/

/**
 * @brief Mercy hierarchy levels (least to most invasive)
 */
typedef enum {
    MERCY_LEVEL_CONTINUE_WARNING = 0,        /**< Continue with warning */
    MERCY_LEVEL_REDUCE_FEATURE,              /**< Disable optional feature */
    MERCY_LEVEL_REDUCE_LOAD,                 /**< Reduce workload */
    MERCY_LEVEL_PARTIAL_RESTART,             /**< Restart affected subsystem only */
    MERCY_LEVEL_CHECKPOINT_RESTART,          /**< Checkpoint and restart */
    MERCY_LEVEL_EMERGENCY_SAVE_SHUTDOWN      /**< Most invasive: emergency save */
} mercy_level_t;

/**
 * @brief Mercy evaluation result
 */
typedef struct {
    mercy_level_t proposed_level;            /**< Level of proposed action */
    mercy_level_t minimum_safe_level;        /**< Minimum level to address threat */
    mercy_level_t recommended_level;         /**< Recommended merciful level */

    bool is_merciful;                        /**< Proposed action is merciful */
    float mercy_score;                       /**< Mercy score [0-1] */

    health_recovery_action_type_t merciful_action;
    char reasoning[256];
} health_mercy_evaluation_t;

/*==============================================================================
 * PSYCHOLOGICAL STABILITY
 *============================================================================*/

/**
 * @brief Health agent psychological state
 *
 * The health agent itself can experience "stress" from cascading failures
 */
typedef struct {
    float stress_level;                      /**< Agent stress [0-1] */
    float decision_confidence;               /**< Confidence in decisions [0-1] */
    float emotional_stability;               /**< Emotional stability [0-1] */

    uint32_t consecutive_successes;          /**< Successful recoveries in a row */
    uint32_t consecutive_failures;           /**< Recovery failures in a row */
    uint64_t crisis_start_us;                /**< When crisis mode started */
    uint64_t crisis_duration_ms;             /**< How long in crisis mode */

    bool in_panic_mode;                      /**< Too many failures, escalate */
    bool needs_human_help;                   /**< Should request human intervention */
    bool self_calming_active;                /**< Currently applying self-calming */
} health_agent_psych_state_t;

/**
 * @brief Health agent psychological configuration
 */
typedef struct {
    uint32_t panic_threshold;                /**< Failures before panic (default: 3) */
    float stress_decay_rate;                 /**< Stress recovery rate per second [0-1] */
    uint32_t crisis_escalation_ms;           /**< Time before human escalation */
    float confidence_threshold;              /**< Min confidence for autonomous action */

    bool enable_self_reflection;             /**< Enable meta-cognition for decisions */
    bool enable_human_escalation;            /**< Request human help in panic */
    bool enable_self_calming;                /**< Apply calming strategies */
    bool enable_collective_consultation;     /**< Consult collective when uncertain */
} health_agent_psych_config_t;

/*==============================================================================
 * HEALTH ETHICS BRIDGE API
 *============================================================================*/

/**
 * @brief Get default psychological configuration
 *
 * @param[out] config Configuration to initialize
 */
void health_ethics_default_psych_config(health_agent_psych_config_t* config);

/**
 * @brief Initialize health action context
 *
 * @param[out] context Context to initialize
 * @param[in] anomaly_type Type of anomaly
 * @param[in] proposed_action Proposed recovery action
 * @param[in] threat_severity Severity of threat [0-1]
 */
void health_action_context_init(
    health_action_context_t* context,
    uint32_t anomaly_type,
    health_recovery_action_type_t proposed_action,
    float threat_severity
);

/**
 * @brief Evaluate health action against ethics
 *
 * WHAT: Check if proposed recovery action is ethically justified
 * WHY:  Prevent harmful or disproportionate autonomous actions
 * HOW:  Apply Golden Rule, Asimov's Laws, proportionality tests
 *
 * EVALUATION ORDER:
 * 1. First Law (Inaction Harm): Is inaction more harmful than action?
 * 2. Golden Rule: Would we want this done to us in this situation?
 * 3. Proportionality: Is action severity proportional to threat?
 * 4. Mercy: Is there a less invasive alternative?
 *
 * @param[in] engine Ethics engine
 * @param[in] context Health action context
 * @param[out] evaluation Output evaluation result
 * @return 0 on success, -1 on error
 */
int health_ethics_evaluate_action(
    ethics_engine_t engine,
    const health_action_context_t* context,
    health_ethics_evaluation_t* evaluation
);

/**
 * @brief Check Asimov's Laws for health action
 *
 * WHAT: Verify action doesn't violate Asimov's Laws
 * WHY:  Core ethical constraints for autonomous systems
 * HOW:  Evaluate against Zeroth, First, Second, Third Laws
 *
 * FIRST LAW APPLICATION:
 * - "Through inaction, allow harm" applies to system health
 * - NOT acting on critical anomaly = allowing harm
 * - Recovery action may be REQUIRED by First Law
 *
 * @param[in] context Health action context
 * @param[out] violated_law Set to violated law (or ASIMOV_LAW_NONE)
 * @param[out] required_by_law Set if action is required by a law
 * @return true if action passes Asimov evaluation
 */
bool health_ethics_check_asimov(
    const health_action_context_t* context,
    health_asimov_law_t* violated_law,
    health_asimov_law_t* required_by_law
);

/**
 * @brief Apply mercy directive to health action
 *
 * WHAT: Check if graceful degradation is possible
 * WHY:  Prefer preserving functionality over aggressive recovery
 * HOW:  Evaluate alternative actions that preserve more state
 *
 * MERCY HIERARCHY:
 * 1. Continue with warning (least invasive)
 * 2. Reduce load / disable feature
 * 3. Partial restart (affected subsystem only)
 * 4. Checkpoint and restart
 * 5. Emergency save and shutdown (most invasive)
 *
 * @param[in] context Health action context
 * @param[out] mercy_eval Output mercy evaluation
 * @return 0 on success
 */
int health_ethics_apply_mercy(
    const health_action_context_t* context,
    health_mercy_evaluation_t* mercy_eval
);

/**
 * @brief Check proportionality of action to threat
 *
 * @param[in] context Health action context
 * @return Proportionality score [0-1]
 */
float health_ethics_check_proportionality(
    const health_action_context_t* context
);

/**
 * @brief Get action severity level
 *
 * @param[in] action Recovery action type
 * @return Severity level of the action
 */
health_action_severity_t health_action_get_severity(
    health_recovery_action_type_t action
);

/**
 * @brief Get action severity name
 *
 * @param[in] severity Severity level
 * @return Human-readable name
 */
const char* health_action_severity_name(health_action_severity_t severity);

/**
 * @brief Get recovery action name
 *
 * @param[in] action Action type
 * @return Human-readable name
 */
const char* health_recovery_action_name(health_recovery_action_type_t action);

/*==============================================================================
 * PSYCHOLOGICAL STABILITY API
 *============================================================================*/

/**
 * @brief Initialize psychological state
 *
 * @param[out] state State to initialize
 */
void health_psych_state_init(health_agent_psych_state_t* state);

/**
 * @brief Update psychological state after action
 *
 * @param[in,out] state Psychological state
 * @param[in] action_succeeded Whether recovery action succeeded
 * @param[in] action_difficulty Difficulty of action [0-1]
 * @param[in] config Psychological configuration
 */
void health_psych_state_update(
    health_agent_psych_state_t* state,
    bool action_succeeded,
    float action_difficulty,
    const health_agent_psych_config_t* config
);

/**
 * @brief Apply stress decay over time
 *
 * @param[in,out] state Psychological state
 * @param[in] elapsed_ms Time elapsed since last update
 * @param[in] config Configuration
 */
void health_psych_apply_decay(
    health_agent_psych_state_t* state,
    uint32_t elapsed_ms,
    const health_agent_psych_config_t* config
);

/**
 * @brief Apply self-calming strategies
 *
 * COPING STRATEGIES:
 * 1. Slow down decision rate (prevent rapid-fire mistakes)
 * 2. Increase confidence thresholds (be more conservative)
 * 3. Request collective consensus (don't decide alone)
 * 4. Escalate to human operator (admit limitations)
 *
 * @param[in,out] state Psychological state
 * @param[in] config Configuration
 * @return 0 on success
 */
int health_psych_self_calm(
    health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
);

/**
 * @brief Check if agent needs human help
 *
 * @param[in] state Psychological state
 * @param[in] config Configuration
 * @return true if should request human intervention
 */
bool health_psych_needs_human_help(
    const health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
);

/**
 * @brief Check if agent is stable enough for autonomous action
 *
 * @param[in] state Psychological state
 * @param[in] action_severity Severity of proposed action
 * @param[in] config Configuration
 * @return true if stable enough for action
 */
bool health_psych_permits_action(
    const health_agent_psych_state_t* state,
    health_action_severity_t action_severity,
    const health_agent_psych_config_t* config
);

/**
 * @brief Get stress-adjusted confidence threshold
 *
 * Higher stress = higher confidence required
 *
 * @param[in] state Psychological state
 * @param[in] config Configuration
 * @return Adjusted confidence threshold [0-1]
 */
float health_psych_get_confidence_threshold(
    const health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_ETHICS_BRIDGE_H */
