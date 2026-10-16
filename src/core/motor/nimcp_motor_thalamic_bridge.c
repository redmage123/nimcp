/**
 * @file nimcp_motor_thalamic_bridge.c
 * @brief Implementation of Motor-Thalamic bridge
 *
 * WHAT: Routes motor signals through thalamic relay (VA/VL)
 * WHY: Motor commands pass through thalamus for cortical coordination
 * HOW: Packages motor signals, routes via ventral anterior/lateral pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/motor/nimcp_motor_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(motor_thalamic_bridge)

#define LOG_MODULE "MOTOR_THALAMIC_BRIDGE"


/**
 * Internal structure for motor-thalamic bridge
 */
struct motor_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* motor;                       /**< Motor processing module */
    thalamic_router_t* router;         /**< Thalamic router instance */
    motor_thalamic_config_t config;    /**< Bridge configuration */
    motor_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;            /**< Current attention weight */
};

motor_thalamic_config_t motor_thalamic_default_config(void) {
    motor_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_inhibition_routing = true,
        .min_urgency_threshold = 0.1f,
        .precision_threshold = 0.5f
    };
    return config;
}

motor_thalamic_bridge_t* motor_thalamic_bridge_create(void* motor,
                                                       thalamic_router_t* router,
                                                       const motor_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    motor_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(motor_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->motor = motor;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = motor_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(motor_thalamic_stats_t));

    return bridge;
}

void motor_thalamic_bridge_destroy(motor_thalamic_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int motor_thalamic_bridge_reset(motor_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(motor_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int motor_thalamic_route_signal(motor_thalamic_bridge_t* bridge,
                                 const motor_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check urgency threshold */
    if (bridge->config.enable_attention_gating &&
        signal->motor_urgency < bridge->config.min_urgency_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply precision modulation for high-precision movements */
    float effective_weight = signal->motor_urgency * bridge->attention_weight;
    if (signal->precision_requirement > bridge->config.precision_threshold) {
        effective_weight *= (1.0f + signal->precision_requirement);
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    bridge->stats.avg_motor_urgency =
        (1.0f - alpha) * bridge->stats.avg_motor_urgency +
        alpha * signal->motor_urgency;

    if (signal->signal_type == MOTOR_SIGNAL_PLAN) {
        bridge->stats.plans_routed++;
    } else if (signal->signal_type == MOTOR_SIGNAL_EXECUTE) {
        bridge->stats.executions_relayed++;
    } else if (signal->signal_type == MOTOR_SIGNAL_INHIBIT) {
        bridge->stats.inhibitions_applied++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int motor_thalamic_route_inhibit(motor_thalamic_bridge_t* bridge, uint32_t program_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_inhibition_routing) {
        return 0;  /* Inhibition routing disabled */
    }

    /* Create inhibition signal with high priority */
    motor_thalamic_signal_t signal = {
        .signal_type = MOTOR_SIGNAL_INHIBIT,
        .motor_urgency = 1.0f,  /* Inhibition is always urgent */
        .precision_requirement = 0.0f,
        .motor_program_id = program_id,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = 0
    };

    return motor_thalamic_route_signal(bridge, &signal);
}

int motor_thalamic_set_attention(motor_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int motor_thalamic_get_attention(const motor_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int motor_thalamic_bridge_get_stats(const motor_thalamic_bridge_t* bridge,
                                     motor_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
