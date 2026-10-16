/**
 * @file nimcp_snn_encoding.c
 * @brief SNN Spike Encoding and Decoding Implementation
 *
 * WHAT: Conversions between continuous values and spike trains
 * WHY:  SNNs operate on discrete spikes, not continuous values
 * HOW:  Rate, temporal, population, and latency coding schemes
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/nimcp_snn_encoding.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>  /* For FLT_MAX in softmax numerical stability */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_encoding)

//=============================================================================
// Default Configurations
//=============================================================================

void snn_rate_encoder_config_default(snn_rate_encoder_config_t* config) {
    if (!config) return;

    config->max_rate = 200.0f;      /* 200 Hz max */
    config->min_rate = 0.0f;
    config->value_min = 0.0f;
    config->value_max = 1.0f;
    config->use_poisson = true;
}

void snn_temporal_encoder_config_default(snn_temporal_encoder_config_t* config) {
    if (!config) return;

    config->t_min = 0.0f;
    config->t_max = 20.0f;          /* 20 ms encoding window */
    config->value_min = 0.0f;
    config->value_max = 1.0f;
    config->inverse = false;
}

void snn_population_encoder_config_default(snn_population_encoder_config_t* config) {
    if (!config) return;

    config->n_neurons = 16;
    config->sigma = 0.25f;          /* Receptive field width */
    config->value_min = 0.0f;
    config->value_max = 1.0f;
    config->normalize_rates = true;
}

void snn_rate_decoder_config_default(snn_rate_decoder_config_t* config) {
    if (!config) return;

    config->time_window = 50.0f;    /* 50 ms integration window */
    config->max_rate = 200.0f;
    config->use_exponential = false;
    config->decay_tau = 20.0f;
}

//=============================================================================
// Encoder Creation
//=============================================================================

/**
 * @brief Allocate base encoder structure
 */
static snn_encoder_t* encoder_alloc(uint32_t n_inputs, uint32_t n_outputs,
                                     snn_encoding_t method) {
    snn_encoder_t* encoder = nimcp_malloc(sizeof(snn_encoder_t));
    if (!encoder) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_encoder_t),
            "Failed to allocate encoder structure");
        return NULL;
    }

    memset(encoder, 0, sizeof(snn_encoder_t));
    encoder->method = method;
    encoder->n_inputs = n_inputs;
    encoder->n_outputs = n_outputs;

    /* Allocate working buffers */
    encoder->spike_times = nimcp_malloc(n_outputs * sizeof(float));
    encoder->spike_mask = nimcp_malloc(n_outputs * sizeof(uint8_t));

    if (!encoder->spike_times || !encoder->spike_mask) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_outputs * sizeof(float),
            "Failed to allocate encoder buffers for %u outputs", n_outputs);
        snn_encoder_destroy(encoder);
        return NULL;
    }

    return encoder;
}

snn_encoder_t* snn_encoder_create_rate(uint32_t n_inputs,
                                        const snn_rate_encoder_config_t* config) {
    if (n_inputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encoder_create_rate: invalid args (n_inputs=%u, config=%p)",
            n_inputs, (void*)config);
        return NULL;
    }

    snn_encoder_t* encoder = encoder_alloc(n_inputs, n_inputs, SNN_ENCODE_RATE);
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_encoder_create_rate: encoder is NULL");
        return NULL;
    }

    encoder->config.rate = *config;

    NIMCP_LOGGING_DEBUG("Created rate encoder for %u inputs", n_inputs);
    return encoder;
}

snn_encoder_t* snn_encoder_create_temporal(uint32_t n_inputs,
                                            const snn_temporal_encoder_config_t* config) {
    if (n_inputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encoder_create_temporal: invalid args (n_inputs=%u, config=%p)",
            n_inputs, (void*)config);
        return NULL;
    }

    snn_encoder_t* encoder = encoder_alloc(n_inputs, n_inputs, SNN_ENCODE_TEMPORAL);
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_encoder_create_temporal: encoder is NULL");
        return NULL;
    }

    encoder->config.temporal = *config;

    NIMCP_LOGGING_DEBUG("Created temporal encoder for %u inputs", n_inputs);
    return encoder;
}

