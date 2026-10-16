/**
 * @file nimcp_metaplasticity_immune_bridge.c
 * @brief Metaplasticity-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/metaplasticity/nimcp_metaplasticity_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metaplasticity_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(metaplasticity_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get threshold modulation factor for inflammation level
 * WHY:  Map inflammation to threshold elevation
 * HOW:  Lookup table based on biological evidence
 */
static float get_inflammation_threshold_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_THETA_NONE,
        INFLAMMATION_THETA_STORM);
}

/**
 * WHAT: Get adaptation rate factor for inflammation level
 * WHY:  Map inflammation to adaptation suppression
 * HOW:  Lookup table
 */
static float get_inflammation_adapt_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_ADAPT_RATE_NONE,
        INFLAMMATION_ADAPT_RATE_STORM);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int metaplasticity_immune_default_config(metaplasticity_immune_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_default_config: config is NULL");
        return -1;
    }

    config->enable_cytokine_metaplasticity_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_instability_detection = true;
    config->enable_homeostatic_feedback = true;

    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->instability_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->base_theta_baseline = METAPLASTICITY_DEFAULT_THETA_BASELINE;
    config->base_adaptation_rate = 1.0f;
    config->base_reset_factor = 1.0f;

    config->theta_runaway_threshold = METAPLASTICITY_THETA_RUNAWAY_THRESHOLD;
    config->theta_stuck_duration_ms = METAPLASTICITY_THETA_STUCK_DURATION;
    config->reset_failure_threshold = METAPLASTICITY_RESET_FAILURE_THRESHOLD;

    return 0;
}

metaplasticity_immune_bridge_t* metaplasticity_immune_bridge_create(
    const metaplasticity_immune_config_t* config,
    brain_immune_system_t* immune_system,
    metaplasticity_controller_t metaplasticity_controller
) {
    /* Guard clauses */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("NULL immune_system in bridge create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_bridge_create: immune_system is NULL");
        return NULL;
    }
    if (!metaplasticity_controller) {
        NIMCP_LOGGING_ERROR("NULL metaplasticity_controller in bridge create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_bridge_create: metaplasticity_controller is NULL");
        return NULL;
    }

    /* Allocate bridge */
    metaplasticity_immune_bridge_t* bridge =
        (metaplasticity_immune_bridge_t*)nimcp_malloc(
            sizeof(metaplasticity_immune_bridge_t)
        );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_immune_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(metaplasticity_immune_bridge_t));

    /* Store system handles */
    bridge->immune_system = immune_system;
    bridge->metaplasticity_controller = metaplasticity_controller;

    /* Store configuration */
    if (config) {
        bridge->enable_cytokine_metaplasticity_modulation =
            config->enable_cytokine_metaplasticity_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_instability_detection = config->enable_instability_detection;
        bridge->enable_homeostatic_feedback = config->enable_homeostatic_feedback;
        bridge->base_theta_baseline = config->base_theta_baseline;
        bridge->base_adaptation_rate = config->base_adaptation_rate;
        bridge->base_reset_factor = config->base_reset_factor;
    } else {
        metaplasticity_immune_config_t default_config;
        metaplasticity_immune_default_config(&default_config);
        bridge->enable_cytokine_metaplasticity_modulation = true;
        bridge->enable_inflammation_impairment = true;
        bridge->enable_instability_detection = true;
        bridge->enable_homeostatic_feedback = true;
        bridge->base_theta_baseline = default_config.base_theta_baseline;
        bridge->base_adaptation_rate = default_config.base_adaptation_rate;
        bridge->base_reset_factor = default_config.base_reset_factor;
    }

    /* Initialize effects to baseline */
    bridge->cytokine_effects.total_threshold_modulation = 1.0f;
    bridge->cytokine_effects.total_adaptation_suppression = 1.0f;
    bridge->inflammation_state.threshold_elevation = 1.0f;

    /* Allocate mutex */
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metaplasticity_immune_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    if (nimcp_platform_mutex_init((nimcp_platform_mutex_t*)bridge->base.mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "metaplasticity_immune_bridge_create: mutex init failed");
        nimcp_free(bridge->base.mutex);
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created metaplasticity-immune bridge");

    return bridge;
}

