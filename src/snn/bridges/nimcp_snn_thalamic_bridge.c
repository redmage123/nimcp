/**
 * @file nimcp_snn_thalamic_bridge.c
 * @brief Implementation of SNN-Thalamic bridge
 */

#include "snn/bridges/nimcp_snn_thalamic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_thalamic_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

void snn_thalamic_config_default(snn_thalamic_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(snn_thalamic_config_t));

    config->default_mode = THALAMIC_MODE_ADAPTIVE;
    config->enable_mode_switching = true;
    config->burst_threshold_ms = 4.0f;    /* <4ms ISI is burst */
    config->tonic_min_isi_ms = 10.0f;     /* >10ms ISI is tonic */

    config->enable_attention_gating = true;
    config->attention_threshold = 0.1f;
    config->attention_boost_burst = 1.5f; /* Burst gets 50% boost */

    config->enable_ct_loop = true;
    config->ct_delay_ms = 5.0f;           /* 5ms cortical feedback delay */
    config->ct_gain = 0.3f;

    config->enable_trn_inhibition = false;
    config->trn_inhibition_radius = 100.0f; /* µm */
    config->trn_inhibition_strength = 0.5f;

    config->enable_first_order = true;
    config->enable_higher_order = true;

    config->enable_bio_async = false;
    config->update_interval_ms = 1.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_thalamic_bridge_t* snn_thalamic_bridge_create(
    const snn_thalamic_config_t* config,
    snn_network_t* network,
    thalamic_router_t* router
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_create: network is NULL");
        return NULL;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_create: router is NULL");
        return NULL;
    }

    /* WHAT: Allocate bridge structure */
    snn_thalamic_bridge_t* bridge = (snn_thalamic_bridge_t*)nimcp_malloc(
        sizeof(snn_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* WHY: Initialize all fields to zero */
    memset(bridge, 0, sizeof(snn_thalamic_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    /* HOW: Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        snn_thalamic_config_default(&bridge->config);
    }

    bridge->network = network;
    bridge->router = router;

    /* WHAT: Calculate total neurons */
    uint32_t total_neurons = 0;
    for (uint32_t i = 0; i < network->n_populations; i++) {
        if (network->populations[i]) {
            total_neurons += network->populations[i]->n_neurons;
        }
    }

    /* WHY: Allocate relay mode array */
    bridge->neuron_modes = (thalamic_relay_mode_t*)nimcp_malloc(
        total_neurons * sizeof(thalamic_relay_mode_t));
    if (!bridge->neuron_modes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate neuron_modes");
        snn_thalamic_bridge_destroy(bridge);
        return NULL;
    }
    for (uint32_t i = 0; i < total_neurons; i++) {
        bridge->neuron_modes[i] = bridge->config.default_mode;
    }

    /* Spike timing for mode detection */
    bridge->last_spike_time_us = (uint64_t*)nimcp_malloc(
        total_neurons * sizeof(uint64_t));
    if (!bridge->last_spike_time_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate last_spike_time_us");
        snn_thalamic_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->last_spike_time_us, 0, total_neurons * sizeof(uint64_t));

    /* Attention weights per population */
    bridge->attention_weights = (float*)nimcp_malloc(
        network->n_populations * sizeof(float));
    if (!bridge->attention_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate attention_weights");
        snn_thalamic_bridge_destroy(bridge);
        return NULL;
    }
    for (uint32_t i = 0; i < network->n_populations; i++) {
        bridge->attention_weights[i] = 1.0f; /* Default: full attention */
    }

    /* TRN inhibition if enabled */
    if (bridge->config.enable_trn_inhibition) {
        bridge->trn_inhibition = (float*)nimcp_malloc(total_neurons * sizeof(float));
        if (!bridge->trn_inhibition) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate trn_inhibition");
            snn_thalamic_bridge_destroy(bridge);
            return NULL;
        }
        memset(bridge->trn_inhibition, 0, total_neurons * sizeof(float));
    }

    /* CT feedback buffer if enabled */
    if (bridge->config.enable_ct_loop) {
        bridge->ct_buffer_size = 1000;
        bridge->ct_feedback_buffer = (snn_spike_t*)nimcp_malloc(
            bridge->ct_buffer_size * sizeof(snn_spike_t));
        if (!bridge->ct_feedback_buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_thalamic_bridge_create: failed to allocate ct_feedback_buffer");
            snn_thalamic_bridge_destroy(bridge);
            return NULL;
        }
        bridge->ct_buffer_count = 0;
    }

    bridge->connected = true;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("SNN thalamic bridge created successfully");
    return bridge;
}

void snn_thalamic_bridge_destroy(snn_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_thalamic_bridge_disconnect_bio_async(bridge);
    }

    /* Free allocated arrays */
    if (bridge->neuron_modes) nimcp_free(bridge->neuron_modes);
    if (bridge->last_spike_time_us) nimcp_free(bridge->last_spike_time_us);
    if (bridge->attention_weights) nimcp_free(bridge->attention_weights);
    if (bridge->trn_inhibition) nimcp_free(bridge->trn_inhibition);
    if (bridge->ct_feedback_buffer) nimcp_free(bridge->ct_feedback_buffer);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SNN thalamic bridge destroyed");
}

int snn_thalamic_bridge_connect_bio_async(snn_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("SNN thalamic bridge connected to bio-async");

    return 0;
}

int snn_thalamic_bridge_disconnect_bio_async(snn_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("SNN thalamic bridge disconnected from bio-async");

    return 0;
}

bool snn_thalamic_bridge_is_bio_async_connected(const snn_thalamic_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}

//=============================================================================
// Relay Functions
//=============================================================================

int snn_thalamic_bridge_process(
    snn_thalamic_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_process: bridge is NULL");
        return -1;
    }
    if (!spikes_in || n_spikes == 0) {
        if (n_out_actual) *n_out_actual = 0;
        return 0;
    }
    if (!spikes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_process: spikes_out is NULL");
        return -1;
    }
    if (!n_out_actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_process: n_out_actual is NULL");
        return -1;
    }

    uint32_t n_relayed = 0;

    /* WHAT: Process each spike */
    for (uint32_t i = 0; i < n_spikes && n_relayed < n_out_capacity; i++) {
        const snn_spike_t* spike = &spikes_in[i];
        uint32_t neuron_id = spike->neuron_id;
        uint32_t pop_id = spike->population_id;

        /* WHY: Detect relay mode if adaptive */
        thalamic_relay_mode_t mode = bridge->neuron_modes[neuron_id];
        if (mode == THALAMIC_MODE_ADAPTIVE && bridge->config.enable_mode_switching) {
            mode = snn_thalamic_bridge_detect_mode(bridge, neuron_id,
                                                   spike->timestamp_us);
            bridge->neuron_modes[neuron_id] = mode;
        }

        /* Track burst/tonic */
        if (mode == THALAMIC_MODE_BURST) {
            bridge->stats.bursts_detected++;
        } else if (mode == THALAMIC_MODE_TONIC) {
            bridge->stats.tonic_spikes++;
        }

        /* HOW: Apply attention gating */
        float attention = bridge->attention_weights[pop_id];
        if (mode == THALAMIC_MODE_BURST) {
            attention *= bridge->config.attention_boost_burst;
        }

        if (bridge->config.enable_attention_gating &&
            attention < bridge->config.attention_threshold) {
            bridge->stats.spikes_blocked++;
            continue;
        }

        /* Apply TRN inhibition if enabled */
        if (bridge->config.enable_trn_inhibition && bridge->trn_inhibition) {
            if (bridge->trn_inhibition[neuron_id] > 0.5f) {
                bridge->stats.spikes_blocked++;
                continue;
            }
        }

        /* Relay the spike */
        spikes_out[n_relayed] = *spike;
        n_relayed++;
        bridge->stats.spikes_relayed++;

        /* Update average attention */
        bridge->stats.avg_attention =
            (bridge->stats.avg_attention * 0.99f) + (attention * 0.01f);
    }

    *n_out_actual = n_relayed;

    /* Update burst ratio */
    if (bridge->stats.spikes_relayed > 0) {
        bridge->stats.burst_ratio =
            (float)bridge->stats.bursts_detected / bridge->stats.spikes_relayed;
    }

    return 0;
}

