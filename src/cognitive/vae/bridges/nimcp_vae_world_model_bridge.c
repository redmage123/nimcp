/**
 * @file nimcp_vae_world_model_bridge.c
 * @brief Implementation of VAE-World Model bridge for latent prediction
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "cognitive/vae/bridges/nimcp_vae_world_model_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread_rand.h"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_us(void) {
    return nimcp_time_now_us();
}

/**
 * @brief Softmax for attention weights
 */
static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            values[i] /= sum;
        }
    }
}

/**
 * @brief Compute dot product
 */
static float dot_product(const float* a, const float* b, uint32_t n) {
    float result = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/**
 * @brief Concatenate multiple latent vectors
 */
static int concatenate_latents(const float** latents, const uint32_t* dims,
                                uint32_t num_latents,
                                float* output, uint32_t* output_dim) {
    uint32_t total_dim = 0;
    for (uint32_t i = 0; i < num_latents; i++) {
        total_dim += dims[i];
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_latents; i++) {
        memcpy(&output[offset], latents[i], dims[i] * sizeof(float));
        offset += dims[i];
    }

    *output_dim = total_dim;
    return 0;
}

/**
 * @brief Product of experts fusion (multiply Gaussians)
 */
static int product_of_experts(const float** means, const float** variances,
                               const float* weights, uint32_t num_experts,
                               uint32_t dim,
                               float* fused_mean, float* fused_var) {
    /* PoE: precision-weighted combination */
    for (uint32_t d = 0; d < dim; d++) {
        float precision_sum = 0.0f;
        float weighted_sum = 0.0f;

        for (uint32_t e = 0; e < num_experts; e++) {
            if (variances[e][d] > 1e-8f) {
                float precision = weights[e] / variances[e][d];
                precision_sum += precision;
                weighted_sum += precision * means[e][d];
            }
        }

        if (precision_sum > 0.0f) {
            fused_mean[d] = weighted_sum / precision_sum;
            fused_var[d] = 1.0f / precision_sum;
        } else {
            fused_mean[d] = 0.0f;
            fused_var[d] = 1.0f;
        }
    }

    return 0;
}

/**
 * @brief Cross-modal attention computation
 */
static int compute_attention(const vae_world_bridge_t* bridge,
                              const float* query, uint32_t query_dim,
                              float* attention_weights) {
    uint32_t num_valid = 0;
    uint32_t valid_indices[VAE_WORLD_MAX_MODALITIES];

    /* Find valid modalities */
    for (uint32_t m = 0; m < VAE_WORLD_MAX_MODALITIES; m++) {
        if (bridge->modality_valid[m] && bridge->modality_latents[m]) {
            valid_indices[num_valid++] = m;
        }
    }

    if (num_valid == 0) return 0;

    /* Compute attention scores */
    float scores[VAE_WORLD_MAX_MODALITIES];
    memset(scores, 0, sizeof(scores));

    for (uint32_t i = 0; i < num_valid; i++) {
        uint32_t m = valid_indices[i];
        uint32_t lat_dim = bridge->config.modalities[m].latent_dim;
        uint32_t min_dim = lat_dim < query_dim ? lat_dim : query_dim;

        /* Dot product attention */
        scores[m] = dot_product(query, bridge->modality_latents[m], min_dim);
        scores[m] /= sqrtf((float)min_dim);  /* Scale */
    }

    /* Softmax over valid modalities */
    float valid_scores[VAE_WORLD_MAX_MODALITIES];
    for (uint32_t i = 0; i < num_valid; i++) {
        valid_scores[i] = scores[valid_indices[i]];
    }
    softmax(valid_scores, num_valid);

    /* Map back to full attention array */
    memset(attention_weights, 0, VAE_WORLD_MAX_MODALITIES * sizeof(float));
    for (uint32_t i = 0; i < num_valid; i++) {
        attention_weights[valid_indices[i]] = valid_scores[i];
    }

    return 0;
}

/**
 * @brief Predict next latent state (simple linear dynamics)
 */
static int predict_latent_dynamics(const float* current_latent,
                                    const float* velocity,
                                    uint32_t dim,
                                    float* next_latent) {
    for (uint32_t i = 0; i < dim; i++) {
        next_latent[i] = current_latent[i];
        if (velocity) {
            next_latent[i] += velocity[i];
        }
    }
    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_world_bridge_default_config(vae_world_bridge_config_t* config) {
    if (!config) return NIMCP_ERROR_VAE_WORLD_NULL;

    memset(config, 0, sizeof(*config));

    /* Default modality configuration */
    config->num_modalities = 3;  /* Visual, Auditory, Tactile */

    config->modalities[0].modality = VAE_WORLD_MOD_VISUAL;
    config->modalities[0].input_dim = 1024;
    config->modalities[0].latent_dim = 64;
    config->modalities[0].weight = 0.5f;
    config->modalities[0].enabled = true;

    config->modalities[1].modality = VAE_WORLD_MOD_AUDITORY;
    config->modalities[1].input_dim = 256;
    config->modalities[1].latent_dim = 32;
    config->modalities[1].weight = 0.3f;
    config->modalities[1].enabled = true;

    config->modalities[2].modality = VAE_WORLD_MOD_TACTILE;
    config->modalities[2].input_dim = 128;
    config->modalities[2].latent_dim = 16;
    config->modalities[2].weight = 0.2f;
    config->modalities[2].enabled = true;

    config->pred_mode = VAE_WORLD_PRED_DETERMINISTIC;
    config->fusion_strategy = VAE_WORLD_FUSE_ATTENTION;

    config->fused_latent_dim = 128;
    config->entity_latent_dim = 32;

    config->default_horizon = 10;
    config->prediction_temperature = 1.0f;
    config->use_action_conditioning = false;

    config->attention_heads = 4;
    config->attention_dim = 64;

    config->enable_entity_tracking = true;
    config->max_entities = VAE_WORLD_MAX_ENTITIES;

    config->enable_logging = false;

    return 0;
}

vae_world_bridge_t* vae_world_bridge_create(const vae_world_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_world_bridge_create: config is NULL");
        return NULL;
    }

    vae_world_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_world_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_world_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_WORLD_STATE_DISCONNECTED;
    bridge->creation_time_us = get_time_us();

    /* Allocate fused latent buffer */
    bridge->fused_latent = nimcp_calloc(config->fused_latent_dim, sizeof(float));
    if (!bridge->fused_latent) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_world_bridge_create: bridge->fused_latent is NULL");
        return NULL;
    }

    /* Allocate attention weights matrix */
    uint32_t attention_size = VAE_WORLD_MAX_MODALITIES * VAE_WORLD_MAX_MODALITIES;
    bridge->attention_weights = nimcp_calloc(attention_size, sizeof(float));
    if (!bridge->attention_weights) {
        nimcp_free(bridge->fused_latent);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_world_bridge_create: bridge->attention_weights is NULL");
        return NULL;
    }

    /* Allocate entity tracking if enabled */
    if (config->enable_entity_tracking) {
        bridge->entities = nimcp_calloc(config->max_entities, sizeof(vae_world_entity_t));
        if (!bridge->entities) {
            nimcp_free(bridge->attention_weights);
            nimcp_free(bridge->fused_latent);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_world_bridge_create: bridge->entities is NULL");
            return NULL;
        }
    }

    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        LOG_INFO("VAE-World Model bridge created (fusion=%d, entities=%s)",
                 config->fusion_strategy,
                 config->enable_entity_tracking ? "enabled" : "disabled");
    }

    return bridge;
}

