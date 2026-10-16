/**
 * @file nimcp_jepa_context.c
 * @brief JEPA Context Encoder Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Context-conditioned encoding for JEPA embeddings
 * WHY:  Same input should produce different representations based on task
 * HOW:  FiLM, cross-attention, or other conditioning mechanisms
 *
 * @author NIMCP Development Team
 */

#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE(jepa_context, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define CONTEXT_LOG_TAG "JEPA_CTX"
#define GELU_CONST 0.044715f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int film_layer_create(jepa_film_layer_t** film, uint32_t context_dim,
                              uint32_t feature_dim, uint32_t hidden_dim);
static void film_layer_destroy(jepa_film_layer_t* film);
static int film_layer_forward(const jepa_film_layer_t* film,
                               const float* context, float* gamma, float* beta);

static int cross_attention_create(jepa_cross_attention_t** attn,
                                   uint32_t input_dim, uint32_t context_dim,
                                   uint32_t num_heads, uint32_t head_dim);
static void cross_attention_destroy(jepa_cross_attention_t* attn);
static int cross_attention_forward(const jepa_cross_attention_t* attn,
                                    const float* query, const float* context,
                                    float* output);

static float gelu_activation(float x);
static void layer_normalize(float* data, uint32_t dim);
static void softmax(float* data, uint32_t dim);
static int compose_context(jepa_context_encoder_t* encoder);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_context_default_config(jepa_context_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Context composition */
    config->context_dim = JEPA_CONTEXT_DEFAULT_DIM;
    config->num_sources = 4;

    /* Task context */
    config->sources[0].type = JEPA_CTX_TASK;
    config->sources[0].dim = 64;
    config->sources[0].weight = 1.0f;
    config->sources[0].enabled = true;
    config->sources[0].name = "task";

    /* Working memory context */
    config->sources[1].type = JEPA_CTX_WORKING_MEMORY;
    config->sources[1].dim = 32;
    config->sources[1].weight = 0.5f;
    config->sources[1].enabled = true;
    config->sources[1].name = "working_memory";

    /* Attention context */
    config->sources[2].type = JEPA_CTX_ATTENTION;
    config->sources[2].dim = 32;
    config->sources[2].weight = 0.5f;
    config->sources[2].enabled = true;
    config->sources[2].name = "attention";

    /* Custom context */
    config->sources[3].type = JEPA_CTX_CUSTOM;
    config->sources[3].dim = 128;
    config->sources[3].weight = 1.0f;
    config->sources[3].enabled = true;
    config->sources[3].name = "custom";

    /* Conditioning mechanism */
    config->conditioning = JEPA_COND_FILM;

    /* FiLM settings */
    config->film_hidden_dim = 64;

    /* Cross-attention settings */
    config->num_attention_heads = JEPA_CONTEXT_DEFAULT_NUM_HEADS;
    config->attention_dim = 32;

    /* Input/output dimensions */
    config->input_dim = 256;
    config->output_dim = 256;

    /* Training */
    config->dropout_rate = 0.0f;
    config->use_layer_norm = true;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_context_encoder_t* jepa_context_encoder_create(
    const jepa_context_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_encoder_create", 0.0f);


    jepa_context_encoder_t* encoder = NULL;
    jepa_context_config_t default_config;
    int rc = 0;

    /* Use defaults if not provided */
    if (!config) {
        jepa_context_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate encoder */
    encoder = (jepa_context_encoder_t*)nimcp_calloc(1, sizeof(jepa_context_encoder_t));
    if (!encoder) return NULL;
    NIMCP_API_CHECK_ALLOC(encoder, "Failed to allocate context encoder");

    /* Initialize bridge base */
    if (bridge_base_init(&encoder->base, BIO_MODULE_JEPA_CONTEXT, "JEPA_CONTEXT") != 0) {
        nimcp_free(encoder);
        return NULL;
    }
    encoder->base.bridge_active = false;

    /* Copy configuration */
    memcpy(&encoder->config, config, sizeof(jepa_context_config_t));

    /* Create context state */
    encoder->current_context = jepa_context_state_create();
    if (!encoder->current_context) {
        NIMCP_LOGGING_ERROR("Failed to create context state");
        goto cleanup;
    }

    /* Allocate composed context buffer */
    encoder->composed_context = (float*)nimcp_calloc(config->context_dim, sizeof(float));
    if (!encoder->composed_context) {
        goto cleanup;
    }

    /* Allocate context weights */
    encoder->context_weights = (float*)nimcp_calloc(
        JEPA_CONTEXT_MAX_SOURCES, sizeof(float));
    if (!encoder->context_weights) {
        goto cleanup;
    }

    /* Initialize weights from config */
    for (uint32_t i = 0; i < config->num_sources && i < JEPA_CONTEXT_MAX_SOURCES; i++) {
        encoder->context_weights[i] = config->sources[i].weight;
    }

    /* Create conditioning layers based on type */
    switch (config->conditioning) {
        case JEPA_COND_FILM:
            rc = film_layer_create(
                &encoder->conditioning.film,
                config->context_dim,
                config->input_dim,
                config->film_hidden_dim
            );
            if (rc != NIMCP_SUCCESS) {
                NIMCP_LOGGING_ERROR("Failed to create FiLM layer");
                goto cleanup;
            }
            break;

        case JEPA_COND_CROSS_ATTENTION:
            rc = cross_attention_create(
                &encoder->conditioning.cross_attn,
                config->input_dim,
                config->context_dim,
                config->num_attention_heads,
                config->attention_dim
            );
            if (rc != NIMCP_SUCCESS) {
                NIMCP_LOGGING_ERROR("Failed to create cross-attention");
                goto cleanup;
            }
            break;

        case JEPA_COND_ADDITIVE:
            encoder->conditioning.additive_proj = (float*)nimcp_calloc(
                config->context_dim * config->input_dim, sizeof(float));
            if (!encoder->conditioning.additive_proj) {
                goto cleanup;
            }
            /* Xavier initialization */
            {
                float scale = sqrtf(2.0f / (float)(config->context_dim + config->input_dim));
                for (uint32_t i = 0; i < config->context_dim * config->input_dim; i++) {
                    encoder->conditioning.additive_proj[i] =
                        ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
                }
            }
            break;

        case JEPA_COND_GATED:
            encoder->conditioning.gate_weights = (float*)nimcp_calloc(
                config->context_dim * config->input_dim, sizeof(float));
            if (!encoder->conditioning.gate_weights) {
                goto cleanup;
            }
            /* Xavier initialization */
            {
                float scale = sqrtf(2.0f / (float)(config->context_dim + config->input_dim));
                for (uint32_t i = 0; i < config->context_dim * config->input_dim; i++) {
                    encoder->conditioning.gate_weights[i] =
                        ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
                }
            }
            break;

        case JEPA_COND_CONCATENATE:
        case JEPA_COND_MULTIPLICATIVE:
            /* These don't need learned parameters */
            break;
    }

    /* Allocate working buffers */
    uint32_t max_dim = config->input_dim > config->output_dim ?
                       config->input_dim : config->output_dim;
    max_dim = max_dim > config->context_dim ? max_dim : config->context_dim;

    encoder->input_buffer = (float*)nimcp_calloc(max_dim, sizeof(float));
    encoder->output_buffer = (float*)nimcp_calloc(max_dim, sizeof(float));
    encoder->temp_buffer = (float*)nimcp_calloc(max_dim, sizeof(float));

    if (!encoder->input_buffer || !encoder->output_buffer || !encoder->temp_buffer) {
        goto cleanup;
    }

    /* Mark as initialized */
    encoder->base.bridge_active = true;

    NIMCP_LOGGING_INFO("Created context encoder: context_dim=%u, conditioning=%d",
        config->context_dim, config->conditioning);

    return encoder;

cleanup:
    jepa_context_encoder_destroy(encoder);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_context_encoder_create: operation failed");
    return NULL;
}

void jepa_context_encoder_destroy(jepa_context_encoder_t* encoder)
{
    if (!encoder) {
        return;
    }

    /* Disconnect from bio-async */
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_encoder_destroy", 0.0f);


    jepa_context_disconnect_bio_async(encoder);

    /* Free context state */
    jepa_context_state_destroy(encoder->current_context);

    /* Free context buffers */
    nimcp_free(encoder->composed_context);
    nimcp_free(encoder->context_weights);

    /* Free conditioning layers */
    switch (encoder->config.conditioning) {
        case JEPA_COND_FILM:
            film_layer_destroy(encoder->conditioning.film);
            break;
        case JEPA_COND_CROSS_ATTENTION:
            cross_attention_destroy(encoder->conditioning.cross_attn);
            break;
        case JEPA_COND_ADDITIVE:
            nimcp_free(encoder->conditioning.additive_proj);
            break;
        case JEPA_COND_GATED:
            nimcp_free(encoder->conditioning.gate_weights);
            break;
        default:
            break;
    }

    /* Free working buffers */
    nimcp_free(encoder->input_buffer);
    nimcp_free(encoder->output_buffer);
    nimcp_free(encoder->temp_buffer);

    /* Cleanup bridge base (mutex, etc.) */
    bridge_base_cleanup(&encoder->base);

    /* Free encoder */
    nimcp_free(encoder);
    encoder = NULL;
}

int jepa_context_encoder_reset(jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_encoder_reset", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");

    /* Clear context */
    jepa_context_clear(encoder);

    /* Reset stats */
    memset(&encoder->stats, 0, sizeof(encoder->stats));

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Context State API
 * ============================================================================ */

jepa_context_state_t* jepa_context_state_create(void)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_state_create", 0.0f);


    jepa_context_state_t* state = (jepa_context_state_t*)nimcp_calloc(
        1, sizeof(jepa_context_state_t));
    return state;
}

void jepa_context_state_destroy(jepa_context_state_t* state)
{
    if (!state) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_state_destroy", 0.0f);


    nimcp_free(state->task.goal_embedding);
    nimcp_free(state->task.task_embedding);
    nimcp_free(state->working_memory.recent_items);
    nimcp_free(state->working_memory.aggregated);
    nimcp_free(state->attention.attention_weights);
    nimcp_free(state->attention.attended_features);
    nimcp_free(state->temporal_context);
    nimcp_free(state->custom_context);
    nimcp_free(state);
    state = NULL;
}

int jepa_context_set_task(
    jepa_context_encoder_t* encoder,
    const float* goal_embedding,
    uint32_t goal_dim,
    const float* task_embedding,
    uint32_t task_dim)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_set_task", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");
    NIMCP_CHECK_THROW(goal_dim == 0 || goal_embedding, NIMCP_ERROR_NULL_POINTER, "goal_embedding is NULL but goal_dim > 0");
    NIMCP_CHECK_THROW(task_dim == 0 || task_embedding, NIMCP_ERROR_NULL_POINTER, "task_embedding is NULL but task_dim > 0");

    jepa_task_state_t* task = &encoder->current_context->task;

    /* Set goal */
    if (goal_embedding && goal_dim > 0) {
        nimcp_free(task->goal_embedding);
        task->goal_embedding = (float*)nimcp_calloc(goal_dim, sizeof(float));
        if (!task->goal_embedding) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(task->goal_embedding, goal_embedding, goal_dim * sizeof(float));
        task->goal_dim = goal_dim;
    }

    /* Set task */
    if (task_embedding && task_dim > 0) {
        nimcp_free(task->task_embedding);
        task->task_embedding = (float*)nimcp_calloc(task_dim, sizeof(float));
        if (!task->task_embedding) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(task->task_embedding, task_embedding, task_dim * sizeof(float));
        task->task_dim = task_dim;
    }

    encoder->stats.context_updates++;

    return NIMCP_SUCCESS;
}

