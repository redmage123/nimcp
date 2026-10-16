/**
 * @file nimcp_security_distributed_training_fep_bridge.c
 * @brief Implementation of Security Distributed Training FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for distributed training security
 * WHY:  Byzantine detection as surprise minimization
 * HOW:  Map Byzantine scores to free energy, use precision for trust weighting
 */

#include "security/distributed/nimcp_security_distributed_training_fep_bridge.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_distributed_training_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Defense policy indices */
#define DEFENSE_POLICY_MONITOR      0
#define DEFENSE_POLICY_REDUCE_TRUST 1
#define DEFENSE_POLICY_QUARANTINE   2
#define DEFENSE_POLICY_REMOVE       3

/** Smoothing factor for running averages */
#define SMOOTHING_ALPHA             0.1f

/** Bio-async module ID (using security range) */
#define BIO_MODULE_SECURITY_DISTRIBUTED_FEP  0x0626

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Convert trust level to precision weight
 *
 * WHAT: Map discrete trust levels to continuous precision
 * WHY:  Precision weights worker influence in FEP
 * HOW:  Lookup table with interpolation
 */
static float trust_to_precision(security_worker_trust_t trust, float scale) {
    float base_precision = 0.0f;

    switch (trust) {
        case SECURITY_WORKER_TRUST_QUARANTINED:
            base_precision = SECURITY_DIST_FEP_QUARANTINED_PRECISION;
            break;
        case SECURITY_WORKER_TRUST_UNTRUSTED:
            base_precision = SECURITY_DIST_FEP_UNTRUSTED_PRECISION;
            break;
        case SECURITY_WORKER_TRUST_PROBATION:
            base_precision = SECURITY_DIST_FEP_PROBATION_PRECISION;
            break;
        case SECURITY_WORKER_TRUST_VERIFIED:
            base_precision = SECURITY_DIST_FEP_VERIFIED_PRECISION;
            break;
        case SECURITY_WORKER_TRUST_TRUSTED:
            base_precision = SECURITY_DIST_FEP_TRUSTED_PRECISION;
            break;
        default:
            base_precision = SECURITY_DIST_FEP_UNTRUSTED_PRECISION;
            break;
    }

    return base_precision * scale;
}

/**
 * @brief Compute Byzantine score from free energy
 *
 * WHAT: Normalize free energy to [0-1] Byzantine score
 * WHY:  Unified anomaly metric
 * HOW:  Sigmoid-like mapping with threshold
 */
