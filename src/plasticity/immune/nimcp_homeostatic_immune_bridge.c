/**
 * @file nimcp_homeostatic_immune_bridge.c
 * @brief Homeostatic Plasticity-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and homeostatic plasticity
 * WHY:  Biological realism - cytokines disrupt homeostasis, instability triggers immunity
 * HOW:  Monitor cytokine levels to modulate homeostatic parameters, monitor instability to trigger immune
 */

#include "plasticity/immune/nimcp_homeostatic_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(homeostatic_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(homeostatic_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>3 days) disrupts homeostasis
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query immune system for inflammation sites - placeholder */
    /* Actual implementation would check inflammation_sites array */
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines homeostatic disruption
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    /* Query immune system inflammation_sites - placeholder */
    return INFLAMMATION_NONE;
}

/**
 * @brief Compute mean firing rate
 *
 * WHAT: Calculate average firing rate across neurons
 * WHY:  Assess overall network excitability
 * HOW:  Sum and divide
 */
static float compute_mean_firing_rate(const float* rates, uint32_t num) {
    if (!rates || num == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < num; i++) {
        sum += rates[i];
    }
    return sum / (float)num;
}

/**
 * @brief Compute firing rate variance
 *
 * WHAT: Calculate variance in firing rates
 * WHY:  High variance indicates instability
 * HOW:  Standard variance formula
 */
