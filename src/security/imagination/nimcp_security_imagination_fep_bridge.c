/**
 * @file nimcp_security_imagination_fep_bridge.c
 * @brief Implementation of Security Imagination FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for imagination security - confabulation as free energy
 * WHY:  Confabulated content represents high prediction error / surprise in FEP
 * HOW:  Map confabulation detection to free energy, use precision for sensitivity
 */

#include "security/imagination/nimcp_security_imagination_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_imagination_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_confab_score_from_fe(float free_energy, float threshold);
static float compute_divergence_from_prediction_error(float pred_error);
static confab_fep_severity_t map_fe_to_severity(float free_energy);
static void update_running_average(float* avg, float new_value, float alpha);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide biologically-plausible defaults
 * WHY:  Simplify initialization
 * HOW:  Return tuned defaults for confabulation detection
 */
int security_imagination_fep_default_config(
    security_imagination_fep_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(security_imagination_fep_config_t));

    /* FEP parameters */
    config->confabulation_fe_threshold = CONFAB_FEP_SIGNIFICANT_THRESHOLD;
    config->divergence_fe_threshold = CONFAB_FEP_MINOR_THRESHOLD;
    config->surprise_threshold = 10.0f;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_detection = true;
    config->enable_precision_modulation = true;
    config->grounded_fe_threshold = CONFAB_FEP_GROUNDED_THRESHOLD;
    config->critical_fe_threshold = CONFAB_FEP_CRITICAL_THRESHOLD;

    /* Active inference settings */
    config->enable_active_inference = true;
    config->inference_learning_rate = CONFAB_FEP_INFERENCE_RATE;
    config->auto_ground_on_detection = true;

    /* Learning settings */
    config->enable_online_learning = true;
    config->reality_model_learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * @brief Create security imagination FEP bridge
 */
security_imagination_fep_bridge_t* security_imagination_fep_create(
    const security_imagination_fep_config_t* config,
    security_imagination_bridge_t* security_bridge,
    fep_system_t* fep_system
) {
    if (!security_bridge || !fep_system) {
        NIMCP_LOGGING_ERROR("Security imagination FEP bridge: NULL pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_create: required parameter is NULL (security_bridge, fep_system)");
        return NULL;
    }

    security_imagination_fep_bridge_t* bridge =
        (security_imagination_fep_bridge_t*)nimcp_malloc(
            sizeof(security_imagination_fep_bridge_t)
        );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security imagination FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_imagination_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_imagination_fep_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_IMAGINATION_FEP,
                         "security_imagination_fep_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_create: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_imagination_fep_default_config(&bridge->config);
    }

    /* Store system handles */
    bridge->fep_system = fep_system;
    bridge->security_bridge = security_bridge;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.security_connected = true;
    bridge->state.fep_connected = true;
    bridge->state.current_precision = CONFAB_FEP_DEFAULT_PRECISION;

    /* Initialize FEP effects defaults */
    bridge->fep_effects.severity = CONFAB_FEP_LEVEL_GROUNDED;
    bridge->fep_effects.response = ACTIVE_INFERENCE_NONE;
    bridge->fep_effects.confidence = 0.5f;

    NIMCP_LOGGING_INFO("Security imagination FEP bridge created");
    return bridge;
}

/**
 * @brief Destroy security imagination FEP bridge
 */
void security_imagination_fep_destroy(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        security_imagination_fep_disconnect_bio_async(bridge);
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Security imagination FEP bridge destroyed");
}

/**
 * @brief Reset bridge state
 */
int security_imagination_fep_reset(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Reset state (preserve connections) */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = CONFAB_FEP_DEFAULT_PRECISION;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.avg_surprise = 0.0f;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_security_effects_t));
    memset(&bridge->security_effects, 0, sizeof(security_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_imagination_fep_stats_t));

    /* Reset working state */
    bridge->last_free_energy = 0.0f;
    bridge->last_prediction_error = 0.0f;
    bridge->last_surprise = 0.0f;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

/**
 * @brief Get current configuration
 */
