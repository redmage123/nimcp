/**
 * @file nimcp_calcium_immune_bridge.c
 * @brief Calcium Dynamics-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional coupling between brain immune and calcium dynamics
 * WHY:  Biological realism - cytokines impair calcium signaling, dysregulated calcium triggers immune
 * HOW:  Monitor cytokines to modulate calcium, monitor calcium health to trigger immune
 */

#include "plasticity/calcium/nimcp_calcium_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(calcium_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(calcium_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Map inflammation level to clearance efficiency factor
 * WHY:  Standardized mapping for biological consistency
 * HOW:  Switch on inflammation level
 */
static float inflammation_to_clearance_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_CLEARANCE_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_CLEARANCE_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_CLEARANCE_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_CLEARANCE_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_CLEARANCE_STORM;
        default:                    return INFLAMMATION_CLEARANCE_NONE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int calcium_immune_default_config(calcium_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_default_config: config is NULL");
        return -1;
    }

    /* All features enabled by default */
    config->enable_cytokine_calcium_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_instability_detection = true;
    config->enable_homeostatic_feedback = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->instability_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Default calcium parameters */
    config->base_influx_alpha = CALCIUM_INFLUX_ALPHA_DEFAULT;
    config->base_pump_rate = CALCIUM_PUMP_RATE_DEFAULT;
    config->base_buffer_capacity = CALCIUM_BUFFER_CAPACITY_DEFAULT;
    config->base_decay_tau_ms = CALCIUM_DECAY_TAU_DEFAULT;

    /* Evidence-based thresholds */
    config->excitotoxicity_threshold_um = CALCIUM_EXCITOTOXICITY_THRESHOLD;
    config->synaptic_failure_threshold_um = CALCIUM_SYNAPTIC_FAILURE_THRESHOLD;
    config->oscillation_freq_threshold_hz = CALCIUM_OSCILLATION_FREQ_THRESHOLD;

    return 0;
}

calcium_immune_bridge_t* calcium_immune_bridge_create(
    const calcium_immune_config_t* config,
    brain_immune_system_t* immune_system,
    calcium_dynamics_t calcium
) {
    /* Guard: require immune system and calcium */
    if (!immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_bridge_create: immune_system is NULL");
        return NULL;
    }
    if (!calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_bridge_create: calcium is NULL");
        return NULL;
    }

    /* Allocate bridge */
    calcium_immune_bridge_t* bridge = (calcium_immune_bridge_t*)
        nimcp_malloc(sizeof(calcium_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "calcium_immune_bridge_create: bridge allocation failed");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(calcium_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->calcium = calcium;

    /* Initialize modulation factors to neutral values (1.0 = no modification) */
    bridge->cytokine_effects.total_influx_modulation = 1.0f;
    bridge->cytokine_effects.total_clearance_modulation = 1.0f;
    bridge->cytokine_effects.total_buffer_modulation = 1.0f;
    bridge->cytokine_effects.il1_influx_impairment = 1.0f;
    bridge->cytokine_effects.il6_influx_impairment = 1.0f;
    bridge->cytokine_effects.tnf_influx_impairment = 1.0f;
    bridge->cytokine_effects.ifn_gamma_influx_impairment = 1.0f;
    bridge->cytokine_effects.il10_influx_restoration = 1.0f;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_calcium_modulation = config->enable_cytokine_calcium_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_instability_detection = config->enable_instability_detection;
        bridge->enable_homeostatic_feedback = config->enable_homeostatic_feedback;

        bridge->base_influx_alpha = config->base_influx_alpha;
        bridge->base_pump_rate = config->base_pump_rate;
        bridge->base_buffer_capacity = config->base_buffer_capacity;
        bridge->base_decay_tau_ms = config->base_decay_tau_ms;
    } else {
        /* Use defaults */
        calcium_immune_config_t default_cfg;
        calcium_immune_default_config(&default_cfg);
        bridge->enable_cytokine_calcium_modulation = default_cfg.enable_cytokine_calcium_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_instability_detection = default_cfg.enable_instability_detection;
        bridge->enable_homeostatic_feedback = default_cfg.enable_homeostatic_feedback;

        bridge->base_influx_alpha = default_cfg.base_influx_alpha;
        bridge->base_pump_rate = default_cfg.base_pump_rate;
        bridge->base_buffer_capacity = default_cfg.base_buffer_capacity;
        bridge->base_decay_tau_ms = default_cfg.base_decay_tau_ms;
    }

    /* Create mutex */
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Calcium-immune bridge mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Calcium-immune bridge mutex allocation failed");
        return NULL;
    }
    nimcp_mutex_init((nimcp_mutex_t*)bridge->base.mutex, NULL);

    NIMCP_LOGGING_INFO("Calcium-immune bridge created");
    return bridge;
}