snn_encoder_t* snn_encoder_create_population(uint32_t n_inputs,
                                              const snn_population_encoder_config_t* config) {
    if (n_inputs == 0 || !config || config->n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encoder_create_population: invalid args (n_inputs=%u, config=%p)",
            n_inputs, (void*)config);
        return NULL;
    }

    /* Population encoder expands each input to n_neurons */
    uint32_t n_outputs = n_inputs * config->n_neurons;

    snn_encoder_t* encoder = encoder_alloc(n_inputs, n_outputs, SNN_ENCODE_POPULATION);
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_encoder_create_population: encoder is NULL");
        return NULL;
    }

    encoder->config.population = *config;

    NIMCP_LOGGING_DEBUG("Created population encoder: %u inputs -> %u neurons",
                        n_inputs, n_outputs);
    return encoder;
}

snn_encoder_t* snn_encoder_create_latency(uint32_t n_inputs,
                                           const snn_latency_encoder_config_t* config) {
    if (n_inputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encoder_create_latency: invalid args (n_inputs=%u, config=%p)",
            n_inputs, (void*)config);
        return NULL;
    }

    snn_encoder_t* encoder = encoder_alloc(n_inputs, n_inputs, SNN_ENCODE_LATENCY);
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_encoder_create_latency: encoder is NULL");
        return NULL;
    }

    encoder->config.latency = *config;

    NIMCP_LOGGING_DEBUG("Created latency encoder for %u inputs", n_inputs);
    return encoder;
}

void snn_encoder_destroy(snn_encoder_t* encoder) {
    if (!encoder) return;

    if (encoder->spike_times) nimcp_free(encoder->spike_times);
    if (encoder->spike_mask) nimcp_free(encoder->spike_mask);

    nimcp_free(encoder);
}

//=============================================================================
// Decoder Creation
//=============================================================================

/**
 * @brief Allocate base decoder structure
 */
static snn_decoder_t* decoder_alloc(uint32_t n_inputs, uint32_t n_outputs,
                                     snn_decoding_t method) {
    snn_decoder_t* decoder = nimcp_malloc(sizeof(snn_decoder_t));
    if (!decoder) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_decoder_t),
            "Failed to allocate decoder structure");
        return NULL;
    }

    memset(decoder, 0, sizeof(snn_decoder_t));
    decoder->method = method;
    decoder->n_inputs = n_inputs;
    decoder->n_outputs = n_outputs;

    /* Allocate working buffers */
    decoder->spike_counts = nimcp_malloc(n_inputs * sizeof(float));
    decoder->first_times = nimcp_malloc(n_inputs * sizeof(float));

    if (!decoder->spike_counts || !decoder->first_times) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n_inputs * sizeof(float),
            "Failed to allocate decoder buffers for %u inputs", n_inputs);
        snn_decoder_destroy(decoder);
        return NULL;
    }

    return decoder;
}

snn_decoder_t* snn_decoder_create_rate(uint32_t n_inputs,
                                        uint32_t n_outputs,
                                        const snn_rate_decoder_config_t* config) {
    if (n_inputs == 0 || n_outputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decoder_create_rate: invalid args (n_inputs=%u, n_outputs=%u, config=%p)",
            n_inputs, n_outputs, (void*)config);
        return NULL;
    }

    snn_decoder_t* decoder = decoder_alloc(n_inputs, n_outputs, SNN_DECODE_RATE);
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_decoder_create_rate: decoder is NULL");
        return NULL;
    }

    decoder->config.rate = *config;

    NIMCP_LOGGING_DEBUG("Created rate decoder: %u inputs -> %u outputs",
                        n_inputs, n_outputs);
    return decoder;
}

snn_decoder_t* snn_decoder_create_first_spike(uint32_t n_inputs,
                                               const snn_first_spike_decoder_config_t* config) {
    if (n_inputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decoder_create_first_spike: invalid args (n_inputs=%u, config=%p)",
            n_inputs, (void*)config);
        return NULL;
    }

    /* First-spike decoder outputs a single class */
    snn_decoder_t* decoder = decoder_alloc(n_inputs, 1, SNN_DECODE_FIRST_SPIKE);
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_decoder_create_first_spike: decoder is NULL");
        return NULL;
    }

    decoder->config.first_spike = *config;

    NIMCP_LOGGING_DEBUG("Created first-spike decoder for %u classes", n_inputs);
    return decoder;
}