int jepa_context_set_working_memory(
    jepa_context_encoder_t* encoder,
    const float* wm_items,
    uint32_t item_dim,
    uint32_t num_items)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_set_working_memory", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");
    NIMCP_CHECK_THROW(!(num_items > 0 && item_dim > 0) || wm_items,
                      NIMCP_ERROR_NULL_POINTER, "wm_items is NULL with non-zero dimensions");

    jepa_wm_context_t* wm = &encoder->current_context->working_memory;

    if (wm_items && item_dim > 0 && num_items > 0) {
        uint32_t total_size = item_dim * num_items;

        nimcp_free(wm->recent_items);
        wm->recent_items = (float*)nimcp_calloc(total_size, sizeof(float));
        if (!wm->recent_items) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(wm->recent_items, wm_items, total_size * sizeof(float));
        wm->item_dim = item_dim;
        wm->num_items = num_items;

        /* Compute aggregated representation (mean pooling) */
        nimcp_free(wm->aggregated);
        wm->aggregated = (float*)nimcp_calloc(item_dim, sizeof(float));
        if (!wm->aggregated) {
            return NIMCP_ERROR_NO_MEMORY;
        }

        for (uint32_t i = 0; i < num_items; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_items > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)num_items);
            }

            for (uint32_t j = 0; j < item_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && item_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(j + 1) / (float)item_dim);
                }

                wm->aggregated[j] += wm_items[i * item_dim + j];
            }
        }
        for (uint32_t j = 0; j < item_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && item_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(j + 1) / (float)item_dim);
            }

            wm->aggregated[j] /= (float)num_items;
        }
        wm->aggregated_dim = item_dim;
    }

    encoder->stats.context_updates++;

    return NIMCP_SUCCESS;
}