void calcium_immune_bridge_destroy(calcium_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        calcium_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
        nimcp_mutex_destroy(mtx);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Calcium Implementation
 * ============================================================================ */

int calcium_immune_apply_cytokine_effects(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_apply_cytokine_effects: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Initialize effects to neutral */
    bridge->cytokine_effects.il1_influx_impairment = 1.0f;
    bridge->cytokine_effects.il6_influx_impairment = 1.0f;
    bridge->cytokine_effects.tnf_influx_impairment = 1.0f;
    bridge->cytokine_effects.ifn_gamma_influx_impairment = 1.0f;
    bridge->cytokine_effects.il10_influx_restoration = 1.0f;

    /* Query immune system for cytokine levels (placeholder - would use real API) */
    /* For now, set to neutral. Actual implementation would call:
     * float il1_level = brain_immune_get_cytokine_level(bridge->immune_system, BRAIN_CYTOKINE_IL1);
     * etc. */

    /* Compute aggregate influx modulation */
    float influx_mod = 1.0f;
    influx_mod *= bridge->cytokine_effects.il1_influx_impairment;
    influx_mod *= bridge->cytokine_effects.il6_influx_impairment;
    influx_mod *= bridge->cytokine_effects.tnf_influx_impairment;
    influx_mod *= bridge->cytokine_effects.ifn_gamma_influx_impairment;
    influx_mod *= bridge->cytokine_effects.il10_influx_restoration;

    bridge->cytokine_effects.total_influx_modulation = nimcp_clampf(influx_mod, 0.1f, 2.0f);
    bridge->cytokine_effects.total_clearance_modulation = 1.0f;
    bridge->cytokine_effects.total_buffer_modulation = 1.0f;

    bridge->cytokine_modulations++;

    nimcp_mutex_unlock(mtx);
    return 0;
}

int calcium_immune_apply_inflammation_effects(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_apply_inflammation_effects: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Get inflammation level (placeholder - would use real API) */
    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.inflammation_duration_sec = 0.0f;
    bridge->inflammation_state.is_chronic = false;

    /* Map inflammation to clearance impairment */
    float clearance_factor = inflammation_to_clearance_factor(
        bridge->inflammation_state.current_level
    );

    bridge->inflammation_state.clearance_impairment = 1.0f - clearance_factor;
    bridge->inflammation_state.buffer_capacity_loss =
        bridge->inflammation_state.clearance_impairment * 0.5f;
    bridge->inflammation_state.nmda_sensitivity_reduction =
        bridge->inflammation_state.clearance_impairment * 0.3f;

    /* Chronic effects */
    if (bridge->inflammation_state.is_chronic) {
        bridge->inflammation_state.pump_expression_loss = 0.3f;
        bridge->inflammation_state.mitochondrial_dysfunction = 0.4f;
    } else {
        bridge->inflammation_state.pump_expression_loss = 0.0f;
        bridge->inflammation_state.mitochondrial_dysfunction = 0.0f;
    }

    nimcp_mutex_unlock(mtx);
    return 0;
}

float calcium_immune_get_effective_influx(const calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_effective_influx: bridge is NULL");
        return 1.0f;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    float influx_factor = bridge->cytokine_effects.total_influx_modulation;
    influx_factor *= (1.0f - bridge->inflammation_state.nmda_sensitivity_reduction);

    nimcp_mutex_unlock(mtx);
    return nimcp_clampf(influx_factor, 0.1f, 2.0f);
}

int calcium_immune_get_modulation_state(
    const calcium_immune_bridge_t* bridge,
    calcium_modulation_state_t* modulation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_modulation_state: bridge is NULL");
        return -1;
    }
    if (!modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_modulation_state: modulation is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Compute modulation factors */
    modulation->influx_modulation = bridge->cytokine_effects.total_influx_modulation *
                                     (1.0f - bridge->inflammation_state.nmda_sensitivity_reduction);

    modulation->pump_modulation = (1.0f - bridge->inflammation_state.clearance_impairment) *
                                   (1.0f - bridge->inflammation_state.pump_expression_loss);

    modulation->buffer_modulation = 1.0f - bridge->inflammation_state.buffer_capacity_loss;

    modulation->decay_modulation = 1.0f / modulation->pump_modulation;  /* Slower decay if pumps impaired */

    /* Compute effective parameters */
    modulation->effective_influx_alpha = bridge->base_influx_alpha * modulation->influx_modulation;
    modulation->effective_pump_rate = bridge->base_pump_rate * modulation->pump_modulation;
    modulation->effective_buffer_capacity = bridge->base_buffer_capacity * modulation->buffer_modulation;
    modulation->effective_decay_tau_ms = bridge->base_decay_tau_ms * modulation->decay_modulation;

    nimcp_mutex_unlock(mtx);
    return 0;
}

int calcium_immune_restore_dynamics(
    calcium_immune_bridge_t* bridge,
    float recovery_factor
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_restore_dynamics: bridge is NULL");
        return -1;
    }
    if (recovery_factor < 0.0f || recovery_factor > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "calcium_immune_restore_dynamics: recovery_factor out of range [0,1]");
        return -1;
    }

    recovery_factor = nimcp_clampf(recovery_factor, 0.0f, 1.0f);

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Interpolate back to baseline */
    bridge->cytokine_effects.total_influx_modulation =
        1.0f + (bridge->cytokine_effects.total_influx_modulation - 1.0f) * (1.0f - recovery_factor);

    bridge->inflammation_state.clearance_impairment *= (1.0f - recovery_factor);
    bridge->inflammation_state.buffer_capacity_loss *= (1.0f - recovery_factor);
    bridge->inflammation_state.nmda_sensitivity_reduction *= (1.0f - recovery_factor);

    bridge->calcium_restorations++;

    nimcp_mutex_unlock(mtx);
    return 0;
}

