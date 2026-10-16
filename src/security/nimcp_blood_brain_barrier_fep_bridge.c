/**
 * @file nimcp_blood_brain_barrier_fep_bridge.c
 * @brief Implementation of BBB-FEP bridge
 */

#include "security/nimcp_blood_brain_barrier_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(blood_brain_barrier_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int bbb_fep_default_config(bbb_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config in bbb_fep_default_config");
        return -1;
    }

    config->threat_free_energy_threshold = 5.0f;
    config->precision_learning_rate = 0.05f;
    config->enable_precision_modulation = true;

    config->input_feature_dim = 16;
    config->extract_timing_features = true;
    config->extract_content_features = true;

    config->allow_fe_threshold = 2.0f;
    config->block_fe_threshold = 5.0f;
    config->quarantine_fe_threshold = 8.0f;

    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

bbb_fep_bridge_t* bbb_fep_create(
    const bbb_fep_config_t* config,
    bbb_system_t bbb_system,
    fep_system_t* fep_system
) {
    if (!bbb_system || !fep_system) {
        NIMCP_LOGGING_ERROR("BBB FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system pointers in bbb_fep_create");
        return NULL;
    }

    bbb_fep_bridge_t* bridge = (bbb_fep_bridge_t*)nimcp_malloc(sizeof(bbb_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("BBB FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate BBB FEP bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(bbb_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        bbb_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    bridge->bbb_system = bbb_system;

    if (bridge_base_init(&bridge->base, 0, "blood_brain_barrier_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("BBB FEP bridge: mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "Failed to create BBB FEP bridge mutex");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = BBB_FEP_DEFAULT_PRECISION;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("BBB FEP bridge created");
    return bridge;
}

void bbb_fep_destroy(bbb_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        bbb_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("BBB FEP bridge destroyed");
}

/* ============================================================================
 * Feature Extraction (Internal)
 * ============================================================================ */

/**
 * @brief Extract features from input data
 *
 * WHAT: Convert raw input to FEP observation vector
 * WHY:  FEP operates on feature vectors
 * HOW:  Compute length, entropy, character statistics
 */
static void extract_input_features(
    const void* data,
    size_t size,
    float* features,
    uint32_t feature_dim
) {
    if (!data || !features || feature_dim == 0) {
        return;
    }

    memset(features, 0, feature_dim * sizeof(float));

    const uint8_t* bytes = (const uint8_t*)data;

    // Feature 0: Normalized length
    features[0] = (float)size / 1024.0f;

    // Feature 1: Byte entropy
    uint32_t byte_counts[256] = {0};
    for (size_t i = 0; i < size; i++) {
        byte_counts[bytes[i]]++;
    }

    float entropy = 0.0f;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) {
            float p = (float)byte_counts[i] / (float)size;
            entropy -= p * log2f(p);
        }
    }
    features[1] = entropy / 8.0f;  // Normalize to [0, 1]

    // Feature 2-5: Character class ratios
    uint32_t alphanumeric = 0, special = 0, control = 0, whitespace = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t c = bytes[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            alphanumeric++;
        } else if (c < 32 || c == 127) {
            control++;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            whitespace++;
        } else {
            special++;
        }
    }

    if (size > 0) {
        features[2] = (float)alphanumeric / (float)size;
        features[3] = (float)special / (float)size;
        features[4] = (float)control / (float)size;
        features[5] = (float)whitespace / (float)size;
    }

    // Remaining features: Pattern-based (simplified)
    for (uint32_t i = 6; i < feature_dim && i < 16; i++) {
        features[i] = 0.0f;
    }
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int bbb_fep_update(bbb_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Get current FEP state
    float current_fe = fep_get_free_energy(bridge->fep_system);

    // Update running average
    bridge->state.avg_free_energy =
        0.9f * bridge->state.avg_free_energy + 0.1f * current_fe;

    // Compute threat score from free energy
    bridge->fep_effects.threat_score = current_fe / bridge->config.threat_free_energy_threshold;
    if (bridge->fep_effects.threat_score > 1.0f) {
        bridge->fep_effects.threat_score = 1.0f;
    }

    // Compute anomaly score from prediction error
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.anomaly_score = pred_error / 10.0f;
    if (bridge->fep_effects.anomaly_score > 1.0f) {
        bridge->fep_effects.anomaly_score = 1.0f;
    }

    // Precision-based validation strictness
    bridge->fep_effects.validation_strictness = bridge->state.current_precision;

    // Recommend action based on free energy
    if (current_fe < bridge->config.allow_fe_threshold) {
        bridge->fep_effects.recommended_action = BBB_ACTION_ALLOW;
    } else if (current_fe < bridge->config.block_fe_threshold) {
        bridge->fep_effects.recommended_action = BBB_ACTION_BLOCK;
    } else if (current_fe < bridge->config.quarantine_fe_threshold) {
        bridge->fep_effects.recommended_action = BBB_ACTION_QUARANTINE;
    } else {
        bridge->fep_effects.recommended_action = BBB_ACTION_LOCKDOWN;
    }

    bridge->state.update_count++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bbb_fep_process_input(
    bbb_fep_bridge_t* bridge,
    const void* data,
    size_t size,
    bbb_validation_result_t* result
) {
    if (!bridge || !data || !result) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Extract features from input
    float features[BBB_FEP_MAX_FEATURES];
    extract_input_features(data, size, features, bridge->config.input_feature_dim);

    // Process through FEP
    fep_process_observation(bridge->fep_system, features, bridge->config.input_feature_dim);

    // Compute free energy
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    // Update effects
    bridge->fep_effects.threat_score = fe.total / bridge->config.threat_free_energy_threshold;
    if (bridge->fep_effects.threat_score > 1.0f) {
        bridge->fep_effects.threat_score = 1.0f;
    }

    // Map to validation result
    result->valid = (fe.total < bridge->config.block_fe_threshold);
    result->severity = BBB_SEVERITY_NONE;

    if (fe.total >= bridge->config.quarantine_fe_threshold) {
        result->severity = BBB_SEVERITY_CRITICAL;
        result->threat = BBB_THREAT_UNKNOWN;
    } else if (fe.total >= bridge->config.block_fe_threshold) {
        result->severity = BBB_SEVERITY_HIGH;
        result->threat = BBB_THREAT_DATA_TAMPERING;
    } else if (fe.total >= bridge->config.allow_fe_threshold) {
        result->severity = BBB_SEVERITY_MEDIUM;
    }

    if (!result->valid) {
        snprintf(result->reason, sizeof(result->reason),
                "High free energy: %.2f", fe.total);
        bridge->stats.threats_via_fep++;
    }

    bridge->state.observation_count++;
    bridge->stats.total_inputs_processed++;
    bridge->stats.avg_prediction_error = fe.inaccuracy;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bbb_fep_apply_modulation(bbb_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Adapt precision based on threat landscape
    float threat_rate = (float)bridge->bbb_effects.threats_detected /
                       (float)(bridge->state.observation_count + 1);

    float target_precision = BBB_FEP_DEFAULT_PRECISION;
    if (threat_rate > 0.1f) {
        // High threat rate → increase precision
        target_precision = BBB_FEP_MAX_PRECISION;
    } else if (threat_rate < 0.01f) {
        // Low threat rate → decrease precision
        target_precision = BBB_FEP_MIN_PRECISION;
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

int bbb_fep_report_threat(
    bbb_fep_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Update BBB effects
    bridge->bbb_effects.threats_detected++;

    float severity_weight = (float)severity / (float)BBB_SEVERITY_CRITICAL;
    bridge->bbb_effects.avg_threat_severity =
        0.9f * bridge->bbb_effects.avg_threat_severity + 0.1f * severity_weight;

    // Create high-surprise observation for FEP
    // (Threats are unexpected → high prediction error)
    float threat_features[BBB_FEP_MAX_FEATURES];
    memset(threat_features, 0, sizeof(threat_features));

    // Encode threat type as feature
    threat_features[0] = (float)threat_type / 10.0f;
    threat_features[1] = severity_weight;

    // Process as high-surprise observation
    if (bridge->config.enable_online_learning) {
        fep_process_observation(bridge->fep_system, threat_features,
                              bridge->config.input_feature_dim);
        fep_update_precision(bridge->fep_system);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int bbb_fep_get_effects(
    const bbb_fep_bridge_t* bridge,
    bbb_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int bbb_fep_get_bbb_effects(
    const bbb_fep_bridge_t* bridge,
    fep_bbb_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->bbb_effects;
    return 0;
}

int bbb_fep_get_stats(
    const bbb_fep_bridge_t* bridge,
    bbb_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float bbb_fep_get_threat_score(const bbb_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.threat_score;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int bbb_fep_connect_bio_async(bbb_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_BBB_FEP,
        .module_name = "bbb_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("BBB FEP bridge connected to bio-async");
    }

    return 0;
}

int bbb_fep_disconnect_bio_async(bbb_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("BBB FEP bridge disconnected from bio-async");
    return 0;
}

bool bbb_fep_is_bio_async_connected(const bbb_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