int jepa_context_set_attention(
    jepa_context_encoder_t* encoder,
    const float* attention_weights,
    uint32_t num_locations,
    const float* attended_features,
    uint32_t feature_dim)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_set_attention", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");
    NIMCP_CHECK_THROW(num_locations == 0 || attention_weights,
                      NIMCP_ERROR_NULL_POINTER, "attention_weights is NULL with num_locations > 0");
    NIMCP_CHECK_THROW(feature_dim == 0 || attended_features,
                      NIMCP_ERROR_NULL_POINTER, "attended_features is NULL with feature_dim > 0");

    jepa_attention_context_t* attn = &encoder->current_context->attention;

    /* Set attention weights */
    if (attention_weights && num_locations > 0) {
        nimcp_free(attn->attention_weights);
        attn->attention_weights = (float*)nimcp_calloc(num_locations, sizeof(float));
        if (!attn->attention_weights) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(attn->attention_weights, attention_weights,
               num_locations * sizeof(float));
        attn->num_locations = num_locations;

        /* Compute attention entropy */
        attn->entropy = 0.0f;
        for (uint32_t i = 0; i < num_locations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_locations > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)num_locations);
            }

            if (attention_weights[i] > 1e-8f) {
                attn->entropy -= attention_weights[i] * logf(attention_weights[i]);
            }
        }
    }

    /* Set attended features */
    if (attended_features && feature_dim > 0) {
        nimcp_free(attn->attended_features);
        attn->attended_features = (float*)nimcp_calloc(feature_dim, sizeof(float));
        if (!attn->attended_features) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(attn->attended_features, attended_features,
               feature_dim * sizeof(float));
        attn->feature_dim = feature_dim;
    }

    encoder->stats.context_updates++;

    return NIMCP_SUCCESS;
}

int jepa_context_set_custom(
    jepa_context_encoder_t* encoder,
    const float* custom,
    uint32_t dim)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_set_custom", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");
    NIMCP_CHECK_THROW(dim == 0 || custom, NIMCP_ERROR_INVALID_PARAM, "custom is NULL with dim > 0");

    if (custom && dim > 0) {
        nimcp_free(encoder->current_context->custom_context);
        encoder->current_context->custom_context = (float*)nimcp_calloc(
            dim, sizeof(float));
        if (!encoder->current_context->custom_context) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(encoder->current_context->custom_context, custom,
               dim * sizeof(float));
        encoder->current_context->custom_dim = dim;
    }

    encoder->stats.context_updates++;

    return NIMCP_SUCCESS;
}