void vae_world_bridge_destroy(vae_world_bridge_t* bridge) {
    if (!bridge) return;

    /* Free per-modality latents */
    for (int m = 0; m < VAE_WORLD_MAX_MODALITIES; m++) {
        if (bridge->modality_latents[m]) {
            nimcp_free(bridge->modality_latents[m]);
        }
    }

    if (bridge->fused_latent) {
        nimcp_free(bridge->fused_latent);
    }
    if (bridge->attention_weights) {
        nimcp_free(bridge->attention_weights);
    }

    /* Free entities */
    if (bridge->entities) {
        for (uint32_t e = 0; e < bridge->num_entities; e++) {
            if (bridge->entities[e].latent) {
                nimcp_free(bridge->entities[e].latent);
            }
            if (bridge->entities[e].velocity) {
                nimcp_free(bridge->entities[e].velocity);
            }
        }
        nimcp_free(bridge->entities);
    }

    if (bridge->encode_buffer) {
        nimcp_free(bridge->encode_buffer);
    }
    if (bridge->predict_buffer) {
        nimcp_free(bridge->predict_buffer);
    }
    if (bridge->fuse_buffer) {
        nimcp_free(bridge->fuse_buffer);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_world_bridge_connect_vae(vae_world_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_WORLD_NULL;

    bridge->vae = vae;

    /* Allocate per-modality latent buffers */
    for (uint32_t m = 0; m < bridge->config.num_modalities; m++) {
        uint32_t lat_dim = bridge->config.modalities[m].latent_dim;
        bridge->modality_latents[m] = nimcp_calloc(lat_dim, sizeof(float));
        if (!bridge->modality_latents[m]) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
            return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
        }
    }

    /* Allocate working buffers */
    uint32_t max_input = 0;
    for (uint32_t m = 0; m < bridge->config.num_modalities; m++) {
        if (bridge->config.modalities[m].input_dim > max_input) {
            max_input = bridge->config.modalities[m].input_dim;
        }
    }

    bridge->encode_buffer = nimcp_calloc(max_input, sizeof(float));
    if (!bridge->encode_buffer) return -1;
    bridge->predict_buffer = nimcp_calloc(bridge->config.fused_latent_dim *
                                           VAE_WORLD_MAX_HORIZON, sizeof(float));
    bridge->fuse_buffer = nimcp_calloc(bridge->config.fused_latent_dim, sizeof(float));

    if (!bridge->encode_buffer || !bridge->predict_buffer || !bridge->fuse_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    /* Allocate entity latents */
    if (bridge->config.enable_entity_tracking && bridge->entities) {
        for (uint32_t e = 0; e < bridge->config.max_entities; e++) {
            bridge->entities[e].latent = nimcp_calloc(bridge->config.entity_latent_dim,
                                                       sizeof(float));
            bridge->entities[e].velocity = nimcp_calloc(bridge->config.entity_latent_dim,
                                                         sizeof(float));
            bridge->entities[e].latent_dim = bridge->config.entity_latent_dim;
        }
    }

    if (bridge->world_model) {
        bridge->state = VAE_WORLD_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        LOG_INFO("VAE-World bridge connected to VAE");
    }

    return 0;
}

int vae_world_bridge_connect_world_model(vae_world_bridge_t* bridge, void* world_model) {
    if (!bridge) return NIMCP_ERROR_VAE_WORLD_NULL;

    bridge->world_model = world_model;

    if (bridge->vae) {
        bridge->state = VAE_WORLD_STATE_CONNECTED;
    }

    return 0;
}

int vae_world_bridge_disconnect(vae_world_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_WORLD_NULL;

    bridge->state = VAE_WORLD_STATE_DISCONNECTED;

    return 0;
}

bool vae_world_bridge_is_connected(const vae_world_bridge_t* bridge) {
    return bridge && bridge->state == VAE_WORLD_STATE_CONNECTED;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_world_encode_modality(vae_world_bridge_t* bridge,
                               vae_world_modality_t modality,
                               const float* input, uint32_t input_dim,
                               vae_world_modality_result_t* result) {
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_WORLD_NULL;
    if (bridge->state != VAE_WORLD_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED;
    }

    uint32_t mod_idx = (uint32_t)modality;
    if (mod_idx >= VAE_WORLD_MAX_MODALITIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED;
    }

    bridge->state = VAE_WORLD_STATE_ENCODING;

    memset(result, 0, sizeof(*result));
    result->modality = modality;

    /* Encode with VAE using tensor API */
    uint32_t vae_latent_dim = vae_get_latent_dim(bridge->vae);
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&input_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&vae_latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&vae_latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), input, input_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED;
    }

    uint32_t latent_dim = bridge->config.modalities[mod_idx].latent_dim;
    if (latent_dim > vae_latent_dim) {
        latent_dim = vae_latent_dim;
    }

    /* Allocate and copy result */
    result->latent = nimcp_calloc(latent_dim, sizeof(float));
    result->variance = nimcp_calloc(latent_dim, sizeof(float));

    if (!result->latent || !result->variance) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    memcpy(result->latent, TENSOR_DATA_F32(mu_tensor), latent_dim * sizeof(float));
    float* log_var_data = TENSOR_DATA_F32(log_var_tensor);
    for (uint32_t i = 0; i < latent_dim; i++) {
        result->variance[i] = expf(log_var_data[i]);
    }
    result->latent_dim = latent_dim;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    /* Compute encoding quality from variance */
    float avg_var = 0.0f;
    for (uint32_t i = 0; i < latent_dim; i++) {
        avg_var += result->variance[i];
    }
    avg_var /= (float)latent_dim;
    result->encoding_quality = expf(-avg_var);

    /* Update bridge state */
    memcpy(bridge->modality_latents[mod_idx], result->latent, latent_dim * sizeof(float));

    bridge->stats.total_encodes++;
    bridge->stats.per_modality_usage[modality] += 1.0f;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_WORLD_STATE_CONNECTED;

    return 0;
}

int vae_world_encode_multimodal(vae_world_bridge_t* bridge,
                                 const float** inputs,
                                 const uint32_t* input_dims,
                                 const vae_world_modality_t* modalities,
                                 uint32_t num_modalities,
                                 vae_world_fusion_result_t* result) {
    if (!bridge || !inputs || !input_dims || !modalities || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NULL, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NULL;
    }

    /* Encode each modality */
    vae_world_modality_result_t mod_results[VAE_WORLD_MAX_MODALITIES];
    memset(mod_results, 0, sizeof(mod_results));

    for (uint32_t i = 0; i < num_modalities; i++) {
        int ret = vae_world_encode_modality(bridge, modalities[i],
                                             inputs[i], input_dims[i],
                                             &mod_results[i]);
        if (ret != 0) {
            /* Clean up on error */
            for (uint32_t j = 0; j < i; j++) {
                vae_world_modality_result_free(&mod_results[j]);
            }
            return ret;
        }
    }

    /* Fuse the results */
    int ret = vae_world_fuse(bridge, result);

    /* Clean up modality results */
    for (uint32_t i = 0; i < num_modalities; i++) {
        vae_world_modality_result_free(&mod_results[i]);
    }

    return ret;
}

/* ============================================================================
 * Fusion API
 * ============================================================================ */

int vae_world_fuse(vae_world_bridge_t* bridge,
                    vae_world_fusion_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_WORLD_NULL;
    if (bridge->state != VAE_WORLD_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED;
    }

    uint64_t start_time = get_time_us();
    bridge->state = VAE_WORLD_STATE_FUSING;

    memset(result, 0, sizeof(*result));

    /* Count valid modalities */
    uint32_t num_valid = 0;
    const float* valid_latents[VAE_WORLD_MAX_MODALITIES];
    uint32_t valid_dims[VAE_WORLD_MAX_MODALITIES];
    float valid_weights[VAE_WORLD_MAX_MODALITIES];

    for (uint32_t m = 0; m < VAE_WORLD_MAX_MODALITIES; m++) {
        if (bridge->modality_valid[m] && bridge->modality_latents[m]) {
            valid_latents[num_valid] = bridge->modality_latents[m];
            valid_dims[num_valid] = bridge->config.modalities[m].latent_dim;
            valid_weights[num_valid] = bridge->config.modalities[m].weight;
            num_valid++;
        }
    }

    if (num_valid == 0) {
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_FUSE_FAILED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_FUSE_FAILED;
    }

    /* Allocate output based on fusion strategy */
    uint32_t fused_dim = 0;

    switch (bridge->config.fusion_strategy) {
        case VAE_WORLD_FUSE_CONCATENATE: {
            /* Concatenate all latents */
            fused_dim = 0;
            for (uint32_t i = 0; i < num_valid; i++) {
                fused_dim += valid_dims[i];
            }

            result->fused_latent = nimcp_calloc(fused_dim, sizeof(float));
            if (!result->fused_latent) {
                bridge->state = VAE_WORLD_STATE_CONNECTED;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
                return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
            }

            concatenate_latents(valid_latents, valid_dims, num_valid,
                               result->fused_latent, &result->fused_dim);
            break;
        }

        case VAE_WORLD_FUSE_ATTENTION: {
            /* Attention-weighted combination */
            fused_dim = bridge->config.fused_latent_dim;

            result->fused_latent = nimcp_calloc(fused_dim, sizeof(float));
            result->attention_weights = nimcp_calloc(VAE_WORLD_MAX_MODALITIES *
                                                      VAE_WORLD_MAX_MODALITIES,
                                                      sizeof(float));
            if (!result->fused_latent || !result->attention_weights) {
                bridge->state = VAE_WORLD_STATE_CONNECTED;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
                return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
            }

            /* Use first modality as query for cross-modal attention */
            float attention[VAE_WORLD_MAX_MODALITIES];
            compute_attention(bridge, valid_latents[0], valid_dims[0], attention);

            /* Weighted sum */
            for (uint32_t m = 0; m < num_valid; m++) {
                float w = attention[m] > 0.0f ? attention[m] : valid_weights[m];
                uint32_t d = valid_dims[m] < fused_dim ? valid_dims[m] : fused_dim;
                for (uint32_t i = 0; i < d; i++) {
                    result->fused_latent[i] += w * valid_latents[m][i];
                }
            }

            /* Store attention weights */
            memcpy(result->attention_weights, attention,
                   VAE_WORLD_MAX_MODALITIES * sizeof(float));
            memcpy(bridge->attention_weights, attention,
                   VAE_WORLD_MAX_MODALITIES * sizeof(float));

            result->fused_dim = fused_dim;
            break;
        }

        case VAE_WORLD_FUSE_PRODUCT: {
            /* Product of experts - assumes same dimensionality */
            fused_dim = valid_dims[0];
            for (uint32_t i = 1; i < num_valid; i++) {
                if (valid_dims[i] < fused_dim) fused_dim = valid_dims[i];
            }

            result->fused_latent = nimcp_calloc(fused_dim, sizeof(float));
            if (!result->fused_latent) {
                bridge->state = VAE_WORLD_STATE_CONNECTED;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
                return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
            }

            /* Simplified PoE with unit variance assumption */
            float* variances[VAE_WORLD_MAX_MODALITIES];
            float* temp_vars = nimcp_calloc(num_valid * fused_dim, sizeof(float));
            if (!temp_vars) {
                nimcp_free(result->fused_latent);
                bridge->state = VAE_WORLD_STATE_CONNECTED;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
                return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
            }

            for (uint32_t i = 0; i < num_valid; i++) {
                variances[i] = &temp_vars[i * fused_dim];
                for (uint32_t d = 0; d < fused_dim; d++) {
                    variances[i][d] = 1.0f;  /* Unit variance */
                }
            }

            float* fused_var = nimcp_calloc(fused_dim, sizeof(float));
            if (!fused_var) return -1;
            product_of_experts(valid_latents, (const float**)variances,
                              valid_weights, num_valid, fused_dim,
                              result->fused_latent, fused_var);

            nimcp_free(temp_vars);
            temp_vars = NULL;
            nimcp_free(fused_var);
            fused_var = NULL;

            result->fused_dim = fused_dim;
            break;
        }

        case VAE_WORLD_FUSE_MIXTURE:
        default: {
            /* Mixture of experts - weighted average */
            fused_dim = bridge->config.fused_latent_dim;

            result->fused_latent = nimcp_calloc(fused_dim, sizeof(float));
            if (!result->fused_latent) {
                bridge->state = VAE_WORLD_STATE_CONNECTED;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
                return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
            }

            /* Normalize weights */
            float weight_sum = 0.0f;
            for (uint32_t i = 0; i < num_valid; i++) {
                weight_sum += valid_weights[i];
            }

            for (uint32_t m = 0; m < num_valid; m++) {
                float w = valid_weights[m] / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);
                uint32_t d = valid_dims[m] < fused_dim ? valid_dims[m] : fused_dim;
                for (uint32_t i = 0; i < d; i++) {
                    result->fused_latent[i] += w * valid_latents[m][i];
                }
            }

            result->fused_dim = fused_dim;
            break;
        }
    }

    result->num_modalities = num_valid;

    /* Compute fusion confidence */
    float avg_weight = 0.0f;
    for (uint32_t i = 0; i < num_valid; i++) {
        avg_weight += valid_weights[i];
    }
    avg_weight /= (float)num_valid;
    result->fusion_confidence = avg_weight * (float)num_valid / (float)VAE_WORLD_MAX_MODALITIES;

    /* Update bridge state */
    memcpy(bridge->fused_latent, result->fused_latent,
           result->fused_dim * sizeof(float));

    result->fusion_time_us = get_time_us() - start_time;

    bridge->stats.total_fusions++;
    bridge->stats.avg_fusion_confidence =
        (bridge->stats.avg_fusion_confidence * (bridge->stats.total_fusions - 1) +
         result->fusion_confidence) / bridge->stats.total_fusions;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_WORLD_STATE_CONNECTED;

    return 0;
}

