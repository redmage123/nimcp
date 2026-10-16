/**
 * @file nimcp_synaptic_scaling_immune_bridge.c
 * @brief Synaptic Scaling-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and synaptic scaling
 * WHY:  Biological realism - TNF-α regulates AMPA receptor trafficking and scaling
 * HOW:  Monitor cytokine levels to modulate scaling, monitor scaling to trigger immune
 */

#include "plasticity/immune/nimcp_synaptic_scaling_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synaptic_scaling_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(synaptic_scaling_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get TNF-α concentration from immune system
 *
 * WHAT: Query brain immune system for TNF-α level
 * WHY:  TNF-α directly modulates synaptic scaling
 * HOW:  Search cytokine pool for TNF-α, return concentration
 */
static float get_tnf_alpha_concentration(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Iterate through active cytokines to find TNF-α */
    float tnf_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        const brain_cytokine_t* cyt = &immune->cytokines[i];
        if (cyt->type == BRAIN_CYTOKINE_TNF && !cyt->delivered) {
            tnf_concentration += cyt->concentration;
        }
    }

    return nimcp_clampf(tnf_concentration, 0.0f, 1.0f);
}

/**
 * @brief Get IL-1β concentration from immune system
 *
 * WHAT: Query brain immune system for IL-1β level
 * WHY:  IL-1β affects plasticity thresholds
 * HOW:  Search cytokine pool for IL-1β, return concentration
 */
static float get_il1_beta_concentration(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    float il1_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        const brain_cytokine_t* cyt = &immune->cytokines[i];
        if (cyt->type == BRAIN_CYTOKINE_IL1 && !cyt->delivered) {
            il1_concentration += cyt->concentration;
        }
    }

    return nimcp_clampf(il1_concentration, 0.0f, 1.0f);
}

/**
 * @brief Get IL-10 concentration from immune system
 *
 * WHAT: Query brain immune system for IL-10 level
 * WHY:  IL-10 restores normal scaling
 * HOW:  Search cytokine pool for IL-10, return concentration
 */
static float get_il10_concentration(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    float il10_concentration = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        const brain_cytokine_t* cyt = &immune->cytokines[i];
        if (cyt->type == BRAIN_CYTOKINE_IL10 && !cyt->delivered) {
            il10_concentration += cyt->concentration;
        }
    }

    return nimcp_clampf(il10_concentration, 0.0f, 1.0f);
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines scaling impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune || immune->inflammation_count == 0) {
        return INFLAMMATION_NONE;
    }

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        const brain_inflammation_site_t* site = &immune->inflammation_sites[i];
        if (site->level > max_level) {
            max_level = site->level;
        }
    }

    return max_level;
}

/**
 * @brief Compute inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation has different effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune || immune->inflammation_count == 0) {
        return 0.0f;
    }

    uint64_t oldest_time = UINT64_MAX;
    uint64_t current_time = immune->start_time; /* Would use actual time */

    for (size_t i = 0; i < immune->inflammation_count; i++) {
        const brain_inflammation_site_t* site = &immune->inflammation_sites[i];
        if (site->start_time < oldest_time) {
            oldest_time = site->start_time;
        }
    }

    if (oldest_time == UINT64_MAX) return 0.0f;

    float duration_ms = (float)(current_time - oldest_time);
    return duration_ms / 1000.0f; /* Convert to seconds */
}

/**
 * @brief Map TNF-α concentration to scaling factor
 *
 * WHAT: Convert TNF-α level to scaling multiplier
 * WHY:  TNF-α enhances AMPA receptor trafficking (Stellwagen & Malenka 2006)
 * HOW:  Piecewise linear mapping based on concentration ranges
 */
static float map_tnf_to_scaling_factor(float tnf_concentration, float sensitivity) {
    /* Guard: no TNF-α = baseline */
    if (tnf_concentration <= 0.0f) {
        return TNF_ALPHA_SCALING_BASELINE;
    }

    /* Low physiological range [0.0-0.2] → 1.0-1.2x */
    if (tnf_concentration <= 0.2f) {
        float t = tnf_concentration / 0.2f;
        return 1.0f + (0.2f * t * sensitivity);
    }

    /* Moderate range [0.2-0.5] → 1.2-1.8x */
    if (tnf_concentration <= 0.5f) {
        float t = (tnf_concentration - 0.2f) / 0.3f;
        return 1.2f + (0.6f * t * sensitivity);
    }

    /* High range [0.5-0.8] → 1.8-2.5x */
    if (tnf_concentration <= 0.8f) {
        float t = (tnf_concentration - 0.5f) / 0.3f;
        return 1.8f + (0.7f * t * sensitivity);
    }

    /* Excessive range [0.8-1.0] → 2.5-3.5x (pathological) */
    float t = (tnf_concentration - 0.8f) / 0.2f;
    return 2.5f + (1.0f * t * sensitivity);
}