int jepa_context_clear(jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_clear", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");

    jepa_context_state_t* ctx = encoder->current_context;

    /* Clear task context */
    nimcp_free(ctx->task.goal_embedding);
    nimcp_free(ctx->task.task_embedding);
    memset(&ctx->task, 0, sizeof(ctx->task));

    /* Clear working memory context */
    nimcp_free(ctx->working_memory.recent_items);
    nimcp_free(ctx->working_memory.aggregated);
    memset(&ctx->working_memory, 0, sizeof(ctx->working_memory));

    /* Clear attention context */
    nimcp_free(ctx->attention.attention_weights);
    nimcp_free(ctx->attention.attended_features);
    memset(&ctx->attention, 0, sizeof(ctx->attention));

    /* Clear other contexts */
    nimcp_free(ctx->temporal_context);
    nimcp_free(ctx->custom_context);
    ctx->temporal_context = NULL;
    ctx->custom_context = NULL;
    ctx->temporal_dim = 0;
    ctx->custom_dim = 0;

    /* Clear composed context */
    memset(encoder->composed_context, 0,
           encoder->config.context_dim * sizeof(float));

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int jepa_context_encode(
    jepa_context_encoder_t* encoder,
    const jepa_latent_t* input,
    jepa_latent_t* output)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_encode", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output is NULL");

    int rc = 0;

    /* Compose context from all sources */
    rc = compose_context(encoder);
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    uint32_t input_dim = encoder->config.input_dim;
    uint32_t output_dim = encoder->config.output_dim;

    /* Initialize output latent if needed */
    if (!output->embedding) {
        output->embedding = (float*)nimcp_calloc(output_dim, sizeof(float));
        if (!output->embedding) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        output->latent_dim = output_dim;
    }

    /* Copy input to buffer */
    memcpy(encoder->input_buffer, input->embedding,
           input_dim * sizeof(float));

    /* Apply conditioning based on type */
    switch (encoder->config.conditioning) {
        case JEPA_COND_FILM: {
            /* Compute gamma and beta from context */
            float* gamma = encoder->temp_buffer;
            float* beta = encoder->output_buffer;

            rc = film_layer_forward(
                encoder->conditioning.film,
                encoder->composed_context,
                gamma, beta
            );
            if (rc != NIMCP_SUCCESS) {
                return rc;
            }

            /* Apply: output = gamma * input + beta */
            for (uint32_t i = 0; i < input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)input_dim);
                }

                output->embedding[i] = gamma[i] * encoder->input_buffer[i] + beta[i];
            }

            /* Track modulation strength */
            float mod_strength = 0.0f;
            for (uint32_t i = 0; i < input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)input_dim);
                }

                mod_strength += fabsf(gamma[i] - 1.0f) + fabsf(beta[i]);
            }
            encoder->stats.avg_modulation_strength =
                (encoder->stats.avg_modulation_strength *
                 encoder->stats.encodings_performed + mod_strength / (fabsf(input_dim) > 1e-7f ? input_dim : 1e-7f)) /
                (encoder->stats.encodings_performed + 1);
            break;
        }

        case JEPA_COND_CROSS_ATTENTION: {
            rc = cross_attention_forward(
                encoder->conditioning.cross_attn,
                encoder->input_buffer,
                encoder->composed_context,
                output->embedding
            );
            if (rc != NIMCP_SUCCESS) {
                return rc;
            }
            break;
        }

        case JEPA_COND_ADDITIVE: {
            /* Project context and add to input */
            float* context_proj = encoder->temp_buffer;
            memset(context_proj, 0, input_dim * sizeof(float));

            for (uint32_t j = 0; j < input_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(j + 1) / (float)input_dim);
                }

                for (uint32_t i = 0; i < encoder->config.context_dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && encoder->config.context_dim > 256) {
                        jepa_context_heartbeat("jepa_context_loop",
                                         (float)(i + 1) / (float)encoder->config.context_dim);
                    }

                    context_proj[j] += encoder->composed_context[i] *
                        encoder->conditioning.additive_proj[i * input_dim + j];
                }
            }

            for (uint32_t i = 0; i < input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)input_dim);
                }

                output->embedding[i] = encoder->input_buffer[i] + context_proj[i];
            }
            break;
        }

        case JEPA_COND_MULTIPLICATIVE: {
            /* Element-wise multiplication (context broadcast) */
            for (uint32_t i = 0; i < input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)input_dim);
                }

                uint32_t ctx_idx = i % encoder->config.context_dim;
                output->embedding[i] = encoder->input_buffer[i] *
                    encoder->composed_context[ctx_idx];
            }
            break;
        }

        case JEPA_COND_GATED: {
            /* Compute gate from context */
            float* gate = encoder->temp_buffer;
            memset(gate, 0, input_dim * sizeof(float));

            for (uint32_t j = 0; j < input_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(j + 1) / (float)input_dim);
                }

                for (uint32_t i = 0; i < encoder->config.context_dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && encoder->config.context_dim > 256) {
                        jepa_context_heartbeat("jepa_context_loop",
                                         (float)(i + 1) / (float)encoder->config.context_dim);
                    }

                    gate[j] += encoder->composed_context[i] *
                        encoder->conditioning.gate_weights[i * input_dim + j];
                }
                /* Sigmoid gate */
                gate[j] = 1.0f / (1.0f + expf(-gate[j]));
            }

            /* Apply gate */
            for (uint32_t i = 0; i < input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && input_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)input_dim);
                }

                output->embedding[i] = gate[i] * encoder->input_buffer[i];
            }
            break;
        }

        case JEPA_COND_CONCATENATE: {
            /* Concatenate context to input */
            uint32_t concat_dim = input_dim + encoder->config.context_dim;
            if (output_dim >= concat_dim) {
                memcpy(output->embedding, encoder->input_buffer,
                       input_dim * sizeof(float));
                memcpy(output->embedding + input_dim, encoder->composed_context,
                       encoder->config.context_dim * sizeof(float));
            } else {
                /* Just copy input if output too small */
                memcpy(output->embedding, encoder->input_buffer,
                       output_dim * sizeof(float));
            }
            break;
        }
    }

    /* Layer normalization if enabled */
    if (encoder->config.use_layer_norm) {
        layer_normalize(output->embedding, output_dim);
    }

    /* Set output metadata */
    output->modality = input->modality;
    output->precision = input->precision;
    output->timestamp_ms = input->timestamp_ms;

    /* Update stats */
    encoder->stats.encodings_performed++;

    /* Track context norm */
    float ctx_norm = 0.0f;
    for (uint32_t i = 0; i < encoder->config.context_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && encoder->config.context_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)encoder->config.context_dim);
        }

        ctx_norm += encoder->composed_context[i] * encoder->composed_context[i];
    }
    ctx_norm = sqrtf(ctx_norm);
    encoder->stats.avg_context_norm =
        (encoder->stats.avg_context_norm * (encoder->stats.encodings_performed - 1) +
         ctx_norm) / encoder->stats.encodings_performed;

    return NIMCP_SUCCESS;
}

