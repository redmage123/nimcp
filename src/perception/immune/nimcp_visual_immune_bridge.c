/**
 * @file nimcp_visual_immune_bridge.c
 * @brief Visual Cortex-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and visual cortex systems
 * WHY:  Biological realism - cytokines impair vision, visual threats trigger immunity
 * HOW:  Monitor cytokine levels to modulate visual processing, monitor visual threats to trigger immune responses
 */

#include "perception/immune/nimcp_visual_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_immune_bridge)

/* Local constant for chronic inflammation threshold (7 days in seconds) */
#ifndef CHRONIC_INFLAMMATION_THRESHOLD
#define CHRONIC_INFLAMMATION_THRESHOLD    (86400.0f * 7)
#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute sickness behavior level from immune system
 *
 * WHAT: Calculate overall sickness behavior intensity
 * WHY:  Sickness behavior is distinct syndrome affecting visual processing
 * HOW:  Query immune system inflammation level
 */
static float compute_sickness_behavior(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Get immune statistics to determine sickness level */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return 0.0f;
    }

    /* Sickness behavior requires active inflammation sites */
    float sickness = 0.0f;
    if (stats.inflammation_sites > 0) {
        sickness = (float)stats.inflammation_sites / 10.0f; /* Normalize */

        /* Only factor in reduced system health if inflammation is present */
        if (stats.system_health < 0.7f && stats.system_health > 0.0f) {
            sickness += (1.0f - stats.system_health) * 0.5f;
        }
    }

    return nimcp_clampf(sickness, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration from immune system
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different visual effects
 * HOW:  Query immune system for inflammation sites
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query immune statistics */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return 0.0f;
    }

    /* If inflammation sites exist, estimate duration */
    if (stats.inflammation_sites > 0) {
        /* Would need actual site access for precise duration */
        /* For now, return moderate duration if inflamed */
        return 3600.0f; /* 1 hour estimate */
    }

    return 0.0f;
}

/**
 * @brief Get maximum inflammation level from immune system
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines visual impact
 * HOW:  Query immune system statistics
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* Map inflammation site count to level */
    if (stats.inflammation_sites == 0) return INFLAMMATION_NONE;
    if (stats.inflammation_sites == 1) return INFLAMMATION_LOCAL;
    if (stats.inflammation_sites <= 3) return INFLAMMATION_REGIONAL;
    if (stats.inflammation_sites <= 6) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Map inflammation level to visual impairment factor
 *
 * WHAT: Convert inflammation severity to visual degradation
 * WHY:  Different inflammation levels have different visual impacts
 * HOW:  Linear scaling based on severity
 */
static float inflammation_to_visual_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return 0.0f;
        case INFLAMMATION_LOCAL:    return 0.1f;  /* 10% impairment */
        case INFLAMMATION_REGIONAL: return 0.3f;  /* 30% impairment */
        case INFLAMMATION_SYSTEMIC: return 0.6f;  /* 60% impairment */
        case INFLAMMATION_STORM:    return 0.8f;  /* 80% impairment */
        default:                    return 0.0f;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int visual_immune_default_config(visual_immune_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "visual_immune_default_config: NULL config");

    /* All features enabled by default */
    config->enable_cytokine_visual_modulation = true;
    config->enable_inflammation_visual_impairment = true;
    config->enable_visual_immune_trigger = true;
    config->enable_sickness_visual_reduction = true;
    config->enable_tunnel_vision = true;
    config->enable_threat_salience_boost = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->visual_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->visual_threat_threshold = VISUAL_THREAT_IMMUNE_THRESHOLD;
    config->inflammation_visual_threshold = INFLAMMATION_VISUAL_THRESHOLD;

    return 0;
}

