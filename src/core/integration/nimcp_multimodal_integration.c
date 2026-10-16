#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_multimodal_integration.c - Multi-Modal Integration Implementation
//=============================================================================

#include "core/integration/nimcp_multimodal_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "multimodal_integration"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multimodal_integration)

#define BIO_MODULE_ID 0x0132


//=============================================================================
// Internal Structure
//=============================================================================

struct multimodal_integration_struct {
    // Thread safety
    nimcp_mutex_t* mutex;

    // Configuration
    multimodal_config_t config;

    // Integration weights (for LEARNED method)
    float* visual_weights;   // [visual_dim × output_dim]
    float* audio_weights;    // [audio_dim × output_dim]
    float* speech_weights;   // [speech_dim × output_dim] - Phase 8.8
    float* direct_weights;   // [direct_dim × output_dim]

    // Attention weights (dynamic)
    float visual_attention;
    float audio_attention;
    float speech_attention;  // Phase 8.8
    float direct_attention;

    // Output buffer
    float* output_buffer;

    // Statistics
    uint64_t num_integrations;
};

//=============================================================================
// Lifecycle
//=============================================================================

multimodal_integration_t multimodal_integration_create(const multimodal_config_t* config)
{
    if (!config || config->output_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_integration_create: config is NULL");
        return NULL;
    }

    multimodal_integration_t integration = nimcp_calloc(1, sizeof(struct multimodal_integration_struct));
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_integration_create: allocation failed");
        return NULL;
    }

    // Create mutex for thread safety
    integration->mutex = nimcp_mutex_create(NULL);
    if (!integration->mutex) {
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_integration_create: mutex allocation failed");
        return NULL;
    }

    // Copy configuration
    integration->config = *config;

    // Initialize attention weights
    integration->visual_attention = config->visual_weight;
    integration->audio_attention = config->audio_weight;
    integration->speech_attention = config->speech_weight;  // Phase 8.8
    integration->direct_attention = config->direct_weight;

    // Normalize attention weights
    float total = integration->visual_attention + integration->audio_attention +
                  integration->speech_attention + integration->direct_attention;
    if (total > 0.0F) {
        integration->visual_attention /= total;
        integration->audio_attention /= total;
        integration->speech_attention /= total;
        integration->direct_attention /= total;
    }

    // Allocate output buffer
    integration->output_buffer = nimcp_calloc(config->output_dim, sizeof(float));
    if (!integration->output_buffer) {
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_integration_create: integration->output_buffer is NULL");
        return NULL;
    }

    // Allocate learned weights if needed
    if (config->method == INTEGRATION_LEARNED) {
        bool alloc_ok = true;
        if (config->visual_dim > 0) {
            integration->visual_weights = nimcp_calloc(config->visual_dim * config->output_dim, sizeof(float));
            if (!integration->visual_weights) { alloc_ok = false; }
            else {
                float scale = sqrtf(2.0F / (config->visual_dim + config->output_dim));
                for (uint32_t i = 0; i < config->visual_dim * config->output_dim; i++) {
                    integration->visual_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 2.0F * scale;
                }
            }
        }

        if (alloc_ok && config->audio_dim > 0) {
            integration->audio_weights = nimcp_calloc(config->audio_dim * config->output_dim, sizeof(float));
            if (!integration->audio_weights) { alloc_ok = false; }
            else {
                float scale = sqrtf(2.0F / (config->audio_dim + config->output_dim));
                for (uint32_t i = 0; i < config->audio_dim * config->output_dim; i++) {
                    integration->audio_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 2.0F * scale;
                }
            }
        }

        if (alloc_ok && config->speech_dim > 0) {
            integration->speech_weights = nimcp_calloc(config->speech_dim * config->output_dim, sizeof(float));
            if (!integration->speech_weights) { alloc_ok = false; }
            else {
                float scale = sqrtf(2.0F / (config->speech_dim + config->output_dim));
                for (uint32_t i = 0; i < config->speech_dim * config->output_dim; i++) {
                    integration->speech_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 2.0F * scale;
                }
            }
        }

        if (alloc_ok && config->direct_dim > 0) {
            integration->direct_weights = nimcp_calloc(config->direct_dim * config->output_dim, sizeof(float));
            if (!integration->direct_weights) { alloc_ok = false; }
            else {
                float scale = sqrtf(2.0F / (config->direct_dim + config->output_dim));
                for (uint32_t i = 0; i < config->direct_dim * config->output_dim; i++) {
                    integration->direct_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 2.0F * scale;
                }
            }
        }

        if (!alloc_ok) {
            multimodal_integration_destroy(integration);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_integration_create: weight allocation failed");
            return NULL;
        }
    }

    return integration;
}

