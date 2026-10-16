/**
 * @file nimcp_vae_snn_bridge.c
 * @brief VAE-SNN Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements bidirectional conversion between VAE latent space and spike trains.
 *
 * BIO_MODULE: 0x1F1B
 */

#include "cognitive/vae/bridges/nimcp_vae_snn_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_SNN_MODULE_ID           BIO_MODULE_VAE_SNN_BRIDGE
#define VAE_SNN_HISTORY_SIZE        100
#define VAE_SNN_DEFAULT_NEURONS     64
#define VAE_SNN_DEFAULT_RATE_HZ     100.0f

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Generate random number [0,1]
 */
static inline float randf(void)
{
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/**
 * @brief Map latent value to firing rate
 */
static float latent_to_rate(const vae_snn_bridge_t* bridge, float latent_value)
{
    if (!bridge) return 0.0f;

    const vae_snn_rate_config_t* rc = &bridge->config.rate_config;

    /* Normalize latent to [0,1] range */
    float normalized = (latent_value - rc->latent_min) /
                       (rc->latent_max - rc->latent_min);
    normalized = nimcp_clampf(normalized, 0.0f, 1.0f);

    /* Apply softplus if enabled */
    if (rc->use_softplus) {
        normalized = logf(1.0f + expf(normalized * 5.0f - 2.5f)) / 5.0f;
    }

    /* Map to rate range */
    return rc->min_rate_hz + normalized * (rc->max_rate_hz - rc->min_rate_hz);
}

/**
 * @brief Map firing rate to latent value
 */
static float rate_to_latent(const vae_snn_bridge_t* bridge, float rate_hz)
{
    if (!bridge) return 0.0f;

    const vae_snn_rate_config_t* rc = &bridge->config.rate_config;

    /* Normalize rate to [0,1] */
    float normalized = (rate_hz - rc->min_rate_hz) /
                       (rc->max_rate_hz - rc->min_rate_hz);
    normalized = nimcp_clampf(normalized, 0.0f, 1.0f);

    /* Map to latent range */
    return rc->latent_min + normalized * (rc->latent_max - rc->latent_min);
}

/**
 * @brief Map latent value to first spike time (temporal coding)
 */
static float latent_to_spike_time(const vae_snn_bridge_t* bridge, float latent_value)
{
    if (!bridge) return 0.0f;

    const vae_snn_temporal_config_t* tc = &bridge->config.temporal_config;
    const vae_snn_rate_config_t* rc = &bridge->config.rate_config;

    /* Normalize latent */
    float normalized = (latent_value - rc->latent_min) /
                       (rc->latent_max - rc->latent_min);
    normalized = nimcp_clampf(normalized, 0.0f, 1.0f);

    /* Invert if higher value means shorter latency */
    if (tc->inverse_latency) {
        normalized = 1.0f - normalized;
    }

    return tc->min_latency_ms + normalized * (tc->max_latency_ms - tc->min_latency_ms);
}

/**
 * @brief Generate Poisson spike times
 */
static int generate_poisson_spikes(float rate_hz, float window_ms,
                                    uint32_t** spike_times, uint32_t* num_spikes)
{
    if (!spike_times || !num_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_poisson_spikes: required parameter is NULL (spike_times, num_spikes)");
        return -1;
    }

    float dt_ms = 1.0f; /* 1 ms resolution */
    uint32_t num_bins = (uint32_t)(window_ms / dt_ms);
    float p_spike = rate_hz * dt_ms / 1000.0f;

    /* Count spikes first */
    uint32_t count = 0;
    for (uint32_t t = 0; t < num_bins; t++) {
        if (randf() < p_spike) count++;
    }

    if (count == 0) {
        *spike_times = NULL;
        *num_spikes = 0;
        return 0;
    }

    /* Allocate and fill */
    *spike_times = nimcp_calloc(count, sizeof(uint32_t));
    if (!*spike_times) return NIMCP_ERROR_VAE_SNN_NO_MEMORY;

    uint32_t idx = 0;
    for (uint32_t t = 0; t < num_bins && idx < count; t++) {
        if (randf() < p_spike) {
            (*spike_times)[idx++] = t;
        }
    }

    *num_spikes = idx;
    return 0;
}

/**
 * @brief Update bridge statistics
 */
static void update_stats(vae_snn_bridge_t* bridge, bool is_encode,
                         uint64_t time_us, uint32_t spike_count)
{
    if (!bridge) return;

    if (is_encode) {
        bridge->stats.total_encodes++;
        bridge->stats.total_spikes_generated += spike_count;
        bridge->stats.avg_encoding_time_us =
            0.95f * bridge->stats.avg_encoding_time_us + 0.05f * (float)time_us;
    } else {
        bridge->stats.total_decodes++;
        bridge->stats.total_spikes_processed += spike_count;
        bridge->stats.avg_decoding_time_us =
            0.95f * bridge->stats.avg_decoding_time_us + 0.05f * (float)time_us;
    }

    bridge->stats.last_operation_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_snn_bridge_default_config(vae_snn_bridge_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_SNN_NULL;

    memset(config, 0, sizeof(*config));

    /* Encoding/decoding methods */
    config->encode_method = VAE_SNN_ENCODE_RATE;
    config->decode_method = VAE_SNN_DECODE_RATE;
    config->precision_map = VAE_SNN_PREC_TIMING_JITTER;

    /* Rate coding config */
    config->rate_config.min_rate_hz = 1.0f;
    config->rate_config.max_rate_hz = 200.0f;
    config->rate_config.latent_min = -3.0f;
    config->rate_config.latent_max = 3.0f;
    config->rate_config.use_softplus = false;

    /* Temporal coding config */
    config->temporal_config.window_ms = VAE_SNN_DEFAULT_WINDOW_MS;
    config->temporal_config.min_latency_ms = 5.0f;
    config->temporal_config.max_latency_ms = 50.0f;
    config->temporal_config.inverse_latency = true;

    /* Population parameters */
    config->neurons_per_latent_dim = 8;
    config->population_overlap = 0.25f;

    /* Time window */
    config->encoding_window_ms = VAE_SNN_DEFAULT_WINDOW_MS;
    config->decoding_window_ms = VAE_SNN_DEFAULT_WINDOW_MS;
    config->dt_ms = 1.0f;

    /* Precision parameters */
    config->precision_to_jitter_scale = 10.0f;
    config->min_timing_jitter_ms = 0.1f;
    config->max_timing_jitter_ms = 10.0f;

    /* Options */
    config->enable_adaptation = true;
    config->enable_noise = true;
    config->noise_level = 0.1f;
    config->enable_logging = false;

    return 0;
}

vae_snn_bridge_t* vae_snn_bridge_create(const vae_snn_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_snn_bridge_create: config is NULL");
        return NULL;
    }

    vae_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_snn_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_SNN_STATE_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();

    /* Allocate population array */
    bridge->populations = nimcp_calloc(VAE_SNN_MAX_POPULATIONS,
                                        sizeof(vae_snn_population_config_t));
    if (!bridge->populations) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_snn_bridge_create: bridge->populations is NULL");
        return NULL;
    }
    bridge->num_populations = 0;

    /* Allocate working buffers (will resize when connected) */
    uint32_t default_dim = VAE_SNN_DEFAULT_NEURONS;
    bridge->encode_buffer = nimcp_calloc(default_dim, sizeof(float));
    bridge->decode_buffer = nimcp_calloc(default_dim, sizeof(float));
    bridge->spike_buffer = nimcp_calloc(default_dim * 100, sizeof(uint32_t));

    if (!bridge->encode_buffer || !bridge->decode_buffer || !bridge->spike_buffer) {
        vae_snn_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_snn_bridge_create: required parameter is NULL (bridge->encode_buffer, bridge->decode_buffer, bridge->spike_buffer)");
        return NULL;
    }

    /* Initialize statistics */
    bridge->stats.creation_time_us = bridge->creation_time_us;
    bridge->stats.avg_firing_rate_hz = VAE_SNN_DEFAULT_RATE_HZ;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        nimcp_log_info(VAE_SNN_MODULE_ID, "VAE-SNN Bridge created");
    }

    return bridge;
}

