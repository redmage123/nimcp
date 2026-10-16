/**
 * @file nimcp_snn_buffer_bridge.c
 * @brief Implementation of SNN-Buffer bridge
 */

#include "snn/bridges/nimcp_snn_buffer_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_buffer_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

void snn_buffer_config_default(snn_buffer_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_config_default: null config pointer");
        return;
    }

    memset(config, 0, sizeof(snn_buffer_config_t));

    config->buffer_capacity = 1000;
    config->overflow = OVERFLOW_OVERWRITE;
    config->temporal_window_ms = 100.0f;

    config->enable_per_neuron_buffers = false;
    config->enable_population_buffers = true;
    config->enable_spike_replay = true;
    config->enable_delay_lines = true;

    config->enable_pattern_detection = false;
    config->min_pattern_isi_ms = 1.0f;
    config->max_pattern_isi_ms = 100.0f;

    config->min_delay_ms = 0.5f;
    config->max_delay_ms = 10.0f;
    config->delay_std_ms = 1.0f;

    config->enable_bio_async = false;
    config->update_interval_ms = 1.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_buffer_bridge_t* snn_buffer_bridge_create(
    const snn_buffer_config_t* config,
    snn_network_t* network
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_create: network is NULL");
        return NULL;
    }

    /* WHAT: Allocate bridge structure */
    snn_buffer_bridge_t* bridge = (snn_buffer_bridge_t*)nimcp_malloc(
        sizeof(snn_buffer_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN buffer bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_buffer_bridge_create: bridge is NULL");
        return NULL;
    }

    /* WHY: Initialize all fields to zero */
    memset(bridge, 0, sizeof(snn_buffer_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_buffer") != 0) { nimcp_free(bridge); return NULL; }

    /* HOW: Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        snn_buffer_config_default(&bridge->config);
    }

    bridge->network = network;

    /* WHAT: Determine number of buffers needed */
    if (bridge->config.enable_per_neuron_buffers) {
        /* One buffer per neuron */
        uint32_t total_neurons = 0;
        for (uint32_t i = 0; i < network->n_populations; i++) {
            total_neurons += network->populations[i]->n_neurons;
        }
        bridge->n_buffers = total_neurons;
    } else if (bridge->config.enable_population_buffers) {
        /* One buffer per population */
        bridge->n_buffers = network->n_populations;
    } else {
        /* Single global buffer */
        bridge->n_buffers = 1;
    }

    /* WHY: Allocate buffer array */
    bridge->buffers = (circular_buffer_t**)nimcp_malloc(
        bridge->n_buffers * sizeof(circular_buffer_t*));
    if (!bridge->buffers) {
        NIMCP_LOGGING_ERROR("Failed to allocate buffer array");
        snn_buffer_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_buffer_bridge_create: bridge->buffers is NULL");
        return NULL;
    }

    /* HOW: Create circular buffers */
    for (uint32_t i = 0; i < bridge->n_buffers; i++) {
        bridge->buffers[i] = circular_buffer_create(
            sizeof(snn_spike_t),
            bridge->config.buffer_capacity,
            bridge->config.overflow
        );
        if (!bridge->buffers[i]) {
            NIMCP_LOGGING_ERROR("Failed to create circular buffer %u", i);
            snn_buffer_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_buffer_bridge_create: bridge->buffers is NULL");
            return NULL;
        }
    }

    /* Buffer map */
    bridge->buffer_map = (uint32_t*)nimcp_malloc(
        bridge->n_buffers * sizeof(uint32_t));
    if (!bridge->buffer_map) {
        NIMCP_LOGGING_ERROR("Failed to allocate buffer map");
        snn_buffer_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_buffer_bridge_create: bridge->buffer_map is NULL");
        return NULL;
    }
    for (uint32_t i = 0; i < bridge->n_buffers; i++) {
        bridge->buffer_map[i] = i;
    }

    /* Delay line storage */
    if (bridge->config.enable_delay_lines) {
        uint32_t max_delayed = bridge->config.buffer_capacity;
        bridge->delayed_spike_times = (uint64_t*)nimcp_malloc(
            max_delayed * sizeof(uint64_t));
        bridge->delayed_spike_neurons = (uint32_t*)nimcp_malloc(
            max_delayed * sizeof(uint32_t));

        if (!bridge->delayed_spike_times || !bridge->delayed_spike_neurons) {
            NIMCP_LOGGING_ERROR("Failed to allocate delay line storage");
            snn_buffer_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_buffer_bridge_create: required parameter is NULL (bridge->delayed_spike_times, bridge->delayed_spike_neurons)");
            return NULL;
        }
        memset(bridge->delayed_spike_times, 0, max_delayed * sizeof(uint64_t));
        memset(bridge->delayed_spike_neurons, 0, max_delayed * sizeof(uint32_t));
    }

    bridge->connected = true;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("SNN buffer bridge created with %u buffers", bridge->n_buffers);
    return bridge;
}

void snn_buffer_bridge_destroy(snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_buffer_bridge_disconnect_bio_async(bridge);
    }

    /* Free buffers */
    if (bridge->buffers) {
        for (uint32_t i = 0; i < bridge->n_buffers; i++) {
            if (bridge->buffers[i]) {
                circular_buffer_destroy(bridge->buffers[i]);
            }
        }
        nimcp_free(bridge->buffers);
    }

    /* Free other allocations */
    if (bridge->buffer_map) nimcp_free(bridge->buffer_map);
    if (bridge->delayed_spike_times) nimcp_free(bridge->delayed_spike_times);
    if (bridge->delayed_spike_neurons) nimcp_free(bridge->delayed_spike_neurons);
    if (bridge->pattern_hashes) nimcp_free(bridge->pattern_hashes);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SNN buffer bridge destroyed");
}

int snn_buffer_bridge_connect_bio_async(snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("SNN buffer bridge connected to bio-async");

    return 0;
}

int snn_buffer_bridge_disconnect_bio_async(snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("SNN buffer bridge disconnected from bio-async");

    return 0;
}

bool snn_buffer_bridge_is_bio_async_connected(const snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Buffering Functions
//=============================================================================

int snn_buffer_bridge_process(
    snn_buffer_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_process: null bridge pointer");
        return -1;
    }
    if (!spikes_in || n_spikes == 0) {
        if (n_out_actual) *n_out_actual = 0;
        return 0;
    }
    if (!spikes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_process: null spikes_out pointer");
        return -1;
    }
    if (!n_out_actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_process: null n_out_actual pointer");
        return -1;
    }

    uint32_t n_output = 0;

    /* WHAT: Buffer each spike */
    for (uint32_t i = 0; i < n_spikes; i++) {
        const snn_spike_t* spike = &spikes_in[i];

        /* WHY: Determine target buffer */
        uint32_t buffer_id = 0;
        if (bridge->config.enable_population_buffers) {
            buffer_id = spike->population_id;
            if (buffer_id >= bridge->n_buffers) continue;
        }

        /* HOW: Push to buffer */
        bool pushed = circular_buffer_push(bridge->buffers[buffer_id], spike);
        if (pushed) {
            bridge->stats.spikes_buffered++;
        } else {
            bridge->stats.buffer_overflows++;
        }

        /* Pass through spike (optionally delayed) */
        if (n_output < n_out_capacity) {
            spikes_out[n_output] = *spike;
            n_output++;
        }
    }

    *n_out_actual = n_output;

    return 0;
}

int snn_buffer_bridge_update(snn_buffer_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_update: null bridge pointer");
        return -1;
    }

    bridge->last_update_time += dt;

    /* Update buffer utilization statistics */
    float total_util = 0.0f;
    float peak_util = 0.0f;

    for (uint32_t i = 0; i < bridge->n_buffers; i++) {
        float util = circular_buffer_utilization(bridge->buffers[i]);
        total_util += util;
        if (util > peak_util) peak_util = util;
    }

    bridge->stats.avg_buffer_utilization = total_util / bridge->n_buffers;
    if (peak_util > bridge->stats.peak_buffer_utilization) {
        bridge->stats.peak_buffer_utilization = peak_util;
    }

    return 0;
}

