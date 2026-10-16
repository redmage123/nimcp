/**
 * @file nimcp_eligibility_fep_bridge.c
 * @brief FEP-Eligibility Integration Bridge Implementation
 */

#include "plasticity/eligibility/nimcp_eligibility_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE_ELIGIBILITY_FEP "ELIGIBILITY_FEP_BRIDGE"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(eligibility_fep_bridge)


int eligibility_fep_bridge_default_config(eligibility_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_default_config: config is NULL");
        return -1;
    }
    config->pe_trace_gain = ELIGIBILITY_FEP_PE_TRACE_SCALING;
    config->precision_decay_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->fe_consolidation_threshold = 1.0f;
    config->enable_pe_eligibility = true;
    config->enable_precision_decay_modulation = true;
    config->enable_fe_gated_consolidation = true;
    return 0;
}

eligibility_fep_bridge_t* eligibility_fep_bridge_create(const eligibility_fep_config_t* config) {
    eligibility_fep_bridge_t* bridge = (eligibility_fep_bridge_t*)nimcp_malloc(sizeof(eligibility_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    memset(bridge, 0, sizeof(eligibility_fep_bridge_t));
    if (config) bridge->config = *config;
    else eligibility_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "eligibility_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    bridge->effects.total_decay_modulation = 1.0f;
    NIMCP_LOGGING_INFO("Eligibility-FEP bridge created");
    return bridge;
}

void eligibility_fep_bridge_destroy(eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_destroy: bridge is NULL");
        return;
    }
    if (bridge->base.bio_async_enabled) eligibility_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int eligibility_fep_bridge_connect_fep(eligibility_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    /* Allow NULL fep to disconnect/reset FEP connection */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int eligibility_fep_bridge_connect_eligibility(eligibility_fep_bridge_t* bridge, eligibility_trace_t* traces, uint32_t num) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_connect_eligibility: bridge is NULL");
        return -1;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_connect_eligibility: traces is NULL");
        return -1;
    }
    if (num == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_fep_bridge_connect_eligibility: num is zero");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->eligibility_system = traces;
    bridge->num_traces = num;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int eligibility_fep_bridge_disconnect(eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->eligibility_system = NULL;
    bridge->num_traces = 0;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float eligibility_fep_apply_pe_eligibility(eligibility_fep_bridge_t* bridge, float pe) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_apply_pe_eligibility: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->config.enable_pe_eligibility) return 1.0f;
    return 1.0f + fabsf(pe) * bridge->config.pe_trace_gain;
}

float eligibility_fep_apply_precision_decay_modulation(eligibility_fep_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_apply_precision_decay_modulation: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->config.enable_precision_decay_modulation) return 1.0f;
    float decay = precision * bridge->config.precision_decay_sensitivity;
    decay = fminf(fmaxf(decay, ELIGIBILITY_FEP_PRECISION_DECAY_MIN), ELIGIBILITY_FEP_PRECISION_DECAY_MAX);
    /* Store for get_effective_decay to use */
    bridge->effects.precision_value = precision;
    bridge->effects.precision_decay_modulation = decay;
    bridge->effects.total_decay_modulation = decay;
    return decay;
}

bool eligibility_fep_should_consolidate(const eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    if (!bridge->config.enable_fe_gated_consolidation) return true;
    return bridge->effects.free_energy_value > bridge->config.fe_consolidation_threshold;
}

float eligibility_fep_get_effective_decay(const eligibility_fep_bridge_t* bridge, float base_decay) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_get_effective_decay: bridge is NULL");
        return base_decay;
    }
    return base_decay * bridge->effects.total_decay_modulation;
}

int eligibility_fep_report_consolidation(eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_report_consolidation: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.trace_consolidations++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int eligibility_fep_bridge_update(eligibility_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_update: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->fep_system) {
        float pe_scaling = eligibility_fep_apply_pe_eligibility(bridge, bridge->effects.pe_magnitude);
        float precision_decay = eligibility_fep_apply_precision_decay_modulation(bridge, bridge->effects.precision_value);
        bridge->effects.pe_trace_scaling = pe_scaling;
        bridge->effects.precision_decay_modulation = precision_decay;
        bridge->effects.total_decay_modulation = precision_decay;
        bridge->state.consolidation_active = eligibility_fep_should_consolidate(bridge);
    }
    bridge->stats.total_updates++;
    bridge->state.last_update_time = delta_ms;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int eligibility_fep_bridge_get_state(const eligibility_fep_bridge_t* bridge, eligibility_fep_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_get_state: state is NULL");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int eligibility_fep_bridge_get_stats(const eligibility_fep_bridge_t* bridge, eligibility_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int eligibility_fep_bridge_connect_bio_async(eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_ELIGIBILITY_BRIDGE,
        .module_name = "eligibility_fep_bridge",
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

int eligibility_fep_bridge_disconnect_bio_async(eligibility_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected - success */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool eligibility_fep_bridge_is_bio_async_connected(const eligibility_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
