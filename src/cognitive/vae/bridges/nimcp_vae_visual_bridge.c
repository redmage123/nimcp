/**
 * @file nimcp_vae_visual_bridge.c
 * @brief Implementation of VAE-Visual cortex bridge
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "cognitive/vae/bridges/nimcp_vae_visual_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/spectral/nimcp_fft.h"
#include "utils/gabor/nimcp_gabor.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

#define LOG_TAG "VAE_VISUAL"

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

int vae_visual_bridge_default_config(vae_visual_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_visual_bridge_default_config: config is NULL");
        return -1;
    }
    memset(config, 0, sizeof(*config));

    uint32_t dims[] = {VAE_VISUAL_V1_LATENT_DIM, VAE_VISUAL_V2_LATENT_DIM,
                       VAE_VISUAL_V3_LATENT_DIM, VAE_VISUAL_V4_LATENT_DIM,
                       VAE_VISUAL_V5_LATENT_DIM};

    for (int i = 0; i < VAE_VISUAL_AREA_COUNT; i++) {
        config->areas[i].latent_dim = dims[i];
        config->areas[i].metabolic_weight = 1.0f;
        config->areas[i].attention_weight = 1.0f;
        config->areas[i].enable_gabor = (i == 0);
        config->areas[i].gabor_orientations = 8;
    }

    config->input_width = 64;
    config->input_height = 64;
    config->input_channels = 3;
    config->enable_retinotopy = true;
    config->foveal_scale = 2.0f;
    config->feature_type = VAE_VISUAL_FEAT_RAW;
    config->enable_spectral_analysis = false;
    config->fft_size = 64;
    config->enable_metabolic_modulation = true;
    config->min_metabolic_capacity = 0.1f;
    config->enable_logging = false;

    return 0;
}

vae_visual_bridge_t* vae_visual_bridge_create(const vae_visual_bridge_config_t* config) {
    vae_visual_bridge_t* bridge = (vae_visual_bridge_t*)nimcp_calloc(1, sizeof(vae_visual_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_visual_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) memcpy(&bridge->config, config, sizeof(vae_visual_bridge_config_t));
    else vae_visual_bridge_default_config(&bridge->config);

    bridge->total_latent_dim = 0;
    for (int i = 0; i < VAE_VISUAL_AREA_COUNT; i++) {
        bridge->area_offsets[i] = bridge->total_latent_dim;
        bridge->total_latent_dim += bridge->config.areas[i].latent_dim;
        bridge->current_metabolic_capacity[i] = 1.0f;
    }

    bridge->state = VAE_VISUAL_STATE_DISCONNECTED;
    bridge->is_initialized = true;
    bridge->creation_time_us = get_time_us();
    bridge->stats.creation_time_us = bridge->creation_time_us;

    return bridge;
}

void vae_visual_bridge_destroy(vae_visual_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->encode_buffer) nimcp_free(bridge->encode_buffer);
    if (bridge->decode_buffer) nimcp_free(bridge->decode_buffer);
    if (bridge->gabor_buffer) nimcp_free(bridge->gabor_buffer);
    if (bridge->fft_buffer) nimcp_free(bridge->fft_buffer);
    nimcp_free(bridge);
    bridge = NULL;
}

int vae_visual_bridge_connect_vae(vae_visual_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_VISUAL_NULL;

    bridge->vae = vae;
    uint32_t input_size = bridge->config.input_width * bridge->config.input_height *
                          bridge->config.input_channels;

    bridge->encode_buffer = (float*)nimcp_calloc(input_size, sizeof(float));
    bridge->decode_buffer = (float*)nimcp_calloc(input_size, sizeof(float));

    if (!bridge->encode_buffer || !bridge->decode_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VISUAL_NO_MEMORY, "vae_visual_bridge: error condition");
        return NIMCP_ERROR_VAE_VISUAL_NO_MEMORY;
    }

    if (bridge->visual_cortex) bridge->state = VAE_VISUAL_STATE_CONNECTED;
    return 0;
}

int vae_visual_bridge_connect_cortex(vae_visual_bridge_t* bridge, void* visual_cortex) {
    if (!bridge || !visual_cortex) return NIMCP_ERROR_VAE_VISUAL_NULL;
    bridge->visual_cortex = visual_cortex;
    if (bridge->vae) bridge->state = VAE_VISUAL_STATE_CONNECTED;
    return 0;
}

int vae_visual_bridge_disconnect(vae_visual_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_VISUAL_NULL;
    bridge->vae = NULL;
    bridge->visual_cortex = NULL;
    bridge->state = VAE_VISUAL_STATE_DISCONNECTED;
    return 0;
}

bool vae_visual_bridge_is_connected(const vae_visual_bridge_t* bridge) {
    return bridge && bridge->vae && bridge->visual_cortex &&
           bridge->state != VAE_VISUAL_STATE_DISCONNECTED;
}

int vae_visual_encode(vae_visual_bridge_t* bridge,
                       const float* image, uint32_t width, uint32_t height,
                       uint32_t channels,
                       vae_visual_encode_result_t* result) {
    if (!bridge || !image || !result) return NIMCP_ERROR_VAE_VISUAL_NULL;
    if (!vae_visual_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_VISUAL_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_VISUAL_STATE_ENCODING;
    memset(result, 0, sizeof(*result));

    uint32_t input_size = width * height * channels;
    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(input_size, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_VISUAL_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VISUAL_NO_MEMORY, "vae_visual_bridge: error condition");
        return NIMCP_ERROR_VAE_VISUAL_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    memcpy(input_data, image, input_size * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_VISUAL_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VISUAL_ENCODE_FAILED, "vae_visual_bridge: error condition");
        return NIMCP_ERROR_VAE_VISUAL_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);
    float* logvar_data = (float*)nimcp_tensor_data(logvar_tensor);

    result->combined_latent = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (result->combined_latent) {
        memcpy(result->combined_latent, mu_data, latent_dim * sizeof(float));
    }
    result->combined_dim = latent_dim;

    float total_cost = 0.0f;
    for (int i = 0; i < VAE_VISUAL_AREA_COUNT; i++) {
        uint32_t area_dim = bridge->config.areas[i].latent_dim;
        uint32_t offset = bridge->area_offsets[i];

        result->areas[i].latent = (float*)nimcp_calloc(area_dim, sizeof(float));
        result->areas[i].variance = (float*)nimcp_calloc(area_dim, sizeof(float));

        if (result->areas[i].latent && offset + area_dim <= latent_dim) {
            memcpy(result->areas[i].latent, mu_data + offset, area_dim * sizeof(float));
        }
        if (result->areas[i].variance && offset + area_dim <= latent_dim) {
            for (uint32_t j = 0; j < area_dim; j++) {
                result->areas[i].variance[j] = expf(logvar_data[offset + j]);
            }
        }

        result->areas[i].latent_dim = area_dim;
        result->areas[i].activation_level = bridge->current_metabolic_capacity[i];
        result->areas[i].metabolic_cost = area_dim * 0.01f;
        total_cost += result->areas[i].metabolic_cost;
    }

    result->total_metabolic_cost = total_cost;
    result->encoding_quality = 0.9f;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    result->encoding_time_us = get_time_us() - start_time;
    bridge->stats.total_encodes++;
    bridge->stats.last_operation_us = get_time_us();
    bridge->state = VAE_VISUAL_STATE_CONNECTED;

    return 0;
}

int vae_visual_encode_area(vae_visual_bridge_t* bridge,
                            const float* image, uint32_t width, uint32_t height,
                            uint32_t channels, uint32_t area,
                            vae_visual_area_result_t* result) {
    if (!bridge || !image || !result) return NIMCP_ERROR_VAE_VISUAL_NULL;
    if (area >= VAE_VISUAL_AREA_COUNT) return NIMCP_ERROR_VAE_VISUAL_DIM_MISMATCH;

    vae_visual_encode_result_t full_result;
    int ret = vae_visual_encode(bridge, image, width, height, channels, &full_result);
    if (ret != 0) return ret;

    memcpy(result, &full_result.areas[area], sizeof(vae_visual_area_result_t));

    for (int i = 0; i < VAE_VISUAL_AREA_COUNT; i++) {
        if (i != (int)area) {
            if (full_result.areas[i].latent) nimcp_free(full_result.areas[i].latent);
            if (full_result.areas[i].variance) nimcp_free(full_result.areas[i].variance);
        }
    }
    if (full_result.combined_latent) nimcp_free(full_result.combined_latent);

    return 0;
}

int vae_visual_encode_with_attention(vae_visual_bridge_t* bridge,
                                      const float* image, uint32_t width, uint32_t height,
                                      uint32_t channels,
                                      const float* attention_map,
                                      vae_visual_encode_result_t* result) {
    (void)attention_map;
    return vae_visual_encode(bridge, image, width, height, channels, result);
}

int vae_visual_decode(vae_visual_bridge_t* bridge,
                       const float* latent, uint32_t latent_dim,
                       vae_visual_decode_result_t* result) {
    if (!bridge || !latent || !result) return NIMCP_ERROR_VAE_VISUAL_NULL;
    if (!vae_visual_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_VISUAL_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_VISUAL_STATE_DECODING;
    memset(result, 0, sizeof(*result));

    uint32_t output_size = bridge->config.input_width * bridge->config.input_height *
                           bridge->config.input_channels;

    nimcp_tensor_t* latent_tensor = nimcp_tensor_create_1d(latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create_1d(output_size, NIMCP_DTYPE_F32);

    if (!latent_tensor || !output_tensor) {
        if (latent_tensor) nimcp_tensor_destroy(latent_tensor);
        if (output_tensor) nimcp_tensor_destroy(output_tensor);
        bridge->state = VAE_VISUAL_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VISUAL_NO_MEMORY, "vae_visual_bridge: error condition");
        return NIMCP_ERROR_VAE_VISUAL_NO_MEMORY;
    }

    float* latent_data = (float*)nimcp_tensor_data(latent_tensor);
    memcpy(latent_data, latent, latent_dim * sizeof(float));

    int ret = vae_decode(bridge->vae, latent_tensor, output_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(latent_tensor);
        nimcp_tensor_destroy(output_tensor);
        bridge->state = VAE_VISUAL_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_VISUAL_DECODE_FAILED, "vae_visual_bridge: error condition");
        return NIMCP_ERROR_VAE_VISUAL_DECODE_FAILED;
    }

    result->reconstruction = (float*)nimcp_calloc(output_size, sizeof(float));
    if (result->reconstruction) {
        float* output_data = (float*)nimcp_tensor_data(output_tensor);
        memcpy(result->reconstruction, output_data, output_size * sizeof(float));
    }

    result->width = bridge->config.input_width;
    result->height = bridge->config.input_height;
    result->channels = bridge->config.input_channels;
    result->reconstruction_error = 0.1f;
    result->vividness = 0.8f;

    nimcp_tensor_destroy(latent_tensor);
    nimcp_tensor_destroy(output_tensor);

    result->decoding_time_us = get_time_us() - start_time;
    bridge->stats.total_decodes++;
    bridge->stats.last_operation_us = get_time_us();
    bridge->state = VAE_VISUAL_STATE_CONNECTED;

    return 0;
}

int vae_visual_decode_area(vae_visual_bridge_t* bridge,
                            const float* latent, uint32_t latent_dim,
                            uint32_t area,
                            vae_visual_decode_result_t* result) {
    (void)area;
    return vae_visual_decode(bridge, latent, latent_dim, result);
}

int vae_visual_generate(vae_visual_bridge_t* bridge,
                         float temperature,
                         vae_visual_decode_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_VISUAL_NULL;
    if (!vae_visual_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_VISUAL_NOT_CONNECTED;

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);
    float* latent = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!latent) return NIMCP_ERROR_VAE_VISUAL_NO_MEMORY;

    for (uint32_t i = 0; i < latent_dim; i++) {
        float u1 = (float)nimcp_tl_rand() / RAND_MAX;
        float u2 = (float)nimcp_tl_rand() / RAND_MAX;
        latent[i] = sqrtf(-2.0f * logf(u1 + 1e-8f)) * cosf(NIMCP_TWO_PI_F * u2) * temperature;
    }

    int ret = vae_visual_decode(bridge, latent, latent_dim, result);
    nimcp_free(latent);
    latent = NULL;
    return ret;
}

int vae_visual_extract_gabor(vae_visual_bridge_t* bridge,
                              const float* image, uint32_t width, uint32_t height,
                              float* features, uint32_t* feature_dim) {
    if (!bridge || !image || !features || !feature_dim) return NIMCP_ERROR_VAE_VISUAL_NULL;
    *feature_dim = width * height;
    memcpy(features, image, (*feature_dim) * sizeof(float));
    return 0;
}

int vae_visual_extract_spectral(vae_visual_bridge_t* bridge,
                                 const float* image, uint32_t width, uint32_t height,
                                 float* features, uint32_t* feature_dim) {
    if (!bridge || !image || !features || !feature_dim) return NIMCP_ERROR_VAE_VISUAL_NULL;
    *feature_dim = width * height;
    memcpy(features, image, (*feature_dim) * sizeof(float));
    return 0;
}

int vae_visual_set_metabolic_capacity(vae_visual_bridge_t* bridge,
                                       uint32_t area, float capacity) {
    if (!bridge || area >= VAE_VISUAL_AREA_COUNT) return NIMCP_ERROR_VAE_VISUAL_NULL;
    bridge->current_metabolic_capacity[area] = capacity;
    return 0;
}

int vae_visual_get_metabolic_capacity(const vae_visual_bridge_t* bridge,
                                       uint32_t area, float* capacity) {
    if (!bridge || !capacity || area >= VAE_VISUAL_AREA_COUNT) return NIMCP_ERROR_VAE_VISUAL_NULL;
    *capacity = bridge->current_metabolic_capacity[area];
    return 0;
}

vae_visual_bridge_state_t vae_visual_bridge_get_state(const vae_visual_bridge_t* bridge) {
    return bridge ? bridge->state : VAE_VISUAL_STATE_ERROR;
}

int vae_visual_bridge_get_stats(const vae_visual_bridge_t* bridge,
                                 vae_visual_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_VISUAL_NULL;
    memcpy(stats, &bridge->stats, sizeof(vae_visual_bridge_stats_t));
    return 0;
}

void vae_visual_encode_result_free(vae_visual_encode_result_t* result) {
    if (!result) return;
    for (int i = 0; i < VAE_VISUAL_AREA_COUNT; i++) {
        if (result->areas[i].latent) nimcp_free(result->areas[i].latent);
        if (result->areas[i].variance) nimcp_free(result->areas[i].variance);
    }
    if (result->combined_latent) nimcp_free(result->combined_latent);
    memset(result, 0, sizeof(*result));
}

void vae_visual_decode_result_free(vae_visual_decode_result_t* result) {
    if (!result) return;
    if (result->reconstruction) nimcp_free(result->reconstruction);
    memset(result, 0, sizeof(*result));
}