void vae_snn_bridge_destroy(vae_snn_bridge_t* bridge)
{
    if (!bridge) return;

    /* Disconnect if connected */
    vae_snn_bridge_disconnect(bridge);

    /* Free populations */
    nimcp_free(bridge->populations);

    /* Free working buffers */
    nimcp_free(bridge->encode_buffer);
    nimcp_free(bridge->decode_buffer);
    nimcp_free(bridge->spike_buffer);

    /* Free precision state */
    nimcp_free(bridge->current_precision);
    nimcp_free(bridge->timing_jitter);
    nimcp_free(bridge->adaptation_state);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_snn_bridge_connect_vae(vae_snn_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_SNN_NULL;
    if (!vae) return NIMCP_ERROR_VAE_SNN_NULL;

    bridge->vae = vae;

    /* Get latent dimension and allocate precision buffers */
    uint32_t latent_dim = vae_get_latent_dim(vae);
    if (latent_dim > 0) {
        nimcp_free(bridge->current_precision);
        bridge->current_precision = nimcp_calloc(latent_dim, sizeof(float));
        if (!bridge->current_precision) return NIMCP_ERROR_VAE_SNN_NO_MEMORY;

        /* Initialize precision to 1.0 */
        for (uint32_t i = 0; i < latent_dim; i++) {
            bridge->current_precision[i] = 1.0f;
        }
    }

    /* Auto-configure populations if none defined */
    if (bridge->num_populations == 0) {
        vae_snn_auto_configure(bridge);
    }

    if (bridge->snn_network) {
        bridge->state = VAE_SNN_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SNN_MODULE_ID, "VAE connected (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_snn_bridge_connect_snn(vae_snn_bridge_t* bridge, void* snn_network)
{
    if (!bridge) return NIMCP_ERROR_VAE_SNN_NULL;

    bridge->snn_network = snn_network;

    if (bridge->vae) {
        bridge->state = VAE_SNN_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SNN_MODULE_ID, "SNN network connected");
    }

    return 0;
}

int vae_snn_bridge_disconnect(vae_snn_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SNN_NULL;

    bridge->vae = NULL;
    bridge->snn_network = NULL;
    bridge->state = VAE_SNN_STATE_DISCONNECTED;

    return 0;
}

bool vae_snn_bridge_is_connected(const vae_snn_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_snn_bridge_is_connected: bridge is NULL");
        return false;
    }
    return bridge->state == VAE_SNN_STATE_CONNECTED;
}

/* ============================================================================
 * Population Configuration API
 * ============================================================================ */

int vae_snn_add_population(vae_snn_bridge_t* bridge,
                            const vae_snn_population_config_t* pop_config)
{
    if (!bridge || !pop_config) return NIMCP_ERROR_VAE_SNN_NULL;
    if (bridge->num_populations >= VAE_SNN_MAX_POPULATIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "vae_snn_add_population: capacity exceeded");
        return -1;
    }

    bridge->populations[bridge->num_populations] = *pop_config;
    bridge->num_populations++;

    return 0;
}