void metaplasticity_immune_bridge_destroy(metaplasticity_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        metaplasticity_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed metaplasticity-immune bridge");
}

/* ============================================================================
 * Immune → Metaplasticity API
 * ============================================================================ */

int metaplasticity_immune_apply_cytokine_effects(
    metaplasticity_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in apply_cytokine_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_apply_cytokine_effects: bridge is NULL");
        return -1;
    }

    /* Skip if disabled */
    if (!bridge->enable_cytokine_metaplasticity_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Query cytokine levels from immune system */
    float il1 = 0.0f, il6 = 0.0f, tnf = 0.0f, ifn = 0.0f, il10 = 0.0f;

    /* Get cytokine concentrations (placeholder - actual API depends on brain_immune) */
    /* In real implementation, query brain_immune_get_cytokine_level() */

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_threshold_elevation =
        1.0f + (CYTOKINE_IL1_THRESHOLD_ELEVATION - 1.0f) * il1;
    bridge->cytokine_effects.il6_threshold_elevation =
        1.0f + (CYTOKINE_IL6_THRESHOLD_ELEVATION - 1.0f) * il6;
    bridge->cytokine_effects.tnf_threshold_elevation =
        1.0f + (CYTOKINE_TNF_THRESHOLD_ELEVATION - 1.0f) * tnf;
    bridge->cytokine_effects.ifn_gamma_threshold_elevation =
        1.0f + (CYTOKINE_IFN_GAMMA_THRESHOLD_ELEVATION - 1.0f) * ifn;

    /* IL-10 restoration (lowers threshold) */
    bridge->cytokine_effects.il10_threshold_restoration =
        1.0f - (1.0f - CYTOKINE_IL10_THRESHOLD_RESTORATION) * il10;

    /* Compute total modulation */
    float pro_inflammatory =
        bridge->cytokine_effects.il1_threshold_elevation *
        bridge->cytokine_effects.il6_threshold_elevation *
        bridge->cytokine_effects.tnf_threshold_elevation *
        bridge->cytokine_effects.ifn_gamma_threshold_elevation;

    float anti_inflammatory = bridge->cytokine_effects.il10_threshold_restoration;

    bridge->cytokine_effects.total_threshold_modulation =
        pro_inflammatory * anti_inflammatory;

    /* Adaptation suppression proportional to threshold elevation */
    float suppression_factor = 1.0f / bridge->cytokine_effects.total_threshold_modulation;
    bridge->cytokine_effects.total_adaptation_suppression = suppression_factor;

    /* Baseline reset impairment */
    bridge->cytokine_effects.baseline_reset_impairment =
        1.0f - (bridge->cytokine_effects.total_threshold_modulation - 1.0f) * 0.5f;
    if (bridge->cytokine_effects.baseline_reset_impairment < 0.0f) {
        bridge->cytokine_effects.baseline_reset_impairment = 0.0f;
    }

    bridge->cytokine_modulations++;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cytokine effects: threshold×%.2f, adaptation×%.2f",
                       bridge->cytokine_effects.total_threshold_modulation,
                       bridge->cytokine_effects.total_adaptation_suppression);

    return 0;
}

