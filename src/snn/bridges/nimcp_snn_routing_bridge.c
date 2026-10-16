/**
 * @file nimcp_snn_routing_bridge.c
 * @brief Implementation of SNN-Routing bridge
 */

#include "snn/bridges/nimcp_snn_routing_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_routing_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

void snn_routing_config_default(snn_routing_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_config_default: null config pointer");
        return;
    }

    memset(config, 0, sizeof(snn_routing_config_t));

    config->attention_threshold = 0.1f;
    config->default_priority = SIGNAL_PRIORITY_NORMAL;
    config->max_queue_size = 1000;
    config->max_destinations = 8;

    config->enable_burst_routing = true;
    config->enable_population_broadcast = false;
    config->enable_selective_routing = true;
    config->burst_detection_threshold = 10.0f; /* 10ms ISI for burst */

    config->enable_attention_gating = true;
    config->min_firing_rate_hz = 1.0f;
    config->max_firing_rate_hz = 500.0f;

    config->enable_bio_async = false;
    config->update_interval_ms = 10.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_routing_bridge_t* snn_routing_bridge_create(
    const snn_routing_config_t* config,
    snn_network_t* network,
    thalamic_router_t* router
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_LOGGING_ERROR("SNN network is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_create: network is NULL");
        return NULL;
    }
    if (!router) {
        NIMCP_LOGGING_ERROR("Thalamic router is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_create: router is NULL");
        return NULL;
    }

    /* WHAT: Allocate bridge structure */
    snn_routing_bridge_t* bridge = (snn_routing_bridge_t*)nimcp_malloc(
        sizeof(snn_routing_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN routing bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_routing_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* WHY: Initialize all fields to zero */
    memset(bridge, 0, sizeof(snn_routing_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_routing") != 0) { nimcp_free(bridge); return NULL; }

    /* HOW: Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        snn_routing_config_default(&bridge->config);
    }

    bridge->network = network;
    bridge->router = router;

    /* WHAT: Allocate routing state arrays */
    uint32_t max_neurons = 0;
    uint32_t max_pops = network->n_populations;

    /* Calculate max neurons across all populations */
    for (uint32_t i = 0; i < max_pops; i++) {
        snn_population_t* pop = network->populations[i];
        if (pop && pop->n_neurons > max_neurons) {
            max_neurons = pop->n_neurons;
        }
    }

    /* Route map: population ID → destination ID */
    bridge->route_map = (uint32_t*)nimcp_malloc(max_pops * sizeof(uint32_t));
    if (!bridge->route_map) {
        NIMCP_LOGGING_ERROR("Failed to allocate route map");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_routing_bridge_create: failed to allocate route_map");
        snn_routing_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->route_map, 0xFF, max_pops * sizeof(uint32_t)); /* 0xFF = no route */

    /* Attention weights per population */
    bridge->attention_weights = (float*)nimcp_malloc(max_pops * sizeof(float));
    if (!bridge->attention_weights) {
        NIMCP_LOGGING_ERROR("Failed to allocate attention weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_routing_bridge_create: failed to allocate attention_weights");
        snn_routing_bridge_destroy(bridge);
        return NULL;
    }
    for (uint32_t i = 0; i < max_pops; i++) {
        bridge->attention_weights[i] = 1.0f; /* Default: full attention */
    }

    /* Burst detection state */
    bridge->last_spike_time_us = (uint64_t*)nimcp_malloc(max_neurons * sizeof(uint64_t));
    if (!bridge->last_spike_time_us) {
        NIMCP_LOGGING_ERROR("Failed to allocate burst detection state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_routing_bridge_create: failed to allocate last_spike_time_us");
        snn_routing_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->last_spike_time_us, 0, max_neurons * sizeof(uint64_t));

    bridge->in_burst_mode = (bool*)nimcp_malloc(max_neurons * sizeof(bool));
    if (!bridge->in_burst_mode) {
        NIMCP_LOGGING_ERROR("Failed to allocate burst mode flags");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_routing_bridge_create: failed to allocate in_burst_mode");
        snn_routing_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->in_burst_mode, 0, max_neurons * sizeof(bool));

    bridge->connected = true;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("SNN routing bridge created successfully");
    return bridge;
}

void snn_routing_bridge_destroy(snn_routing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_routing_bridge_disconnect_bio_async(bridge);
    }

    /* Free allocated arrays */
    if (bridge->route_map) nimcp_free(bridge->route_map);
    if (bridge->attention_weights) nimcp_free(bridge->attention_weights);
    if (bridge->last_spike_time_us) nimcp_free(bridge->last_spike_time_us);
    if (bridge->in_burst_mode) nimcp_free(bridge->in_burst_mode);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SNN routing bridge destroyed");
}

int snn_routing_bridge_connect_bio_async(snn_routing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Bio-async integration would go here */
    /* For now, just set flag */
    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("SNN routing bridge connected to bio-async");

    return 0;
}

int snn_routing_bridge_disconnect_bio_async(snn_routing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("SNN routing bridge disconnected from bio-async");

    return 0;
}

bool snn_routing_bridge_is_bio_async_connected(const snn_routing_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Routing Functions
//=============================================================================

int snn_routing_bridge_process(
    snn_routing_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_process: null bridge pointer");
        return -1;
    }
    if (!spikes_in || n_spikes == 0) {
        if (n_out_actual) *n_out_actual = 0;
        return 0;
    }
    if (!spikes_out || !n_out_actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_process: required parameter is NULL (spikes_out, n_out_actual)");
        return -1;
    }

    uint32_t n_routed = 0;

    /* WHAT: Process each spike */
    for (uint32_t i = 0; i < n_spikes && n_routed < n_out_capacity; i++) {
        const snn_spike_t* spike = &spikes_in[i];
        uint32_t pop_id = spike->population_id;

        /* WHY: Check if population has valid route */
        if (pop_id >= bridge->network->n_populations) continue;
        if (bridge->route_map[pop_id] == 0xFFFFFFFF) continue; /* No route */

        /* HOW: Apply attention gating */
        float attention = bridge->attention_weights[pop_id];
        if (bridge->config.enable_attention_gating &&
            attention < bridge->config.attention_threshold) {
            bridge->stats.spikes_filtered++;
            continue;
        }

        /* Detect burst if enabled */
        bool is_burst = false;
        if (bridge->config.enable_burst_routing) {
            is_burst = snn_routing_bridge_detect_burst(
                bridge, pop_id, spike->neuron_id, spike->timestamp_us);
            if (is_burst) {
                bridge->stats.bursts_detected++;
            }
        }

        /* Create routed spike */
        spikes_out[n_routed] = *spike;

        /* High priority for bursts */
        if (is_burst) {
            /* Mark as high priority (implementation-specific) */
        }

        n_routed++;
        bridge->stats.spikes_routed++;
    }

    *n_out_actual = n_routed;

    return 0;
}

int snn_routing_bridge_update(snn_routing_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_update: null bridge pointer");
        return -1;
    }

    bridge->last_update_time += dt;

    /* Update routing statistics */
    if (bridge->stats.spikes_routed > 0) {
        float elapsed_sec = bridge->last_update_time / 1000.0f;
        if (elapsed_sec > 0.0f) {
            bridge->stats.throughput_hz = bridge->stats.spikes_routed / elapsed_sec;
        }
    }

    return 0;
}

int snn_routing_bridge_set_attention(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    float attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_set_attention: null bridge pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_set_attention: invalid pop_id");
        return -1;
    }
    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_set_attention: attention out of range [0,1]");
        return -1;
    }

    bridge->attention_weights[pop_id] = attention;
    return 0;
}

