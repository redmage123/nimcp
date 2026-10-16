/**
 * @file nimcp_security_immune_fep_bridge.c
 * @brief Implementation of Security-Immune FEP Bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Implements FEP integration for brain immune system security
 * WHY:  Enables predictive processing approach to immune threat detection
 * HOW:  Maps immune anomalies to free energy, uses active inference for
 *       protective responses, precision modulation from cytokines
 */

#include "security/immune/nimcp_security_immune_fep_bridge.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_immune_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Running average smoothing factor */
#define FEP_SMOOTHING_ALPHA  0.1f

/** Minimum update interval (ms) */
#define MIN_UPDATE_INTERVAL_MS  10

/** Storm detection threshold for cytokine sum */
#define STORM_CYTOKINE_THRESHOLD 2.5f

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_evasion_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float detection_rate
);

static float compute_autoimmune_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float attack_rate
);

static float compute_storm_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float cytokine_sum
);

static float compute_memory_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float corruption_rate
);

static immune_fep_threat_t classify_threat_level(
    const security_immune_fep_bridge_t* bridge,
    float total_fe
);

static immune_fep_response_t determine_response(
    const security_immune_fep_bridge_t* bridge,
    float total_fe,
    immune_fep_threat_t threat
);

static void update_running_averages(
    security_immune_fep_bridge_t* bridge,
    float fe,
    float surprise
);

