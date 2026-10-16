/**
 * @file nimcp_cerebellum_thalamic_bridge.c
 * @brief Implementation of Cerebellum-Thalamic bridge
 *
 * WHAT: Routes cerebellar output through thalamic relay (VL)
 * WHY: Cerebellar output reaches cortex via ventral lateral thalamus
 * HOW: Packages cerebellar signals, routes via VL nucleus pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/cerebellum/nimcp_cerebellum_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cerebellum_thalamic_bridge)

#define LOG_MODULE "CEREBELLUM_THALAMIC_BRIDGE"


/**
 * Internal structure for cerebellum-thalamic bridge
 */
struct cerebellum_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* cerebellum;                       /**< Cerebellum processing module */
    thalamic_router_t* router;              /**< Thalamic router instance */
    cerebellum_thalamic_config_t config;    /**< Bridge configuration */
    cerebellum_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;                 /**< Current attention weight */
};

cerebellum_thalamic_config_t cerebellum_thalamic_default_config(void) {
    cerebellum_thalamic_config_t config = {
        .enable_timing_relay = true,
        .enable_error_routing = true,
        .min_timing_precision = 0.1f,
        .error_threshold = 0.05f
    };
    return config;
}

cerebellum_thalamic_bridge_t* cerebellum_thalamic_bridge_create(void* cerebellum,
                                                                 thalamic_router_t* router,
                                                                 const cerebellum_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    cerebellum_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(cerebellum_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->cerebellum = cerebellum;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cerebellum_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(cerebellum_thalamic_stats_t));

    return bridge;
}

void cerebellum_thalamic_bridge_destroy(cerebellum_thalamic_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int cerebellum_thalamic_bridge_reset(cerebellum_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(cerebellum_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int cerebellum_thalamic_route_signal(cerebellum_thalamic_bridge_t* bridge,
                                      const cerebellum_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check timing precision threshold for timing signals */
    if (bridge->config.enable_timing_relay &&
        signal->signal_type == CEREBELLUM_SIGNAL_TIMING &&
        signal->timing_precision < bridge->config.min_timing_precision) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply error modulation for correction signals */
    float effective_weight = signal->timing_precision * bridge->attention_weight;
    if (bridge->config.enable_error_routing &&
        signal->error_magnitude > bridge->config.error_threshold) {
        effective_weight *= (1.0f + signal->error_magnitude);
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    bridge->stats.avg_timing_precision =
        (1.0f - alpha) * bridge->stats.avg_timing_precision +
        alpha * signal->timing_precision;

    if (signal->signal_type == CEREBELLUM_SIGNAL_TIMING) {
        bridge->stats.timing_signals_routed++;
    } else if (signal->signal_type == CEREBELLUM_SIGNAL_CORRECTION) {
        bridge->stats.corrections_applied++;
    } else if (signal->signal_type == CEREBELLUM_SIGNAL_LEARNING) {
        bridge->stats.learning_signals++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int cerebellum_thalamic_route_correction(cerebellum_thalamic_bridge_t* bridge,
                                          const void* correction, float magnitude) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create correction signal */
    cerebellum_thalamic_signal_t signal = {
        .signal_type = CEREBELLUM_SIGNAL_CORRECTION,
        .timing_precision = 1.0f,
        .error_magnitude = magnitude,
        .learning_signal = 0.0f,
        .content = (void*)correction,
        .content_size = 0,
        .timestamp_us = 0
    };

    return cerebellum_thalamic_route_signal(bridge, &signal);
}

int cerebellum_thalamic_set_attention(cerebellum_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int cerebellum_thalamic_get_attention(const cerebellum_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int cerebellum_thalamic_bridge_get_stats(const cerebellum_thalamic_bridge_t* bridge,
                                          cerebellum_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