int snn_buffer_bridge_get_window(
    const snn_buffer_bridge_t* bridge,
    uint32_t buffer_id,
    float window_ms,
    snn_spike_t* spikes_out,
    uint32_t capacity,
    uint32_t* n_spikes_actual
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_window: null bridge pointer");
        return -1;
    }
    if (!spikes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_window: null spikes_out pointer");
        return -1;
    }
    if (!n_spikes_actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_window: null n_spikes_actual pointer");
        return -1;
    }
    if (buffer_id >= bridge->n_buffers) return -1;

    /* For now, return all buffered spikes */
    /* Full temporal windowing would require timestamp filtering */
    size_t n_buffered = circular_buffer_size(bridge->buffers[buffer_id]);
    uint32_t n_to_copy = (n_buffered < capacity) ? (uint32_t)n_buffered : capacity;

    for (uint32_t i = 0; i < n_to_copy; i++) {
        circular_buffer_peek(bridge->buffers[buffer_id], i, &spikes_out[i]);
    }

    *n_spikes_actual = n_to_copy;
    return 0;
}

int snn_buffer_bridge_clear_buffer(
    snn_buffer_bridge_t* bridge,
    uint32_t buffer_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_clear_buffer: null bridge pointer");
        return -1;
    }
    if (buffer_id >= bridge->n_buffers) return -1;

    circular_buffer_clear(bridge->buffers[buffer_id]);
    return 0;
}

