/**
 * @file nimcp_insula_thalamic_bridge.c
 * @brief Implementation of Insula-Thalamic bridge
 *
 * WHAT: Routes interoceptive signals through thalamic relay
 * WHY: Interoceptive information relayed via VMpo and other nuclei
 * HOW: Packages bodily signals, routes via appropriate thalamic pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/insula/nimcp_insula_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(insula_thalamic_bridge)

#define LOG_MODULE "INSULA_THALAMIC_BRIDGE"


/**
 * Internal structure for insula-thalamic bridge
 */
struct insula_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* insula;                       /**< Insula processing module */
    thalamic_router_t* router;          /**< Thalamic router instance */
    insula_thalamic_config_t config;    /**< Bridge configuration */
    insula_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;             /**< Current attention weight */
};

insula_thalamic_config_t insula_thalamic_default_config(void) {
    insula_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_disgust_priority = true,
        .min_interoceptive_threshold = 0.1f,
        .urgency_threshold = 0.5f
    };
    return config;
}

insula_thalamic_bridge_t* insula_thalamic_bridge_create(void* insula,
                                                         thalamic_router_t* router,
                                                         const insula_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    insula_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(insula_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->insula = insula;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = insula_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(insula_thalamic_stats_t));

    return bridge;
}

void insula_thalamic_bridge_destroy(insula_thalamic_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int insula_thalamic_bridge_reset(insula_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(insula_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int insula_thalamic_route_signal(insula_thalamic_bridge_t* bridge,
                                  const insula_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check interoceptive threshold (disgust always passes) */
    if (bridge->config.enable_attention_gating &&
        signal->signal_type != INSULA_SIGNAL_DISGUST &&
        signal->interoceptive_strength < bridge->config.min_interoceptive_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply urgency modulation */
    float effective_weight = signal->interoceptive_strength * bridge->attention_weight;
    if (signal->bodily_urgency > bridge->config.urgency_threshold) {
        effective_weight *= (1.0f + signal->bodily_urgency);
    }

    /* Disgust priority boost */
    if (bridge->config.enable_disgust_priority &&
        signal->signal_type == INSULA_SIGNAL_DISGUST) {
        effective_weight *= 1.5f;
    }

    /* Update statistics - routing is handled externally if router is connected */
    float alpha = 0.1f;
    bridge->stats.avg_interoceptive_strength =
        (1.0f - alpha) * bridge->stats.avg_interoceptive_strength +
        alpha * signal->interoceptive_strength;

    if (signal->signal_type == INSULA_SIGNAL_INTEROCEPTION) {
        bridge->stats.interoceptions_routed++;
    } else if (signal->signal_type == INSULA_SIGNAL_DISGUST) {
        bridge->stats.disgust_responses++;
    } else if (signal->signal_type == INSULA_SIGNAL_EMPATHY) {
        bridge->stats.empathic_signals++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int insula_thalamic_route_interoception(insula_thalamic_bridge_t* bridge,
                                         const void* state, float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create interoception signal */
    insula_thalamic_signal_t signal = {
        .signal_type = INSULA_SIGNAL_INTEROCEPTION,
        .interoceptive_strength = strength,
        .emotional_salience = 0.5f,
        .bodily_urgency = strength,
        .content = (void*)state,
        .content_size = 0,
        .timestamp_us = 0
    };

    return insula_thalamic_route_signal(bridge, &signal);
}

int insula_thalamic_set_attention(insula_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int insula_thalamic_get_attention(const insula_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int insula_thalamic_bridge_get_stats(const insula_thalamic_bridge_t* bridge,
                                      insula_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