int vae_snn_configure_mapping(vae_snn_bridge_t* bridge,
                               uint32_t latent_dim,
                               uint32_t neurons_per_dim)
{
    if (!bridge) return NIMCP_ERROR_VAE_SNN_NULL;

    /* Clear existing populations */
    bridge->num_populations = 0;

    /* Create one population per latent dimension */
    for (uint32_t d = 0; d < latent_dim && d < VAE_SNN_MAX_POPULATIONS; d++) {
        vae_snn_population_config_t pop = {
            .population_id = d,
            .num_neurons = neurons_per_dim,
            .latent_dim_start = d,
            .latent_dim_count = 1,
            .tuning_width = 1.0f,
            .max_rate_hz = bridge->config.rate_config.max_rate_hz
        };
        vae_snn_add_population(bridge, &pop);
    }

    return 0;
}

int vae_snn_auto_configure(vae_snn_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SNN_NULL;

    uint32_t latent_dim = 32; /* Default */
    if (bridge->vae) {
        latent_dim = vae_get_latent_dim(bridge->vae);
    }

    return vae_snn_configure_mapping(bridge, latent_dim,
                                      bridge->config.neurons_per_latent_dim);
}

/* ============================================================================
 * Encoding API (Latent → Spikes)
 * ============================================================================ */

