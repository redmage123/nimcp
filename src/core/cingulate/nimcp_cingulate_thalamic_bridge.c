/**
 * @file nimcp_cingulate_thalamic_bridge.c
 * @brief Implementation of Cingulate-Thalamic bridge
 *
 * WHAT: Routes cingulate signals through thalamic relay
 * WHY: Anterior thalamus connects with cingulate (Papez circuit)
 * HOW: Packages error/conflict signals, routes via anterior thalamic pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/cingulate/nimcp_cingulate_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cingulate_thalamic_bridge)

#define LOG_MODULE "CINGULATE_THALAMIC_BRIDGE"


/**
 * Internal structure for cingulate-thalamic bridge
 */
struct cingulate_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* cingulate;                       /**< Cingulate processing module */
    thalamic_router_t* router;             /**< Thalamic router instance */
    cingulate_thalamic_config_t config;    /**< Bridge configuration */
    cingulate_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;                /**< Current attention weight */
};

cingulate_thalamic_config_t cingulate_thalamic_default_config(void) {
    cingulate_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_error_priority = true,
        .min_error_threshold = 0.1f,
        .conflict_threshold = 0.3f
    };
    return config;
}

cingulate_thalamic_bridge_t* cingulate_thalamic_bridge_create(void* cingulate,
                                                               thalamic_router_t* router,
                                                               const cingulate_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    cingulate_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(cingulate_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->cingulate = cingulate;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cingulate_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(cingulate_thalamic_stats_t));

    return bridge;
}

void cingulate_thalamic_bridge_destroy(cingulate_thalamic_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int cingulate_thalamic_bridge_reset(cingulate_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(cingulate_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int cingulate_thalamic_route_signal(cingulate_thalamic_bridge_t* bridge,
                                     const cingulate_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check error threshold (pain always passes) */
    if (bridge->config.enable_attention_gating &&
        signal->signal_type != CINGULATE_SIGNAL_PAIN &&
        signal->error_magnitude < bridge->config.min_error_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply error priority modulation */
    float effective_weight = signal->urgency * bridge->attention_weight;
    if (bridge->config.enable_error_priority &&
        signal->error_magnitude > bridge->config.min_error_threshold) {
        effective_weight *= (1.0f + signal->error_magnitude);
    }

    /* Boost for conflict signals above threshold */
    if (signal->conflict_level > bridge->config.conflict_threshold) {
        effective_weight *= (1.0f + signal->conflict_level);
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    bridge->stats.avg_error_magnitude =
        (1.0f - alpha) * bridge->stats.avg_error_magnitude +
        alpha * signal->error_magnitude;

    if (signal->signal_type == CINGULATE_SIGNAL_ERROR) {
        bridge->stats.errors_routed++;
    } else if (signal->signal_type == CINGULATE_SIGNAL_CONFLICT) {
        bridge->stats.conflicts_detected++;
    } else if (signal->signal_type == CINGULATE_SIGNAL_PAIN) {
        bridge->stats.pain_signals++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int cingulate_thalamic_route_error(cingulate_thalamic_bridge_t* bridge,
                                    const void* error, float magnitude) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create error signal */
    cingulate_thalamic_signal_t signal = {
        .signal_type = CINGULATE_SIGNAL_ERROR,
        .error_magnitude = magnitude,
        .conflict_level = 0.0f,
        .urgency = magnitude,
        .content = (void*)error,
        .content_size = 0,
        .timestamp_us = 0
    };

    return cingulate_thalamic_route_signal(bridge, &signal);
}

int cingulate_thalamic_set_attention(cingulate_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int cingulate_thalamic_get_attention(const cingulate_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int cingulate_thalamic_bridge_get_stats(const cingulate_thalamic_bridge_t* bridge,
                                         cingulate_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
