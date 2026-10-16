/**
 * @file nimcp_visual_jepa_bridge.c
 * @brief Visual JEPA Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Implementation of visual-to-JEPA encoding bridge
 * WHY:  Enable JEPA-based visual representation learning
 * HOW:  Patch extraction, MLP encoding, masked prediction training
 */

#include "perception/nimcp_visual_jepa_bridge.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "[VISUAL_JEPA]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_jepa_bridge)

/* ============================================================================
 * Internal Helpers - Activation
 * ============================================================================ */

static inline float gelu(float x) {
    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;
    float x3 = x * x * x;
    float inner = sqrt_2_pi * (x + coeff * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

/* ============================================================================
 * Internal Helpers - Encoder Operations
 * ============================================================================ */

static visual_jepa_encoder_t* encoder_create(uint32_t input_dim,
                                               uint32_t hidden_dim,
                                               uint32_t output_dim) {
    visual_jepa_encoder_t* enc = nimcp_malloc(sizeof(visual_jepa_encoder_t));
    if (!enc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "enc is NULL");

        return NULL;

    }

    enc->input_dim = input_dim;
    enc->hidden_dim = hidden_dim;
    enc->output_dim = output_dim;

    /* Allocate weights and biases */
    enc->weights_1 = nimcp_malloc(hidden_dim * input_dim * sizeof(float));
    enc->bias_1 = nimcp_malloc(hidden_dim * sizeof(float));
    enc->weights_2 = nimcp_malloc(output_dim * hidden_dim * sizeof(float));
    enc->bias_2 = nimcp_malloc(output_dim * sizeof(float));

    if (!enc->weights_1 || !enc->bias_1 || !enc->weights_2 || !enc->bias_2) {
        nimcp_free(enc->weights_1);
        nimcp_free(enc->bias_1);
        nimcp_free(enc->weights_2);
        nimcp_free(enc->bias_2);
        nimcp_free(enc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gelu: required parameter is NULL (enc->weights_1, enc->bias_1, enc->weights_2, enc->bias_2)");
        return NULL;
    }

    /* Xavier initialization */
    float scale1 = sqrtf(2.0f / (float)(input_dim + hidden_dim));
    float scale2 = sqrtf(2.0f / (float)(hidden_dim + output_dim));

    for (uint32_t i = 0; i < hidden_dim * input_dim; i++) {
        enc->weights_1[i] = (nimcp_rand_uniform() * 2.0f - 1.0f) * scale1;
    }
    for (uint32_t i = 0; i < output_dim * hidden_dim; i++) {
        enc->weights_2[i] = (nimcp_rand_uniform() * 2.0f - 1.0f) * scale2;
    }
    memset(enc->bias_1, 0, hidden_dim * sizeof(float));
    memset(enc->bias_2, 0, output_dim * sizeof(float));

    return enc;
}

static void encoder_destroy(visual_jepa_encoder_t* enc) {
    if (!enc) return;
    nimcp_free(enc->weights_1);
    nimcp_free(enc->bias_1);
    nimcp_free(enc->weights_2);
    nimcp_free(enc->bias_2);
    nimcp_free(enc);
}

static int encoder_forward(const visual_jepa_encoder_t* enc,
                            const float* input,
                            float* output,
                            float* hidden_buffer) {
    NIMCP_CHECK_THROW(enc, NIMCP_ERROR_NULL_POINTER, "encoder is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");

    /* Layer 1: hidden = GELU(W1 @ input + b1) */
    for (uint32_t i = 0; i < enc->hidden_dim; i++) {
        double sum = enc->bias_1[i];
        for (uint32_t j = 0; j < enc->input_dim; j++) {
            sum += enc->weights_1[i * enc->input_dim + j] * input[j];
        }
        hidden_buffer[i] = gelu((float)sum);
    }

    /* Layer 2: output = W2 @ hidden + b2 */
    for (uint32_t i = 0; i < enc->output_dim; i++) {
        double sum = enc->bias_2[i];
        for (uint32_t j = 0; j < enc->hidden_dim; j++) {
            sum += enc->weights_2[i * enc->hidden_dim + j] * hidden_buffer[j];
        }
        output[i] = (float)sum;
    }

    return NIMCP_SUCCESS;
}

static visual_jepa_encoder_t* encoder_clone(const visual_jepa_encoder_t* src) {
    if (!src) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;

    }

    visual_jepa_encoder_t* dst = encoder_create(src->input_dim,
                                                  src->hidden_dim,
                                                  src->output_dim);
    if (!dst) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dst is NULL");

        return NULL;

    }

    memcpy(dst->weights_1, src->weights_1,
           src->hidden_dim * src->input_dim * sizeof(float));
    memcpy(dst->bias_1, src->bias_1, src->hidden_dim * sizeof(float));
    memcpy(dst->weights_2, src->weights_2,
           src->output_dim * src->hidden_dim * sizeof(float));
    memcpy(dst->bias_2, src->bias_2, src->output_dim * sizeof(float));

    return dst;
}