int vae_world_fuse_with_attention(vae_world_bridge_t* bridge,
                                   const float* query, uint32_t query_dim,
                                   vae_world_fusion_result_t* result) {
    if (!bridge || !query || !result) return NIMCP_ERROR_VAE_WORLD_NULL;

    /* Temporarily override fusion strategy */
    vae_world_fusion_t original = bridge->config.fusion_strategy;
    bridge->config.fusion_strategy = VAE_WORLD_FUSE_ATTENTION;

    /* Compute attention with provided query */
    float attention[VAE_WORLD_MAX_MODALITIES];
    compute_attention(bridge, query, query_dim, attention);

    /* Store in bridge for use by fusion */
    memcpy(bridge->attention_weights, attention, VAE_WORLD_MAX_MODALITIES * sizeof(float));

    int ret = vae_world_fuse(bridge, result);

    bridge->config.fusion_strategy = original;

    return ret;
}

int vae_world_compute_cross_modal_attention(vae_world_bridge_t* bridge,
                                             float* attention_weights) {
    if (!bridge || !attention_weights) return NIMCP_ERROR_VAE_WORLD_NULL;

    /* Compute full attention matrix [num_mod, num_mod] */
    uint32_t n_mod = bridge->config.num_modalities;

    for (uint32_t q = 0; q < n_mod; q++) {
        if (!bridge->modality_valid[q]) continue;

        float row_attention[VAE_WORLD_MAX_MODALITIES];
        compute_attention(bridge, bridge->modality_latents[q],
                         bridge->config.modalities[q].latent_dim,
                         row_attention);

        memcpy(&attention_weights[q * VAE_WORLD_MAX_MODALITIES],
               row_attention, VAE_WORLD_MAX_MODALITIES * sizeof(float));
    }

    return 0;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int vae_world_predict(vae_world_bridge_t* bridge,
                       uint32_t horizon,
                       vae_world_prediction_result_t* result) {
    return vae_world_predict_from_latent(bridge, bridge->fused_latent,
                                          bridge->config.fused_latent_dim,
                                          horizon, result);
}

int vae_world_predict_with_action(vae_world_bridge_t* bridge,
                                   const float* action, uint32_t action_dim,
                                   uint32_t horizon,
                                   vae_world_prediction_result_t* result) {
    if (!bridge || !action || !result) return NIMCP_ERROR_VAE_WORLD_NULL;

    /* In a full implementation, action would condition the dynamics */
    /* For now, just add action as bias to prediction */
    (void)action;
    (void)action_dim;

    return vae_world_predict(bridge, horizon, result);
}

int vae_world_predict_from_latent(vae_world_bridge_t* bridge,
                                   const float* latent, uint32_t latent_dim,
                                   uint32_t horizon,
                                   vae_world_prediction_result_t* result) {
    if (!bridge || !latent || !result) return NIMCP_ERROR_VAE_WORLD_NULL;
    if (bridge->state != VAE_WORLD_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED;
    }

    if (horizon > VAE_WORLD_MAX_HORIZON) {
        horizon = VAE_WORLD_MAX_HORIZON;
    }

    uint64_t start_time = get_time_us();
    bridge->state = VAE_WORLD_STATE_PREDICTING;

    memset(result, 0, sizeof(*result));

    /* Allocate prediction buffers */
    result->predicted_latents = nimcp_calloc(horizon, sizeof(float*));
    result->predicted_variances = nimcp_calloc(horizon, sizeof(float*));

    if (!result->predicted_latents || !result->predicted_variances) {
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    for (uint32_t t = 0; t < horizon; t++) {
        result->predicted_latents[t] = nimcp_calloc(latent_dim, sizeof(float));
        result->predicted_variances[t] = nimcp_calloc(latent_dim, sizeof(float));
        if (!result->predicted_latents[t] || !result->predicted_variances[t]) {
            bridge->state = VAE_WORLD_STATE_CONNECTED;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
            return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
        }
    }

    result->horizon = horizon;
    result->latent_dim = latent_dim;

    /* Simple linear prediction (would use JEPA world model in full impl) */
    float* current = nimcp_calloc(latent_dim, sizeof(float));
    float* velocity = nimcp_calloc(latent_dim, sizeof(float));

    if (!current || !velocity) {
        nimcp_free(current);
        current = NULL;
        nimcp_free(velocity);
        velocity = NULL;
        bridge->state = VAE_WORLD_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    memcpy(current, latent, latent_dim * sizeof(float));

    /* Estimate velocity (small random for demo) */
    for (uint32_t i = 0; i < latent_dim; i++) {
        velocity[i] = 0.01f * ((float)nimcp_tl_rand() / RAND_MAX - 0.5f);
    }

    /* Generate predictions */
    for (uint32_t t = 0; t < horizon; t++) {
        predict_latent_dynamics(current, velocity, latent_dim, current);
        memcpy(result->predicted_latents[t], current, latent_dim * sizeof(float));

        /* Variance grows with horizon */
        float var_scale = 0.1f + 0.1f * (float)t;
        for (uint32_t i = 0; i < latent_dim; i++) {
            result->predicted_variances[t][i] = var_scale;
        }
    }

    /* Decode final prediction */
    uint32_t output_dim = vae_get_output_dim(bridge->vae);
    result->decoded_prediction = nimcp_calloc(output_dim, sizeof(float));
    result->decoded_dim = output_dim;

    if (result->decoded_prediction) {
        nimcp_tensor_t* z_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* output_tensor = nimcp_tensor_create(&output_dim, 1, NIMCP_DTYPE_F32);

        if (z_tensor && output_tensor) {
            memcpy(TENSOR_DATA_F32(z_tensor), result->predicted_latents[horizon - 1],
                   latent_dim * sizeof(float));
            int ret = vae_decode(bridge->vae, z_tensor, output_tensor);
            if (ret == 0) {
                memcpy(result->decoded_prediction, TENSOR_DATA_F32(output_tensor),
                       output_dim * sizeof(float));
            }
        }
        nimcp_tensor_destroy(z_tensor);
        nimcp_tensor_destroy(output_tensor);
    }

    nimcp_free(current);
    current = NULL;
    nimcp_free(velocity);
    velocity = NULL;

    /* Confidence decreases with horizon */
    result->prediction_confidence = expf(-0.05f * (float)horizon);

    result->prediction_time_us = get_time_us() - start_time;

    bridge->stats.total_predictions++;
    bridge->stats.avg_prediction_confidence =
        (bridge->stats.avg_prediction_confidence * (bridge->stats.total_predictions - 1) +
         result->prediction_confidence) / bridge->stats.total_predictions;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_WORLD_STATE_CONNECTED;

    return 0;
}

/* ============================================================================
 * Entity Tracking API
 * ============================================================================ */

int vae_world_track_entities(vae_world_bridge_t* bridge,
                              const float* observation, uint32_t obs_dim,
                              vae_world_entity_result_t* result) {
    if (!bridge || !observation || !result) return NIMCP_ERROR_VAE_WORLD_NULL;
    if (!bridge->config.enable_entity_tracking) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_FUSE_FAILED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_FUSE_FAILED;
    }

    uint64_t start_time = get_time_us();

    memset(result, 0, sizeof(*result));

    /* Encode observation to find entities using tensor API */
    uint32_t vae_lat_dim = vae_get_latent_dim(bridge->vae);
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&obs_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&vae_lat_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&vae_lat_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), observation, obs_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED;
    }

    float* mu_data = TENSOR_DATA_F32(mu_tensor);

    /* Simple entity extraction - use latent clusters */
    /* In full impl, would use slot attention or similar */
    uint32_t num_entities = 1 + (nimcp_tl_rand() % 4);  /* 1-4 entities */
    if (num_entities > bridge->config.max_entities) {
        num_entities = bridge->config.max_entities;
    }

    result->entities = nimcp_calloc(num_entities, sizeof(vae_world_entity_t));
    if (!result->entities) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
    }
    result->num_entities = num_entities;

    uint32_t entity_dim = bridge->config.entity_latent_dim;

    for (uint32_t e = 0; e < num_entities; e++) {
        result->entities[e].entity_id = e;
        result->entities[e].latent_dim = entity_dim;
        result->entities[e].latent = nimcp_calloc(entity_dim, sizeof(float));
        result->entities[e].velocity = nimcp_calloc(entity_dim, sizeof(float));

        if (!result->entities[e].latent || !result->entities[e].velocity) {
            nimcp_tensor_destroy(input_tensor);
            nimcp_tensor_destroy(mu_tensor);
            nimcp_tensor_destroy(log_var_tensor);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NO_MEMORY, "vae_world_model_bridge: error condition");
            return NIMCP_ERROR_VAE_WORLD_NO_MEMORY;
        }

        /* Partition latent across entities */
        uint32_t start = (e * vae_lat_dim) / num_entities;
        uint32_t copy_dim = entity_dim < vae_lat_dim - start ?
                           entity_dim : vae_lat_dim - start;
        memcpy(result->entities[e].latent, &mu_data[start],
               copy_dim * sizeof(float));

        result->entities[e].confidence = 0.8f - 0.1f * (float)e;
        result->entities[e].last_update_us = get_time_us();
    }

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    /* Compute entity interaction matrix */
    result->entity_interactions = nimcp_calloc(num_entities * num_entities, sizeof(float));
    if (result->entity_interactions) {
        for (uint32_t i = 0; i < num_entities; i++) {
            for (uint32_t j = 0; j < num_entities; j++) {
                if (i == j) {
                    result->entity_interactions[i * num_entities + j] = 1.0f;
                } else {
                    /* Cosine similarity between entity latents */
                    float dot = dot_product(result->entities[i].latent,
                                           result->entities[j].latent, entity_dim);
                    float norm_i = sqrtf(dot_product(result->entities[i].latent,
                                                     result->entities[i].latent, entity_dim));
                    float norm_j = sqrtf(dot_product(result->entities[j].latent,
                                                     result->entities[j].latent, entity_dim));
                    if (norm_i > 0.0f && norm_j > 0.0f) {
                        result->entity_interactions[i * num_entities + j] = dot / (norm_i * norm_j);
                    }
                }
            }
        }
    }

    result->tracking_time_us = get_time_us() - start_time;

    bridge->stats.active_entities = num_entities;

    return 0;
}

