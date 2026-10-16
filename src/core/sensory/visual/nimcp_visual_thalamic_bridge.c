/**
 * @file nimcp_visual_thalamic_bridge.c
 * @brief Implementation of Visual-Thalamic bridge
 *
 * WHAT: Routes visual signals through thalamic relay (LGN)
 * WHY: All visual information passes through LGN to cortex
 * HOW: Tracks visual signal routing statistics
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/sensory/visual/nimcp_visual_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_thalamic_bridge)

#define LOG_MODULE "VISUAL_THALAMIC_BRIDGE"


struct visual_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* visual;
    thalamic_router_t* router;
    visual_thalamic_config_t config;
    visual_thalamic_stats_t stats;
    float attention_weight;
};

visual_thalamic_config_t visual_thalamic_default_config(void) {
    visual_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_pulvinar_modulation = true,
        .min_salience_threshold = 0.1f,
        .attention_boost = 1.5f
    };
    return config;
}

visual_thalamic_bridge_t* visual_thalamic_bridge_create(void* visual,
                                                         thalamic_router_t* router,
                                                         const visual_thalamic_config_t* config) {
    visual_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(visual_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_thalamic_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "visual_thalamic");

    bridge->visual = visual;
    bridge->router = router;
    bridge->config = config ? *config : visual_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(visual_thalamic_stats_t));

    return bridge;
}

void visual_thalamic_bridge_destroy(visual_thalamic_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int visual_thalamic_bridge_reset(visual_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(visual_thalamic_stats_t));
    bridge->attention_weight = 1.0f;
    return 0;
}

int visual_thalamic_route_signal(visual_thalamic_bridge_t* bridge,
                                  const visual_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_route_signal: required parameter is NULL");
        return -1;
    }

    if (bridge->config.enable_attention_gating &&
        signal->visual_salience < bridge->config.min_salience_threshold) {
        return 0;
    }

    bridge->stats.signals_relayed++;

    float alpha = 0.1f;
    {
        float new_sal = (1.0f - alpha) * bridge->stats.avg_visual_salience +
                        alpha * signal->visual_salience;
        if (isfinite(new_sal)) bridge->stats.avg_visual_salience = new_sal;
    }

    if (signal->signal_type == VISUAL_SIGNAL_FEATURE) {
        bridge->stats.features_routed++;
    }

    return 0;
}

int visual_thalamic_route_attention(visual_thalamic_bridge_t* bridge,
                                     uint32_t x, uint32_t y, float weight) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_route_attention: bridge is NULL");
        return -1;
    }

    visual_thalamic_signal_t signal = {
        .signal_type = VISUAL_SIGNAL_ATTENTION,
        .visual_salience = weight,
        .attention_weight = weight,
        .retinotopic_x = x,
        .retinotopic_y = y,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = 0
    };

    int result = visual_thalamic_route_signal(bridge, &signal);
    if (result == 0) {
        bridge->stats.attention_shifts++;
    }
    return result;
}

int visual_thalamic_set_attention(visual_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    return 0;
}

int visual_thalamic_get_attention(const visual_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_get_attention: required parameter is NULL");
        return -1;
    }
    *attention = bridge->attention_weight;
    return 0;
}

int visual_thalamic_bridge_get_stats(const visual_thalamic_bridge_t* bridge,
                                      visual_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_thalamic_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}
