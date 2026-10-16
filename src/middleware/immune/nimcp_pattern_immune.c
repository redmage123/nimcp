/**
 * @file nimcp_pattern_immune.c
 * @brief Pattern Detection-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and pattern detection systems
 * WHY:  Biological realism - inflammation disrupts patterns, anomalies trigger immune
 * HOW:  Monitor inflammation to degrade detection, monitor patterns to trigger immune
 */

#include "middleware/immune/nimcp_pattern_immune.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pattern_immune)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get max inflammation level from immune system
 *
 * WHAT: Query highest inflammation level
 * WHY:  Max inflammation determines pattern degradation
 * HOW:  Iterate inflammation sites, return max (skip resolving sites)
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;

    /* Query immune system for active inflammation sites */
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Skip sites that are resolving (resolution_progress > 0) */
        if (immune->inflammation_sites[i].resolution_progress > 0.0f) {
            continue;
        }
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
 * WHY:  Chronic inflammation has cumulative effects
 * HOW:  Find oldest active inflammation site (skip resolving sites)
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) return 0.0f;

    uint64_t current_time = immune->start_time;  /* Would use actual current time */
    uint64_t oldest_start = current_time;
    bool found_active = false;

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Skip sites that are resolving */
        if (immune->inflammation_sites[i].resolution_progress > 0.0f) {
            continue;
        }
        if (immune->inflammation_sites[i].start_time < oldest_start) {
            oldest_start = immune->inflammation_sites[i].start_time;
            found_active = true;
        }
    }

    if (!found_active) return 0.0f;
    return (float)(current_time - oldest_start) / 1000.0f;  /* Convert ms to sec */
}

/**
 * @brief Hash pattern features to signature
 *
 * WHAT: Convert pattern features to fixed-size signature
 * WHY:  Pattern signature → immune epitope
 * HOW:  Simple hash combining feature values
 */
