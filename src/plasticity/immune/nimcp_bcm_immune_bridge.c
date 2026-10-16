/**
 * @file nimcp_bcm_immune_bridge.c
 * @brief BCM Learning-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and BCM learning systems
 * WHY:  Biological realism - cytokines modulate plasticity, plasticity failure triggers immunity
 * HOW:  Monitor cytokines to modulate BCM parameters, monitor BCM to trigger immune responses
 */

#include "plasticity/immune/nimcp_bcm_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bcm_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(bcm_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get cytokine concentration from immune system
 *
 * WHAT: Query current level of specific cytokine
 * WHY:  Need cytokine levels to modulate BCM
 * HOW:  Sum concentrations of all matching cytokines
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune || !immune->cytokines) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type) {
            total += immune->cytokines[i].concentration;
        }
    }
    return nimcp_clampf(total, 0.0f, 1.0f);
}

/**
 * @brief Get max inflammation level
 *
 * WHAT: Find highest inflammation level in system
 * WHY:  Max inflammation determines BCM disruption severity
 * HOW:  Scan all inflammation sites, return maximum
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || !immune->inflammation_sites) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }
    return max_level;
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has different BCM effects
 * HOW:  Find oldest active inflammation site
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || !immune->inflammation_sites || immune->inflammation_count == 0) {
        return 0.0f;
    }

    uint64_t current_time = immune->start_time; /* Would get actual current time */
    uint64_t oldest_start = current_time;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    return (float)(current_time - oldest_start) / 1000.0f; /* Convert ms to seconds */
}

/**
 * @brief Compute threshold variance
 *
 * WHAT: Calculate variance of BCM thresholds
 * WHY:  High variance indicates instability
 * HOW:  Standard variance formula
 */
static float compute_threshold_variance(
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    float mean_threshold
) {
    if (!synapses || num_synapses == 0) return 0.0f;

    float sum_squared_diff = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        float diff = synapses[i].threshold - mean_threshold;
        sum_squared_diff += diff * diff;
    }

    return sum_squared_diff / (float)num_synapses;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int bcm_immune_default_config(bcm_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_modulation = true;
    config->enable_inflammation_disruption = true;
    config->enable_bcm_immune_trigger = true;
    config->enable_baseline_tracking = true;
    config->enable_recovery_assistance = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->abnormality_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->threshold_instability_factor = BCM_THRESHOLD_INSTABILITY_FACTOR;
    config->learning_collapse_factor = BCM_LEARNING_COLLAPSE_FACTOR;
    config->metaplasticity_stuck_factor = BCM_METAPLASTICITY_STUCK_FACTOR;

    /* Baseline collection */
    config->baseline_samples_required = 100; /* 100 samples for stable baseline */

    return 0;
}