int vae_snn_encode_latent(vae_snn_bridge_t* bridge,
                           const float* latent, uint32_t latent_dim,
                           float window_ms,
                           vae_snn_encode_result_t* result)
{
    if (!bridge || !latent || !result) return NIMCP_ERROR_VAE_SNN_NULL;
    if (bridge->state != VAE_SNN_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NOT_CONNECTED, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NOT_CONNECTED;
    }

    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_SNN_STATE_ENCODING;

    memset(result, 0, sizeof(*result));
    result->window_ms = window_ms;

    /* Calculate total neurons needed */
    uint32_t total_neurons = 0;
    for (uint32_t p = 0; p < bridge->num_populations; p++) {
        total_neurons += bridge->populations[p].num_neurons;
    }

    if (total_neurons == 0) {
        total_neurons = latent_dim * bridge->config.neurons_per_latent_dim;
    }

    result->num_neurons = total_neurons;
    result->spike_trains = nimcp_calloc(total_neurons, sizeof(vae_snn_spike_train_t));
    if (!result->spike_trains) {
        bridge->state = VAE_SNN_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NO_MEMORY, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NO_MEMORY;
    }

    /* Generate spikes based on encoding method */
    uint32_t neuron_idx = 0;
    uint32_t total_spike_count = 0;

    for (uint32_t d = 0; d < latent_dim; d++) {
        float latent_val = latent[d];
        uint32_t neurons_for_dim = bridge->config.neurons_per_latent_dim;

        switch (bridge->config.encode_method) {
            case VAE_SNN_ENCODE_RATE:
            case VAE_SNN_ENCODE_POISSON: {
                float rate_hz = latent_to_rate(bridge, latent_val);

                for (uint32_t n = 0; n < neurons_for_dim && neuron_idx < total_neurons; n++) {
                    vae_snn_spike_train_t* train = &result->spike_trains[neuron_idx];
                    train->neuron_id = neuron_idx;
                    train->window_ms = window_ms;

                    /* Add noise if enabled */
                    float effective_rate = rate_hz;
                    if (bridge->config.enable_noise) {
                        effective_rate *= (1.0f + (randf() - 0.5f) * 2.0f * bridge->config.noise_level);
                    }

                    generate_poisson_spikes(effective_rate, window_ms,
                                           &train->spike_times_ms, &train->num_spikes);
                    total_spike_count += train->num_spikes;
                    neuron_idx++;
                }
                break;
            }

            case VAE_SNN_ENCODE_TEMPORAL: {
                float spike_time_ms = latent_to_spike_time(bridge, latent_val);

                for (uint32_t n = 0; n < neurons_for_dim && neuron_idx < total_neurons; n++) {
                    vae_snn_spike_train_t* train = &result->spike_trains[neuron_idx];
                    train->neuron_id = neuron_idx;
                    train->window_ms = window_ms;
                    train->num_spikes = 1;
                    train->spike_times_ms = nimcp_calloc(1, sizeof(uint32_t));
                    if (train->spike_times_ms) {
                        /* Add jitter if noise enabled */
                        float jitter = 0.0f;
                        if (bridge->config.enable_noise && bridge->timing_jitter) {
                            jitter = bridge->timing_jitter[d % latent_dim] * (randf() - 0.5f);
                        }
                        train->spike_times_ms[0] = (uint32_t)nimcp_clampf(spike_time_ms + jitter, 0.0f, window_ms);
                        total_spike_count++;
                    }
                    neuron_idx++;
                }
                break;
            }

            case VAE_SNN_ENCODE_BURST: {
                float rate_hz = latent_to_rate(bridge, latent_val);
                uint32_t burst_count = (uint32_t)(rate_hz / 20.0f); /* ~20 Hz per burst */
                burst_count = (burst_count < 1) ? 1 : (burst_count > 7 ? 7 : burst_count);

                for (uint32_t n = 0; n < neurons_for_dim && neuron_idx < total_neurons; n++) {
                    vae_snn_spike_train_t* train = &result->spike_trains[neuron_idx];
                    train->neuron_id = neuron_idx;
                    train->window_ms = window_ms;
                    train->num_spikes = burst_count;
                    train->spike_times_ms = nimcp_calloc(burst_count, sizeof(uint32_t));
                    if (train->spike_times_ms) {
                        float start_time = randf() * (window_ms - burst_count * 2.0f);
                        for (uint32_t b = 0; b < burst_count; b++) {
                            train->spike_times_ms[b] = (uint32_t)(start_time + b * 2.0f);
                        }
                        total_spike_count += burst_count;
                    }
                    neuron_idx++;
                }
                break;
            }

            default:
                /* Use rate coding as fallback */
                break;
        }
    }

    result->total_spike_count = (float)total_spike_count;
    result->avg_firing_rate_hz = (total_spike_count * 1000.0f) / (window_ms * total_neurons);

    uint64_t elapsed_us = get_timestamp_us() - start_us;
    result->encoding_time_us = (float)elapsed_us;

    update_stats(bridge, true, elapsed_us, total_spike_count);

    bridge->state = VAE_SNN_STATE_CONNECTED;
    return 0;
}

