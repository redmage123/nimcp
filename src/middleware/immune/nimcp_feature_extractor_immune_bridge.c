/**
 * @file nimcp_feature_extractor_immune_bridge.c
 * @brief Feature Extractor-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and feature extraction
 * WHY:  Biological realism - cytokines affect sensory processing, anomalies trigger immune
 * HOW:  Monitor cytokine levels to modulate features, monitor features to trigger immune responses
 */

#include "middleware/immune/nimcp_feature_extractor_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(feature_extractor_immune_bridge)

/* Chronic inflammation threshold (7 days in seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD (86400.0f * 7.0f)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get max inflammation level from immune system
 *
 * WHAT: Query highest inflammation level
 * WHY:  Max inflammation determines precision reduction
 * HOW:  Scan inflammation sites, return highest level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;

    /* Scan inflammation sites */
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
 * WHY:  Chronic inflammation (>7 days) has stronger effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    uint64_t current_time = immune->start_time; /* Would use actual time */
    uint64_t oldest_start = current_time;

    /* Find oldest inflammation site */
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
        }
    }

    return (float)(current_time - oldest_start) / 1000.0f;
}

/**
 * @brief Map inflammation level to precision multiplier
 *
 * WHAT: Convert inflammation severity to precision reduction
 * WHY:  Different inflammation levels have different impacts
 * HOW:  Use biologically-based mapping
 */
