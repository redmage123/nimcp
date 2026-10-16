/**
 * @file nimcp_health_emotion_bridge.c
 * @brief Health Agent Emotion Integration Bridge Implementation
 *
 * WHAT: Bidirectional emotion-health integration
 * WHY:  Emotional state affects appropriate health thresholds
 * HOW:  Query emotion system, adjust monitoring sensitivity
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#include "cognitive/emotion/nimcp_health_emotion_bridge.h"
#include "utils/memory/nimcp_memory.h"

/* Forward-declare health agent public API to avoid header conflicts */
typedef struct {
    uint64_t uptime_ms;
    uint64_t checks_performed;
    uint64_t anomalies_detected;
    uint64_t messages_sent;
    uint64_t recoveries_triggered;
    uint64_t recoveries_succeeded;
    uint64_t heartbeats_received;
    uint64_t heartbeat_timeouts;
    uint64_t deadlocks_detected;
    uint64_t nans_detected;
    uint64_t memory_corruptions;
    uint64_t consistency_failures;
    float avg_check_duration_us;
    float avg_message_latency_us;
    uint32_t queue_high_watermark;
    int highest_severity_seen;
} health_agent_stats_local_t;

extern int nimcp_health_agent_get_stats(nimcp_health_agent_t* agent,
                                         health_agent_stats_local_t* stats);
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(health_emotion_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_health_emotion_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_health_emotion_bridge_mesh_registry = NULL;

nimcp_error_t health_emotion_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_health_emotion_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "health_emotion_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "health_emotion_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_health_emotion_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_health_emotion_bridge_mesh_registry = registry;
    return err;
}

