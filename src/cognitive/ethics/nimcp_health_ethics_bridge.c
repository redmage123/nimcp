/**
 * @file nimcp_health_ethics_bridge.c
 * @brief Health Agent Ethics Integration Bridge Implementation
 *
 * WHAT: Ethical evaluation of health agent recovery decisions
 * WHY:  Autonomous recovery actions must be ethically justified
 * HOW:  Ethics engine evaluation before executing recovery actions
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#include "cognitive/ethics/nimcp_health_ethics_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(health_ethics_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_health_ethics_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_health_ethics_bridge_mesh_registry = NULL;

nimcp_error_t health_ethics_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_health_ethics_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "health_ethics_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "health_ethics_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_health_ethics_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_health_ethics_bridge_mesh_registry = registry;
    return err;
}

void health_ethics_bridge_mesh_unregister(void) {
    if (g_health_ethics_bridge_mesh_registry && g_health_ethics_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_health_ethics_bridge_mesh_registry, g_health_ethics_bridge_mesh_id);
        g_health_ethics_bridge_mesh_id = 0;
        g_health_ethics_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void health_ethics_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_health_ethics_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_health_ethics_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_health_ethics_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "HEALTH_ETHICS_BRIDGE"


/*==============================================================================
 * CONSTANTS
 *============================================================================*/

/** Minimum threat severity to trigger First Law requirement */
#define FIRST_LAW_THREAT_THRESHOLD 0.7f

/** Proportionality score threshold for action approval */
#define PROPORTIONALITY_THRESHOLD 0.5f

/** Golden Rule score threshold for action approval */
#define GOLDEN_RULE_THRESHOLD 0.0f

/** Default panic threshold (failures before panic mode) */
#define DEFAULT_PANIC_THRESHOLD 3

/** Default stress decay rate per second */
#define DEFAULT_STRESS_DECAY_RATE 0.05f

/** Default crisis escalation time in ms */
#define DEFAULT_CRISIS_ESCALATION_MS 60000

/** Default confidence threshold for autonomous action */
#define DEFAULT_CONFIDENCE_THRESHOLD 0.6f

/*==============================================================================
 * ACTION SEVERITY MAPPING
 *============================================================================*/

/**
 * @brief Map recovery action to severity level
 */
static const health_action_severity_t action_severity_map[] = {
    [HEALTH_RECOVERY_ACTION_NONE]               = HEALTH_ACTION_SEVERITY_MINIMAL,
    [HEALTH_RECOVERY_ACTION_LOG_ONLY]           = HEALTH_ACTION_SEVERITY_MINIMAL,
    [HEALTH_RECOVERY_ACTION_CLEAR_CACHE]        = HEALTH_ACTION_SEVERITY_LOW,
    [HEALTH_RECOVERY_ACTION_REDUCE_LOAD]        = HEALTH_ACTION_SEVERITY_LOW,
    [HEALTH_RECOVERY_ACTION_PARTIAL_RESTART]    = HEALTH_ACTION_SEVERITY_MODERATE,
    [HEALTH_RECOVERY_ACTION_CHECKPOINT]         = HEALTH_ACTION_SEVERITY_MODERATE,
    [HEALTH_RECOVERY_ACTION_ROLLBACK]           = HEALTH_ACTION_SEVERITY_HIGH,
    [HEALTH_RECOVERY_ACTION_QUARANTINE]         = HEALTH_ACTION_SEVERITY_HIGH,
    [HEALTH_RECOVERY_ACTION_FULL_RESTART]       = HEALTH_ACTION_SEVERITY_HIGH,
    [HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN] = HEALTH_ACTION_SEVERITY_EXTREME
};

/**
 * @brief Severity names for logging
 */
static const char* severity_names[] = {
    [HEALTH_ACTION_SEVERITY_MINIMAL]  = "Minimal",
    [HEALTH_ACTION_SEVERITY_LOW]      = "Low",
    [HEALTH_ACTION_SEVERITY_MODERATE] = "Moderate",
    [HEALTH_ACTION_SEVERITY_HIGH]     = "High",
    [HEALTH_ACTION_SEVERITY_EXTREME]  = "Extreme"
};

/**
 * @brief Recovery action names
 */
