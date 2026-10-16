/**
 * @file nimcp_stdp_immune_bridge.c
 * @brief STDP Plasticity-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and STDP plasticity
 * WHY:  Biological realism - cytokines impair LTP/LTD, inflammation reduces learning
 * HOW:  Monitor cytokine levels to modulate STDP, monitor STDP health to trigger immune
 */

#include "plasticity/immune/nimcp_stdp_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stdp_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>7 days) has different STDP effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;
    /* Would query immune system for inflammation sites */
    /* For now, return 0 - actual implementation would check inflammation_sites array */
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines STDP impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;
    /* Would query immune system inflammation_sites */
    return INFLAMMATION_NONE;
}

/**
 * @brief Map inflammation level to learning rate factor
 *
 * WHAT: Convert inflammation level to LR multiplier
 * WHY:  Standardized mapping for biological consistency
 * HOW:  Switch on inflammation level
 */
static float inflammation_to_lr_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_LR_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LR_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_LR_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_LR_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_LR_STORM;
        default:                    return INFLAMMATION_LR_NONE;
    }
}

/**
 * @brief Map inflammation level to timing window narrowing
 *
 * WHAT: Convert inflammation level to tau scaling factor
 * WHY:  Inflammation narrows STDP timing window
 * HOW:  Switch on inflammation level
 */
static float inflammation_to_tau_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return 1.0f;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_TAU_NARROWING_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_TAU_NARROWING_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_TAU_NARROWING_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_TAU_NARROWING_STORM;
        default:                    return 1.0f;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int stdp_immune_default_config(stdp_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_stdp_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_instability_detection = true;
    config->enable_homeostatic_feedback = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->instability_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Default STDP parameters (Bi & Poo, 1998) */
    stdp_config_t stdp_defaults = stdp_config_default();
    config->base_learning_rate = stdp_defaults.learning_rate;
    config->base_a_plus = stdp_defaults.a_plus;
    config->base_a_minus = stdp_defaults.a_minus;
    config->base_tau_plus = stdp_defaults.tau_plus;
    config->base_tau_minus = stdp_defaults.tau_minus;

    /* Evidence-based thresholds */
    config->ltp_runaway_threshold = STDP_LTP_RUNAWAY_THRESHOLD;
    config->ltd_runaway_threshold = STDP_LTD_RUNAWAY_THRESHOLD;
    config->balance_threshold = STDP_LTP_LTD_BALANCE_THRESHOLD;

    return 0;
}