void health_emotion_bridge_mesh_unregister(void) {
    if (g_health_emotion_bridge_mesh_registry && g_health_emotion_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_health_emotion_bridge_mesh_registry, g_health_emotion_bridge_mesh_id);
        g_health_emotion_bridge_mesh_id = 0;
        g_health_emotion_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from health_emotion_bridge module (instance-level) */
static inline void health_emotion_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_health_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_health_emotion_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_health_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "HEALTH_EMOTION_BRIDGE"


/*==============================================================================
 * CONSTANTS
 *============================================================================*/

/** High stress threshold */
#define HIGH_STRESS_AROUSAL 0.7f

/** Negative valence threshold */
#define NEGATIVE_VALENCE_THRESHOLD -0.3f

/** Positive valence threshold */
#define POSITIVE_VALENCE_THRESHOLD 0.3f

/** Low stability threshold */
#define LOW_STABILITY_THRESHOLD 0.4f

/** Neutral valence threshold (for is_positive/is_negative) */
#define NEUTRAL_VALENCE_THRESHOLD 0.2f

/** Calm arousal threshold */
#define CALM_AROUSAL_THRESHOLD 0.3f

/*==============================================================================
 * DEFAULT FACTORS
 *============================================================================*/

void health_emotion_default_factors(threshold_adjustment_factors_t* factors) {
    if (!factors) return;

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_defau", 0.0f);


    factors->high_stress_modifier = 0.8f;        /* Lower thresholds by 20% */
    factors->negative_valence_modifier = 1.2f;   /* Raise sensitivity by 20% */
    factors->positive_valence_modifier = 0.9f;   /* Relax thresholds by 10% */
    factors->instability_modifier = 1.3f;        /* Raise sensitivity by 30% */
    factors->inflammation_modifier = 1.25f;      /* Raise sensitivity by 25% */
}

/*==============================================================================
 * EVENT MAPPING
 *============================================================================*/

/**
 * @brief Default emotion mappings for health events
 */
static const health_event_emotion_mapping_t default_event_mappings[] = {
    [HEALTH_EMOTION_EVENT_MINOR_ANOMALY] = {
        .valence_delta = -0.05f,
        .arousal_delta = 0.1f,
        .duration_ms = 5000,
        .triggers_fear = false,
        .triggers_relief = false,
        .triggers_stress = false
    },
    [HEALTH_EMOTION_EVENT_MODERATE_ANOMALY] = {
        .valence_delta = -0.15f,
        .arousal_delta = 0.25f,
        .duration_ms = 15000,
        .triggers_fear = false,
        .triggers_relief = false,
        .triggers_stress = true
    },
    [HEALTH_EMOTION_EVENT_CRITICAL_ANOMALY] = {
        .valence_delta = -0.4f,
        .arousal_delta = 0.5f,
        .duration_ms = 30000,
        .triggers_fear = true,
        .triggers_relief = false,
        .triggers_stress = true
    },
    [HEALTH_EMOTION_EVENT_RECOVERY_SUCCESS] = {
        .valence_delta = 0.2f,
        .arousal_delta = -0.15f,
        .duration_ms = 10000,
        .triggers_fear = false,
        .triggers_relief = true,
        .triggers_stress = false
    },
    [HEALTH_EMOTION_EVENT_RECOVERY_FAILURE] = {
        .valence_delta = -0.25f,
        .arousal_delta = 0.3f,
        .duration_ms = 20000,
        .triggers_fear = true,
        .triggers_relief = false,
        .triggers_stress = true
    },
    [HEALTH_EMOTION_EVENT_SYSTEM_STABLE] = {
        .valence_delta = 0.1f,
        .arousal_delta = -0.2f,
        .duration_ms = 5000,
        .triggers_fear = false,
        .triggers_relief = true,
        .triggers_stress = false
    },
    [HEALTH_EMOTION_EVENT_PROLONGED_CRISIS] = {
        .valence_delta = -0.35f,
        .arousal_delta = 0.15f,  /* Arousal may decrease due to exhaustion */
        .duration_ms = 60000,
        .triggers_fear = true,
        .triggers_relief = false,
        .triggers_stress = true
    }
};

/**
 * @brief Event type names
 */
static const char* event_type_names[] = {
    [HEALTH_EMOTION_EVENT_MINOR_ANOMALY]    = "Minor Anomaly",
    [HEALTH_EMOTION_EVENT_MODERATE_ANOMALY] = "Moderate Anomaly",
    [HEALTH_EMOTION_EVENT_CRITICAL_ANOMALY] = "Critical Anomaly",
    [HEALTH_EMOTION_EVENT_RECOVERY_SUCCESS] = "Recovery Success",
    [HEALTH_EMOTION_EVENT_RECOVERY_FAILURE] = "Recovery Failure",
    [HEALTH_EMOTION_EVENT_SYSTEM_STABLE]    = "System Stable",
    [HEALTH_EMOTION_EVENT_PROLONGED_CRISIS] = "Prolonged Crisis"
};

/**
 * @brief Recovery action names
 */
static const char* recovery_action_names[] = {
    [HEALTH_RECOVERY_NONE]               = "None",
    [HEALTH_RECOVERY_LOG_ONLY]           = "Log Only",
    [HEALTH_RECOVERY_CLEAR_CACHE]        = "Clear Cache",
    [HEALTH_RECOVERY_REDUCE_LOAD]        = "Reduce Load",
    [HEALTH_RECOVERY_PARTIAL_RESTART]    = "Partial Restart",
    [HEALTH_RECOVERY_FULL_RESTART]       = "Full Restart",
    [HEALTH_RECOVERY_QUARANTINE]         = "Quarantine",
    [HEALTH_RECOVERY_ROLLBACK]           = "Rollback",
    [HEALTH_RECOVERY_EMERGENCY_SHUTDOWN] = "Emergency Shutdown"
};

/**
 * @brief Shadow pattern names
 */
static const char* shadow_pattern_names[] = {
    [HEALTH_SHADOW_NONE]               = "None",
    [HEALTH_SHADOW_HYPERVIGILANCE]     = "Hypervigilance",
    [HEALTH_SHADOW_OVERREACTION]       = "Overreaction",
    [HEALTH_SHADOW_DENIAL]             = "Denial",
    [HEALTH_SHADOW_PROCRASTINATION]    = "Procrastination",
    [HEALTH_SHADOW_OBSESSIVE_CHECKING] = "Obsessive Checking",
    [HEALTH_SHADOW_NEVER_GOOD_ENOUGH]  = "Never Good Enough",
    [HEALTH_SHADOW_OVER_ESCALATION]    = "Over Escalation",
    [HEALTH_SHADOW_DECISION_PARALYSIS] = "Decision Paralysis"
};

/**
 * @brief Inflammation level names
 */
static const char* inflammation_names[] = {
    [INFLAMMATION_NONE]     = "None",
    [INFLAMMATION_LOW]      = "Low",
    [INFLAMMATION_MODERATE] = "Moderate",
    [INFLAMMATION_HIGH]     = "High",
    [INFLAMMATION_SEVERE]   = "Severe"
};

/*==============================================================================
 * THRESHOLD COMPUTATION
 *============================================================================*/

int health_emotion_compute_thresholds(
    nimcp_health_agent_t* agent,
    const emotional_system_t* emotion_system,
    const threshold_adjustment_factors_t* factors,
    emotion_adjusted_thresholds_t* thresholds
) {
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat_instance(agent, "health_emoti_health_emotion_compu", 0.0f);

    if (!thresholds) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thresholds is NULL");


        return -1;


    }

    /* Get default factors if not provided */
    threshold_adjustment_factors_t local_factors;
    if (!factors) {
        health_emotion_default_factors(&local_factors);
        factors = &local_factors;
    }

    /* Default thresholds */
    thresholds->memory_warning_threshold = 0.75f;
    thresholds->memory_critical_threshold = 0.90f;
    thresholds->cpu_warning_threshold = 0.80f;
    thresholds->cpu_critical_threshold = 0.95f;
    thresholds->anomaly_sensitivity = 1.0f;
    thresholds->recovery_aggressiveness = 0.5f;
    thresholds->check_frequency_modifier = 1.0f;
    thresholds->response_delay_modifier = 1.0f;

    /* If no emotion system, return defaults */
    if (!emotion_system) return 0;

    /* Query emotional state */
    health_emotion_state_t emotion_state;
    int result = health_emotion_get_state(emotion_system, &emotion_state);
    if (result != 0) return 0;  /* Use defaults on error */

    /* Apply stress-based adjustments */
    if (emotion_state.is_stressed) {
        /* Lower thresholds (more sensitive) */
        thresholds->memory_warning_threshold *= factors->high_stress_modifier;
        thresholds->cpu_warning_threshold *= factors->high_stress_modifier;
        thresholds->check_frequency_modifier = 1.5f;  /* More frequent checks */
        thresholds->response_delay_modifier = 0.7f;   /* Faster response */
    }

    /* Apply valence-based adjustments */
    if (emotion_state.is_negative) {
        /* Increase sensitivity */
        thresholds->anomaly_sensitivity *= factors->negative_valence_modifier;
        thresholds->recovery_aggressiveness *= 0.8f;  /* More conservative */
    } else if (emotion_state.is_positive) {
        /* Relax thresholds slightly */
        thresholds->memory_warning_threshold *= (1.0f / factors->positive_valence_modifier);
        thresholds->cpu_warning_threshold *= (1.0f / factors->positive_valence_modifier);
        thresholds->recovery_aggressiveness *= 1.1f;  /* Can be more aggressive */
    }

    /* Apply stability-based adjustments */
    if (emotion_state.stability < LOW_STABILITY_THRESHOLD) {
        thresholds->anomaly_sensitivity *= factors->instability_modifier;
        thresholds->recovery_aggressiveness *= 0.7f;  /* Very conservative */
    }

    /* Clamp values to reasonable ranges */
    if (thresholds->memory_warning_threshold < 0.5f)
        thresholds->memory_warning_threshold = 0.5f;
    if (thresholds->memory_warning_threshold > 0.9f)
        thresholds->memory_warning_threshold = 0.9f;

    if (thresholds->cpu_warning_threshold < 0.5f)
        thresholds->cpu_warning_threshold = 0.5f;
    if (thresholds->cpu_warning_threshold > 0.95f)
        thresholds->cpu_warning_threshold = 0.95f;

    if (thresholds->anomaly_sensitivity < 0.5f)
        thresholds->anomaly_sensitivity = 0.5f;
    if (thresholds->anomaly_sensitivity > 2.0f)
        thresholds->anomaly_sensitivity = 2.0f;

    if (thresholds->recovery_aggressiveness < 0.2f)
        thresholds->recovery_aggressiveness = 0.2f;
    if (thresholds->recovery_aggressiveness > 1.0f)
        thresholds->recovery_aggressiveness = 1.0f;

    return 0;
}

