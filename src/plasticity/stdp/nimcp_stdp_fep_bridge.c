/**
 * @file nimcp_stdp_fep_bridge.c
 * @brief Free Energy Principle - STDP Integration Bridge Implementation
 */

#include "plasticity/stdp/nimcp_stdp_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

/* Quantum bridge integration */
#define NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/stdp/nimcp_stdp_quantum_bridge.h"

/* Logging module name */
#define LOG_MODULE_STDP_FEP "STDP_FEP_BRIDGE"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stdp_fep_bridge)


/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int stdp_fep_bridge_default_config(stdp_fep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "STDP-FEP config is NULL");

    config->pe_min_threshold = STDP_FEP_PE_MIN_THRESHOLD;
    config->pe_max_threshold = STDP_FEP_PE_MAX_THRESHOLD;
    config->precision_sensitivity = STDP_FEP_PRECISION_SENSITIVITY;
    config->lr_min_factor = STDP_FEP_LR_MIN_FACTOR;
    config->lr_max_factor = STDP_FEP_LR_MAX_FACTOR;

    config->enable_pe_scaling = true;
    config->enable_precision_weighting = true;
    config->enable_belief_modulation = true;
    config->enable_complexity_regularization = true;

    config->pe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->precision_gain = 1.0f;
    config->belief_sensitivity = 0.5f;

    config->enable_quantum_optimization = true;  /* Enabled by default */

    return 0;
}

stdp_fep_bridge_t* stdp_fep_bridge_create(const stdp_fep_config_t* config) {
    stdp_fep_bridge_t* bridge = (stdp_fep_bridge_t*)nimcp_malloc(sizeof(stdp_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "STDP-FEP bridge allocation failed");

    memset(bridge, 0, sizeof(stdp_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        stdp_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "stdp_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_fep_bridge_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("STDP-FEP bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_fep_bridge_create: mutex creation failed");
        return NULL;
    }

    bridge->effects.total_lr_scaling = 1.0f;
    bridge->effects.effective_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    /* Initialize quantum bridge if enabled */
    bridge->quantum_bridge = NULL;
    bridge->quantum_optimizations = 0;
    if (bridge->config.enable_quantum_optimization) {
        stdp_quantum_config_t qconfig = stdp_quantum_default_config();
        bridge->quantum_bridge = stdp_quantum_bridge_create(&qconfig);
        if (bridge->quantum_bridge) {
            NIMCP_LOGGING_INFO("Quantum-accelerated STDP optimization enabled");
        }
    }

    NIMCP_LOGGING_INFO("STDP-FEP bridge created");
    return bridge;
}

void stdp_fep_bridge_destroy(stdp_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        stdp_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy quantum bridge */
    if (bridge->quantum_bridge) {
        stdp_quantum_bridge_destroy((stdp_quantum_bridge_t*)bridge->quantum_bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("STDP-FEP bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int stdp_fep_bridge_connect_fep(stdp_fep_bridge_t* bridge, fep_system_t* fep) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    NIMCP_API_CHECK_NULL(fep, -1, "FEP system is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to FEP system");
    return 0;
}

int stdp_fep_bridge_connect_stdp(stdp_fep_bridge_t* bridge,
                                   stdp_synapse_t* stdp_synapses,
                                   uint32_t num_synapses) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    NIMCP_API_CHECK_NULL(stdp_synapses, -1, "STDP synapses array is NULL");
    NIMCP_API_CHECK(num_synapses > 0, -1, "Number of synapses must be greater than 0");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stdp_system = stdp_synapses;
    bridge->num_synapses = num_synapses;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to STDP system");
    return 0;
}

int stdp_fep_bridge_disconnect(stdp_fep_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->stdp_system = NULL;
    bridge->num_synapses = 0;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected systems");
    return 0;
}

/* ============================================================================
 * FEP → STDP Direction
 * ============================================================================ */

float stdp_fep_apply_pe_scaling(stdp_fep_bridge_t* bridge, float pe) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_fep_apply_pe_scaling: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_pe_scaling) {
        return 1.0f;
    }

    float pe_abs = fabsf(pe);
    if (pe_abs < bridge->config.pe_min_threshold) {
        return bridge->config.lr_min_factor;
    }

    if (pe_abs > bridge->config.pe_max_threshold) {
        pe_abs = bridge->config.pe_max_threshold;
    }

    float normalized_pe = (pe_abs - bridge->config.pe_min_threshold) /
                          (bridge->config.pe_max_threshold - bridge->config.pe_min_threshold);

    float scaling = 1.0f + normalized_pe * bridge->config.pe_sensitivity;

    if (scaling < bridge->config.lr_min_factor) {
        scaling = bridge->config.lr_min_factor;
    }
    if (scaling > bridge->config.lr_max_factor) {
        scaling = bridge->config.lr_max_factor;
    }

    return scaling;
}

float stdp_fep_apply_precision_weighting(stdp_fep_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_fep_apply_precision_weighting: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_precision_weighting) {
        return 1.0f;
    }

    float scaling = powf(precision, bridge->config.precision_sensitivity);
    scaling *= bridge->config.precision_gain;

    if (scaling < bridge->config.lr_min_factor) {
        scaling = bridge->config.lr_min_factor;
    }
    if (scaling > bridge->config.lr_max_factor) {
        scaling = bridge->config.lr_max_factor;
    }

    return scaling;
}

float stdp_fep_apply_belief_modulation(stdp_fep_bridge_t* bridge, float belief_delta) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_fep_apply_belief_modulation: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_belief_modulation) {
        return 1.0f;
    }

    float scaling = 1.0f + fabsf(belief_delta) * bridge->config.belief_sensitivity;

    if (scaling < bridge->config.lr_min_factor) {
        scaling = bridge->config.lr_min_factor;
    }
    if (scaling > bridge->config.lr_max_factor) {
        scaling = bridge->config.lr_max_factor;
    }

    return scaling;
}