int snn_routing_bridge_get_attention(
    const snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    float* attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_get_attention: null bridge pointer");
        return -1;
    }
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_get_attention: null attention pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_get_attention: invalid pop_id");
        return -1;
    }

    *attention = bridge->attention_weights[pop_id];
    return 0;
}

int snn_routing_bridge_add_route(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t dest_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_add_route: null bridge pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_add_route: invalid pop_id");
        return -1;
    }

    bridge->route_map[pop_id] = dest_id;
    bridge->stats.active_routes++;
    return 0;
}

int snn_routing_bridge_remove_route(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t dest_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_remove_route: null bridge pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_remove_route: invalid pop_id");
        return -1;
    }

    if (bridge->route_map[pop_id] == dest_id) {
        bridge->route_map[pop_id] = 0xFFFFFFFF; /* No route */
        bridge->stats.active_routes--;
    }

    return 0;
}

void snn_routing_bridge_clear_routes(snn_routing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_clear_routes: null bridge pointer");
        return;
    }

    memset(bridge->route_map, 0xFF,
           bridge->network->n_populations * sizeof(uint32_t));
    bridge->stats.active_routes = 0;
}

//=============================================================================
// Burst Detection
//=============================================================================

bool snn_routing_bridge_detect_burst(
    snn_routing_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    uint64_t spike_time_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_detect_burst: null bridge pointer");
        return false;
    }

    /* WHAT: Calculate ISI (inter-spike interval) */
    uint64_t last_time = bridge->last_spike_time_us[neuron_idx];
    bridge->last_spike_time_us[neuron_idx] = spike_time_us;

    if (last_time == 0) {
        /* First spike, not a burst */
        bridge->in_burst_mode[neuron_idx] = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_routing_bridge_detect_burst: last_time is zero");
        return false;
    }

    /* WHY: Short ISI indicates burst */
    uint64_t isi_us = spike_time_us - last_time;
    float isi_ms = isi_us / 1000.0f;

    /* HOW: Compare ISI to threshold */
    bool is_burst = (isi_ms < bridge->config.burst_detection_threshold);
    bridge->in_burst_mode[neuron_idx] = is_burst;

    return is_burst;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_routing_bridge_get_stats(
    const snn_routing_bridge_t* bridge,
    snn_routing_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_get_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_get_stats: null stats pointer");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void snn_routing_bridge_reset_stats(snn_routing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_routing_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->stats, 0, sizeof(snn_routing_stats_t));
    bridge->last_update_time = 0.0f;
}