/*==============================================================================
 * EMOTIONAL STATE QUERY
 *============================================================================*/

int health_emotion_get_state(
    const emotional_system_t* emotion_system,
    health_emotion_state_t* state
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_get_s", 0.0f);


    memset(state, 0, sizeof(health_emotion_state_t));

    /* Default to neutral if no emotion system */
    if (!emotion_system) {
        state->valence = 0.0f;
        state->arousal = 0.3f;
        state->stability = 0.8f;
        state->stress_index = 0.2f;
        state->is_calm = true;
        return 0;
    }

    /* Query emotion system - in real implementation, this would call
     * emotion_system_get_state() or similar */

    /* TODO: call emotion_system_get_state(emotion_system, &raw_state) once the API
     * signature is finalized. Using neutral defaults until then. */
    state->valence = 0.0f;
    state->arousal = 0.4f;
    state->stability = 0.7f;

    /* Derive stress index from arousal and valence */
    state->stress_index = state->arousal * (1.0f - ((state->valence + 1.0f) / 2.0f));

    /* Set boolean flags */
    state->is_positive = (state->valence > NEUTRAL_VALENCE_THRESHOLD);
    state->is_negative = (state->valence < -NEUTRAL_VALENCE_THRESHOLD);
    state->is_stressed = (state->arousal > HIGH_STRESS_AROUSAL);
    state->is_calm = (state->arousal < CALM_AROUSAL_THRESHOLD);

    return 0;
}

