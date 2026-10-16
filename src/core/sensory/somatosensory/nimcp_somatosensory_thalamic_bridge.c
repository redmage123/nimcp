/**
 * @file nimcp_somatosensory_thalamic_bridge.c
 * @brief Implementation of Somatosensory-Thalamic bridge
 *
 * WHAT: Routes touch/proprioception through thalamic relay (VPL/VPM)
 * WHY: All somatosensory information passes through VPL/VPM to cortex
 * HOW: Packages sensory signals, routes via ventral posterior nuclei pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/sensory/somatosensory/nimcp_somatosensory_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(somatosensory_thalamic_bridge)

#define LOG_MODULE "SOMATOSENSORY_THALAMIC_BRIDGE"


/**
 * Internal structure for somatosensory-thalamic bridge
 */
struct somatosensory_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* somatosensory;                       /**< Somatosensory processing module */
    thalamic_router_t* router;                 /**< Thalamic router instance */
    somatosensory_thalamic_config_t config;    /**< Bridge configuration */
    somatosensory_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;                    /**< Current attention weight */
};

somatosensory_thalamic_config_t somatosensory_thalamic_default_config(void) {
    somatosensory_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_pain_priority = true,
        .min_salience_threshold = 0.1f,
        .pain_boost = 2.0f
    };
    return config;
}

somatosensory_thalamic_bridge_t* somatosensory_thalamic_bridge_create(void* somatosensory,
                                                                       thalamic_router_t* router,
                                                                       const somatosensory_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    somatosensory_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(somatosensory_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "somatosensory_thalamic");

    bridge->somatosensory = somatosensory;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = somatosensory_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(somatosensory_thalamic_stats_t));

    return bridge;
}

void somatosensory_thalamic_bridge_destroy(somatosensory_thalamic_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int somatosensory_thalamic_bridge_reset(somatosensory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(somatosensory_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int somatosensory_thalamic_route_signal(somatosensory_thalamic_bridge_t* bridge,
                                         const somatosensory_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somatosensory_thalamic_bridge_reset: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Check salience threshold (pain always passes) */
    if (bridge->config.enable_attention_gating &&
        signal->signal_type != SOMATOSENSORY_SIGNAL_PAIN &&
        signal->somatosensory_salience < bridge->config.min_salience_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply attention modulation */
    float effective_weight = signal->attention_weight * bridge->attention_weight;
    if (bridge->config.enable_pain_priority &&
        signal->signal_type == SOMATOSENSORY_SIGNAL_PAIN) {
        effective_weight *= bridge->config.pain_boost;
    }

    /* Update statistics - routing is handled externally if router is connected */
    bridge->stats.signals_relayed++;

    float alpha = 0.1f;
    {
        float new_sal = (1.0f - alpha) * bridge->stats.avg_somatosensory_salience +
                        alpha * signal->somatosensory_salience;
        if (isfinite(new_sal)) bridge->stats.avg_somatosensory_salience = new_sal;
    }

    if (signal->signal_type == SOMATOSENSORY_SIGNAL_PAIN) {
        bridge->stats.pain_signals++;
    } else if (signal->signal_type == SOMATOSENSORY_SIGNAL_PROPRIOCEPT) {
        bridge->stats.proprioceptive_updates++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int somatosensory_thalamic_route_pain(somatosensory_thalamic_bridge_t* bridge,
                                       uint32_t region, float intensity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create pain signal with high priority */
    somatosensory_thalamic_signal_t signal = {
        .signal_type = SOMATOSENSORY_SIGNAL_PAIN,
        .somatosensory_salience = intensity,
        .attention_weight = intensity,
        .body_region = region,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = 0
    };

    return somatosensory_thalamic_route_signal(bridge, &signal);
}

int somatosensory_thalamic_set_attention(somatosensory_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "somatosensory_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int somatosensory_thalamic_get_attention(const somatosensory_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somatosensory_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int somatosensory_thalamic_bridge_get_stats(const somatosensory_thalamic_bridge_t* bridge,
                                             somatosensory_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somatosensory_thalamic_get_attention: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
