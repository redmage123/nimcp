/**
 * @file nimcp_sequence_immune_bridge.c
 * @brief Sequence Detector-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and sequence detection systems
 * WHY:  Biological realism - cytokines impair sequence learning, anomalies signal dysfunction
 * HOW:  Monitor cytokine levels to modulate detector, monitor sequences to trigger immune
 */

#include "middleware/immune/nimcp_sequence_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sequence_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation level from immune system
 *
 * WHAT: Query max inflammation level
 * WHY:  Inflammation determines sequence impairment
 * HOW:  Query immune system inflammation sites
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    /* Would query immune system inflammation_sites array */
    /* For now, return NONE - actual implementation would iterate sites */
    return INFLAMMATION_NONE;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has different effects than acute
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Would query immune system for inflammation sites */
    /* For now, return 0 - actual implementation would check inflammation_sites array */
    return 0.0f;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int sequence_immune_default_config(sequence_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_anomaly_immune_trigger = true;
    config->enable_positive_feedback = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->anomaly_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->anomaly_match_threshold = SEQUENCE_ANOMALY_THRESHOLD;
    config->timing_violation_threshold = SEQUENCE_TIMING_VIOLATION_THRESHOLD;
    config->learning_failure_threshold = SEQUENCE_LEARNING_FAILURE_COUNT;

    return 0;
}