static float free_energy_to_byzantine_score(float free_energy, float threshold) {
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
 * @brief Find worker index by ID
 */
static int find_worker_index(
    const security_distributed_training_bridge_t* security,
    const char* worker_id
) {
    if (!security || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_worker_index: required parameter is NULL (security, worker_id)");
        return -1;
    }

    for (uint32_t i = 0; i < security->num_workers; i++) {
        if (strcmp(security->workers[i].worker_id, worker_id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int security_dist_fep_default_config(security_dist_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_default_config: config is NULL");
        return -1;
    }

    /* FEP parameters */
    config->byzantine_fe_threshold = SECURITY_DIST_FEP_BYZANTINE_THRESHOLD;
    config->surprise_threshold = 15.0f;
    config->precision_learning_rate = SECURITY_DIST_FEP_DEFAULT_PRECISION_LR;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SECURITY_DIST_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SECURITY_DIST_FEP_CRITICAL_THRESHOLD;

    /* Trust-precision coupling */
    config->enable_trust_precision_coupling = true;
    config->trust_precision_scale = 1.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->belief_learning_rate = SECURITY_DIST_FEP_DEFAULT_BELIEF_LR;
    config->learn_from_quarantines = true;

    /* Active defense */
    config->enable_active_defense = false;
    config->action_temperature = 1.0f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_DIST_FEP_BIO_INBOX_CAPACITY;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_dist_fep_bridge_t* security_dist_fep_create(
    const security_dist_fep_config_t* config,
    fep_system_t* fep_system,
    security_distributed_training_bridge_t* security_bridge
) {
    if (!fep_system || !security_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_create: fep_system or security_bridge is NULL");
        NIMCP_LOGGING_ERROR("Security distributed FEP bridge: NULL system pointers");
        return NULL;
    }

    /* Allocate bridge */
    security_dist_fep_bridge_t* bridge = nimcp_malloc(sizeof(security_dist_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_dist_fep_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Security distributed FEP bridge: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_dist_fep_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_DISTRIBUTED_FEP,
                         SECURITY_DISTRIBUTED_FEP_MODULE_NAME) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_dist_fep_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_dist_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->fep_system = fep_system;
    bridge->security_bridge = security_bridge;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.system_precision = SECURITY_DIST_FEP_DEFAULT_PRECISION;
    bridge->state.avg_worker_precision = SECURITY_DIST_FEP_DEFAULT_PRECISION;

    /* Allocate worker precisions if workers already registered */
    if (security_bridge->num_workers > 0) {
        uint32_t n = security_bridge->num_workers;
        bridge->fep_effects.worker_precisions = nimcp_malloc(n * sizeof(float));
        if (bridge->fep_effects.worker_precisions) {
            bridge->fep_effects.num_worker_precisions = n;
            for (uint32_t i = 0; i < n; i++) {
                bridge->fep_effects.worker_precisions[i] = SECURITY_DIST_FEP_DEFAULT_PRECISION;
            }
        }
    }

    /* Initialize effects validity */
    bridge->fep_effects.valid = false;
    bridge->security_effects.valid = false;

    /* Update stats */
    bridge->stats.fep_connected = true;
    bridge->stats.security_connected = true;

    NIMCP_LOGGING_INFO("Security distributed FEP bridge created");
    return bridge;
}

void security_dist_fep_destroy(security_dist_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async first */
    if (bridge->base.bio_async_enabled) {
        security_dist_fep_disconnect_bio_async(bridge);
    }

    /* Free worker precisions */
    if (bridge->fep_effects.worker_precisions) {
        nimcp_free(bridge->fep_effects.worker_precisions);
    }

    /* Free expected gradients */
    if (bridge->state.expected_gradients) {
        nimcp_free(bridge->state.expected_gradients);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security distributed FEP bridge destroyed");
}

int security_dist_fep_reset(security_dist_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state but preserve connections */
    bridge->state.update_count = 0;
    bridge->state.detection_cycles = 0;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.max_surprise_seen = 0.0f;
    bridge->state.system_precision = SECURITY_DIST_FEP_DEFAULT_PRECISION;

    /* Reset worker precisions - save pointer before memset */
    float* saved_worker_precisions = bridge->fep_effects.worker_precisions;
    uint32_t saved_num_worker_precisions = bridge->fep_effects.num_worker_precisions;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(bridge->fep_effects));
    memset(&bridge->security_effects, 0, sizeof(bridge->security_effects));

    /* Restore and reset worker precisions */
    bridge->fep_effects.worker_precisions = saved_worker_precisions;
    bridge->fep_effects.num_worker_precisions = saved_num_worker_precisions;
    if (bridge->fep_effects.worker_precisions) {
        for (uint32_t i = 0; i < bridge->fep_effects.num_worker_precisions; i++) {
            bridge->fep_effects.worker_precisions[i] = SECURITY_DIST_FEP_DEFAULT_PRECISION;
        }
    }

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.fep_connected = (bridge->fep_system != NULL);
    bridge->stats.security_connected = (bridge->security_bridge != NULL);
    bridge->stats.bio_async_connected = bridge->base.bio_async_enabled;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int security_dist_fep_get_config(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int security_dist_fep_set_config(
    security_dist_fep_bridge_t* bridge,
    const security_dist_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_set_config: required parameter is NULL (bridge, config)");
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

int security_dist_fep_compute_effects(security_dist_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_compute_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "security_dist_fep_compute_effects: bridge inactive or fep_system is NULL");
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
        bridge->state.system_precision / SECURITY_DIST_FEP_MAX_PRECISION,
        0.0f, 1.0f
    );

    /* Compute quarantine urgency */
    float urgency = current_fe / bridge->config.byzantine_fe_threshold;
    bridge->fep_effects.quarantine_urgency = nimcp_clampf(urgency, 0.0f, 1.0f);

    /* Store FEP metrics */
    bridge->fep_effects.current_free_energy = current_fe;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.prediction_error_magnitude = pred_error;

    /* Compute detection confidence (inverse of uncertainty) */
    float uncertainty = 1.0f / (bridge->state.system_precision + 0.1f);
    bridge->fep_effects.detection_confidence = 1.0f - nimcp_clampf(uncertainty, 0.0f, 0.9f);

    /* Active defense policy evaluation (if enabled) */
    if (bridge->config.enable_active_defense) {
        float base_efe = current_fe;

        /* Policy EFE scores (lower = better) */
        bridge->fep_effects.defense_policy_scores[DEFENSE_POLICY_MONITOR] =
            base_efe * 0.1f;  /* Minimal action */
        bridge->fep_effects.defense_policy_scores[DEFENSE_POLICY_REDUCE_TRUST] =
            base_efe * 0.5f + 1.0f;  /* Moderate action */
        bridge->fep_effects.defense_policy_scores[DEFENSE_POLICY_QUARANTINE] =
            (base_efe > bridge->config.byzantine_fe_threshold) ?
            base_efe * 0.3f : base_efe * 2.0f;  /* Good if threat confirmed */
        bridge->fep_effects.defense_policy_scores[DEFENSE_POLICY_REMOVE] =
            base_efe * 0.2f + 5.0f;  /* Costly but effective */

        /* Find recommended policy (lowest EFE) */
        float min_efe = bridge->fep_effects.defense_policy_scores[0];
        bridge->fep_effects.recommended_policy = 0;
        for (int i = 1; i < 4; i++) {
            if (bridge->fep_effects.defense_policy_scores[i] < min_efe) {
                min_efe = bridge->fep_effects.defense_policy_scores[i];
                bridge->fep_effects.recommended_policy = (uint32_t)i;
            }
        }
    }

    /* Update worker precisions from trust levels */
    if (bridge->config.enable_trust_precision_coupling && bridge->security_bridge) {
        uint32_t num_workers = bridge->security_bridge->num_workers;

        /* Reallocate if needed */
        if (num_workers != bridge->fep_effects.num_worker_precisions) {
            if (bridge->fep_effects.worker_precisions) {
                nimcp_free(bridge->fep_effects.worker_precisions);
            }
            bridge->fep_effects.worker_precisions = nimcp_malloc(num_workers * sizeof(float));
            if (bridge->fep_effects.worker_precisions) {
                bridge->fep_effects.num_worker_precisions = num_workers;
            } else {
                bridge->fep_effects.num_worker_precisions = 0;
            }
        }

        /* Update each worker's precision */
        if (bridge->fep_effects.worker_precisions) {
            float total_precision = 0.0f;
            for (uint32_t i = 0; i < num_workers; i++) {
                security_worker_trust_t trust = bridge->security_bridge->workers[i].trust_level;
                float precision = trust_to_precision(trust, bridge->config.trust_precision_scale);
                bridge->fep_effects.worker_precisions[i] = precision;
                total_precision += precision;
            }
            if (num_workers > 0) {
                bridge->state.avg_worker_precision = total_precision / num_workers;
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

int security_dist_fep_update_from_detection(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    float byzantine_score,
    float gradient_divergence
) {
    if (!bridge || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_update_from_detection: required parameter is NULL (bridge, worker_id)");
        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_update_from_detection: required parameter is NULL (bridge->state, bridge->fep_system)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create observation from detection */
    float observation[4];
    observation[0] = byzantine_score;
    observation[1] = gradient_divergence;
    observation[2] = (float)bridge->security_effects.byzantine_detections /
                     (float)(bridge->state.detection_cycles + 1);
    observation[3] = bridge->security_effects.current_threat_level;

    /* Process observation through FEP */
    fep_process_observation(bridge->fep_system, observation, 4);

    /* Compute free energy after observation */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    /* Update security effects */
    if (byzantine_score > 0.5f) {
        bridge->security_effects.byzantine_detections++;
    } else {
        bridge->security_effects.normal_observations++;
    }

    /* Update aggregate metrics with smoothing */
    bridge->security_effects.avg_byzantine_score =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_byzantine_score +
        SMOOTHING_ALPHA * byzantine_score;

    bridge->security_effects.avg_gradient_divergence =
        (1.0f - SMOOTHING_ALPHA) * bridge->security_effects.avg_gradient_divergence +
        SMOOTHING_ALPHA * gradient_divergence;

    /* Track max divergence */
    if (gradient_divergence > bridge->security_effects.max_gradient_divergence) {
        bridge->security_effects.max_gradient_divergence = gradient_divergence;
        int idx = find_worker_index(bridge->security_bridge, worker_id);
        if (idx >= 0) {
            bridge->security_effects.max_divergence_worker_idx = (uint32_t)idx;
        }
    }

    /* Update threat level */
    bridge->security_effects.current_threat_level =
        0.6f * bridge->security_effects.avg_byzantine_score +
        0.4f * (bridge->security_effects.avg_gradient_divergence / 10.0f);
    bridge->security_effects.current_threat_level = nimcp_clampf(
        bridge->security_effects.current_threat_level, 0.0f, 1.0f
    );

    /* Online learning: update beliefs if enabled */
    if (bridge->config.enable_online_learning) {
        if (byzantine_score > 0.7f) {
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
    bridge->stats.avg_byzantine_score = bridge->security_effects.avg_byzantine_score;

    if (fe.total > bridge->config.byzantine_fe_threshold) {
        bridge->stats.byzantine_events++;
    }
    if (fe.total > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = fe.total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_dist_fep_update_worker_precision(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    security_worker_trust_t trust_level
) {
    if (!bridge || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_update_worker_precision: required parameter is NULL (bridge, worker_id)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find worker index */
    int idx = find_worker_index(bridge->security_bridge, worker_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_dist_fep_update_worker_precision: validation failed");
        return -1;
    }

    /* Ensure precision array exists */
    if (!bridge->fep_effects.worker_precisions ||
        (uint32_t)idx >= bridge->fep_effects.num_worker_precisions) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_dist_fep_update_worker_precision: validation failed");
        return -1;
    }

    /* Update precision */
    float new_precision = trust_to_precision(trust_level, bridge->config.trust_precision_scale);
    bridge->fep_effects.worker_precisions[idx] = new_precision;

    /* Recompute average */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->fep_effects.num_worker_precisions; i++) {
        total += bridge->fep_effects.worker_precisions[i];
    }
    bridge->state.avg_worker_precision = total / bridge->fep_effects.num_worker_precisions;

    bridge->stats.precision_adaptations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_dist_fep_report_quarantine(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id,
    security_byzantine_type_t byzantine_type
) {
    if (!bridge || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_report_quarantine: required parameter is NULL (bridge, worker_id)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update security effects */
    bridge->security_effects.quarantine_events++;
    bridge->security_effects.quarantined_count++;

    /* High-surprise observation: confirmed Byzantine */
    float observation[4] = {1.0f, 10.0f, (float)byzantine_type / 7.0f, 1.0f};
    fep_process_observation(bridge->fep_system, observation, 4);

    /* Update beliefs for confirmed threat */
    if (bridge->config.learn_from_quarantines) {
        fep_update_precision(bridge->fep_system);
        fep_update_beliefs(bridge->fep_system);
    }

    /* Zero out quarantined worker's precision */
    int idx = find_worker_index(bridge->security_bridge, worker_id);
    if (idx >= 0 && bridge->fep_effects.worker_precisions &&
        (uint32_t)idx < bridge->fep_effects.num_worker_precisions) {
        bridge->fep_effects.worker_precisions[idx] = 0.0f;
    }

    bridge->stats.fep_triggered_quarantines++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_dist_fep_report_false_positive(
    security_dist_fep_bridge_t* bridge,
    const char* worker_id
) {
    if (!bridge || !worker_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_report_false_positive: required parameter is NULL (bridge, worker_id)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update effects */
    bridge->security_effects.false_positive_corrections++;

    /* Reduce system precision to prevent future FPs */
    float reduction = 0.95f;
    bridge->state.system_precision *= reduction;
    bridge->state.system_precision = nimcp_clampf(
        bridge->state.system_precision,
        SECURITY_DIST_FEP_MIN_PRECISION,
        SECURITY_DIST_FEP_MAX_PRECISION
    );

    /* Restore worker precision */
    int idx = find_worker_index(bridge->security_bridge, worker_id);
    if (idx >= 0 && bridge->fep_effects.worker_precisions &&
        (uint32_t)idx < bridge->fep_effects.num_worker_precisions) {
        bridge->fep_effects.worker_precisions[idx] = SECURITY_DIST_FEP_DEFAULT_PRECISION;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_dist_fep_get_fep_effects(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Copy effects (shallow - worker_precisions pointer copied) */
    *effects = bridge->fep_effects;
    return 0;
}

int security_dist_fep_get_security_effects(
    const security_dist_fep_bridge_t* bridge,
    fep_security_dist_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_get_security_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->security_effects;
    return 0;
}

int security_dist_fep_get_stats(
    const security_dist_fep_bridge_t* bridge,
    security_dist_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float security_dist_fep_get_free_energy(const security_dist_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    return fep_get_free_energy(bridge->fep_system);
}

float security_dist_fep_get_surprise(const security_dist_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    return fep_compute_surprise(bridge->fep_system);
}

float security_dist_fep_get_worker_precision(
    const security_dist_fep_bridge_t* bridge,
    const char* worker_id
) {
    if (!bridge || !worker_id || !bridge->fep_effects.worker_precisions) {
        return -1.0f;
    }

    int idx = find_worker_index(bridge->security_bridge, worker_id);
    if (idx < 0 || (uint32_t)idx >= bridge->fep_effects.num_worker_precisions) {
        return -1.0f;
    }

    return bridge->fep_effects.worker_precisions[idx];
}

float security_dist_fep_get_byzantine_score(const security_dist_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1.0f;
    }

    float fe = fep_get_free_energy(bridge->fep_system);
    return free_energy_to_byzantine_score(fe, bridge->config.byzantine_fe_threshold);
}

bool security_dist_fep_should_quarantine(const security_dist_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return false;
    }

    float fe = fep_get_free_energy(bridge->fep_system);
    return fe > bridge->config.byzantine_fe_threshold;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int security_dist_fep_connect_bio_async(security_dist_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_dist_fep_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_DISTRIBUTED_FEP,
        .module_name = SECURITY_DISTRIBUTED_FEP_MODULE_NAME,
        .inbox_capacity = bridge->config.bio_inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        bridge->stats.bio_async_connected = true;
        NIMCP_LOGGING_INFO("Security distributed FEP bridge connected to bio-async");
    }

    return 0;
}

int security_dist_fep_disconnect_bio_async(security_dist_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;
    bridge->stats.bio_async_connected = false;

    NIMCP_LOGGING_INFO("Security distributed FEP bridge disconnected from bio-async");
    return 0;
}

bool security_dist_fep_is_bio_async_connected(const security_dist_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Debug Implementation
 * ============================================================================ */

void security_dist_fep_print_summary(const security_dist_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Distributed FEP Bridge: NULL\n");
        return;
    }

    printf("\n");
    printf("============================================================\n");
    printf("Security Distributed Training FEP Bridge Summary\n");
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
    printf("  Avg worker precision:%.4f\n", bridge->state.avg_worker_precision);
    printf("\n");

    printf("Detection:\n");
    printf("  Sensitivity:         %.4f\n", bridge->fep_effects.detection_sensitivity);
    printf("  Quarantine urgency:  %.4f\n", bridge->fep_effects.quarantine_urgency);
    printf("  Detection confidence:%.4f\n", bridge->fep_effects.detection_confidence);
    printf("  Byzantine score:     %.4f\n", security_dist_fep_get_byzantine_score(bridge));
    printf("\n");

    printf("Security Effects:\n");
    printf("  Byzantine detections:%lu\n", (unsigned long)bridge->security_effects.byzantine_detections);
    printf("  Quarantine events:   %lu\n", (unsigned long)bridge->security_effects.quarantine_events);
    printf("  Normal observations: %lu\n", (unsigned long)bridge->security_effects.normal_observations);
    printf("  False positives:     %lu\n", (unsigned long)bridge->security_effects.false_positive_corrections);
    printf("  Threat level:        %.4f\n", bridge->security_effects.current_threat_level);
    printf("  Avg gradient div:    %.4f\n", bridge->security_effects.avg_gradient_divergence);
    printf("\n");

    printf("Thresholds:\n");
    printf("  Normal FE:           %.2f\n", bridge->config.normal_fe_threshold);
    printf("  Byzantine FE:        %.2f\n", bridge->config.byzantine_fe_threshold);
    printf("  Critical FE:         %.2f\n", bridge->config.critical_fe_threshold);
    printf("\n");

    printf("Connections:\n");
    printf("  FEP connected:       %s\n", bridge->stats.fep_connected ? "yes" : "no");
    printf("  Security connected:  %s\n", bridge->stats.security_connected ? "yes" : "no");
    printf("  Bio-async connected: %s\n", bridge->stats.bio_async_connected ? "yes" : "no");
    printf("\n");

    printf("============================================================\n");
}