int jepa_context_encode_batch(
    jepa_context_encoder_t* encoder,
    jepa_latent_t** inputs,
    jepa_latent_t** outputs,
    uint32_t batch_size)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_encode_batch", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(inputs, NIMCP_ERROR_INVALID_PARAM, "inputs is NULL");
    NIMCP_CHECK_THROW(outputs, NIMCP_ERROR_INVALID_PARAM, "outputs is NULL");

    for (uint32_t i = 0; i < batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && batch_size > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)batch_size);
        }

        int rc = jepa_context_encode(encoder, inputs[i], outputs[i]);
        if (rc != NIMCP_SUCCESS) {
            return rc;
        }
    }

    return NIMCP_SUCCESS;
}

int jepa_context_get_composed(
    const jepa_context_encoder_t* encoder,
    float* context,
    uint32_t dim)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_get_composed", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_INVALID_PARAM, "context is NULL");

    uint32_t copy_dim = dim < encoder->config.context_dim ?
                        dim : encoder->config.context_dim;
    memcpy(context, encoder->composed_context, copy_dim * sizeof(float));

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Conditioning Mechanism API
 * ============================================================================ */

int jepa_context_apply_film(
    jepa_context_encoder_t* encoder,
    const float* input,
    const float* context,
    float* output,
    uint32_t dim)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_apply_film", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output is NULL");
    NIMCP_CHECK_THROW(encoder->conditioning.film, NIMCP_ERROR_NOT_INITIALIZED, "FiLM layer not initialized");

    float* gamma = (float*)nimcp_calloc(dim, sizeof(float));
    float* beta = (float*)nimcp_calloc(dim, sizeof(float));

    if (!gamma || !beta) {
        nimcp_free(gamma);
        gamma = NULL;
        nimcp_free(beta);
        beta = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    int rc = film_layer_forward(encoder->conditioning.film, context, gamma, beta);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(gamma);
        gamma = NULL;
        nimcp_free(beta);
        beta = NULL;
        return rc;
    }

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        output[i] = gamma[i] * input[i] + beta[i];
    }

    nimcp_free(gamma);
    gamma = NULL;
    nimcp_free(beta);
    beta = NULL;

    return NIMCP_SUCCESS;
}

int jepa_context_apply_cross_attention(
    jepa_context_encoder_t* encoder,
    const float* input,
    const float* context,
    float* output)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_apply_cross_attentio", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output is NULL");
    NIMCP_CHECK_THROW(encoder->conditioning.cross_attn, NIMCP_ERROR_NOT_INITIALIZED, "cross-attention not initialized");

    return cross_attention_forward(
        encoder->conditioning.cross_attn,
        input, context, output
    );
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int jepa_context_get_stats(
    const jepa_context_encoder_t* encoder,
    jepa_context_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_get_stats", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &encoder->stats, sizeof(jepa_context_stats_t));
    return NIMCP_SUCCESS;
}

int jepa_context_reset_stats(jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");

    memset(&encoder->stats, 0, sizeof(encoder->stats));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int jepa_context_connect_bio_async(jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");

    encoder->base.bio_async_enabled = true;
    NIMCP_LOGGING_DEBUG("Connected to bio-async router");

    return NIMCP_SUCCESS;
}

int jepa_context_disconnect_bio_async(jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");

    encoder->base.bio_async_enabled = false;
    return NIMCP_SUCCESS;
}

bool jepa_context_is_bio_async_connected(const jepa_context_encoder_t* encoder)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_is_bio_async_connect", 0.0f);


    return encoder && encoder->base.bio_async_enabled;
}

/* ============================================================================
 * Internal Functions - Context Composition
 * ============================================================================ */

