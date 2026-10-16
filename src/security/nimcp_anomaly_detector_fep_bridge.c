/**
 * @file nimcp_anomaly_detector_fep_bridge.c
 * @brief Implementation of anomaly detector-FEP bridge
 */

#include "security/nimcp_anomaly_detector_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(anomaly_detector_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int anomaly_fep_default_config(anomaly_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config in anomaly_fep_default_config");
        return -1;
    }

    config->anomaly_fe_threshold = ANOMALY_FEP_ANOMALOUS_THRESHOLD;
    config->surprise_threshold = 10.0f;
    config->precision_learning_rate = 0.05f;

    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = ANOMALY_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = ANOMALY_FEP_CRITICAL_THRESHOLD;

    config->enable_online_learning = true;
    config->learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config->learn_from_false_positives = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

anomaly_fep_bridge_t* anomaly_fep_create(
    const anomaly_fep_config_t* config,
    nimcp_anomaly_detector_t detector,
    fep_system_t* fep_system
) {
    if (!detector || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_fep_create: detector or fep_system is NULL");
        return NULL;
    }

    anomaly_fep_bridge_t* bridge = (anomaly_fep_bridge_t*)nimcp_malloc(sizeof(anomaly_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anomaly_fep_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(anomaly_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        anomaly_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    bridge->detector = detector;

    if (bridge_base_init(&bridge->base, 0, "anomaly_detector_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "anomaly_fep_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_fep_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = ANOMALY_FEP_DEFAULT_PRECISION;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Anomaly FEP bridge created");
    return bridge;
}

void anomaly_fep_destroy(anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        anomaly_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Anomaly FEP bridge destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int anomaly_fep_update(anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    /* Query FEP state outside lock to avoid deadlock with FEP mutex */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Update running averages
    bridge->state.avg_surprise = 0.9f * bridge->state.avg_surprise + 0.1f * surprise;
    if (!isfinite(bridge->state.avg_surprise)) bridge->state.avg_surprise = surprise;

    // Compute FEP-based anomaly score
    bridge->fep_effects.fep_anomaly_score = current_fe / bridge->config.anomaly_fe_threshold;
    if (bridge->fep_effects.fep_anomaly_score > 1.0f) {
        bridge->fep_effects.fep_anomaly_score = 1.0f;
    }

    // Compute surprise score
    bridge->fep_effects.surprise_score = surprise / bridge->config.surprise_threshold;
    if (bridge->fep_effects.surprise_score > 1.0f) {
        bridge->fep_effects.surprise_score = 1.0f;
    }

    // Precision-based detection sensitivity
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    // Confidence based on prediction stability
    bridge->fep_effects.confidence = 1.0f - (pred_error / 10.0f);
    if (bridge->fep_effects.confidence < 0.0f) {
        bridge->fep_effects.confidence = 0.0f;
    }

    bridge->state.update_count++;
    bridge->stats.avg_free_energy = current_fe;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_fep_detect(
    anomaly_fep_bridge_t* bridge,
    const void* input,
    size_t input_len,
    nimcp_anomaly_result_t* result
) {
    if (!bridge || !input || !result) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Run standard Bayesian detection
    nimcp_error_t err = nimcp_anomaly_detect(bridge->detector, input, input_len, result);
    if (err != NIMCP_SUCCESS) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "anomaly_fep_detect: validation failed");
        return -1;
    }

    // If FEP scoring enabled, fuse with FEP-based detection
    if (bridge->config.use_fep_scoring) {
        // Process input through FEP
        // Note: Real implementation would extract features from input
        // For now, use dummy observation
        float observation[16] = {0};
        for (size_t i = 0; i < input_len && i < 16; i++) {
            observation[i] = ((const uint8_t*)input)[i] / 255.0f;
        }

        fep_process_observation(bridge->fep_system, observation, 16);

        // Compute free energy
        fep_free_energy_t fe;
        fep_compute_free_energy(bridge->fep_system, &fe);

        float fep_score = fe.total / bridge->config.anomaly_fe_threshold;
        if (fep_score > 1.0f) {
            fep_score = 1.0f;
        }

        // Fuse scores (weighted average)
        float fused_score = 0.6f * result->anomaly_score + 0.4f * fep_score;
        result->anomaly_score = fused_score;

        // Update confidence
        result->confidence = bridge->fep_effects.confidence;

        // Categorize based on fused score
        if (fused_score >= 0.8f) {
            snprintf(result->explanation, sizeof(result->explanation),
                    "Critical anomaly (FEP score: %.2f, FE: %.2f)", fep_score, fe.total);
        } else if (fused_score >= 0.6f) {
            snprintf(result->explanation, sizeof(result->explanation),
                    "Anomaly detected (FEP score: %.2f, FE: %.2f)", fep_score, fe.total);
        }
    }

    bridge->state.detection_count++;
    bridge->stats.total_detections++;

    if (result->anomaly_score >= 0.6f) {
        bridge->stats.anomalies_found++;
        bridge->stats.fep_based_detections++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_fep_apply_modulation(anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Adapt precision based on detection performance
    float anomaly_rate = (float)bridge->anomaly_effects.anomalies_detected /
                        (float)(bridge->state.detection_count + 1);

    float target_precision = ANOMALY_FEP_DEFAULT_PRECISION;
    if (anomaly_rate > 0.2f) {
        // High anomaly rate → increase precision
        target_precision = ANOMALY_FEP_MAX_PRECISION;
    } else if (anomaly_rate < 0.05f) {
        // Low anomaly rate → decrease precision
        target_precision = ANOMALY_FEP_MIN_PRECISION;
    }

    // Smooth adaptation
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_fep_report_detection(
    anomaly_fep_bridge_t* bridge,
    bool is_anomaly,
    float confidence
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Update effects
    if (is_anomaly) {
        bridge->anomaly_effects.anomalies_detected++;
    } else {
        bridge->anomaly_effects.normal_samples++;
    }

    // Update average anomaly score
    float score = is_anomaly ? confidence : 0.0f;
    bridge->anomaly_effects.avg_anomaly_score =
        0.9f * bridge->anomaly_effects.avg_anomaly_score + 0.1f * score;

    // Update FEP if online learning enabled
    if (bridge->config.enable_online_learning) {
        // For anomalies, this is a high-surprise observation
        // For normal samples, this updates the generative model
        if (is_anomaly) {
            // Increase precision for anomalies (high confidence in detection)
            fep_update_precision(bridge->fep_system);
        } else {
            // Update beliefs for normal samples
            fep_update_beliefs(bridge->fep_system);
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_fep_report_false_positive(anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->anomaly_effects.false_positives++;

    // Reduce precision to prevent similar false positives
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < ANOMALY_FEP_MIN_PRECISION) {
            bridge->state.current_precision = ANOMALY_FEP_MIN_PRECISION;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int anomaly_fep_get_effects(
    const anomaly_fep_bridge_t* bridge,
    anomaly_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int anomaly_fep_get_anomaly_effects(
    const anomaly_fep_bridge_t* bridge,
    fep_anomaly_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->anomaly_effects;
    return 0;
}

int anomaly_fep_get_stats(
    const anomaly_fep_bridge_t* bridge,
    anomaly_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float anomaly_fep_get_anomaly_score(const anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.fep_anomaly_score;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int anomaly_fep_connect_bio_async(anomaly_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_ANOMALY_FEP,
        .module_name = "anomaly_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Anomaly FEP bridge connected to bio-async");
    }

    return 0;
}

int anomaly_fep_disconnect_bio_async(anomaly_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Anomaly FEP bridge disconnected from bio-async");
    return 0;
}

bool anomaly_fep_is_bio_async_connected(const anomaly_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
