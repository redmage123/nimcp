/**
 * @file nimcp_security_training_fep_bridge.c
 * @brief Implementation of Security Training FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for training security
 * WHY:  Attack detection as surprise minimization
 * HOW:  Map attack scores to free energy, use precision for trust weighting
 */

#include "security/training/nimcp_security_training_fep_bridge.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_training_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Smoothing factor for running averages */
#define SMOOTHING_ALPHA             0.1f

/** Bio-async module ID (using security range) */
#define BIO_MODULE_SECURITY_TRAINING_FEP  0x0627

/** Observation vector size */
#define FEP_OBSERVATION_DIM         6

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Convert trust level to precision weight
 *
 * WHAT: Map discrete trust levels to continuous precision
 * WHY:  Precision weights data source influence in FEP
 * HOW:  Lookup table with scaling
 */
static float trust_to_precision(security_data_trust_t trust, float scale) {
    float base_precision = 0.0f;

    switch (trust) {
        case SECURITY_TRUST_UNTRUSTED:
            base_precision = SECURITY_TRAIN_FEP_UNTRUSTED_PRECISION;
            break;
        case SECURITY_TRUST_VERIFIED:
            base_precision = SECURITY_TRAIN_FEP_VERIFIED_PRECISION;
            break;
        case SECURITY_TRUST_CERTIFIED:
            base_precision = SECURITY_TRAIN_FEP_CERTIFIED_PRECISION;
            break;
        case SECURITY_TRUST_INTERNAL:
            base_precision = SECURITY_TRAIN_FEP_INTERNAL_PRECISION;
            break;
        default:
            base_precision = SECURITY_TRAIN_FEP_UNTRUSTED_PRECISION;
            break;
    }

    return base_precision * scale;
}

/**
 * @brief Compute attack score from free energy
 *
 * WHAT: Normalize free energy to [0-1] attack score
 * WHY:  Unified anomaly metric
 * HOW:  Sigmoid-like mapping with threshold
 */
static float free_energy_to_attack_score(float free_energy, float threshold) {
    if (free_energy <= 0.0f) {
        return 0.0f;
    }

    float normalized = free_energy / threshold;
    if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    return normalized;
}

/**
 * @brief Compute severity from free energy
 *
 * WHAT: Map free energy to severity level
 * WHY:  Inform response urgency
 * HOW:  Threshold-based classification
 */
static security_train_severity_t free_energy_to_severity(
    float free_energy,
    const security_train_fep_config_t* config
) {
    if (free_energy >= config->critical_fe_threshold) {
        return SECURITY_TRAIN_SEVERITY_CRITICAL;
    } else if (free_energy >= config->attack_fe_threshold) {
        return SECURITY_TRAIN_SEVERITY_HIGH;
    } else if (free_energy >= SECURITY_TRAIN_FEP_SUSPICIOUS_THRESHOLD) {
        return SECURITY_TRAIN_SEVERITY_MEDIUM;
    } else if (free_energy >= config->normal_fe_threshold) {
        return SECURITY_TRAIN_SEVERITY_LOW;
    }

    return SECURITY_TRAIN_SEVERITY_NONE;
}

/**
 * @brief Find data source index by name
 */