int metaplasticity_immune_apply_inflammation_effects(
    metaplasticity_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in apply_inflammation_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_apply_inflammation_effects: bridge is NULL");
        return -1;
    }

    /* Skip if disabled */
    if (!bridge->enable_inflammation_impairment) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Get current inflammation level from immune system */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    /* In real implementation: level = brain_immune_get_inflammation_level(bridge->immune_system); */

    bridge->inflammation_state.current_level = level;

    /* Compute threshold elevation */
    float base_elevation = get_inflammation_threshold_factor(level);
    bridge->inflammation_state.threshold_elevation = base_elevation;

    /* Compute adaptation rate suppression */
    float base_adapt = get_inflammation_adapt_factor(level);
    bridge->inflammation_state.adaptation_rate_suppression = base_adapt;

    /* Baseline reset impairment (proportional to inflammation) */
    bridge->inflammation_state.baseline_reset_impairment =
        1.0f - (bridge->inflammation_state.threshold_elevation - 1.0f) * 0.3f;
    if (bridge->inflammation_state.baseline_reset_impairment < 0.0f) {
        bridge->inflammation_state.baseline_reset_impairment = 0.0f;
    }

    /* Update duration (simplified - would track actual time) */
    if (level != INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_sec += 1.0f;
    } else {
        bridge->inflammation_state.inflammation_duration_sec = 0.0f;
    }

    /* Check for chronic inflammation */
    bridge->inflammation_state.is_chronic =
        (bridge->inflammation_state.inflammation_duration_sec >=
         CHRONIC_INFLAMMATION_THRESHOLD_SEC);

    /* Chronic effects */
    if (bridge->inflammation_state.is_chronic) {
        bridge->inflammation_state.threshold_homeostasis_loss = 0.7f;
        bridge->inflammation_state.adaptive_capacity_loss = 0.5f;
        bridge->inflammation_state.metaplastic_range_reduction = 0.6f;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Inflammation effects: level=%d, threshold×%.2f, adapt×%.2f",
                       level, bridge->inflammation_state.threshold_elevation,
                       bridge->inflammation_state.adaptation_rate_suppression);

    return 0;
}

float metaplasticity_immune_get_effective_threshold(
    const metaplasticity_immune_bridge_t* bridge,
    float base_theta
) {
    if (!bridge) return base_theta;

    /* Combine cytokine and inflammation effects */
    float modulation = bridge->cytokine_effects.total_threshold_modulation *
                       bridge->inflammation_state.threshold_elevation;

    return base_theta * modulation;
}

int metaplasticity_immune_get_modulation_state(
    const metaplasticity_immune_bridge_t* bridge,
    metaplasticity_modulation_state_t* modulation
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in get_modulation_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_modulation_state: bridge is NULL");
        return -1;
    }
    if (!modulation) {
        NIMCP_LOGGING_ERROR("NULL modulation in get_modulation_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_modulation_state: modulation is NULL");
        return -1;
    }

    /* Compute modulation factors */
    modulation->threshold_modulation =
        bridge->cytokine_effects.total_threshold_modulation *
        bridge->inflammation_state.threshold_elevation;

    modulation->adaptation_rate_modulation =
        bridge->cytokine_effects.total_adaptation_suppression *
        bridge->inflammation_state.adaptation_rate_suppression;

    modulation->baseline_reset_modulation =
        bridge->cytokine_effects.baseline_reset_impairment *
        bridge->inflammation_state.baseline_reset_impairment;

    /* Neuromodulator blocking (inflammation overrides DA/NE) */
    modulation->neuromodulator_block =
        1.0f - bridge->inflammation_state.adaptation_rate_suppression;

    /* Compute effective parameters */
    modulation->effective_theta_baseline =
        bridge->base_theta_baseline * modulation->threshold_modulation;

    modulation->effective_adaptation_rate =
        bridge->base_adaptation_rate * modulation->adaptation_rate_modulation;

    modulation->effective_reset_factor =
        bridge->base_reset_factor * modulation->baseline_reset_modulation;

    return 0;
}