void multimodal_integration_destroy(multimodal_integration_t integration)
{
    if (!integration) return;

    nimcp_mutex_free(integration->mutex);
    nimcp_free(integration->output_buffer);
    nimcp_free(integration->visual_weights);
    nimcp_free(integration->audio_weights);
    nimcp_free(integration->speech_weights);
    nimcp_free(integration->direct_weights);
    nimcp_free(integration);
}

//=============================================================================
// Integration Methods
//=============================================================================

static bool integrate_concatenate(
    multimodal_integration_t integration,
    const multimodal_input_t* input,
    float* output)
{
    uint32_t offset = 0;

    // Copy visual features
    if (input->visual_features && input->visual_dim > 0) {
        memcpy(output + offset, input->visual_features, input->visual_dim * sizeof(float));
        offset += input->visual_dim;
    }

    // Copy audio features
    if (input->audio_features && input->audio_dim > 0) {
        memcpy(output + offset, input->audio_features, input->audio_dim * sizeof(float));
        offset += input->audio_dim;
    }

    // Copy speech features (Phase 8.8)
    if (input->speech_features && input->speech_dim > 0) {
        memcpy(output + offset, input->speech_features, input->speech_dim * sizeof(float));
        offset += input->speech_dim;
    }

    // Copy direct features
    if (input->direct_features && input->direct_dim > 0) {
        memcpy(output + offset, input->direct_features, input->direct_dim * sizeof(float));
        offset += input->direct_dim;
    }

    // Pad remaining with zeros
    while (offset < integration->config.output_dim) {
        output[offset++] = 0.0F;
    }

    return true;
}

static bool integrate_attention(
    multimodal_integration_t integration,
    const multimodal_input_t* input,
    float* output)
{
    // Zero output
    memset(output, 0, integration->config.output_dim * sizeof(float));

    uint32_t offset = 0;

    // Add weighted visual features
    if (input->visual_features && input->visual_dim > 0) {
        for (uint32_t i = 0; i < input->visual_dim && offset < integration->config.output_dim; i++) {
            output[offset++] = input->visual_features[i] * integration->visual_attention;
        }
    }

    // Add weighted audio features
    if (input->audio_features && input->audio_dim > 0) {
        for (uint32_t i = 0; i < input->audio_dim && offset < integration->config.output_dim; i++) {
            output[offset++] = input->audio_features[i] * integration->audio_attention;
        }
    }

    // Add weighted speech features (Phase 8.8)
    if (input->speech_features && input->speech_dim > 0) {
        for (uint32_t i = 0; i < input->speech_dim && offset < integration->config.output_dim; i++) {
            output[offset++] = input->speech_features[i] * integration->speech_attention;
        }
    }

    // Add weighted direct features
    if (input->direct_features && input->direct_dim > 0) {
        for (uint32_t i = 0; i < input->direct_dim && offset < integration->config.output_dim; i++) {
            output[offset++] = input->direct_features[i] * integration->direct_attention;
        }
    }

    return true;
}

static bool integrate_learned(
    multimodal_integration_t integration,
    const multimodal_input_t* input,
    float* output)
{
    // Zero output
    memset(output, 0, integration->config.output_dim * sizeof(float));

    // Apply learned projection: output = W_v·v + W_a·a + W_d·d
    if (input->visual_features && input->visual_dim > 0 && integration->visual_weights) {
        for (uint32_t i = 0; i < integration->config.output_dim; i++) {
            float sum = 0.0F;
            for (uint32_t j = 0; j < input->visual_dim; j++) {
                sum += integration->visual_weights[j * integration->config.output_dim + i] *
                       input->visual_features[j];
            }
            output[i] += sum * integration->visual_attention;
        }
    }

    if (input->audio_features && input->audio_dim > 0 && integration->audio_weights) {
        for (uint32_t i = 0; i < integration->config.output_dim; i++) {
            float sum = 0.0F;
            for (uint32_t j = 0; j < input->audio_dim; j++) {
                sum += integration->audio_weights[j * integration->config.output_dim + i] *
                       input->audio_features[j];
            }
            output[i] += sum * integration->audio_attention;
        }
    }

    // Phase 8.8: Speech learned projection
    if (input->speech_features && input->speech_dim > 0 && integration->speech_weights) {
        for (uint32_t i = 0; i < integration->config.output_dim; i++) {
            float sum = 0.0F;
            for (uint32_t j = 0; j < input->speech_dim; j++) {
                sum += integration->speech_weights[j * integration->config.output_dim + i] *
                       input->speech_features[j];
            }
            output[i] += sum * integration->speech_attention;
        }
    }

    if (input->direct_features && input->direct_dim > 0 && integration->direct_weights) {
        for (uint32_t i = 0; i < integration->config.output_dim; i++) {
            float sum = 0.0F;
            for (uint32_t j = 0; j < input->direct_dim; j++) {
                sum += integration->direct_weights[j * integration->config.output_dim + i] *
                       input->direct_features[j];
            }
            output[i] += sum * integration->direct_attention;
        }
    }

    return true;
}