static void encoder_ema_update(visual_jepa_encoder_t* target,
                                const visual_jepa_encoder_t* online,
                                float decay) {
    if (!target || !online) return;

    float one_minus_decay = 1.0f - decay;

    for (uint32_t i = 0; i < target->hidden_dim * target->input_dim; i++) {
        target->weights_1[i] = decay * target->weights_1[i] +
                                one_minus_decay * online->weights_1[i];
    }
    for (uint32_t i = 0; i < target->hidden_dim; i++) {
        target->bias_1[i] = decay * target->bias_1[i] +
                            one_minus_decay * online->bias_1[i];
    }
    for (uint32_t i = 0; i < target->output_dim * target->hidden_dim; i++) {
        target->weights_2[i] = decay * target->weights_2[i] +
                                one_minus_decay * online->weights_2[i];
    }
    for (uint32_t i = 0; i < target->output_dim; i++) {
        target->bias_2[i] = decay * target->bias_2[i] +
                            one_minus_decay * online->bias_2[i];
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int visual_jepa_bridge_default_config(visual_jepa_bridge_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(visual_jepa_bridge_config_t));

    /* Encoder config */
    config->encoder.type = VISUAL_JEPA_ENCODER_MLP;
    config->encoder.input_dim = 256;  /* From V1 Gabor features */
    config->encoder.hidden_dim = VISUAL_JEPA_DEFAULT_ENCODER_DIM;
    config->encoder.output_dim = NIMCP_JEPA_LATENT_DIM;
    config->encoder.num_layers = 2;
    config->encoder.use_layer_norm = true;

    /* Patch config */
    config->patch.strategy = VISUAL_JEPA_PATCH_GRID;
    config->patch.patch_width = VISUAL_JEPA_DEFAULT_PATCH_SIZE;
    config->patch.patch_height = VISUAL_JEPA_DEFAULT_PATCH_SIZE;
    config->patch.num_patches_x = 7;
    config->patch.num_patches_y = 7;
    config->patch.stride_x = VISUAL_JEPA_DEFAULT_PATCH_SIZE;
    config->patch.stride_y = VISUAL_JEPA_DEFAULT_PATCH_SIZE;

    /* Predictor config */
    jepa_predictor_default_config(&config->predictor);
    config->predictor.input_dim = NIMCP_JEPA_LATENT_DIM;
    config->predictor.output_dim = NIMCP_JEPA_LATENT_DIM;

    /* Masking config */
    jepa_mask_default_config(&config->masking, JEPA_MASK_BLOCK_MULTI);
    config->masking.target_ratio = 0.75f;

    /* Training parameters */
    config->learning_rate = 0.001f;
    config->momentum = 0.996f;
    config->use_target_encoder = true;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

visual_jepa_bridge_t* visual_jepa_bridge_create(
    const visual_jepa_bridge_config_t* config) {

    visual_jepa_bridge_config_t default_config;
    if (!config) {
        visual_jepa_bridge_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    visual_jepa_bridge_t* bridge = nimcp_malloc(sizeof(visual_jepa_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_bridge_default_config: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(visual_jepa_bridge_t));

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_VISUAL_JEPA,
                         "visual_jepa") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: operation failed");
        return NULL;
    }

    /* Store config */
    bridge->config = *config;

    /* Create encoder */
    bridge->encoder = encoder_create(config->encoder.input_dim,
                                      config->encoder.hidden_dim,
                                      config->encoder.output_dim);
    if (!bridge->encoder) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: bridge->encoder is NULL");
        return NULL;
    }

    /* Create target encoder (EMA copy) */
    if (config->use_target_encoder) {
        bridge->target_encoder = encoder_clone(bridge->encoder);
        if (!bridge->target_encoder) {
            encoder_destroy(bridge->encoder);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: bridge->target_encoder is NULL");
            return NULL;
        }
    }

    /* Create predictor */
    bridge->predictor = jepa_predictor_create(&config->predictor);
    if (!bridge->predictor) {
        encoder_destroy(bridge->target_encoder);
        encoder_destroy(bridge->encoder);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: bridge->predictor is NULL");
        return NULL;
    }

    /* Create mask generator */
    bridge->mask_gen = jepa_mask_generator_create(&config->masking);
    if (!bridge->mask_gen) {
        jepa_predictor_destroy(bridge->predictor);
        encoder_destroy(bridge->target_encoder);
        encoder_destroy(bridge->encoder);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: bridge->mask_gen is NULL");
        return NULL;
    }

    /* Allocate working buffers */
    uint32_t max_patch_size = config->patch.patch_width *
                               config->patch.patch_height * 4;  /* RGBA */
    bridge->patch_buffer_size = max_patch_size;
    bridge->patch_buffer = nimcp_malloc(bridge->patch_buffer_size * sizeof(float));
    bridge->encoding_buffer = nimcp_malloc(config->encoder.hidden_dim * sizeof(float));

    if (!bridge->patch_buffer || !bridge->encoding_buffer) {
        nimcp_free(bridge->patch_buffer);
        nimcp_free(bridge->encoding_buffer);
        jepa_mask_generator_destroy(bridge->mask_gen);
        jepa_predictor_destroy(bridge->predictor);
        encoder_destroy(bridge->target_encoder);
        encoder_destroy(bridge->encoder);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_bridge_default_config: required parameter is NULL (bridge->patch_buffer, bridge->encoding_buffer)");
        return NULL;
    }

    /* Initialize training state */
    bridge->training_mode = false;
    bridge->ema_decay = config->momentum;
    bridge->stats.min_loss = FLT_MAX;

    NIMCP_LOGGING_INFO(LOG_MODULE " Created bridge: encoder=%u→%u→%u, patches=%ux%u",
                      config->encoder.input_dim,
                      config->encoder.hidden_dim,
                      config->encoder.output_dim,
                      config->patch.num_patches_x,
                      config->patch.num_patches_y);

    return bridge;
}

void visual_jepa_bridge_destroy(visual_jepa_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->patch_buffer);
    nimcp_free(bridge->encoding_buffer);
    jepa_mask_generator_destroy(bridge->mask_gen);
    jepa_predictor_destroy(bridge->predictor);
    encoder_destroy(bridge->target_encoder);
    encoder_destroy(bridge->encoder);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int visual_jepa_bridge_reset(visual_jepa_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Reset encoder weights */
    encoder_destroy(bridge->encoder);
    bridge->encoder = encoder_create(bridge->config.encoder.input_dim,
                                      bridge->config.encoder.hidden_dim,
                                      bridge->config.encoder.output_dim);
    NIMCP_CHECK_THROW(bridge->encoder, NIMCP_ERROR_NO_MEMORY, "Failed to create encoder");

    if (bridge->config.use_target_encoder) {
        encoder_destroy(bridge->target_encoder);
        bridge->target_encoder = encoder_clone(bridge->encoder);
    }

    /* Reset predictor */
    jepa_predictor_reset(bridge->predictor);

    /* Reset mask generator */
    jepa_mask_generator_reset(bridge->mask_gen, 0);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(visual_jepa_stats_t));
    bridge->stats.min_loss = FLT_MAX;
    bridge->training_step = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int visual_jepa_bridge_connect_visual_cortex(
    visual_jepa_bridge_t* bridge,
    visual_cortex_t* visual) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(visual, NIMCP_ERROR_NULL_POINTER, "visual cortex is NULL");

    bridge->visual_cortex = visual;
    bridge->base.system_a = visual;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    NIMCP_LOGGING_INFO(LOG_MODULE " Connected to visual cortex");
    return NIMCP_SUCCESS;
}

