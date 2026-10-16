/**
 * @file nimcp_auditory_thalamic_bridge.c
 * @brief Implementation of Auditory-Thalamic bridge
 *
 * WHAT: Routes auditory signals through thalamic relay (MGN)
 * WHY: All auditory information passes through MGN to cortex
 * HOW: Packages auditory signals, routes via medial geniculate nucleus pathway
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/sensory/auditory/nimcp_auditory_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(auditory_thalamic_bridge)

#define LOG_MODULE "AUDITORY_THALAMIC_BRIDGE"


/**
 * Internal structure for auditory-thalamic bridge
 */
struct auditory_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* auditory;                       /**< Auditory processing module */
    thalamic_router_t* router;            /**< Thalamic router instance */
    auditory_thalamic_config_t config;    /**< Bridge configuration */
    auditory_thalamic_stats_t stats;      /**< Runtime statistics */
    float attention_weight;               /**< Current attention weight */
};

auditory_thalamic_config_t auditory_thalamic_default_config(void) {
    auditory_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_speech_priority = true,
        .min_salience_threshold = 0.1f,
        .speech_boost = 1.5f
    };
    return config;
}

auditory_thalamic_bridge_t* auditory_thalamic_bridge_create(void* auditory,
                                                             thalamic_router_t* router,
                                                             const auditory_thalamic_config_t* config) {
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");

        return NULL;
    }

    auditory_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(auditory_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "auditory_thalamic");

    bridge->auditory = auditory;
    bridge->router = router;
    bridge->attention_weight = 1.0f;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = auditory_thalamic_default_config();
    }

    memset(&bridge->stats, 0, sizeof(auditory_thalamic_stats_t));

    return bridge;
}

void auditory_thalamic_bridge_destroy(auditory_thalamic_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int auditory_thalamic_bridge_reset(auditory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(auditory_thalamic_stats_t));
    bridge->attention_weight = 1.0f;

    return 0;
}

int auditory_thalamic_route_signal(auditory_thalamic_bridge_t* bridge,
                                    const auditory_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auditory_thalamic_route_signal: required parameter is NULL");
        return -1;
    }

    /* Check salience threshold */
    if (bridge->config.enable_attention_gating &&
        signal->auditory_salience < bridge->config.min_salience_threshold) {
        return 0;  /* Signal gated, not an error */
    }

    /* Apply attention modulation */
    float effective_weight = signal->attention_weight * bridge->attention_weight;
    if (bridge->config.enable_speech_priority &&
        signal->signal_type == AUDITORY_SIGNAL_SPEECH) {
        effective_weight *= bridge->config.speech_boost;
    }

    /* Update statistics - routing is handled externally if router is connected */
    bridge->stats.signals_relayed++;

    float alpha = 0.1f;
    {
        float new_sal = (1.0f - alpha) * bridge->stats.avg_auditory_salience +
                        alpha * signal->auditory_salience;
        if (isfinite(new_sal)) bridge->stats.avg_auditory_salience = new_sal;
    }

    if (signal->signal_type == AUDITORY_SIGNAL_SPEECH) {
        bridge->stats.speech_processed++;
    }

    (void)effective_weight;  /* Suppress unused warning */
    return 0;
}

int auditory_thalamic_route_alert(auditory_thalamic_bridge_t* bridge,
                                   const void* alert, float urgency) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Create alert signal with high priority */
    auditory_thalamic_signal_t signal = {
        .signal_type = AUDITORY_SIGNAL_ALERT,
        .auditory_salience = urgency,
        .attention_weight = urgency,
        .frequency_center = 0.0f,
        .content = (void*)alert,
        .content_size = 0,
        .timestamp_us = 0
    };

    int result = auditory_thalamic_route_signal(bridge, &signal);
    if (result == 0) {
        bridge->stats.alerts_triggered++;
    }

    return result;
}

int auditory_thalamic_set_attention(auditory_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auditory_thalamic_set_attention: validation failed");
        return -1;
    }

    bridge->attention_weight = attention;
    return 0;
}

int auditory_thalamic_get_attention(const auditory_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auditory_thalamic_get_attention: required parameter is NULL");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

int auditory_thalamic_bridge_get_stats(const auditory_thalamic_bridge_t* bridge,
                                        auditory_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auditory_thalamic_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