/**
 * @brief Map inflammation level to scaling rate multiplier
 *
 * WHAT: Convert inflammation level to rate factor
 * WHY:  Inflammation accelerates scaling dynamics
 * HOW:  Discrete mapping per inflammation level
 */
static float map_inflammation_to_rate(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_SCALING_RATE_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_SCALING_RATE_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_SCALING_RATE_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SCALING_RATE_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_SCALING_RATE_STORM;
        default:                    return INFLAMMATION_SCALING_RATE_NONE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int synaptic_scaling_immune_default_config(synaptic_scaling_immune_config_t* config) {
    /* Guard: validate input */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_tnf_scaling_modulation = true;
    config->enable_il1_threshold_modulation = true;
    config->enable_inflammation_rate_modulation = true;
    config->enable_aberrance_detection = true;
    config->enable_recovery_tracking = true;
    config->enable_il10_restoration = true;

    /* Biologically-based default sensitivities */
    config->tnf_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->aberrance_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->recovery_threshold = SCALING_RECOVERY_THRESHOLD;

    /* Evidence-based thresholds */
    config->aberrant_scale_up_threshold = SCALING_ABERRANT_THRESHOLD;
    config->aberrant_scale_down_threshold = SCALING_HYPOACTIVITY_THRESHOLD;
    config->instability_variance_threshold = SCALING_INSTABILITY_THRESHOLD;

    return 0;
}

synaptic_scaling_immune_bridge_t* synaptic_scaling_immune_bridge_create(
    const synaptic_scaling_immune_config_t* config,
    brain_immune_system_t* immune_system,
    homeostatic_controller_t homeostatic_controller
) {
    /* Guard: require immune system */
    if (!immune_system) {
        LOG_MODULE_ERROR("synaptic_scaling_immune_bridge",
                  "Cannot create bridge without immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    /* Allocate bridge */
    synaptic_scaling_immune_bridge_t* bridge = (synaptic_scaling_immune_bridge_t*)
        nimcp_malloc(sizeof(synaptic_scaling_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("synaptic_scaling_immune_bridge",
                  "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(synaptic_scaling_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->homeostatic_controller = homeostatic_controller;

    /* Apply configuration or defaults */
    if (config) {
        bridge->enable_tnf_scaling_modulation = config->enable_tnf_scaling_modulation;
        bridge->enable_il1_threshold_modulation = config->enable_il1_threshold_modulation;
        bridge->enable_inflammation_rate_modulation = config->enable_inflammation_rate_modulation;
        bridge->enable_aberrance_detection = config->enable_aberrance_detection;
        bridge->enable_recovery_tracking = config->enable_recovery_tracking;
        bridge->enable_il10_restoration = config->enable_il10_restoration;

        bridge->tnf_sensitivity = config->tnf_sensitivity;
        bridge->aberrance_sensitivity = config->aberrance_sensitivity;
        bridge->recovery_threshold = config->recovery_threshold;
    } else {
        /* Use defaults */
        synaptic_scaling_immune_config_t default_config;
        synaptic_scaling_immune_default_config(&default_config);

        bridge->enable_tnf_scaling_modulation = default_config.enable_tnf_scaling_modulation;
        bridge->enable_il1_threshold_modulation = default_config.enable_il1_threshold_modulation;
        bridge->enable_inflammation_rate_modulation = default_config.enable_inflammation_rate_modulation;
        bridge->enable_aberrance_detection = default_config.enable_aberrance_detection;
        bridge->enable_recovery_tracking = default_config.enable_recovery_tracking;
        bridge->enable_il10_restoration = default_config.enable_il10_restoration;

        bridge->tnf_sensitivity = default_config.tnf_sensitivity;
        bridge->aberrance_sensitivity = default_config.aberrance_sensitivity;
        bridge->recovery_threshold = default_config.recovery_threshold;
    }

    /* Initialize state to normal */
    bridge->tnf_effects.scaling_factor_modulation = TNF_ALPHA_SCALING_BASELINE;
    bridge->tnf_effects.receptor_surface_density = 1.0f;
    bridge->tnf_effects.effective_scaling_rate = 1.0f;
    bridge->tnf_effects.homeostatic_set_point = 1.0f;

    bridge->il1_effects.ltp_threshold_modulation = IL1_BETA_LTP_THRESHOLD_NORMAL;
    bridge->il1_effects.ltd_threshold_modulation = 1.0f;
    bridge->il1_effects.plasticity_rate_modulation = 1.0f;

    bridge->inflammation_state.scaling_rate_multiplier = 1.0f;
    bridge->inflammation_state.excitation_inhibition_balance = 0.5f; /* Balanced */
    bridge->inflammation_state.network_stability = 1.0f;

    bridge->aberrance.scaling_factor_mean = 1.0f;

    bridge->recovery.baseline_scaling_factor = 1.0f;
    bridge->recovery.target_scaling_factor = 1.0f;

    /* Create mutex for thread safety */
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (mutex) {
        nimcp_mutex_init(mutex, NULL);
        bridge->base.mutex = mutex;
    }

    LOG_MODULE_INFO("synaptic_scaling_immune_bridge",
                  "Bridge created successfully");

    return bridge;
}

void synaptic_scaling_immune_bridge_destroy(synaptic_scaling_immune_bridge_t* bridge) {
    /* Guard: null check */
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Synaptic Scaling Implementation
 * ============================================================================ */

int synaptic_scaling_immune_apply_tnf_effects(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_tnf_scaling_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_apply_tnf_effects: required parameter is NULL (bridge, bridge->enable_tnf_scaling_modulation)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get TNF-α concentration */
    float tnf_conc = get_tnf_alpha_concentration(bridge->immune_system);
    bridge->tnf_effects.tnf_alpha_concentration = tnf_conc;

    /* Compute scaling factor modulation */
    float scaling_mod = map_tnf_to_scaling_factor(tnf_conc, bridge->tnf_sensitivity);
    bridge->tnf_effects.scaling_factor_modulation = scaling_mod;

    /* TNF-α enhances AMPA receptor surface expression */
    /* Linear relationship: more TNF-α → more surface receptors */
    bridge->tnf_effects.ampa_receptor_trafficking = nimcp_clampf(
        0.5f + (tnf_conc * 0.5f), 0.0f, 1.0f
    );

    /* Surface receptor density tracks trafficking rate */
    bridge->tnf_effects.receptor_surface_density = nimcp_clampf(
        0.7f + (tnf_conc * 0.3f), 0.5f, 1.0f
    );

    /* Receptor insertion rate increases with TNF-α */
    bridge->tnf_effects.receptor_insertion_rate = tnf_conc;
    bridge->tnf_effects.receptor_internalization_rate = nimcp_clampf(
        0.5f - (tnf_conc * 0.3f), 0.1f, 0.5f
    );

    /* Effective scaling rate is boosted by TNF-α */
    bridge->tnf_effects.effective_scaling_rate = scaling_mod;

    /* High TNF-α shifts homeostatic set point higher */
    bridge->tnf_effects.homeostatic_set_point = nimcp_clampf(
        1.0f + (tnf_conc * 0.3f), 1.0f, 1.5f
    );

    /* Update statistics */
    bridge->tnf_modulations++;

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int synaptic_scaling_immune_apply_il1_effects(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_il1_threshold_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_apply_il1_effects: required parameter is NULL (bridge, bridge->enable_il1_threshold_modulation)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get IL-1β concentration */
    float il1_conc = get_il1_beta_concentration(bridge->immune_system);
    bridge->il1_effects.il1_beta_concentration = il1_conc;

    /* IL-1β elevates LTP threshold (makes LTP harder) */
    if (il1_conc <= 0.3f) {
        bridge->il1_effects.ltp_threshold_modulation = IL1_BETA_LTP_THRESHOLD_NORMAL;
    } else if (il1_conc <= 0.6f) {
        bridge->il1_effects.ltp_threshold_modulation = IL1_BETA_LTP_THRESHOLD_ELEVATED;
    } else {
        bridge->il1_effects.ltp_threshold_modulation = IL1_BETA_LTP_THRESHOLD_HIGH;
    }

    /* IL-1β also affects LTD (slightly reduces) */
    bridge->il1_effects.ltd_threshold_modulation = nimcp_clampf(
        1.0f - (il1_conc * 0.2f), 0.8f, 1.0f
    );

    /* Overall plasticity rate is reduced by IL-1β */
    bridge->il1_effects.plasticity_rate_modulation = nimcp_clampf(
        1.0f - (il1_conc * 0.3f), 0.7f, 1.0f
    );

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int synaptic_scaling_immune_apply_inflammation_effects(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_inflammation_rate_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->enable_inflammation_rate_modulation)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);

    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic = (duration > 1800.0f); /* 30 min = chronic */

    /* Map inflammation to scaling rate multiplier */
    bridge->inflammation_state.scaling_rate_multiplier = map_inflammation_to_rate(level);

    /* Chronic inflammation increases risk of aberrant scaling */
    if (bridge->inflammation_state.is_chronic) {
        bridge->inflammation_state.scaling_aberrance_risk = nimcp_clampf(
            0.3f + (duration / 3600.0f) * 0.5f, 0.0f, 0.9f
        );
        bridge->inflammation_state.homeostatic_impairment = nimcp_clampf(
            (duration / 7200.0f), 0.0f, 0.8f
        );
    } else {
        bridge->inflammation_state.scaling_aberrance_risk = 0.1f;
        bridge->inflammation_state.homeostatic_impairment = 0.0f;
    }

    /* Systemic/storm inflammation disrupts E/I balance */
    if (level >= INFLAMMATION_SYSTEMIC) {
        /* Shift toward excitation */
        bridge->inflammation_state.excitation_inhibition_balance = nimcp_clampf(
            0.5f + (float)(level - INFLAMMATION_SYSTEMIC) * 0.2f, 0.0f, 1.0f
        );
        bridge->inflammation_state.network_stability = nimcp_clampf(
            1.0f - (float)(level - INFLAMMATION_SYSTEMIC) * 0.3f, 0.2f, 1.0f
        );
    } else {
        bridge->inflammation_state.excitation_inhibition_balance = 0.5f;
        bridge->inflammation_state.network_stability = 1.0f;
    }

    /* Check for runaway conditions */
    bridge->inflammation_state.runaway_excitation =
        (bridge->inflammation_state.excitation_inhibition_balance > 0.75f);
    bridge->inflammation_state.global_silencing =
        (bridge->inflammation_state.excitation_inhibition_balance < 0.25f);

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float synaptic_scaling_immune_compute_tnf_modulation(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) return TNF_ALPHA_SCALING_BASELINE;

    return bridge->tnf_effects.scaling_factor_modulation;
}

int synaptic_scaling_immune_restore_from_il10(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_il10_restoration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_restore_from_il10: required parameter is NULL (bridge, bridge->enable_il10_restoration)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get IL-10 concentration */
    float il10_conc = get_il10_concentration(bridge->immune_system);

    /* IL-10 restores normal scaling (reduces TNF-α effects) */
    if (il10_conc > 0.3f) {
        /* Gradually restore to baseline */
        float restoration_factor = nimcp_clampf((il10_conc - 0.3f) / 0.7f, 0.0f, 1.0f);

        /* Restore scaling factor toward baseline */
        float current = bridge->tnf_effects.scaling_factor_modulation;
        float target = TNF_ALPHA_SCALING_BASELINE;
        bridge->tnf_effects.scaling_factor_modulation =
            current + (target - current) * restoration_factor * 0.1f;

        /* Restore receptor density */
        bridge->tnf_effects.receptor_surface_density =
            bridge->tnf_effects.receptor_surface_density +
            (1.0f - bridge->tnf_effects.receptor_surface_density) * restoration_factor * 0.1f;

        /* Restore homeostatic set point */
        bridge->tnf_effects.homeostatic_set_point =
            bridge->tnf_effects.homeostatic_set_point +
            (1.0f - bridge->tnf_effects.homeostatic_set_point) * restoration_factor * 0.1f;

        /* Track recovery */
        if (bridge->enable_recovery_tracking) {
            bridge->recovery.in_recovery = true;
            bridge->recovery.recovery_progress = restoration_factor;
            bridge->recovery.il10_release_rate = il10_conc;
        }
    }

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Synaptic Scaling → Immune Implementation
 * ============================================================================ */

int synaptic_scaling_immune_detect_aberrance(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_aberrance_detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_detect_aberrance: required parameter is NULL (bridge, bridge->enable_aberrance_detection)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get effective scaling factor */
    float scaling_factor = bridge->tnf_effects.scaling_factor_modulation;

    /* Detect excessive scale-up */
    bridge->aberrance.excessive_scale_up =
        (scaling_factor > SCALING_ABERRANT_THRESHOLD * bridge->aberrance_sensitivity);

    /* Detect excessive scale-down (hypoactivity) */
    bridge->aberrance.excessive_scale_down =
        (scaling_factor < SCALING_HYPOACTIVITY_THRESHOLD);

    /* Compute variance (simplified - would track history in real implementation) */
    bridge->aberrance.scaling_factor_mean = scaling_factor;
    bridge->aberrance.scaling_factor_variance = 0.0f; /* Would compute from history */

    /* Detect oscillations based on variance */
    bridge->aberrance.scaling_oscillations =
        (bridge->aberrance.scaling_factor_variance > SCALING_INSTABILITY_THRESHOLD);

    /* Homeostatic failure if aberrant for too long */
    bridge->aberrance.homeostatic_failure =
        (bridge->aberrance.excessive_scale_up || bridge->aberrance.excessive_scale_down) &&
        (bridge->aberrance.time_since_last_stable_sec > 600.0f); /* 10 min */

    /* Compute overall severity */
    float severity = 0.0f;
    if (bridge->aberrance.excessive_scale_up) severity += 0.3f;
    if (bridge->aberrance.excessive_scale_down) severity += 0.3f;
    if (bridge->aberrance.scaling_oscillations) severity += 0.2f;
    if (bridge->aberrance.homeostatic_failure) severity += 0.2f;
    bridge->aberrance.severity = nimcp_clampf(severity, 0.0f, 1.0f);

    /* Update statistics */
    if (bridge->aberrance.severity > 0.3f) {
        bridge->aberrance_detections++;
    }

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int synaptic_scaling_immune_trigger_from_aberrance(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_trigger_from_aberrance: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Guard: only trigger if aberrant */
    if (!synaptic_scaling_immune_is_aberrant(bridge)) return 0;

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Guard: avoid repeat triggers */
    if (bridge->aberrance.immune_triggered) {
        if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Create epitope from scaling state */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode aberrance pattern into epitope */
    float* epitope_data = (float*)epitope;
    epitope_data[0] = bridge->aberrance.scaling_factor_mean;
    epitope_data[1] = bridge->aberrance.scaling_factor_variance;
    epitope_data[2] = bridge->aberrance.severity;

    /* Determine severity for antigen (1-10 scale) */
    uint32_t severity = (uint32_t)(bridge->aberrance.severity * 10.0f);
    if (severity < 1) severity = 1;
    if (severity > 10) severity = 10;

    /* Present antigen to immune system */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(float) * 3,
        severity,
        0, /* node ID not applicable */
        &antigen_id
    );

    if (result == 0) {
        bridge->aberrance.immune_triggered = true;
        bridge->aberrance.trigger_count++;
        bridge->immune_triggers++;

        LOG_MODULE_INFO("synaptic_scaling_immune_bridge",
                  "Triggered immune response from aberrant scaling (severity=%u, antigen=%u)",
                  severity, antigen_id);
    }

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

int synaptic_scaling_immune_signal_recovery(
    synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge || !bridge->enable_recovery_tracking) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_signal_recovery: required parameter is NULL (bridge, bridge->enable_recovery_tracking)");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Check if scaling is near normal */
    float current_factor = bridge->tnf_effects.scaling_factor_modulation;
    bool near_normal = (fabsf(current_factor - 1.0f) < 0.2f);

    if (near_normal) {
        /* Accumulate stable time */
        bridge->recovery.time_stable_sec += 0.1f; /* Approximate delta */

        /* Check if stable for sufficient duration */
        if (bridge->recovery.time_stable_sec >= SCALING_STABILITY_DURATION_SEC) {
            if (!bridge->recovery.recovery_complete) {
                /* Signal recovery by releasing IL-10 */
                uint32_t cytokine_id;
                brain_immune_release_cytokine(
                    bridge->immune_system,
                    BRAIN_CYTOKINE_IL10,
                    0, /* No specific source cell */
                    0.5f, /* Moderate concentration */
                    0, /* Broadcast */
                    &cytokine_id
                );

                bridge->recovery.recovery_complete = true;
                bridge->recoveries_completed++;

                /* Reset aberrance trigger flag */
                bridge->aberrance.immune_triggered = false;

                LOG_MODULE_INFO("synaptic_scaling_immune_bridge",
                  "Scaling recovered, released IL-10 (cytokine=%u)", cytokine_id);
            }
        }
    } else {
        /* Reset stable time if not near normal */
        bridge->recovery.time_stable_sec = 0.0f;
        bridge->recovery.recovery_complete = false;
    }

    /* Update recovery progress */
    bridge->recovery.current_scaling_factor = current_factor;
    bridge->recovery.recovery_progress = nimcp_clampf(
        1.0f - fabsf(current_factor - 1.0f) / 2.5f, 0.0f, 1.0f
    );

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool synaptic_scaling_immune_check_runaway_excitation(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) {
        return false;
    }

    return bridge->inflammation_state.runaway_excitation;
}

bool synaptic_scaling_immune_check_global_silencing(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) {
        return false;
    }

    return bridge->inflammation_state.global_silencing;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int synaptic_scaling_immune_bridge_update(
    synaptic_scaling_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard: validate input */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update statistics */
    bridge->total_updates++;

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* IMMUNE → SCALING: Apply cytokine effects */
    if (bridge->enable_tnf_scaling_modulation) {
        synaptic_scaling_immune_apply_tnf_effects(bridge);
    }

    if (bridge->enable_il1_threshold_modulation) {
        synaptic_scaling_immune_apply_il1_effects(bridge);
    }

    if (bridge->enable_inflammation_rate_modulation) {
        synaptic_scaling_immune_apply_inflammation_effects(bridge);
    }

    if (bridge->enable_il10_restoration) {
        synaptic_scaling_immune_restore_from_il10(bridge);
    }

    /* SCALING → IMMUNE: Detect aberrance and trigger if needed */
    if (bridge->enable_aberrance_detection) {
        synaptic_scaling_immune_detect_aberrance(bridge);
        synaptic_scaling_immune_trigger_from_aberrance(bridge);
    }

    /* Track recovery */
    if (bridge->enable_recovery_tracking) {
        synaptic_scaling_immune_signal_recovery(bridge);
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int synaptic_scaling_immune_get_tnf_effects(
    const synaptic_scaling_immune_bridge_t* bridge,
    tnf_alpha_scaling_effects_t* effects
) {
    /* Guard: validate inputs */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_get_tnf_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Copy effects */
    *effects = bridge->tnf_effects;

    return 0;
}

int synaptic_scaling_immune_get_inflammation_state(
    const synaptic_scaling_immune_bridge_t* bridge,
    inflammation_scaling_state_t* state
) {
    /* Guard: validate inputs */
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synaptic_scaling_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Copy state */
    *state = bridge->inflammation_state;
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    return 0;
}

bool synaptic_scaling_immune_is_aberrant(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) {
        return false;
    }

    /* Aberrant if any detection flag is set */
    return bridge->aberrance.excessive_scale_up ||
           bridge->aberrance.excessive_scale_down ||
           bridge->aberrance.scaling_oscillations ||
           bridge->aberrance.homeostatic_failure;
}

float synaptic_scaling_immune_get_effective_scaling_factor(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) return 1.0f;

    /* Combine TNF-α modulation with inflammation rate multiplier */
    float effective = bridge->tnf_effects.scaling_factor_modulation *
                     bridge->inflammation_state.scaling_rate_multiplier;

    return nimcp_clampf(effective, 0.1f, 5.0f);
}

float synaptic_scaling_immune_get_ampa_density(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) return 1.0f;

    return bridge->tnf_effects.receptor_surface_density;
}

float synaptic_scaling_immune_get_recovery_progress(
    const synaptic_scaling_immune_bridge_t* bridge
) {
    /* Guard: validate input */
    if (!bridge) return 0.0f;

    return bridge->recovery.recovery_progress;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define SYNAPTIC_SCALING_IMMUNE_MODULE_NAME "synaptic_scaling_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int synaptic_scaling_immune_connect_bio_async(synaptic_scaling_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SYNAPTIC_SCALING,
        .module_name = SYNAPTIC_SCALING_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("synaptic_scaling_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int synaptic_scaling_immune_disconnect_bio_async(synaptic_scaling_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("synaptic_scaling_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool synaptic_scaling_immune_is_bio_async_connected(const synaptic_scaling_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