int metaplasticity_immune_restore_metaplasticity(
    metaplasticity_immune_bridge_t* bridge,
    float recovery_factor
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in restore_metaplasticity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_restore_metaplasticity: bridge is NULL");
        return -1;
    }

    /* Clamp recovery factor */
    if (recovery_factor < 0.0f || recovery_factor > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_immune_restore_metaplasticity: recovery_factor out of range");
        return -1;
    }
    if (recovery_factor < 0.0f) recovery_factor = 0.0f;
    if (recovery_factor > 1.0f) recovery_factor = 1.0f;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Interpolate threshold modulation back to baseline */
    float current_mod = bridge->cytokine_effects.total_threshold_modulation;
    bridge->cytokine_effects.total_threshold_modulation =
        current_mod * (1.0f - recovery_factor) + 1.0f * recovery_factor;

    /* Restore adaptation rate */
    float current_adapt = bridge->cytokine_effects.total_adaptation_suppression;
    bridge->cytokine_effects.total_adaptation_suppression =
        current_adapt * (1.0f - recovery_factor) + 1.0f * recovery_factor;

    bridge->threshold_restorations++;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    NIMCP_LOGGING_INFO("Restored metaplasticity: recovery=%.2f", recovery_factor);

    return 0;
}

/* ============================================================================
 * Metaplasticity → Immune API
 * ============================================================================ */

int metaplasticity_immune_detect_instability(
    metaplasticity_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in detect_instability");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_detect_instability: bridge is NULL");
        return -1;
    }

    /* Skip if disabled */
    if (!bridge->enable_instability_detection) {
        return 0;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);

    /* Get metaplasticity statistics */
    metaplasticity_stats_t stats;
    if (metaplasticity_controller_get_stats(bridge->metaplasticity_controller,
                                           &stats) != 0) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metaplasticity_immune_detect_instability: operation failed");
        return -1;
    }

    /* Compute threshold ratio */
    if (stats.mean_theta_baseline > 0.0f) {
        bridge->instability_state.current_theta_ratio =
            stats.mean_theta_effective / stats.mean_theta_baseline;
    } else {
        bridge->instability_state.current_theta_ratio = 1.0f;
    }

    /* Detect runaway threshold */
    bridge->instability_state.theta_runaway_detected =
        (bridge->instability_state.current_theta_ratio >
         METAPLASTICITY_THETA_RUNAWAY_THRESHOLD);

    /* Detect stuck threshold (simplified) */
    bridge->instability_state.theta_stuck_detected =
        (bridge->instability_state.current_theta_ratio > 2.5f ||
         bridge->instability_state.current_theta_ratio < 0.4f);

    /* Detect reset failure */
    bridge->instability_state.reset_failure_detected =
        (stats.mean_sleep_reset < METAPLASTICITY_RESET_FAILURE_THRESHOLD);

    /* Overall homeostatic threat */
    bridge->instability_state.homeostatic_threat =
        bridge->instability_state.theta_runaway_detected ||
        bridge->instability_state.theta_stuck_detected ||
        bridge->instability_state.reset_failure_detected;

    /* Compute severity */
    float severity = 0.0f;
    if (bridge->instability_state.theta_runaway_detected) severity += 0.4f;
    if (bridge->instability_state.theta_stuck_detected) severity += 0.3f;
    if (bridge->instability_state.reset_failure_detected) severity += 0.3f;
    bridge->instability_state.instability_severity = severity;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    if (bridge->instability_state.homeostatic_threat) {
        NIMCP_LOGGING_WARN("Metaplasticity instability detected: ratio=%.2f, severity=%.2f",
                          bridge->instability_state.current_theta_ratio,
                          bridge->instability_state.instability_severity);
    }

    return 0;
}