int vae_snn_encode_with_precision(vae_snn_bridge_t* bridge,
                                   const float* latent_mu,
                                   const float* latent_var,
                                   uint32_t latent_dim,
                                   float window_ms,
                                   vae_snn_encode_result_t* result)
{
    if (!bridge || !latent_mu || !latent_var) return NIMCP_ERROR_VAE_SNN_NULL;

    /* Update timing jitter based on variance (precision = 1/var) */
    nimcp_free(bridge->timing_jitter);
    bridge->timing_jitter = nimcp_calloc(latent_dim, sizeof(float));
    if (bridge->timing_jitter) {
        for (uint32_t d = 0; d < latent_dim; d++) {
            float precision = 1.0f / (latent_var[d] + 1e-6f);
            bridge->timing_jitter[d] = vae_snn_precision_to_jitter(bridge, precision);
        }
    }

    return vae_snn_encode_latent(bridge, latent_mu, latent_dim, window_ms, result);
}

int vae_snn_encode_from_input(vae_snn_bridge_t* bridge,
                               const float* input, uint32_t input_dim,
                               float window_ms,
                               vae_snn_encode_result_t* result)
{
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_SNN_NULL;
    if (!bridge->vae) return NIMCP_ERROR_VAE_SNN_NOT_CONNECTED;

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    /* Create tensors for VAE encode */
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&input_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NO_MEMORY, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NO_MEMORY;
    }

    /* Copy input data */
    memcpy(TENSOR_DATA_F32(input_tensor), input, input_dim * sizeof(float));

    /* Encode through VAE */
    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_ENCODE_FAILED, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_ENCODE_FAILED;
    }

    /* Encode latent to spikes with precision */
    ret = vae_snn_encode_with_precision(bridge,
                                         TENSOR_DATA_F32(mu_tensor),
                                         TENSOR_DATA_F32(log_var_tensor),
                                         latent_dim,
                                         window_ms,
                                         result);

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);
    return ret;
}

/* ============================================================================
 * Decoding API (Spikes → Latent)
 * ============================================================================ */

