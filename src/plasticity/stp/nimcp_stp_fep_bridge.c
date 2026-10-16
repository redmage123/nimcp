/**
 * @file nimcp_stp_fep_bridge.c
 * @brief Free Energy Principle - Short-Term Plasticity Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-15
 *
 * @author NIMCP Development Team
 */

#include "plasticity/stp/nimcp_stp_fep_bridge.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stp_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stp_fep_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int stp_fep_bridge_default_config(stp_fep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "STP-FEP config is NULL");

    config->pe_min_threshold = STP_FEP_PE_MIN_THRESHOLD;
    config->pe_max_threshold = STP_FEP_PE_MAX_THRESHOLD;
    config->precision_sensitivity = STP_FEP_PRECISION_SENSITIVITY;
    config->u_min = STP_FEP_U_MIN;
    config->u_max = STP_FEP_U_MAX;

    config->enable_pe_modulation = true;
    config->enable_precision_facilitation = true;
    config->enable_free_energy_recovery = true;
    config->enable_stp_precision_feedback = true;

    config->pe_sensitivity = STP_FEP_PE_SCALING_FACTOR;
    config->precision_gain = 1.0f;
    config->free_energy_gain = 0.5f;
    config->stp_precision_gain = 1.0f;

    return 0;
}

stp_fep_bridge_t* stp_fep_bridge_create(const stp_fep_config_t* config) {
    stp_fep_bridge_t* bridge = (stp_fep_bridge_t*)nimcp_malloc(sizeof(stp_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_fep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(stp_fep_config_t));
    } else {
        stp_fep_bridge_default_config(&bridge->config);
    }

    /* Initialize state */
    memset(&bridge->fep_effects, 0, sizeof(stp_fep_effects_t));
    memset(&bridge->stp_effects, 0, sizeof(stp_fep_feedback_t));
    memset(&bridge->state, 0, sizeof(stp_fep_state_t));
    memset(&bridge->stats, 0, sizeof(stp_fep_stats_t));

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "stp_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_fep_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("STP-FEP bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_fep_bridge_create: failed to create mutex");
        return NULL;
    }

    /* Initialize connections */
    bridge->fep_system = NULL;
    bridge->stp_state = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Created STP-FEP bridge");
    return bridge;
}

void stp_fep_bridge_destroy(stp_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if enabled */
    if (bridge->base.bio_async_enabled) {
        stp_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed STP-FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int stp_fep_bridge_connect_fep(stp_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    if (!fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_connect_fep: fep is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to STP-FEP bridge");
    return 0;
}

int stp_fep_bridge_connect_stp(stp_fep_bridge_t* bridge, stp_state_t* stp_state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_connect_stp: bridge is NULL");
        return -1;
    }
    if (!stp_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_connect_stp: stp_state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stp_state = stp_state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected STP state to STP-FEP bridge");
    return 0;
}

int stp_fep_bridge_disconnect(stp_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->stp_state = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected STP-FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP → STP Direction
 * ============================================================================ */

float stp_fep_apply_pe_modulation(stp_fep_bridge_t* bridge, float pe) {
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_pe_modulation) return 1.0f;

    /* Clamp PE to valid range */
    float pe_clamped = nimcp_clampf(fabsf(pe), bridge->config.pe_min_threshold, bridge->config.pe_max_threshold);

    /* Scale PE to [0, 1] range */
    float pe_normalized = (pe_clamped - bridge->config.pe_min_threshold) /
                          (bridge->config.pe_max_threshold - bridge->config.pe_min_threshold);

    /* Apply scaling: higher PE → higher U */
    float scaling = 1.0f + bridge->config.pe_sensitivity * pe_normalized;

    return nimcp_clampf(scaling, STP_FEP_PRECISION_MIN, STP_FEP_PRECISION_MAX);
}

float stp_fep_apply_precision_facilitation(stp_fep_bridge_t* bridge, float precision) {
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_precision_facilitation) return 1.0f;

    /* Higher precision → shorter facilitation time constant (faster facilitation) */
    float scaling = 1.0f / (1.0f + bridge->config.precision_gain * precision * bridge->config.precision_sensitivity);

    return nimcp_clampf(scaling, STP_FEP_TAU_F_MIN_FACTOR, STP_FEP_TAU_F_MAX_FACTOR);
}

float stp_fep_apply_free_energy_recovery(stp_fep_bridge_t* bridge, float free_energy) {
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_free_energy_recovery) return 1.0f;

    /* Higher free energy → faster recovery (lower τ_D) */
    float scaling = 1.0f / (1.0f + bridge->config.free_energy_gain * free_energy);

    return nimcp_clampf(scaling, STP_FEP_TAU_D_MIN_FACTOR, STP_FEP_TAU_D_MAX_FACTOR);
}

float stp_fep_get_effective_u(const stp_fep_bridge_t* bridge, float base_u) {
    if (!bridge) return base_u;

    /* Apply PE modulation to U */
    float effective_u = base_u * bridge->fep_effects.pe_u_scaling;

    /* Apply precision modulation */
    effective_u *= bridge->fep_effects.precision_facilitation_scaling;

    return nimcp_clampf(effective_u, bridge->config.u_min, bridge->config.u_max);
}

/* ============================================================================
 * STP → FEP Direction
 * ============================================================================ */

int stp_fep_report_facilitation(stp_fep_bridge_t* bridge, float u) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_report_facilitation: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_stp_precision_feedback) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stp_effects.current_u = u;
    bridge->stp_effects.u_precision_estimate = u * bridge->config.stp_precision_gain;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stp_fep_report_depression(stp_fep_bridge_t* bridge, float x) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_report_depression: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_stp_precision_feedback) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stp_effects.current_x = x;
    /* Lower x (depletion) → higher complexity */
    bridge->stp_effects.x_complexity_estimate = (1.0f - x);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float stp_fep_compute_stp_precision(const stp_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Precision estimate from facilitation state */
    return bridge->stp_effects.u_precision_estimate;
}

