/**
 * @file nimcp_security_hippocampus_fep_bridge.c
 * @brief Implementation of Security Hippocampus FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hippocampus memory security
 * WHY:  Memory integrity violations are high-surprise events in FEP framework
 * HOW:  Map memory security metrics to free energy, use prediction errors
 */

#include "security/hippocampus/nimcp_security_hippocampus_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_hippocampus_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_free_energy_from_integrity(float integrity_score,
                                                float consolidation_rate,
                                                float replay_fidelity,
                                                const sec_hippo_fep_config_t* config);

static float compute_surprise_from_anomaly(float integrity_deviation,
                                           float expected_integrity);

static sec_hippo_fep_threat_level_t classify_threat_level(float free_energy,
                                                          const sec_hippo_fep_config_t* config);

static sec_hippo_fep_response_t determine_response(sec_hippo_fep_threat_level_t threat,
                                                   float urgency);

static void update_running_averages(sec_hippo_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for security hippocampus FEP bridge
 * WHY:  Provide sensible starting point for most deployments
 * HOW:  Set biologically-plausible defaults for all parameters
 */
int sec_hippo_fep_default_config(sec_hippo_fep_config_t* config) {
    if (!config) {
        return -1;
    }

    /* FEP parameters */
    config->free_energy_threshold = SEC_HIPPO_FEP_ATTACK_THRESHOLD;
    config->surprise_threshold = SEC_HIPPO_FEP_SURPRISE_ANOMALY;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_detection = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SEC_HIPPO_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SEC_HIPPO_FEP_CRITICAL_THRESHOLD;

    /* Memory integrity mapping */
    config->integrity_to_fe_scale = 10.0f;  /* Low integrity maps to high FE */
    config->consolidation_pe_weight = 0.3f;
    config->replay_surprise_weight = 0.4f;

    /* Active inference settings */
    config->enable_active_inference = true;
    config->response_threshold = SEC_HIPPO_FEP_SUSPICIOUS_THRESHOLD;
    config->action_temperature = 1.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create security hippocampus FEP bridge
 * WHY:  Initialize FEP integration for memory security detection
 * HOW:  Allocate structure, initialize base, apply configuration
 */
sec_hippo_fep_bridge_t* sec_hippo_fep_create(
    const sec_hippo_fep_config_t* config,
    sec_hippo_bridge_t* security_hippo,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!security_hippo || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_hippo_fep_create: security_hippo or fep_system is NULL");
        NIMCP_LOGGING_ERROR("Security Hippocampus FEP bridge: NULL system pointers");
        return NULL;
    }

    /* Allocate bridge structure */
    sec_hippo_fep_bridge_t* bridge = (sec_hippo_fep_bridge_t*)nimcp_malloc(
        sizeof(sec_hippo_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_hippo_fep_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Security Hippocampus FEP bridge: allocation failed");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(sec_hippo_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_hippo_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->security_hippo = security_hippo;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "security_hippocampus_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "sec_hippo_fep_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Security Hippocampus FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SEC_HIPPO_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_HIPPO_FEP_THREAT_NONE;

    /* Initialize effects */
    bridge->fep_effects.threat_level = SEC_HIPPO_FEP_THREAT_NONE;
    bridge->fep_effects.detection_sensitivity = SEC_HIPPO_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.integrity_estimate = 1.0f;  /* Assume healthy */

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_SECURITY_HIPPOCAMPUS_FEP;
    bridge->base.module_name = "sec_hippo_fep_bridge";

    NIMCP_LOGGING_INFO("Security Hippocampus FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy security hippocampus FEP bridge
 * WHY:  Clean up all resources to prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void sec_hippo_fep_destroy(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_hippo_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free bridge memory */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Security Hippocampus FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections and config
 */
int sec_hippo_fep_reset(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = SEC_HIPPO_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_HIPPO_FEP_THREAT_NONE;
    bridge->state.last_threat_time = 0;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_security_effects_t));
    bridge->fep_effects.integrity_estimate = 1.0f;
    bridge->fep_effects.detection_sensitivity = SEC_HIPPO_FEP_DEFAULT_PRECISION;

    memset(&bridge->security_effects, 0, sizeof(security_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_hippo_fep_stats_t));
    bridge->stats.current_precision = SEC_HIPPO_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Security Hippocampus FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge configuration
 * WHY:  Allow inspection of current settings
 * HOW:  Copy configuration to output structure
 */
int sec_hippo_fep_get_config(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    *config = bridge->config;
    return 0;
}

/**
 * WHAT: Set bridge configuration
 * WHY:  Allow runtime tuning of detection parameters
 * HOW:  Validate and copy new configuration
 */
int sec_hippo_fep_set_config(
    sec_hippo_fep_bridge_t* bridge,
    const sec_hippo_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Validate critical parameters */
    if (config->free_energy_threshold <= 0.0f ||
        config->surprise_threshold <= 0.0f) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    bridge->config = *config;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Compute FEP effects on security
 * WHY:  Derive threat detection metrics from FEP state
 * HOW:  Query FEP system for free energy, surprise, prediction error
 */
int sec_hippo_fep_compute_effects(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.prediction_error = pred_error;

    /* Classify threat level based on free energy */
    bridge->fep_effects.threat_level = classify_threat_level(current_fe, &bridge->config);

    /* Compute threat confidence based on precision and stability */
    float confidence = 1.0f - (pred_error / 10.0f);
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;
    bridge->fep_effects.threat_confidence = confidence * bridge->state.current_precision;

    /* Compute detection sensitivity from precision */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    /* Estimate memory integrity from FEP (inverted relationship) */
    float integrity_estimate = 1.0f - (current_fe / bridge->config.integrity_to_fe_scale);
    if (integrity_estimate < 0.0f) integrity_estimate = 0.0f;
    if (integrity_estimate > 1.0f) integrity_estimate = 1.0f;
    bridge->fep_effects.integrity_estimate = integrity_estimate;

    /* Determine recommended response */
    float urgency = current_fe / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.threat_level, urgency
    );

    /* Update statistics */
    bridge->stats.avg_free_energy = bridge->state.avg_surprise;  /* Rolling avg */
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP from security detection results
 * WHY:  Feed security observations back to update generative model
 * HOW:  Convert detection to FEP observation, update beliefs and precision
 */
int sec_hippo_fep_update_from_detection(
    sec_hippo_fep_bridge_t* bridge,
    bool threat_detected,
    float integrity_score,
    sec_hippo_consolidation_status_t consolidation_status
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update security effects */
    if (threat_detected) {
        bridge->security_effects.attacks_detected++;
        bridge->security_effects.under_attack = true;
        bridge->stats.threats_detected++;
    } else {
        bridge->security_effects.normal_operations++;
        bridge->security_effects.under_attack = false;
    }

    /* Update average integrity score (exponential moving average) */
    bridge->security_effects.avg_integrity_score =
        0.9f * bridge->security_effects.avg_integrity_score +
        0.1f * integrity_score;

    /* Map consolidation status to health score */
    float consol_health = 1.0f;
    switch (consolidation_status) {
        case SEC_HIPPO_CONSOL_OK:
            consol_health = 1.0f;
            break;
        case SEC_HIPPO_CONSOL_DEGRADED:
            consol_health = 0.7f;
            break;
        case SEC_HIPPO_CONSOL_CORRUPTED:
            consol_health = 0.3f;
            break;
        case SEC_HIPPO_CONSOL_TAMPERED:
            consol_health = 0.1f;
            break;
        case SEC_HIPPO_CONSOL_INCOMPLETE:
            consol_health = 0.5f;
            break;
        default:
            consol_health = 0.5f;
    }
    bridge->security_effects.consolidation_health =
        0.9f * bridge->security_effects.consolidation_health +
        0.1f * consol_health;

    /* Compute current threat level */
    bridge->security_effects.current_threat_level =
        1.0f - bridge->security_effects.avg_integrity_score;

    /* Update FEP system if online learning enabled */
    if (bridge->config.enable_online_learning) {
        if (threat_detected) {
            /*
             * Threat detected = high-surprise observation
             * Increase precision for better detection
             */
            fep_update_precision(bridge->fep_system);

            /* Create observation vector representing threat */
            float observation[16];
            for (int i = 0; i < 16; i++) {
                observation[i] = (1.0f - integrity_score) * (1.0f + (float)i * 0.1f);
            }
            fep_process_observation(bridge->fep_system, observation, 16);
        } else {
            /*
             * Normal operation = update generative model
             * This refines expectations for normal memory behavior
             */
            fep_update_beliefs(bridge->fep_system);
        }
    }

    bridge->state.detection_count++;

    /* Track last threat */
    if (threat_detected) {
        bridge->state.last_threat = bridge->fep_effects.threat_level;
        bridge->state.last_threat_time = nimcp_platform_time_monotonic_ms();
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional effects between security and FEP
 * HOW:  Compute effects, apply precision modulation, update state
 */
int sec_hippo_fep_update(sec_hippo_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        return -1;
    }

    /* Suppress unused parameter warning */
    (void)delta_ms;

    /* Compute FEP effects on security (handles its own locking internally) */
    int result = sec_hippo_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Apply precision modulation if enabled */
    if (bridge->config.enable_precision_modulation) {
        /*
         * Adapt precision based on detection performance
         * High threat rate -> increase precision (more sensitive)
         * Low threat rate -> decrease precision (less false positives)
         */
        float threat_rate = (float)bridge->security_effects.attacks_detected /
                           (float)(bridge->state.detection_count + 1);

        float target_precision = SEC_HIPPO_FEP_DEFAULT_PRECISION;
        if (threat_rate > 0.2f) {
            target_precision = SEC_HIPPO_FEP_MAX_PRECISION;
        } else if (threat_rate < 0.05f) {
            target_precision = SEC_HIPPO_FEP_MIN_PRECISION + 0.5f;
        }

        /* Smooth adaptation */
        float alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - alpha) * bridge->state.current_precision +
            alpha * target_precision;

        /* Clamp precision */
        if (bridge->state.current_precision < SEC_HIPPO_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_HIPPO_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > SEC_HIPPO_FEP_MAX_PRECISION) {
            bridge->state.current_precision = SEC_HIPPO_FEP_MAX_PRECISION;
        }

        bridge->stats.precision_adaptations++;
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

/**
 * WHAT: Detect threat using FEP analysis
 * WHY:  Combine security metrics with FEP for enhanced detection
 * HOW:  Compute free energy from security metrics, classify threat
 */
int sec_hippo_fep_detect_threat(
    sec_hippo_fep_bridge_t* bridge,
    float integrity_score,
    float consolidation_rate,
    float replay_fidelity,
    sec_hippo_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute free energy from security metrics */
    float free_energy = compute_free_energy_from_integrity(
        integrity_score, consolidation_rate, replay_fidelity, &bridge->config
    );

    /* Compute surprise from integrity deviation */
    float expected_integrity = bridge->security_effects.avg_integrity_score;
    if (expected_integrity < 0.5f) expected_integrity = 0.5f;
    float surprise = compute_surprise_from_anomaly(
        fabsf(integrity_score - expected_integrity), expected_integrity
    );

    /* Classify threat level */
    sec_hippo_fep_threat_level_t threat = classify_threat_level(
        free_energy, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision;
    /* Reduce confidence if prediction error is high */
    float pe = bridge->fep_effects.prediction_error;
    if (pe > SEC_HIPPO_FEP_PE_TOLERANCE) {
        confidence *= (1.0f - (pe - SEC_HIPPO_FEP_PE_TOLERANCE));
        if (confidence < 0.1f) confidence = 0.1f;
    }

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update FEP effects */
    bridge->fep_effects.free_energy = free_energy;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.threat_level = threat;
    bridge->fep_effects.threat_confidence = confidence;

    /* Track statistics */
    bridge->stats.fep_detections++;
    if (threat >= SEC_HIPPO_FEP_THREAT_MODERATE) {
        bridge->stats.threats_detected++;
        bridge->stats.true_positive_count++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get recommended protective response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate threat level and urgency to select optimal response
 */
int sec_hippo_fep_get_response(
    sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_response_t* response_out,
    float* urgency_out
) {
    if (!bridge || !response_out || !urgency_out) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current threat metrics */
    float free_energy = bridge->fep_effects.free_energy;
    sec_hippo_fep_threat_level_t threat = bridge->fep_effects.threat_level;

    /* Compute urgency */
    float urgency = free_energy / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;

    /* Determine response */
    sec_hippo_fep_response_t response = determine_response(threat, urgency);

    *response_out = response;
    *urgency_out = urgency;

    /* Track if response is taken */
    if (response != SEC_HIPPO_FEP_RESPONSE_NONE) {
        bridge->stats.protective_responses++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report false positive detection
 * WHY:  Reduce precision to prevent similar false positives
 * HOW:  Decrease precision, update statistics
 */
int sec_hippo_fep_report_false_positive(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->security_effects.false_positives++;
    bridge->stats.false_positive_count++;

    /* Reduce precision if learning from FPs enabled */
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < SEC_HIPPO_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_HIPPO_FEP_MIN_PRECISION;
        }
    }

    /* Update detection accuracy */
    uint64_t total_positives = bridge->stats.true_positive_count +
                               bridge->stats.false_positive_count;
    if (total_positives > 0) {
        bridge->stats.detection_accuracy =
            (float)bridge->stats.true_positive_count / (float)total_positives;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

/**
 * WHAT: Get FEP effects on security
 * WHY:  Allow inspection of current FEP-derived effects
 * HOW:  Copy effects structure
 */
int sec_hippo_fep_get_fep_effects(
    const sec_hippo_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/**
 * WHAT: Get security effects on FEP
 * WHY:  Allow inspection of security-derived effects
 * HOW:  Copy effects structure
 */
int sec_hippo_fep_get_security_effects(
    const sec_hippo_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->security_effects;
    return 0;
}

/**
 * WHAT: Get current bridge state
 * WHY:  Allow inspection of operational state
 * HOW:  Copy state structure
 */
int sec_hippo_fep_get_state(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_state_t* state
) {
    if (!bridge || !state) {
        return -1;
    }

    *state = bridge->state;
    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring and tuning
 * HOW:  Copy statistics structure
 */
int sec_hippo_fep_get_stats(
    const sec_hippo_fep_bridge_t* bridge,
    sec_hippo_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/**
 * WHAT: Get current free energy
 * WHY:  Quick access to key metric
 * HOW:  Return from effects
 */
float sec_hippo_fep_get_free_energy(const sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.free_energy;
}

/**
 * WHAT: Get current surprise level
 * WHY:  Quick access to key metric
 * HOW:  Return from effects
 */
float sec_hippo_fep_get_surprise(const sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.surprise_level;
}

/**
 * WHAT: Get current threat level
 * WHY:  Quick access to threat classification
 * HOW:  Return from effects
 */
sec_hippo_fep_threat_level_t sec_hippo_fep_get_threat_level(
    const sec_hippo_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_HIPPO_FEP_THREAT_NONE;
    }
    return bridge->fep_effects.threat_level;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module with router, setup inbox
 */
int sec_hippo_fep_connect_bio_async(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_HIPPOCAMPUS_FEP,
        .module_name = "sec_hippo_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security Hippocampus FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of inter-module communication
 * HOW:  Unregister module from router
 */
int sec_hippo_fep_disconnect_bio_async(sec_hippo_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security Hippocampus FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Allow callers to verify connection status
 * HOW:  Return connection flag
 */
bool sec_hippo_fep_is_bio_async_connected(const sec_hippo_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

/**
 * WHAT: Get human-readable name for threat level
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* sec_hippo_fep_threat_level_name(sec_hippo_fep_threat_level_t level) {
    switch (level) {
        case SEC_HIPPO_FEP_THREAT_NONE:
            return "None";
        case SEC_HIPPO_FEP_THREAT_SUSPICIOUS:
            return "Suspicious";
        case SEC_HIPPO_FEP_THREAT_MODERATE:
            return "Moderate";
        case SEC_HIPPO_FEP_THREAT_HIGH:
            return "High";
        case SEC_HIPPO_FEP_THREAT_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Get human-readable name for response type
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* sec_hippo_fep_response_name(sec_hippo_fep_response_t response) {
    switch (response) {
        case SEC_HIPPO_FEP_RESPONSE_NONE:
            return "None";
        case SEC_HIPPO_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case SEC_HIPPO_FEP_RESPONSE_PROTECT:
            return "Protect";
        case SEC_HIPPO_FEP_RESPONSE_ISOLATE:
            return "Isolate";
        case SEC_HIPPO_FEP_RESPONSE_RESTORE:
            return "Restore";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Print bridge summary
 * WHY:  Debug and monitoring support
 * HOW:  Format and print key metrics
 */
void sec_hippo_fep_print_summary(const sec_hippo_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Hippocampus FEP Bridge: NULL\n");
        return;
    }

    printf("=== Security Hippocampus FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Surprise: %.3f\n", bridge->fep_effects.surprise_level);
    printf("  Prediction Error: %.3f\n", bridge->fep_effects.prediction_error);
    printf("  Threat Level: %s\n",
           sec_hippo_fep_threat_level_name(bridge->fep_effects.threat_level));
    printf("  Threat Confidence: %.3f\n", bridge->fep_effects.threat_confidence);
    printf("  Integrity Estimate: %.3f\n", bridge->fep_effects.integrity_estimate);
    printf("  Recommended Response: %s\n",
           sec_hippo_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Security Effects:\n");
    printf("  Attacks Detected: %lu\n",
           (unsigned long)bridge->security_effects.attacks_detected);
    printf("  Normal Operations: %lu\n",
           (unsigned long)bridge->security_effects.normal_operations);
    printf("  Under Attack: %s\n",
           bridge->security_effects.under_attack ? "yes" : "no");
    printf("  Avg Integrity: %.3f\n", bridge->security_effects.avg_integrity_score);
    printf("==============================================\n");
}

/**
 * WHAT: Print statistics
 * WHY:  Performance monitoring support
 * HOW:  Format and print statistics
 */
void sec_hippo_fep_print_stats(const sec_hippo_fep_stats_t* stats) {
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("=== Security Hippocampus FEP Statistics ===\n");
    printf("Total Updates: %lu\n", (unsigned long)stats->total_updates);
    printf("FEP Detections: %lu\n", (unsigned long)stats->fep_detections);
    printf("Threats Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("Protective Responses: %lu\n", (unsigned long)stats->protective_responses);
    printf("Precision Adaptations: %lu\n", (unsigned long)stats->precision_adaptations);
    printf("\n");
    printf("Averages:\n");
    printf("  Free Energy: %.3f\n", stats->avg_free_energy);
    printf("  Surprise: %.3f\n", stats->avg_surprise);
    printf("  Prediction Error: %.3f\n", stats->avg_prediction_error);
    printf("  Precision: %.3f\n", stats->current_precision);
    printf("\n");
    printf("Maximums:\n");
    printf("  Free Energy: %.3f\n", stats->max_free_energy);
    printf("  Surprise: %.3f\n", stats->max_surprise);
    printf("\n");
    printf("Detection Performance:\n");
    printf("  True Positives: %lu\n", (unsigned long)stats->true_positive_count);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positive_count);
    printf("  Accuracy: %.3f\n", stats->detection_accuracy);
    printf("==========================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from memory integrity metrics
 * WHY:  Map security domain to FEP domain
 * HOW:  Weighted combination of integrity, consolidation, replay
 *
 * Low integrity = high free energy (inverted relationship)
 * Poor consolidation = prediction error contribution
 * Invalid replay = surprise contribution
 */
static float compute_free_energy_from_integrity(
    float integrity_score,
    float consolidation_rate,
    float replay_fidelity,
    const sec_hippo_fep_config_t* config
) {
    /*
     * Free energy increases as integrity decreases
     * F = scale * (1 - integrity) + PE_weight * consol_error + surprise_weight * replay_error
     */
    float integrity_fe = config->integrity_to_fe_scale * (1.0f - integrity_score);

    /* Consolidation rate deviation from expected (1.0 is normal) */
    float consol_error = fabsf(consolidation_rate - 1.0f);
    float consol_fe = config->consolidation_pe_weight * consol_error * 10.0f;

    /* Replay fidelity deviation (1.0 is perfect fidelity) */
    float replay_error = 1.0f - replay_fidelity;
    float replay_fe = config->replay_surprise_weight * replay_error * 10.0f;

    return integrity_fe + consol_fe + replay_fe;
}

/**
 * WHAT: Compute surprise from anomaly magnitude
 * WHY:  Surprise = -log(probability of observation)
 * HOW:  Approximate using deviation from expected
 */
static float compute_surprise_from_anomaly(
    float integrity_deviation,
    float expected_integrity
) {
    /*
     * Surprise proportional to deviation from expectation
     * Higher deviation = less probable = more surprising
     */
    float normalized_dev = integrity_deviation / (expected_integrity + 0.01f);

    /* Apply log-like transformation for surprise */
    float surprise = -logf(1.0f - normalized_dev + 0.01f);

    if (surprise < 0.0f) surprise = 0.0f;
    if (surprise > 20.0f) surprise = 20.0f;

    return surprise;
}

/**
 * WHAT: Classify threat level from free energy
 * WHY:  Map continuous FE to discrete threat categories
 * HOW:  Threshold-based classification
 */
static sec_hippo_fep_threat_level_t classify_threat_level(
    float free_energy,
    const sec_hippo_fep_config_t* config
) {
    if (free_energy >= config->critical_fe_threshold) {
        return SEC_HIPPO_FEP_THREAT_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return SEC_HIPPO_FEP_THREAT_HIGH;
    } else if (free_energy >= SEC_HIPPO_FEP_SUSPICIOUS_THRESHOLD) {
        return SEC_HIPPO_FEP_THREAT_MODERATE;
    } else if (free_energy >= config->normal_fe_threshold) {
        return SEC_HIPPO_FEP_THREAT_SUSPICIOUS;
    } else {
        return SEC_HIPPO_FEP_THREAT_NONE;
    }
}

/**
 * WHAT: Determine appropriate response based on threat and urgency
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map threat level and urgency to response type
 */
static sec_hippo_fep_response_t determine_response(
    sec_hippo_fep_threat_level_t threat,
    float urgency
) {
    switch (threat) {
        case SEC_HIPPO_FEP_THREAT_CRITICAL:
            if (urgency > 0.8f) {
                return SEC_HIPPO_FEP_RESPONSE_RESTORE;
            } else {
                return SEC_HIPPO_FEP_RESPONSE_ISOLATE;
            }

        case SEC_HIPPO_FEP_THREAT_HIGH:
            return SEC_HIPPO_FEP_RESPONSE_ISOLATE;

        case SEC_HIPPO_FEP_THREAT_MODERATE:
            return SEC_HIPPO_FEP_RESPONSE_PROTECT;

        case SEC_HIPPO_FEP_THREAT_SUSPICIOUS:
            return SEC_HIPPO_FEP_RESPONSE_MONITOR;

        case SEC_HIPPO_FEP_THREAT_NONE:
        default:
            return SEC_HIPPO_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    sec_hippo_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;  /* EMA smoothing factor */

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;

    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;

    /* Also track in stats */
    bridge->stats.avg_free_energy =
        (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}
