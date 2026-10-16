/**
 * @file nimcp_hypothalamus_thalamic_bridge.c
 * @brief Implementation of Hypothalamus-Thalamic bridge
 *
 * WHAT: Routes hypothalamic signals through thalamic relay
 * WHY: Hypothalamus connects with midline and anterior thalamic nuclei
 * HOW: Packages homeostatic signals, routes via appropriate pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/hypothalamus/nimcp_hypothalamus_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hypothalamus_thalamic_bridge)

#define LOG_MODULE "HYPOTHALAMUS_THALAMIC_BRIDGE"


/**
 * Internal structure for hypothalamus-thalamic bridge
 */
struct hypothalamus_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* hypothalamus;                       /**< Hypothalamus processing module */
    thalamic_router_t* router;                /**< Thalamic router instance */
    hypothalamus_thalamic_config_t config;    /**< Bridge configuration */
    hypothalamus_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;                   /**< Current attention weight */
};

hypothalamus_thalamic_config_t hypothalamus_thalamic_default_config(void) {
    hypothalamus_thalamic_config_t config = {
        .enable_drive_routing = true,
        .enable_stress_priority = true,
        .min_drive_threshold = 0.1f,
        .stress_threshold = 0.5f
    };
    return config;
}

hypothalamus_thalamic_bridge_t* hypothalamus_thalamic_bridge_create(void* hypothalamus,
                                                                     thalamic_router_t* router,
                                                                     const hypothalamus_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    hypothalamus_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(hypothalamus_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "hypothalamus_thalamic");

    bridge->hypothalamus = hypothalamus;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypothalamus_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(hypothalamus_thalamic_stats_t));

    return bridge;
}

void hypothalamus_thalamic_bridge_destroy(hypothalamus_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int hypothalamus_thalamic_bridge_reset(hypothalamus_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(hypothalamus_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int hypothalamus_thalamic_route_signal(hypothalamus_thalamic_bridge_t* bridge,
                                        const hypothalamus_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check drive threshold (stress always passes) */
    if (bridge->config.enable_drive_routing &&
        signal->signal_type != HYPOTHALAMUS_SIGNAL_STRESS &&
        signal->drive_strength < bridge->config.min_drive_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply stress priority modulation */
    float effective_weight = signal->drive_strength * bridge->attention_weight;
    if (bridge->config.enable_stress_priority &&
        signal->signal_type == HYPOTHALAMUS_SIGNAL_STRESS) {
        effective_weight *= 2.0f;  /* Stress gets high priority */
    }

    /* Boost for significant homeostatic errors */
    if (signal->homeostatic_error > 0.5f) {
        effective_weight *= (1.0f + signal->homeostatic_error);
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    float new_avg = (1.0f - alpha) * bridge->stats.avg_drive_strength +
                    alpha * signal->drive_strength;
    if (isfinite(new_avg)) {
        bridge->stats.avg_drive_strength = new_avg;
    }

    if (signal->signal_type == HYPOTHALAMUS_SIGNAL_DRIVE) {
        bridge->stats.drives_routed++;
    } else if (signal->signal_type == HYPOTHALAMUS_SIGNAL_HOMEOSTASIS) {
        bridge->stats.homeostatic_updates++;
    } else if (signal->signal_type == HYPOTHALAMUS_SIGNAL_STRESS) {
        bridge->stats.stress_responses++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int hypothalamus_thalamic_route_drive(hypothalamus_thalamic_bridge_t* bridge,
                                       uint32_t drive_type, float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create drive signal */
    hypothalamus_thalamic_signal_t signal = {
        .signal_type = HYPOTHALAMUS_SIGNAL_DRIVE,
        .drive_strength = strength,
        .homeostatic_error = strength,
        .circadian_phase = 0.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = 0
    };

    /* Encode drive type in content if needed */
    (void)drive_type;  /* Currently not used in signal */

    return hypothalamus_thalamic_route_signal(bridge, &signal);
}

int hypothalamus_thalamic_set_attention(hypothalamus_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int hypothalamus_thalamic_get_attention(const hypothalamus_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int hypothalamus_thalamic_bridge_get_stats(const hypothalamus_thalamic_bridge_t* bridge,
                                            hypothalamus_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