int metaplasticity_immune_alert_instability(
    metaplasticity_immune_bridge_t* bridge,
    uint32_t* antigen_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in alert_instability");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_alert_instability: bridge is NULL");
        return -1;
    }
    if (!antigen_id) {
        NIMCP_LOGGING_ERROR("NULL antigen_id in alert_instability");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_alert_instability: antigen_id is NULL");
        return -1;
    }

    /* Only alert if instability detected */
    if (!bridge->instability_state.homeostatic_threat) {
        *antigen_id = 0;
        return 0;
    }

    /* Create antigen signature from instability */
    uint8_t epitope[16];
    memset(epitope, 0, sizeof(epitope));

    /* Encode instability type in epitope */
    epitope[0] = 0xAE;  // Metaplasticity marker
    epitope[1] = (uint8_t)(bridge->instability_state.instability_severity * 255);
    epitope[2] = bridge->instability_state.theta_runaway_detected ? 1 : 0;
    epitope[3] = bridge->instability_state.theta_stuck_detected ? 1 : 0;
    epitope[4] = bridge->instability_state.reset_failure_detected ? 1 : 0;

    /* Present to immune system */
    float severity = bridge->instability_state.instability_severity * 10.0f;
    /* In real implementation: brain_immune_present_antigen() */

    bridge->instability_alerts++;

    NIMCP_LOGGING_WARN("Alerted immune system of metaplasticity instability");

    return 0;
}

int metaplasticity_immune_signal_healthy_homeostasis(
    metaplasticity_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in signal_healthy_homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_signal_healthy_homeostasis: bridge is NULL");
        return -1;
    }

    /* Skip if disabled */
    if (!bridge->enable_homeostatic_feedback) {
        return 0;
    }

    /* Only signal if healthy */
    if (bridge->instability_state.homeostatic_threat) {
        return 0;
    }

    /* Check if threshold is near baseline */
    if (fabsf(bridge->instability_state.current_theta_ratio - 1.0f) < 0.2f) {
        /* Request IL-10 release (anti-inflammatory) */
        /* In real implementation: brain_immune_request_il10() */

        NIMCP_LOGGING_DEBUG("Signaled healthy metaplasticity homeostasis");
    }

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int metaplasticity_immune_bridge_update(
    metaplasticity_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in bridge_update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_bridge_update: bridge is NULL");
        return -1;
    }

    /* Apply immune → metaplasticity effects */
    metaplasticity_immune_apply_cytokine_effects(bridge);
    metaplasticity_immune_apply_inflammation_effects(bridge);

    /* Detect metaplasticity → immune feedback */
    metaplasticity_immune_detect_instability(bridge);

    /* Signal if appropriate */
    if (bridge->instability_state.homeostatic_threat) {
        uint32_t antigen_id;
        metaplasticity_immune_alert_instability(bridge, &antigen_id);
    } else {
        metaplasticity_immune_signal_healthy_homeostasis(bridge);
    }

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int metaplasticity_immune_get_cytokine_effects(
    const metaplasticity_immune_bridge_t* bridge,
    cytokine_metaplasticity_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_cytokine_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_cytokine_effects: effects is NULL");
        return -1;
    }
    *effects = bridge->cytokine_effects;
    return 0;
}

int metaplasticity_immune_get_inflammation_state(
    const metaplasticity_immune_bridge_t* bridge,
    inflammation_metaplasticity_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_inflammation_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_inflammation_state: state is NULL");
        return -1;
    }
    *state = bridge->inflammation_state;
    return 0;
}

int metaplasticity_immune_get_instability_state(
    const metaplasticity_immune_bridge_t* bridge,
    metaplasticity_instability_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_instability_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metaplasticity_immune_get_instability_state: state is NULL");
        return -1;
    }
    *state = bridge->instability_state;
    return 0;
}

bool metaplasticity_immune_is_impaired(const metaplasticity_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->cytokine_effects.total_adaptation_suppression < 1.0f;
}

float metaplasticity_immune_get_threshold_elevation(
    const metaplasticity_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return (bridge->cytokine_effects.total_threshold_modulation - 1.0f) * 100.0f;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int metaplasticity_immune_connect_bio_async(metaplasticity_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_METAPLASTICITY,
        .module_name = "metaplasticity_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected metaplasticity immune bridge to bio-async router");
    }
    return 0;
}

int metaplasticity_immune_disconnect_bio_async(metaplasticity_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected metaplasticity immune bridge from bio-async");
    return 0;
}

bool metaplasticity_immune_is_bio_async_connected(
    const metaplasticity_immune_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
