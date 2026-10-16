/**
 * @file nimcp_thalamic_router_fep_bridge.c
 * @brief Free Energy Principle - Thalamic Router Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "middleware/routing/nimcp_thalamic_router_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thalamic_router_fep_bridge)

int thalamic_router_fep_bridge_default_config(thalamic_router_fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }
    config->enable_precision_gain = true;
    config->enable_prediction_routing = true;
    config->enable_pe_priority_boost = true;
    config->enable_synchrony_confidence = true;
    config->gain_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->priority_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

thalamic_router_fep_bridge_t* thalamic_router_fep_bridge_create(
    const thalamic_router_fep_config_t* config
) {
    thalamic_router_fep_bridge_t* bridge = (thalamic_router_fep_bridge_t*)
        nimcp_calloc(1, sizeof(thalamic_router_fep_bridge_t));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(thalamic_router_fep_bridge_t),
        "thalamic_router_fep_bridge_create: failed to allocate bridge");

    thalamic_router_fep_config_t default_cfg;
    if (!config) {
        thalamic_router_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    bridge->effects.routing_gain_modifier = 1.0f;
    bridge->effects.pe_priority_level = SIGNAL_PRIORITY_NORMAL;
    bridge->state.current_precision = 0.5f;

    if (bridge_base_init(&bridge->base, 0, "thalamic_router_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        thalamic_router_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Thalamic-FEP bridge created");
    return bridge;
}

void thalamic_router_fep_bridge_destroy(thalamic_router_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        thalamic_router_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Thalamic-FEP bridge destroyed");
}

int thalamic_router_fep_bridge_connect_router(
    thalamic_router_fep_bridge_t* bridge,
    thalamic_router_t* router
) {
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_bridge_connect_router: required parameter is NULL (bridge, router)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->thalamic_router = router;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Thalamic router connected to FEP bridge");
    return 0;
}

int thalamic_router_fep_bridge_connect_fep(
    thalamic_router_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("FEP system connected to thalamic bridge");
    return 0;
}

int thalamic_router_fep_bridge_disconnect(thalamic_router_fep_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->thalamic_router = NULL;
    bridge->fep_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Thalamic-FEP bridge disconnected");
    return 0;
}

int thalamic_router_fep_apply_precision_gain(
    thalamic_router_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_precision_gain) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float gain = (precision > 0.7f) ? FEP_PRECISION_HIGH_GAIN :
                 (precision > 0.3f) ? 1.0f : FEP_PRECISION_LOW_GAIN;
    gain *= bridge->config.gain_sensitivity;

    bridge->state.current_precision = precision;
    bridge->effects.routing_gain_modifier = gain;
    bridge->stats.precision_adjustments++;
    bridge->stats.avg_routing_gain =
        (bridge->stats.avg_routing_gain * (bridge->stats.precision_adjustments - 1) +
         gain) / bridge->stats.precision_adjustments;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Precision gain applied: %.3f → %.3f", precision, gain);
    return 0;
}

int thalamic_router_fep_route_prediction(
    thalamic_router_fep_bridge_t* bridge,
    float prediction
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_prediction_routing) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->effects.prediction_routing_weight = prediction;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Prediction routing: %.3f", prediction);
    return 0;
}

int thalamic_router_fep_boost_pe_priority(
    thalamic_router_fep_bridge_t* bridge,
    float prediction_error
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_pe_priority_boost) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    signal_priority_t priority = (prediction_error > FEP_PE_HIGH_PRIORITY_THRESHOLD) ?
                                 SIGNAL_PRIORITY_HIGH : SIGNAL_PRIORITY_NORMAL;

    bridge->state.current_pe = prediction_error;
    bridge->effects.pe_priority_level = priority;

    if (priority == SIGNAL_PRIORITY_HIGH) {
        bridge->stats.pe_priority_boosts++;
    }

    bridge->stats.avg_pe =
        (bridge->stats.avg_pe * bridge->stats.precision_adjustments + prediction_error) /
        (bridge->stats.precision_adjustments + 1);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("PE priority boost: %.3f → %d", prediction_error, priority);
    return 0;
}

int thalamic_router_fep_report_routed_signal(
    thalamic_router_fep_bridge_t* bridge,
    const routed_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_report_routed_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->state.signals_routed++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Signal routed to FEP");
    return 0;
}

int thalamic_router_fep_update_confidence_from_routing(
    thalamic_router_fep_bridge_t* bridge,
    float synchrony
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_synchrony_confidence) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->state.routing_synchrony = synchrony;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Routing synchrony: %.3f", synchrony);
    return 0;
}

int thalamic_router_fep_bridge_update(
    thalamic_router_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    return 0;
}

int thalamic_router_fep_bridge_get_state(
    const thalamic_router_fep_bridge_t* bridge,
    thalamic_router_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    return 0;
}

int thalamic_router_fep_bridge_get_stats(
    const thalamic_router_fep_bridge_t* bridge,
    thalamic_router_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    return 0;
}

int thalamic_router_fep_bridge_connect_bio_async(
    thalamic_router_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_THALAMIC_ROUTER_BRIDGE,
        .module_name = "thalamic_router_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;
}

int thalamic_router_fep_bridge_disconnect_bio_async(
    thalamic_router_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool thalamic_router_fep_bridge_is_bio_async_connected(
    const thalamic_router_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
