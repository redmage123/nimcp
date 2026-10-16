/**
 * @file nimcp_vae_hippocampus_bridge.c
 * @brief Implementation of VAE-Hippocampus bridge for memory encoding
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements the bridge between VAE latent representations and
 * hippocampal memory operations including encoding, retrieval,
 * pattern separation, and pattern completion.
 */

#include "cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "VAE_HIPPO"
#define BASELINE_EMA_ALPHA 0.01f

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Cosine similarity uses nimcp_vector_cosine_similarity from utils/containers/nimcp_vector.h */
/* Euclidean distance uses nimcp_vector_euclidean_distance from utils/containers/nimcp_vector.h */

static float compute_sparsity(const float* vec, uint32_t dim, float threshold) {
    if (!vec || dim == 0) return 0.0f;

    uint32_t active = 0;
    for (uint32_t i = 0; i < dim; i++) {
        if (fabsf(vec[i]) > threshold) {
            active++;
        }
    }
    return 1.0f - ((float)active / (float)dim);
}

static uint32_t count_active_dimensions(const float* variance, uint32_t dim, float threshold) {
    if (!variance || dim == 0) return 0;

    uint32_t active = 0;
    for (uint32_t i = 0; i < dim; i++) {
        if (variance[i] > threshold) {
            active++;
        }
    }
    return active;
}

static void apply_sparsification(float* vec, uint32_t dim, float sparsity_target) {
    if (!vec || dim == 0 || sparsity_target <= 0.0f) return;

    uint32_t target_active = (uint32_t)((1.0f - sparsity_target) * dim);
    if (target_active == 0) target_active = 1;
    if (target_active >= dim) return;

    float* magnitudes = (float*)nimcp_calloc(dim, sizeof(float));
    if (!magnitudes) return;

    for (uint32_t i = 0; i < dim; i++) {
        magnitudes[i] = fabsf(vec[i]);
    }

    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = i + 1; j < dim; j++) {
            if (magnitudes[j] > magnitudes[i]) {
                float tmp = magnitudes[i];
                magnitudes[i] = magnitudes[j];
                magnitudes[j] = tmp;
            }
        }
    }

    float threshold = magnitudes[target_active - 1];

    for (uint32_t i = 0; i < dim; i++) {
        if (fabsf(vec[i]) < threshold) {
            vec[i] = 0.0f;
        }
    }

    nimcp_free(magnitudes);
    magnitudes = NULL;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int vae_hippo_bridge_default_config(vae_hippo_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_hippo_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->encode.mode = VAE_HIPPO_ENCODE_STANDARD;
    config->encode.sparsity_target = 0.8f;
    config->encode.beta_override = 0.0f;
    config->encode.include_variance = true;
    config->encode.compute_novelty = true;
    config->encode.enable_tagging = true;

    config->retrieve.mode = VAE_HIPPO_RETRIEVE_SIMILAR;
    config->retrieve.similarity_threshold = VAE_HIPPO_DEFAULT_SIMILARITY;
    config->retrieve.max_candidates = 10;
    config->retrieve.sample_on_retrieve = false;
    config->retrieve.temperature = NIMCP_TEMPERATURE_DEFAULT;
    config->retrieve.return_confidence = true;

    config->pattern.separation_ratio = HIPPO_PATTERN_SEPARATION_RATIO;
    config->pattern.completion_threshold = 0.8f;
    config->pattern.max_iterations = 10;
    config->pattern.use_attractor_dynamics = true;
    config->pattern.noise_level = 0.01f;

    config->what_dim = 256;
    config->where_dim = 32;
    config->when_dim = 16;

    config->sync_on_encode = true;
    config->sync_on_retrieve = false;
    config->enable_replay_integration = true;
    config->enable_place_cell_binding = true;
    config->enable_logging = false;
    config->log_latent_stats = false;

    return 0;
}

vae_hippo_bridge_t* vae_hippo_bridge_create(const vae_hippo_bridge_config_t* config) {
    vae_hippo_bridge_t* bridge = (vae_hippo_bridge_t*)nimcp_calloc(1, sizeof(vae_hippo_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_hippo_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(vae_hippo_bridge_config_t));
    } else {
        vae_hippo_bridge_default_config(&bridge->config);
    }

    bridge->state = VAE_HIPPO_STATE_DISCONNECTED;
    bridge->is_initialized = true;
    bridge->creation_time_us = get_time_us();
    bridge->stats.creation_time_us = bridge->creation_time_us;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE-Hippocampus bridge created");
    }

    return bridge;
}

