/**
 * @file nimcp_audio_immune_bridge.c
 * @brief Audio Cortex-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and auditory processing systems
 * WHY:  Biological realism - cytokines impair auditory processing, auditory threats affect immunity
 * HOW:  Monitor cytokine levels to modulate processing, monitor audio to trigger immune responses
 */

#include "perception/immune/nimcp_audio_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(audio_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different auditory effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query immune system for inflammation sites */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return 0.0f;
    }

    /* If no inflammation, return 0 */
    if (stats.inflammation_sites == 0) {
        return 0.0f;
    }

    /* For simplicity, estimate duration from system health */
    /* Lower health suggests longer-duration inflammation */
    float duration = (1.0f - stats.system_health) * 604800.0f; /* Up to 7 days */
    return duration;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines auditory impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* Map inflammation sites to severity level */
    if (stats.inflammation_sites == 0) return INFLAMMATION_NONE;
    if (stats.inflammation_sites == 1) return INFLAMMATION_LOCAL;
    if (stats.inflammation_sites <= 3) return INFLAMMATION_REGIONAL;
    if (stats.inflammation_sites <= 6) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Compute cytokine concentration estimate
 *
 * WHAT: Estimate cytokine levels from immune stats
 * WHY:  Need cytokine levels to modulate auditory processing
 * HOW:  Use immune stats as proxy for cytokine activity
 */
static void estimate_cytokine_levels(
    const brain_immune_system_t* immune,
    float* il1,
    float* il6,
    float* tnf,
    float* ifn_gamma,
    float* il10
) {
    if (!immune || !il1 || !il6 || !tnf || !ifn_gamma || !il10) return;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        *il1 = *il6 = *tnf = *ifn_gamma = *il10 = 0.0f;
        return;
    }

    /* Pro-inflammatory: scale with active immune cells and inflammation */
    float inflammation_factor = (float)stats.inflammation_sites / 10.0f;
    float activity_factor = (float)stats.active_t_cells / 100.0f;

    *il1 = nimcp_clampf(inflammation_factor * 0.5f, 0.0f, 1.0f);
    *il6 = nimcp_clampf(inflammation_factor * 0.6f, 0.0f, 1.0f);
    *tnf = nimcp_clampf(inflammation_factor * 0.4f, 0.0f, 1.0f);
    *ifn_gamma = nimcp_clampf(activity_factor * 0.3f, 0.0f, 1.0f);

    /* Anti-inflammatory: inversely related to inflammation */
    *il10 = nimcp_clampf(1.0f - inflammation_factor * 0.5f, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int audio_immune_default_config(audio_immune_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "audio_immune_default_config: NULL config");

    /* All features enabled by default */
    config->enable_cytokine_audio_modulation = true;
    config->enable_inflammation_processing_impairment = true;
    config->enable_audio_immune_trigger = true;
    config->enable_audio_immune_boost = true;
    config->enable_tinnitus_inflammation_coupling = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->threat_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->loudness_trigger_threshold = AUDIO_THREAT_LOUDNESS_THRESHOLD;
    config->anomaly_trigger_threshold = AUDIO_THREAT_NOVELTY_THRESHOLD;
    config->inflammation_audio_threshold = INFLAMMATION_AUDIO_THRESHOLD;

    return 0;
}