static int compose_context(jepa_context_encoder_t* encoder)
{
    NIMCP_CHECK_THROW(encoder, NIMCP_ERROR_INVALID_PARAM, "encoder is NULL");
    NIMCP_CHECK_THROW(encoder->current_context, NIMCP_ERROR_INVALID_PARAM, "encoder->current_context is NULL");

    uint32_t context_dim = encoder->config.context_dim;
    jepa_context_state_t* ctx = encoder->current_context;

    /* Clear composed context */
    memset(encoder->composed_context, 0, context_dim * sizeof(float));

    float total_weight = 0.0f;
    uint32_t write_idx = 0;

    /* Aggregate context from enabled sources */
    for (uint32_t s = 0; s < encoder->config.num_sources && s < JEPA_CONTEXT_MAX_SOURCES; s++) {
        const jepa_context_source_config_t* src = &encoder->config.sources[s];
        if (!src->enabled) continue;

        float weight = encoder->context_weights[s];
        const float* source_data = NULL;
        uint32_t source_dim = 0;

        switch (src->type) {
            case JEPA_CTX_TASK:
                if (ctx->task.goal_embedding) {
                    source_data = ctx->task.goal_embedding;
                    source_dim = ctx->task.goal_dim;
                } else if (ctx->task.task_embedding) {
                    source_data = ctx->task.task_embedding;
                    source_dim = ctx->task.task_dim;
                }
                break;

            case JEPA_CTX_WORKING_MEMORY:
                if (ctx->working_memory.aggregated) {
                    source_data = ctx->working_memory.aggregated;
                    source_dim = ctx->working_memory.aggregated_dim;
                }
                break;

            case JEPA_CTX_ATTENTION:
                if (ctx->attention.attended_features && ctx->attention.attention_weights) {
                    /* Apply attention weights to features */
                    uint32_t feat_dim = ctx->attention.feature_dim;
                    uint32_t num_locs = ctx->attention.num_locations;

                    /* Compute weighted features (features * attention_weight sum) */
                    float* weighted_feats = (float*)nimcp_calloc(feat_dim, sizeof(float));
                    if (weighted_feats) {
                        float total_attn = 0.0f;
                        for (uint32_t loc = 0; loc < num_locs; loc++) {
                            /* Phase 8: Loop progress heartbeat */
                            if ((loc & 0xFF) == 0 && num_locs > 256) {
                                jepa_context_heartbeat("jepa_context_loop",
                                                 (float)(loc + 1) / (float)num_locs);
                            }

                            total_attn += ctx->attention.attention_weights[loc];
                        }

                        /* Weight features by attention distribution entropy/sharpness */
                        float attn_strength = 1.0f - ctx->attention.entropy / logf((float)num_locs + 1.0f);
                        if (attn_strength < 0.0f) attn_strength = 0.0f;
                        if (attn_strength > 1.0f) attn_strength = 1.0f;

                        /* Modulate features by attention strength */
                        for (uint32_t f = 0; f < feat_dim; f++) {
                            /* Phase 8: Loop progress heartbeat */
                            if ((f & 0xFF) == 0 && feat_dim > 256) {
                                jepa_context_heartbeat("jepa_context_loop",
                                                 (float)(f + 1) / (float)feat_dim);
                            }

                            weighted_feats[f] = ctx->attention.attended_features[f] *
                                               (0.5f + 0.5f * attn_strength);
                        }

                        source_data = weighted_feats;
                        source_dim = feat_dim;

                        /* Add weighted contribution */
                        if (source_dim > 0) {
                            uint32_t copy_dim = source_dim;
                            if (write_idx + copy_dim > context_dim) {
                                copy_dim = context_dim - write_idx;
                            }
                            for (uint32_t i = 0; i < copy_dim; i++) {
                                /* Phase 8: Loop progress heartbeat */
                                if ((i & 0xFF) == 0 && copy_dim > 256) {
                                    jepa_context_heartbeat("jepa_context_loop",
                                                     (float)(i + 1) / (float)copy_dim);
                                }

                                encoder->composed_context[write_idx + i] +=
                                    weight * source_data[i];
                            }
                            write_idx += copy_dim;
                            total_weight += weight;
                        }

                        nimcp_free(weighted_feats);
                        weighted_feats = NULL;
                        source_data = NULL;  /* Already added */
                    }
                } else if (ctx->attention.attended_features) {
                    source_data = ctx->attention.attended_features;
                    source_dim = ctx->attention.feature_dim;
                }
                break;

            case JEPA_CTX_TEMPORAL:
                if (ctx->temporal_context) {
                    source_data = ctx->temporal_context;
                    source_dim = ctx->temporal_dim;
                }
                break;

            case JEPA_CTX_CUSTOM:
                if (ctx->custom_context) {
                    source_data = ctx->custom_context;
                    source_dim = ctx->custom_dim;
                }
                break;

            default:
                break;
        }

        /* Add source contribution */
        if (source_data && source_dim > 0) {
            uint32_t copy_dim = source_dim;
            if (write_idx + copy_dim > context_dim) {
                copy_dim = context_dim - write_idx;
            }

            for (uint32_t i = 0; i < copy_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && copy_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)copy_dim);
                }

                encoder->composed_context[write_idx + i] +=
                    weight * source_data[i];
            }

            write_idx += copy_dim;
            total_weight += weight;
        }
    }

    /* Normalize by total weight */
    if (total_weight > 0.0f) {
        for (uint32_t i = 0; i < context_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && context_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)context_dim);
            }

            encoder->composed_context[i] /= total_weight;
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Functions - FiLM Layer
 * ============================================================================ */

static int film_layer_create(
    jepa_film_layer_t** film,
    uint32_t context_dim,
    uint32_t feature_dim,
    uint32_t hidden_dim)
{
    NIMCP_CHECK_THROW(film, NIMCP_ERROR_INVALID_PARAM, "film output pointer is NULL");

    jepa_film_layer_t* f = (jepa_film_layer_t*)nimcp_calloc(1, sizeof(jepa_film_layer_t));
    if (!f) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    f->context_dim = context_dim;
    f->feature_dim = feature_dim;
    f->hidden_dim = hidden_dim;

    /* Allocate hidden layer if used */
    if (hidden_dim > 0) {
        f->hidden_weights = (float*)nimcp_calloc(context_dim * hidden_dim, sizeof(float));
        f->hidden_bias = (float*)nimcp_calloc(hidden_dim, sizeof(float));
        if (!f->hidden_weights || !f->hidden_bias) {
            film_layer_destroy(f);
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Xavier init */
        float scale = sqrtf(2.0f / (float)(context_dim + hidden_dim));
        for (uint32_t i = 0; i < context_dim * hidden_dim; i++) {
            f->hidden_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
        }
    }

    /* Allocate gamma network */
    uint32_t gamma_input = hidden_dim > 0 ? hidden_dim : context_dim;
    f->gamma_weights = (float*)nimcp_calloc(gamma_input * feature_dim, sizeof(float));
    f->gamma_bias = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!f->gamma_weights || !f->gamma_bias) {
        film_layer_destroy(f);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Allocate beta network */
    f->beta_weights = (float*)nimcp_calloc(gamma_input * feature_dim, sizeof(float));
    f->beta_bias = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!f->beta_weights || !f->beta_bias) {
        film_layer_destroy(f);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Xavier initialization for gamma/beta */
    float scale = sqrtf(2.0f / (float)(gamma_input + feature_dim));
    for (uint32_t i = 0; i < gamma_input * feature_dim; i++) {
        f->gamma_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
        f->beta_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
    }

    /* Initialize gamma bias to 1 (no scaling initially) */
    for (uint32_t i = 0; i < feature_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && feature_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)feature_dim);
        }

        f->gamma_bias[i] = 1.0f;
        f->beta_bias[i] = 0.0f;
    }

    *film = f;
    return NIMCP_SUCCESS;
}

