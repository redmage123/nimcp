/**
 * @file nimcp_vae_auditory_bridge.c
 * @brief Implementation of VAE-Auditory cortex bridge
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "cognitive/vae/bridges/nimcp_vae_auditory_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

#define LOG_TAG "VAE_AUDIO"
#define PI NIMCP_PI_F

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void hann_window(float* window, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (size - 1)));
    }
}

static float mel_to_hz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

static float hz_to_mel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

int vae_audio_bridge_default_config(vae_audio_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_audio_bridge_default_config: config is NULL");
        return -1;
    }
    memset(config, 0, sizeof(*config));

    config->sample_rate = VAE_AUDIO_DEFAULT_SAMPLE_RATE;
    config->fft_size = VAE_AUDIO_DEFAULT_FFT_SIZE;
    config->hop_size = VAE_AUDIO_DEFAULT_FFT_SIZE / 4;
    config->mel_bins = VAE_AUDIO_DEFAULT_MEL_BINS;
    config->mfcc_coeffs = VAE_AUDIO_DEFAULT_MFCC_COEFFS;
    config->min_freq = 20.0f;
    config->max_freq = 8000.0f;
    config->feature_type = VAE_AUDIO_FEAT_MEL;
    config->enable_delta_features = false;
    config->enable_energy = true;
    config->enable_novelty_detection = true;
    config->novelty_threshold = 0.5f;
    config->enable_logging = false;

    return 0;
}

vae_audio_bridge_t* vae_audio_bridge_create(const vae_audio_bridge_config_t* config) {
    vae_audio_bridge_t* bridge = (vae_audio_bridge_t*)nimcp_calloc(1, sizeof(vae_audio_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_audio_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) memcpy(&bridge->config, config, sizeof(vae_audio_bridge_config_t));
    else vae_audio_bridge_default_config(&bridge->config);

    bridge->fft_buffer = (float*)nimcp_calloc(bridge->config.fft_size, sizeof(float));
    if (!bridge->fft_buffer) return NULL;
    bridge->mel_buffer = (float*)nimcp_calloc(bridge->config.mel_bins, sizeof(float));
    if (!bridge->mel_buffer) return NULL;
    bridge->mfcc_buffer = (float*)nimcp_calloc(bridge->config.mfcc_coeffs, sizeof(float));
    if (!bridge->mfcc_buffer) return NULL;
    bridge->window = (float*)nimcp_calloc(bridge->config.fft_size, sizeof(float));

    if (bridge->window) {
        hann_window(bridge->window, bridge->config.fft_size);
    }

    bridge->state = VAE_AUDIO_STATE_DISCONNECTED;
    bridge->is_initialized = true;
    bridge->creation_time_us = get_time_us();
    bridge->stats.creation_time_us = bridge->creation_time_us;

    return bridge;
}

void vae_audio_bridge_destroy(vae_audio_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->fft_buffer) nimcp_free(bridge->fft_buffer);
    if (bridge->mel_buffer) nimcp_free(bridge->mel_buffer);
    if (bridge->mfcc_buffer) nimcp_free(bridge->mfcc_buffer);
    if (bridge->window) nimcp_free(bridge->window);
    if (bridge->novelty_baseline) nimcp_free(bridge->novelty_baseline);
    nimcp_free(bridge);
    bridge = NULL;
}

int vae_audio_bridge_connect_vae(vae_audio_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_AUDIO_NULL;
    bridge->vae = vae;
    if (bridge->audio_cortex) bridge->state = VAE_AUDIO_STATE_CONNECTED;
    return 0;
}

int vae_audio_bridge_connect_cortex(vae_audio_bridge_t* bridge, void* audio_cortex) {
    if (!bridge || !audio_cortex) return NIMCP_ERROR_VAE_AUDIO_NULL;
    bridge->audio_cortex = audio_cortex;
    if (bridge->vae) bridge->state = VAE_AUDIO_STATE_CONNECTED;
    return 0;
}

int vae_audio_bridge_disconnect(vae_audio_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_AUDIO_NULL;
    bridge->vae = NULL;
    bridge->audio_cortex = NULL;
    bridge->state = VAE_AUDIO_STATE_DISCONNECTED;
    return 0;
}

bool vae_audio_bridge_is_connected(const vae_audio_bridge_t* bridge) {
    return bridge && bridge->vae && bridge->audio_cortex &&
           bridge->state != VAE_AUDIO_STATE_DISCONNECTED;
}

int vae_audio_encode(vae_audio_bridge_t* bridge,
                      const float* audio, uint32_t num_samples,
                      vae_audio_encode_result_t* result) {
    if (!bridge || !audio || !result) return NIMCP_ERROR_VAE_AUDIO_NULL;
    if (!vae_audio_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_AUDIO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_AUDIO_STATE_ENCODING;
    memset(result, 0, sizeof(*result));

    /* Compute mel features from audio */
    uint32_t feature_dim = bridge->config.mel_bins;
    float* features = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!features) {
        bridge->state = VAE_AUDIO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_NO_MEMORY, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;
    }

    /* Simple energy-based feature extraction (simplified for this implementation) */
    float energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio[i] * audio[i];
    }
    energy = sqrtf(energy / (num_samples > 0 ? num_samples : 1));

    /* Distribute energy across mel bins (simplified) */
    for (uint32_t i = 0; i < feature_dim; i++) {
        features[i] = energy * (1.0f - (float)i / feature_dim);
    }

    /* Encode features through VAE */
    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(feature_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        nimcp_free(features);
        features = NULL;
        bridge->state = VAE_AUDIO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_NO_MEMORY, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    memcpy(input_data, features, feature_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        nimcp_free(features);
        features = NULL;
        bridge->state = VAE_AUDIO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_ENCODE_FAILED, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);

    result->latent = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!result->latent) return -1;
    result->features = features;
    result->feature_dim = feature_dim;
    result->latent_dim = latent_dim;
    result->energy = energy;

    if (result->latent) {
        memcpy(result->latent, mu_data, latent_dim * sizeof(float));
    }

    /* Compute novelty */
    if (bridge->config.enable_novelty_detection) {
        vae_audio_compute_novelty(bridge, audio, num_samples, &result->novelty_score);
    }

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    result->encoding_time_us = get_time_us() - start_time;
    bridge->stats.total_encodes++;
    bridge->stats.frames_processed++;
    bridge->stats.last_operation_us = get_time_us();
    bridge->state = VAE_AUDIO_STATE_CONNECTED;

    return 0;
}