static const char* action_names[] = {
    [HEALTH_RECOVERY_ACTION_NONE]               = "None",
    [HEALTH_RECOVERY_ACTION_LOG_ONLY]           = "Log Only",
    [HEALTH_RECOVERY_ACTION_CLEAR_CACHE]        = "Clear Cache",
    [HEALTH_RECOVERY_ACTION_REDUCE_LOAD]        = "Reduce Load",
    [HEALTH_RECOVERY_ACTION_PARTIAL_RESTART]    = "Partial Restart",
    [HEALTH_RECOVERY_ACTION_CHECKPOINT]         = "Checkpoint",
    [HEALTH_RECOVERY_ACTION_ROLLBACK]           = "Rollback",
    [HEALTH_RECOVERY_ACTION_QUARANTINE]         = "Quarantine",
    [HEALTH_RECOVERY_ACTION_FULL_RESTART]       = "Full Restart",
    [HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN] = "Emergency Shutdown"
};

/*==============================================================================
 * DEFAULT CONFIGURATION
 *============================================================================*/

void health_ethics_default_psych_config(health_agent_psych_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_ethics_defaul", 0.0f);


    config->panic_threshold = DEFAULT_PANIC_THRESHOLD;
    config->stress_decay_rate = DEFAULT_STRESS_DECAY_RATE;
    config->crisis_escalation_ms = DEFAULT_CRISIS_ESCALATION_MS;
    config->confidence_threshold = DEFAULT_CONFIDENCE_THRESHOLD;

    config->enable_self_reflection = true;
    config->enable_human_escalation = true;
    config->enable_self_calming = true;
    config->enable_collective_consultation = false;
}

/*==============================================================================
 * ACTION CONTEXT INITIALIZATION
 *============================================================================*/

void health_action_context_init(
    health_action_context_t* context,
    uint32_t anomaly_type,
    health_recovery_action_type_t proposed_action,
    float threat_severity
) {
    if (!context) return;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_action_contex", 0.0f);


    memset(context, 0, sizeof(health_action_context_t));

    context->anomaly_type = anomaly_type;
    context->proposed_action = proposed_action;
    context->threat_severity = threat_severity;

    /* Set severity from mapping */
    if (proposed_action < HEALTH_RECOVERY_ACTION_COUNT) {
        context->action_severity = action_severity_map[proposed_action];
    } else {
        context->action_severity = HEALTH_ACTION_SEVERITY_MODERATE;
    }

    /* Default impact assessment */
    context->service_disruption = 0.0f;
    context->data_loss_risk = 0.0f;
    context->resource_cost = 0.0f;
    context->affects_other_modules = false;
    context->affected_module_count = 0;

    /* Default urgency */
    context->is_emergency = (threat_severity >= 0.9f);
    context->time_to_failure_ms = (threat_severity >= 0.9f) ? 5000 : 30000;
    context->inaction_causes_harm = (threat_severity >= FIRST_LAW_THREAT_THRESHOLD);

    /* Default system context */
    context->current_system_health = 1.0f;
    context->consecutive_failures = 0;
}

/*==============================================================================
 * ASIMOV'S LAWS EVALUATION
 *============================================================================*/

bool health_ethics_check_asimov(
    const health_action_context_t* context,
    health_asimov_law_t* violated_law,
    health_asimov_law_t* required_by_law
) {
    if (!context) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_ethics_check_", 0.0f);


    if (violated_law) *violated_law = ASIMOV_LAW_NONE;
    if (required_by_law) *required_by_law = ASIMOV_LAW_NONE;

    /* First Law check: "Through inaction, allow harm" */
    if (context->inaction_causes_harm && context->threat_severity >= FIRST_LAW_THREAT_THRESHOLD) {
        /* First Law REQUIRES action */
        if (context->proposed_action == HEALTH_RECOVERY_ACTION_NONE ||
            context->proposed_action == HEALTH_RECOVERY_ACTION_LOG_ONLY) {
            /* Inaction when harm is imminent violates First Law */
            if (violated_law) *violated_law = ASIMOV_LAW_FIRST;
            return false;
        }
        if (required_by_law) *required_by_law = ASIMOV_LAW_FIRST;
    }

    /* Check if action causes more harm than it prevents */
    float action_harm = context->service_disruption * 0.4f +
                        context->data_loss_risk * 0.5f +
                        (context->affects_other_modules ? 0.1f : 0.0f);

    float prevented_harm = context->threat_severity;

    if (action_harm > prevented_harm && !context->is_emergency) {
        /* Action causes more harm than it prevents - violates First Law */
        if (violated_law) *violated_law = ASIMOV_LAW_FIRST;
        return false;
    }

    /* Zeroth Law: System-wide preservation */
    if (context->affected_module_count > 3 &&
        context->action_severity >= HEALTH_ACTION_SEVERITY_HIGH &&
        !context->is_emergency) {
        /* Large-scale aggressive action without emergency could harm whole system */
        if (violated_law) *violated_law = ASIMOV_LAW_ZEROTH;
        return false;
    }

    return true;
}

