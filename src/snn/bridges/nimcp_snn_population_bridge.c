/**
 * @file nimcp_snn_population_bridge.c
 * @brief Implementation of SNN-Population Coding bridge
 */

#include "snn/bridges/nimcp_snn_population_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_population_bridge)


//=============================================================================
// Default Configuration
//=============================================================================

void snn_population_config_default(snn_population_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_config_default: null config pointer");
        return;
    }

    memset(config, 0, sizeof(snn_population_config_t));

    config->tuning_width_rad = M_PI / 4.0f; /* 45 degrees */
    config->max_firing_rate_hz = 100.0f;
    config->n_pca_components = 3;
    config->synchrony_threshold = 0.5f;

    config->enable_vector_sum = true;
    config->enable_center_of_mass = false;
    config->enable_pca = false;
    config->enable_synchrony_analysis = false;

    config->auto_generate_tuning = true;
    config->tuning_diversity = 1.0f;

    config->temporal_window_ms = 50.0f;
    config->rate_decay_tau_ms = 20.0f;

    config->enable_bio_async = false;
    config->update_interval_ms = 10.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_population_bridge_t* snn_population_bridge_create(
    const snn_population_config_t* config,
    snn_network_t* network
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_LOGGING_ERROR("SNN network is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_create: network is NULL");
        return NULL;
    }

    /* WHAT: Allocate bridge structure */
    snn_population_bridge_t* bridge = (snn_population_bridge_t*)nimcp_malloc(
        sizeof(snn_population_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN population bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* WHY: Initialize all fields to zero */
    memset(bridge, 0, sizeof(snn_population_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_population") != 0) { nimcp_free(bridge); return NULL; }

    /* HOW: Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        snn_population_config_default(&bridge->config);
    }

    bridge->network = network;

    /* WHAT: Create population coding encoder */
    population_coding_config_t pop_config = population_coding_default_config();
    pop_config.n_pca_components = bridge->config.n_pca_components;
    pop_config.synchrony_threshold = bridge->config.synchrony_threshold;

    bridge->encoder = population_coding_create(&pop_config);
    if (!bridge->encoder) {
        NIMCP_LOGGING_ERROR("Failed to create population coding encoder");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to create encoder");
        snn_population_bridge_destroy(bridge);
        return NULL;
    }

    /* WHY: Calculate total neurons and max neurons */
    uint32_t total_neurons = 0;
    uint32_t max_neurons = 0;

    for (uint32_t i = 0; i < network->n_populations; i++) {
        snn_population_t* pop = network->populations[i];
        if (pop) {
            total_neurons += pop->n_neurons;
            if (pop->n_neurons > max_neurons) {
                max_neurons = pop->n_neurons;
            }
        }
    }

    bridge->max_neurons = max_neurons;
    bridge->n_tuning_curves = total_neurons;

    /* HOW: Allocate tuning curves */
    bridge->tuning_curves = (tuning_curve_t*)nimcp_malloc(
        total_neurons * sizeof(tuning_curve_t));
    if (!bridge->tuning_curves) {
        NIMCP_LOGGING_ERROR("Failed to allocate tuning curves");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to allocate tuning_curves");
        snn_population_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->tuning_curves, 0, total_neurons * sizeof(tuning_curve_t));

    /* Population vectors */
    bridge->current_vectors = (vector3d_t*)nimcp_malloc(
        network->n_populations * sizeof(vector3d_t));
    if (!bridge->current_vectors) {
        NIMCP_LOGGING_ERROR("Failed to allocate current vectors");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to allocate current_vectors");
        snn_population_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->current_vectors, 0, network->n_populations * sizeof(vector3d_t));

    /* Firing rates */
    bridge->firing_rates = (float*)nimcp_malloc(total_neurons * sizeof(float));
    if (!bridge->firing_rates) {
        NIMCP_LOGGING_ERROR("Failed to allocate firing rates");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to allocate firing_rates");
        snn_population_bridge_destroy(bridge);
        return NULL;
    }
    memset(bridge->firing_rates, 0, total_neurons * sizeof(float));

    /* Synchrony results */
    if (bridge->config.enable_synchrony_analysis) {
        bridge->synchrony_results = (synchrony_result_t*)nimcp_malloc(
            network->n_populations * sizeof(synchrony_result_t));
        if (!bridge->synchrony_results) {
            NIMCP_LOGGING_ERROR("Failed to allocate synchrony results");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_population_bridge_create: failed to allocate synchrony_results");
            snn_population_bridge_destroy(bridge);
            return NULL;
        }
        memset(bridge->synchrony_results, 0,
               network->n_populations * sizeof(synchrony_result_t));
    }

    /* Auto-generate tuning curves if enabled */
    if (bridge->config.auto_generate_tuning) {
        for (uint32_t i = 0; i < network->n_populations; i++) {
            snn_population_bridge_generate_tuning(bridge, i);
        }
    }

    bridge->connected = true;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("SNN population bridge created successfully");
    return bridge;
}

void snn_population_bridge_destroy(snn_population_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_population_bridge_disconnect_bio_async(bridge);
    }

    /* Free encoder */
    if (bridge->encoder) {
        population_coding_destroy(bridge->encoder);
    }

    /* Free allocations */
    if (bridge->tuning_curves) nimcp_free(bridge->tuning_curves);
    if (bridge->current_vectors) nimcp_free(bridge->current_vectors);
    if (bridge->firing_rates) nimcp_free(bridge->firing_rates);
    if (bridge->synchrony_results) nimcp_free(bridge->synchrony_results);
    if (bridge->pca_result) {
        population_coding_pca_result_destroy(bridge->pca_result);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SNN population bridge destroyed");
}

int snn_population_bridge_connect_bio_async(snn_population_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("SNN population bridge connected to bio-async");

    return 0;
}

int snn_population_bridge_disconnect_bio_async(snn_population_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("SNN population bridge disconnected from bio-async");

    return 0;
}

bool snn_population_bridge_is_bio_async_connected(const snn_population_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int snn_population_bridge_process(
    snn_population_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_process: null bridge pointer");
        return -1;
    }
    if (!spikes_in || n_spikes == 0) {
        if (n_out_actual) *n_out_actual = 0;
        return 0;
    }

    /* WHAT: Update firing rates from spikes */
    for (uint32_t i = 0; i < n_spikes; i++) {
        uint32_t neuron_id = spikes_in[i].neuron_id;
        if (neuron_id < bridge->n_tuning_curves) {
            /* Increment rate (simplified) */
            bridge->firing_rates[neuron_id] += 1.0f;
        }
    }

    /* WHY: Encode population vectors */
    for (uint32_t pop_id = 0; pop_id < bridge->network->n_populations; pop_id++) {
        if (bridge->config.enable_vector_sum) {
            snn_population_bridge_encode_vector(bridge, pop_id,
                                                &bridge->current_vectors[pop_id]);
        }
    }

    /* HOW: Pass through spikes */
    if (spikes_out && n_out_actual) {
        uint32_t n_copy = (n_spikes < n_out_capacity) ? n_spikes : n_out_capacity;
        memcpy(spikes_out, spikes_in, n_copy * sizeof(snn_spike_t));
        *n_out_actual = n_copy;
    }

    bridge->stats.vectors_encoded += bridge->network->n_populations;

    return 0;
}

int snn_population_bridge_update(snn_population_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_update: null bridge pointer");
        return -1;
    }

    bridge->last_update_time += dt;

    /* Decay firing rates */
    float decay_factor = expf(-dt / bridge->config.rate_decay_tau_ms);
    for (uint32_t i = 0; i < bridge->n_tuning_curves; i++) {
        bridge->firing_rates[i] *= decay_factor;
    }

    return 0;
}

int snn_population_bridge_encode_vector(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    vector3d_t* vector_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_encode_vector: null bridge pointer");
        return -1;
    }
    if (!vector_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_encode_vector: null vector_out pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_encode_vector: invalid pop_id");
        return -1;
    }

    snn_population_t* pop = bridge->network->populations[pop_id];
    if (!pop) return -1;

    /* Use population coding encoder */
    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    bool success = population_coding_encode_vector_sum(
        bridge->encoder,
        &bridge->firing_rates[neuron_offset],
        &bridge->tuning_curves[neuron_offset],
        pop->n_neurons,
        vector_out
    );

    return success ? 0 : -1;
}

int snn_population_bridge_decode_vector(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    const vector3d_t* vector
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_decode_vector: null bridge pointer");
        return -1;
    }
    if (!vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_decode_vector: null vector pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_decode_vector: invalid pop_id");
        return -1;
    }

    snn_population_t* pop = bridge->network->populations[pop_id];
    if (!pop) return -1;

    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    bool success = population_coding_decode_vector_sum(
        bridge->encoder,
        vector,
        &bridge->tuning_curves[neuron_offset],
        pop->n_neurons,
        &bridge->firing_rates[neuron_offset]
    );

    bridge->stats.vectors_decoded++;

    return success ? 0 : -1;
}