int visual_jepa_bridge_disconnect_visual_cortex(visual_jepa_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bridge->visual_cortex = NULL;
    bridge->base.system_a = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.bridge_active = false;

    return NIMCP_SUCCESS;
}

bool visual_jepa_bridge_is_connected(const visual_jepa_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->visual_cortex != NULL;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int visual_jepa_bridge_encode(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    jepa_latent_t* latent) {

    NIMCP_CHECK_THROW(bridge && features && latent, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_jepa_bridge_encode");
    NIMCP_CHECK_THROW(feature_dim == bridge->encoder->input_dim, NIMCP_ERROR_INVALID_PARAM,
        "Feature dim mismatch in visual_jepa_bridge_encode");
    NIMCP_CHECK_THROW(latent->latent_dim == bridge->encoder->output_dim, NIMCP_ERROR_INVALID_PARAM,
        "Latent dim mismatch in visual_jepa_bridge_encode");

    /* Select encoder (target for inference in training mode) */
    const visual_jepa_encoder_t* enc = bridge->encoder;
    if (bridge->training_mode && bridge->target_encoder) {
        enc = bridge->target_encoder;
    }

    /* Forward pass */
    int result = encoder_forward(enc, features, latent->embedding,
                                  bridge->encoding_buffer);

    if (result == NIMCP_SUCCESS) {
        latent->modality = JEPA_MODALITY_VISUAL;
        latent->is_normalized = false;
        bridge->stats.patches_encoded++;
    }

    return result;
}

int visual_jepa_bridge_encode_patches(
    visual_jepa_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    jepa_latent_t** patch_latents,
    uint32_t* num_patches) {

    NIMCP_CHECK_THROW(bridge && image && patch_latents && num_patches, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_jepa_bridge_encode_patches");
    NIMCP_CHECK_THROW(width > 0 && height > 0 && channels > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid dimensions in visual_jepa_bridge_encode_patches");

    const visual_jepa_patch_config_t* pc = &bridge->config.patch;
    uint32_t total_patches = pc->num_patches_x * pc->num_patches_y;
    *num_patches = total_patches;

    /* Extract and encode each patch */
    for (uint32_t py = 0; py < pc->num_patches_y; py++) {
        for (uint32_t px = 0; px < pc->num_patches_x; px++) {
            uint32_t patch_idx = py * pc->num_patches_x + px;

            /* Calculate patch boundaries */
            uint32_t start_x = px * pc->stride_x;
            uint32_t start_y = py * pc->stride_y;

            /* Extract patch features (flatten and average pool) */
            float* patch = bridge->patch_buffer;
            uint32_t patch_size = 0;

            for (uint32_t y = start_y; y < start_y + pc->patch_height && y < height; y++) {
                for (uint32_t x = start_x; x < start_x + pc->patch_width && x < width; x++) {
                    for (uint32_t c = 0; c < channels; c++) {
                        if (patch_size < bridge->patch_buffer_size) {
                            patch[patch_size++] = image[(y * width + x) * channels + c];
                        }
                    }
                }
            }

            /* Pad/truncate to encoder input dim */
            uint32_t input_dim = bridge->encoder->input_dim;
            float* enc_input = nimcp_malloc(input_dim * sizeof(float));
            NIMCP_CHECK_THROW(enc_input, NIMCP_ERROR_NO_MEMORY, "Failed to allocate encoder input buffer");

            if (patch_size >= input_dim) {
                /* Average pool if too large */
                uint32_t pool_size = patch_size / input_dim;
                for (uint32_t i = 0; i < input_dim; i++) {
                    double sum = 0.0;
                    for (uint32_t j = 0; j < pool_size && (i * pool_size + j) < patch_size; j++) {
                        sum += patch[i * pool_size + j];
                    }
                    enc_input[i] = (float)(sum / pool_size);
                }
            } else {
                /* Pad with zeros */
                memcpy(enc_input, patch, patch_size * sizeof(float));
                memset(enc_input + patch_size, 0,
                       (input_dim - patch_size) * sizeof(float));
            }

            /* Encode patch */
            int result = visual_jepa_bridge_encode(bridge, enc_input, input_dim,
                                                    patch_latents[patch_idx]);
            nimcp_free(enc_input);

            if (result != NIMCP_SUCCESS) {
                return result;
            }
        }
    }

    bridge->stats.frames_processed++;
    return NIMCP_SUCCESS;
}

int visual_jepa_bridge_encode_attended(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    const float* attention,
    uint32_t width,
    uint32_t height,
    jepa_latent_t* latent) {

    NIMCP_CHECK_THROW(bridge && features && attention && latent, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_jepa_bridge_encode_attended");

    /* Compute attention-weighted average of features */
    float* weighted_features = nimcp_malloc(feature_dim * sizeof(float));
    NIMCP_CHECK_THROW(weighted_features, NIMCP_ERROR_NO_MEMORY, "Failed to allocate weighted features buffer");

    memset(weighted_features, 0, feature_dim * sizeof(float));
    double total_weight = 0.0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            float w = attention[y * width + x];
            total_weight += w;

            for (uint32_t f = 0; f < feature_dim; f++) {
                weighted_features[f] += w * features[(y * width + x) * feature_dim + f];
            }
        }
    }

    if (total_weight > 0.0) {
        for (uint32_t f = 0; f < feature_dim; f++) {
            weighted_features[f] /= (float)total_weight;
        }
    }

    int result = visual_jepa_bridge_encode(bridge, weighted_features,
                                            feature_dim, latent);
    nimcp_free(weighted_features);

    return result;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int visual_jepa_bridge_set_training(visual_jepa_bridge_t* bridge, bool training) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bridge->training_mode = training;
    jepa_predictor_set_training(bridge->predictor, training);

    return NIMCP_SUCCESS;
}

int visual_jepa_bridge_train_step(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* loss) {

    NIMCP_CHECK_THROW(bridge && features, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_jepa_bridge_train_step");

    const visual_jepa_patch_config_t* pc = &bridge->config.patch;
    uint32_t num_patches = pc->num_patches_x * pc->num_patches_y;
    uint32_t latent_dim = bridge->config.encoder.output_dim;

    /* Create patch latents */
    jepa_latent_t** patch_latents = nimcp_malloc(num_patches * sizeof(jepa_latent_t*));
    NIMCP_CHECK_THROW(patch_latents, NIMCP_ERROR_NO_MEMORY, "Failed to allocate patch latents array");

    for (uint32_t i = 0; i < num_patches; i++) {
        patch_latents[i] = jepa_latent_create_dim(latent_dim);
        if (!patch_latents[i]) {
            for (uint32_t j = 0; j < i; j++) {
                jepa_latent_destroy(patch_latents[j]);
            }
            nimcp_free(patch_latents);
            return NIMCP_ERROR_MEMORY;
        }
    }

    /* Encode patches using ONLINE encoder */
    int result = visual_jepa_bridge_encode_patches(bridge, features, width, height,
                                                    channels, patch_latents, &num_patches);
    if (result != NIMCP_SUCCESS) {
        for (uint32_t i = 0; i < num_patches; i++) {
            jepa_latent_destroy(patch_latents[i]);
        }
        nimcp_free(patch_latents);
        return result;
    }

    /* Generate mask */
    jepa_mask_t* mask = jepa_mask_create(pc->num_patches_x, pc->num_patches_y, 1);
    if (!mask) {
        for (uint32_t i = 0; i < num_patches; i++) {
            jepa_latent_destroy(patch_latents[i]);
        }
        nimcp_free(patch_latents);
        return NIMCP_ERROR_MEMORY;
    }

    jepa_mask_generate_2d(bridge->mask_gen, pc->num_patches_x, pc->num_patches_y, mask);

    /* Separate context (visible) and target (masked) patches */
    uint32_t num_context = mask->num_visible;
    uint32_t num_targets = mask->num_masked;

    if (num_context == 0 || num_targets == 0) {
        jepa_mask_destroy(mask);
        for (uint32_t i = 0; i < num_patches; i++) {
            jepa_latent_destroy(patch_latents[i]);
        }
        nimcp_free(patch_latents);
        if (loss) *loss = 0.0f;
        return NIMCP_SUCCESS;
    }

    /* Pool context latents (simple mean for now) */
    jepa_latent_t* context_pooled = jepa_latent_create_dim(latent_dim);
    memset(context_pooled->embedding, 0, latent_dim * sizeof(float));

    for (uint32_t i = 0; i < num_patches; i++) {
        if (mask->data[i] < 0.5f) {  /* Visible */
            for (uint32_t d = 0; d < latent_dim; d++) {
                context_pooled->embedding[d] += patch_latents[i]->embedding[d];
            }
        }
    }
    for (uint32_t d = 0; d < latent_dim; d++) {
        context_pooled->embedding[d] /= (float)num_context;
    }

    /* Compute prediction loss for each target */
    float total_loss = 0.0f;

    for (uint32_t i = 0; i < num_patches; i++) {
        if (mask->data[i] >= 0.5f) {  /* Masked = target */
            /* Predict target from context */
            jepa_latent_t* pred = jepa_latent_create_dim(latent_dim);

            result = jepa_predictor_predict(bridge->predictor, context_pooled, pred);
            if (result == NIMCP_SUCCESS) {
                /* Compute loss against target (encoded with target encoder) */
                float patch_loss = jepa_predictor_compute_loss(bridge->predictor,
                                                                pred, patch_latents[i]);
                total_loss += patch_loss;
            }

            jepa_latent_destroy(pred);
        }
    }

    total_loss /= (float)num_targets;

    /* Update statistics */
    bridge->stats.predictions_made += num_targets;
    float new_avg_loss = 0.9f * bridge->stats.avg_prediction_loss + 0.1f * total_loss;
    if (isfinite(new_avg_loss)) bridge->stats.avg_prediction_loss = new_avg_loss;
    if (total_loss < bridge->stats.min_loss) {
        bridge->stats.min_loss = total_loss;
    }

    if (loss) *loss = total_loss;

    /* Cleanup */
    jepa_latent_destroy(context_pooled);
    jepa_mask_destroy(mask);
    for (uint32_t i = 0; i < num_patches; i++) {
        jepa_latent_destroy(patch_latents[i]);
    }
    nimcp_free(patch_latents);

    /* Update target encoder */
    if (bridge->config.use_target_encoder) {
        visual_jepa_bridge_update_target_encoder(bridge);
    }

    bridge->training_step++;
    return NIMCP_SUCCESS;
}

int visual_jepa_bridge_update_target_encoder(visual_jepa_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->target_encoder || !bridge->encoder) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    encoder_ema_update(bridge->target_encoder, bridge->encoder, bridge->ema_decay);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int visual_jepa_bridge_predict_masked(
    visual_jepa_bridge_t* bridge,
    jepa_latent_t** context_latents,
    uint32_t num_context,
    const jepa_mask_t* mask,
    jepa_latent_t** predictions,
    uint32_t* num_predictions) {

    NIMCP_CHECK_THROW(bridge && context_latents && mask && predictions && num_predictions,
        NIMCP_ERROR_NULL_POINTER, "NULL parameter in visual_jepa_bridge_predict_masked");

    uint32_t latent_dim = bridge->config.encoder.output_dim;

    /* Pool context latents */
    jepa_latent_t* context_pooled = jepa_latent_create_dim(latent_dim);
    memset(context_pooled->embedding, 0, latent_dim * sizeof(float));

    for (uint32_t i = 0; i < num_context; i++) {
        for (uint32_t d = 0; d < latent_dim; d++) {
            context_pooled->embedding[d] += context_latents[i]->embedding[d];
        }
    }
    for (uint32_t d = 0; d < latent_dim; d++) {
        context_pooled->embedding[d] /= (float)num_context;
    }

    /* Predict for each masked position */
    *num_predictions = mask->num_masked;
    uint32_t pred_idx = 0;

    for (uint32_t i = 0; i < mask->total_size && pred_idx < *num_predictions; i++) {
        if (mask->data[i] >= 0.5f) {
            int result = jepa_predictor_predict(bridge->predictor,
                                                 context_pooled,
                                                 predictions[pred_idx]);
            if (result != NIMCP_SUCCESS) {
                jepa_latent_destroy(context_pooled);
                return result;
            }
            pred_idx++;
        }
    }

    jepa_latent_destroy(context_pooled);
    bridge->stats.predictions_made += *num_predictions;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int visual_jepa_bridge_get_stats(
    const visual_jepa_bridge_t* bridge,
    visual_jepa_stats_t* stats) {

    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    *stats = bridge->stats;
    return NIMCP_SUCCESS;
}

int visual_jepa_bridge_reset_stats(visual_jepa_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    memset(&bridge->stats, 0, sizeof(visual_jepa_stats_t));
    bridge->stats.min_loss = FLT_MAX;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS(visual_jepa_bridge)

/* ============================================================================
 * Utility API
 * ============================================================================ */

visual_jepa_batch_t* visual_jepa_batch_create(
    uint32_t num_patches,
    uint32_t patch_dim,
    uint32_t latent_dim) {

    /* Require at least one patch */
    if (num_patches == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_jepa_batch_create: num_patches is 0");
        return NULL;
    }

    visual_jepa_batch_t* batch = nimcp_malloc(sizeof(visual_jepa_batch_t));
    if (!batch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "batch is NULL");

        return NULL;

    }
    memset(batch, 0, sizeof(visual_jepa_batch_t));

    batch->num_patches = num_patches;

    /* Allocate patches */
    batch->patches = nimcp_malloc(num_patches * sizeof(visual_jepa_patch_t));
    if (!batch->patches) {
        nimcp_free(batch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_bridge_reset_stats: batch->patches is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_patches; i++) {
        batch->patches[i].features = nimcp_malloc(patch_dim * sizeof(float));
        batch->patches[i].feature_dim = patch_dim;
        if (!batch->patches[i].features) {
            visual_jepa_batch_destroy(batch);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_bridge_reset_stats: batch->patches is NULL");
            return NULL;
        }
    }

    /* Allocate latent arrays */
    batch->context_latents = nimcp_malloc(num_patches * sizeof(jepa_latent_t*));
    batch->target_latents = nimcp_malloc(num_patches * sizeof(jepa_latent_t*));

    if (!batch->context_latents || !batch->target_latents) {
        visual_jepa_batch_destroy(batch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_bridge_reset_stats: required parameter is NULL (batch->context_latents, batch->target_latents)");
        return NULL;
    }

    for (uint32_t i = 0; i < num_patches; i++) {
        batch->context_latents[i] = jepa_latent_create_dim(latent_dim);
        batch->target_latents[i] = jepa_latent_create_dim(latent_dim);
        if (!batch->context_latents[i] || !batch->target_latents[i]) {
            visual_jepa_batch_destroy(batch);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_bridge_reset_stats: required parameter is NULL (batch->context_latents, batch->target_latents)");
            return NULL;
        }
    }

    return batch;
}

void visual_jepa_batch_destroy(visual_jepa_batch_t* batch) {
    if (!batch) return;

    if (batch->patches) {
        for (uint32_t i = 0; i < batch->num_patches; i++) {
            nimcp_free(batch->patches[i].features);
        }
        nimcp_free(batch->patches);
    }

    if (batch->context_latents) {
        for (uint32_t i = 0; i < batch->num_patches; i++) {
            jepa_latent_destroy(batch->context_latents[i]);
        }
        nimcp_free(batch->context_latents);
    }

    if (batch->target_latents) {
        for (uint32_t i = 0; i < batch->num_patches; i++) {
            jepa_latent_destroy(batch->target_latents[i]);
        }
        nimcp_free(batch->target_latents);
    }

    jepa_mask_destroy(batch->mask);
    nimcp_free(batch);
}
