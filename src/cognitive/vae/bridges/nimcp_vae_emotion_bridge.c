/**
 * @file nimcp_vae_emotion_bridge.c
 * @brief Implementation of VAE-Emotion bridge for affect modulation
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "cognitive/vae/bridges/nimcp_vae_emotion_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_us(void) {
    return nimcp_time_now_us();
}

/**
 * @brief Map arousal to sampling temperature
 */
static float compute_temperature_from_arousal(const vae_emotion_bridge_t* bridge,
                                               float arousal, float valence) {
    const vae_emotion_temp_config_t* tc = &bridge->config.temp_config;

    /* Base temperature from arousal */
    float t = tc->min_temperature + arousal * (tc->max_temperature - tc->min_temperature);

    /* Valence modulation (negative valence slightly increases variance) */
    float valence_effect = (1.0f - valence) * 0.5f * tc->valence_weight;
    t *= (1.0f + valence_effect);

    /* Apply logarithmic scaling if configured */
    if (tc->use_logarithmic && t > 0.0f) {
        t = tc->min_temperature + logf(1.0f + t - tc->min_temperature);
    }

    /* Clamp to valid range */
    if (t < tc->min_temperature) t = tc->min_temperature;
    if (t > tc->max_temperature) t = tc->max_temperature;

    return t;
}

/**
 * @brief Convert continuous emotion to categorical
 */
static vae_emotion_category_t emotion_to_category(float valence, float arousal) {
    /* Simple emotion space quadrant mapping */
    if (arousal < 0.3f && fabsf(valence) < 0.3f) {
        return VAE_EMOTION_NEUTRAL;
    }

    if (valence > 0.3f) {
        if (arousal > 0.6f) {
            return VAE_EMOTION_SURPRISED;  /* High arousal, positive */
        } else {
            return VAE_EMOTION_HAPPY;      /* Low-med arousal, positive */
        }
    } else if (valence < -0.3f) {
        if (arousal > 0.7f) {
            return VAE_EMOTION_ANGRY;      /* High arousal, negative */
        } else if (arousal > 0.4f) {
            return VAE_EMOTION_FEARFUL;    /* Med arousal, negative */
        } else {
            return VAE_EMOTION_SAD;        /* Low arousal, negative */
        }
    }

    return VAE_EMOTION_NEUTRAL;
}

/**
 * @brief Get default valence/arousal for emotion category
 */
static void category_to_dimensions(vae_emotion_category_t cat, float* valence, float* arousal) {
    switch (cat) {
        case VAE_EMOTION_HAPPY:
            *valence = 0.8f; *arousal = 0.5f;
            break;
        case VAE_EMOTION_SAD:
            *valence = -0.6f; *arousal = 0.2f;
            break;
        case VAE_EMOTION_ANGRY:
            *valence = -0.7f; *arousal = 0.85f;
            break;
        case VAE_EMOTION_FEARFUL:
            *valence = -0.5f; *arousal = 0.7f;
            break;
        case VAE_EMOTION_SURPRISED:
            *valence = 0.3f; *arousal = 0.9f;
            break;
        case VAE_EMOTION_DISGUSTED:
            *valence = -0.6f; *arousal = 0.5f;
            break;
        default:
            *valence = 0.0f; *arousal = 0.3f;
            break;
    }
}

/**
 * @brief Smooth emotion state with exponential moving average
 */
static void smooth_emotion(vae_emotion_state_t* smoothed,
                           const vae_emotion_state_t* current,
                           float alpha) {
    smoothed->valence = alpha * current->valence + (1.0f - alpha) * smoothed->valence;
    smoothed->arousal = alpha * current->arousal + (1.0f - alpha) * smoothed->arousal;
    smoothed->dominance = alpha * current->dominance + (1.0f - alpha) * smoothed->dominance;
    smoothed->intensity = alpha * current->intensity + (1.0f - alpha) * smoothed->intensity;
    smoothed->category = current->category;
    smoothed->timestamp_ms = current->timestamp_ms;
}

/**
 * @brief Create emotion conditioning vector
 */