float stdp_fep_get_effective_lr(const stdp_fep_bridge_t* bridge, float base_lr) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_fep_get_effective_lr: bridge is NULL");
        return base_lr;
    }

    return base_lr * bridge->effects.total_lr_scaling;
}

/* ============================================================================
 * STDP → FEP Direction
 * ============================================================================ */

int stdp_fep_report_weight_changes(stdp_fep_bridge_t* bridge, float weight_delta) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.total_weight_delta += weight_delta;
    bridge->stats.weight_updates++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float stdp_fep_compute_complexity_regularization(const stdp_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_fep_compute_complexity_regularization: bridge is NULL");
        return 1.0f;
    }

    if (!bridge->config.enable_complexity_regularization) {
        return 1.0f;
    }

    return 0.99f;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int stdp_fep_bridge_update(stdp_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "stdp_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "stdp_fep_bridge_update");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (bridge->fep_system) {
        float pe_lr_scaling = 1.0f;
        float precision_lr_scaling = 1.0f;
        float belief_lr_scaling = 1.0f;

        if (bridge->config.enable_pe_scaling) {
            pe_lr_scaling = stdp_fep_apply_pe_scaling(bridge, bridge->effects.pe_magnitude);
            bridge->effects.pe_lr_scaling = pe_lr_scaling;
        }

        if (bridge->config.enable_precision_weighting) {
            precision_lr_scaling = stdp_fep_apply_precision_weighting(bridge, bridge->effects.precision_value);
            bridge->effects.precision_lr_scaling = precision_lr_scaling;
        }

        if (bridge->config.enable_belief_modulation) {
            belief_lr_scaling = stdp_fep_apply_belief_modulation(bridge, bridge->effects.belief_delta);
            bridge->effects.belief_lr_scaling = belief_lr_scaling;
        }

        bridge->effects.total_lr_scaling = pe_lr_scaling * precision_lr_scaling * belief_lr_scaling;

        /* Apply quantum optimization if enabled */
        if (bridge->quantum_bridge && stdp_quantum_bridge_is_enabled((stdp_quantum_bridge_t*)bridge->quantum_bridge)) {
            qstdp_activity_stats_t activity_stats = {0};
            activity_stats.mean_weight = 0.5f;  /* Default estimate */
            activity_stats.weight_variance = 0.1f;
            activity_stats.firing_rate = 10.0f;  /* Default estimate */
            activity_stats.ltp_rate = bridge->effects.pe_magnitude * 100.0f;
            activity_stats.ltd_rate = (1.0f - bridge->effects.precision_value) * 50.0f;
            activity_stats.ltp_ltd_ratio = (activity_stats.ltd_rate > 0.0f) ?
                activity_stats.ltp_rate / activity_stats.ltd_rate : 1.0f;

            float quantum_lr = stdp_quantum_step((stdp_quantum_bridge_t*)bridge->quantum_bridge, &activity_stats);
            bridge->effects.total_lr_scaling *= quantum_lr;
            bridge->quantum_optimizations++;
        }

        bridge->stats.avg_pe_scaling =
            (bridge->stats.avg_pe_scaling * bridge->stats.total_updates + pe_lr_scaling) /
            (bridge->stats.total_updates + 1);
        bridge->stats.avg_precision_scaling =
            (bridge->stats.avg_precision_scaling * bridge->stats.total_updates + precision_lr_scaling) /
            (bridge->stats.total_updates + 1);
        bridge->stats.avg_lr_modulation =
            (bridge->stats.avg_lr_modulation * bridge->stats.total_updates + bridge->effects.total_lr_scaling) /
            (bridge->stats.total_updates + 1);
    }

    bridge->stats.total_updates++;
    bridge->state.last_update_time = delta_ms;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int stdp_fep_bridge_get_state(const stdp_fep_bridge_t* bridge, stdp_fep_state_t* state) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    NIMCP_API_CHECK_NULL(state, -1, "State output pointer is NULL");

    *state = bridge->state;
    return 0;
}

int stdp_fep_bridge_get_stats(const stdp_fep_bridge_t* bridge, stdp_fep_stats_t* stats) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    NIMCP_API_CHECK_NULL(stats, -1, "Stats output pointer is NULL");

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int stdp_fep_bridge_connect_bio_async(stdp_fep_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_STDP_BRIDGE,
        .module_name = "stdp_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }

    return 0;
}

int stdp_fep_bridge_disconnect_bio_async(stdp_fep_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-FEP bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool stdp_fep_bridge_is_bio_async_connected(const stdp_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