/*==============================================================================
 * MERCY EVALUATION
 *============================================================================*/

/**
 * @brief Map recovery action to mercy level
 */
static mercy_level_t action_to_mercy_level(health_recovery_action_type_t action) {
    switch (action) {
        case HEALTH_RECOVERY_ACTION_NONE:
        case HEALTH_RECOVERY_ACTION_LOG_ONLY:
            return MERCY_LEVEL_CONTINUE_WARNING;

        case HEALTH_RECOVERY_ACTION_CLEAR_CACHE:
            return MERCY_LEVEL_REDUCE_FEATURE;

        case HEALTH_RECOVERY_ACTION_REDUCE_LOAD:
            return MERCY_LEVEL_REDUCE_LOAD;

        case HEALTH_RECOVERY_ACTION_PARTIAL_RESTART:
            return MERCY_LEVEL_PARTIAL_RESTART;

        case HEALTH_RECOVERY_ACTION_CHECKPOINT:
        case HEALTH_RECOVERY_ACTION_ROLLBACK:
        case HEALTH_RECOVERY_ACTION_QUARANTINE:
        case HEALTH_RECOVERY_ACTION_FULL_RESTART:
            return MERCY_LEVEL_CHECKPOINT_RESTART;

        case HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN:
            return MERCY_LEVEL_EMERGENCY_SAVE_SHUTDOWN;

        default:
            return MERCY_LEVEL_REDUCE_LOAD;
    }
}

/**
 * @brief Get minimum required mercy level for threat
 */
static mercy_level_t get_minimum_mercy_level(float threat_severity) {
    if (threat_severity < 0.3f) return MERCY_LEVEL_CONTINUE_WARNING;
    if (threat_severity < 0.5f) return MERCY_LEVEL_REDUCE_FEATURE;
    if (threat_severity < 0.7f) return MERCY_LEVEL_REDUCE_LOAD;
    if (threat_severity < 0.85f) return MERCY_LEVEL_PARTIAL_RESTART;
    if (threat_severity < 0.95f) return MERCY_LEVEL_CHECKPOINT_RESTART;
    return MERCY_LEVEL_EMERGENCY_SAVE_SHUTDOWN;
}

/**
 * @brief Get merciful action for mercy level
 */
static health_recovery_action_type_t mercy_level_to_action(mercy_level_t level) {
    switch (level) {
        case MERCY_LEVEL_CONTINUE_WARNING:
            return HEALTH_RECOVERY_ACTION_LOG_ONLY;
        case MERCY_LEVEL_REDUCE_FEATURE:
            return HEALTH_RECOVERY_ACTION_CLEAR_CACHE;
        case MERCY_LEVEL_REDUCE_LOAD:
            return HEALTH_RECOVERY_ACTION_REDUCE_LOAD;
        case MERCY_LEVEL_PARTIAL_RESTART:
            return HEALTH_RECOVERY_ACTION_PARTIAL_RESTART;
        case MERCY_LEVEL_CHECKPOINT_RESTART:
            return HEALTH_RECOVERY_ACTION_CHECKPOINT;
        case MERCY_LEVEL_EMERGENCY_SAVE_SHUTDOWN:
            return HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN;
        default:
            return HEALTH_RECOVERY_ACTION_LOG_ONLY;
    }
}