/*==============================================================================
 * EVENT REPORTING
 *============================================================================*/

void health_emotion_get_event_mapping(
    health_emotion_event_type_t event_type,
    float severity,
    health_event_emotion_mapping_t* mapping
) {
    if (!mapping) return;

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_get_e", 0.0f);


    if (event_type >= HEALTH_EMOTION_EVENT_COUNT) {
        event_type = HEALTH_EMOTION_EVENT_MINOR_ANOMALY;
    }

    /* Copy base mapping */
    *mapping = default_event_mappings[event_type];

    /* Scale by severity */
    mapping->valence_delta *= severity;
    mapping->arousal_delta *= severity;
    mapping->duration_ms = (uint32_t)(mapping->duration_ms * (0.5f + severity));
}

int health_emotion_report_event(
    nimcp_health_agent_t* agent,
    emotional_system_t* emotion_system,
    health_emotion_event_type_t event_type,
    float severity
) {
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat_instance(agent, "health_emoti_health_emotion_repor", 0.0f);

    if (!emotion_system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system is NULL");


        return -1;


    }

    if (event_type >= HEALTH_EMOTION_EVENT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "health_emotion_report_event: capacity exceeded");
        return -1;
    }
    if (severity < 0.0f || severity > 1.0f) severity = 0.5f;

    /* Get emotion mapping */
    health_event_emotion_mapping_t mapping;
    health_emotion_get_event_mapping(event_type, severity, &mapping);

    /* In real implementation, this would call emotion system APIs:
     * - emotion_system_inject_event()
     * - emotion_system_modulate_valence()
     * - emotion_system_modulate_arousal()
     *
     * For now, we just return success as the emotion system
     * integration will be completed in a later phase */

    (void)mapping;  /* Will be used in full implementation */

    return 0;
}

/*==============================================================================
 * ACTION GATING
 *============================================================================*/

bool health_emotion_permits_action(
    const emotional_system_t* emotion_system,
    health_recovery_action_t action
) {
    if (!emotion_system) return true;  /* No emotion system = no restrictions */

    /* Query emotional state */
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_permi", 0.0f);


    health_emotion_state_t state;
    if (health_emotion_get_state(emotion_system, &state) != 0) {
        return true;  /* Error querying = allow action */
    }

    /* Low severity actions always permitted */
    if (action <= HEALTH_RECOVERY_CLEAR_CACHE) {
        return true;
    }

    /* Check for emotional instability */
    if (state.stability < LOW_STABILITY_THRESHOLD) {
        /* Only allow low-impact actions during instability */
        if (action >= HEALTH_RECOVERY_FULL_RESTART) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_emotion_permits_action: capacity exceeded");
            return false;
        }
    }

    /* Check for high stress */
    if (state.is_stressed) {
        /* Avoid aggressive actions under stress unless critical */
        if (action >= HEALTH_RECOVERY_EMERGENCY_SHUTDOWN) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_emotion_permits_action: capacity exceeded");
            return false;
        }
    }

    /* Check for very negative valence */
    if (state.valence < -0.5f) {
        /* Avoid irreversible actions during emotional crisis */
        if (action >= HEALTH_RECOVERY_QUARANTINE) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_emotion_permits_action: capacity exceeded");
            return false;
        }
    }

    return true;
}