void vae_hippo_bridge_destroy(vae_hippo_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->encode_buffer) nimcp_free(bridge->encode_buffer);
    if (bridge->decode_buffer) nimcp_free(bridge->decode_buffer);
    if (bridge->latent_buffer) nimcp_free(bridge->latent_buffer);
    if (bridge->variance_buffer) nimcp_free(bridge->variance_buffer);
    if (bridge->latent_mean_baseline) nimcp_free(bridge->latent_mean_baseline);
    if (bridge->latent_var_baseline) nimcp_free(bridge->latent_var_baseline);

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE-Hippocampus bridge destroyed");
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_hippo_bridge_connect_vae(vae_hippo_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_HIPPO_NULL;

    bridge->vae = vae;
    bridge->vae_input_dim = vae_get_input_dim(vae);
    bridge->vae_latent_dim = vae_get_latent_dim(vae);
    bridge->vae_output_dim = vae_get_output_dim(vae);

    bridge->encode_buffer = (float*)nimcp_calloc(bridge->vae_input_dim, sizeof(float));
    bridge->decode_buffer = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    bridge->latent_buffer = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    bridge->variance_buffer = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    bridge->latent_mean_baseline = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    bridge->latent_var_baseline = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));

    if (!bridge->encode_buffer || !bridge->decode_buffer ||
        !bridge->latent_buffer || !bridge->variance_buffer ||
        !bridge->latent_mean_baseline || !bridge->latent_var_baseline) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to allocate working buffers");
        /* Free any partially allocated buffers */
        if (bridge->encode_buffer) { nimcp_free(bridge->encode_buffer); bridge->encode_buffer = NULL; }
        if (bridge->decode_buffer) { nimcp_free(bridge->decode_buffer); bridge->decode_buffer = NULL; }
        if (bridge->latent_buffer) { nimcp_free(bridge->latent_buffer); bridge->latent_buffer = NULL; }
        if (bridge->variance_buffer) { nimcp_free(bridge->variance_buffer); bridge->variance_buffer = NULL; }
        if (bridge->latent_mean_baseline) { nimcp_free(bridge->latent_mean_baseline); bridge->latent_mean_baseline = NULL; }
        if (bridge->latent_var_baseline) { nimcp_free(bridge->latent_var_baseline); bridge->latent_var_baseline = NULL; }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        bridge->latent_var_baseline[i] = 1.0f;
    }

    if (bridge->hippocampus) {
        bridge->state = VAE_HIPPO_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "VAE connected: input=%u, latent=%u, output=%u",
                       bridge->vae_input_dim, bridge->vae_latent_dim, bridge->vae_output_dim);
    }

    return 0;
}

int vae_hippo_bridge_connect_hippo(vae_hippo_bridge_t* bridge, void* hippocampus) {
    if (!bridge || !hippocampus) return NIMCP_ERROR_VAE_HIPPO_NULL;

    bridge->hippocampus = hippocampus;

    if (bridge->vae) {
        bridge->state = VAE_HIPPO_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Hippocampus connected");
    }

    return 0;
}

int vae_hippo_bridge_disconnect(vae_hippo_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_HIPPO_NULL;

    bridge->vae = NULL;
    bridge->hippocampus = NULL;
    bridge->state = VAE_HIPPO_STATE_DISCONNECTED;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Bridge disconnected");
    }

    return 0;
}

bool vae_hippo_bridge_is_connected(const vae_hippo_bridge_t* bridge) {
    return bridge && bridge->vae && bridge->hippocampus &&
           bridge->state != VAE_HIPPO_STATE_DISCONNECTED &&
           bridge->state != VAE_HIPPO_STATE_ERROR;
}

/* ============================================================================
 * Memory Encoding Implementation
 * ============================================================================ */