stdp_immune_bridge_t* stdp_immune_bridge_create(
    const stdp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    stdp_synapse_t* synapses,
    size_t num_synapses
) {
    /* Guard: require immune system and synapses */
    if (!immune_system || !synapses || num_synapses == 0) {
        LOG_MODULE_ERROR("stdp_immune_bridge",
                  "Cannot create bridge without immune system and synapses");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_bridge_create: required parameter is NULL (immune_system, synapses)");
        return NULL;
    }

    /* Allocate bridge */
    stdp_immune_bridge_t* bridge = (stdp_immune_bridge_t*)
        nimcp_malloc(sizeof(stdp_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("stdp_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(stdp_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->stdp_synapses = synapses;
    bridge->num_synapses = num_synapses;
    bridge->synapse_capacity = num_synapses;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_stdp_modulation = config->enable_cytokine_stdp_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_instability_detection = config->enable_instability_detection;
        bridge->enable_homeostatic_feedback = config->enable_homeostatic_feedback;

        bridge->base_learning_rate = config->base_learning_rate;
        bridge->base_a_plus = config->base_a_plus;
        bridge->base_a_minus = config->base_a_minus;
        bridge->base_tau_plus = config->base_tau_plus;
        bridge->base_tau_minus = config->base_tau_minus;
    } else {
        /* Use defaults */
        stdp_immune_config_t default_cfg;
        stdp_immune_default_config(&default_cfg);
        bridge->enable_cytokine_stdp_modulation = default_cfg.enable_cytokine_stdp_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_instability_detection = default_cfg.enable_instability_detection;
        bridge->enable_homeostatic_feedback = default_cfg.enable_homeostatic_feedback;

        bridge->base_learning_rate = default_cfg.base_learning_rate;
        bridge->base_a_plus = default_cfg.base_a_plus;
        bridge->base_a_minus = default_cfg.base_a_minus;
        bridge->base_tau_plus = default_cfg.base_tau_plus;
        bridge->base_tau_minus = default_cfg.base_tau_minus;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "stdp_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("stdp_immune_bridge", "Bridge created successfully");
    return bridge;
}

void stdp_immune_bridge_destroy(stdp_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("stdp_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → STDP Implementation
 * ============================================================================ */

int stdp_immune_apply_cytokine_effects(stdp_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_stdp_modulation) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_apply_cytokine_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_stdp_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → LTP impairment */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_ltp_impairment = CYTOKINE_IL1_LTP_IMPAIRMENT;
    effects->il6_ltp_impairment = CYTOKINE_IL6_LTP_IMPAIRMENT;
    effects->tnf_ltp_impairment = CYTOKINE_TNF_LTP_IMPAIRMENT;
    effects->ifn_gamma_ltp_impairment = CYTOKINE_IFN_GAMMA_LTP_IMPAIRMENT;

    /* Anti-inflammatory cytokines → LTP restoration */
    effects->il10_ltp_restoration = CYTOKINE_IL10_LTP_RESTORATION;

    /* Aggregate LTP modulation (multiplicative - cytokines compound) */
    effects->total_ltp_modulation =
        effects->il1_ltp_impairment *
        effects->il6_ltp_impairment *
        effects->tnf_ltp_impairment *
        effects->ifn_gamma_ltp_impairment *
        effects->il10_ltp_restoration;

    /* LTD is less affected by cytokines, mostly unchanged */
    effects->total_ltd_modulation = 1.0f;

    /* Learning rate affected by overall cytokine state */
    effects->learning_rate_factor = nimcp_clampf(effects->total_ltp_modulation, 0.1f, 1.5f);

    /* Timing window narrowing from inflammation */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    effects->timing_window_factor = inflammation_to_tau_factor(level);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int stdp_immune_apply_inflammation_effects(stdp_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_impairment) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_stdp_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_INFLAMMATION_THRESHOLD_SEC);

    /* Inflammation level → learning rate suppression */
    state->learning_rate_suppression = 1.0f - inflammation_to_lr_factor(state->current_level);

    /* LTP capacity reduction based on inflammation level */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;
    state->ltp_capacity_reduction = nimcp_clampf(inflammation_intensity * 0.6f, 0.0f, 0.9f);

    /* LTD enhancement (inflammation makes synapses more prone to depression) */
    state->ltd_enhancement = nimcp_clampf(inflammation_intensity * 0.3f, 0.0f, 0.5f);

    /* Timing window narrowing */
    state->timing_window_narrowing = 1.0f - inflammation_to_tau_factor(state->current_level);

    /* Chronic inflammation effects */
    if (state->is_chronic) {
        float duration_factor = nimcp_clampf(
            state->inflammation_duration_sec / (CHRONIC_INFLAMMATION_THRESHOLD_SEC * 2.0f),
            0.0f, 1.0f
        );
        state->spine_density_loss = duration_factor * 0.4f;
        state->consolidation_impairment = duration_factor * 0.6f;
    } else {
        state->spine_density_loss = 0.0f;
        state->consolidation_impairment = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float stdp_immune_get_effective_learning_rate(
    const stdp_immune_bridge_t* bridge,
    float base_lr
) {
    if (!bridge) return base_lr;

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = bridge->cytokine_effects.learning_rate_factor;
    float inflammation_factor = 1.0f - bridge->inflammation_state.learning_rate_suppression;

    /* Multiplicative (effects compound) */
    float total_factor = cytokine_factor * inflammation_factor;

    return base_lr * nimcp_clampf(total_factor, 0.0f, 1.0f);
}

int stdp_immune_get_modulation_state(
    const stdp_immune_bridge_t* bridge,
    stdp_modulation_state_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_get_modulation_state: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Learning rate modulation */
    modulation->learning_rate_modulation =
        bridge->cytokine_effects.learning_rate_factor *
        (1.0f - bridge->inflammation_state.learning_rate_suppression);

    /* LTP amplitude modulation (a_plus) */
    modulation->a_plus_modulation =
        bridge->cytokine_effects.total_ltp_modulation *
        (1.0f - bridge->inflammation_state.ltp_capacity_reduction);

    /* LTD amplitude modulation (a_minus) */
    modulation->a_minus_modulation =
        bridge->cytokine_effects.total_ltd_modulation *
        (1.0f + bridge->inflammation_state.ltd_enhancement);

    /* Timing window modulation (tau narrowing) */
    modulation->tau_plus_modulation = bridge->cytokine_effects.timing_window_factor;
    modulation->tau_minus_modulation = bridge->cytokine_effects.timing_window_factor;

    /* Compute effective parameters */
    modulation->effective_learning_rate =
        bridge->base_learning_rate * modulation->learning_rate_modulation;
    modulation->effective_a_plus =
        bridge->base_a_plus * modulation->a_plus_modulation;
    modulation->effective_a_minus =
        bridge->base_a_minus * modulation->a_minus_modulation;
    modulation->effective_tau_plus =
        bridge->base_tau_plus * modulation->tau_plus_modulation;
    modulation->effective_tau_minus =
        bridge->base_tau_minus * modulation->tau_minus_modulation;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int stdp_immune_apply_modulation_to_synapse(
    stdp_immune_bridge_t* bridge,
    stdp_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_apply_modulation_to_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Get current modulation state */
    stdp_modulation_state_t modulation;
    if (stdp_immune_get_modulation_state(bridge, &modulation) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_immune_apply_modulation_to_synapse: validation failed");
        return -1;
    }

    /* Apply to synapse */
    synapse->learning_rate = modulation.effective_learning_rate;
    synapse->a_plus = modulation.effective_a_plus;
    synapse->a_minus = modulation.effective_a_minus;
    synapse->tau_plus = modulation.effective_tau_plus;
    synapse->tau_minus = modulation.effective_tau_minus;

    return 0;
}

int stdp_immune_restore_plasticity(
    stdp_immune_bridge_t* bridge,
    float recovery_factor
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    recovery_factor = nimcp_clampf(recovery_factor, 0.0f, 1.0f);

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Interpolate back to base parameters */
    for (size_t i = 0; i < bridge->num_synapses; i++) {
        stdp_synapse_t* synapse = &bridge->stdp_synapses[i];

        /* Lerp: current + recovery_factor * (base - current) */
        synapse->learning_rate += recovery_factor * (bridge->base_learning_rate - synapse->learning_rate);
        synapse->a_plus += recovery_factor * (bridge->base_a_plus - synapse->a_plus);
        synapse->a_minus += recovery_factor * (bridge->base_a_minus - synapse->a_minus);
        synapse->tau_plus += recovery_factor * (bridge->base_tau_plus - synapse->tau_plus);
        synapse->tau_minus += recovery_factor * (bridge->base_tau_minus - synapse->tau_minus);
    }

    bridge->plasticity_restorations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * STDP → Immune Implementation
 * ============================================================================ */

int stdp_immune_detect_instability(stdp_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_instability_detection) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    stdp_instability_state_t* state = &bridge->instability_state;

    /* Accumulate recent LTP/LTD across all synapses */
    state->total_ltp_recent = 0.0f;
    state->total_ltd_recent = 0.0f;
    float total_weight_change = 0.0f;

    for (size_t i = 0; i < bridge->num_synapses; i++) {
        const stdp_synapse_t* synapse = &bridge->stdp_synapses[i];
        state->total_ltp_recent += synapse->total_ltp;
        state->total_ltd_recent += synapse->total_ltd;
        total_weight_change += fabsf(synapse->total_ltp - synapse->total_ltd);
    }

    /* Compute LTP/LTD ratio */
    if (state->total_ltd_recent > 0.0f) {
        state->ltp_ltd_ratio = state->total_ltp_recent / state->total_ltd_recent;
    } else {
        state->ltp_ltd_ratio = (state->total_ltp_recent > 0.0f) ? 100.0f : 1.0f;
    }

    /* Weight change rate (per synapse average) */
    state->weight_change_rate = total_weight_change / (float)bridge->num_synapses;

    /* Detect runaway conditions */
    state->ltp_runaway_detected =
        (state->total_ltp_recent > STDP_LTP_RUNAWAY_THRESHOLD) &&
        (state->ltp_ltd_ratio > STDP_LTP_LTD_BALANCE_THRESHOLD);

    state->ltd_runaway_detected =
        (state->total_ltd_recent > STDP_LTD_RUNAWAY_THRESHOLD) &&
        (state->ltp_ltd_ratio < (1.0f / STDP_LTP_LTD_BALANCE_THRESHOLD));

    /* Homeostatic threat from rapid changes */
    state->homeostatic_threat =
        state->weight_change_rate > STDP_WEIGHT_CHANGE_RATE_THRESHOLD;

    /* Balanced plasticity check */
    state->balanced_plasticity =
        !state->ltp_runaway_detected &&
        !state->ltd_runaway_detected &&
        !state->homeostatic_threat &&
        (state->ltp_ltd_ratio > 0.3f && state->ltp_ltd_ratio < 3.0f);

    /* Compute severity */
    if (state->ltp_runaway_detected || state->ltd_runaway_detected) {
        state->instability_severity = 0.8f;
    } else if (state->homeostatic_threat) {
        state->instability_severity = 0.6f;
    } else if (!state->balanced_plasticity) {
        state->instability_severity = 0.3f;
    } else {
        state->instability_severity = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int stdp_immune_alert_instability(
    stdp_immune_bridge_t* bridge,
    uint32_t* antigen_id
) {
    /* Guard clauses */
    if (!bridge || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_alert_instability: required parameter is NULL (bridge, antigen_id)");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_alert_instability: bridge->immune_system is NULL");
        return -1;
    }

    /* Check if instability detected */
    if (bridge->instability_state.instability_severity < 0.5f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_immune_alert_instability: validation failed");
        return -1;  /* No significant instability */
    }

    /* Create epitope from instability signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode instability type and severity */
    epitope[0] = 0xAA;  /* STDP instability marker */
    if (bridge->instability_state.ltp_runaway_detected) {
        epitope[1] = 0x01;
    } else if (bridge->instability_state.ltd_runaway_detected) {
        epitope[1] = 0x02;
    } else if (bridge->instability_state.homeostatic_threat) {
        epitope[1] = 0x03;
    }

    /* Encode severity */
    uint32_t severity = (uint32_t)(bridge->instability_state.instability_severity * 10.0f);
    severity = (severity < 1) ? 1 : ((severity > 10) ? 10 : severity);

    /* Present to immune system */
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        severity,
        0,  /* No specific source node */
        antigen_id
    );

    if (result == 0) {
        bridge->instability_alerts++;
    }

    return result;
}

int stdp_immune_signal_balanced_plasticity(stdp_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_homeostatic_feedback) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_signal_balanced_plasticity: bridge->immune_system is NULL");
        return -1;
    }

    /* Only signal if actually balanced */
    if (!bridge->instability_state.balanced_plasticity) {
        return 0;
    }

    /* Request IL-10 release for anti-inflammatory signaling */
    /* Note: Would call brain_immune_release_cytokine(BRAIN_CYTOKINE_IL10) */
    /* This signals healthy synaptic function to the immune system */

    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int stdp_immune_bridge_update(
    stdp_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;  /* Not currently used, but available for time-based effects */

    /* Apply all bidirectional effects */

    /* Immune → STDP */
    stdp_immune_apply_cytokine_effects(bridge);
    stdp_immune_apply_inflammation_effects(bridge);

    /* Apply modulation to all synapses */
    for (size_t i = 0; i < bridge->num_synapses; i++) {
        stdp_immune_apply_modulation_to_synapse(bridge, &bridge->stdp_synapses[i]);
    }

    /* STDP → Immune */
    stdp_immune_detect_instability(bridge);

    /* Alert immune system if instability detected */
    if (bridge->instability_state.instability_severity >= 0.5f) {
        uint32_t antigen_id;
        stdp_immune_alert_instability(bridge, &antigen_id);
    }

    /* Signal balanced plasticity if healthy */
    stdp_immune_signal_balanced_plasticity(bridge);

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int stdp_immune_get_cytokine_effects(
    const stdp_immune_bridge_t* bridge,
    cytokine_stdp_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_stdp_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int stdp_immune_get_inflammation_state(
    const stdp_immune_bridge_t* bridge,
    inflammation_stdp_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_stdp_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int stdp_immune_get_instability_state(
    const stdp_immune_bridge_t* bridge,
    stdp_instability_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_immune_get_instability_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->instability_state, sizeof(stdp_instability_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool stdp_immune_is_plasticity_impaired(const stdp_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Impaired if learning rate factor < 1.0 */
    return (bridge->cytokine_effects.learning_rate_factor < 1.0f) ||
           (bridge->inflammation_state.learning_rate_suppression > 0.0f);
}

float stdp_immune_get_ltp_capacity_reduction(const stdp_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Return as percentage */
    return bridge->inflammation_state.ltp_capacity_reduction * 100.0f;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define STDP_IMMUNE_MODULE_NAME "stdp_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int stdp_immune_connect_bio_async(stdp_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_STDP,
        .module_name = STDP_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("stdp_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int stdp_immune_disconnect_bio_async(stdp_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("stdp_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool stdp_immune_is_bio_async_connected(const stdp_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