int health_emotion_adjust_recovery(
    const emotional_system_t* emotion_system,
    health_recovery_action_t proposed_action,
    health_recovery_action_t* adjusted_action
) {
    if (!adjusted_action) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adjusted_action is NULL");

        return -1;

    }

    *adjusted_action = proposed_action;

    if (!emotion_system) return 0;

    /* Check if proposed action is permitted */
    if (!health_emotion_permits_action(emotion_system, proposed_action)) {
        /* Downgrade to next lower severity action */
        if (proposed_action > HEALTH_RECOVERY_LOG_ONLY) {
            *adjusted_action = (health_recovery_action_t)(proposed_action - 1);
        }
    }

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_adjus", 0.0f);


    return 0;
}

/*==============================================================================
 * SHADOW PATTERN DETECTION
 *============================================================================*/

int health_agent_detect_shadow_patterns(
    const nimcp_health_agent_t* agent,
    shadow_detection_result_t* detected_patterns,
    uint32_t max_patterns,
    uint32_t* num_detected
) {
    if (!detected_patterns || !num_detected || max_patterns == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_agent_detect_shadow_patterns: required parameter is NULL (detected_patterns, num_detected)");
        return -1;
    }

    *num_detected = 0;

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat_instance((nimcp_health_agent_t*)agent, "health_emoti_health_agent_detect_", 0.0f);

    if (!agent) return 0;

    /* Get agent stats to analyze behavioral patterns */
    health_agent_stats_local_t stats;
    int rc = nimcp_health_agent_get_stats((nimcp_health_agent_t*)agent, &stats);
    if (rc != 0) return 0;

    uint32_t idx = 0;
    uint64_t now_us = stats.uptime_ms * 1000;

    /* Hypervigilance: anomalies detected >> recoveries (false positive rate > 50%) */
    if (stats.checks_performed > 100 && stats.anomalies_detected > 0) {
        float false_positive_rate = 1.0f;
        if (stats.recoveries_triggered > 0) {
            false_positive_rate = 1.0f - ((float)stats.recoveries_succeeded /
                                           (float)stats.anomalies_detected);
        }
        if (false_positive_rate > 0.5f && idx < max_patterns) {
            detected_patterns[idx].pattern = HEALTH_SHADOW_HYPERVIGILANCE;
            detected_patterns[idx].intensity = false_positive_rate;
            detected_patterns[idx].occurrence_count = (uint32_t)(stats.anomalies_detected - stats.recoveries_succeeded);
            detected_patterns[idx].first_observed_us = 0;
            detected_patterns[idx].last_observed_us = now_us;
            snprintf(detected_patterns[idx].evidence, sizeof(detected_patterns[idx].evidence),
                     "False positive rate %.1f%% (%lu anomalies, %lu succeeded)",
                     false_positive_rate * 100.0f,
                     (unsigned long)stats.anomalies_detected,
                     (unsigned long)stats.recoveries_succeeded);
            idx++;
        }
    }

    /* Obsessive checking: very high check frequency relative to anomalies */
    if (stats.checks_performed > 1000 && stats.anomalies_detected > 0) {
        float check_to_anomaly = (float)stats.checks_performed / (float)stats.anomalies_detected;
        if (check_to_anomaly > 500.0f && idx < max_patterns) {
            detected_patterns[idx].pattern = HEALTH_SHADOW_OBSESSIVE_CHECKING;
            detected_patterns[idx].intensity = (check_to_anomaly > 2000.0f) ? 1.0f :
                                                check_to_anomaly / 2000.0f;
            detected_patterns[idx].occurrence_count = (uint32_t)stats.checks_performed;
            detected_patterns[idx].first_observed_us = 0;
            detected_patterns[idx].last_observed_us = now_us;
            snprintf(detected_patterns[idx].evidence, sizeof(detected_patterns[idx].evidence),
                     "%.0f checks per anomaly (threshold 500)",
                     check_to_anomaly);
            idx++;
        }
    }

    /* Denial: many heartbeat timeouts but few anomalies detected */
    if (stats.heartbeat_timeouts > 5 && stats.anomalies_detected < stats.heartbeat_timeouts) {
        if (idx < max_patterns) {
            float denial_intensity = (float)stats.heartbeat_timeouts /
                                     (float)(stats.anomalies_detected + stats.heartbeat_timeouts);
            detected_patterns[idx].pattern = HEALTH_SHADOW_DENIAL;
            detected_patterns[idx].intensity = denial_intensity;
            detected_patterns[idx].occurrence_count = (uint32_t)stats.heartbeat_timeouts;
            detected_patterns[idx].first_observed_us = 0;
            detected_patterns[idx].last_observed_us = now_us;
            snprintf(detected_patterns[idx].evidence, sizeof(detected_patterns[idx].evidence),
                     "%lu timeouts but only %lu anomalies detected",
                     (unsigned long)stats.heartbeat_timeouts,
                     (unsigned long)stats.anomalies_detected);
            idx++;
        }
    }

    /* NaN corruption detection frequency */
    if (stats.nans_detected > 10 && stats.memory_corruptions > 5 && idx < max_patterns) {
        detected_patterns[idx].pattern = HEALTH_SHADOW_NEVER_GOOD_ENOUGH;
        detected_patterns[idx].intensity = (float)(stats.nans_detected + stats.memory_corruptions) /
                                            (float)(stats.checks_performed + 1);
        if (detected_patterns[idx].intensity > 1.0f) detected_patterns[idx].intensity = 1.0f;
        detected_patterns[idx].occurrence_count = (uint32_t)(stats.nans_detected + stats.memory_corruptions);
        detected_patterns[idx].first_observed_us = 0;
        detected_patterns[idx].last_observed_us = now_us;
        snprintf(detected_patterns[idx].evidence, sizeof(detected_patterns[idx].evidence),
                 "%lu NaNs + %lu corruptions detected",
                 (unsigned long)stats.nans_detected,
                 (unsigned long)stats.memory_corruptions);
        idx++;
    }

    *num_detected = idx;
    return 0;
}