void snn_buffer_bridge_clear_all(snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_clear_all: null bridge pointer");
        return;
    }

    for (uint32_t i = 0; i < bridge->n_buffers; i++) {
        circular_buffer_clear(bridge->buffers[i]);
    }
}

//=============================================================================
// Delay Lines
//=============================================================================

int snn_buffer_bridge_add_delayed_spike(
    snn_buffer_bridge_t* bridge,
    const snn_spike_t* spike,
    float delay_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_add_delayed_spike: null bridge pointer");
        return -1;
    }
    if (!spike) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_add_delayed_spike: null spike pointer");
        return -1;
    }
    if (!bridge->config.enable_delay_lines) return -1;

    /* Store delayed spike */
    uint64_t delivery_time = spike->timestamp_us + (uint64_t)(delay_ms * 1000.0f);

    uint32_t idx = bridge->n_delayed_spikes;
    if (idx < bridge->config.buffer_capacity) {
        bridge->delayed_spike_times[idx] = delivery_time;
        bridge->delayed_spike_neurons[idx] = spike->neuron_id;
        bridge->n_delayed_spikes++;
    }

    return 0;
}

int snn_buffer_bridge_deliver_delayed_spikes(
    snn_buffer_bridge_t* bridge,
    uint64_t current_time_us,
    snn_spike_t* spikes_out,
    uint32_t capacity,
    uint32_t* n_delivered
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_deliver_delayed_spikes: null bridge pointer");
        return -1;
    }
    if (!spikes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_deliver_delayed_spikes: null spikes_out pointer");
        return -1;
    }
    if (!n_delivered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_deliver_delayed_spikes: null n_delivered pointer");
        return -1;
    }

    uint32_t delivered = 0;

    /* Check each delayed spike */
    for (uint32_t i = 0; i < bridge->n_delayed_spikes && delivered < capacity; i++) {
        if (bridge->delayed_spike_times[i] <= current_time_us) {
            /* Deliver this spike */
            spikes_out[delivered].timestamp_us = bridge->delayed_spike_times[i];
            spikes_out[delivered].neuron_id = bridge->delayed_spike_neurons[i];
            spikes_out[delivered].population_id = 0;
            delivered++;

            /* Remove from delayed list */
            bridge->delayed_spike_times[i] = bridge->delayed_spike_times[bridge->n_delayed_spikes - 1];
            bridge->delayed_spike_neurons[i] = bridge->delayed_spike_neurons[bridge->n_delayed_spikes - 1];
            bridge->n_delayed_spikes--;
            i--;
        }
    }

    *n_delivered = delivered;
    return 0;
}

//=============================================================================
// Pattern Detection
//=============================================================================

int snn_buffer_bridge_detect_pattern(
    snn_buffer_bridge_t* bridge,
    uint32_t buffer_id,
    uint32_t min_pattern_length,
    bool* pattern_detected
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_detect_pattern: null bridge pointer");
        return -1;
    }
    if (!pattern_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_detect_pattern: null pattern_detected pointer");
        return -1;
    }
    if (buffer_id >= bridge->n_buffers) return -1;

    /* Simplified pattern detection: check if buffer has repeating ISIs */
    *pattern_detected = false;

    /* Would need full pattern matching algorithm */
    /* For now, just return false */

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_buffer_bridge_get_stats(
    const snn_buffer_bridge_t* bridge,
    snn_buffer_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_stats: null stats pointer");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void snn_buffer_bridge_reset_stats(snn_buffer_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->stats, 0, sizeof(snn_buffer_stats_t));
    bridge->last_update_time = 0.0f;
}

float snn_buffer_bridge_get_utilization(
    const snn_buffer_bridge_t* bridge,
    uint32_t buffer_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_buffer_bridge_get_utilization: null bridge pointer");
        return 0.0f;
    }
    if (buffer_id >= bridge->n_buffers) return 0.0f;

    return circular_buffer_utilization(bridge->buffers[buffer_id]);
}