audio_immune_bridge_t* audio_immune_bridge_create(
    const audio_immune_config_t* config,
    brain_immune_system_t* immune_system,
    audio_cortex_t* audio_cortex
) {
    /* Guard: require immune and audio systems */
    NIMCP_API_CHECK_NULL_RET_NULL(immune_system, "audio_immune_bridge_create: NULL immune_system");
    NIMCP_API_CHECK_NULL_RET_NULL(audio_cortex, "audio_immune_bridge_create: NULL audio_cortex");

    /* Allocate bridge */
    audio_immune_bridge_t* bridge = (audio_immune_bridge_t*)
        nimcp_malloc(sizeof(audio_immune_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "audio_immune_bridge_create: Failed to allocate bridge");

    /* Initialize to zero */
    memset(bridge, 0, sizeof(audio_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->audio_cortex = audio_cortex;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_audio_modulation = config->enable_cytokine_audio_modulation;
        bridge->enable_inflammation_processing_impairment = config->enable_inflammation_processing_impairment;
        bridge->enable_audio_immune_trigger = config->enable_audio_immune_trigger;
        bridge->enable_audio_immune_boost = config->enable_audio_immune_boost;
        bridge->enable_tinnitus_inflammation_coupling = config->enable_tinnitus_inflammation_coupling;
    } else {
        /* Use defaults */
        audio_immune_config_t default_cfg;
        audio_immune_default_config(&default_cfg);
        bridge->enable_cytokine_audio_modulation = default_cfg.enable_cytokine_audio_modulation;
        bridge->enable_inflammation_processing_impairment = default_cfg.enable_inflammation_processing_impairment;
        bridge->enable_audio_immune_trigger = default_cfg.enable_audio_immune_trigger;
        bridge->enable_audio_immune_boost = default_cfg.enable_audio_immune_boost;
        bridge->enable_tinnitus_inflammation_coupling = default_cfg.enable_tinnitus_inflammation_coupling;
    }

    /* Initialize baselines */
    bridge->baseline_processing_accuracy = 1.0f;
    bridge->baseline_noise_tolerance = 1.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "audio_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    LOG_MODULE_INFO("audio_immune_bridge", "Bridge created successfully");
    return bridge;
}

void audio_immune_bridge_destroy(audio_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("audio_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Audio Implementation
 * ============================================================================ */

int audio_immune_apply_cytokine_effects(audio_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_apply_cytokine_effects: NULL bridge");
    if (!bridge->enable_cytokine_audio_modulation) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_apply_cytokine_effects: NULL immune_system");
    NIMCP_API_CHECK_NULL(bridge->audio_cortex, -1, "audio_immune_apply_cytokine_effects: NULL audio_cortex");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels */
    float il1, il6, tnf, ifn_gamma, il10;
    estimate_cytokine_levels(bridge->immune_system, &il1, &il6, &tnf, &ifn_gamma, &il10);

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_discrimination_loss = il1 * fabsf(CYTOKINE_IL1_AUDIO_IMPACT);
    bridge->cytokine_effects.il6_processing_impairment = il6 * fabsf(CYTOKINE_IL6_AUDIO_IMPACT);
    bridge->cytokine_effects.tnf_accuracy_reduction = tnf * fabsf(CYTOKINE_TNF_AUDIO_IMPACT);
    bridge->cytokine_effects.ifn_gamma_sensitivity_loss = ifn_gamma * fabsf(CYTOKINE_IFN_GAMMA_AUDIO_IMPACT);
    bridge->cytokine_effects.il10_recovery_boost = il10 * CYTOKINE_IL10_AUDIO_IMPACT;

    /* Compute aggregate effects */
    float pro_inflammatory_total =
        (il1 * CYTOKINE_IL1_AUDIO_IMPACT) +
        (il6 * CYTOKINE_IL6_AUDIO_IMPACT) +
        (tnf * CYTOKINE_TNF_AUDIO_IMPACT) +
        (ifn_gamma * CYTOKINE_IFN_GAMMA_AUDIO_IMPACT);

    bridge->cytokine_effects.total_processing_impact =
        pro_inflammatory_total + bridge->cytokine_effects.il10_recovery_boost;

    /* Noise sensitivity increases with pro-inflammatory cytokines */
    bridge->cytokine_effects.noise_sensitivity_increase =
        nimcp_clampf((il1 + il6 + tnf) / 3.0f, 0.0f, 1.0f);

    /* Auditory attention impairment (sickness behavior) */
    bridge->cytokine_effects.attention_impairment =
        nimcp_clampf((il1 * 0.4f + il6 * 0.3f + tnf * 0.3f), 0.0f, 1.0f);

    /* Auditory fatigue from cytokines */
    bridge->cytokine_effects.fatigue_level =
        nimcp_clampf((il1 + il6 + tnf) / 3.0f * 0.8f, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int audio_immune_apply_inflammation_effects(audio_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_apply_inflammation_effects: NULL bridge");
    if (!bridge->enable_inflammation_processing_impairment) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_apply_inflammation_effects: NULL immune_system");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    bool is_chronic = duration_sec >= (86400.0f * 7); /* 7 days */

    /* Update inflammation state */
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration_sec;
    bridge->inflammation_state.is_chronic = is_chronic;

    /* Map inflammation to processing impacts */
    float severity_factor = 0.0f;
    switch (level) {
        case INFLAMMATION_NONE:     severity_factor = 0.0f; break;
        case INFLAMMATION_LOCAL:    severity_factor = 0.2f; break;
        case INFLAMMATION_REGIONAL: severity_factor = 0.4f; break;
        case INFLAMMATION_SYSTEMIC: severity_factor = 0.7f; break;
        case INFLAMMATION_STORM:    severity_factor = 1.0f; break;
    }

    /* Chronic inflammation has worse effects */
    if (is_chronic) {
        severity_factor *= 1.3f;
        severity_factor = nimcp_clampf(severity_factor, 0.0f, 1.0f);
    }

    /* Processing accuracy degrades with inflammation */
    bridge->inflammation_state.processing_accuracy = 1.0f - (severity_factor * 0.6f);

    /* Frequency discrimination impairment (IL-6 effect) */
    bridge->inflammation_state.frequency_discrimination = 1.0f - (severity_factor * 0.5f);

    /* Temporal resolution impairment */
    bridge->inflammation_state.temporal_resolution = 1.0f - (severity_factor * 0.4f);

    /* Noise tolerance reduction */
    bridge->inflammation_state.noise_tolerance = 1.0f - (severity_factor * 0.7f);

    /* Processing bandwidth reduction */
    float bandwidth_reduction = audio_immune_compute_bandwidth_reduction(bridge);
    bridge->inflammation_state.processing_bandwidth = 1.0f - bandwidth_reduction;

    /* Tinnitus severity increases with chronic inflammation */
    if (is_chronic && severity_factor > 0.5f) {
        bridge->inflammation_state.tinnitus_severity =
            nimcp_clampf((severity_factor - 0.5f) * 2.0f, 0.0f, 1.0f);
        bridge->tinnitus_episodes++;
    } else {
        bridge->inflammation_state.tinnitus_severity = 0.0f;
    }

    /* Auditory attention reduction (sickness behavior) */
    bridge->inflammation_state.auditory_attention = 1.0f - (severity_factor * 0.6f);

    /* Orienting response reduction */
    bridge->inflammation_state.orienting_response = 1.0f - (severity_factor * 0.5f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float audio_immune_compute_bandwidth_reduction(const audio_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_immune_compute_bandwidth_reduction: bridge is NULL");
        return 0.0f;
    }
    if (!bridge->immune_system) return 0.0f;

    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);

    /* Map inflammation to bandwidth reduction */
    float reduction = 0.0f;
    switch (level) {
        case INFLAMMATION_NONE:     reduction = 0.0f; break;
        case INFLAMMATION_LOCAL:    reduction = 0.1f; break;
        case INFLAMMATION_REGIONAL: reduction = 0.25f; break;
        case INFLAMMATION_SYSTEMIC: reduction = 0.45f; break;
        case INFLAMMATION_STORM:    reduction = MAX_BANDWIDTH_REDUCTION; break;
    }

    return nimcp_clampf(reduction, 0.0f, MAX_BANDWIDTH_REDUCTION);
}

float audio_immune_compute_noise_sensitivity(const audio_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_immune_compute_noise_sensitivity: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->immune_system) return 1.0f;

    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);

    /* Map inflammation to noise sensitivity multiplier */
    float multiplier = 1.0f;
    switch (level) {
        case INFLAMMATION_NONE:     multiplier = 1.0f; break;
        case INFLAMMATION_LOCAL:    multiplier = 1.2f; break;
        case INFLAMMATION_REGIONAL: multiplier = 1.5f; break;
        case INFLAMMATION_SYSTEMIC: multiplier = 2.0f; break;
        case INFLAMMATION_STORM:    multiplier = 3.0f; break;
    }

    return multiplier;
}

/* ============================================================================
 * Audio → Immune Implementation
 * ============================================================================ */

int audio_immune_trigger_from_threat(
    audio_immune_bridge_t* bridge,
    float loudness,
    float novelty,
    float anomaly_score
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_trigger_from_threat: NULL bridge");
    if (!bridge->enable_audio_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_trigger_from_threat: NULL immune_system");

    /* Prepare data outside lock */
    bool do_trigger = false;
    uint8_t threat_epitope[16];
    uint32_t threat_severity_val = 0;
    float threat_sev_f = 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update trigger state */
    bridge->audio_trigger.loudness_level = nimcp_clampf(loudness, 0.0f, 1.0f);
    bridge->audio_trigger.novelty_score = nimcp_clampf(novelty, 0.0f, 1.0f);
    bridge->audio_trigger.anomaly_score = nimcp_clampf(anomaly_score, 0.0f, 1.0f);

    /* Check if threat level warrants immune response */
    bool should_trigger =
        (loudness >= AUDIO_THREAT_LOUDNESS_THRESHOLD) ||
        (novelty >= AUDIO_THREAT_NOVELTY_THRESHOLD) ||
        (anomaly_score >= AUDIO_THREAT_NOVELTY_THRESHOLD);

    if (should_trigger) {
        /* Compute threat severity */
        threat_sev_f = fmaxf(fmaxf(loudness, novelty), anomaly_score);
        threat_severity_val = (uint32_t)(threat_sev_f * 10.0f);
        if (threat_severity_val < 1) threat_severity_val = 1;
        if (threat_severity_val > 10) threat_severity_val = 10;

        /* Create threat signature */
        memset(threat_epitope, 0, sizeof(threat_epitope));
        threat_epitope[0] = (uint8_t)(loudness * 255);
        threat_epitope[1] = (uint8_t)(novelty * 255);
        threat_epitope[2] = (uint8_t)(anomaly_score * 255);
        threat_epitope[3] = 0xAD; /* Audio marker */

        do_trigger = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen to immune system OUTSIDE lock */
    if (do_trigger) {
        uint32_t antigen_id;
        int result = brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            threat_epitope,
            16,
            threat_severity_val,
            0, /* source node */
            &antigen_id
        );

        if (result == 0) {
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->audio_trigger.stress_response_triggered = true;
            bridge->audio_trigger.immune_surveillance_active = true;
            bridge->audio_trigger.immune_activation_level = threat_sev_f;
            bridge->audio_triggered_responses++;
            bridge->anomaly_count++;
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    return 0;
}

int audio_immune_trigger_from_processing_failure(
    audio_immune_bridge_t* bridge,
    float failure_rate
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_trigger_from_processing_failure: NULL bridge");
    if (!bridge->enable_audio_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_trigger_from_processing_failure: NULL immune_system");

    /* Prepare data outside lock */
    bool do_failure_trigger = false;
    uint8_t fail_epitope[8];
    uint32_t fail_severity = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update failure tracking */
    bridge->audio_trigger.processing_failure_rate = nimcp_clampf(failure_rate, 0.0f, 1.0f);
    bridge->processing_failures++;

    /* High failure rate triggers immune response */
    if (failure_rate >= 0.5f) {
        /* Create failure signature */
        memset(fail_epitope, 0, sizeof(fail_epitope));
        fail_epitope[0] = (uint8_t)(failure_rate * 255);
        fail_epitope[1] = 0xF1; /* Failure marker */

        fail_severity = (uint32_t)(failure_rate * 8.0f);
        if (fail_severity < 1) fail_severity = 1;

        do_failure_trigger = true;
        bridge->audio_trigger.immune_activation_level = failure_rate;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen OUTSIDE lock */
    if (do_failure_trigger) {
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            fail_epitope,
            8,
            fail_severity,
            0,
            &antigen_id
        );
    }

    return 0;
}

int audio_immune_amplify_tinnitus_inflammation(
    audio_immune_bridge_t* bridge,
    float tinnitus_severity
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_amplify_tinnitus_inflammation: NULL bridge");
    if (!bridge->enable_tinnitus_inflammation_coupling) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_amplify_tinnitus_inflammation: NULL immune_system");

    /* Prepare data outside lock */
    bool do_tinnitus_trigger = false;
    uint8_t tinnitus_epitope[8];
    uint32_t tinnitus_sev = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update tinnitus state */
    bridge->inflammation_state.tinnitus_severity = nimcp_clampf(tinnitus_severity, 0.0f, 1.0f);

    /* Tinnitus above threshold triggers inflammation */
    if (tinnitus_severity >= 0.5f) {
        /* Create tinnitus signature */
        memset(tinnitus_epitope, 0, sizeof(tinnitus_epitope));
        tinnitus_epitope[0] = (uint8_t)(tinnitus_severity * 255);
        tinnitus_epitope[1] = 0x71; /* Tinnitus marker */

        tinnitus_sev = (uint32_t)(tinnitus_severity * 6.0f);
        do_tinnitus_trigger = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen OUTSIDE lock */
    if (do_tinnitus_trigger) {
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            tinnitus_epitope,
            8,
            tinnitus_sev,
            0,
            &antigen_id
        );

        nimcp_mutex_lock(bridge->base.mutex);
        bridge->tinnitus_episodes++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int audio_immune_boost_from_calm_environment(
    audio_immune_bridge_t* bridge,
    float quietness,
    float music_presence,
    float predictability
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_boost_from_calm_environment: NULL bridge");
    if (!bridge->enable_audio_immune_boost) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "audio_immune_boost_from_calm_environment: NULL immune_system");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update boost state */
    bridge->audio_boost.quietness_level = nimcp_clampf(quietness, 0.0f, 1.0f);
    bridge->audio_boost.music_presence = nimcp_clampf(music_presence, 0.0f, 1.0f);
    bridge->audio_boost.predictability = nimcp_clampf(predictability, 0.0f, 1.0f);

    /* Calm environment boosts immunity */
    float calm_score = (quietness + music_presence + predictability) / 3.0f;

    if (calm_score >= 0.6f) {
        bridge->audio_boost.immune_enhancement = calm_score * 0.3f;
        bridge->audio_boost.il10_release_boost = calm_score * 0.4f;
        bridge->audio_boost.inflammation_reduction = calm_score * 0.25f;
        bridge->audio_boost.stress_reduction = calm_score * 0.5f;
        bridge->audio_boosts++;

        /* Note: Actual IL-10 release would require immune system API support */
    } else {
        bridge->audio_boost.immune_enhancement = 0.0f;
        bridge->audio_boost.il10_release_boost = 0.0f;
        bridge->audio_boost.inflammation_reduction = 0.0f;
        bridge->audio_boost.stress_reduction = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int audio_immune_bridge_update(
    audio_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_bridge_update: NULL bridge");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply immune effects on audio */
    audio_immune_apply_cytokine_effects(bridge);
    audio_immune_apply_inflammation_effects(bridge);

    /* Note: Audio-to-immune effects are event-driven via trigger functions */

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int audio_immune_get_cytokine_effects(
    const audio_immune_bridge_t* bridge,
    cytokine_audio_effects_t* effects
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_get_cytokine_effects: NULL bridge");
    NIMCP_API_CHECK_NULL(effects, -1, "audio_immune_get_cytokine_effects: NULL effects");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_audio_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int audio_immune_get_inflammation_state(
    const audio_immune_bridge_t* bridge,
    inflammation_audio_state_t* state
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_get_inflammation_state: NULL bridge");
    NIMCP_API_CHECK_NULL(state, -1, "audio_immune_get_inflammation_state: NULL state");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_audio_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool audio_immune_is_impaired(const audio_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "audio_immune_is_impaired: bridge is NULL");

            return false;

        }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Impairment if accuracy < 70% or bandwidth < 60% */
    bool impaired =
        (bridge->inflammation_state.processing_accuracy < 0.7f) ||
        (bridge->inflammation_state.processing_bandwidth < 0.6f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return impaired;
}

float audio_immune_get_accuracy_reduction(const audio_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_immune_get_accuracy_reduction: bridge is NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float reduction = 1.0f - bridge->inflammation_state.processing_accuracy;
    nimcp_mutex_unlock(bridge->base.mutex);

    return reduction;
}

float audio_immune_get_tinnitus_severity(const audio_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_immune_get_tinnitus_severity: bridge is NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float severity = bridge->inflammation_state.tinnitus_severity;
    nimcp_mutex_unlock(bridge->base.mutex);

    return severity;
}

float audio_immune_get_attention_level(const audio_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_immune_get_attention_level: bridge is NULL");
        return 1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float attention = bridge->inflammation_state.auditory_attention;
    nimcp_mutex_unlock(bridge->base.mutex);

    return attention;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define AUDIO_IMMUNE_MODULE_NAME "audio_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int audio_immune_connect_bio_async(audio_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_connect_bio_async: NULL bridge");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_AUDIO,
        .module_name = AUDIO_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("audio_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int audio_immune_disconnect_bio_async(audio_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "audio_immune_disconnect_bio_async: NULL bridge");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("audio_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool audio_immune_is_bio_async_connected(const audio_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "audio_immune_is_bio_async_connected: bridge is NULL");

            return false;

        }
    return bridge->base.bio_async_enabled;
}