sequence_immune_bridge_t* sequence_immune_bridge_create(
    const sequence_immune_config_t* config,
    brain_immune_system_t* immune_system,
    sequence_detector_t* sequence_detector
) {
    /* Guard: require both systems */
    if (!immune_system || !sequence_detector) {
        LOG_MODULE_ERROR("sequence_immune_bridge",
                  "Cannot create bridge without immune and sequence detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sequence_immune_bridge_create: required parameter is NULL (immune_system, sequence_detector)");
        return NULL;
    }

    /* Allocate bridge */
    sequence_immune_bridge_t* bridge = (sequence_immune_bridge_t*)
        nimcp_malloc(sizeof(sequence_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("sequence_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(sequence_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->sequence_detector = sequence_detector;

    /* Capture baseline parameters from sequence detector */
    /* Note: Would query detector for actual baseline values */
    bridge->baseline_temporal_tolerance_ms = SEQUENCE_TEMPORAL_TOLERANCE_MS;
    bridge->baseline_min_strength_threshold = SEQUENCE_MIN_STRENGTH;
    bridge->baseline_max_templates = SEQUENCE_MAX_TEMPLATES;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_modulation = config->enable_cytokine_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_anomaly_immune_trigger = config->enable_anomaly_immune_trigger;
        bridge->enable_positive_feedback = config->enable_positive_feedback;
    } else {
        /* Use defaults */
        sequence_immune_config_t default_cfg;
        sequence_immune_default_config(&default_cfg);
        bridge->enable_cytokine_modulation = default_cfg.enable_cytokine_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_anomaly_immune_trigger = default_cfg.enable_anomaly_immune_trigger;
        bridge->enable_positive_feedback = default_cfg.enable_positive_feedback;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "sequence_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("sequence_immune_bridge", "Bridge created successfully");
    return bridge;
}

void sequence_immune_bridge_destroy(sequence_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("sequence_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Sequence Implementation
 * ============================================================================ */

int sequence_immune_apply_cytokine_effects(sequence_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_modulation) return 0;
    if (!bridge->immune_system || !bridge->sequence_detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->sequence_detector)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_sequence_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → timing impairment */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_timing_penalty = 0.0f;  /* IL-1β level * IL1_TIMING_IMPACT */
    effects->il6_timing_penalty = 0.0f;  /* IL-6 level * IL6_TIMING_IMPACT */
    effects->tnf_timing_penalty = 0.0f;  /* TNF-α level * TNF_TIMING_IMPACT */
    effects->ifn_gamma_timing_penalty = 0.0f;

    /* Anti-inflammatory cytokines → precision restoration */
    effects->il10_precision_boost = 0.0f;  /* IL-10 level * precision factor */

    /* Aggregate effects */
    effects->total_timing_tolerance_ms =
        effects->il1_timing_penalty +
        effects->il6_timing_penalty +
        effects->tnf_timing_penalty +
        effects->ifn_gamma_timing_penalty;

    /* Reduce detection accuracy based on inflammation */
    float inflammation_factor = sequence_immune_compute_accuracy_factor(bridge);
    effects->detection_accuracy_factor = inflammation_factor;

    /* Learning impairment from pro-inflammatory cytokines */
    float proinflam_total = effects->il1_timing_penalty +
                           effects->il6_timing_penalty +
                           effects->tnf_timing_penalty;
    effects->learning_impairment = nimcp_clampf(proinflam_total / 100.0f, 0.0f, 0.8f);

    /* Apply IL-10 precision boost */
    if (effects->il10_precision_boost > 0.1f) {
        effects->total_timing_tolerance_ms *= (1.0f - effects->il10_precision_boost * 0.3f);
    }

    /* Apply to sequence detector */
    /* Note: Would call sequence_detector_set_config() with modulated parameters */
    /* New tolerance = baseline + cytokine penalty */
    /* New threshold = baseline * (1 + learning_impairment) */

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int sequence_immune_apply_inflammation_effects(sequence_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_sequence_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= 604800.0f);  /* 7 days */

    /* Chronic inflammation → severe impairment */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    if (state->is_chronic) {
        /* Chronic amplifies impairment */
        float duration_factor = nimcp_clampf(
            state->inflammation_duration_sec / (604800.0f * 2.0f),  /* 2 weeks */
            0.0f, 1.0f
        );
        state->accuracy_reduction = nimcp_clampf(inflammation_intensity * 0.7f * (1.0f + duration_factor), 0.0f, 0.8f);
    } else {
        /* Acute inflammation */
        state->accuracy_reduction = nimcp_clampf(inflammation_intensity * 0.5f, 0.0f, 0.6f);
    }

    /* Timing precision loss */
    state->timing_precision_loss = nimcp_clampf(inflammation_intensity * 0.6f, 0.0f, 0.7f);

    /* Pattern threshold increase (harder to detect patterns) */
    state->pattern_threshold_increase = nimcp_clampf(inflammation_intensity * 0.3f, 0.0f, 0.5f);

    /* Learning rate reduction */
    state->learning_rate_reduction = nimcp_clampf(inflammation_intensity * 0.8f, 0.0f, 0.9f);

    /* Basal ganglia function (procedural memory) */
    state->procedural_memory_impairment = nimcp_clampf(inflammation_intensity * 0.75f, 0.0f, 0.85f);
    state->sequence_consolidation_rate = 1.0f - state->learning_rate_reduction;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float sequence_immune_compute_accuracy_factor(const sequence_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Map inflammation level to accuracy multiplier */
    switch (bridge->inflammation_state.current_level) {
        case INFLAMMATION_NONE:
            return 1.0f;
        case INFLAMMATION_LOCAL:
            return INFLAMMATION_ACCURACY_MULTIPLIER_LOCAL;
        case INFLAMMATION_REGIONAL:
            return INFLAMMATION_ACCURACY_MULTIPLIER_REGIONAL;
        case INFLAMMATION_SYSTEMIC:
            return INFLAMMATION_ACCURACY_MULTIPLIER_SYSTEMIC;
        case INFLAMMATION_STORM:
            return INFLAMMATION_ACCURACY_MULTIPLIER_STORM;
        default:
            return 1.0f;
    }
}

float sequence_immune_compute_timing_tolerance(const sequence_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Return baseline + cytokine-induced tolerance */
    return bridge->baseline_temporal_tolerance_ms +
           bridge->cytokine_effects.total_timing_tolerance_ms;
}

/* ============================================================================
 * Sequence → Immune Implementation
 * ============================================================================ */

int sequence_immune_trigger_from_anomaly(
    sequence_immune_bridge_t* bridge,
    const sequence_detection_t* detection
) {
    /* Guard clauses */
    if (!bridge || !detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_trigger_from_anomaly: required parameter is NULL (bridge, detection)");
        return -1;
    }
    if (!bridge->enable_anomaly_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_trigger_from_anomaly: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    sequence_immune_trigger_t* trigger = &bridge->immune_trigger;

    /* Check for anomalies */
    bool is_anomaly = false;

    /* Low match strength */
    if (detection->strength < SEQUENCE_ANOMALY_THRESHOLD) {
        trigger->anomalous_match_strength = detection->strength;
        is_anomaly = true;
    }

    /* Timing violation */
    float expected_duration = 1000.0f;  /* Would get from template */
    float actual_duration = (float)(detection->end_time_ms - detection->start_time_ms);
    float timing_ratio = actual_duration / expected_duration;
    if (timing_ratio > SEQUENCE_TIMING_VIOLATION_THRESHOLD ||
        timing_ratio < (1.0f / SEQUENCE_TIMING_VIOLATION_THRESHOLD)) {
        trigger->timing_violation_factor = timing_ratio;
        is_anomaly = true;
    }

    /* Corrupted sequence (very low match with existing template) */
    if (detection->matched_elements < detection->total_elements / 2) {
        trigger->corrupted_sequence_count++;
        is_anomaly = true;
    }

    /* Calculate anomaly severity */
    if (is_anomaly) {
        trigger->anomaly_severity = nimcp_clampf(
            (1.0f - detection->strength) * 0.5f +
            (fabs(timing_ratio - 1.0f) / 2.0f) * 0.3f +
            (float)trigger->corrupted_sequence_count * 0.1f,
            0.0f, 1.0f
        );

        /* Trigger immune alert if severe */
        if (trigger->anomaly_severity > 0.6f) {
            trigger->alert_triggered = true;
            trigger->immune_activation_level = trigger->anomaly_severity;

            /* Present antigen to immune system */
            /* Note: Would call brain_immune_present_antigen() with sequence signature */

            bridge->anomaly_alerts_sent++;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int sequence_immune_report_learning_failure(sequence_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_anomaly_immune_trigger) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    sequence_immune_trigger_t* trigger = &bridge->immune_trigger;
    trigger->learning_failure_count++;
    bridge->learning_failures++;

    /* Threshold exceeded - trigger immune alert */
    if (trigger->learning_failure_count >= SEQUENCE_LEARNING_FAILURE_COUNT) {
        trigger->alert_triggered = true;
        trigger->immune_activation_level = 0.7f;  /* High severity */

        /* Present learning failure as antigen */
        /* Note: Would call brain_immune_present_antigen() */

        bridge->anomaly_alerts_sent++;

        /* Reset counter */
        trigger->learning_failure_count = 0;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int sequence_immune_report_detection_failure(
    sequence_immune_bridge_t* bridge,
    float match_strength
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_anomaly_immune_trigger) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    sequence_immune_trigger_t* trigger = &bridge->immune_trigger;
    bridge->detection_failures++;

    /* Track consecutive failures */
    if (match_strength < bridge->baseline_min_strength_threshold) {
        trigger->consecutive_failures++;

        /* Many consecutive failures may indicate inflammatory impairment */
        if (trigger->consecutive_failures > 10) {
            trigger->anomaly_severity = 0.5f;
            /* Could trigger immune surveillance */
        }
    } else {
        /* Reset on success */
        trigger->consecutive_failures = 0;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int sequence_immune_boost_from_learning_success(sequence_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_positive_feedback) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_boost_from_learning_success: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    sequence_immune_feedback_t* feedback = &bridge->positive_feedback;

    /* Track successful learning */
    feedback->successful_detections++;

    /* Calculate success rate */
    uint32_t total_attempts = feedback->successful_detections + bridge->learning_failures;
    if (total_attempts > 0) {
        feedback->template_learning_success_rate =
            (float)feedback->successful_detections / (float)total_attempts;
    }

    /* High success rate → positive immune feedback */
    if (feedback->template_learning_success_rate > 0.8f) {
        feedback->positive_feedback_active = true;

        /* Boost IL-10 (anti-inflammatory) */
        feedback->il10_release_boost = 0.3f;

        /* Reduce inflammation */
        feedback->inflammation_reduction = 0.2f;

        /* Note: Would call brain_immune_release_cytokine(BRAIN_CYTOKINE_IL10) */

        bridge->positive_feedback_events++;
    } else {
        feedback->positive_feedback_active = false;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int sequence_immune_bridge_update(
    sequence_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply all bidirectional effects */

    /* Immune → Sequence */
    sequence_immune_apply_cytokine_effects(bridge);
    sequence_immune_apply_inflammation_effects(bridge);

    /* Sequence → Immune */
    /* Note: Anomaly detection called when sequences are detected */
    /* This is handled externally by calling sequence_immune_trigger_from_anomaly() */

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sequence_immune_get_cytokine_effects(
    const sequence_immune_bridge_t* bridge,
    cytokine_sequence_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_sequence_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int sequence_immune_get_inflammation_state(
    const sequence_immune_bridge_t* bridge,
    inflammation_sequence_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_sequence_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool sequence_immune_is_detection_impaired(const sequence_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Detection is impaired if accuracy < 0.8 or timing tolerance increased >100ms */
    float accuracy = sequence_immune_compute_accuracy_factor(bridge);
    float tolerance = sequence_immune_compute_timing_tolerance(bridge);
    float tolerance_increase = tolerance - bridge->baseline_temporal_tolerance_ms;

    return (accuracy < 0.8f) || (tolerance_increase > 100.0f);
}

float sequence_immune_get_accuracy_factor(const sequence_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return sequence_immune_compute_accuracy_factor(bridge);
}

float sequence_immune_get_timing_tolerance(const sequence_immune_bridge_t* bridge) {
    if (!bridge) return SEQUENCE_TEMPORAL_TOLERANCE_MS;
    return sequence_immune_compute_timing_tolerance(bridge);
}

int sequence_immune_get_stats(
    const sequence_immune_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* anomaly_alerts,
    uint32_t* learning_failures
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    if (total_updates) *total_updates = bridge->total_updates;
    if (anomaly_alerts) *anomaly_alerts = bridge->anomaly_alerts_sent;
    if (learning_failures) *learning_failures = bridge->learning_failures;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define SEQUENCE_IMMUNE_MODULE_NAME "sequence_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int sequence_immune_connect_bio_async(sequence_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SEQUENCE,
        .module_name = SEQUENCE_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("sequence_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int sequence_immune_disconnect_bio_async(sequence_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("sequence_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool sequence_immune_is_bio_async_connected(const sequence_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