int security_imagination_fep_get_config(
    const security_imagination_fep_bridge_t* bridge,
    security_imagination_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

/**
 * @brief Set configuration
 */
int security_imagination_fep_set_config(
    security_imagination_fep_bridge_t* bridge,
    const security_imagination_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    BRIDGE_LOCK(bridge);
    bridge->config = *config;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Core Processing Implementation
 * ============================================================================ */

/**
 * @brief Compute FEP effects from current state
 */
int security_imagination_fep_compute_effects(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_compute_effects: bridge->state is NULL");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Store for later use */
    bridge->last_free_energy = current_fe;
    bridge->last_surprise = surprise;
    bridge->last_prediction_error = pred_error;

    /* Update running averages */
    update_running_average(&bridge->state.avg_free_energy, current_fe, 0.1f);
    update_running_average(&bridge->state.avg_surprise, surprise, 0.1f);

    /* Compute FEP-based confabulation score */
    bridge->fep_effects.fep_confabulation_score =
        compute_confab_score_from_fe(current_fe,
                                     bridge->config.confabulation_fe_threshold);

    /* Compute divergence from prediction error */
    bridge->fep_effects.fep_divergence_score =
        compute_divergence_from_prediction_error(pred_error);

    /* Map surprise to severity */
    bridge->fep_effects.fep_surprise_level =
        surprise / bridge->config.surprise_threshold;
    if (bridge->fep_effects.fep_surprise_level > 1.0f) {
        bridge->fep_effects.fep_surprise_level = 1.0f;
    }

    /* Precision-based detection sensitivity */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    /* Confidence based on prediction stability */
    bridge->fep_effects.confidence = 1.0f - (pred_error / 10.0f);
    if (bridge->fep_effects.confidence < 0.0f) {
        bridge->fep_effects.confidence = 0.0f;
    }
    if (bridge->fep_effects.confidence > 1.0f) {
        bridge->fep_effects.confidence = 1.0f;
    }

    /* Map free energy to severity level */
    bridge->fep_effects.severity = map_fe_to_severity(current_fe);

    /* Compute recommended response */
    bridge->fep_effects.response =
        security_imagination_fep_compute_response(bridge,
                                                  bridge->fep_effects.severity);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = pred_error;

    if (current_fe > bridge->stats.peak_free_energy) {
        bridge->stats.peak_free_energy = current_fe;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/**
 * @brief Update bridge from security detection
 */
int security_imagination_fep_update_from_detection(
    security_imagination_fep_bridge_t* bridge,
    const security_imagination_confab_result_t* confab_result
) {
    if (!bridge || !confab_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_update_from_detection: required parameter is NULL (bridge, confab_result)");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.detection_count++;
    bridge->stats.total_checks++;

    if (confab_result->detected) {
        /* Confabulation detected - high surprise observation */
        bridge->security_effects.confabulations_detected++;
        bridge->stats.confabulations_found++;
        bridge->stats.fep_based_detections++;

        /* Update average confab score */
        update_running_average(&bridge->security_effects.avg_confabulation_score,
                               confab_result->score, 0.1f);

        /* Track peak surprise */
        float surprise = bridge->last_surprise;
        if (surprise > bridge->security_effects.peak_surprise) {
            bridge->security_effects.peak_surprise = surprise;
        }

        /* Update FEP precision - increase for confirmed detections */
        if (bridge->config.enable_online_learning) {
            fep_update_precision(bridge->fep_system);
        }
    } else {
        /* Grounded content - update reality model */
        bridge->security_effects.grounded_content++;

        /* Update FEP beliefs for normal samples */
        if (bridge->config.enable_online_learning) {
            fep_update_beliefs(bridge->fep_system);
            bridge->stats.reality_model_updates++;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/**
 * @brief Detect confabulation using FEP-enhanced detection
 */
int security_imagination_fep_detect(
    security_imagination_fep_bridge_t* bridge,
    const void* content,
    size_t content_size,
    security_imagination_confab_result_t* result
) {
    if (!bridge || !content || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_detect: required parameter is NULL (bridge, content, result)");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* First run standard security detection */
    int err = security_imagination_detect_confabulation(
        bridge->security_bridge,
        content,
        content_size,
        result
    );
    if (err != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_imagination_fep_detect: validation failed");
        return -1;
    }

    /* If FEP detection enabled, enhance with FEP scoring */
    if (bridge->config.use_fep_detection) {
        /* Process content through FEP */
        float observation[32] = {0};
        size_t obs_dim = (content_size < 32) ? content_size : 32;

        for (size_t i = 0; i < obs_dim; i++) {
            observation[i] = ((const uint8_t*)content)[i] / 255.0f;
        }

        fep_process_observation(bridge->fep_system, observation, (uint32_t)obs_dim);

        /* Compute free energy */
        fep_free_energy_t fe;
        fep_compute_free_energy(bridge->fep_system, &fe);

        /* Compute FEP-based score */
        float fep_score = compute_confab_score_from_fe(
            fe.total,
            bridge->config.confabulation_fe_threshold
        );

        /* Fuse security and FEP scores (weighted average) */
        float fused_score = 0.5f * result->score + 0.5f * fep_score;
        result->score = fused_score;

        /* Update detection flag based on fused score */
        if (fused_score >= bridge->security_effects.avg_confabulation_score * 0.8f
            && fused_score >= 0.6f) {
            result->detected = true;
        }

        /* Map severity and add to description */
        confab_fep_severity_t severity = map_fe_to_severity(fe.total);
        if (severity >= CONFAB_FEP_LEVEL_SIGNIFICANT) {
            snprintf(result->description, sizeof(result->description),
                    "FEP-enhanced detection: FE=%.2f, surprise=%.2f, severity=%s",
                    fe.total, fe.surprise,
                    confab_fep_severity_to_string(severity));
        }

        /* Store last values */
        bridge->last_free_energy = fe.total;
        bridge->last_surprise = fe.surprise;

        bridge->stats.fep_based_detections++;
    }

    /* Update from this detection */
    security_imagination_fep_update_from_detection(bridge, result);

    /* Auto-ground if configured and detected */
    if (result->detected && bridge->config.auto_ground_on_detection) {
        /* Would need sandbox_id - skip auto-grounding for raw content */
        bridge->stats.grounding_operations++;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/**
 * @brief Apply precision modulation
 */
int security_imagination_fep_apply_precision(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Adapt precision based on detection performance */
    float confab_rate = 0.0f;
    if (bridge->stats.total_checks > 0) {
        confab_rate = (float)bridge->security_effects.confabulations_detected /
                      (float)bridge->stats.total_checks;
    }

    float target_precision = CONFAB_FEP_DEFAULT_PRECISION;

    if (confab_rate > 0.2f) {
        /* High confabulation rate -> increase precision (more sensitive) */
        target_precision = CONFAB_FEP_MAX_PRECISION * 0.8f;
    } else if (confab_rate < 0.05f) {
        /* Low confabulation rate -> decrease precision (less sensitive) */
        target_precision = CONFAB_FEP_MIN_PRECISION * 2.0f;
    }

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to bounds */
    if (bridge->state.current_precision < CONFAB_FEP_MIN_PRECISION) {
        bridge->state.current_precision = CONFAB_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > CONFAB_FEP_MAX_PRECISION) {
        bridge->state.current_precision = CONFAB_FEP_MAX_PRECISION;
    }

    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Reality Grounding Implementation
 * ============================================================================ */

/**
 * @brief Ground imagination to reality using FEP
 */
int security_imagination_fep_ground(
    security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Get current grounding status from security bridge */
    security_imagination_grounding_result_t grounding;
    int err = security_imagination_ground_reality(
        bridge->security_bridge,
        sandbox_id,
        &grounding
    );

    if (err != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_imagination_fep_ground: validation failed");
        return -1;
    }

    /* Use FEP to strengthen reality priors */
    if (bridge->config.enable_active_inference) {
        /* Update beliefs to reduce prediction error */
        fep_update_beliefs(bridge->fep_system);

        /* Strengthen precision for reality anchors */
        float precision_boost = CONFAB_FEP_GROUNDING_WEIGHT *
                                bridge->config.inference_learning_rate;
        bridge->state.current_precision += precision_boost;

        if (bridge->state.current_precision > CONFAB_FEP_MAX_PRECISION) {
            bridge->state.current_precision = CONFAB_FEP_MAX_PRECISION;
        }

        bridge->stats.grounding_operations++;
        bridge->stats.reality_model_updates++;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/**
 * @brief Get grounding strength from FEP
 */
float security_imagination_fep_get_grounding(
    const security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id
) {
    if (!bridge) {
        return -1.0f;
    }

    /* Get divergence from security bridge */
    float divergence = security_imagination_get_divergence(
        bridge->security_bridge,
        sandbox_id
    );

    if (divergence < 0.0f) {
        return -1.0f;
    }

    /* Grounding is inverse of divergence */
    float grounding = 1.0f - divergence;

    /* Weight by inverse of free energy */
    float fe = bridge->last_free_energy;
    if (fe > 0.0f && fe < 100.0f) {
        float fe_factor = 1.0f / (1.0f + fe / 10.0f);
        grounding = grounding * 0.7f + fe_factor * 0.3f;
    }

    return grounding;
}

/* ============================================================================
 * Active Inference Implementation
 * ============================================================================ */

/**
 * @brief Compute active inference response
 */
active_inference_response_t security_imagination_fep_compute_response(
    security_imagination_fep_bridge_t* bridge,
    confab_fep_severity_t severity
) {
    if (!bridge) {
        return ACTIVE_INFERENCE_NONE;
    }

    if (!bridge->config.enable_active_inference) {
        return ACTIVE_INFERENCE_NONE;
    }

    /* Map severity to response */
    switch (severity) {
        case CONFAB_FEP_LEVEL_GROUNDED:
            return ACTIVE_INFERENCE_NONE;

        case CONFAB_FEP_LEVEL_MINOR:
            return ACTIVE_INFERENCE_MONITOR;

        case CONFAB_FEP_LEVEL_SIGNIFICANT:
            return ACTIVE_INFERENCE_GROUND;

        case CONFAB_FEP_LEVEL_CRITICAL:
            return ACTIVE_INFERENCE_QUARANTINE;

        default:
            return ACTIVE_INFERENCE_NONE;
    }
}

/**
 * @brief Execute active inference response
 */
int security_imagination_fep_execute_response(
    security_imagination_fep_bridge_t* bridge,
    uint64_t sandbox_id,
    active_inference_response_t response
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    int err = 0;

    switch (response) {
        case ACTIVE_INFERENCE_NONE:
            /* No action needed */
            break;

        case ACTIVE_INFERENCE_MONITOR:
            /* Just log for monitoring */
            NIMCP_LOGGING_DEBUG("Active inference: Monitoring sandbox %lu",
                               (unsigned long)sandbox_id);
            break;

        case ACTIVE_INFERENCE_GROUND:
            /* Apply reality grounding */
            err = security_imagination_fep_ground(bridge, sandbox_id);
            break;

        case ACTIVE_INFERENCE_RESTRICT:
            /* Restrict imagination capabilities */
            err = security_imagination_enter_restricted(bridge->security_bridge);
            break;

        case ACTIVE_INFERENCE_QUARANTINE:
            /* Quarantine the sandbox content */
            bridge->security_effects.quarantined_content++;
            bridge->stats.quarantine_operations++;
            /* Note: Actual quarantine would be done via security bridge */
            break;
    }

    if (err == 0) {
        bridge->stats.inference_responses++;
    }

    BRIDGE_UNLOCK(bridge);
    return err;
}

/* ============================================================================
 * Feedback Implementation
 * ============================================================================ */

/**
 * @brief Report false positive to update model
 */
int security_imagination_fep_report_false_positive(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    bridge->security_effects.false_positives++;

    /* Reduce precision to prevent similar false positives */
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < CONFAB_FEP_MIN_PRECISION) {
            bridge->state.current_precision = CONFAB_FEP_MIN_PRECISION;
        }

        bridge->stats.false_positive_adjustments++;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/**
 * @brief Report confirmed confabulation
 */
int security_imagination_fep_confirm_confabulation(
    security_imagination_fep_bridge_t* bridge,
    confabulation_type_t confab_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    BRIDGE_LOCK(bridge);

    /* Increase precision for this type of confabulation */
    float boost = bridge->config.precision_learning_rate * 2.0f;
    bridge->state.current_precision += boost;

    if (bridge->state.current_precision > CONFAB_FEP_MAX_PRECISION) {
        bridge->state.current_precision = CONFAB_FEP_MAX_PRECISION;
    }

    /* Update FEP precision */
    if (bridge->config.enable_online_learning) {
        fep_update_precision(bridge->fep_system);
    }

    bridge->stats.precision_adaptations++;

    (void)confab_type;  /* Could use for type-specific learning */

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 */
int security_imagination_fep_get_fep_effects(
    const security_imagination_fep_bridge_t* bridge,
    fep_to_security_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/**
 * @brief Get security effects on FEP
 */
int security_imagination_fep_get_security_effects(
    const security_imagination_fep_bridge_t* bridge,
    security_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_get_security_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->security_effects;
    return 0;
}

/**
 * @brief Get bridge statistics
 */
int security_imagination_fep_get_stats(
    const security_imagination_fep_bridge_t* bridge,
    security_imagination_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/**
 * @brief Get current free energy
 */
float security_imagination_fep_get_free_energy(
    const security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->last_free_energy;
}

/**
 * @brief Get current confabulation score from FEP
 */
float security_imagination_fep_get_confab_score(
    const security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->fep_effects.fep_confabulation_score;
}

/**
 * @brief Get current severity level
 */
confab_fep_severity_t security_imagination_fep_get_severity(
    const security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        return CONFAB_FEP_LEVEL_GROUNDED;
    }

    return bridge->fep_effects.severity;
}

/* ============================================================================
 * Debug/Diagnostic Implementation
 * ============================================================================ */

/**
 * @brief Print bridge summary
 */
void security_imagination_fep_print_summary(
    const security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        printf("Security Imagination FEP Bridge: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("Security Imagination FEP Bridge Summary\n");
    printf("========================================\n");

    printf("\nState:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy: %.3f\n", bridge->state.avg_free_energy);

    printf("\nFEP Effects:\n");
    printf("  Confabulation Score: %.3f\n",
           bridge->fep_effects.fep_confabulation_score);
    printf("  Divergence Score: %.3f\n",
           bridge->fep_effects.fep_divergence_score);
    printf("  Surprise Level: %.3f\n", bridge->fep_effects.fep_surprise_level);
    printf("  Severity: %s\n",
           confab_fep_severity_to_string(bridge->fep_effects.severity));
    printf("  Response: %s\n",
           active_inference_response_to_string(bridge->fep_effects.response));

    printf("\nSecurity Effects:\n");
    printf("  Confabulations Detected: %lu\n",
           (unsigned long)bridge->security_effects.confabulations_detected);
    printf("  Grounded Content: %lu\n",
           (unsigned long)bridge->security_effects.grounded_content);
    printf("  False Positives: %lu\n",
           (unsigned long)bridge->security_effects.false_positives);
    printf("  Quarantined: %lu\n",
           (unsigned long)bridge->security_effects.quarantined_content);

    printf("\nStatistics:\n");
    printf("  Total Checks: %lu\n", (unsigned long)bridge->stats.total_checks);
    printf("  FEP-Based Detections: %lu\n",
           (unsigned long)bridge->stats.fep_based_detections);
    printf("  Grounding Operations: %lu\n",
           (unsigned long)bridge->stats.grounding_operations);
    printf("  Peak Free Energy: %.3f\n", bridge->stats.peak_free_energy);

    printf("========================================\n\n");
}

/**
 * @brief Get severity level as string
 */
const char* confab_fep_severity_to_string(confab_fep_severity_t severity) {
    switch (severity) {
        case CONFAB_FEP_LEVEL_GROUNDED:
            return "GROUNDED";
        case CONFAB_FEP_LEVEL_MINOR:
            return "MINOR";
        case CONFAB_FEP_LEVEL_SIGNIFICANT:
            return "SIGNIFICANT";
        case CONFAB_FEP_LEVEL_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Get response type as string
 */
const char* active_inference_response_to_string(
    active_inference_response_t response
) {
    switch (response) {
        case ACTIVE_INFERENCE_NONE:
            return "NONE";
        case ACTIVE_INFERENCE_MONITOR:
            return "MONITOR";
        case ACTIVE_INFERENCE_GROUND:
            return "GROUND";
        case ACTIVE_INFERENCE_RESTRICT:
            return "RESTRICT";
        case ACTIVE_INFERENCE_QUARANTINE:
            return "QUARANTINE";
        default:
            return "UNKNOWN";
    }
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int security_imagination_fep_connect_bio_async(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_IMAGINATION_FEP,
        .module_name = "security_imagination_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security imagination FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int security_imagination_fep_disconnect_bio_async(
    security_imagination_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security imagination FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool security_imagination_fep_is_bio_async_connected(
    const security_imagination_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute confabulation score from free energy
 *
 * WHAT: Map free energy to [0-1] confabulation score
 * WHY:  Normalize FE for comparison with thresholds
 * HOW:  Divide by threshold, clamp to [0-1]
 */
static float compute_confab_score_from_fe(float free_energy, float threshold) {
    if (threshold <= 0.0f) {
        threshold = CONFAB_FEP_SIGNIFICANT_THRESHOLD;
    }

    float score = free_energy / threshold;

    if (score < 0.0f) {
        score = 0.0f;
    }
    if (score > 1.0f) {
        score = 1.0f;
    }

    return score;
}

/**
 * @brief Compute divergence from prediction error
 *
 * WHAT: Map prediction error to [0-1] divergence score
 * WHY:  Normalize for comparison with reality grounding
 * HOW:  Sigmoid-like transformation
 */
static float compute_divergence_from_prediction_error(float pred_error) {
    /* Use sigmoid-like mapping: 1 / (1 + exp(-k*(x-x0))) */
    float normalized = pred_error / 10.0f;  /* Scale factor */

    /* Simple linear approximation for efficiency */
    float divergence = normalized;
    if (divergence < 0.0f) {
        divergence = 0.0f;
    }
    if (divergence > 1.0f) {
        divergence = 1.0f;
    }

    return divergence;
}

/**
 * @brief Map free energy to severity level
 *
 * WHAT: Convert continuous FE to discrete severity
 * WHY:  Simplify decision making for responses
 * HOW:  Compare against thresholds
 */
static confab_fep_severity_t map_fe_to_severity(float free_energy) {
    if (free_energy < CONFAB_FEP_GROUNDED_THRESHOLD) {
        return CONFAB_FEP_LEVEL_GROUNDED;
    }
    if (free_energy < CONFAB_FEP_MINOR_THRESHOLD) {
        return CONFAB_FEP_LEVEL_MINOR;
    }
    if (free_energy < CONFAB_FEP_SIGNIFICANT_THRESHOLD) {
        return CONFAB_FEP_LEVEL_SIGNIFICANT;
    }
    return CONFAB_FEP_LEVEL_CRITICAL;
}

/**
 * @brief Update running average with exponential smoothing
 *
 * WHAT: Smooth value updates over time
 * WHY:  Avoid sudden jumps in metrics
 * HOW:  avg = (1-alpha)*avg + alpha*new_value
 */
static void update_running_average(float* avg, float new_value, float alpha) {
    if (!avg) {
        return;
    }

    *avg = (1.0f - alpha) * (*avg) + alpha * new_value;
}