//=============================================================================
// Public API
//=============================================================================

bool multimodal_integrate(
    multimodal_integration_t integration,
    const multimodal_input_t* input,
    float* output)
{
    if (!integration || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_integrate: required parameter is NULL (integration, input, output)");
        return false;
    }

    nimcp_mutex_lock(integration->mutex);

    bool success = false;

    switch (integration->config.method) {
        case INTEGRATION_CONCATENATE:
            success = integrate_concatenate(integration, input, output);
            break;
        case INTEGRATION_ATTENTION:
            success = integrate_attention(integration, input, output);
            break;
        case INTEGRATION_LEARNED:
            success = integrate_learned(integration, input, output);
            break;
        default:
            nimcp_mutex_unlock(integration->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_integrate: operation failed");
            return false;
    }

    if (success) {
        integration->num_integrations++;
    }

    nimcp_mutex_unlock(integration->mutex);

    return success;
}

bool multimodal_get_attention(
    const multimodal_integration_t integration,
    float* visual_attn,
    float* audio_attn,
    float* speech_attn,
    float* direct_attn)
{
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_get_attention: integration is NULL");
        return false;
    }

    nimcp_mutex_lock(integration->mutex);

    if (visual_attn) *visual_attn = integration->visual_attention;
    if (audio_attn) *audio_attn = integration->audio_attention;
    if (speech_attn) *speech_attn = integration->speech_attention;
    if (direct_attn) *direct_attn = integration->direct_attention;

    nimcp_mutex_unlock(integration->mutex);

    return true;
}

bool multimodal_update_weights(
    multimodal_integration_t integration,
    float reward,
    float learning_rate)
{
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_update_weights: integration is NULL");
        return false;
    }

    nimcp_mutex_lock(integration->mutex);

    // Update attention weights using simple gradient ascent
    // Increase weight of modalities that led to good outcomes
    float visual_grad = reward * learning_rate;
    float audio_grad = reward * learning_rate;
    float speech_grad = reward * learning_rate;
    float direct_grad = reward * learning_rate;

    integration->visual_attention += visual_grad;
    integration->audio_attention += audio_grad;
    integration->speech_attention += speech_grad;
    integration->direct_attention += direct_grad;

    // Re-normalize
    float total = integration->visual_attention + integration->audio_attention +
                  integration->speech_attention + integration->direct_attention;
    if (total > 0.0F) {
        integration->visual_attention /= total;
        integration->audio_attention /= total;
        integration->speech_attention /= total;
        integration->direct_attention /= total;
    }

    // Clamp to [0, 1]
    integration->visual_attention = fmaxf(0.0F, fminf(1.0F, integration->visual_attention));
    integration->audio_attention = fmaxf(0.0F, fminf(1.0F, integration->audio_attention));
    integration->speech_attention = fmaxf(0.0F, fminf(1.0F, integration->speech_attention));
    integration->direct_attention = fmaxf(0.0F, fminf(1.0F, integration->direct_attention));

    nimcp_mutex_unlock(integration->mutex);

    return true;
}

//=============================================================================
// Helpers
//=============================================================================

multimodal_config_t multimodal_default_config(
    uint32_t visual_dim,
    uint32_t audio_dim,
    uint32_t speech_dim,
    uint32_t direct_dim)
{
    multimodal_config_t config = {
        .visual_dim = visual_dim,
        .audio_dim = audio_dim,
        .speech_dim = speech_dim,
        .direct_dim = direct_dim,
        .output_dim = visual_dim + audio_dim + speech_dim + direct_dim,
        .method = INTEGRATION_ATTENTION,
        .visual_weight = 0.3F,      // Reduced from 0.4 to accommodate speech
        .audio_weight = 0.3F,       // Reduced from 0.4 to accommodate speech
        .speech_weight = 0.2F,      // Phase 8.8: Speech modality
        .direct_weight = 0.2F
    };
    return config;
}

bool multimodal_validate_input(
    const multimodal_integration_t integration,
    const multimodal_input_t* input)
{
    if (!integration || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_validate_input: required parameter is NULL (integration, input)");
        return false;
    }

    // Check dimensions match
    if (input->visual_dim > 0 && input->visual_dim != integration->config.visual_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_validate_input: validation failed");
        return false;
    }
    if (input->audio_dim > 0 && input->audio_dim != integration->config.audio_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_validate_input: validation failed");
        return false;
    }
    if (input->speech_dim > 0 && input->speech_dim != integration->config.speech_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_validate_input: validation failed");
        return false;
    }
    if (input->direct_dim > 0 && input->direct_dim != integration->config.direct_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_validate_input: validation failed");
        return false;
    }

    // Check at least one input is present
    if (!input->visual_features && !input->audio_features &&
        !input->speech_features && !input->direct_features) return false;

    return true;
}