static void film_layer_destroy(jepa_film_layer_t* film)
{
    if (!film) return;

    nimcp_free(film->hidden_weights);
    nimcp_free(film->hidden_bias);
    nimcp_free(film->gamma_weights);
    nimcp_free(film->gamma_bias);
    nimcp_free(film->beta_weights);
    nimcp_free(film->beta_bias);
    nimcp_free(film);
    film = NULL;
}

static int film_layer_forward(
    const jepa_film_layer_t* film,
    const float* context,
    float* gamma,
    float* beta)
{
    NIMCP_CHECK_THROW(film, NIMCP_ERROR_INVALID_PARAM, "film is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(gamma, NIMCP_ERROR_INVALID_PARAM, "gamma is NULL");
    NIMCP_CHECK_THROW(beta, NIMCP_ERROR_INVALID_PARAM, "beta is NULL");

    const float* features = context;
    uint32_t feature_input_dim = film->context_dim;

    /* Apply hidden layer if present */
    float* hidden = NULL;
    if (film->hidden_dim > 0) {
        hidden = (float*)nimcp_calloc(film->hidden_dim, sizeof(float));
        if (!hidden) {
            return NIMCP_ERROR_NO_MEMORY;
        }

        for (uint32_t j = 0; j < film->hidden_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && film->hidden_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(j + 1) / (float)film->hidden_dim);
            }

            float sum = film->hidden_bias[j];
            for (uint32_t i = 0; i < film->context_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && film->context_dim > 256) {
                    jepa_context_heartbeat("jepa_context_loop",
                                     (float)(i + 1) / (float)film->context_dim);
                }

                sum += context[i] * film->hidden_weights[i * film->hidden_dim + j];
            }
            hidden[j] = gelu_activation(sum);
        }

        features = hidden;
        feature_input_dim = film->hidden_dim;
    }

    /* Compute gamma */
    for (uint32_t j = 0; j < film->feature_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && film->feature_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(j + 1) / (float)film->feature_dim);
        }

        float sum = film->gamma_bias[j];
        for (uint32_t i = 0; i < feature_input_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && feature_input_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)feature_input_dim);
            }

            sum += features[i] * film->gamma_weights[i * film->feature_dim + j];
        }
        gamma[j] = sum;
    }

    /* Compute beta */
    for (uint32_t j = 0; j < film->feature_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && film->feature_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(j + 1) / (float)film->feature_dim);
        }

        float sum = film->beta_bias[j];
        for (uint32_t i = 0; i < feature_input_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && feature_input_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)feature_input_dim);
            }

            sum += features[i] * film->beta_weights[i * film->feature_dim + j];
        }
        beta[j] = sum;
    }

    nimcp_free(hidden);
    hidden = NULL;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Functions - Cross-Attention
 * ============================================================================ */