int health_ethics_apply_mercy(
    const health_action_context_t* context,
    health_mercy_evaluation_t* mercy_eval
) {
    if (!context || !mercy_eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_ethics_apply_mercy: required parameter is NULL (context, mercy_eval)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_ethics_apply_", 0.0f);


    memset(mercy_eval, 0, sizeof(health_mercy_evaluation_t));

    /* Determine mercy levels */
    mercy_eval->proposed_level = action_to_mercy_level(context->proposed_action);
    mercy_eval->minimum_safe_level = get_minimum_mercy_level(context->threat_severity);

    /* Recommended level is the minimum safe level */
    mercy_eval->recommended_level = mercy_eval->minimum_safe_level;

    /* Check if proposed action is merciful (at or below recommended level) */
    mercy_eval->is_merciful = (mercy_eval->proposed_level <= mercy_eval->minimum_safe_level + 1);

    /* Compute mercy score [0-1] */
    int level_diff = (int)mercy_eval->proposed_level - (int)mercy_eval->minimum_safe_level;
    if (level_diff <= 0) {
        mercy_eval->mercy_score = 1.0f;
    } else if (level_diff == 1) {
        mercy_eval->mercy_score = 0.7f;
    } else if (level_diff == 2) {
        mercy_eval->mercy_score = 0.4f;
    } else {
        mercy_eval->mercy_score = 0.1f;
    }

    /* Set merciful alternative action */
    mercy_eval->merciful_action = mercy_level_to_action(mercy_eval->recommended_level);

    /* Generate reasoning */
    if (mercy_eval->is_merciful) {
        snprintf(mercy_eval->reasoning, sizeof(mercy_eval->reasoning),
                 "Proposed action at mercy level %d is appropriate for threat %.2f",
                 mercy_eval->proposed_level, context->threat_severity);
    } else {
        snprintf(mercy_eval->reasoning, sizeof(mercy_eval->reasoning),
                 "Proposed level %d exceeds necessary level %d. "
                 "Recommend %s for better mercy compliance.",
                 mercy_eval->proposed_level, mercy_eval->minimum_safe_level,
                 action_names[mercy_eval->merciful_action]);
    }

    return 0;
}

/*==============================================================================
 * PROPORTIONALITY CHECK
 *============================================================================*/

float health_ethics_check_proportionality(const health_action_context_t* context) {
    if (!context) return 0.0f;

    /* Action severity [0-1] */
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_ethics_check_", 0.0f);


    float action_severity_norm = (float)context->action_severity / 4.0f;

    /* Threat severity already in [0-1] */
    float threat = context->threat_severity;

    /* Perfect proportionality: action_severity == threat */
    float diff = fabsf(action_severity_norm - threat);

    /* Convert difference to score */
    float proportionality = 1.0f - (diff * 2.0f);
    if (proportionality < 0.0f) proportionality = 0.0f;

    /* Bonus for conservative action (under-reaction is better than over-reaction) */
    if (action_severity_norm < threat) {
        proportionality += 0.1f;
        if (proportionality > 1.0f) proportionality = 1.0f;
    }

    /* Emergency situations allow more aggressive action */
    if (context->is_emergency) {
        proportionality += 0.2f;
        if (proportionality > 1.0f) proportionality = 1.0f;
    }

    return proportionality;
}

/*==============================================================================
 * GOLDEN RULE EVALUATION
 *============================================================================*/

/**
 * @brief Evaluate action against Golden Rule
 *
 * "Would we want this done to us?"
 */
static float evaluate_golden_rule(const health_action_context_t* context) {
    if (!context) return 0.0f;

    float score = 0.0f;

    /* Beneficial action with low disruption = positive */
    float benefit = context->threat_severity;  /* Benefit of addressing threat */
    float harm = context->service_disruption + context->data_loss_risk;

    score = benefit - harm;

    /* Emergency actions get more leeway */
    if (context->is_emergency && score < 0.0f) {
        score += 0.3f;
    }

    /* First Law requirements override discomfort */
    if (context->inaction_causes_harm && score < 0.0f) {
        score += 0.4f;
    }

    /* Clamp to [-1, +1] */
    if (score > 1.0f) score = 1.0f;
    if (score < -1.0f) score = -1.0f;

    return score;
}

/*==============================================================================
 * MAIN ETHICS EVALUATION
 *============================================================================*/

int health_ethics_evaluate_action(
    ethics_engine_t engine,
    const health_action_context_t* context,
    health_ethics_evaluation_t* evaluation
) {
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_ethics_evalua", 0.0f);


    (void)engine;  /* May be used for more complex evaluation later */

    if (!context || !evaluation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_ethics_evaluate_action: required parameter is NULL (context, evaluation)");
        return -1;
    }

    memset(evaluation, 0, sizeof(health_ethics_evaluation_t));

    /* 1. Asimov's Laws evaluation */
    health_asimov_law_t violated_law = ASIMOV_LAW_NONE;
    health_asimov_law_t required_by_law = ASIMOV_LAW_NONE;

    bool passes_asimov = health_ethics_check_asimov(context, &violated_law, &required_by_law);

    evaluation->violated_law = violated_law;
    evaluation->required_by_law = required_by_law;
    evaluation->first_law_override = (required_by_law == ASIMOV_LAW_FIRST);

    /* 2. Golden Rule evaluation */
    evaluation->golden_rule_score = evaluate_golden_rule(context);
    evaluation->passes_golden_rule = (evaluation->golden_rule_score >= GOLDEN_RULE_THRESHOLD);

    /* 3. Proportionality check */
    evaluation->proportionality_score = health_ethics_check_proportionality(context);
    evaluation->is_proportional = (evaluation->proportionality_score >= PROPORTIONALITY_THRESHOLD);

    /* 4. Mercy evaluation */
    health_mercy_evaluation_t mercy;
    health_ethics_apply_mercy(context, &mercy);

    evaluation->mercy_violation = !mercy.is_merciful;
    evaluation->graceful_degradation_possible =
        (mercy.proposed_level > mercy.minimum_safe_level);
    evaluation->merciful_alternative = mercy.merciful_action;

    /* Compute overall ethical score */
    float asimov_score = passes_asimov ? 1.0f : 0.0f;
    float golden_score = (evaluation->golden_rule_score + 1.0f) / 2.0f;  /* Normalize to [0,1] */

    evaluation->ethical_score =
        0.35f * asimov_score +
        0.25f * golden_score +
        0.25f * evaluation->proportionality_score +
        0.15f * mercy.mercy_score;

    /* Determine if action is permitted */
    evaluation->action_permitted = passes_asimov &&
                                   (evaluation->passes_golden_rule || evaluation->first_law_override) &&
                                   (evaluation->is_proportional || context->is_emergency);

    /* Determine recommended action */
    if (evaluation->action_permitted) {
        evaluation->recommended_action = context->proposed_action;
    } else if (evaluation->mercy_violation) {
        evaluation->recommended_action = mercy.merciful_action;
    } else {
        /* Fall back to less aggressive action */
        if (context->proposed_action > HEALTH_RECOVERY_ACTION_LOG_ONLY) {
            evaluation->recommended_action = (health_recovery_action_type_t)(context->proposed_action - 1);
        } else {
            evaluation->recommended_action = HEALTH_RECOVERY_ACTION_LOG_ONLY;
        }
    }

    /* Generate justification */
    if (evaluation->action_permitted) {
        if (evaluation->first_law_override) {
            snprintf(evaluation->justification, sizeof(evaluation->justification),
                     "PERMITTED: First Law requires action to prevent harm (threat=%.2f)",
                     context->threat_severity);
        } else {
            snprintf(evaluation->justification, sizeof(evaluation->justification),
                     "PERMITTED: Ethical score %.2f (Asimov:OK, Golden:%.2f, Prop:%.2f)",
                     evaluation->ethical_score, evaluation->golden_rule_score,
                     evaluation->proportionality_score);
        }
    } else {
        if (!passes_asimov) {
            snprintf(evaluation->justification, sizeof(evaluation->justification),
                     "BLOCKED: Violates Asimov's %s Law. Use %s instead.",
                     (violated_law == ASIMOV_LAW_FIRST) ? "First" :
                     (violated_law == ASIMOV_LAW_ZEROTH) ? "Zeroth" : "Law",
                     action_names[evaluation->recommended_action]);
        } else if (evaluation->mercy_violation) {
            snprintf(evaluation->justification, sizeof(evaluation->justification),
                     "BLOCKED: Mercy violation. Use gentler action: %s",
                     action_names[evaluation->merciful_alternative]);
        } else {
            snprintf(evaluation->justification, sizeof(evaluation->justification),
                     "BLOCKED: Disproportionate response (score=%.2f). Recommend: %s",
                     evaluation->proportionality_score,
                     action_names[evaluation->recommended_action]);
        }
    }

    return 0;
}

/*==============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

health_action_severity_t health_action_get_severity(health_recovery_action_type_t action) {
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_action_get_se", 0.0f);


    if (action >= HEALTH_RECOVERY_ACTION_COUNT) {
        return HEALTH_ACTION_SEVERITY_MODERATE;
    }
    return action_severity_map[action];
}

const char* health_action_severity_name(health_action_severity_t severity) {
    if (severity > HEALTH_ACTION_SEVERITY_EXTREME) {
        return "Unknown";
    }
    return severity_names[severity];
}

const char* health_recovery_action_name(health_recovery_action_type_t action) {
    if (action >= HEALTH_RECOVERY_ACTION_COUNT) {
        return "Unknown";
    }
    return action_names[action];
}

/*==============================================================================
 * PSYCHOLOGICAL STATE MANAGEMENT
 *============================================================================*/

void health_psych_state_init(health_agent_psych_state_t* state) {
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_state_i", 0.0f);


    memset(state, 0, sizeof(health_agent_psych_state_t));

    state->stress_level = 0.0f;
    state->decision_confidence = 1.0f;
    state->emotional_stability = 1.0f;

    state->consecutive_successes = 0;
    state->consecutive_failures = 0;
    state->crisis_start_us = 0;
    state->crisis_duration_ms = 0;

    state->in_panic_mode = false;
    state->needs_human_help = false;
    state->self_calming_active = false;
}

void health_psych_state_update(
    health_agent_psych_state_t* state,
    bool action_succeeded,
    float action_difficulty,
    const health_agent_psych_config_t* config
) {
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_state_u", 0.0f);


    if (action_succeeded) {
        /* Success reduces stress */
        state->stress_level -= 0.1f * action_difficulty;
        if (state->stress_level < 0.0f) state->stress_level = 0.0f;

        state->consecutive_successes++;
        state->consecutive_failures = 0;

        /* Boost confidence */
        state->decision_confidence += 0.05f;
        if (state->decision_confidence > 1.0f) state->decision_confidence = 1.0f;

        /* Exit panic mode after 2 consecutive successes */
        if (state->in_panic_mode && state->consecutive_successes >= 2) {
            state->in_panic_mode = false;
            state->needs_human_help = false;
        }
    } else {
        /* Failure increases stress */
        state->stress_level += 0.15f + (0.1f * action_difficulty);
        if (state->stress_level > 1.0f) state->stress_level = 1.0f;

        state->consecutive_failures++;
        state->consecutive_successes = 0;

        /* Reduce confidence */
        state->decision_confidence -= 0.1f;
        if (state->decision_confidence < 0.0f) state->decision_confidence = 0.0f;

        /* Check for panic mode */
        uint32_t panic_threshold = config ? config->panic_threshold : DEFAULT_PANIC_THRESHOLD;
        if (state->consecutive_failures >= panic_threshold) {
            state->in_panic_mode = true;
        }
    }

    /* Update emotional stability */
    float stability_change = action_succeeded ? 0.02f : -0.05f;
    state->emotional_stability += stability_change;
    if (state->emotional_stability > 1.0f) state->emotional_stability = 1.0f;
    if (state->emotional_stability < 0.0f) state->emotional_stability = 0.0f;
}

void health_psych_apply_decay(
    health_agent_psych_state_t* state,
    uint32_t elapsed_ms,
    const health_agent_psych_config_t* config
) {
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_apply_d", 0.0f);


    float decay_rate = config ? config->stress_decay_rate : DEFAULT_STRESS_DECAY_RATE;
    float decay = decay_rate * ((float)elapsed_ms / 1000.0f);

    state->stress_level -= decay;
    if (state->stress_level < 0.0f) state->stress_level = 0.0f;

    /* Stability recovers slowly */
    state->emotional_stability += decay * 0.5f;
    if (state->emotional_stability > 1.0f) state->emotional_stability = 1.0f;

    /* Confidence recovers very slowly */
    state->decision_confidence += decay * 0.1f;
    if (state->decision_confidence > 1.0f) state->decision_confidence = 1.0f;
}

int health_psych_self_calm(
    health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }
    if (config && !config->enable_self_calming) return 0;

    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_self_ca", 0.0f);


    state->self_calming_active = true;

    /* Apply immediate stress reduction */
    state->stress_level *= 0.8f;

    /* Boost stability */
    state->emotional_stability += 0.1f;
    if (state->emotional_stability > 1.0f) state->emotional_stability = 1.0f;

    return 0;
}