int vae_snn_decode_spikes(vae_snn_bridge_t* bridge,
                           const vae_snn_spike_train_t* spike_trains,
                           uint32_t num_neurons,
                           vae_snn_decode_result_t* result)
{
    if (!bridge || !spike_trains || !result) return NIMCP_ERROR_VAE_SNN_NULL;
    if (bridge->state != VAE_SNN_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NOT_CONNECTED, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NOT_CONNECTED;
    }

    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_SNN_STATE_DECODING;

    memset(result, 0, sizeof(*result));

    /* Determine latent dimension */
    uint32_t latent_dim = num_neurons / bridge->config.neurons_per_latent_dim;
    if (latent_dim == 0) latent_dim = 1;

    result->latent_dim = latent_dim;
    result->latent_mu = nimcp_calloc(latent_dim, sizeof(float));
    result->latent_log_var = nimcp_calloc(latent_dim, sizeof(float));

    if (!result->latent_mu || !result->latent_log_var) {
        vae_snn_decode_result_free(result);
        bridge->state = VAE_SNN_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NO_MEMORY, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NO_MEMORY;
    }

    uint32_t total_spikes = 0;
    uint32_t neurons_per_dim = bridge->config.neurons_per_latent_dim;

    /* Decode based on method */
    for (uint32_t d = 0; d < latent_dim; d++) {
        float sum_value = 0.0f;
        float sum_sq_value = 0.0f;
        uint32_t neuron_count = 0;

        for (uint32_t n = d * neurons_per_dim;
             n < (d + 1) * neurons_per_dim && n < num_neurons;
             n++)
        {
            const vae_snn_spike_train_t* train = &spike_trains[n];
            total_spikes += train->num_spikes;

            switch (bridge->config.decode_method) {
                case VAE_SNN_DECODE_RATE: {
                    float rate_hz = (train->num_spikes * 1000.0f) / train->window_ms;
                    float latent_val = rate_to_latent(bridge, rate_hz);
                    sum_value += latent_val;
                    sum_sq_value += latent_val * latent_val;
                    neuron_count++;
                    break;
                }

                case VAE_SNN_DECODE_FIRST_SPIKE: {
                    if (train->num_spikes > 0 && train->spike_times_ms) {
                        float spike_time = (float)train->spike_times_ms[0];
                        const vae_snn_temporal_config_t* tc = &bridge->config.temporal_config;
                        float normalized = (spike_time - tc->min_latency_ms) /
                                          (tc->max_latency_ms - tc->min_latency_ms);
                        if (tc->inverse_latency) normalized = 1.0f - normalized;
                        normalized = nimcp_clampf(normalized, 0.0f, 1.0f);

                        const vae_snn_rate_config_t* rc = &bridge->config.rate_config;
                        float latent_val = rc->latent_min +
                                          normalized * (rc->latent_max - rc->latent_min);
                        sum_value += latent_val;
                        sum_sq_value += latent_val * latent_val;
                        neuron_count++;
                    }
                    break;
                }

                default:
                    /* Use rate decoding as fallback */
                    break;
            }
        }

        if (neuron_count > 0) {
            result->latent_mu[d] = sum_value / neuron_count;
            float mean_sq = sum_sq_value / neuron_count;
            float variance = mean_sq - result->latent_mu[d] * result->latent_mu[d];
            result->latent_log_var[d] = logf(fmaxf(variance, 1e-6f));
        }
    }

    /* Calculate confidence based on spike count consistency */
    float expected_spikes = bridge->stats.avg_firing_rate_hz *
                           spike_trains[0].window_ms / 1000.0f * num_neurons;
    result->decoding_confidence = 1.0f - fabsf(total_spikes - expected_spikes) /
                                         fmaxf(expected_spikes, 1.0f);
    result->decoding_confidence = nimcp_clampf(result->decoding_confidence, 0.0f, 1.0f);

    uint64_t elapsed_us = get_timestamp_us() - start_us;
    result->decoding_time_us = (float)elapsed_us;

    update_stats(bridge, false, elapsed_us, total_spikes);

    bridge->state = VAE_SNN_STATE_CONNECTED;
    return 0;
}