static int create_emotion_condition(const vae_emotion_bridge_t* bridge,
                                     const vae_emotion_state_t* emotion,
                                     float* condition, uint32_t cond_dim) {
    if (cond_dim < VAE_EMOTION_DIM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_INVALID_STATE, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_INVALID_STATE;
    }

    /* Pack continuous dimensions */
    condition[VAE_EMOTION_DIM_VALENCE] = emotion->valence;
    condition[VAE_EMOTION_DIM_AROUSAL] = emotion->arousal;
    condition[VAE_EMOTION_DIM_DOMINANCE] = emotion->dominance;
    condition[VAE_EMOTION_DIM_INTENSITY] = emotion->intensity;

    /* Add categorical embedding if available */
    if (bridge->config.cond_mode == VAE_EMOTION_COND_CATEGORICAL ||
        bridge->config.cond_mode == VAE_EMOTION_COND_MIXED) {

        if (bridge->emotion_embeddings && cond_dim >= VAE_EMOTION_DIM_COUNT + bridge->embed_dim) {
            uint32_t cat_idx = (uint32_t)emotion->category;
            if (cat_idx < VAE_EMOTION_COUNT) {
                memcpy(&condition[VAE_EMOTION_DIM_COUNT],
                       &bridge->emotion_embeddings[cat_idx * bridge->embed_dim],
                       bridge->embed_dim * sizeof(float));
            }
        }
    }

    return 0;
}

/**
 * @brief Compute emotional coherence between latent and emotion state
 */
static float compute_emotional_coherence(const vae_emotion_bridge_t* bridge,
                                          const float* latent, uint32_t latent_dim,
                                          const vae_emotion_state_t* emotion) {
    if (!bridge->emotion_latent_means[emotion->category]) {
        return 0.5f;  /* No baseline, assume moderate coherence */
    }

    /* Compute distance from emotion-specific latent baseline */
    const float* baseline = bridge->emotion_latent_means[emotion->category];
    uint32_t dim = latent_dim < vae_get_latent_dim(bridge->vae) ?
                   latent_dim : vae_get_latent_dim(bridge->vae);

    float dist_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float d = latent[i] - baseline[i];
        dist_sq += d * d;
    }

    /* Convert distance to coherence [0, 1] */
    float dist = sqrtf(dist_sq);
    float coherence = expf(-dist * 0.5f);

    return coherence;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_emotion_bridge_default_config(vae_emotion_bridge_config_t* config) {
    if (!config) return NIMCP_ERROR_VAE_EMOTION_NULL;

    memset(config, 0, sizeof(*config));

    config->cond_mode = VAE_EMOTION_COND_CONTINUOUS;

    config->temp_config.min_temperature = VAE_EMOTION_MIN_TEMPERATURE;
    config->temp_config.max_temperature = VAE_EMOTION_MAX_TEMPERATURE;
    config->temp_config.valence_weight = 0.2f;
    config->temp_config.use_logarithmic = false;

    config->emotion_embed_dim = 16;
    config->conditioning_strength = 0.5f;

    config->enable_latent_tagging = true;
    config->enable_mood_congruent = true;

    config->emotion_decay_rate = 0.1f;
    config->emotion_smoothing = 0.3f;

    config->enable_logging = false;

    return 0;
}

vae_emotion_bridge_t* vae_emotion_bridge_create(const vae_emotion_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_emotion_bridge_create: config is NULL");
        return NULL;
    }

    vae_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_emotion_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_emotion_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_EMOTION_STATE_DISCONNECTED;
    bridge->creation_time_us = get_time_us();
    bridge->embed_dim = config->emotion_embed_dim;

    /* Allocate emotion embeddings */
    if (config->cond_mode == VAE_EMOTION_COND_CATEGORICAL ||
        config->cond_mode == VAE_EMOTION_COND_MIXED) {

        bridge->emotion_embeddings = nimcp_calloc(VAE_EMOTION_COUNT * config->emotion_embed_dim,
                                                   sizeof(float));
        if (!bridge->emotion_embeddings) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_emotion_bridge_create: bridge->emotion_embeddings is NULL");
            return NULL;
        }

        /* Initialize with random embeddings */
        for (uint32_t i = 0; i < VAE_EMOTION_COUNT * config->emotion_embed_dim; i++) {
            bridge->emotion_embeddings[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }

    /* Initialize neutral emotion state */
    bridge->current_emotion.valence = 0.0f;
    bridge->current_emotion.arousal = 0.3f;
    bridge->current_emotion.dominance = 0.5f;
    bridge->current_emotion.intensity = 0.3f;
    bridge->current_emotion.category = VAE_EMOTION_NEUTRAL;
    bridge->current_emotion.timestamp_ms = (uint64_t)(get_time_us() / 1000);

    bridge->smoothed_emotion = bridge->current_emotion;

    bridge->stats.creation_time_us = bridge->creation_time_us;
    bridge->stats.current_emotion = bridge->current_emotion;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        LOG_INFO("VAE-Emotion bridge created (cond_mode=%d)", config->cond_mode);
    }

    return bridge;
}