bcm_immune_bridge_t* bcm_immune_bridge_create(
    const bcm_immune_config_t* config,
    brain_immune_system_t* immune_system,
    bcm_params_t* bcm_params
) {
    /* Guard: require immune and BCM systems */
    if (!immune_system || !bcm_params) {
        LOG_MODULE_ERROR("bcm_immune_bridge",
                  "Cannot create bridge without immune and BCM systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_bridge_create: required parameter is NULL (immune_system, bcm_params)");
        return NULL;
    }

    /* Allocate bridge */
    bcm_immune_bridge_t* bridge = (bcm_immune_bridge_t*)
        nimcp_malloc(sizeof(bcm_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("bcm_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bcm_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(bcm_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->bcm_params = bcm_params;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_modulation = config->enable_cytokine_modulation;
        bridge->enable_inflammation_disruption = config->enable_inflammation_disruption;
        bridge->enable_bcm_immune_trigger = config->enable_bcm_immune_trigger;
        bridge->enable_baseline_tracking = config->enable_baseline_tracking;
        bridge->enable_recovery_assistance = config->enable_recovery_assistance;
    } else {
        /* Use defaults */
        bcm_immune_config_t default_cfg;
        bcm_immune_default_config(&default_cfg);
        bridge->enable_cytokine_modulation = default_cfg.enable_cytokine_modulation;
        bridge->enable_inflammation_disruption = default_cfg.enable_inflammation_disruption;
        bridge->enable_bcm_immune_trigger = default_cfg.enable_bcm_immune_trigger;
        bridge->enable_baseline_tracking = default_cfg.enable_baseline_tracking;
        bridge->enable_recovery_assistance = default_cfg.enable_recovery_assistance;
    }

    /* Initialize baseline metrics with reasonable defaults */
    bridge->baseline_metrics.baseline_threshold_mean = 0.5f;
    bridge->baseline_metrics.baseline_threshold_variance = 0.01f;
    bridge->baseline_metrics.baseline_ltp_rate = 0.5f;
    bridge->baseline_metrics.baseline_ltd_rate = 0.5f;
    bridge->baseline_metrics.baseline_sliding_rate = 0.01f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "bcm_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("bcm_immune_bridge", "Bridge created successfully");
    return bridge;
}

void bcm_immune_bridge_destroy(bcm_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("bcm_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → BCM Implementation
 * ============================================================================ */

int bcm_immune_apply_cytokine_effects(bcm_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_modulation) return 0;
    if (!bridge->immune_system || !bridge->bcm_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->bcm_params)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Query cytokine concentrations */
    float il1_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float il10_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    cytokine_bcm_effects_t* effects = &bridge->cytokine_effects;

    /* Compute individual cytokine effects */
    effects->il1_threshold_elevation = il1_level * (CYTOKINE_IL1_THRESHOLD_ELEVATION - 1.0f);
    effects->il6_learning_reduction = il6_level * (1.0f - CYTOKINE_IL6_LEARNING_REDUCTION);
    effects->tnf_sliding_acceleration = tnf_level * (1.0f - CYTOKINE_TNF_SLIDING_ACCELERATION);
    effects->il10_recovery_factor = il10_level * (CYTOKINE_IL10_RECOVERY_BOOST - 1.0f);

    /* Compute aggregate multipliers */
    /* IL-1β elevates threshold */
    effects->theta_m_multiplier = 1.0f + effects->il1_threshold_elevation;

    /* IL-6 reduces learning rate */
    effects->learning_rate_multiplier = 1.0f - effects->il6_learning_reduction;

    /* TNF-α accelerates sliding (reduces tau) */
    effects->tau_multiplier = 1.0f - effects->tnf_sliding_acceleration;

    /* IL-10 recovery moves toward baseline (1.0) */
    if (il10_level > 0.0f) {
        float recovery_strength = effects->il10_recovery_factor;
        effects->theta_m_multiplier = effects->theta_m_multiplier * (1.0f - recovery_strength) +
                                      1.0f * recovery_strength;
        effects->learning_rate_multiplier = effects->learning_rate_multiplier * (1.0f - recovery_strength) +
                                           1.0f * recovery_strength;
        effects->tau_multiplier = effects->tau_multiplier * (1.0f - recovery_strength) +
                                 1.0f * recovery_strength;
    }

    /* Clamp multipliers to safe ranges */
    effects->theta_m_multiplier = nimcp_clampf(effects->theta_m_multiplier,
                                         INFLAMMATION_THETA_MIN_FACTOR,
                                         INFLAMMATION_THETA_MAX_FACTOR);
    effects->learning_rate_multiplier = nimcp_clampf(effects->learning_rate_multiplier,
                                               INFLAMMATION_LR_MIN_FACTOR, 1.0f);
    effects->tau_multiplier = nimcp_clampf(effects->tau_multiplier, 0.5f, 2.0f);

    /* Apply modulations to BCM parameters */
    /* Note: We store original values and apply multipliers during BCM updates */
    /* This is done in the BCM module, we just track the multipliers here */

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int bcm_immune_apply_inflammation_effects(bcm_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_disruption) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_bcm_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= (86400.0f * 7.0f)); /* 7 days */

    /* Compute BCM disruptions based on inflammation level */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Threshold instability increases with inflammation */
    state->threshold_instability = nimcp_clampf(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    /* Metaplasticity impairment */
    state->metaplasticity_impairment = nimcp_clampf(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    /* Learning suppression */
    state->learning_suppression = nimcp_clampf(inflammation_intensity * 0.9f, 0.0f, 1.0f);

    /* Chronic inflammation amplifies disruptions */
    if (state->is_chronic) {
        float duration_factor = nimcp_clampf(
            state->inflammation_duration_sec / (86400.0f * 14.0f), /* 2 weeks */
            0.0f, 1.0f
        );
        state->threshold_instability += duration_factor * 0.2f;
        state->metaplasticity_impairment += duration_factor * 0.3f;
        state->learning_suppression += duration_factor * 0.1f;

        /* Clamp after amplification */
        state->threshold_instability = nimcp_clampf(state->threshold_instability, 0.0f, 1.0f);
        state->metaplasticity_impairment = nimcp_clampf(state->metaplasticity_impairment, 0.0f, 1.0f);
        state->learning_suppression = nimcp_clampf(state->learning_suppression, 0.0f, 1.0f);
    }

    /* Homeostatic error (distance from healthy state) */
    state->homeostatic_error = nimcp_clampf(
        (state->threshold_instability + state->metaplasticity_impairment + state->learning_suppression) / 3.0f,
        0.0f, 1.0f
    );

    /* Estimate recovery time (hours) */
    state->recovery_time_estimate_ms = state->homeostatic_error * 86400000.0f; /* Up to 24 hours */

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float bcm_immune_compute_theta_modulation(const bcm_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Combine cytokine and inflammation effects */
    float cytokine_modulation = bridge->cytokine_effects.theta_m_multiplier;
    float inflammation_factor = bridge->inflammation_state.threshold_instability;

    /* Inflammation adds additional elevation */
    float total_modulation = cytokine_modulation * (1.0f + inflammation_factor * 0.5f);

    return nimcp_clampf(total_modulation, INFLAMMATION_THETA_MIN_FACTOR, INFLAMMATION_THETA_MAX_FACTOR);
}

int bcm_immune_assist_recovery(bcm_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_recovery_assistance) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_assist_recovery: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Query IL-10 level */
    float il10_level = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);

    /* If IL-10 present, accelerate return to baseline */
    if (il10_level > 0.1f) {
        /* Recovery assistance tracked in cytokine effects */
        bridge->recovery_assists++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * BCM → Immune Implementation
 * ============================================================================ */

int bcm_immune_update_baseline(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_baseline_tracking) return 0;
    if (!synapses || num_synapses == 0 || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_update_baseline: required parameter is NULL (synapses, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bcm_baseline_metrics_t* baseline = &bridge->baseline_metrics;

    /* Only update if baseline not yet established */
    if (!baseline->baseline_established) {
        /* Exponential moving average */
        float alpha = 0.1f; /* Smoothing factor */

        baseline->baseline_threshold_mean =
            baseline->baseline_threshold_mean * (1.0f - alpha) +
            stats->avg_threshold * alpha;

        float threshold_variance = compute_threshold_variance(
            synapses, num_synapses, stats->avg_threshold);
        baseline->baseline_threshold_variance =
            baseline->baseline_threshold_variance * (1.0f - alpha) +
            threshold_variance * alpha;

        /* LTP/LTD rates (events per update) */
        float total_updates = (float)stats->total_updates;
        if (total_updates > 0.0f) {
            float ltp_rate = (float)stats->ltp_events / total_updates;
            float ltd_rate = (float)stats->ltd_events / total_updates;

            baseline->baseline_ltp_rate =
                baseline->baseline_ltp_rate * (1.0f - alpha) + ltp_rate * alpha;
            baseline->baseline_ltd_rate =
                baseline->baseline_ltd_rate * (1.0f - alpha) + ltd_rate * alpha;
        }

        /* Increment sample count */
        baseline->samples_collected++;

        /* Check if baseline established (need 100 samples by default) */
        if (baseline->samples_collected >= 100) {
            baseline->baseline_established = true;
            LOG_MODULE_INFO("bcm_immune_bridge", "Baseline established");
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int bcm_immune_detect_abnormalities(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_bcm_immune_trigger) return 0;
    if (!synapses || num_synapses == 0 || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_detect_abnormalities: required parameter is NULL (synapses, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bcm_abnormality_state_t* abnormality = &bridge->abnormality_state;
    const bcm_baseline_metrics_t* baseline = &bridge->baseline_metrics;

    /* Update current metrics */
    abnormality->threshold_mean = stats->avg_threshold;
    abnormality->threshold_variance = compute_threshold_variance(
        synapses, num_synapses, stats->avg_threshold);
    abnormality->ltp_events = stats->ltp_events;
    abnormality->ltd_events = stats->ltd_events;

    /* Only detect abnormalities if baseline established */
    if (!baseline->baseline_established) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        return 0;
    }

    /* 1. Threshold instability detection */
    float variance_ratio = abnormality->threshold_variance / baseline->baseline_threshold_variance;
    abnormality->threshold_instability_score = nimcp_clampf(variance_ratio / BCM_THRESHOLD_INSTABILITY_FACTOR, 0.0f, 1.0f);
    abnormality->threshold_unstable = (variance_ratio > BCM_THRESHOLD_INSTABILITY_FACTOR);

    if (abnormality->threshold_unstable) {
        bridge->threshold_instabilities_detected++;
    }

    /* 2. Learning collapse detection */
    float total_events = (float)(abnormality->ltp_events + abnormality->ltd_events);
    float total_updates = (float)stats->total_updates;
    float current_activity = (total_updates > 0.0f) ? (total_events / total_updates) : 0.0f;
    float baseline_activity = baseline->baseline_ltp_rate + baseline->baseline_ltd_rate;
    float activity_ratio = (baseline_activity > 0.0f) ? (current_activity / baseline_activity) : 1.0f;

    abnormality->learning_activity_score = nimcp_clampf(activity_ratio, 0.0f, 1.0f);
    abnormality->learning_collapsed = (activity_ratio < BCM_LEARNING_COLLAPSE_FACTOR);

    if (abnormality->learning_collapsed) {
        bridge->learning_collapses_detected++;
    }

    /* 3. Metaplasticity stuck detection */
    /* Estimate sliding rate from threshold change */
    abnormality->sliding_rate = fabsf(abnormality->threshold_mean - baseline->baseline_threshold_mean);
    float sliding_ratio = (baseline->baseline_sliding_rate > 0.0f) ?
                          (abnormality->sliding_rate / baseline->baseline_sliding_rate) : 1.0f;

    abnormality->metaplasticity_health = nimcp_clampf(sliding_ratio, 0.0f, 1.0f);
    abnormality->metaplasticity_stuck = (sliding_ratio < BCM_METAPLASTICITY_STUCK_FACTOR);

    if (abnormality->metaplasticity_stuck) {
        bridge->metaplasticity_failures_detected++;
    }

    /* Compute combined severity (1-10) */
    uint32_t severity = 0;
    if (abnormality->threshold_unstable) severity += 4;
    if (abnormality->learning_collapsed) severity += 3;
    if (abnormality->metaplasticity_stuck) severity += 3;

    /* Cap at 10 */
    abnormality->immune_trigger_severity = (severity > 10) ? 10 : severity;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int bcm_immune_trigger_from_abnormality(bcm_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_bcm_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_trigger_from_abnormality: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    const bcm_abnormality_state_t* abnormality = &bridge->abnormality_state;

    /* Only trigger if severity >= 3 */
    if (abnormality->immune_trigger_severity < 3) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        return 0;
    }

    /* Create epitope from abnormality pattern */
    uint8_t epitope[64];
    memset(epitope, 0, sizeof(epitope));

    /* Encode abnormality type in epitope */
    epitope[0] = 0xBC; /* "BCM" marker */
    epitope[1] = 0xAB; /* "Abnormality" marker */
    epitope[2] = abnormality->threshold_unstable ? 1 : 0;
    epitope[3] = abnormality->learning_collapsed ? 1 : 0;
    epitope[4] = abnormality->metaplasticity_stuck ? 1 : 0;
    epitope[5] = (uint8_t)(abnormality->threshold_instability_score * 255.0f);
    epitope[6] = (uint8_t)(abnormality->learning_activity_score * 255.0f);
    epitope[7] = (uint8_t)(abnormality->metaplasticity_health * 255.0f);

    /* Present antigen to immune system */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        abnormality->immune_trigger_severity,
        0, /* source_node */
        &antigen_id
    );

    if (result == 0) {
        bridge->immune_triggered_responses++;
        LOG_MODULE_WARN("bcm_immune_bridge",
                  "BCM abnormality triggered immune response (severity %u)",
                  abnormality->immune_trigger_severity);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int bcm_immune_bridge_update(
    bcm_immune_bridge_t* bridge,
    const bcm_synapse_t* synapses,
    uint32_t num_synapses,
    const bcm_stats_t* stats,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Update baseline (if tracking) */
    if (bridge->enable_baseline_tracking && synapses && stats) {
        bcm_immune_update_baseline(bridge, synapses, num_synapses, stats);
    }

    /* Immune → BCM: Apply cytokine effects */
    if (bridge->enable_cytokine_modulation) {
        bcm_immune_apply_cytokine_effects(bridge);
    }

    /* Immune → BCM: Apply inflammation disruption */
    if (bridge->enable_inflammation_disruption) {
        bcm_immune_apply_inflammation_effects(bridge);
    }

    /* Immune → BCM: Assist recovery */
    if (bridge->enable_recovery_assistance) {
        bcm_immune_assist_recovery(bridge);
    }

    /* BCM → Immune: Detect abnormalities */
    if (bridge->enable_bcm_immune_trigger && synapses && stats) {
        bcm_immune_detect_abnormalities(bridge, synapses, num_synapses, stats);

        /* Trigger immune if abnormalities detected */
        if (bridge->abnormality_state.immune_trigger_severity >= 3) {
            bcm_immune_trigger_from_abnormality(bridge);
        }
    }

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int bcm_immune_get_cytokine_effects(
    const bcm_immune_bridge_t* bridge,
    cytokine_bcm_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int bcm_immune_get_inflammation_state(
    const bcm_immune_bridge_t* bridge,
    inflammation_bcm_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int bcm_immune_get_abnormality_state(
    const bcm_immune_bridge_t* bridge,
    bcm_abnormality_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_immune_get_abnormality_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *state = bridge->abnormality_state;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

bool bcm_immune_is_healthy(const bcm_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bool healthy = !bridge->abnormality_state.threshold_unstable &&
                   !bridge->abnormality_state.learning_collapsed &&
                   !bridge->abnormality_state.metaplasticity_stuck;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return healthy;
}

float bcm_immune_get_threshold_instability(const bcm_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->abnormality_state.threshold_instability_score;
}

float bcm_immune_get_learning_activity(const bcm_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->abnormality_state.learning_activity_score;
}

float bcm_immune_get_metaplasticity_health(const bcm_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->abnormality_state.metaplasticity_health;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define BCM_IMMUNE_MODULE_NAME "bcm_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int bcm_immune_connect_bio_async(bcm_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_BCM,
        .module_name = BCM_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("bcm_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int bcm_immune_disconnect_bio_async(bcm_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("bcm_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool bcm_immune_is_bio_async_connected(const bcm_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