int vae_snn_decode_to_output(vae_snn_bridge_t* bridge,
                              const vae_snn_spike_train_t* spike_trains,
                              uint32_t num_neurons,
                              float* output, uint32_t* output_dim)
{
    if (!bridge || !spike_trains || !output || !output_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NULL, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NULL;
    }
    if (!bridge->vae) return NIMCP_ERROR_VAE_SNN_NOT_CONNECTED;

    /* First decode spikes to latent */
    vae_snn_decode_result_t decode_result;
    int ret = vae_snn_decode_spikes(bridge, spike_trains, num_neurons, &decode_result);
    if (ret != 0) return ret;

    /* Then decode latent through VAE decoder */
    uint32_t vae_output_dim = vae_get_output_dim(bridge->vae);
    nimcp_tensor_t* z_tensor = nimcp_tensor_create(&decode_result.latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create(&vae_output_dim, 1, NIMCP_DTYPE_F32);

    if (!z_tensor || !output_tensor) {
        nimcp_tensor_destroy(z_tensor);
        nimcp_tensor_destroy(output_tensor);
        vae_snn_decode_result_free(&decode_result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NO_MEMORY, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(z_tensor), decode_result.latent_mu,
           decode_result.latent_dim * sizeof(float));

    ret = vae_decode(bridge->vae, z_tensor, output_tensor);

    if (ret == 0) {
        memcpy(output, TENSOR_DATA_F32(output_tensor), vae_output_dim * sizeof(float));
        *output_dim = vae_output_dim;
    }

    nimcp_tensor_destroy(z_tensor);
    nimcp_tensor_destroy(output_tensor);
    vae_snn_decode_result_free(&decode_result);

    return ret;
}

/* ============================================================================
 * Roundtrip API
 * ============================================================================ */

int vae_snn_roundtrip(vae_snn_bridge_t* bridge,
                       const float* latent, uint32_t latent_dim,
                       float window_ms,
                       vae_snn_roundtrip_result_t* result)
{
    if (!bridge || !latent || !result) return NIMCP_ERROR_VAE_SNN_NULL;

    memset(result, 0, sizeof(*result));

    /* Encode latent to spikes */
    int ret = vae_snn_encode_latent(bridge, latent, latent_dim, window_ms,
                                    &result->encode);
    if (ret != 0) return ret;

    /* Decode spikes back to latent */
    ret = vae_snn_decode_spikes(bridge, result->encode.spike_trains,
                                 result->encode.num_neurons, &result->decode);
    if (ret != 0) return ret;

    /* Compute roundtrip error */
    result->round_trip_error = vae_snn_compute_reconstruction_error(
        bridge, latent, result->decode.latent_mu, latent_dim);

    return 0;
}

float vae_snn_compute_reconstruction_error(const vae_snn_bridge_t* bridge,
                                            const float* original,
                                            const float* reconstructed,
                                            uint32_t dim)
{
    (void)bridge;
    if (!original || !reconstructed || dim == 0) return -1.0f;

    float mse = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = original[i] - reconstructed[i];
        mse += diff * diff;
    }
    return mse / dim;
}

/* ============================================================================
 * Precision API
 * ============================================================================ */

int vae_snn_set_precision(vae_snn_bridge_t* bridge,
                           const float* precision, uint32_t dim)
{
    if (!bridge || !precision) return NIMCP_ERROR_VAE_SNN_NULL;

    nimcp_free(bridge->current_precision);
    bridge->current_precision = nimcp_calloc(dim, sizeof(float));
    if (!bridge->current_precision) return NIMCP_ERROR_VAE_SNN_NO_MEMORY;

    memcpy(bridge->current_precision, precision, dim * sizeof(float));

    /* Update timing jitter */
    nimcp_free(bridge->timing_jitter);
    bridge->timing_jitter = nimcp_calloc(dim, sizeof(float));
    if (bridge->timing_jitter) {
        for (uint32_t d = 0; d < dim; d++) {
            bridge->timing_jitter[d] = vae_snn_precision_to_jitter(bridge, precision[d]);
        }
    }

    return 0;
}

int vae_snn_update_precision_from_vae(vae_snn_bridge_t* bridge)
{
    if (!bridge || !bridge->vae) return NIMCP_ERROR_VAE_SNN_NULL;

    /* Get current VAE latent state */
    uint32_t dim = vae_get_latent_dim(bridge->vae);
    vae_latent_state_t* state = vae_latent_state_create(dim);
    if (!state) return NIMCP_ERROR_VAE_SNN_NO_MEMORY;

    int ret = vae_get_latent_state(bridge->vae, state);
    if (ret != 0 || !state->log_var) {
        vae_latent_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_snn_update_precision_from_vae: state->log_var is NULL");
        return -1;
    }

    float* precision = nimcp_calloc(dim, sizeof(float));
    if (!precision) {
        vae_latent_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SNN_NO_MEMORY, "vae_snn_bridge: error condition");
        return NIMCP_ERROR_VAE_SNN_NO_MEMORY;
    }

    for (uint32_t i = 0; i < dim; i++) {
        float var = expf(state->log_var[i]);
        precision[i] = 1.0f / (var + 1e-6f);
    }

    vae_latent_state_destroy(state);

    ret = vae_snn_set_precision(bridge, precision, dim);
    nimcp_free(precision);
    precision = NULL;

    return ret;
}