//=============================================================================
// Tuning Curve Management
//=============================================================================

int snn_population_bridge_set_tuning(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    const tuning_curve_t* tuning
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_set_tuning: null bridge pointer");
        return -1;
    }
    if (!tuning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_set_tuning: null tuning pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_set_tuning: invalid pop_id");
        return -1;
    }

    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    uint32_t global_idx = neuron_offset + neuron_idx;
    if (global_idx >= bridge->n_tuning_curves) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_set_tuning: capacity exceeded");
        return -1;
    }

    bridge->tuning_curves[global_idx] = *tuning;
    return 0;
}

int snn_population_bridge_get_tuning(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    uint32_t neuron_idx,
    tuning_curve_t* tuning
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_tuning: null bridge pointer");
        return -1;
    }
    if (!tuning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_tuning: null tuning pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_get_tuning: invalid pop_id");
        return -1;
    }

    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    uint32_t global_idx = neuron_offset + neuron_idx;
    if (global_idx >= bridge->n_tuning_curves) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_get_tuning: capacity exceeded");
        return -1;
    }

    *tuning = bridge->tuning_curves[global_idx];
    return 0;
}

int snn_population_bridge_generate_tuning(
    snn_population_bridge_t* bridge,
    uint32_t pop_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_generate_tuning: null bridge pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_generate_tuning: invalid pop_id");
        return -1;
    }

    snn_population_t* pop = bridge->network->populations[pop_id];
    if (!pop) return -1;

    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    /* Distribute preferred directions uniformly */
    for (uint32_t i = 0; i < pop->n_neurons; i++) {
        float theta = 2.0f * M_PI * i / (float)pop->n_neurons;
        float phi = M_PI * (i % 10) / 10.0f;

        tuning_curve_t* tuning = &bridge->tuning_curves[neuron_offset + i];
        tuning->preferred_direction = population_coding_vector3d_make(
            cosf(theta) * sinf(phi),
            sinf(theta) * sinf(phi),
            cosf(phi)
        );
        tuning->tuning_width = bridge->config.tuning_width_rad;
        tuning->max_rate = bridge->config.max_firing_rate_hz;
    }

    return 0;
}

