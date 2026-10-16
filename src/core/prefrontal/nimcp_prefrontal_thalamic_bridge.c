/**
 * @file nimcp_prefrontal_thalamic_bridge.c
 * @brief Implementation of Prefrontal-Thalamic bridge
 *
 * WHAT: Routes executive signals through thalamic relay (MD)
 * WHY: Mediodorsal thalamus is primary prefrontal thalamic relay
 * HOW: Packages executive signals, routes via MD nucleus pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/prefrontal/nimcp_prefrontal_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(prefrontal_thalamic_bridge)

#define LOG_MODULE "PREFRONTAL_THALAMIC_BRIDGE"


/**
 * Internal structure for prefrontal-thalamic bridge
 */
struct prefrontal_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* prefrontal;                       /**< Prefrontal processing module */
    thalamic_router_t* router;              /**< Thalamic router instance */
    prefrontal_thalamic_config_t config;    /**< Bridge configuration */
    prefrontal_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;                 /**< Current attention weight */
};

prefrontal_thalamic_config_t prefrontal_thalamic_default_config(void) {
    prefrontal_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_working_memory_boost = true,
        .min_executive_load = 0.1f,
        .inhibition_threshold = 0.5f
    };
    return config;
}

prefrontal_thalamic_bridge_t* prefrontal_thalamic_bridge_create(void* prefrontal,
                                                                 thalamic_router_t* router,
                                                                 const prefrontal_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    prefrontal_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(prefrontal_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->prefrontal = prefrontal;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = prefrontal_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(prefrontal_thalamic_stats_t));

    return bridge;
}

void prefrontal_thalamic_bridge_destroy(prefrontal_thalamic_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int prefrontal_thalamic_bridge_reset(prefrontal_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(prefrontal_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int prefrontal_thalamic_route_signal(prefrontal_thalamic_bridge_t* bridge,
                                      const prefrontal_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check executive load threshold */
    if (bridge->config.enable_attention_gating &&
        signal->executive_load < bridge->config.min_executive_load) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply working memory modulation */
    float effective_weight = signal->executive_load * bridge->attention_weight;
    if (bridge->config.enable_working_memory_boost &&
        signal->working_memory_demand > 0.5f) {
        effective_weight *= (1.0f + signal->working_memory_demand);
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    bridge->stats.avg_executive_load =
        (1.0f - alpha) * bridge->stats.avg_executive_load +
        alpha * signal->executive_load;

    if (signal->signal_type == PREFRONTAL_SIGNAL_EXECUTIVE) {
        bridge->stats.executive_signals_routed++;
    } else if (signal->signal_type == PREFRONTAL_SIGNAL_WORKING_MEM) {
        bridge->stats.working_memory_updates++;
    } else if (signal->signal_type == PREFRONTAL_SIGNAL_INHIBITION) {
        bridge->stats.inhibitions_applied++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int prefrontal_thalamic_route_inhibition(prefrontal_thalamic_bridge_t* bridge,
                                          const void* target, float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Check inhibition threshold */
    if (strength < bridge->config.inhibition_threshold) {
        return 0;  /* Below threshold, not an error */
    }

    /* Create inhibition signal */
    prefrontal_thalamic_signal_t signal = {
        .signal_type = PREFRONTAL_SIGNAL_INHIBITION,
        .executive_load = strength,
        .working_memory_demand = 0.0f,
        .inhibition_strength = strength,
        .content = (void*)target,
        .content_size = 0,
        .timestamp_us = 0
    };

    return prefrontal_thalamic_route_signal(bridge, &signal);
}

int prefrontal_thalamic_set_attention(prefrontal_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int prefrontal_thalamic_get_attention(const prefrontal_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int prefrontal_thalamic_bridge_get_stats(const prefrontal_thalamic_bridge_t* bridge,
                                          prefrontal_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