float vae_snn_precision_to_jitter(const vae_snn_bridge_t* bridge,
                                   float precision)
{
    if (!bridge) return 0.0f;

    /* High precision → low jitter, low precision → high jitter */
    float scale = bridge->config.precision_to_jitter_scale;
    float jitter = scale / (precision + 0.1f);

    return nimcp_clampf(jitter,
                  bridge->config.min_timing_jitter_ms,
                  bridge->config.max_timing_jitter_ms);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_snn_bridge_state_t vae_snn_bridge_get_state(const vae_snn_bridge_t* bridge)
{
    if (!bridge) return VAE_SNN_STATE_ERROR;
    return bridge->state;
}

int vae_snn_bridge_get_stats(const vae_snn_bridge_t* bridge,
                              vae_snn_bridge_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_SNN_NULL;
    *stats = bridge->stats;
    return 0;
}

uint32_t vae_snn_get_total_neurons(const vae_snn_bridge_t* bridge)
{
    if (!bridge) return 0;

    uint32_t total = 0;
    for (uint32_t p = 0; p < bridge->num_populations; p++) {
        total += bridge->populations[p].num_neurons;
    }
    return total;
}

const char* vae_snn_encode_method_to_string(vae_snn_encode_method_t method)
{
    switch (method) {
        case VAE_SNN_ENCODE_RATE: return "rate";
        case VAE_SNN_ENCODE_TEMPORAL: return "temporal";
        case VAE_SNN_ENCODE_POPULATION: return "population";
        case VAE_SNN_ENCODE_PHASE: return "phase";
        case VAE_SNN_ENCODE_BURST: return "burst";
        case VAE_SNN_ENCODE_POISSON: return "poisson";
        case VAE_SNN_ENCODE_RANK_ORDER: return "rank_order";
        default: return "unknown";
    }
}

const char* vae_snn_decode_method_to_string(vae_snn_decode_method_t method)
{
    switch (method) {
        case VAE_SNN_DECODE_RATE: return "rate";
        case VAE_SNN_DECODE_FIRST_SPIKE: return "first_spike";
        case VAE_SNN_DECODE_POPULATION: return "population";
        case VAE_SNN_DECODE_MEMBRANE: return "membrane";
        case VAE_SNN_DECODE_WEIGHTED_SUM: return "weighted_sum";
        default: return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_snn_encode_result_free(vae_snn_encode_result_t* result)
{
    if (!result) return;

    if (result->spike_trains) {
        for (uint32_t n = 0; n < result->num_neurons; n++) {
            nimcp_free(result->spike_trains[n].spike_times_ms);
        }
        nimcp_free(result->spike_trains);
    }

    memset(result, 0, sizeof(*result));
}

void vae_snn_decode_result_free(vae_snn_decode_result_t* result)
{
    if (!result) return;

    nimcp_free(result->latent_mu);
    nimcp_free(result->latent_log_var);

    memset(result, 0, sizeof(*result));
}

void vae_snn_roundtrip_result_free(vae_snn_roundtrip_result_t* result)
{
    if (!result) return;

    vae_snn_encode_result_free(&result->encode);
    vae_snn_decode_result_free(&result->decode);
}

void vae_snn_spike_train_free(vae_snn_spike_train_t* train)
{
    if (!train) return;

    nimcp_free(train->spike_times_ms);
    memset(train, 0, sizeof(*train));
}