static void hash_pattern_to_signature(
    const float* features,
    uint32_t num_features,
    uint8_t* signature,
    size_t max_len
) {
    if (!features || !signature || num_features == 0) return;

    /* Simple hash: XOR feature bytes into signature */
    memset(signature, 0, max_len);

    for (uint32_t i = 0; i < num_features && i < max_len; i++) {
        /* Convert float to bytes and hash */
        uint32_t fval;
        memcpy(&fval, &features[i], sizeof(uint32_t));
        signature[i % max_len] ^= (uint8_t)(fval & 0xFF);
        signature[(i + 1) % max_len] ^= (uint8_t)((fval >> 8) & 0xFF);
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int pattern_immune_default_config(pattern_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_inflammation_degradation = true;
    config->enable_oscillation_monitoring = true;
    config->enable_synchrony_monitoring = true;
    config->enable_sequence_monitoring = true;
    config->enable_pattern_library_monitoring = true;

    /* Default sensitivities */
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->anomaly_detection_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Use default thresholds (0 = use constants) */
    config->pathological_hypersync_threshold = 0.0f;
    config->pathological_desync_threshold = 0.0f;
    config->pathological_sequence_fail_rate = 0.0f;

    /* Default capacity */
    config->max_anomalies = PATTERN_IMMUNE_MAX_ANOMALIES;

    return 0;
}

pattern_immune_bridge_t* pattern_immune_bridge_create(
    const pattern_immune_config_t* config,
    brain_immune_system_t* immune_system,
    oscillation_detector_t* oscillation_detector,
    synchrony_detector_t* synchrony_detector,
    sequence_detector_t* sequence_detector,
    pattern_library_t* pattern_library
) {
    /* Guard: require immune system */
    if (!immune_system) {
        LOG_MODULE_ERROR("pattern_immune_bridge",
                  "Cannot create bridge without immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    pattern_immune_bridge_t* bridge = (pattern_immune_bridge_t*)
        nimcp_malloc(sizeof(pattern_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("pattern_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(pattern_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->oscillation_detector = oscillation_detector;
    bridge->synchrony_detector = synchrony_detector;
    bridge->sequence_detector = sequence_detector;
    bridge->pattern_library = pattern_library;

    /* Apply configuration */
    pattern_immune_config_t default_cfg;
    const pattern_immune_config_t* cfg = config;
    if (!cfg) {
        pattern_immune_default_config(&default_cfg);
        cfg = &default_cfg;
    }

    bridge->enable_inflammation_degradation = cfg->enable_inflammation_degradation;
    bridge->enable_oscillation_monitoring = cfg->enable_oscillation_monitoring;
    bridge->enable_synchrony_monitoring = cfg->enable_synchrony_monitoring;
    bridge->enable_sequence_monitoring = cfg->enable_sequence_monitoring;
    bridge->enable_pattern_library_monitoring = cfg->enable_pattern_library_monitoring;

    /* Allocate anomalies array */
    bridge->anomaly_capacity = cfg->max_anomalies;
    bridge->anomalies = (pattern_anomaly_t*)
        nimcp_malloc(sizeof(pattern_anomaly_t) * bridge->anomaly_capacity);
    if (!bridge->anomalies) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_immune_bridge_create: bridge->anomalies is NULL");
        return NULL;
    }
    memset(bridge->anomalies, 0, sizeof(pattern_anomaly_t) * bridge->anomaly_capacity);

    /* Create mutex */
    bridge->base.mutex = nimcp_platform_mutex_create();
    if (!bridge->base.mutex) {
        nimcp_free(bridge->anomalies);
        nimcp_free(bridge);    return NULL;
    }

    /* Initialize accuracy factors to 1.0 (no degradation) */
    bridge->inflammation_effects.oscillation_accuracy_factor = 1.0f;
    bridge->inflammation_effects.synchrony_accuracy_factor = 1.0f;
    bridge->inflammation_effects.sequence_accuracy_factor = 1.0f;
    bridge->inflammation_effects.pattern_match_accuracy_factor = 1.0f;

    LOG_MODULE_INFO("pattern_immune_bridge", "Bridge created successfully");
    return bridge;
}

void pattern_immune_bridge_destroy(pattern_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free anomalies */
    if (bridge->anomalies) {
        nimcp_free(bridge->anomalies);
    }

    /* Free bridge */
    nimcp_free(bridge);
    LOG_MODULE_INFO("pattern_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Pattern Implementation
 * ============================================================================ */

float pattern_immune_compute_accuracy_factor(
    const pattern_immune_bridge_t* bridge,
    brain_inflammation_level_t inflammation_level
) {
    if (!bridge) return 1.0f;

    /* Map inflammation level to accuracy factor */
    float factor;
    switch (inflammation_level) {
        case INFLAMMATION_NONE:
            factor = INFLAMMATION_NONE_ACCURACY_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            factor = INFLAMMATION_LOCAL_ACCURACY_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            factor = INFLAMMATION_REGIONAL_ACCURACY_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            factor = INFLAMMATION_SYSTEMIC_ACCURACY_FACTOR;
            break;
        case INFLAMMATION_STORM:
            factor = INFLAMMATION_STORM_ACCURACY_FACTOR;
            break;
        default:
            factor = 1.0f;
    }

    return factor;
}

int pattern_immune_apply_inflammation_effects(pattern_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_degradation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Get current inflammation state */
    brain_inflammation_level_t max_level = get_max_inflammation_level(bridge->immune_system);
    float duration_sec = get_inflammation_duration_sec(bridge->immune_system);

    /* Compute base accuracy factor */
    float base_factor = pattern_immune_compute_accuracy_factor(bridge, max_level);

    /* Update inflammation effects */
    bridge->inflammation_effects.inflammation_level = max_level;
    bridge->inflammation_effects.inflammation_duration_sec = duration_sec;

    /* Apply to all pattern detectors */
    bridge->inflammation_effects.oscillation_accuracy_factor = base_factor;
    bridge->inflammation_effects.synchrony_accuracy_factor = base_factor;
    bridge->inflammation_effects.sequence_accuracy_factor = base_factor;
    bridge->inflammation_effects.pattern_match_accuracy_factor = base_factor;

    /* Compute specific disruptions based on inflammation level */
    float inflammation_ratio = (float)max_level / (float)INFLAMMATION_STORM;

    bridge->inflammation_effects.gamma_coherence_loss = inflammation_ratio * 0.7f;
    bridge->inflammation_effects.theta_gamma_uncoupling = inflammation_ratio * 0.6f;
    bridge->inflammation_effects.synchrony_reduction = inflammation_ratio * 0.5f;
    bridge->inflammation_effects.sequence_replay_failure_rate = inflammation_ratio * 0.8f;
    bridge->inflammation_effects.temporal_precision_loss = inflammation_ratio * 0.4f;
    bridge->inflammation_effects.pattern_completion_impairment = inflammation_ratio * 0.5f;

    /* Track degradation event */
    if (base_factor < 1.0f) {
        bridge->pattern_degradation_events++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int pattern_immune_degrade_oscillation(pattern_immune_bridge_t* bridge) {
    if (!bridge || !bridge->oscillation_detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_degrade_oscillation: required parameter is NULL (bridge, bridge->oscillation_detector)");
        return -1;
    }

    /* Oscillation degradation happens via accuracy factor */
    /* Actual detector would need to expose sensitivity settings */
    /* For now, just log the degradation */
    float factor = bridge->inflammation_effects.oscillation_accuracy_factor;

    if (factor < 1.0f) {
        LOG_MODULE_DEBUG("pattern_immune_bridge",
                  "Oscillation accuracy degraded to %.2f", factor);
    }

    return 0;
}

int pattern_immune_degrade_synchrony(pattern_immune_bridge_t* bridge) {
    if (!bridge || !bridge->synchrony_detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_degrade_synchrony: required parameter is NULL (bridge, bridge->synchrony_detector)");
        return -1;
    }

    float factor = bridge->inflammation_effects.synchrony_accuracy_factor;

    if (factor < 1.0f) {
        LOG_MODULE_DEBUG("pattern_immune_bridge",
                  "Synchrony accuracy degraded to %.2f", factor);
    }

    return 0;
}

int pattern_immune_degrade_sequence(pattern_immune_bridge_t* bridge) {
    if (!bridge || !bridge->sequence_detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_degrade_sequence: required parameter is NULL (bridge, bridge->sequence_detector)");
        return -1;
    }

    float factor = bridge->inflammation_effects.sequence_accuracy_factor;

    if (factor < 1.0f) {
        LOG_MODULE_DEBUG("pattern_immune_bridge",
                  "Sequence accuracy degraded to %.2f", factor);
    }

    return 0;
}

int pattern_immune_degrade_pattern_library(pattern_immune_bridge_t* bridge) {
    if (!bridge || !bridge->pattern_library) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_degrade_pattern_library: required parameter is NULL (bridge, bridge->pattern_library)");
        return -1;
    }

    float factor = bridge->inflammation_effects.pattern_match_accuracy_factor;

    if (factor < 1.0f) {
        LOG_MODULE_DEBUG("pattern_immune_bridge",
                  "Pattern library accuracy degraded to %.2f", factor);
    }

    return 0;
}

/* ============================================================================
 * Pattern → Immune Implementation
 * ============================================================================ */

int pattern_immune_detect_pathological_oscillation(
    pattern_immune_bridge_t* bridge,
    const oscillation_result_t* oscillation_result
) {
    /* Guard clauses */
    if (!bridge || !oscillation_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_detect_pathological_oscillation: required parameter is NULL (bridge, oscillation_result)");
        return -1;
    }
    if (!bridge->enable_oscillation_monitoring) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bool detected_anomaly = false;
    pattern_anomaly_type_t anomaly_type = PATTERN_ANOMALY_NONE;

    /* Check for seizure-like high gamma (highest priority) */
    if (oscillation_result->bands[OSC_BAND_GAMMA].peak_frequency > PATHOLOGICAL_GAMMA_MIN_HZ) {
        bridge->pathological_oscillation.has_seizure_oscillation = true;
        bridge->pathological_oscillation.seizure_gamma_power =
            oscillation_result->bands[OSC_BAND_GAMMA].power;
        detected_anomaly = true;
        anomaly_type = PATTERN_ANOMALY_SEIZURE_OSCILLATION;
    }
    /* Check for delta intrusion during waking */
    else if (oscillation_result->bands[OSC_BAND_DELTA].relative_power > PATHOLOGICAL_DELTA_WAKING_POWER) {
        bridge->pathological_oscillation.has_delta_intrusion = true;
        bridge->pathological_oscillation.delta_power_waking =
            oscillation_result->bands[OSC_BAND_DELTA].relative_power;
        detected_anomaly = true;
        anomaly_type = PATTERN_ANOMALY_DELTA_INTRUSION;
    }
    /* Check for theta-gamma uncoupling - only if coupling was expected but missing */
    else if (oscillation_result->bands[OSC_BAND_THETA].power > 0.1f &&
             !oscillation_result->has_theta_gamma_coupling) {
        bridge->pathological_oscillation.has_theta_gamma_uncoupling = true;
        detected_anomaly = true;
        anomaly_type = PATTERN_ANOMALY_THETA_GAMMA_UNCOUPLING;
    }

    /* Create anomaly if detected */
    if (detected_anomaly && bridge->anomaly_count < bridge->anomaly_capacity) {
        pattern_anomaly_t* anomaly = &bridge->anomalies[bridge->anomaly_count++];
        anomaly->anomaly_id = bridge->next_anomaly_id++;
        anomaly->type = anomaly_type;
        anomaly->detection_time_ms = 0;  /* Would use actual timestamp */
        anomaly->severity = 0.8f;
        anomaly->confidence = 0.9f;
        anomaly->source = ANOMALY_SOURCE_OSCILLATION;

        /* Create signature from band powers */
        float features[OSC_NUM_BANDS];
        for (int i = 0; i < OSC_NUM_BANDS; i++) {
            features[i] = oscillation_result->bands[i].power;
        }
        hash_pattern_to_signature(features, OSC_NUM_BANDS,
                                  anomaly->pattern_signature,
                                  BRAIN_IMMUNE_EPITOPE_SIZE);
        anomaly->signature_len = BRAIN_IMMUNE_EPITOPE_SIZE;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int pattern_immune_detect_pathological_synchrony(
    pattern_immune_bridge_t* bridge,
    const synchrony_result_t* synchrony_result
) {
    /* Guard clauses */
    if (!bridge || !synchrony_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_detect_pathological_synchrony: required parameter is NULL (bridge, synchrony_result)");
        return -1;
    }
    if (!bridge->enable_synchrony_monitoring) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bool detected_anomaly = false;
    pattern_anomaly_type_t anomaly_type = PATTERN_ANOMALY_NONE;

    /* Check for hypersynchrony */
    if (synchrony_result->synchrony_index > PATHOLOGICAL_HYPERSYNCHRONY_THRESHOLD) {
        bridge->pathological_synchrony.has_hypersynchrony = true;
        bridge->pathological_synchrony.synchrony_index = synchrony_result->synchrony_index;
        detected_anomaly = true;
        anomaly_type = PATTERN_ANOMALY_HYPERSYNCHRONY;
    }

    /* Check for desynchronization */
    if (synchrony_result->synchrony_index < PATHOLOGICAL_DESYNCHRONY_THRESHOLD) {
        bridge->pathological_synchrony.has_desynchronization = true;
        bridge->pathological_synchrony.mean_correlation = synchrony_result->mean_correlation;
        detected_anomaly = true;
        anomaly_type = PATTERN_ANOMALY_DESYNCHRONIZATION;
    }

    /* Create anomaly if detected */
    if (detected_anomaly && bridge->anomaly_count < bridge->anomaly_capacity) {
        pattern_anomaly_t* anomaly = &bridge->anomalies[bridge->anomaly_count++];
        anomaly->anomaly_id = bridge->next_anomaly_id++;
        anomaly->type = anomaly_type;
        anomaly->detection_time_ms = 0;
        anomaly->severity = 0.7f;
        anomaly->confidence = 0.85f;
        anomaly->source = ANOMALY_SOURCE_SYNCHRONY;

        /* Create signature from synchrony metrics */
        float features[3] = {
            synchrony_result->synchrony_index,
            synchrony_result->coincidence_rate,
            synchrony_result->mean_correlation
        };
        hash_pattern_to_signature(features, 3,
                                  anomaly->pattern_signature,
                                  BRAIN_IMMUNE_EPITOPE_SIZE);
        anomaly->signature_len = BRAIN_IMMUNE_EPITOPE_SIZE;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int pattern_immune_detect_pathological_sequence(
    pattern_immune_bridge_t* bridge,
    const sequence_detection_t* detections,
    uint32_t num_detections
) {
    /* Guard clauses */
    if (!bridge || !detections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_detect_pathological_sequence: required parameter is NULL (bridge, detections)");
        return -1;
    }
    if (!bridge->enable_sequence_monitoring) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Check for repetitive sequences */
    uint32_t repetition_counts[256] = {0};  /* Track template repetitions */
    for (uint32_t i = 0; i < num_detections; i++) {
        uint32_t tid = detections[i].template_id % 256;
        repetition_counts[tid]++;

        if (repetition_counts[tid] >= PATHOLOGICAL_REPETITION_COUNT) {
            bridge->pathological_sequence.has_repetitive_sequences = true;
            bridge->pathological_sequence.max_repetition_count = repetition_counts[tid];

            /* Create anomaly */
            if (bridge->anomaly_count < bridge->anomaly_capacity) {
                pattern_anomaly_t* anomaly = &bridge->anomalies[bridge->anomaly_count++];
                anomaly->anomaly_id = bridge->next_anomaly_id++;
                anomaly->type = PATTERN_ANOMALY_REPETITIVE_SEQUENCE;
                anomaly->detection_time_ms = detections[i].start_time_ms;
                anomaly->severity = 0.6f;
                anomaly->confidence = 0.8f;
                anomaly->source = ANOMALY_SOURCE_SEQUENCE;

                /* Simple signature from template ID */
                memset(anomaly->pattern_signature, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
                uint32_t tid_val = detections[i].template_id;
                memcpy(anomaly->pattern_signature, &tid_val, sizeof(uint32_t));
                anomaly->signature_len = sizeof(uint32_t);
            }
            break;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int pattern_immune_present_anomaly(
    pattern_immune_bridge_t* bridge,
    pattern_anomaly_t* anomaly
) {
    /* Guard clauses */
    if (!bridge || !anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_present_anomaly: required parameter is NULL (bridge, anomaly)");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_present_anomaly: bridge->immune_system is NULL");
        return -1;
    }
    if (anomaly->immune_alerted) return 0;  /* Already presented */

    /* Determine severity based on anomaly type */
    uint32_t severity = 5;  /* Default */
    switch (anomaly->type) {
        case PATTERN_ANOMALY_SEIZURE_OSCILLATION:
            severity = PATTERN_ANOMALY_SEIZURE_SEVERITY;
            break;
        case PATTERN_ANOMALY_HYPERSYNCHRONY:
            severity = PATTERN_ANOMALY_HYPERSYNC_SEVERITY;
            break;
        case PATTERN_ANOMALY_DESYNCHRONIZATION:
            severity = PATTERN_ANOMALY_DESYNC_SEVERITY;
            break;
        case PATTERN_ANOMALY_SEQUENCE_FAILURE:
            severity = PATTERN_ANOMALY_SEQUENCE_FAIL_SEVERITY;
            break;
        case PATTERN_ANOMALY_REPETITIVE_SEQUENCE:
            severity = PATTERN_ANOMALY_REPETITIVE_SEVERITY;
            break;
        default:
            break;
    }

    /* Present as antigen to immune system */
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        anomaly->pattern_signature,
        anomaly->signature_len,
        severity,
        0,  /* source_node */
        &anomaly->antigen_id
    );

    if (result == 0) {
        anomaly->immune_alerted = true;
        bridge->immune_alerts_triggered++;

        LOG_MODULE_INFO("pattern_immune_bridge",
                  "Pattern anomaly %u presented as antigen %u (severity %u)",
                  anomaly->anomaly_id, anomaly->antigen_id, severity);
    }

    return result;
}

int pattern_immune_create_signature(
    pattern_anomaly_type_t anomaly_type,
    const float* pattern_features,
    uint32_t num_features,
    uint8_t* signature,
    size_t* signature_len
) {
    if (!pattern_features || !signature || !signature_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_create_signature: required parameter is NULL (pattern_features, signature, signature_len)");
        return -1;
    }

    /* Include anomaly type in signature */
    signature[0] = (uint8_t)anomaly_type;

    /* Hash features into remaining bytes */
    hash_pattern_to_signature(pattern_features, num_features,
                              signature + 1, BRAIN_IMMUNE_EPITOPE_SIZE - 1);

    *signature_len = BRAIN_IMMUNE_EPITOPE_SIZE;
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int pattern_immune_bridge_update(
    pattern_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bridge->total_updates++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    /* Apply inflammation effects to patterns */
    if (bridge->enable_inflammation_degradation) {
        pattern_immune_apply_inflammation_effects(bridge);
    }

    /* Present any un-alerted anomalies to immune system */
    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    for (size_t i = 0; i < bridge->anomaly_count; i++) {
        if (!bridge->anomalies[i].immune_alerted) {
            pattern_immune_present_anomaly(bridge, &bridge->anomalies[i]);
        }
    }
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int pattern_immune_get_inflammation_effects(
    const pattern_immune_bridge_t* bridge,
    inflammation_pattern_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_get_inflammation_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->inflammation_effects, sizeof(inflammation_pattern_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int pattern_immune_get_pathological_oscillation_state(
    const pattern_immune_bridge_t* bridge,
    pathological_oscillation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_get_pathological_oscillation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->pathological_oscillation, sizeof(pathological_oscillation_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int pattern_immune_get_pathological_synchrony_state(
    const pattern_immune_bridge_t* bridge,
    pathological_synchrony_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_get_pathological_synchrony_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->pathological_synchrony, sizeof(pathological_synchrony_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int pattern_immune_get_pathological_sequence_state(
    const pattern_immune_bridge_t* bridge,
    pathological_sequence_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_get_pathological_sequence_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->pathological_sequence, sizeof(pathological_sequence_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int pattern_immune_get_anomalies(
    const pattern_immune_bridge_t* bridge,
    pattern_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* num_anomalies
) {
    if (!bridge || !anomalies || !num_anomalies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_immune_get_anomalies: required parameter is NULL (bridge, anomalies, num_anomalies)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    uint32_t count = (bridge->anomaly_count < max_anomalies) ?
                     bridge->anomaly_count : max_anomalies;

    memcpy(anomalies, bridge->anomalies, sizeof(pattern_anomaly_t) * count);
    *num_anomalies = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool pattern_immune_is_degraded(const pattern_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Check if any accuracy factor is below 1.0 */
    return (bridge->inflammation_effects.oscillation_accuracy_factor < 1.0f ||
            bridge->inflammation_effects.synchrony_accuracy_factor < 1.0f ||
            bridge->inflammation_effects.sequence_accuracy_factor < 1.0f ||
            bridge->inflammation_effects.pattern_match_accuracy_factor < 1.0f);
}

const char* pattern_anomaly_type_to_string(pattern_anomaly_type_t type) {
    switch (type) {
        case PATTERN_ANOMALY_NONE: return "None";
        case PATTERN_ANOMALY_SEIZURE_OSCILLATION: return "SeizureOscillation";
        case PATTERN_ANOMALY_HYPERSYNCHRONY: return "Hypersynchrony";
        case PATTERN_ANOMALY_DESYNCHRONIZATION: return "Desynchronization";
        case PATTERN_ANOMALY_SEQUENCE_FAILURE: return "SequenceFailure";
        case PATTERN_ANOMALY_REPETITIVE_SEQUENCE: return "RepetitiveSequence";
        case PATTERN_ANOMALY_DELTA_INTRUSION: return "DeltaIntrusion";
        case PATTERN_ANOMALY_GAMMA_DISRUPTION: return "GammaDisruption";
        case PATTERN_ANOMALY_THETA_GAMMA_UNCOUPLING: return "ThetaGammaUncoupling";
        default: return "Unknown";
    }
}