static int find_source_index(
    const security_training_bridge_t* security,
    const char* source_name
) {
    if (!security || !source_name) {
        return -1;
    }

    for (uint32_t i = 0; i < security->num_data_sources; i++) {
        if (strcmp(security->data_sources[i].name, source_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/**
 * @brief Process bio-async message callback
 *
 * WHAT: Handle bio-async messages routed to this module
 * WHY:  React to security alerts from other modules
 * HOW:  Match signature expected by bio_router_register_handler
 */
static nimcp_error_t bio_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;
    (void)response_promise;

    security_train_fep_bridge_t* bridge = (security_train_fep_bridge_t*)user_data;
    if (!bridge || !msg) {
        return -1;
    }

    /* Increase vigilance on receiving any security message */
    bridge->state.system_precision *= 1.1f;
    bridge->state.system_precision = nimcp_clampf(
        bridge->state.system_precision,
        SECURITY_TRAIN_FEP_MIN_PRECISION,
        SECURITY_TRAIN_FEP_MAX_PRECISION
    );

    bridge->stats.bio_async_messages_received++;
    return 0;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int security_train_fep_default_config(security_train_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->attack_fe_threshold = SECURITY_TRAIN_FEP_ATTACK_THRESHOLD;
    config->surprise_threshold = 15.0f;
    config->precision_learning_rate = SECURITY_TRAIN_FEP_DEFAULT_PRECISION_LR;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SECURITY_TRAIN_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SECURITY_TRAIN_FEP_CRITICAL_THRESHOLD;

    /* Trust-precision coupling */
    config->enable_trust_precision_coupling = true;
    config->trust_precision_scale = 1.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->belief_learning_rate = SECURITY_TRAIN_FEP_DEFAULT_BELIEF_LR;
    config->learn_from_responses = true;

    /* Active defense */
    config->enable_active_defense = true;
    config->action_temperature = 1.0f;

    /* Attack-specific weights */
    config->poisoning_fe_weight = 1.0f;
    config->gradient_fe_weight = 0.8f;
    config->extraction_fe_weight = 0.6f;
    config->backdoor_fe_weight = 1.2f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_TRAIN_FEP_BIO_INBOX_CAPACITY;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_train_fep_bridge_t* security_train_fep_create(
    const security_train_fep_config_t* config,
    fep_system_t* fep_system,
    security_training_bridge_t* security_bridge
) {
    if (!fep_system || !security_bridge) {
        NIMCP_LOGGING_ERROR("Security training FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_create: required parameter is NULL (fep_system, security_bridge)");
        return NULL;
    }

    /* Allocate bridge */
    security_train_fep_bridge_t* bridge = nimcp_malloc(sizeof(security_train_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security training FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_train_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_train_fep_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_TRAINING_FEP,
                         SECURITY_TRAINING_FEP_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_create: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_train_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->fep_system = fep_system;
    bridge->security_bridge = security_bridge;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.system_precision = SECURITY_TRAIN_FEP_DEFAULT_PRECISION;
    bridge->state.avg_source_precision = SECURITY_TRAIN_FEP_DEFAULT_PRECISION;

    /* Initialize baselines for attack detection */
    bridge->state.poisoning_baseline = 0.1f;
    bridge->state.gradient_baseline = 0.1f;
    bridge->state.extraction_baseline = 0.1f;
    bridge->state.backdoor_baseline = 0.05f;

    /* Allocate source precisions if sources already registered */
    if (security_bridge->num_data_sources > 0) {
        uint32_t n = security_bridge->num_data_sources;
        bridge->fep_effects.source_precisions = nimcp_malloc(n * sizeof(float));
        if (bridge->fep_effects.source_precisions) {
            bridge->fep_effects.num_source_precisions = n;
            for (uint32_t i = 0; i < n; i++) {
                security_data_trust_t trust = security_bridge->data_sources[i].trust_level;
                bridge->fep_effects.source_precisions[i] = trust_to_precision(
                    trust, bridge->config.trust_precision_scale
                );
            }
        }
    }

    /* Initialize effects validity */
    bridge->fep_effects.valid = false;
    bridge->security_effects.valid = false;

    /* Update stats */
    bridge->stats.fep_connected = true;
    bridge->stats.security_connected = true;

    NIMCP_LOGGING_INFO("Security training FEP bridge created");
    return bridge;
}

void security_train_fep_destroy(security_train_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async first */
    if (bridge->base.bio_async_enabled) {
        security_train_fep_disconnect_bio_async(bridge);
    }

    /* Free source precisions */
    if (bridge->fep_effects.source_precisions) {
        nimcp_free(bridge->fep_effects.source_precisions);
    }

    /* Free expected gradient stats */
    if (bridge->state.expected_gradient_stats) {
        nimcp_free(bridge->state.expected_gradient_stats);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security training FEP bridge destroyed");
}

int security_train_fep_reset(security_train_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state but preserve connections */
    bridge->state.update_count = 0;
    bridge->state.detection_cycles = 0;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.max_surprise_seen = 0.0f;
    bridge->state.system_precision = SECURITY_TRAIN_FEP_DEFAULT_PRECISION;

    /* Reset baselines */
    bridge->state.poisoning_baseline = 0.1f;
    bridge->state.gradient_baseline = 0.1f;
    bridge->state.extraction_baseline = 0.1f;
    bridge->state.backdoor_baseline = 0.05f;

    /* Reset source precisions */
    if (bridge->fep_effects.source_precisions) {
        for (uint32_t i = 0; i < bridge->fep_effects.num_source_precisions; i++) {
            bridge->fep_effects.source_precisions[i] = SECURITY_TRAIN_FEP_DEFAULT_PRECISION;
        }
    }

    /* Reset effects */
    float* saved_precisions = bridge->fep_effects.source_precisions;
    uint32_t saved_count = bridge->fep_effects.num_source_precisions;
    memset(&bridge->fep_effects, 0, sizeof(bridge->fep_effects));
    bridge->fep_effects.source_precisions = saved_precisions;
    bridge->fep_effects.num_source_precisions = saved_count;

    memset(&bridge->security_effects, 0, sizeof(bridge->security_effects));

    /* Reset stats but preserve connection status */
    bool fep_conn = bridge->stats.fep_connected;
    bool sec_conn = bridge->stats.security_connected;
    bool bio_conn = bridge->stats.bio_async_connected;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.fep_connected = fep_conn;
    bridge->stats.security_connected = sec_conn;
    bridge->stats.bio_async_connected = bio_conn;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int security_train_fep_get_config(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int security_train_fep_set_config(
    security_train_fep_bridge_t* bridge,
    const security_train_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int security_train_fep_compute_effects(security_train_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    bridge->state.avg_surprise = (1.0f - SMOOTHING_ALPHA) * bridge->state.avg_surprise +
                                  SMOOTHING_ALPHA * surprise;
    if (surprise > bridge->state.max_surprise_seen) {
        bridge->state.max_surprise_seen = surprise;
    }

    /* Compute detection threshold scale */
    float threshold_scale = 1.0f;
    if (bridge->config.enable_precision_modulation) {
        threshold_scale = 1.0f / (bridge->state.system_precision + 0.01f);
        threshold_scale = nimcp_clampf(threshold_scale, 0.5f, 2.0f);
    }
    bridge->fep_effects.detection_threshold_scale = threshold_scale;

    /* Compute detection sensitivity from precision */
    bridge->fep_effects.detection_sensitivity = nimcp_clampf(
        bridge->state.system_precision / SECURITY_TRAIN_FEP_MAX_PRECISION,
        0.0f, 1.0f
    );

    /* Compute action urgency */
    float urgency = current_fe / bridge->config.attack_fe_threshold;
    bridge->fep_effects.action_urgency = nimcp_clampf(urgency, 0.0f, 1.0f);

    /* Store FEP metrics */
    bridge->fep_effects.current_free_energy = current_fe;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.prediction_error_magnitude = pred_error;

    /* Compute attack probability and severity */
    bridge->fep_effects.attack_probability = free_energy_to_attack_score(
        current_fe, bridge->config.attack_fe_threshold
    );
    bridge->fep_effects.severity = free_energy_to_severity(current_fe, &bridge->config);

    /* Compute detection confidence (inverse of uncertainty) */
    float uncertainty = 1.0f / (bridge->state.system_precision + 0.1f);
    bridge->fep_effects.detection_confidence = 1.0f - nimcp_clampf(uncertainty, 0.0f, 0.9f);

    /* Active defense policy evaluation (if enabled) */
    if (bridge->config.enable_active_defense) {
        float base_efe = current_fe;
        float threat = bridge->security_effects.current_threat_level;

        /* Policy EFE scores (lower = better) */
        /* MONITOR: Low cost, low effect */
        bridge->fep_effects.policy_scores[SECURITY_TRAIN_POLICY_MONITOR] =
            base_efe * 0.1f;

        /* QUARANTINE: Medium cost, targets specific samples */
        bridge->fep_effects.policy_scores[SECURITY_TRAIN_POLICY_QUARANTINE] =
            (threat > 0.3f) ? base_efe * 0.4f + 1.0f : base_efe * 1.5f;

        /* SANITIZE: Medium cost, affects all gradients */
        bridge->fep_effects.policy_scores[SECURITY_TRAIN_POLICY_SANITIZE] =
            (threat > 0.5f) ? base_efe * 0.3f + 2.0f : base_efe * 1.2f;

        /* HALT: High cost, stops all training */
        bridge->fep_effects.policy_scores[SECURITY_TRAIN_POLICY_HALT] =
            (base_efe > bridge->config.critical_fe_threshold) ?
            base_efe * 0.2f : base_efe * 3.0f + 10.0f;

        /* ROLLBACK: Very high cost, loses training progress */
        bridge->fep_effects.policy_scores[SECURITY_TRAIN_POLICY_ROLLBACK] =
            (base_efe > bridge->config.critical_fe_threshold && threat > 0.8f) ?
            base_efe * 0.15f : base_efe * 5.0f + 20.0f;

        /* Find recommended policy (lowest EFE) */
        float min_efe = bridge->fep_effects.policy_scores[0];
        bridge->fep_effects.recommended_policy = SECURITY_TRAIN_POLICY_MONITOR;
        for (int i = 1; i < SECURITY_TRAIN_POLICY_COUNT; i++) {
            if (bridge->fep_effects.policy_scores[i] < min_efe) {
                min_efe = bridge->fep_effects.policy_scores[i];
                bridge->fep_effects.recommended_policy = (security_train_policy_t)i;
            }
        }
    }

    /* Update source precisions from trust levels */
    if (bridge->config.enable_trust_precision_coupling && bridge->security_bridge) {
        uint32_t num_sources = bridge->security_bridge->num_data_sources;

        /* Reallocate if needed */
        if (num_sources != bridge->fep_effects.num_source_precisions) {
            if (bridge->fep_effects.source_precisions) {
                nimcp_free(bridge->fep_effects.source_precisions);
            }
            bridge->fep_effects.source_precisions = nimcp_malloc(num_sources * sizeof(float));
            bridge->fep_effects.num_source_precisions = num_sources;
        }

        /* Update each source's precision */
        if (bridge->fep_effects.source_precisions) {
            float total_precision = 0.0f;
            for (uint32_t i = 0; i < num_sources; i++) {
                security_data_trust_t trust = bridge->security_bridge->data_sources[i].trust_level;
                float precision = trust_to_precision(trust, bridge->config.trust_precision_scale);
                bridge->fep_effects.source_precisions[i] = precision;
                total_precision += precision;
            }
            if (num_sources > 0) {
                bridge->state.avg_source_precision = total_precision / num_sources;
            }
        }
    }

    /* Mark effects as valid */
    bridge->fep_effects.last_update_ms = nimcp_platform_time_monotonic_ms();
    bridge->fep_effects.valid = true;

    /* Update stats */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.fep_updates++;
    bridge->stats.avg_free_energy = current_fe;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.current_precision = bridge->state.system_precision;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_update_from_poisoning(
    security_train_fep_bridge_t* bridge,
    const security_poisoning_result_t* poisoning_result
) {
    if (!bridge || !poisoning_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_update_from_poisoning: required parameter is NULL (bridge, poisoning_result)");
        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create observation from poisoning detection */
    float observation[FEP_OBSERVATION_DIM];
    observation[0] = poisoning_result->confidence;
    observation[1] = (float)poisoning_result->num_affected / 100.0f;
    observation[2] = poisoning_result->poisoning_detected ? 1.0f : 0.0f;
    observation[3] = (float)poisoning_result->type / (float)SECURITY_POISONING_COUNT;
    observation[4] = bridge->security_effects.current_threat_level;
    observation[5] = bridge->state.poisoning_baseline;

    /* Process observation through FEP */
    fep_process_observation(bridge->fep_system, observation, FEP_OBSERVATION_DIM);

    /* Compute free energy contribution from poisoning */
    float poisoning_fe = poisoning_result->confidence * bridge->config.poisoning_fe_weight;
    if (poisoning_result->poisoning_detected) {
        poisoning_fe *= 2.0f;
    }
    bridge->fep_effects.poisoning_free_energy = poisoning_fe;

    /* Update security effects */
    if (poisoning_result->poisoning_detected) {
        bridge->security_effects.poisoning_detections++;
        bridge->stats.poisoning_events++;
    } else {
        bridge->security_effects.normal_observations++;
    }

    /* Update aggregate metrics with smoothing */
    bridge->security_effects.avg_poisoning_score =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_poisoning_score +
        SMOOTHING_ALPHA * poisoning_result->confidence;

    /* Update threat level */
    float threat = 0.4f * bridge->security_effects.avg_poisoning_score +
                   0.3f * bridge->security_effects.avg_gradient_anomaly +
                   0.15f * bridge->security_effects.avg_extraction_score +
                   0.15f * bridge->security_effects.avg_backdoor_score;
    bridge->security_effects.current_threat_level = nimcp_clampf(threat, 0.0f, 1.0f);

    /* Update baseline with slow adaptation */
    bridge->state.poisoning_baseline =
        0.99f * bridge->state.poisoning_baseline +
        0.01f * poisoning_result->confidence;

    /* Online learning */
    if (bridge->config.enable_online_learning) {
        if (poisoning_result->poisoning_detected) {
            fep_update_precision(bridge->fep_system);
        } else {
            fep_update_beliefs(bridge->fep_system);
        }
    }

    /* Mark effects as valid */
    bridge->security_effects.timestamp_ms = nimcp_platform_time_monotonic_ms();
    bridge->security_effects.valid = true;

    /* Update stats */
    bridge->state.detection_cycles++;
    bridge->stats.detections_processed++;
    bridge->stats.avg_attack_score = bridge->security_effects.current_threat_level;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_update_from_gradient_anomaly(
    security_train_fep_bridge_t* bridge,
    float anomaly_score,
    float gradient_norm,
    float expected_norm
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create observation from gradient anomaly */
    float observation[FEP_OBSERVATION_DIM];
    observation[0] = anomaly_score;
    observation[1] = gradient_norm;
    observation[2] = expected_norm;
    observation[3] = fabsf(gradient_norm - expected_norm) / (expected_norm + 0.01f);
    observation[4] = bridge->security_effects.current_threat_level;
    observation[5] = bridge->state.gradient_baseline;

    /* Process observation through FEP */
    fep_process_observation(bridge->fep_system, observation, FEP_OBSERVATION_DIM);

    /* Compute free energy contribution */
    float gradient_fe = anomaly_score * bridge->config.gradient_fe_weight;
    float norm_deviation = fabsf(gradient_norm - expected_norm) / (expected_norm + 0.01f);
    gradient_fe += norm_deviation * 0.5f;
    bridge->fep_effects.gradient_free_energy = gradient_fe;

    /* Update security effects */
    if (anomaly_score > 0.7f) {
        bridge->security_effects.gradient_manipulations++;
        bridge->stats.gradient_events++;
    }

    /* Update aggregate metrics with smoothing */
    bridge->security_effects.avg_gradient_anomaly =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_gradient_anomaly +
        SMOOTHING_ALPHA * anomaly_score;

    /* Update gradient stability */
    bridge->security_effects.gradient_stability =
        1.0f - nimcp_clampf(anomaly_score, 0.0f, 1.0f);

    /* Update baseline */
    bridge->state.gradient_baseline =
        0.99f * bridge->state.gradient_baseline +
        0.01f * anomaly_score;

    /* Online learning */
    if (bridge->config.enable_online_learning && anomaly_score > 0.5f) {
        fep_update_precision(bridge->fep_system);
    }

    /* Update timestamp */
    bridge->security_effects.timestamp_ms = nimcp_platform_time_monotonic_ms();
    bridge->security_effects.valid = true;
    bridge->stats.detections_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_update_from_extraction_attempt(
    security_train_fep_bridge_t* bridge,
    float extraction_score,
    float query_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create observation from extraction attempt */
    float observation[FEP_OBSERVATION_DIM];
    observation[0] = extraction_score;
    observation[1] = query_rate / 100.0f;  /* Normalize query rate */
    observation[2] = (extraction_score > 0.5f) ? 1.0f : 0.0f;
    observation[3] = bridge->security_effects.avg_extraction_score;
    observation[4] = bridge->security_effects.current_threat_level;
    observation[5] = bridge->state.extraction_baseline;

    /* Process observation through FEP */
    fep_process_observation(bridge->fep_system, observation, FEP_OBSERVATION_DIM);

    /* Compute free energy contribution */
    float extraction_fe = extraction_score * bridge->config.extraction_fe_weight;
    extraction_fe += (query_rate / 1000.0f) * 0.5f;  /* Bonus for high query rate */
    bridge->fep_effects.extraction_free_energy = extraction_fe;

    /* Update security effects */
    if (extraction_score > 0.6f) {
        bridge->security_effects.extraction_attempts++;
        bridge->stats.extraction_events++;
    }

    /* Update aggregate metrics with smoothing */
    bridge->security_effects.avg_extraction_score =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_extraction_score +
        SMOOTHING_ALPHA * extraction_score;

    /* Update baseline */
    bridge->state.extraction_baseline =
        0.99f * bridge->state.extraction_baseline +
        0.01f * extraction_score;

    /* Online learning */
    if (bridge->config.enable_online_learning && extraction_score > 0.6f) {
        fep_update_precision(bridge->fep_system);
    }

    /* Update timestamp */
    bridge->security_effects.timestamp_ms = nimcp_platform_time_monotonic_ms();
    bridge->security_effects.valid = true;
    bridge->stats.detections_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_update_from_backdoor_detection(
    security_train_fep_bridge_t* bridge,
    float backdoor_score,
    float trigger_confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create observation from backdoor detection */
    float observation[FEP_OBSERVATION_DIM];
    observation[0] = backdoor_score;
    observation[1] = trigger_confidence;
    observation[2] = (backdoor_score > 0.5f && trigger_confidence > 0.5f) ? 1.0f : 0.0f;
    observation[3] = bridge->security_effects.avg_backdoor_score;
    observation[4] = bridge->security_effects.current_threat_level;
    observation[5] = bridge->state.backdoor_baseline;

    /* Process observation through FEP */
    fep_process_observation(bridge->fep_system, observation, FEP_OBSERVATION_DIM);

    /* Compute free energy contribution - backdoors are critical */
    float backdoor_fe = backdoor_score * bridge->config.backdoor_fe_weight;
    backdoor_fe += trigger_confidence * 0.8f;  /* High penalty for trigger detection */
    bridge->fep_effects.backdoor_free_energy = backdoor_fe;

    /* Update security effects */
    if (backdoor_score > 0.5f) {
        bridge->security_effects.backdoor_detections++;
        bridge->stats.backdoor_events++;
    }

    /* Update aggregate metrics with smoothing */
    bridge->security_effects.avg_backdoor_score =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_backdoor_score +
        SMOOTHING_ALPHA * backdoor_score;

    /* Update baseline (slower for backdoors - they're rare) */
    bridge->state.backdoor_baseline =
        0.995f * bridge->state.backdoor_baseline +
        0.005f * backdoor_score;

    /* Online learning - always update for backdoors */
    if (bridge->config.enable_online_learning) {
        fep_update_precision(bridge->fep_system);
        if (backdoor_score > 0.7f) {
            fep_update_beliefs(bridge->fep_system);
        }
    }

    /* Update timestamp */
    bridge->security_effects.timestamp_ms = nimcp_platform_time_monotonic_ms();
    bridge->security_effects.valid = true;
    bridge->stats.detections_processed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_update_source_precision(
    security_train_fep_bridge_t* bridge,
    const char* source_name,
    security_data_trust_t trust_level
) {
    if (!bridge || !source_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_update_source_precision: required parameter is NULL (bridge, source_name)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find source index */
    int idx = find_source_index(bridge->security_bridge, source_name);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Ensure precision array exists */
    if (!bridge->fep_effects.source_precisions ||
        (uint32_t)idx >= bridge->fep_effects.num_source_precisions) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Update precision */
    float new_precision = trust_to_precision(trust_level, bridge->config.trust_precision_scale);
    bridge->fep_effects.source_precisions[idx] = new_precision;

    /* Recompute average */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->fep_effects.num_source_precisions; i++) {
        total += bridge->fep_effects.source_precisions[i];
    }
    bridge->state.avg_source_precision = total / bridge->fep_effects.num_source_precisions;

    bridge->stats.precision_adaptations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_report_action(
    security_train_fep_bridge_t* bridge,
    security_train_policy_t action,
    bool success
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update security effects based on action */
    switch (action) {
        case SECURITY_TRAIN_POLICY_QUARANTINE:
            bridge->security_effects.samples_quarantined++;
            break;
        case SECURITY_TRAIN_POLICY_SANITIZE:
            bridge->security_effects.gradients_sanitized++;
            break;
        case SECURITY_TRAIN_POLICY_HALT:
            bridge->security_effects.training_halted = true;
            break;
        case SECURITY_TRAIN_POLICY_ROLLBACK:
            bridge->security_effects.rollbacks_performed++;
            break;
        default:
            break;
    }

    /* Action taken is high-surprise response */
    if (action != SECURITY_TRAIN_POLICY_MONITOR) {
        float observation[FEP_OBSERVATION_DIM] = {
            (float)action / (float)SECURITY_TRAIN_POLICY_COUNT,
            success ? 1.0f : 0.0f,
            bridge->fep_effects.current_free_energy,
            bridge->security_effects.current_threat_level,
            bridge->state.system_precision,
            0.5f
        };
        fep_process_observation(bridge->fep_system, observation, FEP_OBSERVATION_DIM);

        /* Update beliefs based on action outcome */
        if (bridge->config.learn_from_responses) {
            if (success) {
                fep_update_beliefs(bridge->fep_system);
            } else {
                /* Failed action - increase precision for future */
                bridge->state.system_precision *= 1.1f;
                bridge->state.system_precision = nimcp_clampf(
                    bridge->state.system_precision,
                    SECURITY_TRAIN_FEP_MIN_PRECISION,
                    SECURITY_TRAIN_FEP_MAX_PRECISION
                );
            }
        }

        bridge->stats.fep_triggered_actions++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_train_fep_report_false_positive(
    security_train_fep_bridge_t* bridge,
    security_poisoning_type_t attack_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reduce system precision to prevent future FPs */
    float reduction = 0.95f;
    bridge->state.system_precision *= reduction;
    bridge->state.system_precision = nimcp_clampf(
        bridge->state.system_precision,
        SECURITY_TRAIN_FEP_MIN_PRECISION,
        SECURITY_TRAIN_FEP_MAX_PRECISION
    );

    /* Adjust attack-specific baseline based on FP type */
    switch (attack_type) {
        case SECURITY_POISONING_LABEL_FLIP:
        case SECURITY_POISONING_DATA_INJECTION:
            bridge->state.poisoning_baseline *= 1.1f;
            break;
        case SECURITY_POISONING_GRADIENT_MANIPULATION:
            bridge->state.gradient_baseline *= 1.1f;
            break;
        case SECURITY_POISONING_BACKDOOR:
            bridge->state.backdoor_baseline *= 1.1f;
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_train_fep_get_fep_effects(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Copy effects (shallow - source_precisions pointer copied) */
    *effects = bridge->fep_effects;
    return 0;
}

int security_train_fep_get_security_effects(
    const security_train_fep_bridge_t* bridge,
    fep_security_train_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_get_security_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->security_effects;
    return 0;
}

int security_train_fep_get_stats(
    const security_train_fep_bridge_t* bridge,
    security_train_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float security_train_fep_get_free_energy(const security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    return fep_get_free_energy(bridge->fep_system);
}

float security_train_fep_get_surprise(const security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    return fep_compute_surprise(bridge->fep_system);
}

float security_train_fep_get_source_precision(
    const security_train_fep_bridge_t* bridge,
    const char* source_name
) {
    if (!bridge || !source_name || !bridge->fep_effects.source_precisions) {
        return -1.0f;
    }

    int idx = find_source_index(bridge->security_bridge, source_name);
    if (idx < 0 || (uint32_t)idx >= bridge->fep_effects.num_source_precisions) {
        return -1.0f;
    }

    return bridge->fep_effects.source_precisions[idx];
}

float security_train_fep_get_attack_score(const security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    float fe = fep_get_free_energy(bridge->fep_system);
    return free_energy_to_attack_score(fe, bridge->config.attack_fe_threshold);
}

security_train_severity_t security_train_fep_get_severity(
    const security_train_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system) {
        return SECURITY_TRAIN_SEVERITY_NONE;
    }

    float fe = fep_get_free_energy(bridge->fep_system);
    return free_energy_to_severity(fe, &bridge->config);
}

security_train_policy_t security_train_fep_get_recommended_policy(
    const security_train_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SECURITY_TRAIN_POLICY_MONITOR;
    }

    return bridge->fep_effects.recommended_policy;
}

bool security_train_fep_should_act(const security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return false;
    }

    float fe = fep_get_free_energy(bridge->fep_system);
    return fe > bridge->config.attack_fe_threshold;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int security_train_fep_connect_bio_async(security_train_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_TRAINING_FEP,
        .module_name = SECURITY_TRAINING_FEP_MODULE_NAME,
        .inbox_capacity = bridge->config.bio_inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        bridge->stats.bio_async_connected = true;
        NIMCP_LOGGING_INFO("Security training FEP bridge connected to bio-async");
    }

    return 0;
}

int security_train_fep_disconnect_bio_async(security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;
    bridge->stats.bio_async_connected = false;

    NIMCP_LOGGING_INFO("Security training FEP bridge disconnected from bio-async");
    return 0;
}

int security_train_fep_process_messages(security_train_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    /* Process pending messages - handlers are registered separately */
    uint32_t count = bio_router_process_inbox(bridge->base.bio_ctx, 0);
    return (int)count;
}

bool security_train_fep_is_bio_async_connected(const security_train_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* security_train_policy_to_string(security_train_policy_t policy) {
    switch (policy) {
        case SECURITY_TRAIN_POLICY_MONITOR:    return "MONITOR";
        case SECURITY_TRAIN_POLICY_QUARANTINE: return "QUARANTINE";
        case SECURITY_TRAIN_POLICY_SANITIZE:   return "SANITIZE";
        case SECURITY_TRAIN_POLICY_HALT:       return "HALT";
        case SECURITY_TRAIN_POLICY_ROLLBACK:   return "ROLLBACK";
        default:                               return "UNKNOWN";
    }
}

const char* security_train_severity_to_string(security_train_severity_t severity) {
    switch (severity) {
        case SECURITY_TRAIN_SEVERITY_NONE:     return "NONE";
        case SECURITY_TRAIN_SEVERITY_LOW:      return "LOW";
        case SECURITY_TRAIN_SEVERITY_MEDIUM:   return "MEDIUM";
        case SECURITY_TRAIN_SEVERITY_HIGH:     return "HIGH";
        case SECURITY_TRAIN_SEVERITY_CRITICAL: return "CRITICAL";
        default:                               return "UNKNOWN";
    }
}

/* ============================================================================
 * Debug Implementation
 * ============================================================================ */

void security_train_fep_print_summary(const security_train_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Training FEP Bridge: NULL\n");
        return;
    }

    printf("\n");
    printf("============================================================\n");
    printf("Security Training FEP Bridge Summary\n");
    printf("============================================================\n");
    printf("\n");

    printf("State:\n");
    printf("  Active:              %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update count:        %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detection cycles:    %lu\n", (unsigned long)bridge->state.detection_cycles);
    printf("\n");

    printf("FEP Metrics:\n");
    printf("  Free energy:         %.4f\n", bridge->fep_effects.current_free_energy);
    printf("  Surprise:            %.4f\n", bridge->fep_effects.surprise_level);
    printf("  Prediction error:    %.4f\n", bridge->fep_effects.prediction_error_magnitude);
    printf("  System precision:    %.4f\n", bridge->state.system_precision);
    printf("  Avg source precision:%.4f\n", bridge->state.avg_source_precision);
    printf("\n");

    printf("Per-Attack Free Energy:\n");
    printf("  Poisoning FE:        %.4f\n", bridge->fep_effects.poisoning_free_energy);
    printf("  Gradient FE:         %.4f\n", bridge->fep_effects.gradient_free_energy);
    printf("  Extraction FE:       %.4f\n", bridge->fep_effects.extraction_free_energy);
    printf("  Backdoor FE:         %.4f\n", bridge->fep_effects.backdoor_free_energy);
    printf("\n");

    printf("Detection:\n");
    printf("  Sensitivity:         %.4f\n", bridge->fep_effects.detection_sensitivity);
    printf("  Action urgency:      %.4f\n", bridge->fep_effects.action_urgency);
    printf("  Detection confidence:%.4f\n", bridge->fep_effects.detection_confidence);
    printf("  Attack probability:  %.4f\n", bridge->fep_effects.attack_probability);
    printf("  Severity:            %s\n", security_train_severity_to_string(bridge->fep_effects.severity));
    printf("  Recommended policy:  %s\n", security_train_policy_to_string(bridge->fep_effects.recommended_policy));
    printf("\n");

    printf("Security Effects:\n");
    printf("  Poisoning detections:%lu\n", (unsigned long)bridge->security_effects.poisoning_detections);
    printf("  Gradient attacks:    %lu\n", (unsigned long)bridge->security_effects.gradient_manipulations);
    printf("  Extraction attempts: %lu\n", (unsigned long)bridge->security_effects.extraction_attempts);
    printf("  Backdoor detections: %lu\n", (unsigned long)bridge->security_effects.backdoor_detections);
    printf("  Normal observations: %lu\n", (unsigned long)bridge->security_effects.normal_observations);
    printf("  Threat level:        %.4f\n", bridge->security_effects.current_threat_level);
    printf("\n");

    printf("Protective Actions:\n");
    printf("  Samples quarantined: %lu\n", (unsigned long)bridge->security_effects.samples_quarantined);
    printf("  Gradients sanitized: %lu\n", (unsigned long)bridge->security_effects.gradients_sanitized);
    printf("  Training halted:     %s\n", bridge->security_effects.training_halted ? "yes" : "no");
    printf("  Rollbacks performed: %lu\n", (unsigned long)bridge->security_effects.rollbacks_performed);
    printf("\n");

    printf("Thresholds:\n");
    printf("  Normal FE:           %.2f\n", bridge->config.normal_fe_threshold);
    printf("  Attack FE:           %.2f\n", bridge->config.attack_fe_threshold);
    printf("  Critical FE:         %.2f\n", bridge->config.critical_fe_threshold);
    printf("\n");

    printf("Connections:\n");
    printf("  FEP connected:       %s\n", bridge->stats.fep_connected ? "yes" : "no");
    printf("  Security connected:  %s\n", bridge->stats.security_connected ? "yes" : "no");
    printf("  Bio-async connected: %s\n", bridge->stats.bio_async_connected ? "yes" : "no");
    printf("\n");

    printf("============================================================\n");
}