void vae_emotion_bridge_destroy(vae_emotion_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->emotion_embeddings) {
        nimcp_free(bridge->emotion_embeddings);
    }

    for (int i = 0; i < VAE_EMOTION_COUNT; i++) {
        if (bridge->emotion_latent_means[i]) {
            nimcp_free(bridge->emotion_latent_means[i]);
        }
    }

    if (bridge->latent_buffer) {
        nimcp_free(bridge->latent_buffer);
    }
    if (bridge->condition_buffer) {
        nimcp_free(bridge->condition_buffer);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_emotion_bridge_connect_vae(vae_emotion_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_EMOTION_NULL;

    bridge->vae = vae;

    uint32_t latent_dim = vae_get_latent_dim(vae);

    /* Allocate working buffers */
    bridge->latent_buffer = nimcp_calloc(latent_dim, sizeof(float));
    if (!bridge->latent_buffer) return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;

    uint32_t cond_dim = VAE_EMOTION_DIM_COUNT + bridge->embed_dim;
    bridge->condition_buffer = nimcp_calloc(cond_dim, sizeof(float));
    if (!bridge->condition_buffer) return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;

    /* Initialize per-emotion latent baselines */
    for (int i = 0; i < VAE_EMOTION_COUNT; i++) {
        bridge->emotion_latent_means[i] = nimcp_calloc(latent_dim, sizeof(float));
        bridge->emotion_sample_counts[i] = 0;
    }

    if (bridge->emotion_system) {
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        LOG_INFO("VAE-Emotion bridge connected to VAE (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_emotion_bridge_connect_emotion(vae_emotion_bridge_t* bridge, void* emotion_system) {
    if (!bridge) return NIMCP_ERROR_VAE_EMOTION_NULL;

    bridge->emotion_system = emotion_system;

    if (bridge->vae) {
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
    }

    return 0;
}

int vae_emotion_bridge_disconnect(vae_emotion_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_EMOTION_NULL;

    bridge->state = VAE_EMOTION_STATE_DISCONNECTED;

    return 0;
}

bool vae_emotion_bridge_is_connected(const vae_emotion_bridge_t* bridge) {
    return bridge && bridge->state == VAE_EMOTION_STATE_CONNECTED;
}

/* ============================================================================
 * Emotion State API
 * ============================================================================ */

int vae_emotion_set_state(vae_emotion_bridge_t* bridge, const vae_emotion_state_t* state) {
    if (!bridge || !state) return NIMCP_ERROR_VAE_EMOTION_NULL;

    bridge->current_emotion = *state;

    /* Apply smoothing */
    smooth_emotion(&bridge->smoothed_emotion, state, bridge->config.emotion_smoothing);

    bridge->stats.current_emotion = bridge->smoothed_emotion;

    return 0;
}

int vae_emotion_get_state(const vae_emotion_bridge_t* bridge, vae_emotion_state_t* state) {
    if (!bridge || !state) return NIMCP_ERROR_VAE_EMOTION_NULL;

    *state = bridge->smoothed_emotion;

    return 0;
}

int vae_emotion_update_from_system(vae_emotion_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_EMOTION_NULL;

    /* In a real implementation, this would query the emotion system */
    /* For now, just update timestamp */
    bridge->current_emotion.timestamp_ms = (uint64_t)(get_time_us() / 1000);

    return 0;
}

int vae_emotion_decay(vae_emotion_bridge_t* bridge, float dt) {
    if (!bridge) return NIMCP_ERROR_VAE_EMOTION_NULL;

    float decay = expf(-bridge->config.emotion_decay_rate * dt);

    /* Decay toward neutral */
    bridge->current_emotion.valence *= decay;
    bridge->current_emotion.arousal = 0.3f + (bridge->current_emotion.arousal - 0.3f) * decay;
    bridge->current_emotion.intensity *= decay;

    /* Update category if needed */
    bridge->current_emotion.category = emotion_to_category(
        bridge->current_emotion.valence,
        bridge->current_emotion.arousal
    );

    bridge->current_emotion.timestamp_ms = (uint64_t)(get_time_us() / 1000);

    /* Apply smoothing */
    smooth_emotion(&bridge->smoothed_emotion, &bridge->current_emotion,
                   bridge->config.emotion_smoothing);

    return 0;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_emotion_encode(vae_emotion_bridge_t* bridge,
                        const float* input, uint32_t input_dim,
                        vae_emotion_encode_result_t* result) {
    return vae_emotion_encode_with_emotion(bridge, input, input_dim,
                                            &bridge->smoothed_emotion, result);
}

int vae_emotion_encode_with_emotion(vae_emotion_bridge_t* bridge,
                                     const float* input, uint32_t input_dim,
                                     const vae_emotion_state_t* emotion,
                                     vae_emotion_encode_result_t* result) {
    if (!bridge || !input || !emotion || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NULL, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NULL;
    }
    if (bridge->state != VAE_EMOTION_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NOT_CONNECTED, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NOT_CONNECTED;
    }

    uint64_t start_time = get_time_us();
    bridge->state = VAE_EMOTION_STATE_ENCODING;

    memset(result, 0, sizeof(*result));

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    /* Allocate output */
    result->latent = nimcp_calloc(latent_dim, sizeof(float));
    if (!result->latent) {
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NO_MEMORY, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;
    }
    result->latent_dim = latent_dim;

    /* Create conditioning vector (for future conditional encoding support) */
    uint32_t cond_dim = VAE_EMOTION_DIM_COUNT + bridge->embed_dim;
    create_emotion_condition(bridge, emotion, bridge->condition_buffer, cond_dim);
    (void)cond_dim; /* Currently unused - conditional encoding not yet supported */

    /* Create input tensor from float array */
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&input_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        nimcp_free(result->latent);
        result->latent = NULL;
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NO_MEMORY, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;
    }

    /* Copy input data to tensor */
    memcpy(TENSOR_DATA_F32(input_tensor), input, input_dim * sizeof(float));

    /* Encode with VAE */
    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        nimcp_free(result->latent);
        result->latent = NULL;
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_ENCODE_FAILED, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_ENCODE_FAILED;
    }

    /* Copy latent (mu) to result */
    memcpy(result->latent, TENSOR_DATA_F32(mu_tensor), latent_dim * sizeof(float));

    /* Clean up tensors */
    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    /* Store emotion tag */
    result->emotion_tag = *emotion;

    /* Compute emotional coherence */
    result->emotional_coherence = compute_emotional_coherence(
        bridge, result->latent, latent_dim, emotion
    );

    /* Update emotion-specific baseline (online mean) */
    uint32_t cat_idx = (uint32_t)emotion->category;
    if (cat_idx < VAE_EMOTION_COUNT && bridge->emotion_latent_means[cat_idx]) {
        float* mean = bridge->emotion_latent_means[cat_idx];
        uint64_t n = ++bridge->emotion_sample_counts[cat_idx];

        for (uint32_t i = 0; i < latent_dim; i++) {
            mean[i] += (result->latent[i] - mean[i]) / (float)n;
        }
    }

    result->encoding_time_us = get_time_us() - start_time;

    bridge->stats.total_encodes++;
    bridge->stats.avg_emotional_coherence =
        (bridge->stats.avg_emotional_coherence * (bridge->stats.total_encodes - 1) +
         result->emotional_coherence) / bridge->stats.total_encodes;
    bridge->stats.per_category_count[cat_idx]++;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_EMOTION_STATE_CONNECTED;

    return 0;
}

/* ============================================================================
 * Generation API
 * ============================================================================ */

int vae_emotion_generate(vae_emotion_bridge_t* bridge,
                          vae_emotion_generate_result_t* result) {
    return vae_emotion_generate_with_emotion(bridge, &bridge->smoothed_emotion, result);
}

int vae_emotion_generate_with_emotion(vae_emotion_bridge_t* bridge,
                                       const vae_emotion_state_t* target_emotion,
                                       vae_emotion_generate_result_t* result) {
    if (!bridge || !target_emotion || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NULL, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NULL;
    }
    if (bridge->state != VAE_EMOTION_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NOT_CONNECTED, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NOT_CONNECTED;
    }

    uint64_t start_time = get_time_us();
    bridge->state = VAE_EMOTION_STATE_GENERATING;

    memset(result, 0, sizeof(*result));

    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);
    uint32_t output_dim = vae_get_output_dim(bridge->vae);

    /* Compute temperature from emotion */
    float temperature = compute_temperature_from_arousal(
        bridge, target_emotion->arousal, target_emotion->valence
    );
    result->temperature_used = temperature;

    /* Sample latent with emotion-based temperature */
    result->latent = nimcp_calloc(latent_dim, sizeof(float));
    if (!result->latent) {
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NO_MEMORY, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;
    }
    result->latent_dim = latent_dim;

    /* Start from emotion-specific mean if available */
    uint32_t cat_idx = (uint32_t)target_emotion->category;
    if (cat_idx < VAE_EMOTION_COUNT && bridge->emotion_latent_means[cat_idx] &&
        bridge->emotion_sample_counts[cat_idx] > 0) {
        memcpy(result->latent, bridge->emotion_latent_means[cat_idx],
               latent_dim * sizeof(float));
    }

    /* Add temperature-scaled noise */
    for (uint32_t i = 0; i < latent_dim; i++) {
        /* Box-Muller transform for Gaussian noise */
        float u1 = (float)nimcp_tl_rand() / RAND_MAX;
        float u2 = (float)nimcp_tl_rand() / RAND_MAX;
        if (u1 < 1e-10f) u1 = 1e-10f;
        float noise = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
        result->latent[i] += temperature * noise;
    }

    /* Decode to output space */
    result->generated = nimcp_calloc(output_dim, sizeof(float));
    if (!result->generated) {
        nimcp_free(result->latent);
        result->latent = NULL;
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NO_MEMORY, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;
    }
    result->generated_dim = output_dim;

    /* Create tensors for decode */
    nimcp_tensor_t* z_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create(&output_dim, 1, NIMCP_DTYPE_F32);

    if (!z_tensor || !output_tensor) {
        nimcp_tensor_destroy(z_tensor);
        nimcp_tensor_destroy(output_tensor);
        nimcp_free(result->generated);
        nimcp_free(result->latent);
        result->generated = NULL;
        result->latent = NULL;
        bridge->state = VAE_EMOTION_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NO_MEMORY, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NO_MEMORY;
    }

    /* Copy latent to tensor */
    memcpy(TENSOR_DATA_F32(z_tensor), result->latent, latent_dim * sizeof(float));

    /* Decode */
    int ret = vae_decode(bridge->vae, z_tensor, output_tensor);

    if (ret == 0) {
        memcpy(result->generated, TENSOR_DATA_F32(output_tensor), output_dim * sizeof(float));
    }

    nimcp_tensor_destroy(z_tensor);
    nimcp_tensor_destroy(output_tensor);

    /* Compute emotion alignment */
    result->target_emotion = *target_emotion;
    result->emotion_alignment = compute_emotional_coherence(
        bridge, result->latent, latent_dim, target_emotion
    );

    result->generation_time_us = get_time_us() - start_time;

    bridge->stats.total_generations++;
    bridge->stats.avg_emotion_alignment =
        (bridge->stats.avg_emotion_alignment * (bridge->stats.total_generations - 1) +
         result->emotion_alignment) / bridge->stats.total_generations;
    bridge->stats.last_operation_us = get_time_us();

    bridge->state = VAE_EMOTION_STATE_CONNECTED;

    return 0;
}

int vae_emotion_generate_toward_category(vae_emotion_bridge_t* bridge,
                                          vae_emotion_category_t category,
                                          float intensity,
                                          vae_emotion_generate_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_EMOTION_NULL;

    /* Convert category to emotion state */
    vae_emotion_state_t target;
    memset(&target, 0, sizeof(target));

    category_to_dimensions(category, &target.valence, &target.arousal);
    target.intensity = intensity;
    target.category = category;
    target.dominance = 0.5f;
    target.timestamp_ms = (uint64_t)(get_time_us() / 1000);

    return vae_emotion_generate_with_emotion(bridge, &target, result);
}

int vae_emotion_interpolate_emotions(vae_emotion_bridge_t* bridge,
                                      const vae_emotion_state_t* emotion_a,
                                      const vae_emotion_state_t* emotion_b,
                                      float t,
                                      vae_emotion_generate_result_t* result) {
    if (!bridge || !emotion_a || !emotion_b || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NULL, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NULL;
    }

    /* Interpolate emotion states */
    vae_emotion_state_t interpolated;
    interpolated.valence = (1.0f - t) * emotion_a->valence + t * emotion_b->valence;
    interpolated.arousal = (1.0f - t) * emotion_a->arousal + t * emotion_b->arousal;
    interpolated.dominance = (1.0f - t) * emotion_a->dominance + t * emotion_b->dominance;
    interpolated.intensity = (1.0f - t) * emotion_a->intensity + t * emotion_b->intensity;
    interpolated.category = emotion_to_category(interpolated.valence, interpolated.arousal);
    interpolated.timestamp_ms = (uint64_t)(get_time_us() / 1000);

    return vae_emotion_generate_with_emotion(bridge, &interpolated, result);
}

/* ============================================================================
 * Temperature Mapping API
 * ============================================================================ */

float vae_emotion_compute_temperature(const vae_emotion_bridge_t* bridge,
                                       const vae_emotion_state_t* emotion) {
    if (!bridge || !emotion) return 1.0f;

    return compute_temperature_from_arousal(bridge, emotion->arousal, emotion->valence);
}

int vae_emotion_set_temp_config(vae_emotion_bridge_t* bridge,
                                 const vae_emotion_temp_config_t* config) {
    if (!bridge || !config) return NIMCP_ERROR_VAE_EMOTION_NULL;

    bridge->config.temp_config = *config;

    return 0;
}

/* ============================================================================
 * Latent Tagging API
 * ============================================================================ */

int vae_emotion_tag_latent(vae_emotion_bridge_t* bridge,
                            const float* latent, uint32_t latent_dim,
                            const vae_emotion_state_t* emotion) {
    if (!bridge || !latent || !emotion) return NIMCP_ERROR_VAE_EMOTION_NULL;

    /* Update emotion-specific baseline */
    uint32_t cat_idx = (uint32_t)emotion->category;
    if (cat_idx < VAE_EMOTION_COUNT && bridge->emotion_latent_means[cat_idx]) {
        float* mean = bridge->emotion_latent_means[cat_idx];
        uint64_t n = ++bridge->emotion_sample_counts[cat_idx];

        uint32_t dim = latent_dim < vae_get_latent_dim(bridge->vae) ?
                       latent_dim : vae_get_latent_dim(bridge->vae);

        for (uint32_t i = 0; i < dim; i++) {
            mean[i] += (latent[i] - mean[i]) / (float)n;
        }
    }

    return 0;
}

int vae_emotion_find_by_emotion(vae_emotion_bridge_t* bridge,
                                 const vae_emotion_state_t* target,
                                 float tolerance,
                                 float* latent, uint32_t* latent_dim) {
    if (!bridge || !target || !latent || !latent_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_EMOTION_NULL, "vae_emotion_bridge: error condition");
        return NIMCP_ERROR_VAE_EMOTION_NULL;
    }

    (void)tolerance;  /* Not used in this simple implementation */

    /* Return emotion-specific mean as best match */
    uint32_t cat_idx = (uint32_t)target->category;
    uint32_t dim = vae_get_latent_dim(bridge->vae);

    if (cat_idx < VAE_EMOTION_COUNT && bridge->emotion_latent_means[cat_idx] &&
        bridge->emotion_sample_counts[cat_idx] > 0) {
        memcpy(latent, bridge->emotion_latent_means[cat_idx], dim * sizeof(float));
        *latent_dim = dim;
        return 0;
    }

    /* No samples for this emotion, return zeros */
    memset(latent, 0, dim * sizeof(float));
    *latent_dim = dim;

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_emotion_bridge_state_t vae_emotion_bridge_get_state(const vae_emotion_bridge_t* bridge) {
    if (!bridge) return VAE_EMOTION_STATE_DISCONNECTED;
    return bridge->state;
}

int vae_emotion_bridge_get_stats(const vae_emotion_bridge_t* bridge,
                                  vae_emotion_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_EMOTION_NULL;

    *stats = bridge->stats;

    return 0;
}

const char* vae_emotion_category_to_string(vae_emotion_category_t category) {
    switch (category) {
        case VAE_EMOTION_NEUTRAL:   return "neutral";
        case VAE_EMOTION_HAPPY:     return "happy";
        case VAE_EMOTION_SAD:       return "sad";
        case VAE_EMOTION_ANGRY:     return "angry";
        case VAE_EMOTION_FEARFUL:   return "fearful";
        case VAE_EMOTION_SURPRISED: return "surprised";
        case VAE_EMOTION_DISGUSTED: return "disgusted";
        default:                    return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_emotion_encode_result_free(vae_emotion_encode_result_t* result) {
    if (!result) return;

    if (result->latent) {
        nimcp_free(result->latent);
        result->latent = NULL;
    }
}

void vae_emotion_generate_result_free(vae_emotion_generate_result_t* result) {
    if (!result) return;

    if (result->generated) {
        nimcp_free(result->generated);
        result->generated = NULL;
    }
    if (result->latent) {
        nimcp_free(result->latent);
        result->latent = NULL;
    }
}