int vae_hippo_encode_episode(vae_hippo_bridge_t* bridge,
                              const float* input, uint32_t input_dim,
                              const float* where, uint32_t where_dim,
                              const float* when, uint32_t when_dim,
                              float emotional_valence,
                              float emotional_arousal,
                              vae_hippo_encode_result_t* result) {
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_HIPPO_STATE_ENCODING;

    memset(result, 0, sizeof(*result));

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(input_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* z_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor || !z_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        if (z_tensor) nimcp_tensor_destroy(z_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    uint32_t copy_dim = (input_dim < bridge->vae_input_dim) ? input_dim : bridge->vae_input_dim;
    memcpy(input_data, input, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "VAE encoding failed: %d", ret);
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        nimcp_tensor_destroy(z_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        bridge->stats.failed_operations++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    ret = vae_sample(bridge->vae, mu_tensor, logvar_tensor, z_tensor);
    if (ret != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "VAE sampling failed: %d", ret);
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        nimcp_tensor_destroy(z_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);
    float* logvar_data = (float*)nimcp_tensor_data(logvar_tensor);
    float* z_data = (float*)nimcp_tensor_data(z_tensor);

    memcpy(bridge->latent_buffer, z_data, bridge->vae_latent_dim * sizeof(float));
    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        bridge->variance_buffer[i] = expf(logvar_data[i]);
    }

    if (bridge->config.encode.mode == VAE_HIPPO_ENCODE_SPARSE) {
        apply_sparsification(bridge->latent_buffer, bridge->vae_latent_dim,
                             bridge->config.encode.sparsity_target);
        result->pattern_separated = true;
    }

    float novelty_score = 0.0f;
    if (bridge->config.encode.compute_novelty && bridge->baseline_samples > 0) {
        float dist = nimcp_vector_euclidean_distance(bridge->latent_buffer,
                                                     bridge->latent_mean_baseline,
                                                     bridge->vae_latent_dim);
        novelty_score = 1.0f - expf(-dist * 0.5f);
    }

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
    uint32_t episode_id = 0;

    ret = hippo_encode_episode(hippo,
                               bridge->latent_buffer, bridge->vae_latent_dim,
                               where, where_dim,
                               when, when_dim,
                               emotional_valence,
                               emotional_arousal,
                               &episode_id);
    if (ret != HIPPO_ERROR_NONE) {
        NIMCP_LOG_ERROR(LOG_TAG, "Hippocampus encoding failed: %d", ret);
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        nimcp_tensor_destroy(z_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        bridge->stats.failed_operations++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    result->episode_id = episode_id;
    result->novelty_score = novelty_score;
    result->encoding_strength = 1.0f - novelty_score * 0.3f;
    result->latent_dim = bridge->vae_latent_dim;
    result->active_dimensions = count_active_dimensions(bridge->variance_buffer,
                                                        bridge->vae_latent_dim, 0.01f);

    result->latent_code = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
    if (result->latent_code) {
        memcpy(result->latent_code, bridge->latent_buffer,
               bridge->vae_latent_dim * sizeof(float));
    }

    if (bridge->config.encode.include_variance) {
        result->latent_variance = (float*)nimcp_calloc(bridge->vae_latent_dim, sizeof(float));
        if (result->latent_variance) {
            memcpy(result->latent_variance, bridge->variance_buffer,
                   bridge->vae_latent_dim * sizeof(float));
        }
    }

    vae_hippo_update_baseline(bridge, input, input_dim);

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);
    nimcp_tensor_destroy(z_tensor);

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_encodes++;
    bridge->stats.successful_encodes++;
    bridge->stats.avg_encode_latency_us = bridge->stats.avg_encode_latency_us * 0.9f + (float)elapsed * 0.1f;
    if (isfinite(result->encoding_strength)) {
        bridge->stats.avg_encoding_strength = bridge->stats.avg_encoding_strength * 0.9f +
                                              result->encoding_strength * 0.1f;
    }
    if (isfinite(novelty_score)) {
        bridge->stats.avg_novelty_score = bridge->stats.avg_novelty_score * 0.9f +
                                          novelty_score * 0.1f;
    }
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_HIPPO_STATE_CONNECTED;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_DEBUG(LOG_TAG, "Encoded episode %u: novelty=%.3f, strength=%.3f",
                        episode_id, novelty_score, result->encoding_strength);
    }

    return 0;
}

int vae_hippo_encode_tensor(vae_hippo_bridge_t* bridge,
                             const nimcp_tensor_t* input,
                             const nimcp_tensor_t* context,
                             vae_hippo_encode_result_t* result) {
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;

    float* input_data = (float*)nimcp_tensor_data((nimcp_tensor_t*)input);
    uint32_t input_dim = nimcp_tensor_size(input);

    float* context_data = NULL;
    uint32_t context_dim = 0;
    if (context) {
        context_data = (float*)nimcp_tensor_data((nimcp_tensor_t*)context);
        context_dim = nimcp_tensor_size(context);
    }

    return vae_hippo_encode_episode(bridge,
                                     input_data, input_dim,
                                     context_data, context_dim,
                                     NULL, 0,
                                     0.0f, 0.5f,
                                     result);
}

int vae_hippo_encode_with_config(vae_hippo_bridge_t* bridge,
                                  const float* input, uint32_t input_dim,
                                  const vae_hippo_encode_config_t* encode_config,
                                  vae_hippo_encode_result_t* result) {
    if (!bridge || !input || !encode_config || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;

    vae_hippo_encode_config_t saved = bridge->config.encode;
    memcpy(&bridge->config.encode, encode_config, sizeof(vae_hippo_encode_config_t));

    int ret = vae_hippo_encode_episode(bridge, input, input_dim,
                                        NULL, 0, NULL, 0,
                                        0.0f, 0.5f, result);

    memcpy(&bridge->config.encode, &saved, sizeof(vae_hippo_encode_config_t));
    return ret;
}

int vae_hippo_encode_batch(vae_hippo_bridge_t* bridge,
                            const float* inputs, uint32_t num_inputs,
                            uint32_t input_dim,
                            vae_hippo_encode_result_t* results) {
    if (!bridge || !inputs || !results) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (num_inputs == 0) return 0;

    for (uint32_t i = 0; i < num_inputs; i++) {
        int ret = vae_hippo_encode_episode(bridge,
                                            inputs + i * input_dim, input_dim,
                                            NULL, 0, NULL, 0,
                                            0.0f, 0.5f,
                                            &results[i]);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/* ============================================================================
 * Memory Retrieval Implementation
 * ============================================================================ */

int vae_hippo_retrieve(vae_hippo_bridge_t* bridge,
                        const float* cue, uint32_t cue_dim,
                        vae_hippo_retrieve_result_t* result) {
    if (!bridge || !cue || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_HIPPO_STATE_RETRIEVING;

    memset(result, 0, sizeof(*result));

    nimcp_tensor_t* cue_tensor = nimcp_tensor_create_1d(cue_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!cue_tensor || !mu_tensor || !logvar_tensor) {
        if (cue_tensor) nimcp_tensor_destroy(cue_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* cue_data = (float*)nimcp_tensor_data(cue_tensor);
    uint32_t copy_dim = (cue_dim < bridge->vae_input_dim) ? cue_dim : bridge->vae_input_dim;
    memcpy(cue_data, cue, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, cue_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        bridge->stats.failed_operations++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    float* cue_latent = (float*)nimcp_tensor_data(mu_tensor);

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
    uint32_t episode_id = 0;
    float match_confidence = 0.0f;

    ret = hippo_retrieve_episode(hippo, cue_latent, bridge->vae_latent_dim,
                                  RETRIEVAL_CUED_RECALL,
                                  &episode_id, &match_confidence);

    if (ret != HIPPO_ERROR_NONE || match_confidence < bridge->config.retrieve.similarity_threshold) {
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_CONNECTED;
        bridge->stats.total_retrieves++;
        return 0;
    }

    const nimcp_episode_t* episode = hippo_get_episode(hippo, episode_id);
    if (!episode || !episode->what_content) {
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    nimcp_tensor_t* latent_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon_tensor = nimcp_tensor_create_1d(bridge->vae_output_dim, NIMCP_DTYPE_F32);

    if (!latent_tensor || !recon_tensor) {
        if (latent_tensor) nimcp_tensor_destroy(latent_tensor);
        if (recon_tensor) nimcp_tensor_destroy(recon_tensor);
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* latent_data = (float*)nimcp_tensor_data(latent_tensor);
    uint32_t latent_copy = (episode->what_dim < bridge->vae_latent_dim) ?
                           episode->what_dim : bridge->vae_latent_dim;
    memcpy(latent_data, episode->what_content, latent_copy * sizeof(float));

    ret = vae_decode(bridge->vae, latent_tensor, recon_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(latent_tensor);
        nimcp_tensor_destroy(recon_tensor);
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED;
    }

    result->episode_id = episode_id;
    result->similarity = match_confidence;
    result->confidence = match_confidence;
    result->reconstruction_dim = bridge->vae_output_dim;

    result->reconstruction = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (result->reconstruction) {
        float* recon_data = (float*)nimcp_tensor_data(recon_tensor);
        memcpy(result->reconstruction, recon_data, bridge->vae_output_dim * sizeof(float));

        float error = 0.0f;
        for (uint32_t i = 0; i < copy_dim && i < bridge->vae_output_dim; i++) {
            float diff = cue[i] - recon_data[i];
            error += diff * diff;
        }
        result->reconstruction_error = sqrtf(error / copy_dim);
    }

    nimcp_tensor_destroy(latent_tensor);
    nimcp_tensor_destroy(recon_tensor);
    nimcp_tensor_destroy(cue_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_retrieves++;
    bridge->stats.successful_retrieves++;
    bridge->stats.avg_retrieve_latency_us = bridge->stats.avg_retrieve_latency_us * 0.9f + (float)elapsed * 0.1f;
    if (isfinite(match_confidence)) {
        bridge->stats.avg_retrieval_similarity = bridge->stats.avg_retrieval_similarity * 0.9f +
                                                 match_confidence * 0.1f;
    }
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_HIPPO_STATE_CONNECTED;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_DEBUG(LOG_TAG, "Retrieved episode %u: similarity=%.3f",
                        episode_id, match_confidence);
    }

    return 0;
}

int vae_hippo_retrieve_by_id(vae_hippo_bridge_t* bridge,
                              uint32_t episode_id,
                              vae_hippo_retrieve_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    memset(result, 0, sizeof(*result));

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
    const nimcp_episode_t* episode = hippo_get_episode(hippo, episode_id);

    if (!episode || !episode->what_content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    nimcp_tensor_t* latent_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon_tensor = nimcp_tensor_create_1d(bridge->vae_output_dim, NIMCP_DTYPE_F32);

    if (!latent_tensor || !recon_tensor) {
        if (latent_tensor) nimcp_tensor_destroy(latent_tensor);
        if (recon_tensor) nimcp_tensor_destroy(recon_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* latent_data = (float*)nimcp_tensor_data(latent_tensor);
    uint32_t copy_dim = (episode->what_dim < bridge->vae_latent_dim) ?
                        episode->what_dim : bridge->vae_latent_dim;
    memcpy(latent_data, episode->what_content, copy_dim * sizeof(float));

    int ret = vae_decode(bridge->vae, latent_tensor, recon_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(latent_tensor);
        nimcp_tensor_destroy(recon_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED;
    }

    result->episode_id = episode_id;
    result->similarity = 1.0f;
    result->confidence = 1.0f;
    result->reconstruction_dim = bridge->vae_output_dim;

    result->reconstruction = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (result->reconstruction) {
        float* recon_data = (float*)nimcp_tensor_data(recon_tensor);
        memcpy(result->reconstruction, recon_data, bridge->vae_output_dim * sizeof(float));
    }

    nimcp_tensor_destroy(latent_tensor);
    nimcp_tensor_destroy(recon_tensor);

    return 0;
}

int vae_hippo_retrieve_with_config(vae_hippo_bridge_t* bridge,
                                    const float* cue, uint32_t cue_dim,
                                    const vae_hippo_retrieve_config_t* config,
                                    vae_hippo_retrieve_result_t* result) {
    if (!bridge || !cue || !config || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;

    vae_hippo_retrieve_config_t saved = bridge->config.retrieve;
    memcpy(&bridge->config.retrieve, config, sizeof(vae_hippo_retrieve_config_t));

    int ret = vae_hippo_retrieve(bridge, cue, cue_dim, result);

    memcpy(&bridge->config.retrieve, &saved, sizeof(vae_hippo_retrieve_config_t));
    return ret;
}

int vae_hippo_find_similar(vae_hippo_bridge_t* bridge,
                            const float* cue, uint32_t cue_dim,
                            float similarity_threshold,
                            uint32_t max_results,
                            vae_hippo_similar_result_t* result) {
    if (!bridge || !cue || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    memset(result, 0, sizeof(*result));

    if (max_results > VAE_HIPPO_MAX_SIMILAR) max_results = VAE_HIPPO_MAX_SIMILAR;

    result->episode_ids = (uint32_t*)nimcp_calloc(max_results, sizeof(uint32_t));
    result->similarities = (float*)nimcp_calloc(max_results, sizeof(float));

    if (!result->episode_ids || !result->similarities) {
        if (result->episode_ids) nimcp_free(result->episode_ids);
        if (result->similarities) nimcp_free(result->similarities);
        result->episode_ids = NULL;
        result->similarities = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    result->capacity = max_results;

    nimcp_tensor_t* cue_tensor = nimcp_tensor_create_1d(cue_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!cue_tensor || !mu_tensor || !logvar_tensor) {
        if (cue_tensor) nimcp_tensor_destroy(cue_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* cue_data = (float*)nimcp_tensor_data(cue_tensor);
    uint32_t copy_dim = (cue_dim < bridge->vae_input_dim) ? cue_dim : bridge->vae_input_dim;
    memcpy(cue_data, cue, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, cue_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(cue_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    float* cue_latent = (float*)nimcp_tensor_data(mu_tensor);

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;

    ret = hippo_find_similar_episodes(hippo, cue_latent, bridge->vae_latent_dim,
                                       result->episode_ids, result->similarities,
                                       max_results, &result->num_found);

    nimcp_tensor_destroy(cue_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    if (ret != HIPPO_ERROR_NONE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    uint32_t filtered_count = 0;
    for (uint32_t i = 0; i < result->num_found; i++) {
        if (result->similarities[i] >= similarity_threshold) {
            if (filtered_count != i) {
                result->episode_ids[filtered_count] = result->episode_ids[i];
                result->similarities[filtered_count] = result->similarities[i];
            }
            filtered_count++;
        }
    }
    result->num_found = filtered_count;

    return 0;
}

int vae_hippo_retrieve_tensor(vae_hippo_bridge_t* bridge,
                               const nimcp_tensor_t* cue,
                               nimcp_tensor_t* reconstruction) {
    if (!bridge || !cue || !reconstruction) return NIMCP_ERROR_VAE_HIPPO_NULL;

    float* cue_data = (float*)nimcp_tensor_data((nimcp_tensor_t*)cue);
    uint32_t cue_dim = nimcp_tensor_size(cue);

    vae_hippo_retrieve_result_t result;
    int ret = vae_hippo_retrieve(bridge, cue_data, cue_dim, &result);
    if (ret != 0) return ret;

    if (result.reconstruction && result.reconstruction_dim > 0) {
        float* recon_data = (float*)nimcp_tensor_data(reconstruction);
        uint32_t recon_size = nimcp_tensor_size(reconstruction);
        uint32_t copy_size = (result.reconstruction_dim < recon_size) ?
                             result.reconstruction_dim : recon_size;
        memcpy(recon_data, result.reconstruction, copy_size * sizeof(float));
    }

    vae_hippo_retrieve_result_free(&result);
    return 0;
}

/* ============================================================================
 * Pattern Operations Implementation
 * ============================================================================ */

int vae_hippo_pattern_separate(vae_hippo_bridge_t* bridge,
                                const float* input, uint32_t input_dim,
                                vae_hippo_pattern_result_t* result) {
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_HIPPO_STATE_SEPARATING;

    memset(result, 0, sizeof(*result));
    result->operation = VAE_HIPPO_PATTERN_SEPARATE;

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(input_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    uint32_t copy_dim = (input_dim < bridge->vae_input_dim) ? input_dim : bridge->vae_input_dim;
    memcpy(input_data, input, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_PATTERN_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_PATTERN_FAIL;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);

    uint32_t expanded_dim = (uint32_t)(bridge->vae_latent_dim * bridge->config.pattern.separation_ratio);
    result->output_pattern = (float*)nimcp_calloc(expanded_dim, sizeof(float));
    if (!result->output_pattern) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        uint32_t idx = (uint32_t)(i * bridge->config.pattern.separation_ratio);
        if (idx < expanded_dim) {
            result->output_pattern[idx] = mu_data[i];
        }
    }

    float target_sparsity = 0.98f;
    apply_sparsification(result->output_pattern, expanded_dim, target_sparsity);

    result->output_dim = expanded_dim;
    result->sparsity_achieved = compute_sparsity(result->output_pattern, expanded_dim, 1e-6f);
    result->confidence = result->sparsity_achieved;
    result->converged = true;
    result->iterations_used = 1;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_separations++;
    bridge->stats.avg_pattern_latency_us = bridge->stats.avg_pattern_latency_us * 0.9f + (float)elapsed * 0.1f;
    if (isfinite(result->sparsity_achieved)) {
        bridge->stats.avg_latent_sparsity = bridge->stats.avg_latent_sparsity * 0.9f +
                                            result->sparsity_achieved * 0.1f;
    }
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_HIPPO_STATE_CONNECTED;

    return 0;
}

int vae_hippo_pattern_complete(vae_hippo_bridge_t* bridge,
                                const float* partial_cue, uint32_t cue_dim,
                                const float* mask,
                                vae_hippo_pattern_result_t* result) {
    if (!bridge || !partial_cue || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_HIPPO_STATE_COMPLETING;

    memset(result, 0, sizeof(*result));
    result->operation = VAE_HIPPO_PATTERN_COMPLETE;

    result->output_pattern = (float*)nimcp_calloc(bridge->vae_output_dim, sizeof(float));
    if (!result->output_pattern) {
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(cue_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon_tensor = nimcp_tensor_create_1d(bridge->vae_output_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !recon_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (recon_tensor) nimcp_tensor_destroy(recon_tensor);
        nimcp_free(result->output_pattern);
        result->output_pattern = NULL;
        bridge->state = VAE_HIPPO_STATE_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* current = (float*)nimcp_tensor_data(input_tensor);
    memcpy(current, partial_cue, cue_dim * sizeof(float));

    float prev_error = FLT_MAX;
    uint32_t iter = 0;

    for (iter = 0; iter < bridge->config.pattern.max_iterations; iter++) {
        int ret = vae_reconstruct(bridge->vae, input_tensor, recon_tensor);
        if (ret != 0) {
            nimcp_tensor_destroy(input_tensor);
            nimcp_tensor_destroy(recon_tensor);
            nimcp_free(result->output_pattern);
            result->output_pattern = NULL;
            bridge->state = VAE_HIPPO_STATE_ERROR;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_PATTERN_FAIL, "vae_hippocampus_bridge: error condition");
            return NIMCP_ERROR_VAE_HIPPO_PATTERN_FAIL;
        }

        float* recon_data = (float*)nimcp_tensor_data(recon_tensor);

        float error = 0.0f;
        for (uint32_t i = 0; i < cue_dim && i < bridge->vae_output_dim; i++) {
            if (!mask || mask[i] > 0.5f) {
                float diff = partial_cue[i] - recon_data[i];
                error += diff * diff;
            }
        }
        error = sqrtf(error / cue_dim);

        if (fabsf(error - prev_error) < 0.001f ||
            error < bridge->config.pattern.completion_threshold) {
            result->converged = true;
            break;
        }
        prev_error = error;

        for (uint32_t i = 0; i < cue_dim && i < bridge->vae_input_dim; i++) {
            if (mask && mask[i] > 0.5f) {
                current[i] = partial_cue[i];
            } else {
                current[i] = recon_data[i];
            }
        }
    }

    float* final_recon = (float*)nimcp_tensor_data(recon_tensor);
    memcpy(result->output_pattern, final_recon, bridge->vae_output_dim * sizeof(float));

    result->output_dim = bridge->vae_output_dim;
    result->iterations_used = iter + 1;
    result->confidence = result->converged ? 0.9f : (float)iter / bridge->config.pattern.max_iterations;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(recon_tensor);

    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.total_completions++;
    bridge->stats.avg_pattern_latency_us = bridge->stats.avg_pattern_latency_us * 0.9f + (float)elapsed * 0.1f;
    if (isfinite(result->confidence)) {
        bridge->stats.avg_completion_confidence = bridge->stats.avg_completion_confidence * 0.9f +
                                                  result->confidence * 0.1f;
    }
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_HIPPO_STATE_CONNECTED;

    return 0;
}

int vae_hippo_pattern_bind(vae_hippo_bridge_t* bridge,
                            const float* what, uint32_t what_dim,
                            const float* where, uint32_t where_dim,
                            const float* when, uint32_t when_dim,
                            vae_hippo_pattern_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!what && !where && !when) return NIMCP_ERROR_VAE_HIPPO_NULL;

    memset(result, 0, sizeof(*result));
    result->operation = VAE_HIPPO_PATTERN_BIND;

    uint32_t total_dim = (what ? what_dim : 0) + (where ? where_dim : 0) + (when ? when_dim : 0);
    result->output_pattern = (float*)nimcp_calloc(total_dim, sizeof(float));
    if (!result->output_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    uint32_t offset = 0;
    if (what && what_dim > 0) {
        memcpy(result->output_pattern + offset, what, what_dim * sizeof(float));
        offset += what_dim;
    }
    if (where && where_dim > 0) {
        memcpy(result->output_pattern + offset, where, where_dim * sizeof(float));
        offset += where_dim;
    }
    if (when && when_dim > 0) {
        memcpy(result->output_pattern + offset, when, when_dim * sizeof(float));
        offset += when_dim;
    }

    result->output_dim = total_dim;
    result->confidence = 1.0f;
    result->converged = true;
    result->iterations_used = 1;

    return 0;
}

/* ============================================================================
 * Generative Memory Implementation
 * ============================================================================ */

int vae_hippo_generate_memories(vae_hippo_bridge_t* bridge,
                                 uint32_t num_samples,
                                 float* samples,
                                 uint32_t sample_dim) {
    if (!bridge || !samples) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    nimcp_tensor_t* output = nimcp_tensor_create_2d(num_samples, bridge->vae_output_dim,
                                                    NIMCP_DTYPE_F32);
    if (!output) return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;

    int ret = vae_generate(bridge->vae, num_samples, output);
    if (ret != 0) {
        nimcp_tensor_destroy(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED;
    }

    float* output_data = (float*)nimcp_tensor_data(output);
    uint32_t copy_dim = (sample_dim < bridge->vae_output_dim) ? sample_dim : bridge->vae_output_dim;

    for (uint32_t i = 0; i < num_samples; i++) {
        memcpy(samples + i * sample_dim, output_data + i * bridge->vae_output_dim,
               copy_dim * sizeof(float));
    }

    nimcp_tensor_destroy(output);
    return 0;
}

int vae_hippo_interpolate_episodes(vae_hippo_bridge_t* bridge,
                                    uint32_t episode_id_1,
                                    uint32_t episode_id_2,
                                    uint32_t num_steps,
                                    float* interpolations,
                                    uint32_t step_dim) {
    if (!bridge || !interpolations) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;

    const nimcp_episode_t* ep1 = hippo_get_episode(hippo, episode_id_1);
    const nimcp_episode_t* ep2 = hippo_get_episode(hippo, episode_id_2);

    if (!ep1 || !ep2 || !ep1->what_content || !ep2->what_content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    nimcp_tensor_t* z1 = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* z2 = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* interp = nimcp_tensor_create_2d(num_steps, bridge->vae_output_dim,
                                                    NIMCP_DTYPE_F32);

    if (!z1 || !z2 || !interp) {
        if (z1) nimcp_tensor_destroy(z1);
        if (z2) nimcp_tensor_destroy(z2);
        if (interp) nimcp_tensor_destroy(interp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* z1_data = (float*)nimcp_tensor_data(z1);
    float* z2_data = (float*)nimcp_tensor_data(z2);

    uint32_t copy1 = (ep1->what_dim < bridge->vae_latent_dim) ? ep1->what_dim : bridge->vae_latent_dim;
    uint32_t copy2 = (ep2->what_dim < bridge->vae_latent_dim) ? ep2->what_dim : bridge->vae_latent_dim;

    memcpy(z1_data, ep1->what_content, copy1 * sizeof(float));
    memcpy(z2_data, ep2->what_content, copy2 * sizeof(float));

    int ret = vae_interpolate(bridge->vae, z1, z2, num_steps, interp);
    if (ret != 0) {
        nimcp_tensor_destroy(z1);
        nimcp_tensor_destroy(z2);
        nimcp_tensor_destroy(interp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED;
    }

    float* interp_data = (float*)nimcp_tensor_data(interp);
    uint32_t copy_dim = (step_dim < bridge->vae_output_dim) ? step_dim : bridge->vae_output_dim;

    for (uint32_t i = 0; i < num_steps; i++) {
        memcpy(interpolations + i * step_dim, interp_data + i * bridge->vae_output_dim,
               copy_dim * sizeof(float));
    }

    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(z2);
    nimcp_tensor_destroy(interp);

    return 0;
}

int vae_hippo_generate_variation(vae_hippo_bridge_t* bridge,
                                  uint32_t episode_id,
                                  float variation_strength,
                                  float* variation,
                                  uint32_t output_dim) {
    if (!bridge || !variation) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
    const nimcp_episode_t* episode = hippo_get_episode(hippo, episode_id);

    if (!episode || !episode->what_content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    nimcp_tensor_t* z_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon_tensor = nimcp_tensor_create_1d(bridge->vae_output_dim, NIMCP_DTYPE_F32);

    if (!z_tensor || !recon_tensor) {
        if (z_tensor) nimcp_tensor_destroy(z_tensor);
        if (recon_tensor) nimcp_tensor_destroy(recon_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* z_data = (float*)nimcp_tensor_data(z_tensor);
    uint32_t copy_dim = (episode->what_dim < bridge->vae_latent_dim) ?
                        episode->what_dim : bridge->vae_latent_dim;
    memcpy(z_data, episode->what_content, copy_dim * sizeof(float));

    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        float noise = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 2.0f * variation_strength;
        z_data[i] += noise;
    }

    int ret = vae_decode(bridge->vae, z_tensor, recon_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(z_tensor);
        nimcp_tensor_destroy(recon_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED;
    }

    float* recon_data = (float*)nimcp_tensor_data(recon_tensor);
    uint32_t out_copy = (output_dim < bridge->vae_output_dim) ? output_dim : bridge->vae_output_dim;
    memcpy(variation, recon_data, out_copy * sizeof(float));

    nimcp_tensor_destroy(z_tensor);
    nimcp_tensor_destroy(recon_tensor);

    return 0;
}

/* ============================================================================
 * Novelty and Familiarity Implementation
 * ============================================================================ */

int vae_hippo_compute_novelty(vae_hippo_bridge_t* bridge,
                               const float* input, uint32_t input_dim,
                               float* novelty_score) {
    if (!bridge || !input || !novelty_score) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    *novelty_score = 0.5f;

    if (bridge->baseline_samples == 0) {
        *novelty_score = 1.0f;
        return 0;
    }

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(input_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    uint32_t copy_dim = (input_dim < bridge->vae_input_dim) ? input_dim : bridge->vae_input_dim;
    memcpy(input_data, input, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);

    float distance = nimcp_vector_euclidean_distance(mu_data, bridge->latent_mean_baseline,
                                                     bridge->vae_latent_dim);

    *novelty_score = 1.0f - expf(-distance * 0.5f);
    if (*novelty_score > 1.0f) *novelty_score = 1.0f;
    if (*novelty_score < 0.0f) *novelty_score = 0.0f;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    return 0;
}

int vae_hippo_compute_familiarity(vae_hippo_bridge_t* bridge,
                                   const float* input, uint32_t input_dim,
                                   float* familiarity_score) {
    if (!bridge || !input || !familiarity_score) return NIMCP_ERROR_VAE_HIPPO_NULL;

    float novelty = 0.0f;
    int ret = vae_hippo_compute_novelty(bridge, input, input_dim, &novelty);
    if (ret != 0) return ret;

    *familiarity_score = 1.0f - novelty;
    return 0;
}

int vae_hippo_update_baseline(vae_hippo_bridge_t* bridge,
                               const float* input, uint32_t input_dim) {
    if (!bridge || !input) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!bridge->vae) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    nimcp_tensor_t* input_tensor = nimcp_tensor_create_1d(input_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);
    nimcp_tensor_t* logvar_tensor = nimcp_tensor_create_1d(bridge->vae_latent_dim, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !logvar_tensor) {
        if (input_tensor) nimcp_tensor_destroy(input_tensor);
        if (mu_tensor) nimcp_tensor_destroy(mu_tensor);
        if (logvar_tensor) nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_NO_MEMORY, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_NO_MEMORY;
    }

    float* input_data = (float*)nimcp_tensor_data(input_tensor);
    uint32_t copy_dim = (input_dim < bridge->vae_input_dim) ? input_dim : bridge->vae_input_dim;
    memcpy(input_data, input, copy_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, logvar_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(logvar_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED;
    }

    float* mu_data = (float*)nimcp_tensor_data(mu_tensor);
    float* logvar_data = (float*)nimcp_tensor_data(logvar_tensor);

    float alpha = (bridge->baseline_samples == 0) ? 1.0f : BASELINE_EMA_ALPHA;

    for (uint32_t i = 0; i < bridge->vae_latent_dim; i++) {
        bridge->latent_mean_baseline[i] = bridge->latent_mean_baseline[i] * (1.0f - alpha) +
                                          mu_data[i] * alpha;
        bridge->latent_var_baseline[i] = bridge->latent_var_baseline[i] * (1.0f - alpha) +
                                         expf(logvar_data[i]) * alpha;
    }

    bridge->baseline_samples++;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(logvar_tensor);

    return 0;
}

/* ============================================================================
 * Replay Integration Implementation
 * ============================================================================ */

int vae_hippo_get_replay_content(vae_hippo_bridge_t* bridge,
                                  uint32_t episode_id,
                                  float* content,
                                  uint32_t max_dim,
                                  uint32_t* actual_dim) {
    if (!bridge || !content || !actual_dim) return NIMCP_ERROR_VAE_HIPPO_NULL;

    vae_hippo_retrieve_result_t result;
    int ret = vae_hippo_retrieve_by_id(bridge, episode_id, &result);
    if (ret != 0) return ret;

    if (result.reconstruction && result.reconstruction_dim > 0) {
        uint32_t copy_dim = (result.reconstruction_dim < max_dim) ?
                            result.reconstruction_dim : max_dim;
        memcpy(content, result.reconstruction, copy_dim * sizeof(float));
        *actual_dim = copy_dim;
    } else {
        *actual_dim = 0;
    }

    vae_hippo_retrieve_result_free(&result);
    return 0;
}

int vae_hippo_process_replay(vae_hippo_bridge_t* bridge,
                              const uint32_t* episode_ids,
                              uint32_t num_episodes,
                              float compression_factor) {
    if (!bridge || !episode_ids) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    (void)compression_factor;

    for (uint32_t i = 0; i < num_episodes; i++) {
        nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
        hippo_strengthen_episode(hippo, episode_ids[i], 0.1f);
    }

    return 0;
}

/* ============================================================================
 * Synchronization Implementation
 * ============================================================================ */

int vae_hippo_sync(vae_hippo_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    bridge->stats.last_operation_us = get_time_us();

    if (bridge->config.enable_logging) {
        NIMCP_LOG_DEBUG(LOG_TAG, "Bridge synchronized");
    }

    return 0;
}

int vae_hippo_export_for_training(vae_hippo_bridge_t* bridge,
                                   float* patterns,
                                   uint32_t max_patterns,
                                   uint32_t pattern_dim,
                                   uint32_t* num_exported) {
    if (!bridge || !patterns || !num_exported) return NIMCP_ERROR_VAE_HIPPO_NULL;
    if (!vae_hippo_bridge_is_connected(bridge)) return NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED;

    *num_exported = 0;

    nimcp_hippocampus_t* hippo = (nimcp_hippocampus_t*)bridge->hippocampus;
    uint32_t episode_ids[256];
    uint32_t num_episodes = 0;

    int ret = hippo_get_recent_episodes(hippo, episode_ids, max_patterns, &num_episodes);
    if (ret != HIPPO_ERROR_NONE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL, "vae_hippocampus_bridge: error condition");
        return NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL;
    }

    for (uint32_t i = 0; i < num_episodes && i < max_patterns; i++) {
        vae_hippo_retrieve_result_t result;
        ret = vae_hippo_retrieve_by_id(bridge, episode_ids[i], &result);
        if (ret == 0 && result.reconstruction) {
            uint32_t copy_dim = (result.reconstruction_dim < pattern_dim) ?
                                result.reconstruction_dim : pattern_dim;
            memcpy(patterns + (*num_exported) * pattern_dim, result.reconstruction,
                   copy_dim * sizeof(float));
            (*num_exported)++;
            vae_hippo_retrieve_result_free(&result);
        }
    }

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

vae_hippo_bridge_state_t vae_hippo_bridge_get_state(const vae_hippo_bridge_t* bridge) {
    if (!bridge) return VAE_HIPPO_STATE_ERROR;
    return bridge->state;
}

int vae_hippo_bridge_get_stats(const vae_hippo_bridge_t* bridge,
                                vae_hippo_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_HIPPO_NULL;
    memcpy(stats, &bridge->stats, sizeof(vae_hippo_bridge_stats_t));
    return 0;
}

uint32_t vae_hippo_get_latent_dim(const vae_hippo_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->vae_latent_dim;
}

/* ============================================================================
 * Result Management Implementation
 * ============================================================================ */

void vae_hippo_encode_result_free(vae_hippo_encode_result_t* result) {
    if (!result) return;
    if (result->latent_code) nimcp_free(result->latent_code);
    if (result->latent_variance) nimcp_free(result->latent_variance);
    memset(result, 0, sizeof(*result));
}

void vae_hippo_retrieve_result_free(vae_hippo_retrieve_result_t* result) {
    if (!result) return;
    if (result->reconstruction) nimcp_free(result->reconstruction);
    memset(result, 0, sizeof(*result));
}

void vae_hippo_similar_result_free(vae_hippo_similar_result_t* result) {
    if (!result) return;
    if (result->episode_ids) nimcp_free(result->episode_ids);
    if (result->similarities) nimcp_free(result->similarities);
    memset(result, 0, sizeof(*result));
}

void vae_hippo_pattern_result_free(vae_hippo_pattern_result_t* result) {
    if (!result) return;
    if (result->output_pattern) nimcp_free(result->output_pattern);
    memset(result, 0, sizeof(*result));
}