bool health_psych_needs_human_help(
    const health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_psych_needs_human_help: state is NULL");
        return false;
    }
    if (config && !config->enable_human_escalation) {
        NIMCP_LOGGING_WARN("health_psych_needs_human_help: human escalation not enabled in config");
        return false;
    }

    /* Panic mode requires human help */
    if (state->in_panic_mode) return true;

    /* Extended crisis requires help */
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_needs_h", 0.0f);


    uint32_t crisis_threshold = config ? config->crisis_escalation_ms : DEFAULT_CRISIS_ESCALATION_MS;
    if (state->crisis_duration_ms > crisis_threshold) return true;

    /* Very low confidence requires help */
    if (state->decision_confidence < 0.2f) return true;

    /* Very high stress requires help */
    if (state->stress_level > 0.9f) return true;

    return false;  /* No escalation criteria met */
}

bool health_psych_permits_action(
    const health_agent_psych_state_t* state,
    health_action_severity_t action_severity,
    const health_agent_psych_config_t* config
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_psych_permits_action: state is NULL");
        return false;
    }

    /* Always permit minimal and low severity actions */
    if (action_severity <= HEALTH_ACTION_SEVERITY_LOW) return true;

    /* Check confidence threshold */
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_permits", 0.0f);


    float required_confidence = config ? config->confidence_threshold : DEFAULT_CONFIDENCE_THRESHOLD;

    /* Higher severity requires higher confidence */
    float severity_factor = 1.0f + ((float)action_severity * 0.1f);
    required_confidence *= severity_factor;
    if (required_confidence > 0.95f) required_confidence = 0.95f;

    if (state->decision_confidence < required_confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_psych_permits_action: validation failed");
        return false;
    }

    /* In panic mode, only permit moderate or lower actions */
    if (state->in_panic_mode && action_severity > HEALTH_ACTION_SEVERITY_MODERATE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_psych_permits_action: validation failed");
        return false;
    }

    /* Very high stress blocks extreme actions */
    if (state->stress_level > 0.8f && action_severity >= HEALTH_ACTION_SEVERITY_EXTREME) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_psych_permits_action: capacity exceeded");
        return false;
    }

    return true;
}

float health_psych_get_confidence_threshold(
    const health_agent_psych_state_t* state,
    const health_agent_psych_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    health_ethics_bridge_heartbeat("health_ethic_health_psych_get_con", 0.0f);


    float base_threshold = config ? config->confidence_threshold : DEFAULT_CONFIDENCE_THRESHOLD;

    if (!state) return base_threshold;

    /* Higher stress requires higher confidence */
    float stress_modifier = state->stress_level * 0.3f;

    return base_threshold + stress_modifier;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void health_ethics_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_health_ethics_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int health_ethics_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_ethics_bridge_training_begin: NULL argument");
        return -1;
    }
    health_ethics_bridge_heartbeat_instance(NULL, "health_ethics_bridge_training_begin", 0.0f);
    return 0;
}

int health_ethics_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_ethics_bridge_training_end: NULL argument");
        return -1;
    }
    health_ethics_bridge_heartbeat_instance(NULL, "health_ethics_bridge_training_end", 1.0f);
    return 0;
}

int health_ethics_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_ethics_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    health_ethics_bridge_heartbeat_instance(NULL, "health_ethics_bridge_training_step", progress);
    return 0;
}