static float inflammation_to_precision(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_PRECISION_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_PRECISION_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_PRECISION_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_PRECISION_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_PRECISION_STORM;
        default:                    return INFLAMMATION_PRECISION_NONE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int feature_immune_default_config(feature_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_feature_modulation = true;
    config->enable_inflammation_precision_reduction = true;
    config->enable_feature_immune_trigger = true;
    config->enable_threat_feature_bias = true;
    config->enable_quality_monitoring = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->anomaly_trigger_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->burst_threshold = FEATURE_BURST_THREAT_THRESHOLD;
    config->fano_threshold = FEATURE_FANO_THREAT_THRESHOLD;
    config->isi_cv_threshold = FEATURE_ISI_CV_THREAT_THRESHOLD;
    config->sync_threshold = FEATURE_SYNC_THREAT_THRESHOLD;
    config->entropy_collapse_threshold = FEATURE_ENTROPY_DEAD_THRESHOLD;
    config->gamma_collapse_threshold = FEATURE_GAMMA_COLLAPSE_THRESHOLD;

    /* Quality monitoring */
    config->chronic_degradation_threshold = 0.5f;
    config->chronic_duration_sec = 300.0f; /* 5 minutes */

    return 0;
}

feature_immune_bridge_t* feature_immune_bridge_create(
    const feature_immune_config_t* config,
    brain_immune_system_t* immune_system,
    feature_extractor_t feature_extractor
) {
    /* Guard: require immune and feature extractor */
    if (!immune_system || !feature_extractor) {
        LOG_MODULE_ERROR("feature_immune_bridge",
                  "Cannot create bridge without immune and feature extractor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "feature_immune_bridge_create: required parameter is NULL (immune_system, feature_extractor)");
        return NULL;
    }

    /* Allocate bridge */
    feature_immune_bridge_t* bridge = (feature_immune_bridge_t*)
        nimcp_malloc(sizeof(feature_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("feature_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(feature_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->feature_extractor = feature_extractor;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_feature_modulation = config->enable_cytokine_feature_modulation;
        bridge->enable_inflammation_precision_reduction = config->enable_inflammation_precision_reduction;
        bridge->enable_feature_immune_trigger = config->enable_feature_immune_trigger;
        bridge->enable_threat_feature_bias = config->enable_threat_feature_bias;
        bridge->enable_quality_monitoring = config->enable_quality_monitoring;
    } else {
        /* Use defaults */
        feature_immune_config_t default_cfg;
        feature_immune_default_config(&default_cfg);
        bridge->enable_cytokine_feature_modulation = default_cfg.enable_cytokine_feature_modulation;
        bridge->enable_inflammation_precision_reduction = default_cfg.enable_inflammation_precision_reduction;
        bridge->enable_feature_immune_trigger = default_cfg.enable_feature_immune_trigger;
        bridge->enable_threat_feature_bias = default_cfg.enable_threat_feature_bias;
        bridge->enable_quality_monitoring = default_cfg.enable_quality_monitoring;
    }

    /* Initialize quality monitor */
    bridge->quality_monitor.mean_precision = 1.0f;
    bridge->quality_monitor.min_precision = 1.0f;
    bridge->quality_monitor.precision_stability = 1.0f;
    bridge->quality_monitor.chronic_threshold =
        config ? config->chronic_degradation_threshold : 0.5f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "feature_extractor_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("feature_immune_bridge", "Bridge created successfully");
    return bridge;
}

void feature_immune_bridge_destroy(feature_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("feature_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Feature Extraction Implementation
 * ============================================================================ */

int feature_immune_apply_cytokine_effects(feature_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_feature_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Query cytokine concentrations from immune system */
    /* Note: Would need actual implementation in brain_immune to query cytokines */
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float tnf_level = 0.0f;
    float ifn_gamma_level = 0.0f;

    /* Compute individual cytokine impacts */
    bridge->cytokine_effects.il1_precision_reduction =
        il1_level * fabsf(CYTOKINE_IL1_PRECISION_IMPACT);
    bridge->cytokine_effects.il6_precision_reduction =
        il6_level * fabsf(CYTOKINE_IL6_PRECISION_IMPACT);
    bridge->cytokine_effects.tnf_precision_reduction =
        tnf_level * fabsf(CYTOKINE_TNF_PRECISION_IMPACT);
    bridge->cytokine_effects.ifn_gamma_precision_reduction =
        ifn_gamma_level * fabsf(CYTOKINE_IFN_GAMMA_PRECISION_IMPACT);

    /* Compute total precision reduction */
    float total_reduction =
        bridge->cytokine_effects.il1_precision_reduction +
        bridge->cytokine_effects.il6_precision_reduction +
        bridge->cytokine_effects.tnf_precision_reduction +
        bridge->cytokine_effects.ifn_gamma_precision_reduction;

    /* Precision factor is 1.0 minus reduction */
    bridge->cytokine_effects.total_precision_factor =
        nimcp_clampf(1.0f - total_reduction, 0.0f, 1.0f);

    /* Noise amplification is inverse of precision */
    bridge->cytokine_effects.noise_amplification = total_reduction;

    /* Bandwidth reduction scales with cytokine load */
    bridge->cytokine_effects.bandwidth_reduction = total_reduction * 0.7f;

    /* Temporal jitter increases with cytokines (in ms) */
    bridge->cytokine_effects.temporal_jitter = total_reduction * 5.0f;

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int feature_immune_apply_inflammation_effects(feature_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_precision_reduction) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    bool is_chronic = duration_sec >= CHRONIC_INFLAMMATION_THRESHOLD;

    /* Update state */
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration_sec;
    bridge->inflammation_state.is_chronic = is_chronic;

    /* Compute precision multiplier */
    bridge->inflammation_state.precision_multiplier = inflammation_to_precision(level);

    /* Compute specific coding impairments */
    float impairment = 1.0f - bridge->inflammation_state.precision_multiplier;
    bridge->inflammation_state.rate_coding_impairment = impairment * 0.6f;
    bridge->inflammation_state.temporal_coding_impairment = impairment * 0.8f;
    bridge->inflammation_state.population_coding_impairment = impairment * 0.7f;
    bridge->inflammation_state.oscillation_impairment = impairment * 0.9f;

    /* Threat feature bias increases with inflammation */
    bridge->inflammation_state.threat_feature_bias = impairment * 0.5f;
    bridge->inflammation_state.non_threat_suppression = impairment * 0.4f;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float feature_immune_compute_precision_reduction(const feature_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = bridge->cytokine_effects.total_precision_factor;
    float inflammation_factor = bridge->inflammation_state.precision_multiplier;

    /* Multiplicative combination (both can reduce precision) */
    return cytokine_factor * inflammation_factor;
}

int feature_immune_apply_threat_bias(feature_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_threat_feature_bias) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Threat bias is already computed in inflammation state */
    /* This function would adjust feature extractor thresholds */
    /* Implementation would depend on feature extractor API */

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Feature Extraction → Immune Implementation
 * ============================================================================ */

int feature_immune_trigger_from_anomalies(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
) {
    /* Guard clauses */
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_trigger_from_anomalies: required parameter is NULL (bridge, features)");
        return -1;
    }
    if (!bridge->enable_feature_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_trigger_from_anomalies: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Reset anomaly flags */
    memset(&bridge->immune_trigger, 0, sizeof(feature_immune_trigger_t));

    /* Check burst index anomaly */
    if (features->burst_index > FEATURE_BURST_THREAT_THRESHOLD) {
        bridge->immune_trigger.burst_anomaly = true;
        bridge->immune_trigger.burst_severity =
            (features->burst_index - FEATURE_BURST_THREAT_THRESHOLD) /
            (1.0f - FEATURE_BURST_THREAT_THRESHOLD);
    }

    /* Check Fano factor anomaly */
    if (features->fano_factor > FEATURE_FANO_THREAT_THRESHOLD) {
        bridge->immune_trigger.fano_anomaly = true;
        bridge->immune_trigger.fano_severity =
            nimcp_clampf((features->fano_factor - FEATURE_FANO_THREAT_THRESHOLD) / 3.0f, 0.0f, 1.0f);
    }

    /* Check ISI CV anomaly */
    if (features->isi_cv > FEATURE_ISI_CV_THREAT_THRESHOLD) {
        bridge->immune_trigger.isi_cv_anomaly = true;
        bridge->immune_trigger.isi_cv_severity =
            nimcp_clampf((features->isi_cv - FEATURE_ISI_CV_THREAT_THRESHOLD) / 2.0f, 0.0f, 1.0f);
    }

    /* Check synchrony anomaly */
    if (features->synchrony_index > FEATURE_SYNC_THREAT_THRESHOLD) {
        bridge->immune_trigger.sync_anomaly = true;
        bridge->immune_trigger.sync_severity =
            (features->synchrony_index - FEATURE_SYNC_THREAT_THRESHOLD) /
            (1.0f - FEATURE_SYNC_THREAT_THRESHOLD);
    }

    /* Check entropy collapse */
    if (features->spike_entropy < FEATURE_ENTROPY_DEAD_THRESHOLD) {
        bridge->immune_trigger.entropy_collapse = true;
        bridge->immune_trigger.entropy_severity =
            1.0f - (features->spike_entropy / FEATURE_ENTROPY_DEAD_THRESHOLD);
    }

    /* Check gamma collapse */
    if (features->gamma_power < FEATURE_GAMMA_COLLAPSE_THRESHOLD) {
        bridge->immune_trigger.gamma_collapse = true;
        bridge->immune_trigger.gamma_severity =
            1.0f - (features->gamma_power / FEATURE_GAMMA_COLLAPSE_THRESHOLD);
    }

    /* Compute total threat level */
    bridge->immune_trigger.total_threat_level =
        (bridge->immune_trigger.burst_severity +
         bridge->immune_trigger.fano_severity +
         bridge->immune_trigger.isi_cv_severity +
         bridge->immune_trigger.sync_severity +
         bridge->immune_trigger.entropy_severity +
         bridge->immune_trigger.gamma_severity) / 6.0f;

    /* Determine immune severity (1-10) */
    if (bridge->immune_trigger.entropy_collapse) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_ENTROPY_ZERO;
    } else if (bridge->immune_trigger.gamma_collapse) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_GAMMA_COLLAPSE;
    } else if (bridge->immune_trigger.isi_cv_anomaly) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_ISI_ANOMALY;
    } else if (bridge->immune_trigger.fano_anomaly) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_FANO_ANOMALY;
    } else if (bridge->immune_trigger.burst_anomaly) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_BURST_ANOMALY;
    } else if (bridge->immune_trigger.sync_anomaly) {
        bridge->immune_trigger.immune_severity = FEATURE_SEVERITY_SYNC_ANOMALY;
    } else {
        bridge->immune_trigger.immune_severity = 0;
    }

    /* If threat detected, present antigen to immune system */
    if (bridge->immune_trigger.immune_severity > 0) {
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];

        /* Create epitope from feature anomaly pattern */
        memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
        epitope[0] = (uint8_t)(features->burst_index * 255);
        epitope[1] = (uint8_t)(nimcp_clampf(features->fano_factor / 10.0f, 0.0f, 1.0f) * 255);
        epitope[2] = (uint8_t)(nimcp_clampf(features->isi_cv / 5.0f, 0.0f, 1.0f) * 255);
        epitope[3] = (uint8_t)(features->synchrony_index * 255);
        epitope[4] = (uint8_t)(features->spike_entropy * 255);
        epitope[5] = (uint8_t)(features->gamma_power * 255);

        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            6,
            bridge->immune_trigger.immune_severity,
            0, /* source_node */
            &antigen_id
        );

        bridge->feature_triggered_responses++;
        bridge->anomalies_detected++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int feature_immune_escalate_from_degradation(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
) {
    /* Guard clauses */
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_escalate_from_degradation: required parameter is NULL (bridge, features)");
        return -1;
    }
    if (!bridge->enable_quality_monitoring) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute current precision */
    float current_precision = feature_immune_compute_precision_reduction(bridge);

    /* Update quality monitor */
    bridge->quality_monitor.mean_precision =
        0.9f * bridge->quality_monitor.mean_precision + 0.1f * current_precision;

    if (current_precision < bridge->quality_monitor.min_precision) {
        bridge->quality_monitor.min_precision = current_precision;
    }

    /* Check for chronic degradation */
    if (current_precision < bridge->quality_monitor.chronic_threshold) {
        bridge->quality_monitor.degradation_duration_sec += 0.1f; /* Assume 100ms update */
    } else {
        bridge->quality_monitor.degradation_duration_sec = 0.0f;
    }

    /* Check if chronic */
    float chronic_duration_threshold = 300.0f; /* 5 minutes */
    bridge->quality_monitor.chronic_degradation =
        bridge->quality_monitor.degradation_duration_sec >= chronic_duration_threshold;

    /* If chronic degradation and not already activated, escalate */
    if (bridge->quality_monitor.chronic_degradation &&
        !bridge->quality_monitor.immune_activated) {

        /* Escalate to systemic inflammation */
        uint32_t site_id;
        brain_immune_initiate_inflammation(
            bridge->immune_system,
            0, /* region_id */
            0, /* antigen_id */
            &site_id
        );

        bridge->quality_monitor.immune_activated = true;
        bridge->quality_monitor.escalation_count++;
        bridge->quality_escalations++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int feature_immune_detect_dead_neurons(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
) {
    /* Guard clauses */
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_detect_dead_neurons: required parameter is NULL (bridge, features)");
        return -1;
    }
    if (!bridge->enable_feature_immune_trigger) return 0;

    /* Check for zero entropy (dead neurons) */
    if (features->spike_entropy < FEATURE_ENTROPY_DEAD_THRESHOLD) {
        /* Trigger critical immune response */
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
        epitope[0] = 0xFF; /* Dead neuron marker */

        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            1,
            FEATURE_SEVERITY_ENTROPY_ZERO,
            0,
            &antigen_id
        );

        return 0;
    }

    return 0;
}

int feature_immune_detect_binding_failure(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
) {
    /* Guard clauses */
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_detect_binding_failure: required parameter is NULL (bridge, features)");
        return -1;
    }
    if (!bridge->enable_feature_immune_trigger) return 0;

    /* Check for gamma collapse (binding failure) */
    if (features->gamma_power < FEATURE_GAMMA_COLLAPSE_THRESHOLD) {
        /* Trigger severe immune response */
        uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
        memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
        epitope[0] = 0xFE; /* Binding failure marker */

        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            1,
            FEATURE_SEVERITY_GAMMA_COLLAPSE,
            0,
            &antigen_id
        );

        return 0;
    }

    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int feature_immune_bridge_update(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply immune → feature effects */
    feature_immune_apply_cytokine_effects(bridge);
    feature_immune_apply_inflammation_effects(bridge);
    feature_immune_apply_threat_bias(bridge);

    /* Apply feature → immune effects (if features provided) */
    if (features) {
        feature_immune_trigger_from_anomalies(bridge, features);
        feature_immune_escalate_from_degradation(bridge, features);
        feature_immune_detect_dead_neurons(bridge, features);
        feature_immune_detect_binding_failure(bridge, features);
    }

    bridge->total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int feature_immune_get_cytokine_effects(
    const feature_immune_bridge_t* bridge,
    cytokine_feature_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->cytokine_effects;
    return 0;
}

int feature_immune_get_inflammation_state(
    const feature_immune_bridge_t* bridge,
    inflammation_feature_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->inflammation_state;
    return 0;
}

float feature_immune_get_precision_factor(const feature_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    return feature_immune_compute_precision_reduction(bridge);
}

bool feature_immune_is_threat_detected(const feature_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->immune_trigger.immune_severity > 0;
}

float feature_immune_get_quality_score(const feature_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Combine precision, stability, and degradation */
    float precision_score = bridge->quality_monitor.mean_precision;
    float stability_score = bridge->quality_monitor.precision_stability;
    float degradation_penalty = bridge->quality_monitor.chronic_degradation ? 0.5f : 1.0f;

    return precision_score * stability_score * degradation_penalty;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define FEATURE_EXTRACTOR_IMMUNE_MODULE_NAME "feature_extractor_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int feature_extractor_immune_connect_bio_async(feature_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_FEATURE_EXTRACTOR,
        .module_name = FEATURE_EXTRACTOR_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("feature_extractor_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int feature_extractor_immune_disconnect_bio_async(feature_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("feature_extractor_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool feature_extractor_immune_is_bio_async_connected(const feature_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