visual_immune_bridge_t* visual_immune_bridge_create(
    const visual_immune_config_t* config,
    brain_immune_system_t* immune_system,
    visual_cortex_t* visual_cortex
) {
    /* Guard: require both systems */
    NIMCP_API_CHECK_NULL_RET_NULL(immune_system, "visual_immune_bridge_create: NULL immune_system");
    NIMCP_API_CHECK_NULL_RET_NULL(visual_cortex, "visual_immune_bridge_create: NULL visual_cortex");

    /* Allocate bridge */
    visual_immune_bridge_t* bridge = (visual_immune_bridge_t*)
        nimcp_malloc(sizeof(visual_immune_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "visual_immune_bridge_create: Failed to allocate bridge");

    /* Initialize to zero */
    memset(bridge, 0, sizeof(visual_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->visual_cortex = visual_cortex;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_visual_modulation = config->enable_cytokine_visual_modulation;
        bridge->enable_inflammation_visual_impairment = config->enable_inflammation_visual_impairment;
        bridge->enable_visual_immune_trigger = config->enable_visual_immune_trigger;
        bridge->enable_sickness_visual_reduction = config->enable_sickness_visual_reduction;
        bridge->enable_tunnel_vision = config->enable_tunnel_vision;
        bridge->enable_threat_salience_boost = config->enable_threat_salience_boost;
    } else {
        /* Use defaults */
        visual_immune_config_t default_cfg;
        visual_immune_default_config(&default_cfg);
        bridge->enable_cytokine_visual_modulation = default_cfg.enable_cytokine_visual_modulation;
        bridge->enable_inflammation_visual_impairment = default_cfg.enable_inflammation_visual_impairment;
        bridge->enable_visual_immune_trigger = default_cfg.enable_visual_immune_trigger;
        bridge->enable_sickness_visual_reduction = default_cfg.enable_sickness_visual_reduction;
        bridge->enable_tunnel_vision = default_cfg.enable_tunnel_vision;
        bridge->enable_threat_salience_boost = default_cfg.enable_threat_salience_boost;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "visual_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize cytokine effect factors to baseline (1.0 = no impairment) */
    bridge->cytokine_effects.total_processing_factor = 1.0f;
    bridge->cytokine_effects.total_accuracy_factor = 1.0f;
    bridge->cytokine_effects.total_attention_factor = 1.0f;

    LOG_MODULE_INFO("visual_immune_bridge", "Bridge created successfully");
    return bridge;
}

void visual_immune_bridge_destroy(visual_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("visual_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Visual Implementation
 * ============================================================================ */

int visual_immune_apply_cytokine_effects(visual_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_apply_cytokine_effects: NULL bridge");
    if (!bridge->enable_cytokine_visual_modulation) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_apply_cytokine_effects: NULL immune_system");
    NIMCP_API_CHECK_NULL(bridge->visual_cortex, -1, "visual_immune_apply_cytokine_effects: NULL visual_cortex");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects from immune system */
    /* Note: In full implementation, would query actual cytokine levels */
    /* For now, estimate from inflammation level */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float inflammation_factor = inflammation_to_visual_factor(level);

    /* Pro-inflammatory effects (estimated from inflammation) */
    bridge->cytokine_effects.il1_processing_reduction =
        inflammation_factor * fabsf(CYTOKINE_IL1_VISUAL_IMPAIRMENT);
    bridge->cytokine_effects.il6_accuracy_reduction =
        inflammation_factor * fabsf(CYTOKINE_IL6_VISUAL_IMPAIRMENT);
    bridge->cytokine_effects.tnf_attention_reduction =
        inflammation_factor * fabsf(CYTOKINE_TNF_VISUAL_IMPAIRMENT);
    bridge->cytokine_effects.ifn_gamma_contrast_reduction =
        inflammation_factor * fabsf(CYTOKINE_IFN_GAMMA_VISUAL_IMPAIRMENT);

    /* Anti-inflammatory recovery (inverse of inflammation) */
    bridge->cytokine_effects.il10_recovery_boost =
        (1.0f - inflammation_factor) * CYTOKINE_IL10_VISUAL_RECOVERY;

    /* Compute aggregate factors */
    bridge->cytokine_effects.total_processing_factor =
        1.0f - bridge->cytokine_effects.il1_processing_reduction;
    bridge->cytokine_effects.total_accuracy_factor =
        1.0f - bridge->cytokine_effects.il6_accuracy_reduction;
    bridge->cytokine_effects.total_attention_factor =
        1.0f - bridge->cytokine_effects.tnf_attention_reduction;

    /* Sickness behavior overall impairment */
    bridge->cytokine_effects.sickness_visual_impairment =
        compute_sickness_behavior(bridge->immune_system);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int visual_immune_apply_inflammation_effects(visual_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_apply_inflammation_effects: NULL bridge");
    if (!bridge->enable_inflammation_visual_impairment) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_apply_inflammation_effects: NULL immune_system");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);
    bool is_chronic = (duration >= CHRONIC_INFLAMMATION_THRESHOLD);

    /* Update inflammation state */
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic = is_chronic;

    /* Compute visual impacts */
    float base_impairment = inflammation_to_visual_factor(level);

    /* Chronic inflammation has worse effects */
    float chronic_multiplier = is_chronic ? 1.5f : 1.0f;
    float impairment = base_impairment * chronic_multiplier;
    impairment = nimcp_clampf(impairment, 0.0f, INFLAMMATION_MAX_VISUAL_IMPAIRMENT);

    bridge->inflammation_state.processing_speed_reduction = impairment * 0.6f;
    bridge->inflammation_state.visual_acuity_loss = impairment * 0.5f;
    bridge->inflammation_state.contrast_sensitivity_loss = impairment * 0.4f;

    /* Tunnel vision increases with inflammation */
    if (level >= INFLAMMATION_REGIONAL) {
        bridge->inflammation_state.tunnel_vision_severity =
            (impairment - 0.3f) / 0.5f; /* Normalize to [0-1] */
        bridge->inflammation_state.tunnel_vision_severity =
            nimcp_clampf(bridge->inflammation_state.tunnel_vision_severity, 0.0f, 1.0f);
    } else {
        bridge->inflammation_state.tunnel_vision_severity = 0.0f;
    }

    /* Photophobia during systemic/storm inflammation */
    bridge->inflammation_state.photophobia_level =
        (level >= INFLAMMATION_SYSTEMIC) ? impairment * 0.7f : 0.0f;

    /* Feature extraction degradation */
    bridge->inflammation_state.gabor_filter_gain_reduction = impairment * 0.5f;
    bridge->inflammation_state.feature_extraction_noise = impairment * 0.3f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int visual_immune_apply_sickness_effects(visual_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_apply_sickness_effects: NULL bridge");
    if (!bridge->enable_sickness_visual_reduction) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_apply_sickness_effects: NULL immune_system");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get sickness behavior level */
    float sickness = compute_sickness_behavior(bridge->immune_system);

    /* Get inflammation for fatigue */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float fatigue = inflammation_to_visual_factor(level) * 0.8f;

    /* Update sickness effects */
    bridge->sickness_effects.sickness_behavior_level = sickness;
    bridge->sickness_effects.fatigue_level = fatigue;

    /* Visual behavior changes */
    bridge->sickness_effects.exploration_reduction =
        sickness * SICKNESS_VISUAL_EXPLORATION_FACTOR;
    bridge->sickness_effects.processing_speed_reduction =
        sickness * SICKNESS_VISUAL_SPEED_FACTOR;

    /* Threat attention boost during sickness */
    bridge->sickness_effects.attention_to_threats_boost =
        sickness * 0.5f; /* 50% boost to threat detection */

    /* Novelty seeking reduced */
    bridge->sickness_effects.attention_to_novelty_reduction =
        sickness * 0.7f; /* 70% reduction in novelty seeking */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float visual_immune_compute_tunnel_vision(const visual_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_immune_compute_tunnel_vision: bridge is NULL");
        return 0.0f;
    }
    if (!bridge->enable_tunnel_vision) return 0.0f;

    /* Return stored tunnel vision severity */
    return bridge->inflammation_state.tunnel_vision_severity;
}

int visual_immune_modulate_neurotransmitters(visual_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_modulate_neurotransmitters: NULL bridge");
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_modulate_neurotransmitters: NULL immune_system");
    NIMCP_API_CHECK_NULL(bridge->visual_cortex, -1, "visual_immune_modulate_neurotransmitters: NULL visual_cortex");

    /* Get inflammation level */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);

    /* Inflammation reduces acetylcholine (attention impairment) */
    float ach_reduction = inflammation_to_visual_factor(level) * 0.4f;
    float new_ach_tonic = nimcp_clampf(0.5f - ach_reduction, 0.1f, 1.0f);

    /* Inflammation increases norepinephrine (stress/arousal) */
    float ne_increase = inflammation_to_visual_factor(level) * 0.3f;
    float new_ne_tonic = nimcp_clampf(0.3f + ne_increase, 0.0f, 0.8f);

    /* Set visual cortex neuromodulator levels */
    /* 0=dopamine, 1=acetylcholine, 2=norepinephrine */
    visual_cortex_set_tonic_level(bridge->visual_cortex, 1, new_ach_tonic);
    visual_cortex_set_tonic_level(bridge->visual_cortex, 2, new_ne_tonic);

    return 0;
}

/* ============================================================================
 * Visual → Immune Implementation
 * ============================================================================ */

int visual_immune_trigger_from_threat(
    visual_immune_bridge_t* bridge,
    const float* threat_features,
    uint32_t num_features,
    float salience
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_trigger_from_threat: NULL bridge");
    if (!bridge->enable_visual_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(threat_features, -1, "visual_immune_trigger_from_threat: NULL threat_features");
    NIMCP_API_CHECK(num_features > 0, -1, "visual_immune_trigger_from_threat: num_features is 0");
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_trigger_from_threat: NULL immune_system");

    /* Prepare antigen data outside lock scope */
    bool should_trigger = false;
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t epitope_len = 0;
    uint32_t severity = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update visual trigger state */
    bridge->visual_trigger.threat_salience = salience;

    /* Check if threat is severe enough to trigger immune */
    if (salience >= VISUAL_THREAT_IMMUNE_THRESHOLD) {
        bridge->visual_trigger.threat_triggered = true;
        bridge->visual_trigger.immune_activation_level = salience;

        /* Create antigen from visual threat */
        /* Convert features to epitope (use first features as signature) */
        epitope_len = (num_features < BRAIN_IMMUNE_EPITOPE_SIZE) ?
            num_features : BRAIN_IMMUNE_EPITOPE_SIZE;

        for (size_t i = 0; i < epitope_len; i++) {
            /* Quantize float feature to uint8 */
            epitope[i] = (uint8_t)(nimcp_clampf(threat_features[i], 0.0f, 1.0f) * 255.0f);
        }

        severity = (uint32_t)(salience * 10.0f); /* Map to 1-10 */
        should_trigger = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen to immune system OUTSIDE lock */
    if (should_trigger) {
        uint32_t antigen_id;
        int result = brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            epitope,
            epitope_len,
            severity,
            0, /* source_node */
            &antigen_id
        );

        if (result == 0) {
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->visual_triggered_responses++;
            bridge->threat_detections++;
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    return 0;
}

int visual_immune_trigger_from_anomaly(
    visual_immune_bridge_t* bridge,
    float anomaly_score
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_trigger_from_anomaly: NULL bridge");
    if (!bridge->enable_visual_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_trigger_from_anomaly: NULL immune_system");

    /* Prepare antigen data outside lock scope */
    bool should_trigger_anomaly = false;
    uint8_t anomaly_epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    uint32_t anomaly_severity = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update visual trigger state */
    bridge->visual_trigger.pattern_corruption_level = anomaly_score;

    /* Anomalies trigger at lower threshold (amplified) */
    float effective_severity = anomaly_score * VISUAL_ANOMALY_SEVERITY_MULTIPLIER;

    if (effective_severity >= VISUAL_THREAT_IMMUNE_THRESHOLD) {
        bridge->visual_trigger.anomaly_triggered = true;
        bridge->visual_trigger.immune_activation_level = effective_severity;

        /* Create anomaly epitope (simple pattern corruption signature) */
        memset(anomaly_epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
        anomaly_epitope[0] = 0xFF; /* Marker for visual anomaly */
        anomaly_epitope[1] = (uint8_t)(anomaly_score * 255.0f);

        anomaly_severity = (uint32_t)(effective_severity * 10.0f);
        should_trigger_anomaly = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present to immune system OUTSIDE lock */
    if (should_trigger_anomaly) {
        uint32_t antigen_id;
        int result = brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            anomaly_epitope,
            32, /* Use partial epitope */
            anomaly_severity,
            0,
            &antigen_id
        );

        if (result == 0) {
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->visual_triggered_responses++;
            bridge->anomaly_detections++;
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    return 0;
}

int visual_immune_trigger_from_visual_stress(visual_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_trigger_from_visual_stress: NULL bridge");
    if (!bridge->enable_visual_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "visual_immune_trigger_from_visual_stress: NULL immune_system");

    /* Prepare stress data outside lock scope */
    bool should_trigger_stress = false;
    uint8_t stress_epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    uint32_t stress_severity = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check visual stress duration */
    float stress_duration = bridge->visual_trigger.visual_stress_duration_sec;

    /* Chronic stress (>1 hour) triggers inflammation */
    if (stress_duration >= 3600.0f) {
        float chronic_level = nimcp_clampf(stress_duration / 86400.0f, 0.0f, 1.0f);
        bridge->visual_trigger.chronic_overstimulation = chronic_level;

        /* Create stress epitope */
        memset(stress_epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
        stress_epitope[0] = 0xFE; /* Marker for visual stress */
        stress_epitope[1] = (uint8_t)(chronic_level * 255.0f);

        stress_severity = 3 + (uint32_t)(chronic_level * 5.0f); /* 3-8 severity */
        should_trigger_stress = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present to immune system OUTSIDE lock */
    if (should_trigger_stress) {
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            stress_epitope,
            16,
            stress_severity,
            0,
            &antigen_id
        );

        nimcp_mutex_lock(bridge->base.mutex);
        bridge->visual_triggered_responses++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int visual_immune_bridge_update(
    visual_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_bridge_update: NULL bridge");

    /* Update visual stress duration */
    bridge->visual_trigger.visual_stress_duration_sec += (float)delta_ms / 1000.0f;

    /* Apply immune → visual effects */
    visual_immune_apply_cytokine_effects(bridge);
    visual_immune_apply_inflammation_effects(bridge);
    visual_immune_apply_sickness_effects(bridge);
    visual_immune_modulate_neurotransmitters(bridge);

    /* Check for visual → immune triggers */
    visual_immune_trigger_from_visual_stress(bridge);

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int visual_immune_get_cytokine_effects(
    const visual_immune_bridge_t* bridge,
    cytokine_visual_effects_t* effects
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_get_cytokine_effects: NULL bridge");
    NIMCP_API_CHECK_NULL(effects, -1, "visual_immune_get_cytokine_effects: NULL effects");

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int visual_immune_get_inflammation_state(
    const visual_immune_bridge_t* bridge,
    inflammation_visual_state_t* state
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_get_inflammation_state: NULL bridge");
    NIMCP_API_CHECK_NULL(state, -1, "visual_immune_get_inflammation_state: NULL state");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool visual_immune_is_sick_behavior(const visual_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "visual_immune_is_sick_behavior: bridge is NULL");

            return false;

        }
    return bridge->sickness_effects.sickness_behavior_level >= 0.3f;
}

float visual_immune_get_processing_speed_factor(const visual_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_immune_get_processing_speed_factor: bridge is NULL");
        return 1.0f;
    }

    /* Combine cytokine and sickness effects */
    float factor = bridge->cytokine_effects.total_processing_factor;
    factor *= (1.0f - bridge->sickness_effects.processing_speed_reduction);
    factor *= (1.0f - bridge->inflammation_state.processing_speed_reduction);

    return nimcp_clampf(factor, 0.1f, 1.0f); /* Minimum 10% speed */
}

float visual_immune_get_accuracy_factor(const visual_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_immune_get_accuracy_factor: bridge is NULL");
        return 1.0f;
    }

    /* Combine cytokine and inflammation effects */
    float factor = bridge->cytokine_effects.total_accuracy_factor;
    factor *= (1.0f - bridge->inflammation_state.visual_acuity_loss);

    return nimcp_clampf(factor, 0.2f, 1.0f); /* Minimum 20% accuracy */
}

float visual_immune_get_attention_capacity(const visual_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_immune_get_attention_capacity: bridge is NULL");
        return 1.0f;
    }

    /* Combine cytokine effects and tunnel vision */
    float capacity = bridge->cytokine_effects.total_attention_factor;
    capacity *= (1.0f - bridge->inflammation_state.tunnel_vision_severity * 0.5f);

    return nimcp_clampf(capacity, 0.2f, 1.0f); /* Minimum 20% capacity */
}

float visual_immune_get_threat_salience_boost(const visual_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_immune_get_threat_salience_boost: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->enable_threat_salience_boost) return 1.0f;

    /* Sickness behavior and inflammation enhance threat detection */
    float boost = 1.0f;
    boost += bridge->sickness_effects.attention_to_threats_boost;
    boost += bridge->inflammation_state.tunnel_vision_severity * 0.3f;

    return nimcp_clampf(boost, 1.0f, 2.0f); /* Up to 2x threat salience */
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define VISUAL_IMMUNE_MODULE_NAME "visual_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int visual_immune_connect_bio_async(visual_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_connect_bio_async: NULL bridge");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_VISUAL,
        .module_name = VISUAL_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("visual_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int visual_immune_disconnect_bio_async(visual_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_immune_disconnect_bio_async: NULL bridge");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("visual_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool visual_immune_is_bio_async_connected(const visual_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "visual_immune_is_bio_async_connected: bridge is NULL");

            return false;

        }
    return bridge->base.bio_async_enabled;
}