//=============================================================================
// Synchrony Analysis
//=============================================================================

int snn_population_bridge_compute_synchrony(
    snn_population_bridge_t* bridge,
    uint32_t pop_id,
    synchrony_result_t* synchrony_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_compute_synchrony: null bridge pointer");
        return -1;
    }
    if (!synchrony_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_compute_synchrony: null synchrony_out pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_compute_synchrony: invalid pop_id");
        return -1;
    }

    /* Simplified: return cached result if available */
    if (bridge->synchrony_results) {
        *synchrony_out = bridge->synchrony_results[pop_id];
    } else {
        memset(synchrony_out, 0, sizeof(synchrony_result_t));
    }

    return 0;
}

int snn_population_bridge_get_current_vector(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    vector3d_t* vector
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_current_vector: null bridge pointer");
        return -1;
    }
    if (!vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_current_vector: null vector pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_get_current_vector: invalid pop_id");
        return -1;
    }

    *vector = bridge->current_vectors[pop_id];
    return 0;
}

int snn_population_bridge_get_rates(
    const snn_population_bridge_t* bridge,
    uint32_t pop_id,
    float* rates,
    uint32_t n_neurons
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_rates: null bridge pointer");
        return -1;
    }
    if (!rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_rates: null rates pointer");
        return -1;
    }
    if (pop_id >= bridge->network->n_populations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_population_bridge_get_rates: invalid pop_id");
        return -1;
    }

    uint32_t neuron_offset = 0;
    for (uint32_t i = 0; i < pop_id; i++) {
        neuron_offset += bridge->network->populations[i]->n_neurons;
    }

    snn_population_t* pop = bridge->network->populations[pop_id];
    uint32_t n_copy = (pop->n_neurons < n_neurons) ? pop->n_neurons : n_neurons;

    memcpy(rates, &bridge->firing_rates[neuron_offset], n_copy * sizeof(float));
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_population_bridge_get_stats(
    const snn_population_bridge_t* bridge,
    snn_population_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_get_stats: null stats pointer");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void snn_population_bridge_reset_stats(snn_population_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_population_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->stats, 0, sizeof(snn_population_stats_t));
    bridge->last_update_time = 0.0f;
}