float stp_fep_get_effective_transmission(const stp_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;

    /* Effective synaptic strength = u * x */
    return bridge->stp_effects.current_u * bridge->stp_effects.current_x;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int stp_fep_bridge_update(stp_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update only if both systems are connected */
    if (bridge->fep_system && bridge->stp_state) {
        /* Query FEP state */
        bridge->state.current_pe = fep_get_prediction_error(bridge->fep_system, 0);
        fep_free_energy_t fe;
        fep_compute_free_energy(bridge->fep_system, &fe);
        bridge->state.current_free_energy = fe.total;

        fep_belief_t beliefs;
        fep_get_beliefs(bridge->fep_system, 0, &beliefs);
        bridge->state.current_precision = beliefs.precision ? beliefs.precision[0] : 1.0f;

        /* Update FEP effects on STP */
        bridge->fep_effects.pe_magnitude = fabsf(bridge->state.current_pe);
        bridge->fep_effects.pe_u_scaling = stp_fep_apply_pe_modulation(bridge, bridge->state.current_pe);

        bridge->fep_effects.precision_value = bridge->state.current_precision;
        bridge->fep_effects.precision_facilitation_scaling = stp_fep_apply_precision_facilitation(bridge, bridge->state.current_precision);

        bridge->fep_effects.free_energy_value = bridge->state.current_free_energy;
        bridge->fep_effects.free_energy_recovery_scaling = stp_fep_apply_free_energy_recovery(bridge, bridge->state.current_free_energy);

        /* Update effective parameters */
        bridge->fep_effects.effective_u = stp_fep_get_effective_u(bridge, bridge->stp_state->params.U);
        bridge->fep_effects.effective_tau_d = bridge->stp_state->params.tau_D * bridge->fep_effects.free_energy_recovery_scaling;
        bridge->fep_effects.effective_tau_f = bridge->stp_state->params.tau_F * bridge->fep_effects.precision_facilitation_scaling;

        /* Update STP effects on FEP */
        bridge->state.current_u = bridge->stp_state->u;
        bridge->state.current_x = bridge->stp_state->x;
        bridge->state.current_modulation = bridge->state.current_u * bridge->state.current_x;

        stp_fep_report_facilitation(bridge, bridge->state.current_u);
        stp_fep_report_depression(bridge, bridge->state.current_x);

        bridge->stp_effects.effective_transmission = stp_fep_get_effective_transmission(bridge);
        bridge->stp_effects.transmission_precision_weight = bridge->stp_effects.effective_transmission;

        /* Update statistics */
        bridge->stats.total_updates++;
        if (bridge->config.enable_pe_modulation) bridge->stats.pe_modulated_events++;
        if (bridge->config.enable_precision_facilitation) bridge->stats.precision_modulated_events++;

        bridge->stats.avg_pe_scaling = (bridge->stats.avg_pe_scaling * (bridge->stats.total_updates - 1) + bridge->fep_effects.pe_u_scaling) / bridge->stats.total_updates;
        bridge->stats.avg_precision_scaling = (bridge->stats.avg_precision_scaling * (bridge->stats.total_updates - 1) + bridge->fep_effects.precision_facilitation_scaling) / bridge->stats.total_updates;
        bridge->stats.avg_u_modulation = (bridge->stats.avg_u_modulation * (bridge->stats.total_updates - 1) + bridge->fep_effects.effective_u) / bridge->stats.total_updates;
        bridge->stats.avg_facilitation = (bridge->stats.avg_facilitation * (bridge->stats.total_updates - 1) + bridge->state.current_u) / bridge->stats.total_updates;
        bridge->stats.avg_depression = (bridge->stats.avg_depression * (bridge->stats.total_updates - 1) + bridge->state.current_x) / bridge->stats.total_updates;
        bridge->stats.avg_effective_transmission = (bridge->stats.avg_effective_transmission * (bridge->stats.total_updates - 1) + bridge->stp_effects.effective_transmission) / bridge->stats.total_updates;
        bridge->stats.avg_free_energy = (bridge->stats.avg_free_energy * (bridge->stats.total_updates - 1) + bridge->state.current_free_energy) / bridge->stats.total_updates;
        bridge->stats.avg_prediction_error = (bridge->stats.avg_prediction_error * (bridge->stats.total_updates - 1) + bridge->state.current_pe) / bridge->stats.total_updates;

        bridge->state.last_update_time += delta_ms;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int stp_fep_bridge_get_state(const stp_fep_bridge_t* bridge, stp_fep_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_get_state: state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->state, sizeof(stp_fep_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stp_fep_bridge_get_stats(const stp_fep_bridge_t* bridge, stp_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_get_stats: stats is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(stp_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int stp_fep_bridge_connect_bio_async(stp_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_STP_BRIDGE,
        .module_name = "stp_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected STP-FEP bridge to bio-async");
    }

    return 0;
}

int stp_fep_bridge_disconnect_bio_async(stp_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected STP-FEP bridge from bio-async");

    return 0;
}

bool stp_fep_bridge_is_bio_async_connected(const stp_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