shadow_intervention_type_t health_shadow_get_intervention(health_shadow_pattern_t pattern) {
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_shadow_get_in", 0.0f);


    switch (pattern) {
        case HEALTH_SHADOW_HYPERVIGILANCE:
            return SHADOW_INTERVENTION_RAISE_THRESHOLD;
        case HEALTH_SHADOW_OVERREACTION:
            return SHADOW_INTERVENTION_RAISE_THRESHOLD;
        case HEALTH_SHADOW_DENIAL:
            return SHADOW_INTERVENTION_LOWER_THRESHOLD;
        case HEALTH_SHADOW_PROCRASTINATION:
            return SHADOW_INTERVENTION_REQUIRE_ACTION;
        case HEALTH_SHADOW_OBSESSIVE_CHECKING:
            return SHADOW_INTERVENTION_LIMIT_FREQUENCY;
        case HEALTH_SHADOW_NEVER_GOOD_ENOUGH:
            return SHADOW_INTERVENTION_ACCEPT_GOOD_ENOUGH;
        case HEALTH_SHADOW_OVER_ESCALATION:
            return SHADOW_INTERVENTION_REDUCE_ESCALATION;
        case HEALTH_SHADOW_DECISION_PARALYSIS:
            return SHADOW_INTERVENTION_FORCE_DEFAULT;
        default:
            return SHADOW_INTERVENTION_NONE;
    }
}

int health_agent_intervene_shadow(
    nimcp_health_agent_t* agent,
    health_shadow_pattern_t pattern
) {
    if (!agent) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;

    }
    if (pattern == HEALTH_SHADOW_NONE || pattern >= HEALTH_SHADOW_COUNT) return 0;

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_agent_interve", 0.0f);


    shadow_intervention_type_t intervention = health_shadow_get_intervention(pattern);

    /* Apply intervention via public health agent API.
     * The agent struct is opaque — interventions are applied
     * by sending adjustment messages through the agent's message queue.
     * For now, log the intervention and mark it applied.
     * Real message-based intervention will be wired through bio-async. */

    const char* intervention_names[] = {
        [SHADOW_INTERVENTION_NONE] = "none",
        [SHADOW_INTERVENTION_RAISE_THRESHOLD] = "raise_threshold",
        [SHADOW_INTERVENTION_LOWER_THRESHOLD] = "lower_threshold",
        [SHADOW_INTERVENTION_LIMIT_FREQUENCY] = "limit_frequency",
        [SHADOW_INTERVENTION_FORCE_DEFAULT] = "force_default",
        [SHADOW_INTERVENTION_REDUCE_ESCALATION] = "reduce_escalation",
        [SHADOW_INTERVENTION_REQUIRE_ACTION] = "require_action",
        [SHADOW_INTERVENTION_ACCEPT_GOOD_ENOUGH] = "accept_good_enough",
    };

    if (intervention > SHADOW_INTERVENTION_NONE &&
        intervention <= SHADOW_INTERVENTION_ACCEPT_GOOD_ENOUGH) {
        LOG_INFO("HEALTH_EMOTION", "Shadow intervention applied: %s for pattern %d",
                 intervention_names[intervention], (int)pattern);
    }

    return 0;
}