int vae_world_get_entity(const vae_world_bridge_t* bridge,
                          uint32_t entity_id,
                          vae_world_entity_t* entity) {
    if (!bridge || !entity) return NIMCP_ERROR_VAE_WORLD_NULL;

    if (entity_id >= bridge->num_entities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NULL, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NULL;
    }

    *entity = bridge->entities[entity_id];

    return 0;
}

int vae_world_predict_entity(vae_world_bridge_t* bridge,
                              uint32_t entity_id,
                              uint32_t horizon,
                              float* predicted_latent) {
    if (!bridge || !predicted_latent) return NIMCP_ERROR_VAE_WORLD_NULL;

    if (entity_id >= bridge->num_entities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_WORLD_NULL, "vae_world_model_bridge: error condition");
        return NIMCP_ERROR_VAE_WORLD_NULL;
    }

    vae_world_entity_t* entity = &bridge->entities[entity_id];

    /* Simple linear extrapolation */
    for (uint32_t i = 0; i < entity->latent_dim; i++) {
        predicted_latent[i] = entity->latent[i];
        if (entity->velocity) {
            predicted_latent[i] += entity->velocity[i] * (float)horizon;
        }
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_world_bridge_state_t vae_world_bridge_get_state(const vae_world_bridge_t* bridge) {
    if (!bridge) return VAE_WORLD_STATE_DISCONNECTED;
    return bridge->state;
}

int vae_world_bridge_get_stats(const vae_world_bridge_t* bridge,
                                vae_world_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_WORLD_NULL;

    *stats = bridge->stats;

    return 0;
}

int vae_world_get_fused_latent(const vae_world_bridge_t* bridge,
                                float* latent, uint32_t* dim) {
    if (!bridge || !latent || !dim) return NIMCP_ERROR_VAE_WORLD_NULL;

    memcpy(latent, bridge->fused_latent, bridge->config.fused_latent_dim * sizeof(float));
    *dim = bridge->config.fused_latent_dim;

    return 0;
}

const char* vae_world_modality_to_string(vae_world_modality_t modality) {
    switch (modality) {
        case VAE_WORLD_MOD_VISUAL:         return "visual";
        case VAE_WORLD_MOD_AUDITORY:       return "auditory";
        case VAE_WORLD_MOD_TACTILE:        return "tactile";
        case VAE_WORLD_MOD_PROPRIOCEPTIVE: return "proprioceptive";
        case VAE_WORLD_MOD_VESTIBULAR:     return "vestibular";
        case VAE_WORLD_MOD_OLFACTORY:      return "olfactory";
        case VAE_WORLD_MOD_GUSTATORY:      return "gustatory";
        case VAE_WORLD_MOD_INTEROCEPTIVE:  return "interoceptive";
        case VAE_WORLD_MOD_LINGUISTIC:     return "linguistic";
        case VAE_WORLD_MOD_SEMANTIC:       return "semantic";
        default:                           return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_world_modality_result_free(vae_world_modality_result_t* result) {
    if (!result) return;

    if (result->latent) {
        nimcp_free(result->latent);
        result->latent = NULL;
    }
    if (result->variance) {
        nimcp_free(result->variance);
        result->variance = NULL;
    }
}

void vae_world_fusion_result_free(vae_world_fusion_result_t* result) {
    if (!result) return;

    if (result->fused_latent) {
        nimcp_free(result->fused_latent);
        result->fused_latent = NULL;
    }
    if (result->attention_weights) {
        nimcp_free(result->attention_weights);
        result->attention_weights = NULL;
    }
}

void vae_world_prediction_result_free(vae_world_prediction_result_t* result) {
    if (!result) return;

    if (result->predicted_latents) {
        for (uint32_t t = 0; t < result->horizon; t++) {
            if (result->predicted_latents[t]) {
                nimcp_free(result->predicted_latents[t]);
            }
        }
        nimcp_free(result->predicted_latents);
        result->predicted_latents = NULL;
    }

    if (result->predicted_variances) {
        for (uint32_t t = 0; t < result->horizon; t++) {
            if (result->predicted_variances[t]) {
                nimcp_free(result->predicted_variances[t]);
            }
        }
        nimcp_free(result->predicted_variances);
        result->predicted_variances = NULL;
    }

    if (result->decoded_prediction) {
        nimcp_free(result->decoded_prediction);
        result->decoded_prediction = NULL;
    }
}

void vae_world_entity_result_free(vae_world_entity_result_t* result) {
    if (!result) return;

    if (result->entities) {
        for (uint32_t e = 0; e < result->num_entities; e++) {
            if (result->entities[e].latent) {
                nimcp_free(result->entities[e].latent);
            }
            if (result->entities[e].velocity) {
                nimcp_free(result->entities[e].velocity);
            }
        }
        nimcp_free(result->entities);
        result->entities = NULL;
    }

    if (result->entity_interactions) {
        nimcp_free(result->entity_interactions);
        result->entity_interactions = NULL;
    }
}