static void sync_immune_state(security_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int security_immune_fep_default_config(security_immune_fep_config_t* config) {
    /* WHAT: Initialize config with sensible defaults */
    /* WHY:  Provide biologically-plausible starting values */
    /* HOW:  Set thresholds based on expected variance in healthy immune function */

    if (!config) {
        return -1;
    }

    /* FEP thresholds */
    config->evasion_fe_threshold = IMMUNE_FEP_THREAT_THRESHOLD;
    config->autoimmune_fe_threshold = IMMUNE_FEP_THREAT_THRESHOLD;
    config->storm_fe_threshold = IMMUNE_FEP_SUSPICIOUS_THRESHOLD;
    config->memory_fe_threshold = IMMUNE_FEP_THREAT_THRESHOLD;
    config->surprise_threshold = 10.0f;

    /* Detection weights */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->evasion_weight = IMMUNE_FEP_EVASION_WEIGHT;
    config->autoimmune_weight = IMMUNE_FEP_AUTOIMMUNE_WEIGHT;
    config->storm_weight = IMMUNE_FEP_STORM_WEIGHT;
    config->memory_weight = IMMUNE_FEP_MEMORY_WEIGHT;

    /* Active inference */
    config->enable_active_inference = true;
    config->response_threshold = IMMUNE_FEP_RESPONSE_THRESHOLD;
    config->emergency_threshold = IMMUNE_FEP_EMERGENCY_THRESHOLD;
    config->recovery_threshold = IMMUNE_FEP_RECOVERY_THRESHOLD;
    config->inference_learning_rate = 0.1f;

    /* Precision learning */
    config->precision_learning_rate = 0.05f;
    config->learn_from_cytokines = true;
    config->learn_from_inflammation = true;

    /* Cytokine influence */
    config->il1_precision_boost = IMMUNE_FEP_IL1_PRECISION_BOOST;
    config->il6_precision_boost = IMMUNE_FEP_IL6_PRECISION_BOOST;
    config->tnf_precision_boost = IMMUNE_FEP_TNF_PRECISION_BOOST;
    config->il10_precision_reduce = IMMUNE_FEP_IL10_PRECISION_REDUCE;

    /* Sensitivity */
    config->fep_sensitivity = 1.0f;
    config->immune_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_immune_fep_bridge_t* security_immune_fep_create(
    const security_immune_fep_config_t* config,
    sec_immune_unified_bridge_t* unified_bridge,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
) {
    /* WHAT: Create and initialize the FEP bridge */
    /* WHY:  Enable predictive processing for immune security */
    /* HOW:  Allocate, initialize base, connect systems */

    if (!unified_bridge || !immune_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Security immune FEP: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_immune_fep_create: required parameter is NULL (unified_bridge, immune_system, fep_system)");
        return NULL;
    }

    security_immune_fep_bridge_t* bridge =
        (security_immune_fep_bridge_t*)nimcp_malloc(sizeof(security_immune_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security immune FEP: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_immune_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_immune_fep_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_IMMUNE_FEP,
                         "security_immune_fep") != 0) {
        NIMCP_LOGGING_ERROR("Security immune FEP: base init failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_immune_fep_create: operation failed");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_immune_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->unified_bridge = unified_bridge;
    bridge->immune_system = immune_system;
    bridge->fep_system = fep_system;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.evasion_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.autoimmune_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.storm_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.memory_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.last_response = IMMUNE_FEP_RESPONSE_NONE;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    NIMCP_LOGGING_INFO("Security immune FEP bridge created");
    return bridge;
}

void security_immune_fep_destroy(security_immune_fep_bridge_t* bridge) {
    /* WHAT: Clean up bridge resources */
    /* WHY:  Prevent memory leaks */
    /* HOW:  Disconnect bio-async, cleanup base, free */

    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        security_immune_fep_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Security immune FEP bridge destroyed");
}

int security_immune_fep_reset(security_immune_fep_bridge_t* bridge) {
    /* WHAT: Reset state while preserving connections */
    /* WHY:  Allow bridge reuse */
    /* HOW:  Zero state/stats, reset precision */

    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.inference_count = 0;
    bridge->state.current_free_energy = 0.0f;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.max_free_energy = 0.0f;
    bridge->state.current_surprise = 0.0f;
    bridge->state.avg_surprise = 0.0f;

    /* Reset precision to defaults */
    bridge->state.evasion_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.autoimmune_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.storm_precision = IMMUNE_FEP_DEFAULT_PRECISION;
    bridge->state.memory_precision = IMMUNE_FEP_DEFAULT_PRECISION;

    bridge->state.last_response = IMMUNE_FEP_RESPONSE_NONE;
    bridge->state.emergency_mode_active = false;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_immune_effects_t));
    memset(&bridge->immune_effects, 0, sizeof(immune_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_immune_fep_stats_t));

    /* Reset base */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Security immune FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int security_immune_fep_get_config(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int security_immune_fep_set_config(
    security_immune_fep_bridge_t* bridge,
    const security_immune_fep_config_t* config
) {
    if (!bridge || !config) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Security immune FEP config updated");
    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

int security_immune_fep_compute_effects(security_immune_fep_bridge_t* bridge) {
    /* WHAT: Compute FEP effects from current immune state */
    /* WHY:  Core FEP computation - quantify surprise */
    /* HOW:  Process immune state through generative model */

    if (!bridge) {
        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Sync immune state from connected systems */
    sync_immune_state(bridge);

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise);

    /* Compute detection rates from counts */
    float total_updates = (float)(bridge->state.update_count + 1);
    float evasion_rate = (float)bridge->immune_effects.evasion_detections / total_updates;
    float autoimmune_rate = (float)bridge->immune_effects.autoimmune_detections / total_updates;
    float memory_rate = (float)bridge->immune_effects.memory_corruptions / total_updates;

    /* Compute cytokine sum for storm detection */
    float cytokine_sum = bridge->immune_effects.cytokine_il1 +
                         bridge->immune_effects.cytokine_il6 +
                         bridge->immune_effects.cytokine_tnf;

    /* Compute component-wise free energy */
    float evasion_fe = compute_evasion_free_energy(bridge, evasion_rate);
    float autoimmune_fe = compute_autoimmune_free_energy(bridge, autoimmune_rate);
    float storm_fe = compute_storm_free_energy(bridge, cytokine_sum);
    float memory_fe = compute_memory_free_energy(bridge, memory_rate);

    /* Combine with sensitivity scaling */
    float total_fe = (evasion_fe + autoimmune_fe + storm_fe + memory_fe) *
                     bridge->config.fep_sensitivity;

    /* Update effects */
    bridge->fep_effects.total_free_energy = total_fe;
    bridge->fep_effects.evasion_surprise = evasion_fe;
    bridge->fep_effects.autoimmune_surprise = autoimmune_fe;
    bridge->fep_effects.storm_surprise = storm_fe;
    bridge->fep_effects.memory_surprise = memory_fe;

    /* Compute normalized threat severity */
    float max_threshold = IMMUNE_FEP_CRITICAL_THRESHOLD;
    bridge->fep_effects.threat_severity = fminf(total_fe / max_threshold, 1.0f);

    /* Compute probabilities from component FE */
    bridge->fep_effects.evasion_probability =
        fminf(evasion_fe / bridge->config.evasion_fe_threshold, 1.0f);
    bridge->fep_effects.autoimmune_probability =
        fminf(autoimmune_fe / bridge->config.autoimmune_fe_threshold, 1.0f);
    bridge->fep_effects.storm_probability =
        fminf(storm_fe / bridge->config.storm_fe_threshold, 1.0f);
    bridge->fep_effects.memory_corruption_prob =
        fminf(memory_fe / bridge->config.memory_fe_threshold, 1.0f);

    /* Update precision effects */
    float avg_precision = (bridge->state.evasion_precision +
                           bridge->state.autoimmune_precision +
                           bridge->state.storm_precision +
                           bridge->state.memory_precision) / 4.0f;
    bridge->fep_effects.detection_sensitivity = avg_precision;
    bridge->fep_effects.current_precision = avg_precision;
    bridge->fep_effects.cytokine_precision_mod =
        security_immune_fep_get_cytokine_precision_mod(bridge);

    /* Classify threat level */
    bridge->fep_effects.threat_level = classify_threat_level(bridge, total_fe);

    /* Determine recommended action via active inference */
    bridge->fep_effects.recommended_action = determine_response(bridge, total_fe,
        bridge->fep_effects.threat_level);

    /* Update state */
    bridge->state.current_free_energy = total_fe;
    if (total_fe > bridge->state.max_free_energy) {
        bridge->state.max_free_energy = total_fe;
    }
    bridge->state.current_surprise = surprise;
    bridge->state.update_count++;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.max_free_energy = bridge->state.max_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.current_precision = avg_precision;

    /* Track update time */
    uint64_t end_time = nimcp_platform_time_monotonic_us();
    float update_time = (float)(end_time - start_time);
    bridge->stats.avg_update_time_us =
        0.9f * bridge->stats.avg_update_time_us + 0.1f * update_time;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_immune_fep_update_from_detection(
    security_immune_fep_bridge_t* bridge,
    immune_fep_threat_t detection_type,
    float severity,
    uint32_t antigen_id
) {
    /* WHAT: Feed detection result to FEP */
    /* WHY:  Detections are high-surprise observations */
    /* HOW:  Convert to FEP observation, update beliefs */

    if (!bridge) {
        return -1;
    }

    (void)antigen_id;  /* Future: correlate with specific antigens */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map detection to prediction error weight */
    float weight = 1.0f;
    switch (detection_type) {
        case IMMUNE_FEP_THREAT_EVASION:
            weight = bridge->config.evasion_weight;
            bridge->immune_effects.evasion_detections++;
            bridge->stats.evasion_detections++;
            break;
        case IMMUNE_FEP_THREAT_AUTOIMMUNE:
            weight = bridge->config.autoimmune_weight;
            bridge->immune_effects.autoimmune_detections++;
            bridge->stats.autoimmune_detections++;
            break;
        case IMMUNE_FEP_THREAT_STORM:
            weight = bridge->config.storm_weight;
            bridge->immune_effects.storm_indicators++;
            bridge->stats.storm_detections++;
            break;
        case IMMUNE_FEP_THREAT_MEMORY:
            weight = bridge->config.memory_weight;
            bridge->immune_effects.memory_corruptions++;
            bridge->stats.memory_corruptions++;
            break;
        default:
            break;
    }

    /* Create observation for FEP */
    float observation[16] = {0};
    observation[0] = severity * weight;
    observation[1] = (float)detection_type / 7.0f;  /* Normalized type */

    fep_process_observation(bridge->fep_system, observation, 2);

    /* Update beliefs based on detection */
    fep_update_beliefs(bridge->fep_system);

    bridge->stats.threats_detected++;
    bridge->stats.fep_based_decisions++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_immune_fep_active_inference(
    security_immune_fep_bridge_t* bridge,
    immune_fep_response_t* response
) {
    /* WHAT: Use active inference for protective response */
    /* WHY:  Actions minimize expected free energy */
    /* HOW:  Evaluate policies, select EFE-minimizing action */

    if (!bridge || !response) {
        return -1;
    }

    if (!bridge->config.enable_active_inference) {
        *response = IMMUNE_FEP_RESPONSE_NONE;
        return 0;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy ratio */
    float fe_ratio = bridge->state.current_free_energy / IMMUNE_FEP_CRITICAL_THRESHOLD;

    /* Check for storm condition - highest priority */
    if (bridge->immune_effects.cytokine_storm_active ||
        bridge->fep_effects.storm_probability > 0.7f) {
        *response = IMMUNE_FEP_RESPONSE_SUPPRESS;
        bridge->state.emergency_mode_active = true;
        bridge->stats.emergency_responses++;
    }
    /* Check for emergency threshold */
    else if (fe_ratio >= bridge->config.emergency_threshold) {
        *response = IMMUNE_FEP_RESPONSE_EMERGENCY;
        bridge->state.emergency_mode_active = true;
        bridge->stats.emergency_responses++;
    }
    /* Check for memory corruption - needs repair */
    else if (bridge->fep_effects.memory_corruption_prob > 0.5f) {
        *response = IMMUNE_FEP_RESPONSE_REPAIR;
        bridge->stats.memory_repairs++;
    }
    /* Check for autoimmune - activate regulators */
    else if (bridge->fep_effects.autoimmune_probability > 0.5f) {
        *response = IMMUNE_FEP_RESPONSE_REGULATE;
        bridge->stats.regulatory_activations++;
    }
    /* Check for general response threshold */
    else if (fe_ratio >= bridge->config.response_threshold) {
        *response = IMMUNE_FEP_RESPONSE_INFLAME;
        bridge->stats.inflammation_triggers++;
    }
    /* Check for monitoring threshold */
    else if (fe_ratio >= 0.3f) {
        *response = IMMUNE_FEP_RESPONSE_ALERT;
    }
    /* Low threat - observe */
    else if (fe_ratio >= 0.1f) {
        *response = IMMUNE_FEP_RESPONSE_OBSERVE;
    }
    /* Recovery from emergency */
    else if (fe_ratio <= bridge->config.recovery_threshold &&
             bridge->state.emergency_mode_active) {
        *response = IMMUNE_FEP_RESPONSE_NONE;
        bridge->state.emergency_mode_active = false;
    }
    else {
        *response = IMMUNE_FEP_RESPONSE_NONE;
    }

    bridge->state.last_response = *response;
    bridge->state.last_response_time = nimcp_platform_time_monotonic_ms();
    bridge->state.inference_count++;

    /* Track inference time */
    uint64_t end_time = nimcp_platform_time_monotonic_us();
    float inference_time = (float)(end_time - start_time);
    bridge->stats.avg_inference_time_us =
        0.9f * bridge->stats.avg_inference_time_us + 0.1f * inference_time;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_immune_fep_modulate_precision(
    security_immune_fep_bridge_t* bridge
) {
    /* WHAT: Adapt detection precision */
    /* WHY:  Precision is attention - focus on likely threats */
    /* HOW:  Update based on cytokines, inflammation, detection history */

    if (!bridge) {
        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float lr = bridge->config.precision_learning_rate;
    float total_detections = (float)(bridge->stats.threats_detected + 1);
    float fp_rate = (float)bridge->immune_effects.false_positives / total_detections;

    /* Compute cytokine-based precision modifier */
    float cytokine_mod = 1.0f;
    if (bridge->config.learn_from_cytokines) {
        cytokine_mod += bridge->immune_effects.cytokine_il1 * bridge->config.il1_precision_boost;
        cytokine_mod += bridge->immune_effects.cytokine_il6 * bridge->config.il6_precision_boost;
        cytokine_mod += bridge->immune_effects.cytokine_tnf * bridge->config.tnf_precision_boost;
        cytokine_mod -= bridge->immune_effects.cytokine_il10 * bridge->config.il10_precision_reduce;
    }

    /* Compute inflammation-based precision modifier */
    float inflammation_mod = 1.0f;
    if (bridge->config.learn_from_inflammation) {
        switch (bridge->immune_effects.inflammation_level) {
            case INFLAMMATION_LOCAL:    inflammation_mod = 1.1f; break;
            case INFLAMMATION_REGIONAL: inflammation_mod = 1.3f; break;
            case INFLAMMATION_SYSTEMIC: inflammation_mod = 1.5f; break;
            case INFLAMMATION_STORM:    inflammation_mod = 2.0f; break;
            default: break;
        }
    }

    /* Target precision based on threat rates and modifiers */
    float base_target = IMMUNE_FEP_DEFAULT_PRECISION * cytokine_mod * inflammation_mod;

    /* Evasion precision - increase if evasions detected */
    float evasion_rate = (float)bridge->stats.evasion_detections / total_detections;
    float evasion_target = base_target;
    if (evasion_rate > 0.1f) {
        evasion_target = IMMUNE_FEP_MAX_PRECISION * 0.8f * cytokine_mod;
    } else if (fp_rate > 0.2f) {
        evasion_target = IMMUNE_FEP_MIN_PRECISION * 2.0f;
    }
    bridge->state.evasion_precision =
        (1.0f - lr) * bridge->state.evasion_precision + lr * evasion_target;

    /* Autoimmune precision - reduce if autoimmune-like patterns detected */
    float autoimmune_rate = (float)bridge->stats.autoimmune_detections / total_detections;
    float autoimmune_target = base_target;
    if (autoimmune_rate > 0.05f) {
        /* Reduce precision to avoid false positives on self */
        autoimmune_target = IMMUNE_FEP_DEFAULT_PRECISION * 0.7f;
    }
    bridge->state.autoimmune_precision =
        (1.0f - lr) * bridge->state.autoimmune_precision + lr * autoimmune_target;

    /* Storm precision - always high during inflammation */
    float storm_target = base_target * inflammation_mod;
    bridge->state.storm_precision =
        (1.0f - lr) * bridge->state.storm_precision + lr * storm_target;

    /* Memory precision - increase if corruptions detected */
    float memory_rate = (float)bridge->stats.memory_corruptions / total_detections;
    float memory_target = base_target;
    if (memory_rate > 0.05f) {
        memory_target = IMMUNE_FEP_MAX_PRECISION * cytokine_mod;
    }
    bridge->state.memory_precision =
        (1.0f - lr) * bridge->state.memory_precision + lr * memory_target;

    /* Clamp all precisions to valid range */
    bridge->state.evasion_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        fminf(IMMUNE_FEP_MAX_PRECISION, bridge->state.evasion_precision));
    bridge->state.autoimmune_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        fminf(IMMUNE_FEP_MAX_PRECISION, bridge->state.autoimmune_precision));
    bridge->state.storm_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        fminf(IMMUNE_FEP_MAX_PRECISION, bridge->state.storm_precision));
    bridge->state.memory_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        fminf(IMMUNE_FEP_MAX_PRECISION, bridge->state.memory_precision));

    bridge->stats.precision_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Detection Feedback Implementation
 * ============================================================================ */

int security_immune_fep_report_evasion(
    security_immune_fep_bridge_t* bridge,
    uint32_t antigen_id,
    uint32_t evasion_method,
    float confidence
) {
    /* WHAT: Feed evasion detection to FEP */
    /* WHY:  Evasion attempts are high-surprise */
    /* HOW:  Map to prediction error, increase precision */

    if (!bridge) {
        return -1;
    }

    (void)evasion_method;  /* Future: weight by evasion sophistication */

    return security_immune_fep_update_from_detection(
        bridge, IMMUNE_FEP_THREAT_EVASION, confidence, antigen_id);
}

int security_immune_fep_report_autoimmune(
    security_immune_fep_bridge_t* bridge,
    uint32_t target_component,
    uint32_t attacker_id,
    float severity
) {
    /* WHAT: Feed autoimmune detection to FEP */
    /* WHY:  Self-attack patterns indicate tolerance failure */
    /* HOW:  Map to prediction error, activate regulators */

    if (!bridge) {
        return -1;
    }

    (void)target_component;
    (void)attacker_id;

    return security_immune_fep_update_from_detection(
        bridge, IMMUNE_FEP_THREAT_AUTOIMMUNE, severity, 0);
}

int security_immune_fep_report_storm(
    security_immune_fep_bridge_t* bridge,
    const float* cytokine_levels,
    brain_inflammation_level_t inflammation_level
) {
    /* WHAT: Feed storm indicators to FEP */
    /* WHY:  Runaway inflammation is extremely high surprise */
    /* HOW:  Map cytokine levels to prediction error */

    if (!bridge || !cytokine_levels) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update cytokine state */
    bridge->immune_effects.cytokine_il1 = cytokine_levels[0];
    bridge->immune_effects.cytokine_il6 = cytokine_levels[1];
    bridge->immune_effects.cytokine_tnf = cytokine_levels[2];
    bridge->immune_effects.cytokine_il10 = cytokine_levels[3];
    bridge->immune_effects.cytokine_ifn = cytokine_levels[4];

    bridge->immune_effects.inflammation_level = inflammation_level;

    /* Check for storm condition */
    float pro_inflammatory_sum = cytokine_levels[0] + cytokine_levels[1] + cytokine_levels[2];
    bool storm_active = (pro_inflammatory_sum > STORM_CYTOKINE_THRESHOLD) ||
                        (inflammation_level == INFLAMMATION_STORM);
    bridge->immune_effects.cytokine_storm_active = storm_active;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Compute severity from cytokine sum */
    float severity = fminf(pro_inflammatory_sum / 3.0f, 1.0f);

    return security_immune_fep_update_from_detection(
        bridge, IMMUNE_FEP_THREAT_STORM, severity, 0);
}

int security_immune_fep_report_memory_corruption(
    security_immune_fep_bridge_t* bridge,
    uint32_t memory_cell_id,
    uint32_t corruption_type,
    float severity
) {
    /* WHAT: Feed memory corruption detection to FEP */
    /* WHY:  Memory failures undermine predictive model */
    /* HOW:  Map to prediction error, trigger reconsolidation */

    if (!bridge) {
        return -1;
    }

    (void)memory_cell_id;
    (void)corruption_type;

    return security_immune_fep_update_from_detection(
        bridge, IMMUNE_FEP_THREAT_MEMORY, severity, 0);
}

int security_immune_fep_report_false_positive(
    security_immune_fep_bridge_t* bridge,
    immune_fep_threat_t detection_type
) {
    /* WHAT: Indicate false positive detection */
    /* WHY:  Reduce precision to prevent similar FPs */
    /* HOW:  Lower type-specific precision */

    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_effects.false_positives++;
    bridge->stats.false_positives++;

    /* Reduce precision for the specific detection type */
    float reduction = 0.9f;
    switch (detection_type) {
        case IMMUNE_FEP_THREAT_EVASION:
            bridge->state.evasion_precision *= reduction;
            break;
        case IMMUNE_FEP_THREAT_AUTOIMMUNE:
            bridge->state.autoimmune_precision *= reduction;
            break;
        case IMMUNE_FEP_THREAT_STORM:
            bridge->state.storm_precision *= reduction;
            break;
        case IMMUNE_FEP_THREAT_MEMORY:
            bridge->state.memory_precision *= reduction;
            break;
        default:
            /* Reduce all for unknown type */
            bridge->state.evasion_precision *= reduction;
            bridge->state.autoimmune_precision *= reduction;
            bridge->state.storm_precision *= reduction;
            bridge->state.memory_precision *= reduction;
            break;
    }

    /* Clamp to minimum */
    bridge->state.evasion_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        bridge->state.evasion_precision);
    bridge->state.autoimmune_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        bridge->state.autoimmune_precision);
    bridge->state.storm_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        bridge->state.storm_precision);
    bridge->state.memory_precision = fmaxf(IMMUNE_FEP_MIN_PRECISION,
        bridge->state.memory_precision);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Cytokine Integration Implementation
 * ============================================================================ */

int security_immune_fep_sync_cytokines(
    security_immune_fep_bridge_t* bridge
) {
    /* WHAT: Sync cytokine levels from immune system */
    /* WHY:  Keep FEP precision modulation up-to-date */
    /* HOW:  Read levels from immune system, update effects */

    if (!bridge || !bridge->immune_system) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Read cytokine levels from brain immune system */
    bridge->immune_effects.cytokine_il1 =
        brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    bridge->immune_effects.cytokine_il6 =
        brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    bridge->immune_effects.cytokine_tnf =
        brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    bridge->immune_effects.cytokine_il10 =
        brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL10);
    bridge->immune_effects.cytokine_ifn =
        brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    /* Read inflammation level */
    bridge->immune_effects.inflammation_level =
        brain_immune_get_inflammation_level(bridge->immune_system);

    /* Check for storm condition */
    float pro_inflammatory_sum = bridge->immune_effects.cytokine_il1 +
                                 bridge->immune_effects.cytokine_il6 +
                                 bridge->immune_effects.cytokine_tnf;
    bridge->immune_effects.cytokine_storm_active =
        (pro_inflammatory_sum > STORM_CYTOKINE_THRESHOLD) ||
        (bridge->immune_effects.inflammation_level == INFLAMMATION_STORM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float security_immune_fep_get_cytokine_precision_mod(
    const security_immune_fep_bridge_t* bridge
) {
    /* WHAT: Compute precision modifier from cytokine state */
    /* WHY:  Pro-inflammatory cytokines increase alertness */
    /* HOW:  Weighted sum of cytokine effects */

    if (!bridge) {
        return 1.0f;
    }

    float mod = 1.0f;
    mod += bridge->immune_effects.cytokine_il1 * bridge->config.il1_precision_boost;
    mod += bridge->immune_effects.cytokine_il6 * bridge->config.il6_precision_boost;
    mod += bridge->immune_effects.cytokine_tnf * bridge->config.tnf_precision_boost;
    mod -= bridge->immune_effects.cytokine_il10 * bridge->config.il10_precision_reduce;

    return fmaxf(0.5f, fminf(2.0f, mod));  /* Clamp to reasonable range */
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_immune_fep_get_fep_effects(
    const security_immune_fep_bridge_t* bridge,
    fep_to_immune_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int security_immune_fep_get_immune_effects(
    const security_immune_fep_bridge_t* bridge,
    immune_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->immune_effects;
    return 0;
}

int security_immune_fep_get_state(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_state_t* state
) {
    if (!bridge || !state) {
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int security_immune_fep_get_stats(
    const security_immune_fep_bridge_t* bridge,
    security_immune_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float security_immune_fep_get_free_energy(
    const security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->state.current_free_energy;
}

immune_fep_threat_t security_immune_fep_get_threat_level(
    const security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        return IMMUNE_FEP_THREAT_NONE;
    }

    return bridge->fep_effects.threat_level;
}

bool security_immune_fep_is_emergency_mode(
    const security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    return bridge->state.emergency_mode_active;
}

/* ============================================================================
 * Debug/Diagnostic Implementation
 * ============================================================================ */

void security_immune_fep_print_summary(
    const security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        printf("Security Immune FEP Bridge: NULL\n");
        return;
    }

    printf("=== Security Immune FEP Bridge Summary ===\n");
    printf("State: %s\n", bridge->state.active ? "ACTIVE" : "INACTIVE");
    printf("Emergency Mode: %s\n", bridge->state.emergency_mode_active ? "YES" : "NO");
    printf("Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("\n");

    printf("Free Energy:\n");
    printf("  Current: %.3f\n", bridge->state.current_free_energy);
    printf("  Average: %.3f\n", bridge->state.avg_free_energy);
    printf("  Maximum: %.3f\n", bridge->state.max_free_energy);
    printf("\n");

    printf("Component Surprise:\n");
    printf("  Evasion:    %.3f\n", bridge->fep_effects.evasion_surprise);
    printf("  Autoimmune: %.3f\n", bridge->fep_effects.autoimmune_surprise);
    printf("  Storm:      %.3f\n", bridge->fep_effects.storm_surprise);
    printf("  Memory:     %.3f\n", bridge->fep_effects.memory_surprise);
    printf("\n");

    printf("Precision:\n");
    printf("  Evasion:    %.3f\n", bridge->state.evasion_precision);
    printf("  Autoimmune: %.3f\n", bridge->state.autoimmune_precision);
    printf("  Storm:      %.3f\n", bridge->state.storm_precision);
    printf("  Memory:     %.3f\n", bridge->state.memory_precision);
    printf("  Cytokine Mod: %.3f\n", bridge->fep_effects.cytokine_precision_mod);
    printf("\n");

    printf("Cytokine State:\n");
    printf("  IL-1:  %.2f\n", bridge->immune_effects.cytokine_il1);
    printf("  IL-6:  %.2f\n", bridge->immune_effects.cytokine_il6);
    printf("  TNF-a: %.2f\n", bridge->immune_effects.cytokine_tnf);
    printf("  IL-10: %.2f\n", bridge->immune_effects.cytokine_il10);
    printf("  IFN-g: %.2f\n", bridge->immune_effects.cytokine_ifn);
    printf("  Storm Active: %s\n",
           bridge->immune_effects.cytokine_storm_active ? "YES" : "NO");
    printf("\n");

    printf("Threat Assessment:\n");
    printf("  Level: %s\n", immune_fep_threat_to_string(bridge->fep_effects.threat_level));
    printf("  Severity: %.1f%%\n", bridge->fep_effects.threat_severity * 100.0f);
    printf("  Evasion P: %.1f%%\n", bridge->fep_effects.evasion_probability * 100.0f);
    printf("  Autoimmune P: %.1f%%\n", bridge->fep_effects.autoimmune_probability * 100.0f);
    printf("  Storm P: %.1f%%\n", bridge->fep_effects.storm_probability * 100.0f);
    printf("  Memory Corrupt P: %.1f%%\n", bridge->fep_effects.memory_corruption_prob * 100.0f);
    printf("  Recommended: %s\n",
           immune_fep_response_to_string(bridge->fep_effects.recommended_action));
    printf("\n");

    printf("Statistics:\n");
    printf("  Threats detected: %lu\n", (unsigned long)bridge->stats.threats_detected);
    printf("  Evasion: %lu\n", (unsigned long)bridge->stats.evasion_detections);
    printf("  Autoimmune: %lu\n", (unsigned long)bridge->stats.autoimmune_detections);
    printf("  Storm: %lu\n", (unsigned long)bridge->stats.storm_detections);
    printf("  Memory: %lu\n", (unsigned long)bridge->stats.memory_corruptions);
    printf("  False positives: %lu\n", (unsigned long)bridge->stats.false_positives);
    printf("  Emergency responses: %lu\n", (unsigned long)bridge->stats.emergency_responses);
    printf("  Avg update time: %.1f us\n", bridge->stats.avg_update_time_us);
    printf("=================================================\n");
}

int security_immune_fep_reset_stats(
    security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(security_immune_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int security_immune_fep_connect_bio_async(
    security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_IMMUNE_FEP,
        .module_name = "security_immune_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security immune FEP connected to bio-async");
        return 0;
    }

    NIMCP_LOGGING_WARN("Security immune FEP: bio-async connection failed");
    return -1;
}

int security_immune_fep_disconnect_bio_async(
    security_immune_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security immune FEP disconnected from bio-async");
    return 0;
}

bool security_immune_fep_is_bio_async_connected(
    const security_immune_fep_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int security_immune_fep_process_messages(
    security_immune_fep_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Process up to 16 messages per call using bio_router_process_inbox */
    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, 16);

    return (int)processed;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* immune_fep_threat_to_string(immune_fep_threat_t threat) {
    switch (threat) {
        case IMMUNE_FEP_THREAT_NONE:       return "NONE";
        case IMMUNE_FEP_THREAT_MONITOR:    return "MONITOR";
        case IMMUNE_FEP_THREAT_EVASION:    return "EVASION";
        case IMMUNE_FEP_THREAT_AUTOIMMUNE: return "AUTOIMMUNE";
        case IMMUNE_FEP_THREAT_STORM:      return "STORM";
        case IMMUNE_FEP_THREAT_MEMORY:     return "MEMORY";
        case IMMUNE_FEP_THREAT_CRITICAL:   return "CRITICAL";
        default:                            return "UNKNOWN";
    }
}

const char* immune_fep_response_to_string(immune_fep_response_t response) {
    switch (response) {
        case IMMUNE_FEP_RESPONSE_NONE:      return "NONE";
        case IMMUNE_FEP_RESPONSE_OBSERVE:   return "OBSERVE";
        case IMMUNE_FEP_RESPONSE_ALERT:     return "ALERT";
        case IMMUNE_FEP_RESPONSE_INFLAME:   return "INFLAME";
        case IMMUNE_FEP_RESPONSE_REGULATE:  return "REGULATE";
        case IMMUNE_FEP_RESPONSE_REPAIR:    return "REPAIR";
        case IMMUNE_FEP_RESPONSE_SUPPRESS:  return "SUPPRESS";
        case IMMUNE_FEP_RESPONSE_EMERGENCY: return "EMERGENCY";
        default:                             return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

static float compute_evasion_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float detection_rate
) {
    /* WHAT: Compute FE from evasion rate */
    /* WHY:  Evasion attempts generate high surprise */
    /* HOW:  Scale by precision and weight */

    float precision = bridge->state.evasion_precision;
    float weight = bridge->config.evasion_weight;

    /* FE = precision * rate * weight * threshold */
    return precision * detection_rate * weight * bridge->config.evasion_fe_threshold;
}

static float compute_autoimmune_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float attack_rate
) {
    /* WHAT: Compute FE from autoimmune rate */
    /* WHY:  Self-attack is extremely surprising */
    /* HOW:  Scale by precision and weight */

    float precision = bridge->state.autoimmune_precision;
    float weight = bridge->config.autoimmune_weight;

    /* FE = precision * rate * weight * threshold */
    return precision * attack_rate * weight * bridge->config.autoimmune_fe_threshold;
}

static float compute_storm_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float cytokine_sum
) {
    /* WHAT: Compute FE from cytokine levels */
    /* WHY:  Storm is runaway inflammation - catastrophic */
    /* HOW:  Nonlinear scaling for high cytokine levels */

    float precision = bridge->state.storm_precision;
    float weight = bridge->config.storm_weight;

    /* Normalize cytokine sum (expected max ~3.0) */
    float normalized = cytokine_sum / 3.0f;

    /* Nonlinear scaling - storms are exponentially surprising */
    float surprise_factor = normalized * normalized;

    return precision * surprise_factor * weight * bridge->config.storm_fe_threshold;
}

static float compute_memory_free_energy(
    const security_immune_fep_bridge_t* bridge,
    float corruption_rate
) {
    /* WHAT: Compute FE from memory corruption rate */
    /* WHY:  Memory failures undermine the predictive model */
    /* HOW:  Scale by precision and weight */

    float precision = bridge->state.memory_precision;
    float weight = bridge->config.memory_weight;

    return precision * corruption_rate * weight * bridge->config.memory_fe_threshold;
}

static immune_fep_threat_t classify_threat_level(
    const security_immune_fep_bridge_t* bridge,
    float total_fe
) {
    /* WHAT: Classify threat based on total FE */
    /* WHY:  Categorical output for decision making */
    /* HOW:  Compare against thresholds, check component dominance */

    /* Check for critical first */
    if (total_fe >= IMMUNE_FEP_CRITICAL_THRESHOLD) {
        return IMMUNE_FEP_THREAT_CRITICAL;
    }

    /* Check which component dominates if above threat threshold */
    if (total_fe >= IMMUNE_FEP_THREAT_THRESHOLD) {
        float max_component = fmaxf(fmaxf(bridge->fep_effects.evasion_surprise,
                                          bridge->fep_effects.autoimmune_surprise),
                                    fmaxf(bridge->fep_effects.storm_surprise,
                                          bridge->fep_effects.memory_surprise));

        if (max_component == bridge->fep_effects.storm_surprise) {
            return IMMUNE_FEP_THREAT_STORM;
        } else if (max_component == bridge->fep_effects.autoimmune_surprise) {
            return IMMUNE_FEP_THREAT_AUTOIMMUNE;
        } else if (max_component == bridge->fep_effects.memory_surprise) {
            return IMMUNE_FEP_THREAT_MEMORY;
        } else {
            return IMMUNE_FEP_THREAT_EVASION;
        }
    }

    /* Check suspicious threshold */
    if (total_fe >= IMMUNE_FEP_SUSPICIOUS_THRESHOLD) {
        return IMMUNE_FEP_THREAT_MONITOR;
    }

    return IMMUNE_FEP_THREAT_NONE;
}

static immune_fep_response_t determine_response(
    const security_immune_fep_bridge_t* bridge,
    float total_fe,
    immune_fep_threat_t threat
) {
    /* WHAT: Map threat level to response action */
    /* WHY:  Active inference for protective decisions */
    /* HOW:  Use threat classification and component analysis */

    (void)total_fe;  /* Used in active_inference function */

    switch (threat) {
        case IMMUNE_FEP_THREAT_CRITICAL:
            return IMMUNE_FEP_RESPONSE_EMERGENCY;
        case IMMUNE_FEP_THREAT_STORM:
            return IMMUNE_FEP_RESPONSE_SUPPRESS;
        case IMMUNE_FEP_THREAT_AUTOIMMUNE:
            return IMMUNE_FEP_RESPONSE_REGULATE;
        case IMMUNE_FEP_THREAT_MEMORY:
            return IMMUNE_FEP_RESPONSE_REPAIR;
        case IMMUNE_FEP_THREAT_EVASION:
            return IMMUNE_FEP_RESPONSE_INFLAME;
        case IMMUNE_FEP_THREAT_MONITOR:
            return IMMUNE_FEP_RESPONSE_OBSERVE;
        default:
            return IMMUNE_FEP_RESPONSE_NONE;
    }
}

static void update_running_averages(
    security_immune_fep_bridge_t* bridge,
    float fe,
    float surprise
) {
    /* WHAT: Update exponential moving averages */
    /* WHY:  Smooth noisy measurements */
    /* HOW:  EMA with configurable alpha */

    float alpha = FEP_SMOOTHING_ALPHA;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * fe;

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;
}

static void sync_immune_state(security_immune_fep_bridge_t* bridge) {
    /* WHAT: Sync immune cell metrics from immune system */
    /* WHY:  Keep FEP model informed of immune activity */
    /* HOW:  Read stats from brain immune system */

    if (!bridge->immune_system) {
        return;
    }

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) == 0) {
        bridge->immune_effects.active_b_cells = stats.active_b_cells;
        bridge->immune_effects.active_t_cells = stats.active_t_cells;
        bridge->immune_effects.memory_cells = stats.memory_cells;

        /* Sync cytokines from stats */
        bridge->immune_effects.cytokine_il1 = stats.cytokine_il1;
        bridge->immune_effects.cytokine_il6 = stats.cytokine_il6;
        bridge->immune_effects.cytokine_tnf = stats.cytokine_tnf;
        bridge->immune_effects.cytokine_il10 = stats.cytokine_il10;
        bridge->immune_effects.cytokine_ifn = stats.cytokine_ifn_gamma;

        bridge->immune_effects.inflammation_level = stats.inflammation_level;
    }
}