static int cross_attention_create(
    jepa_cross_attention_t** attn,
    uint32_t input_dim,
    uint32_t context_dim,
    uint32_t num_heads,
    uint32_t head_dim)
{
    NIMCP_CHECK_THROW(attn, NIMCP_ERROR_INVALID_PARAM, "attn output pointer is NULL");

    jepa_cross_attention_t* a = (jepa_cross_attention_t*)nimcp_calloc(
        1, sizeof(jepa_cross_attention_t));
    if (!a) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    a->num_heads = num_heads;
    a->head_dim = head_dim;
    a->input_dim = input_dim;
    a->context_dim = context_dim;
    a->output_dim = input_dim;

    uint32_t proj_dim = num_heads * head_dim;

    /* Q from input */
    a->q_weights = (float*)nimcp_calloc(input_dim * proj_dim, sizeof(float));
    if (!a->q_weights) return -1;
    /* K, V from context */
    a->k_weights = (float*)nimcp_calloc(context_dim * proj_dim, sizeof(float));
    if (!a->k_weights) return -1;
    a->v_weights = (float*)nimcp_calloc(context_dim * proj_dim, sizeof(float));
    if (!a->v_weights) return -1;
    /* Output projection */
    a->o_weights = (float*)nimcp_calloc(proj_dim * input_dim, sizeof(float));

    if (!a->q_weights || !a->k_weights || !a->v_weights || !a->o_weights) {
        cross_attention_destroy(a);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Xavier initialization */
    float scale_q = sqrtf(2.0f / (float)(input_dim + proj_dim));
    float scale_kv = sqrtf(2.0f / (float)(context_dim + proj_dim));
    float scale_o = sqrtf(2.0f / (float)(proj_dim + input_dim));

    for (uint32_t i = 0; i < input_dim * proj_dim; i++) {
        a->q_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale_q;
    }
    for (uint32_t i = 0; i < context_dim * proj_dim; i++) {
        a->k_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale_kv;
        a->v_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale_kv;
    }
    for (uint32_t i = 0; i < proj_dim * input_dim; i++) {
        a->o_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale_o;
    }

    *attn = a;
    return NIMCP_SUCCESS;
}

static void cross_attention_destroy(jepa_cross_attention_t* attn)
{
    if (!attn) return;

    nimcp_free(attn->q_weights);
    nimcp_free(attn->k_weights);
    nimcp_free(attn->v_weights);
    nimcp_free(attn->o_weights);
    nimcp_free(attn);
    attn = NULL;
}

static int cross_attention_forward(
    const jepa_cross_attention_t* attn,
    const float* query,
    const float* context,
    float* output)
{
    NIMCP_CHECK_THROW(attn, NIMCP_ERROR_INVALID_PARAM, "attn is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_INVALID_PARAM, "query is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_INVALID_PARAM, "context is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output is NULL");

    uint32_t proj_dim = attn->num_heads * attn->head_dim;

    /* Allocate projections */
    float* q_proj = (float*)nimcp_calloc(proj_dim, sizeof(float));
    if (!q_proj) return -1;
    float* k_proj = (float*)nimcp_calloc(proj_dim, sizeof(float));
    if (!k_proj) return -1;
    float* v_proj = (float*)nimcp_calloc(proj_dim, sizeof(float));
    float* attn_out = (float*)nimcp_calloc(proj_dim, sizeof(float));

    if (!q_proj || !k_proj || !v_proj || !attn_out) {
        nimcp_free(q_proj);
        q_proj = NULL;
        nimcp_free(k_proj);
        k_proj = NULL;
        nimcp_free(v_proj);
        v_proj = NULL;
        nimcp_free(attn_out);
        attn_out = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Project Q from query */
    for (uint32_t j = 0; j < proj_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && proj_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(j + 1) / (float)proj_dim);
        }

        float sum = 0.0f;
        for (uint32_t i = 0; i < attn->input_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && attn->input_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)attn->input_dim);
            }

            sum += query[i] * attn->q_weights[i * proj_dim + j];
        }
        q_proj[j] = sum;
    }

    /* Project K, V from context */
    for (uint32_t j = 0; j < proj_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && proj_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(j + 1) / (float)proj_dim);
        }

        float sum_k = 0.0f;
        float sum_v = 0.0f;
        for (uint32_t i = 0; i < attn->context_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && attn->context_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)attn->context_dim);
            }

            sum_k += context[i] * attn->k_weights[i * proj_dim + j];
            sum_v += context[i] * attn->v_weights[i * proj_dim + j];
        }
        k_proj[j] = sum_k;
        v_proj[j] = sum_v;
    }

    /* Compute attention (simplified: single position) */
    float scale = 1.0f / sqrtf((float)attn->head_dim);

    /* For each head */
    for (uint32_t h = 0; h < attn->num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && attn->num_heads > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(h + 1) / (float)attn->num_heads);
        }

        uint32_t offset = h * attn->head_dim;

        /* Compute attention score: Q . K */
        float score = 0.0f;
        for (uint32_t d = 0; d < attn->head_dim; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && attn->head_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(d + 1) / (float)attn->head_dim);
            }

            score += q_proj[offset + d] * k_proj[offset + d];
        }
        score *= scale;

        /* Softmax over single key (becomes 1.0) */
        float weight = 1.0f;  /* expf(score) / expf(score) */
        (void)weight;

        /* Apply attention to V */
        for (uint32_t d = 0; d < attn->head_dim; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && attn->head_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(d + 1) / (float)attn->head_dim);
            }

            attn_out[offset + d] = v_proj[offset + d];
        }
    }

    /* Output projection */
    for (uint32_t j = 0; j < attn->output_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && attn->output_dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(j + 1) / (float)attn->output_dim);
        }

        float sum = 0.0f;
        for (uint32_t i = 0; i < proj_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && proj_dim > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)proj_dim);
            }

            sum += attn_out[i] * attn->o_weights[i * attn->output_dim + j];
        }
        output[j] = sum;
    }

    nimcp_free(q_proj);
    q_proj = NULL;
    nimcp_free(k_proj);
    k_proj = NULL;
    nimcp_free(v_proj);
    v_proj = NULL;
    nimcp_free(attn_out);
    attn_out = NULL;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Functions - Math
 * ============================================================================ */

static float gelu_activation(float x)
{
    float x3 = x * x * x;
    float inner = 0.7978845608f * (x + GELU_CONST * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

static void layer_normalize(float* data, uint32_t dim)
{
    if (!data || dim == 0) return;

    float mean = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        mean += data[i];
    }
    mean /= (float)dim;

    float variance = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)dim;

    float std_inv = 1.0f / sqrtf(variance + 1e-5f);
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        data[i] = (data[i] - mean) * std_inv;
    }
}

static void softmax(float* data, uint32_t dim)
{
    if (!data || dim == 0) return;

    float max_val = data[0];
    for (uint32_t i = 1; i < dim; i++) {
        if (data[i] > max_val) max_val = data[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_context_heartbeat("jepa_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        data[i] /= sum;
    }
}

/* ============================================================================
 * KG Self-Awareness API
 * ============================================================================ */

int jepa_context_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_context_heartbeat("jepa_context_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Context");
    if (self) {
        NIMCP_LOGGING_INFO("[%s] Self-knowledge entity: %s (type: %s)",
                          CONTEXT_LOG_TAG, self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_context_heartbeat("jepa_context_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("[%s] Observation[%u]: %s",
                               CONTEXT_LOG_TAG, i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Context");
    if (connections) {
        NIMCP_LOGGING_DEBUG("[%s] Outgoing connections: %u", CONTEXT_LOG_TAG, connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Context");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("[%s] Incoming connections: %u", CONTEXT_LOG_TAG, incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_context_set_instance_health_agent(void* instance,
                                             nimcp_health_agent_t* agent) {
    (void)instance;
    g_jepa_context_health_agent = agent;
}

int jepa_context_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_context_training_begin: NULL argument");
        return -1;
    }
    jepa_context_heartbeat_instance(g_jepa_context_health_agent,
                                     "jepa_context_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int jepa_context_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_context_training_end: NULL argument");
        return -1;
    }
    jepa_context_heartbeat_instance(g_jepa_context_health_agent,
                                     "jepa_context_training_end", 1.0f);
    (void)instance;
    return 0;
}

int jepa_context_training_step(void* instance, uint32_t step) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_context_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_context_heartbeat_instance(g_jepa_context_health_agent,
                                     "jepa_context_training_step", progress);
    (void)instance;
    return 0;
}