/* ============================================================================
 * Calcium → Immune Implementation
 * ============================================================================ */

int calcium_immune_detect_instability(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_detect_instability: bridge is NULL");
        return -1;
    }
    if (!bridge->calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "calcium_immune_detect_instability: calcium dynamics not connected");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Get current calcium concentration */
    float ca = calcium_get_concentration(bridge->calcium);

    /* Update min/max tracking */
    if (ca > bridge->instability_state.max_ca_recent) {
        bridge->instability_state.max_ca_recent = ca;
    }
    if (ca < bridge->instability_state.min_ca_recent || bridge->total_updates == 0) {
        bridge->instability_state.min_ca_recent = ca;
    }

    /* Detect excitotoxicity (sustained high [Ca²⁺]) */
    if (ca > CALCIUM_EXCITOTOXICITY_THRESHOLD) {
        bridge->instability_state.time_above_excitotoxic_ms += 1.0f;  /* Assume 1 ms update */
        if (bridge->instability_state.time_above_excitotoxic_ms > CALCIUM_EXCITOTOXICITY_DURATION_MS) {
            bridge->instability_state.excitotoxicity_detected = true;
        }
    } else {
        bridge->instability_state.time_above_excitotoxic_ms = 0.0f;
        bridge->instability_state.excitotoxicity_detected = false;
    }

    /* Detect synaptic failure (prolonged low [Ca²⁺]) */
    if (ca < CALCIUM_SYNAPTIC_FAILURE_THRESHOLD) {
        bridge->instability_state.time_below_failure_ms += 1.0f;
        if (bridge->instability_state.time_below_failure_ms > CALCIUM_SYNAPTIC_FAILURE_DURATION_MS) {
            bridge->instability_state.synaptic_failure_detected = true;
        }
    } else {
        bridge->instability_state.time_below_failure_ms = 0.0f;
        bridge->instability_state.synaptic_failure_detected = false;
    }

    /* Compute instability severity */
    float severity = 0.0f;
    if (bridge->instability_state.excitotoxicity_detected) severity += 0.5f;
    if (bridge->instability_state.synaptic_failure_detected) severity += 0.3f;
    if (bridge->instability_state.oscillatory_instability) severity += 0.2f;

    bridge->instability_state.instability_severity = nimcp_clampf(severity, 0.0f, 1.0f);

    /* Healthy dynamics if no instabilities */
    bridge->instability_state.healthy_dynamics =
        !bridge->instability_state.excitotoxicity_detected &&
        !bridge->instability_state.synaptic_failure_detected &&
        !bridge->instability_state.oscillatory_instability;

    nimcp_mutex_unlock(mtx);
    return 0;
}