/*==============================================================================
 * UNIFIED STATE
 *============================================================================*/

int health_emotion_update_unified_state(
    const nimcp_health_agent_t* agent,
    const emotional_system_t* emotion_system,
    const emotion_immune_bridge_t* emotion_immune_bridge,
    immune_emotion_health_state_t* state
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_updat", 0.0f);


    memset(state, 0, sizeof(immune_emotion_health_state_t));

    /* Default values */
    state->inflammation_level = INFLAMMATION_NONE;
    state->immune_health_score = 1.0f;
    state->active_threats = 0;

    state->valence = 0.0f;
    state->arousal = 0.3f;
    state->emotional_stability = 0.8f;

    state->overall_health_score = 1.0f;
    state->active_anomalies = 0;
    state->agent_stress = 0.0f;
    state->agent_confidence = 1.0f;

    /* Query emotion system if available */
    if (emotion_system) {
        health_emotion_state_t emotion_state;
        if (health_emotion_get_state(emotion_system, &emotion_state) == 0) {
            state->valence = emotion_state.valence;
            state->arousal = emotion_state.arousal;
            state->emotional_stability = emotion_state.stability;
        }
    }

    /* Query immune bridge if available */
    (void)emotion_immune_bridge;  /* Will integrate in full implementation */

    /* Use health agent for instance heartbeat */
    health_emotion_bridge_heartbeat_instance((nimcp_health_agent_t*)agent, "health_emoti_health_emotion_updat", 0.5f);

    /* Compute derived metrics */
    state->combined_stress_index = health_emotion_compute_combined_stress(state);

    /* Resilience is inverse of stress, modulated by stability */
    state->system_resilience = (1.0f - state->combined_stress_index) * state->emotional_stability;

    /* Recovery capacity depends on immune health and available resources */
    state->recovery_capacity = state->immune_health_score *
                               (1.0f - (float)state->active_anomalies * 0.1f);
    if (state->recovery_capacity < 0.1f) state->recovery_capacity = 0.1f;

    return 0;
}

float health_emotion_compute_combined_stress(const immune_emotion_health_state_t* state) {
    if (!state) return 0.5f;

    /* Weights for each component */
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_compu", 0.0f);


    const float w_inflammation = 0.25f;
    const float w_arousal = 0.25f;
    const float w_anomaly = 0.25f;
    const float w_agent_stress = 0.15f;
    const float w_valence = 0.10f;

    /* Normalize inflammation to [0, 1] */
    float inflammation_norm = (float)state->inflammation_level / 4.0f;

    /* Arousal already in [0, 1] */
    float arousal = state->arousal;

    /* Normalize anomaly count (assume 10+ is max stress) */
    float anomaly_norm = (float)state->active_anomalies / 10.0f;
    if (anomaly_norm > 1.0f) anomaly_norm = 1.0f;

    /* Agent stress already in [0, 1] */
    float agent_stress = state->agent_stress;

    /* Negative valence contributes to stress */
    float valence_stress = (state->valence < 0.0f) ? -state->valence : 0.0f;

    /* Base stress computation */
    float stress = w_inflammation * inflammation_norm +
                   w_arousal * arousal +
                   w_anomaly * anomaly_norm +
                   w_agent_stress * agent_stress +
                   w_valence * valence_stress;

    /* Cross-system effects */

    /* Inflammation + high arousal has compounding effect */
    if (inflammation_norm > 0.5f && arousal > 0.6f) {
        stress += 0.1f;
    }

    /* Multiple active anomalies + agent stress compounds */
    if (state->active_anomalies > 3 && agent_stress > 0.5f) {
        stress += 0.1f;
    }

    /* Clamp to [0, 1] */
    if (stress > 1.0f) stress = 1.0f;
    if (stress < 0.0f) stress = 0.0f;

    return stress;
}