snn_decoder_t* snn_decoder_create_population(uint32_t n_inputs,
                                              uint32_t n_outputs,
                                              const snn_population_decoder_config_t* config) {
    if (n_inputs == 0 || n_outputs == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decoder_create_population: invalid args (n_inputs=%u, n_outputs=%u, config=%p)",
            n_inputs, n_outputs, (void*)config);
        return NULL;
    }

    snn_decoder_t* decoder = decoder_alloc(n_inputs, n_outputs, SNN_DECODE_POPULATION);
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_decoder_create_population: decoder is NULL");
        return NULL;
    }

    decoder->config.population = *config;

    NIMCP_LOGGING_DEBUG("Created population decoder: %u inputs -> %u outputs",
                        n_inputs, n_outputs);
    return decoder;
}

void snn_decoder_destroy(snn_decoder_t* decoder) {
    if (!decoder) return;

    if (decoder->spike_counts) nimcp_free(decoder->spike_counts);
    if (decoder->first_times) nimcp_free(decoder->first_times);

    nimcp_free(decoder);
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Random number in [0, 1]
 */
static inline float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

int snn_encode_rate(snn_encoder_t* encoder,
                    const float* values,
                    float dt,
                    uint8_t* spikes_out) {
    if (!encoder || !values || !spikes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encode_rate: NULL pointer (encoder=%p, values=%p, spikes_out=%p)",
            (void*)encoder, (void*)values, (void*)spikes_out);
        return SNN_ERROR_NULL_POINTER;
    }
    if (encoder->method != SNN_ENCODE_RATE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_encode_rate: encoder method mismatch (expected RATE, got %d)",
            encoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_rate_encoder_config_t* cfg = &encoder->config.rate;
    float value_range = cfg->value_max - cfg->value_min;
    float rate_range = cfg->max_rate - cfg->min_rate;

    if (value_range <= 0.0f) value_range = 1.0f;

    for (uint32_t i = 0; i < encoder->n_inputs; i++) {
        /* Normalize value to [0, 1] */
        float norm = (values[i] - cfg->value_min) / value_range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        /* Convert to firing rate */
        float rate = cfg->min_rate + norm * rate_range;

        /* Probability of spike in this timestep */
        float p_spike = rate * dt / 1000.0f;  /* dt in ms, rate in Hz */

        if (cfg->use_poisson) {
            spikes_out[i] = (randf() < p_spike) ? 1 : 0;
        } else {
            /* Deterministic: spike if probability > 0.5 */
            spikes_out[i] = (p_spike > 0.5f) ? 1 : 0;
        }

        encoder->total_spikes += spikes_out[i];
    }

    encoder->encode_count++;
    return SNN_SUCCESS;
}

int snn_encode_temporal(snn_encoder_t* encoder,
                        const float* values,
                        float* spike_times_out) {
    if (!encoder || !values || !spike_times_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encode_temporal: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (encoder->method != SNN_ENCODE_TEMPORAL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_encode_temporal: encoder method mismatch (expected TEMPORAL, got %d)",
            encoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_temporal_encoder_config_t* cfg = &encoder->config.temporal;
    float value_range = cfg->value_max - cfg->value_min;
    float time_range = cfg->t_max - cfg->t_min;

    if (value_range <= 0.0f) value_range = 1.0f;

    for (uint32_t i = 0; i < encoder->n_inputs; i++) {
        /* Normalize value to [0, 1] */
        float norm = (values[i] - cfg->value_min) / value_range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        /* Convert to spike time */
        if (cfg->inverse) {
            /* High value = late spike */
            spike_times_out[i] = cfg->t_min + norm * time_range;
        } else {
            /* High value = early spike */
            spike_times_out[i] = cfg->t_max - norm * time_range;
        }

        encoder->total_spikes++;
    }

    encoder->encode_count++;
    return SNN_SUCCESS;
}

int snn_encode_population(snn_encoder_t* encoder,
                          const float* values,
                          float* rates_out) {
    if (!encoder || !values || !rates_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encode_population: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (encoder->method != SNN_ENCODE_POPULATION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_encode_population: encoder method mismatch (expected POPULATION, got %d)",
            encoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_population_encoder_config_t* cfg = &encoder->config.population;
    float value_range = cfg->value_max - cfg->value_min;

    if (value_range <= 0.0f) value_range = 1.0f;

    uint32_t out_idx = 0;
    for (uint32_t i = 0; i < encoder->n_inputs; i++) {
        /* Normalize value */
        float norm = (values[i] - cfg->value_min) / value_range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        /* Compute Gaussian activation for each neuron in population */
        float sum = 0.0f;

        /* NUMERICAL STABILITY: Pre-compute sigma squared with epsilon guard
         * WHY:  Division by sigma^2 can overflow if sigma is very small
         * HOW:  Add epsilon to prevent division by zero
         */
        float sigma_sq = cfg->sigma * cfg->sigma;
        if (sigma_sq < 1e-8f) sigma_sq = 1e-8f;  /* Minimum sigma^2 */
        float inv_two_sigma_sq = 1.0f / (2.0f * sigma_sq);

        for (uint32_t j = 0; j < cfg->n_neurons; j++) {
            /* Preferred value for this neuron */
            float preferred = (float)j / (float)(cfg->n_neurons - 1);
            float diff = norm - preferred;

            /* NUMERICAL STABILITY: Clamp exponential argument to prevent underflow
             * WHY:  exp(-x) underflows for x > ~88, and becomes negligible for x > 20
             * HOW:  Clamp diff^2 / (2*sigma^2) to reasonable range
             */
            float exp_arg = -diff * diff * inv_two_sigma_sq;
            if (exp_arg < -20.0f) exp_arg = -20.0f;  /* exp(-20) ≈ 2e-9 */
            float activation = expf(exp_arg);

            rates_out[out_idx + j] = activation;
            sum += activation;
        }

        /* Normalize if requested */
        if (cfg->normalize_rates && sum > 0.0f) {
            for (uint32_t j = 0; j < cfg->n_neurons; j++) {
                rates_out[out_idx + j] /= sum;
            }
        }

        out_idx += cfg->n_neurons;
    }

    encoder->encode_count++;
    return SNN_SUCCESS;
}

int snn_encode_latency(snn_encoder_t* encoder,
                       const float* values,
                       float* latencies_out) {
    if (!encoder || !values || !latencies_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encode_latency: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (encoder->method != SNN_ENCODE_LATENCY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_encode_latency: encoder method mismatch (expected LATENCY, got %d)",
            encoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_latency_encoder_config_t* cfg = &encoder->config.latency;

    for (uint32_t i = 0; i < encoder->n_inputs; i++) {
        float v = values[i];

        /* Below threshold: no spike */
        if (v < cfg->threshold) {
            latencies_out[i] = cfg->t_max;  /* Maximum latency = no spike */
            continue;
        }

        /* Compute latency */
        if (cfg->use_log) {
            /* Logarithmic: latency = tau * ln(1 / activation) */
            float activation = v - cfg->threshold;
            if (activation < 0.001f) activation = 0.001f;
            latencies_out[i] = cfg->tau * logf(1.0f / activation);
        } else {
            /* Linear: higher value = shorter latency */
            float norm = (v - cfg->threshold) / (1.0f - cfg->threshold);
            if (norm > 1.0f) norm = 1.0f;
            latencies_out[i] = cfg->t_max * (1.0f - norm);
        }

        /* Clamp latency */
        if (latencies_out[i] < 0.0f) latencies_out[i] = 0.0f;
        if (latencies_out[i] > cfg->t_max) latencies_out[i] = cfg->t_max;

        encoder->total_spikes++;
    }

    encoder->encode_count++;
    return SNN_SUCCESS;
}

int snn_encode(snn_encoder_t* encoder,
               const float* values,
               float dt,
               void* output) {
    if (!encoder || !values || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_encode: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    switch (encoder->method) {
        case SNN_ENCODE_RATE:
            return snn_encode_rate(encoder, values, dt, (uint8_t*)output);
        case SNN_ENCODE_TEMPORAL:
            return snn_encode_temporal(encoder, values, (float*)output);
        case SNN_ENCODE_POPULATION:
            return snn_encode_population(encoder, values, (float*)output);
        case SNN_ENCODE_LATENCY:
            return snn_encode_latency(encoder, values, (float*)output);
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "snn_encode: unsupported method %d", encoder->method);
            return SNN_ERROR_INVALID_CONFIG;
    }
}

//=============================================================================
// Decoding Functions
//=============================================================================

int snn_decode_rate(snn_decoder_t* decoder,
                    const float* spike_counts,
                    float* values_out) {
    if (!decoder || !spike_counts || !values_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decode_rate: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (decoder->method != SNN_DECODE_RATE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_decode_rate: decoder method mismatch (expected RATE, got %d)",
            decoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_rate_decoder_config_t* cfg = &decoder->config.rate;
    float time_s = cfg->time_window / 1000.0f;  /* Convert ms to s */

    /* For rate decoding, we just normalize spike counts to rates */
    for (uint32_t i = 0; i < decoder->n_outputs; i++) {
        /* Compute rate in Hz */
        float rate = spike_counts[i] / time_s;

        /* Normalize by max rate */
        values_out[i] = rate / cfg->max_rate;
        if (values_out[i] > 1.0f) values_out[i] = 1.0f;
    }

    decoder->total_outputs += decoder->n_outputs;
    decoder->decode_count++;
    return SNN_SUCCESS;
}

/** Minimum softmax temperature to prevent division by zero / overflow */
#define SNN_SOFTMAX_TEMPERATURE_MIN 0.001f
/** Maximum softmax temperature for reasonable numerical behavior */
#define SNN_SOFTMAX_TEMPERATURE_MAX 100.0f

int snn_decode_first_spike(snn_decoder_t* decoder,
                           const float* spike_times,
                           uint32_t* class_out,
                           float* confidence_out) {
    if (!decoder || !spike_times || !class_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decode_first_spike: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (decoder->method != SNN_DECODE_FIRST_SPIKE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_decode_first_spike: decoder method mismatch (expected FIRST_SPIKE, got %d)",
            decoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_first_spike_decoder_config_t* cfg = &decoder->config.first_spike;

    /* Validate softmax temperature parameter to prevent numerical issues
     * WHAT: Ensure temperature is in valid range
     * WHY:  Temperature near 0 causes exp() overflow, negative causes incorrect behavior
     * HOW:  Check bounds and return error if invalid
     */
    if (cfg->use_softmax) {
        if (cfg->temperature < SNN_SOFTMAX_TEMPERATURE_MIN) {
            NIMCP_LOGGING_ERROR("First-spike decoder: softmax temperature %.6f below minimum %.6f",
                                cfg->temperature, SNN_SOFTMAX_TEMPERATURE_MIN);
            return SNN_ERROR_INVALID_CONFIG;
        }
        if (cfg->temperature > SNN_SOFTMAX_TEMPERATURE_MAX) {
            NIMCP_LOGGING_WARN("First-spike decoder: softmax temperature %.2f above recommended max %.2f",
                                  cfg->temperature, SNN_SOFTMAX_TEMPERATURE_MAX);
            /* Continue with warning - high temperature is valid but may produce uniform outputs */
        }
    }

    /* Find neuron with earliest spike */
    float earliest = cfg->max_latency;
    uint32_t winner = 0;

    for (uint32_t i = 0; i < decoder->n_inputs; i++) {
        if (spike_times[i] < earliest) {
            earliest = spike_times[i];
            winner = i;
        }
    }

    *class_out = winner;

    /* Compute confidence */
    if (confidence_out) {
        if (cfg->use_softmax) {
            /* Softmax over latencies (inverted: shorter = higher)
             * NUMERICAL STABILITY: Use log-sum-exp trick to prevent overflow
             * WHY:  exp(x) overflows for x > ~88, causing NaN/Inf
             * HOW:  Compute max first, then exp(x - max) to keep values bounded
             */
            float sum_exp = 0.0f;
            float winner_exp = 0.0f;

            /* First pass: find maximum inv_latency for numerical stability */
            float max_inv_latency = -FLT_MAX;
            for (uint32_t i = 0; i < decoder->n_inputs; i++) {
                float inv_latency = (cfg->max_latency - spike_times[i]) / cfg->temperature;
                if (inv_latency > max_inv_latency) {
                    max_inv_latency = inv_latency;
                }
            }

            /* Second pass: compute softmax with shifted values */
            for (uint32_t i = 0; i < decoder->n_inputs; i++) {
                float inv_latency = (cfg->max_latency - spike_times[i]) / cfg->temperature;
                /* Shift by max to prevent overflow: exp(x - max) is always <= 1 */
                float shifted = inv_latency - max_inv_latency;
                /* Clamp to prevent underflow for very negative values */
                if (shifted < -20.0f) shifted = -20.0f;
                float exp_val = expf(shifted);
                sum_exp += exp_val;
                if (i == winner) winner_exp = exp_val;
            }

            /* Safe division with epsilon guard */
            *confidence_out = winner_exp / (sum_exp + 1e-8f);
        } else {
            /* Simple confidence: how much earlier than second-best */
            float second_earliest = cfg->max_latency;
            for (uint32_t i = 0; i < decoder->n_inputs; i++) {
                if (i != winner && spike_times[i] < second_earliest) {
                    second_earliest = spike_times[i];
                }
            }
            float margin = second_earliest - earliest;
            *confidence_out = margin / cfg->max_latency;
            if (*confidence_out > 1.0f) *confidence_out = 1.0f;
        }
    }

    decoder->total_outputs++;
    decoder->decode_count++;
    return SNN_SUCCESS;
}

int snn_decode_population(snn_decoder_t* decoder,
                          const float* activities,
                          float* values_out) {
    if (!decoder || !activities || !values_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decode_population: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (decoder->method != SNN_DECODE_POPULATION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_decode_population: decoder method mismatch (expected POPULATION, got %d)",
            decoder->method);
        return SNN_ERROR_INVALID_CONFIG;
    }

    const snn_population_decoder_config_t* cfg = &decoder->config.population;

    /* Population vector decoding: weighted sum of preferred values */
    float weighted_sum = 0.0f;
    float activity_sum = 0.0f;

    for (uint32_t i = 0; i < decoder->n_inputs; i++) {
        float activity = activities[i];
        float preferred = cfg->preferred_values ? cfg->preferred_values[i] :
                          (float)i / (float)(decoder->n_inputs - 1);

        weighted_sum += activity * preferred;
        activity_sum += activity;
    }

    if (activity_sum > 0.0f && cfg->normalize) {
        values_out[0] = weighted_sum / activity_sum;
    } else {
        values_out[0] = weighted_sum;
    }

    decoder->total_outputs++;
    decoder->decode_count++;
    return SNN_SUCCESS;
}

int snn_decode(snn_decoder_t* decoder,
               const void* input,
               void* output) {
    if (!decoder || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_decode: NULL pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    switch (decoder->method) {
        case SNN_DECODE_RATE:
            return snn_decode_rate(decoder, (const float*)input, (float*)output);
        case SNN_DECODE_FIRST_SPIKE:
            return snn_decode_first_spike(decoder, (const float*)input,
                                          (uint32_t*)output, NULL);
        case SNN_DECODE_POPULATION:
            return snn_decode_population(decoder, (const float*)input, (float*)output);
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "snn_decode: unsupported method %d", decoder->method);
            return SNN_ERROR_INVALID_CONFIG;
    }
}

//=============================================================================
// Statistics Functions
//=============================================================================

void snn_encoder_get_stats(const snn_encoder_t* encoder,
                           uint64_t* total_spikes,
                           uint64_t* encode_count) {
    if (!encoder) return;

    if (total_spikes) *total_spikes = encoder->total_spikes;
    if (encode_count) *encode_count = encoder->encode_count;
}

void snn_decoder_get_stats(const snn_decoder_t* decoder,
                           uint64_t* total_outputs,
                           uint64_t* decode_count) {
    if (!decoder) return;

    if (total_outputs) *total_outputs = decoder->total_outputs;
    if (decode_count) *decode_count = decoder->decode_count;
}

void snn_encoder_reset_stats(snn_encoder_t* encoder) {
    if (!encoder) return;

    encoder->total_spikes = 0;
    encoder->encode_count = 0;
}

void snn_decoder_reset_stats(snn_decoder_t* decoder) {
    if (!decoder) return;

    decoder->total_outputs = 0;
    decoder->decode_count = 0;
}