static float compute_firing_rate_variance(
    const float* rates,
    uint32_t num,
    float mean
) {
    if (!rates || num == 0) return 0.0f;

    float sum_sq_diff = 0.0f;
    for (uint32_t i = 0; i < num; i++) {
        float diff = rates[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / (float)num;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int homeostatic_immune_default_config(homeostatic_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_homeostasis_modulation = true;
    config->enable_inflammation_disruption = true;
    config->enable_instability_immune_trigger = true;
    config->enable_recovery_immune_boost = true;
    config->enable_tnf_biphasic_effect = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->instability_trigger_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->instability_trigger_threshold = INSTABILITY_IMMUNE_TRIGGER_THRESHOLD;
    config->inflammation_disruption_threshold = INFLAMMATION_SCALING_DISRUPTION_THRESHOLD;

    /* Baseline homeostatic parameters (typical cortical values) */
    config->baseline_scaling_factor = 1.0f;
    config->baseline_target_rate = 5.0f;  /* 5 Hz typical cortical */
    config->baseline_threshold = 0.5f;    /* Normalized threshold */

    return 0;
}

homeostatic_immune_bridge_t* homeostatic_immune_bridge_create(
    const homeostatic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    homeostatic_controller_t homeostatic_controller
) {
    /* Guard: require both systems */
    if (!immune_system || !homeostatic_controller) {
        LOG_MODULE_ERROR("homeostatic_immune_bridge",
                  "Cannot create bridge without immune and homeostatic systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_bridge_create: required parameter is NULL (immune_system, homeostatic_controller)");
        return NULL;
    }

    /* Allocate bridge */
    homeostatic_immune_bridge_t* bridge = (homeostatic_immune_bridge_t*)
        nimcp_malloc(sizeof(homeostatic_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("homeostatic_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(homeostatic_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->homeostatic_controller = homeostatic_controller;

    /* Apply configuration */
    homeostatic_immune_config_t default_cfg;
    const homeostatic_immune_config_t* cfg = config;
    if (!cfg) {
        homeostatic_immune_default_config(&default_cfg);
        cfg = &default_cfg;
    }

    bridge->enable_cytokine_homeostasis_modulation = cfg->enable_cytokine_homeostasis_modulation;
    bridge->enable_inflammation_disruption = cfg->enable_inflammation_disruption;
    bridge->enable_instability_immune_trigger = cfg->enable_instability_immune_trigger;
    bridge->enable_recovery_immune_boost = cfg->enable_recovery_immune_boost;
    bridge->enable_tnf_biphasic_effect = cfg->enable_tnf_biphasic_effect;

    /* Initialize baseline parameters */
    bridge->base_scaling_factor = cfg->baseline_scaling_factor;
    bridge->base_target_rate = cfg->baseline_target_rate;
    bridge->base_threshold = cfg->baseline_threshold;

    /* Initialize current parameters (start at baseline) */
    bridge->current_scaling_factor = bridge->base_scaling_factor;
    bridge->current_target_rate = bridge->base_target_rate;
    bridge->current_threshold = bridge->base_threshold;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "homeostatic_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("homeostatic_immune_bridge",
                  "Bridge created successfully");
    return bridge;
}

void homeostatic_immune_bridge_destroy(homeostatic_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("homeostatic_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Homeostasis Implementation
 * ============================================================================ */

int homeostatic_immune_apply_cytokine_effects(
    homeostatic_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_homeostasis_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_homeostatic_effects_t* effects = &bridge->cytokine_effects;

    /* Query cytokine levels from immune system - placeholder */
    /* In actual implementation, would query brain_immune_system cytokine levels */
    float tnf_level = 0.0f;
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float ifn_gamma_level = 0.0f;
    float il10_level = 0.0f;

    /* TNF-α biphasic effect on scaling */
    if (bridge->enable_tnf_biphasic_effect) {
        effects->tnf_scaling_modulation =
            homeostatic_immune_compute_tnf_biphasic(bridge, tnf_level);
    } else {
        effects->tnf_scaling_modulation = 0.0f;
    }

    /* IL-1β → threshold increase */
    effects->il1_threshold_shift = il1_level * CYTOKINE_IL1_THRESHOLD_IMPACT;

    /* IL-6 → target rate decrease */
    effects->il6_target_rate_shift = il6_level * CYTOKINE_IL6_TARGET_IMPACT;

    /* IFN-γ → scaling disruption */
    effects->ifn_gamma_scaling_disruption =
        ifn_gamma_level * CYTOKINE_IFN_GAMMA_SCALING_IMPACT;

    /* IL-10 → restoration */
    effects->il10_restoration_factor = il10_level * CYTOKINE_IL10_RESTORATION;

    /* Aggregate effects */
    effects->total_scaling_factor_shift =
        effects->tnf_scaling_modulation +
        effects->ifn_gamma_scaling_disruption +
        effects->il10_restoration_factor;

    effects->total_target_rate_shift =
        effects->il6_target_rate_shift +
        effects->il10_restoration_factor * 0.5f;  /* IL-10 partially restores */

    effects->total_threshold_shift =
        effects->il1_threshold_shift -
        effects->il10_restoration_factor * 0.3f;  /* IL-10 normalizes threshold */

    /* Overall disruption level */
    float pro_inflammatory = fabsf(effects->il1_threshold_shift) +
                            fabsf(effects->il6_target_rate_shift) +
                            fabsf(effects->ifn_gamma_scaling_disruption);
    float anti_inflammatory = effects->il10_restoration_factor;
    effects->homeostatic_disruption_level =
        nimcp_clampf(pro_inflammatory - anti_inflammatory, 0.0f, 1.0f);

    /* Apply to current homeostatic parameters */
    bridge->current_scaling_factor =
        bridge->base_scaling_factor + effects->total_scaling_factor_shift;
    bridge->current_scaling_factor =
        nimcp_clampf(bridge->current_scaling_factor, 0.1f, 2.0f);

    bridge->current_target_rate =
        bridge->base_target_rate + effects->total_target_rate_shift;
    bridge->current_target_rate =
        nimcp_clampf(bridge->current_target_rate, 1.0f, 20.0f);

    bridge->current_threshold =
        bridge->base_threshold + effects->total_threshold_shift;
    bridge->current_threshold =
        nimcp_clampf(bridge->current_threshold, 0.0f, 1.0f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int homeostatic_immune_apply_inflammation_effects(
    homeostatic_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_disruption) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_homeostatic_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec =
        get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic =
        (state->inflammation_duration_sec >= CHRONIC_INFLAMMATION_HOMEOSTATIC_THRESHOLD);

    /* Inflammation intensity */
    float inflammation_intensity =
        (float)state->current_level / (float)INFLAMMATION_STORM;

    /* Scaling disruption increases with inflammation */
    state->scaling_disruption =
        nimcp_clampf(inflammation_intensity * 0.8f, 0.0f, 1.0f);

    /* Setpoint shift (inflammation pushes away from baseline) */
    state->setpoint_shift =
        nimcp_clampf(inflammation_intensity * INFLAMMATION_SETPOINT_SHIFT_MAX,
                0.0f, INFLAMMATION_SETPOINT_SHIFT_MAX);

    /* Adaptation impairment (chronic inflammation reduces adaptability) */
    if (state->is_chronic) {
        float duration_factor = nimcp_clampf(
            state->inflammation_duration_sec /
            (CHRONIC_INFLAMMATION_HOMEOSTATIC_THRESHOLD * 2.0f),
            0.0f, 1.0f
        );
        state->adaptation_impairment = duration_factor * 0.9f;
    } else {
        state->adaptation_impairment = inflammation_intensity * 0.4f;
    }

    /* Excitability dysregulation */
    state->excitability_dysregulation =
        nimcp_clampf(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    /* Stability loss */
    state->stability_loss =
        (state->scaling_disruption + state->excitability_dysregulation) * 0.5f;

    /* Homeostatic failure at severe inflammation */
    state->homeostatic_failure =
        (state->current_level >= INFLAMMATION_SYSTEMIC) && state->is_chronic;

    if (state->homeostatic_failure) {
        bridge->homeostatic_failures++;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float homeostatic_immune_compute_tnf_biphasic(
    const homeostatic_immune_bridge_t* bridge,
    float tnf_level
) {
    if (!bridge) return 0.0f;

    /* TNF-α has U-shaped dose-response:
     * - Low TNF-α (< 0.3): enhances scaling (+)
     * - Medium TNF-α (0.3-0.7): optimal range
     * - High TNF-α (> 0.7): suppresses scaling (-)
     */

    float net_effect = 0.0f;

    if (tnf_level < TNF_LOW_THRESHOLD) {
        /* Low TNF-α: enhancement phase */
        float enhancement = (TNF_LOW_THRESHOLD - tnf_level) / TNF_LOW_THRESHOLD;
        net_effect = enhancement * CYTOKINE_TNF_SCALING_IMPACT;
    } else if (tnf_level > TNF_HIGH_THRESHOLD) {
        /* High TNF-α: suppression phase */
        float suppression = (tnf_level - TNF_HIGH_THRESHOLD) /
                           (1.0f - TNF_HIGH_THRESHOLD);
        net_effect = -suppression * CYTOKINE_TNF_SCALING_IMPACT;
    } else {
        /* Optimal range: minimal effect */
        net_effect = 0.0f;
    }

    return net_effect;
}

int homeostatic_immune_apply_modulated_parameters(
    homeostatic_immune_bridge_t* bridge
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->homeostatic_controller) return -1;

    /* Note: Actual implementation would call homeostatic_controller API
     * to set the modulated parameters. This is a placeholder showing intent.
     */

    LOG_MODULE_DEBUG("homeostatic_immune_bridge",
                  "Applied modulated parameters: scaling=%.3f, target=%.2f, threshold=%.3f",
              bridge->current_scaling_factor,
              bridge->current_target_rate,
              bridge->current_threshold);

    return 0;
}

/* ============================================================================
 * Homeostasis → Immune Implementation
 * ============================================================================ */

int homeostatic_immune_trigger_from_instability(
    homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons
) {
    /* Guard clauses */
    BRIDGE_BBB_VALIDATE(bridge, firing_rates, sizeof(*firing_rates));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->enable_instability_immune_trigger) return 0;
    if (!bridge->immune_system || !firing_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_trigger_from_instability: required parameter is NULL (bridge->immune_system, firing_rates)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    homeostatic_immune_trigger_t* trigger = &bridge->instability_trigger;

    /* Detect hyperexcitability */
    trigger->hyperexcitability_level =
        homeostatic_immune_detect_hyperexcitability(bridge, firing_rates, num_neurons);

    /* Compute overall instability score */
    trigger->instability_score =
        (trigger->hyperexcitability_level + trigger->scaling_failure_level) * 0.5f;

    /* Trigger immune response if above threshold */
    if (trigger->instability_score >= INSTABILITY_IMMUNE_TRIGGER_THRESHOLD) {
        trigger->cytokine_triggered = true;
        trigger->inflammation_triggered =
            (trigger->instability_score >= 0.85f);
        trigger->immune_activation_strength = trigger->instability_score;

        /* Present antigen to immune system */
        uint8_t epitope[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        uint32_t severity = (uint32_t)(trigger->instability_score * 10.0f);
        uint32_t antigen_id;

        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope, 8,
            severity,
            0,  /* source_node */
            &antigen_id
        );

        bridge->instability_triggered_responses++;

        LOG_MODULE_WARN("homeostatic_immune_bridge",
                  "Triggered immune response from instability (score=%.3f)",
                  trigger->instability_score);
    } else {
        trigger->cytokine_triggered = false;
        trigger->inflammation_triggered = false;
        trigger->immune_activation_strength = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float homeostatic_immune_detect_hyperexcitability(
    const homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons
) {
    if (!bridge || !firing_rates || num_neurons == 0) return 0.0f;

    /* Compute mean firing rate */
    BRIDGE_BBB_VALIDATE(bridge, firing_rates, sizeof(*firing_rates));
    float mean_rate = compute_mean_firing_rate(firing_rates, num_neurons);

    /* Hyperexcitability: firing rate significantly above target */
    float target_rate = bridge->base_target_rate;
    float rate_deviation = (mean_rate - target_rate) / target_rate;

    /* Positive deviation only (hyperexcitability, not hypoactivity) */
    if (rate_deviation < 0.0f) return 0.0f;

    /* Normalize to [0-1] range, saturate at 3x target */
    float hyperexcitability = nimcp_clampf(rate_deviation / 3.0f, 0.0f, 1.0f);

    return hyperexcitability;
}

float homeostatic_immune_detect_scaling_failure(
    homeostatic_immune_bridge_t* bridge,
    bool scaling_success
) {
    if (!bridge) return 0.0f;

    homeostatic_immune_trigger_t* trigger = &bridge->instability_trigger;

    /* Track consecutive failures */
    if (!scaling_success) {
        trigger->consecutive_failures++;
    } else {
        trigger->consecutive_failures = 0;
        trigger->failure_duration_sec = 0.0f;
    }

    /* Failure level based on consecutive failures */
    /* 5+ failures = critical (1.0) */
    float failure_level = nimcp_clampf(
        (float)trigger->consecutive_failures / 5.0f,
        0.0f, 1.0f
    );

    trigger->scaling_failure_level = failure_level;
    return failure_level;
}

int homeostatic_immune_boost_from_recovery(
    homeostatic_immune_bridge_t* bridge,
    bool is_stable
) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_recovery_immune_boost) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_boost_from_recovery: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    homeostatic_recovery_immune_boost_t* boost = &bridge->recovery_boost;

    boost->achieved_homeostasis = is_stable;

    if (is_stable) {
        /* Successful homeostasis → immune resolution */
        boost->stability_improvement = 0.8f;  /* Significant improvement */
        boost->scaling_success_rate = 0.9f;   /* High success rate */

        /* Boost IL-10 release */
        boost->il10_release_boost = 0.6f;
        boost->inflammation_reduction = 0.4f;
        boost->immune_resolution_speed = 1.5f;  /* 50% faster resolution */

        /* Release IL-10 cytokine via immune system */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL10,
            0,  /* source_cell */
            boost->il10_release_boost,
            0,  /* broadcast */
            &cytokine_id
        );

        bridge->recovery_boosts++;
        bridge->successful_restorations++;

        LOG_MODULE_INFO("homeostatic_immune_bridge",
                  "Boosted immune resolution from homeostatic recovery");
    } else {
        /* Not stable - reset boost */
        boost->stability_improvement = 0.0f;
        boost->scaling_success_rate = 0.0f;
        boost->il10_release_boost = 0.0f;
        boost->inflammation_reduction = 0.0f;
        boost->immune_resolution_speed = 1.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int homeostatic_immune_bridge_update(
    homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons,
    bool is_stable,
    bool scaling_success,
    uint64_t delta_ms
) {
    /* Guard clauses */
    BRIDGE_BBB_VALIDATE(bridge, firing_rates, sizeof(*firing_rates));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bridge->total_updates++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    /* Immune → Homeostasis */
    homeostatic_immune_apply_cytokine_effects(bridge);
    homeostatic_immune_apply_inflammation_effects(bridge);
    homeostatic_immune_apply_modulated_parameters(bridge);

    /* Homeostasis → Immune */
    homeostatic_immune_detect_scaling_failure(bridge, scaling_success);
    homeostatic_immune_trigger_from_instability(bridge, firing_rates, num_neurons);
    homeostatic_immune_boost_from_recovery(bridge, is_stable);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int homeostatic_immune_get_cytokine_effects(
    const homeostatic_immune_bridge_t* bridge,
    cytokine_homeostatic_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int homeostatic_immune_get_inflammation_state(
    const homeostatic_immune_bridge_t* bridge,
    inflammation_homeostatic_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool homeostatic_immune_is_homeostatic_failure(
    const homeostatic_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bool failure = bridge->inflammation_state.homeostatic_failure;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return failure;
}

float homeostatic_immune_get_current_scaling_factor(
    const homeostatic_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    float factor = bridge->current_scaling_factor;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return factor;
}

float homeostatic_immune_get_current_target_rate(
    const homeostatic_immune_bridge_t* bridge
) {
    if (!bridge) return 5.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    float rate = bridge->current_target_rate;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return rate;
}

float homeostatic_immune_get_current_threshold(
    const homeostatic_immune_bridge_t* bridge
) {
    if (!bridge) return 0.5f;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    float threshold = bridge->current_threshold;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return threshold;
}

float homeostatic_immune_get_disruption_level(
    const homeostatic_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    float disruption = bridge->cytokine_effects.homeostatic_disruption_level;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return disruption;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define HOMEOSTATIC_IMMUNE_MODULE_NAME "homeostatic_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int homeostatic_immune_connect_bio_async(homeostatic_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_HOMEOSTATIC,
        .module_name = HOMEOSTATIC_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("homeostatic_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int homeostatic_immune_disconnect_bio_async(homeostatic_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("homeostatic_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool homeostatic_immune_is_bio_async_connected(const homeostatic_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