int health_emotion_get_holistic_recommendation(
    const immune_emotion_health_state_t* state,
    health_recovery_action_t base_recommendation,
    health_recovery_action_t* adjusted_recommendation
) {
    if (!adjusted_recommendation) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adjusted_recommendation is NULL");

        return -1;

    }

    *adjusted_recommendation = base_recommendation;

    if (!state) return 0;

    /* High combined stress = more conservative */
    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat("health_emoti_health_emotion_get_h", 0.0f);


    if (state->combined_stress_index > 0.7f) {
        if (base_recommendation > HEALTH_RECOVERY_PARTIAL_RESTART) {
            *adjusted_recommendation = HEALTH_RECOVERY_PARTIAL_RESTART;
        }
    }

    /* Low resilience = even more conservative */
    if (state->system_resilience < 0.3f) {
        if (base_recommendation > HEALTH_RECOVERY_REDUCE_LOAD) {
            *adjusted_recommendation = HEALTH_RECOVERY_REDUCE_LOAD;
        }
    }

    /* High inflammation + low recovery capacity = minimal action */
    if (state->inflammation_level >= INFLAMMATION_HIGH &&
        state->recovery_capacity < 0.3f) {
        if (base_recommendation > HEALTH_RECOVERY_LOG_ONLY) {
            *adjusted_recommendation = HEALTH_RECOVERY_LOG_ONLY;
        }
    }

    /* Low agent confidence = defer to conservative action */
    if (state->agent_confidence < 0.4f) {
        if (base_recommendation > HEALTH_RECOVERY_CLEAR_CACHE) {
            *adjusted_recommendation = HEALTH_RECOVERY_CLEAR_CACHE;
        }
    }

    return 0;
}

/*==============================================================================
 * STATISTICS
 *============================================================================*/

int health_emotion_get_stats(
    const nimcp_health_agent_t* agent,
    health_emotion_stats_t* stats
) {
    if (!stats) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat_instance((nimcp_health_agent_t*)agent, "health_emoti_health_emotion_get_s", 0.0f);


    memset(stats, 0, sizeof(health_emotion_stats_t));

    /* In full implementation, this would query accumulated statistics
     * from the health agent's emotion bridge state */

    return 0;
}

void health_emotion_reset_stats(nimcp_health_agent_t* agent) {
    /* In full implementation, this would reset the emotion bridge
     * statistics stored in the health agent */

    /* Phase 8: Heartbeat at operation start */
    health_emotion_bridge_heartbeat_instance(agent, "health_emoti_health_emotion_reset", 0.0f);
}

/*==============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* health_emotion_event_name(health_emotion_event_type_t event_type) {
    if (event_type >= HEALTH_EMOTION_EVENT_COUNT) {
        return "Unknown";
    }
    return event_type_names[event_type];
}

const char* health_emotion_recovery_action_name(health_recovery_action_t action) {
    if (action > HEALTH_RECOVERY_EMERGENCY_SHUTDOWN) {
        return "Unknown";
    }
    return recovery_action_names[action];
}

const char* health_shadow_pattern_name(health_shadow_pattern_t pattern) {
    if (pattern >= HEALTH_SHADOW_COUNT) {
        return "Unknown";
    }
    return shadow_pattern_names[pattern];
}

const char* health_inflammation_level_name(brain_inflammation_level_t level) {
    if (level > INFLAMMATION_SEVERE) {
        return "Unknown";
    }
    return inflammation_names[level];
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void health_emotion_bridge_set_instance_health_agent(void* agent) {
    /* Instance-level setter used by test harness - sets global agent as fallback */
    g_health_emotion_bridge_health_agent = (nimcp_health_agent_t*)agent;
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int health_emotion_bridge_training_begin(void* ctx) {
    (void)ctx;
    health_emotion_bridge_heartbeat_instance(NULL, "health_emoti_training_begin", 0.0f);
    return 0;
}

int health_emotion_bridge_training_end(void* ctx) {
    (void)ctx;
    health_emotion_bridge_heartbeat_instance(NULL, "health_emoti_training_end", 1.0f);
    return 0;
}

int health_emotion_bridge_training_step(void* ctx, float progress) {
    (void)ctx;
    health_emotion_bridge_heartbeat_instance(NULL, "health_emoti_training_step", progress);
    return 0;
}