int calcium_immune_alert_instability(
    calcium_immune_bridge_t* bridge,
    uint32_t* antigen_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_alert_instability: bridge is NULL");
        return -1;
    }
    if (!antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_alert_instability: antigen_id is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Create epitope from instability signature (placeholder) */
    uint8_t epitope[64];
    memset(epitope, 0, sizeof(epitope));

    /* Encode instability type in epitope */
    epitope[0] = bridge->instability_state.excitotoxicity_detected ? 1 : 0;
    epitope[1] = bridge->instability_state.synaptic_failure_detected ? 1 : 0;
    epitope[2] = bridge->instability_state.oscillatory_instability ? 1 : 0;

    /* Severity level */
    epitope[3] = (uint8_t)(bridge->instability_state.instability_severity * 10.0f);

    /* Would call: brain_immune_present_antigen() here */
    *antigen_id = 0;  /* Placeholder */

    bridge->instability_alerts++;

    nimcp_mutex_unlock(mtx);
    return 0;
}

int calcium_immune_signal_healthy_dynamics(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_signal_healthy_dynamics: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    /* Only signal if truly healthy */
    if (bridge->instability_state.healthy_dynamics) {
        /* Would trigger IL-10 release or anti-inflammatory signaling */
        NIMCP_LOGGING_DEBUG("Signaling healthy calcium dynamics to immune system");
    }

    nimcp_mutex_unlock(mtx);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int calcium_immune_bridge_update(
    calcium_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    bridge->total_updates++;

    nimcp_mutex_unlock(mtx);

    /* Apply immune → calcium effects */
    if (bridge->enable_cytokine_calcium_modulation) {
        calcium_immune_apply_cytokine_effects(bridge);
    }

    if (bridge->enable_inflammation_impairment) {
        calcium_immune_apply_inflammation_effects(bridge);
    }

    /* Detect calcium → immune signals */
    if (bridge->enable_instability_detection) {
        calcium_immune_detect_instability(bridge);

        /* Alert if instability detected */
        if (bridge->instability_state.instability_severity > 0.5f) {
            uint32_t antigen_id;
            calcium_immune_alert_instability(bridge, &antigen_id);
        }
    }

    /* Signal healthy dynamics */
    if (bridge->enable_homeostatic_feedback) {
        calcium_immune_signal_healthy_dynamics(bridge);
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int calcium_immune_get_cytokine_effects(
    const calcium_immune_bridge_t* bridge,
    cytokine_calcium_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_cytokine_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_cytokine_effects: effects is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);
    *effects = bridge->cytokine_effects;
    nimcp_mutex_unlock(mtx);

    return 0;
}

int calcium_immune_get_inflammation_state(
    const calcium_immune_bridge_t* bridge,
    inflammation_calcium_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_inflammation_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_inflammation_state: state is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);
    *state = bridge->inflammation_state;
    nimcp_mutex_unlock(mtx);

    return 0;
}

int calcium_immune_get_instability_state(
    const calcium_immune_bridge_t* bridge,
    calcium_instability_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_instability_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_get_instability_state: state is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);
    *state = bridge->instability_state;
    nimcp_mutex_unlock(mtx);

    return 0;
}

bool calcium_immune_is_dynamics_impaired(const calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    bool impaired = (bridge->cytokine_effects.total_influx_modulation < 0.9f) ||
                     (bridge->inflammation_state.clearance_impairment > 0.1f);

    nimcp_mutex_unlock(mtx);
    return impaired;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int calcium_immune_connect_bio_async(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    if (bridge->base.bio_async_enabled) {
        nimcp_mutex_unlock(mtx);
        return 0;
    }

    bio_module_info_t info = {
        .module_id = 0x0E01,  /* BIO_MODULE_IMMUNE_CALCIUM placeholder */
        .module_name = "calcium_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    nimcp_mutex_unlock(mtx);
    return 0;
}

int calcium_immune_disconnect_bio_async(calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);

    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
        bridge->base.bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    nimcp_mutex_unlock(mtx);
    return 0;
}

bool calcium_immune_is_bio_async_connected(const calcium_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_t* mtx = (nimcp_mutex_t*)bridge->base.mutex;
    nimcp_mutex_lock(mtx);
    bool connected = bridge->base.bio_async_enabled;
    nimcp_mutex_unlock(mtx);

    return connected;
}