int vae_audio_encode_spectrum(vae_audio_bridge_t* bridge,
                               const float* spectrum, uint32_t spectrum_dim,
                               vae_audio_encode_result_t* result) {
    return vae_audio_encode_features(bridge, spectrum, spectrum_dim, result);
}

int vae_audio_encode_features(vae_audio_bridge_t* bridge,
                               const float* features, uint32_t feature_dim,
                               vae_audio_encode_result_t* result) {
    if (!bridge || !features || !result) return NIMCP_ERROR_VAE_AUDIO_NULL;
    if (!vae_audio_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_AUDIO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    memset(result, 0, sizeof(*result));

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(feature_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_NO_MEMORY, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    memcpy(input_data, features, feature_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_ENCODE_FAILED, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);

    result->latent = (float*)nimcp_calloc(latent_dim, sizeof(float));
    result->latent_dim = latent_dim;
    if (result->latent) {
        memcpy(result->latent, mu_data, latent_dim * sizeof(float));
    }

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    result->encoding_time_us = get_time_us() - start_time;
    return 0;
}

int vae_audio_decode(vae_audio_bridge_t* bridge,
                      const float* latent, uint32_t latent_dim,
                      vae_audio_decode_result_t* result) {
    if (!bridge || !latent || !result) return NIMCP_ERROR_VAE_AUDIO_NULL;
    if (!vae_audio_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_AUDIO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    memset(result, 0, sizeof(*result));

    uint32_t output_dim = vae_get_output_dim(bridge->vae);

    nimcp_tensor_t* latent_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create_1d(output_dim, NIMCP_DTYPE_F32);

    if (!latent_tensor || !output_tensor) {
        if (latent_tensor) nimcp_tensor_destroy(latent_tensor);
        if (output_tensor) nimcp_tensor_destroy(output_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_NO_MEMORY, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;
    }

    float* latent_data = (float*)nimcp_tensor_data(latent_tensor);
    memcpy(latent_data, latent, latent_dim * sizeof(float));

    int ret = vae_decode(bridge->vae, latent_tensor, output_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(latent_tensor);
        nimcp_tensor_destroy(output_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_AUDIO_DECODE_FAILED, "vae_auditory_bridge: error condition");
        return NIMCP_ERROR_VAE_AUDIO_DECODE_FAILED;
    }

    result->spectrum = (float*)nimcp_calloc(output_dim, sizeof(float));
    result->spectrum_dim = output_dim;
    if (result->spectrum) {
        float* output_data = (float*)nimcp_tensor_data(output_tensor);
        memcpy(result->spectrum, output_data, output_dim * sizeof(float));
    }

    nimcp_tensor_destroy(latent_tensor);
    nimcp_tensor_destroy(output_tensor);

    result->decoding_time_us = get_time_us() - start_time;
    bridge->stats.total_decodes++;
    return 0;
}

int vae_audio_generate(vae_audio_bridge_t* bridge,
                        float temperature,
                        vae_audio_decode_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_AUDIO_NULL;

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);
    float* latent = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!latent) return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;

    for (uint32_t i = 0; i < latent_dim; i++) {
        float u1 = (float)nimcp_tl_rand() / RAND_MAX;
        float u2 = (float)nimcp_tl_rand() / RAND_MAX;
        latent[i] = sqrtf(-2.0f * logf(u1 + 1e-8f)) * cosf(2.0f * PI * u2) * temperature;
    }

    int ret = vae_audio_decode(bridge, latent, latent_dim, result);
    nimcp_free(latent);
    latent = NULL;
    return ret;
}

int vae_audio_compute_mel(vae_audio_bridge_t* bridge,
                           const float* audio, uint32_t num_samples,
                           float* mel_features, uint32_t* mel_dim) {
    if (!bridge || !audio || !mel_features || !mel_dim) return NIMCP_ERROR_VAE_AUDIO_NULL;

    *mel_dim = bridge->config.mel_bins;

    /* Simplified mel computation */
    float energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio[i] * audio[i];
    }
    energy = sqrtf(energy / (num_samples > 0 ? num_samples : 1));

    for (uint32_t i = 0; i < *mel_dim; i++) {
        mel_features[i] = energy * (1.0f - (float)i / (*mel_dim));
    }

    return 0;
}

int vae_audio_compute_mfcc(vae_audio_bridge_t* bridge,
                            const float* audio, uint32_t num_samples,
                            float* mfcc_features, uint32_t* mfcc_dim) {
    if (!bridge || !audio || !mfcc_features || !mfcc_dim) return NIMCP_ERROR_VAE_AUDIO_NULL;

    *mfcc_dim = bridge->config.mfcc_coeffs;

    /* Simplified MFCC (DCT of log mel) */
    float* mel = (float*)nimcp_calloc(bridge->config.mel_bins, sizeof(float));
    if (!mel) return NIMCP_ERROR_VAE_AUDIO_NO_MEMORY;

    uint32_t mel_dim = 0;
    vae_audio_compute_mel(bridge, audio, num_samples, mel, &mel_dim);

    for (uint32_t k = 0; k < *mfcc_dim; k++) {
        mfcc_features[k] = 0.0f;
        for (uint32_t n = 0; n < mel_dim; n++) {
            float log_mel = logf(mel[n] + 1e-8f);
            mfcc_features[k] += log_mel * cosf(PI * k * (n + 0.5f) / mel_dim);
        }
        mfcc_features[k] /= mel_dim;
    }

    nimcp_free(mel);
    mel = NULL;
    return 0;
}

int vae_audio_compute_novelty(vae_audio_bridge_t* bridge,
                               const float* audio, uint32_t num_samples,
                               float* novelty_score) {
    if (!bridge || !audio || !novelty_score) return NIMCP_ERROR_VAE_AUDIO_NULL;

    float energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio[i] * audio[i];
    }
    energy = sqrtf(energy / (num_samples > 0 ? num_samples : 1));

    /* Novelty based on energy deviation from baseline */
    if (bridge->baseline_samples > 0 && bridge->novelty_baseline) {
        float baseline_energy = bridge->novelty_baseline[0];
        float deviation = fabsf(energy - baseline_energy) / (baseline_energy + 1e-8f);
        *novelty_score = fminf(deviation, 1.0f);
    } else {
        *novelty_score = 0.5f;
    }

    /* Update baseline */
    if (!bridge->novelty_baseline) {
        bridge->novelty_baseline = (float*)nimcp_calloc(1, sizeof(float));
    }
    if (bridge->novelty_baseline) {
        bridge->novelty_baseline[0] = bridge->novelty_baseline[0] * 0.99f + energy * 0.01f;
        bridge->baseline_samples++;
    }

    if (isfinite(*novelty_score)) {
        bridge->stats.avg_novelty_score = bridge->stats.avg_novelty_score * 0.9f +
                                          (*novelty_score) * 0.1f;
    }

    return 0;
}

vae_audio_bridge_state_t vae_audio_bridge_get_state(const vae_audio_bridge_t* bridge) {
    return bridge ? bridge->state : VAE_AUDIO_STATE_ERROR;
}

int vae_audio_bridge_get_stats(const vae_audio_bridge_t* bridge,
                                vae_audio_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_AUDIO_NULL;
    memcpy(stats, &bridge->stats, sizeof(vae_audio_bridge_stats_t));
    return 0;
}

void vae_audio_encode_result_free(vae_audio_encode_result_t* result) {
    if (!result) return;
    if (result->latent) nimcp_free(result->latent);
    if (result->features) nimcp_free(result->features);
    memset(result, 0, sizeof(*result));
}

void vae_audio_decode_result_free(vae_audio_decode_result_t* result) {
    if (!result) return;
    if (result->audio) nimcp_free(result->audio);
    if (result->spectrum) nimcp_free(result->spectrum);
    memset(result, 0, sizeof(*result));
}