int snn_thalamic_bridge_update(snn_thalamic_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_update: bridge is NULL");
        return -1;
    }

    bridge->last_update_time += dt;

    /* Process CT feedback loop if enabled */
    if (bridge->config.enable_ct_loop) {
        uint64_t current_time_us = (uint64_t)(bridge->last_update_time * 1000.0f);
        snn_thalamic_bridge_process_ct_loop(bridge, current_time_us);
    }

    return 0;
}

//=============================================================================
// Mode Control
//=============================================================================

int snn_thalamic_bridge_set_mode(
    snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    thalamic_relay_mode_t mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_set_mode: bridge is NULL");
        return -1;
    }

    uint32_t total_neurons = 0;
    for (uint32_t i = 0; i < bridge->network->n_populations; i++) {
        total_neurons += bridge->network->populations[i]->n_neurons;
    }

    if (neuron_id >= total_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_thalamic_bridge_set_mode: capacity exceeded");
        return -1;
    }

    bridge->neuron_modes[neuron_id] = mode;
    return 0;
}

int snn_thalamic_bridge_get_mode(
    const snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    thalamic_relay_mode_t* mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_mode: bridge is NULL");
        return -1;
    }
    if (!mode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_mode: mode is NULL");
        return -1;
    }

    uint32_t total_neurons = 0;
    for (uint32_t i = 0; i < bridge->network->n_populations; i++) {
        total_neurons += bridge->network->populations[i]->n_neurons;
    }

    if (neuron_id >= total_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_thalamic_bridge_get_mode: capacity exceeded");
        return -1;
    }

    *mode = bridge->neuron_modes[neuron_id];
    return 0;
}

thalamic_relay_mode_t snn_thalamic_bridge_detect_mode(
    snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    uint64_t spike_time_us
) {
    if (!bridge) return THALAMIC_MODE_TONIC;

    /* WHAT: Calculate ISI */
    uint64_t last_time = bridge->last_spike_time_us[neuron_id];
    bridge->last_spike_time_us[neuron_id] = spike_time_us;

    if (last_time == 0) {
        /* First spike, assume tonic */
        return THALAMIC_MODE_TONIC;
    }

    /* WHY: Determine mode from ISI */
    uint64_t isi_us = spike_time_us - last_time;
    float isi_ms = isi_us / 1000.0f;

    /* HOW: Compare to thresholds */
    if (isi_ms < bridge->config.burst_threshold_ms) {
        return THALAMIC_MODE_BURST;
    } else if (isi_ms > bridge->config.tonic_min_isi_ms) {
        return THALAMIC_MODE_TONIC;
    } else {
        /* In between: maintain current mode */
        return bridge->neuron_modes[neuron_id];
    }
}

//=============================================================================
// Attention Control
//=============================================================================

int snn_thalamic_bridge_set_attention(
    snn_thalamic_bridge_t* bridge,
    uint32_t pop_id,
    float attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_set_attention: bridge is NULL");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_thalamic_bridge_set_attention: pop_id out of range");
        return -1;
    }
    if (attention < 0.0f || attention > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_thalamic_bridge_set_attention: attention out of range [0,1]");
        return -1;
    }

    bridge->attention_weights[pop_id] = attention;
    return 0;
}

int snn_thalamic_bridge_get_attention(
    const snn_thalamic_bridge_t* bridge,
    uint32_t pop_id,
    float* attention
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_attention: bridge is NULL");
        return -1;
    }
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_attention: attention is NULL");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_thalamic_bridge_get_attention: pop_id out of range");
        return -1;
    }

    *attention = bridge->attention_weights[pop_id];
    return 0;
}

//=============================================================================
// Cortical-Thalamic Loop
//=============================================================================

int snn_thalamic_bridge_ct_feedback(
    snn_thalamic_bridge_t* bridge,
    const snn_spike_t* spike
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_ct_feedback: bridge is NULL");
        return -1;
    }
    if (!spike) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_ct_feedback: spike is NULL");
        return -1;
    }
    if (!bridge->config.enable_ct_loop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_thalamic_bridge_ct_feedback: CT loop not enabled");
        return -1;
    }

    /* Buffer feedback spike */
    if (bridge->ct_buffer_count < bridge->ct_buffer_size) {
        bridge->ct_feedback_buffer[bridge->ct_buffer_count] = *spike;
        bridge->ct_buffer_count++;
        bridge->stats.ct_loop_activations++;
    }

    return 0;
}

int snn_thalamic_bridge_process_ct_loop(
    snn_thalamic_bridge_t* bridge,
    uint64_t current_time_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_process_ct_loop: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_ct_loop) return 0;

    /* Process buffered feedback spikes with delay */
    uint64_t delay_us = (uint64_t)(bridge->config.ct_delay_ms * 1000.0f);

    for (uint32_t i = 0; i < bridge->ct_buffer_count; i++) {
        snn_spike_t* fb_spike = &bridge->ct_feedback_buffer[i];
        uint64_t delivery_time = fb_spike->timestamp_us + delay_us;

        if (delivery_time <= current_time_us) {
            /* Apply feedback (modulate attention) */
            uint32_t pop_id = fb_spike->population_id;
            if (pop_id < bridge->network->n_populations) {
                bridge->attention_weights[pop_id] += bridge->config.ct_gain;
                if (bridge->attention_weights[pop_id] > 1.0f) {
                    bridge->attention_weights[pop_id] = 1.0f;
                }
            }

            /* Remove from buffer */
            bridge->ct_feedback_buffer[i] =
                bridge->ct_feedback_buffer[bridge->ct_buffer_count - 1];
            bridge->ct_buffer_count--;
            i--;
        }
    }

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_thalamic_bridge_get_stats(
    const snn_thalamic_bridge_t* bridge,
    snn_thalamic_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_thalamic_bridge_get_stats: stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void snn_thalamic_bridge_reset_stats(snn_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    memset(&bridge->stats, 0, sizeof(snn_thalamic_stats_t));
    bridge->last_update_time = 0.0f;
}
