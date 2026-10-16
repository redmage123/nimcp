#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_positional_encoding.c - Positional Encoding Implementation
//=============================================================================
/**
 * @file nimcp_positional_encoding.c
 * @brief Positional Encoding for Sequence and Spatial Data
 *
 * WHAT: Implementation of sinusoidal, learned, RoPE, and ALiBi encodings
 * WHY:  Enable position-aware neural network processing
 * HOW:  Modular encoders with caching and bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include "utils/encoding/nimcp_positional_encoding.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

//=============================================================================
// Module Configuration
//=============================================================================

#define LOG_MODULE "PosEncoding"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(positional_encoding)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Cache for pre-computed encodings
 */
struct nimcp_pos_cache_s {
    float* data;              /**< Cached encoding data */
    uint32_t cached_length;   /**< Number of positions cached */
    uint32_t dim;             /**< Embedding dimension */
    uint64_t hits;            /**< Cache hit counter */
    uint64_t misses;          /**< Cache miss counter */
};

/**
 * @brief Internal encoder structure
 */
struct nimcp_pos_encoder_s {
    nimcp_pos_encoding_type_t type;
    uint32_t max_seq_length;
    uint32_t embedding_dim;
    bool cache_enabled;
    bool thread_safe;

    /* Type-specific data */
    union {
        struct {
            float frequency_base;
            float frequency_scale;
            float* inv_freq;           /**< Pre-computed 1/freq for each dim */
        } sinusoidal;

        struct {
            float* embeddings;         /**< Learnable embeddings */
            float* gradients;          /**< Accumulated gradients */
            float init_std;
            float learning_rate;
            float weight_decay;
        } learned;

        struct {
            float rope_base;
            float rope_scaling;
            uint32_t rope_dim;
            bool use_ntk_scaling;
            float ntk_factor;
            float* cos_cache;          /**< Cached cos values */
            float* sin_cache;          /**< Cached sin values */
        } rope;

        struct {
            uint32_t num_heads;
            float slope_base;
            bool use_symmetric;
            float* slopes;             /**< Pre-computed slopes per head */
        } alibi;

        struct {
            uint32_t max_relative_pos;
            bool use_clipping;
            float* key_embeddings;     /**< Relative key embeddings */
            float* value_embeddings;   /**< Relative value embeddings */
        } relative;
    } data;

    /* Cache */
    nimcp_pos_cache_t* cache;

    /* Statistics */
    nimcp_pos_stats_t stats;

    /* Thread safety */
    pthread_rwlock_t lock;
};

//=============================================================================
// Helper Functions - Sinusoidal
//=============================================================================

/**
 * @brief Compute inverse frequencies for sinusoidal encoding
 *
 * WHAT: inv_freq[i] = 1 / (base^(2i/d))
 * WHY:  Pre-compute for efficiency
 * HOW:  Exponential scaling across dimensions
 */
static float* compute_inv_frequencies(uint32_t dim, float base, float scale)
{
    float* inv_freq = nimcp_calloc(dim / 2, sizeof(float));
    if (!inv_freq) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inv_freq is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < dim / 2; i++) {
        float exp = (float)(2 * i) / (float)dim;
        inv_freq[i] = scale / powf(base, exp);
    }

    return inv_freq;
}

/**
 * @brief Encode single position with sinusoidal method
 *
 * WHAT: PE(pos, 2i) = sin(pos * inv_freq[i])
 *       PE(pos, 2i+1) = cos(pos * inv_freq[i])
 */
static void encode_sinusoidal_position(
    const float* inv_freq,
    uint32_t dim,
    uint32_t position,
    float* output
)
{
    for (uint32_t i = 0; i < dim / 2; i++) {
        float angle = (float)position * inv_freq[i];
        output[2 * i] = sinf(angle);
        output[2 * i + 1] = cosf(angle);
    }

    /* Handle odd dimension */
    if (dim % 2 == 1) {
        float angle = (float)position * inv_freq[dim / 2 - 1];
        output[dim - 1] = sinf(angle);
    }
}

//=============================================================================
// Helper Functions - RoPE
//=============================================================================

/**
 * @brief Pre-compute RoPE cos/sin cache
 *
 * WHAT: Cache cos(m*theta) and sin(m*theta) for all positions and dims
 * WHY:  Avoid repeated trigonometric computation
 */
static bool precompute_rope_cache(
    nimcp_pos_encoder_t* encoder,
    uint32_t length
)
{
    uint32_t dim = encoder->data.rope.rope_dim;
    if (dim == 0) dim = encoder->embedding_dim;

    /* Allocate cache */
    size_t cache_size = length * (dim / 2) * sizeof(float);
    encoder->data.rope.cos_cache = nimcp_malloc(cache_size);
    encoder->data.rope.sin_cache = nimcp_malloc(cache_size);

    if (!encoder->data.rope.cos_cache || !encoder->data.rope.sin_cache) {
        nimcp_free(encoder->data.rope.cos_cache);
        nimcp_free(encoder->data.rope.sin_cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "precompute_rope_cache: required parameter is NULL (encoder->data, encoder->data)");
        return false;
    }

    float base = encoder->data.rope.rope_base;
    float scaling = encoder->data.rope.rope_scaling;

    /* Apply NTK-aware scaling if enabled */
    if (encoder->data.rope.use_ntk_scaling && length > encoder->max_seq_length) {
        float factor = encoder->data.rope.ntk_factor;
        base = base * powf((float)length / (float)encoder->max_seq_length, factor);
    }

    /* Compute frequencies and cache sin/cos */
    for (uint32_t pos = 0; pos < length; pos++) {
        for (uint32_t i = 0; i < dim / 2; i++) {
            float exp = (float)(2 * i) / (float)dim;
            float freq = 1.0F / (powf(base, exp) * scaling);
            float angle = (float)pos * freq;

            size_t idx = pos * (dim / 2) + i;
            encoder->data.rope.cos_cache[idx] = cosf(angle);
            encoder->data.rope.sin_cache[idx] = sinf(angle);
        }
    }

    return true;
}

/**
 * @brief Apply RoPE rotation to a single vector
 *
 * WHAT: Rotate consecutive pairs by position-dependent angle
 * HOW:  (q0, q1) -> (q0*cos - q1*sin, q0*sin + q1*cos)
 */
static void apply_rope_rotation(
    const float* cos_cache,
    const float* sin_cache,
    const float* input,
    uint32_t dim,
    uint32_t position,
    float* output
)
{
    uint32_t half_dim = dim / 2;
    size_t cache_offset = position * half_dim;

    for (uint32_t i = 0; i < half_dim; i++) {
        float cos_val = cos_cache[cache_offset + i];
        float sin_val = sin_cache[cache_offset + i];
        float x0 = input[2 * i];
        float x1 = input[2 * i + 1];

        output[2 * i] = x0 * cos_val - x1 * sin_val;
        output[2 * i + 1] = x0 * sin_val + x1 * cos_val;
    }

    /* Copy remaining dimensions unchanged */
    if (dim % 2 == 1) {
        output[dim - 1] = input[dim - 1];
    }
}

//=============================================================================
// Helper Functions - ALiBi
//=============================================================================

/**
 * @brief Compute ALiBi slopes for all heads
 *
 * WHAT: slope[h] = 2^(-8 * (h+1) / num_heads)
 * WHY:  Geometric sequence of decay rates
 */
static float* compute_alibi_slopes(uint32_t num_heads, float slope_base)
{
    float* slopes = nimcp_calloc(num_heads, sizeof(float));
    if (!slopes) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "slopes is NULL");

        return NULL;

    }

    float ratio = powf(slope_base, 1.0F / (float)num_heads);
    float slope = ratio;  /* Start at ratio, not 1.0: slope[h] = slope_base^((h+1)/n) */

    for (uint32_t h = 0; h < num_heads; h++) {
        slopes[h] = slope;
        slope *= ratio;
    }

    return slopes;
}

//=============================================================================
// Helper Functions - Learned Embeddings
//=============================================================================

/**
 * @brief Initialize learned embeddings with normal distribution
 */
static bool init_learned_embeddings(
    nimcp_pos_encoder_t* encoder
)
{
    size_t size = encoder->max_seq_length * encoder->embedding_dim;
    encoder->data.learned.embeddings = nimcp_calloc(size, sizeof(float));
    encoder->data.learned.gradients = nimcp_calloc(size, sizeof(float));

    if (!encoder->data.learned.embeddings || !encoder->data.learned.gradients) {
        nimcp_free(encoder->data.learned.embeddings);
        nimcp_free(encoder->data.learned.gradients);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_learned_embeddings: required parameter is NULL (encoder->data, encoder->data)");
        return false;
    }

    /* Initialize with normal distribution */
    float std = encoder->data.learned.init_std;
    for (size_t i = 0; i < size; i++) {
        /* Box-Muller transform for normal distribution */
        float u1 = ((float)nimcp_tl_rand() + 1.0F) / ((float)RAND_MAX + 2.0F);
        float u2 = ((float)nimcp_tl_rand() + 1.0F) / ((float)RAND_MAX + 2.0F);
        float z = sqrtf(-2.0F * logf(u1)) * cosf(2.0F * (float)M_PI * u2);
        encoder->data.learned.embeddings[i] = z * std;
    }

    return true;
}

//=============================================================================
// Helper Functions - Cache Management
//=============================================================================

/**
 * @brief Create encoding cache
 */
static nimcp_pos_cache_t* cache_create(uint32_t dim)
{
    nimcp_pos_cache_t* cache = nimcp_calloc(1, sizeof(nimcp_pos_cache_t));
    if (!cache) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache is NULL");

        return NULL;

    }

    cache->dim = dim;
    cache->cached_length = 0;
    cache->hits = 0;
    cache->misses = 0;

    return cache;
}

/**
 * @brief Destroy encoding cache
 */
static void cache_destroy(nimcp_pos_cache_t* cache)
{
    if (!cache) return;
    nimcp_free(cache->data);
    nimcp_free(cache);
}

/**
 * @brief Pre-compute and cache encodings
 */
static bool cache_precompute(
    nimcp_pos_encoder_t* encoder,
    uint32_t length
)
{
    if (!encoder->cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_precompute: encoder->cache is NULL");
        return false;
    }

    /* Allocate cache data */
    size_t size = length * encoder->embedding_dim * sizeof(float);
    float* new_data = nimcp_realloc(encoder->cache->data, size);
    if (!new_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cache_precompute: new_data is NULL");
        return false;
    }

    encoder->cache->data = new_data;

    /* Compute encodings for each position */
    for (uint32_t pos = 0; pos < length; pos++) {
        float* output = &encoder->cache->data[pos * encoder->embedding_dim];

        switch (encoder->type) {
            case NIMCP_POS_SINUSOIDAL:
                encode_sinusoidal_position(
                    encoder->data.sinusoidal.inv_freq,
                    encoder->embedding_dim,
                    pos,
                    output
                );
                break;

            case NIMCP_POS_LEARNED:
                memcpy(output,
                       &encoder->data.learned.embeddings[pos * encoder->embedding_dim],
                       encoder->embedding_dim * sizeof(float));
                break;

            case NIMCP_POS_ROTARY: {
                /* Generate position embedding from RoPE cos/sin cache */
                uint32_t rope_dim = encoder->data.rope.rope_dim;
                if (rope_dim == 0) rope_dim = encoder->embedding_dim;
                uint32_t half_dim = rope_dim / 2;
                memset(output, 0, encoder->embedding_dim * sizeof(float));
                for (uint32_t i = 0; i < half_dim && (2 * i + 1) < encoder->embedding_dim; i++) {
                    size_t cache_idx = pos * half_dim + i;
                    output[2 * i]     = encoder->data.rope.cos_cache[cache_idx];
                    output[2 * i + 1] = encoder->data.rope.sin_cache[cache_idx];
                }
                break;
            }

            case NIMCP_POS_RELATIVE: {
                /* Generate simple sinusoidal position embedding for relative PE */
                memset(output, 0, encoder->embedding_dim * sizeof(float));
                for (uint32_t i = 0; i < encoder->embedding_dim; i++) {
                    float freq = 1.0F / powf(10000.0F, (float)(i / 2 * 2) / (float)encoder->embedding_dim);
                    float angle = (float)pos * freq;
                    output[i] = (i % 2 == 0) ? sinf(angle) : cosf(angle);
                }
                break;
            }

            default:
                /* Other types may not use simple position caching */
                break;
        }
    }

    encoder->cache->cached_length = length;
    return true;
}

//=============================================================================
// Default Configuration Functions
//=============================================================================

nimcp_pos_sinusoidal_config_t nimcp_pos_sinusoidal_default_config(void)
{
    nimcp_pos_sinusoidal_config_t config = {
        .base = {
            .max_seq_length = 8192,
            .embedding_dim = NIMCP_LARGE_EMBEDDING_DIM,
            .cache_enabled = true,
            .thread_safe = true
        },
        .frequency_base = NIMCP_ROPE_DEFAULT_BASE,
        .frequency_scale = 1.0F
    };
    return config;
}

nimcp_pos_learned_config_t nimcp_pos_learned_default_config(void)
{
    nimcp_pos_learned_config_t config = {
        .base = {
            .max_seq_length = 2048,
            .embedding_dim = NIMCP_LARGE_EMBEDDING_DIM,
            .cache_enabled = true,
            .thread_safe = true
        },
        .init_std = 0.02F,
        .learning_rate = 0.001F,
        .weight_decay = 0.0001F
    };
    return config;
}

nimcp_pos_rope_config_t nimcp_pos_rope_default_config(void)
{
    nimcp_pos_rope_config_t config = {
        .base = {
            .max_seq_length = 8192,
            .embedding_dim = NIMCP_LARGE_EMBEDDING_DIM,
            .cache_enabled = true,
            .thread_safe = true
        },
        .rope_base = NIMCP_ROPE_DEFAULT_BASE,
        .rope_scaling = 1.0F,
        .rope_dim = 0,
        .use_ntk_scaling = false,
        .ntk_factor = 1.0F
    };
    return config;
}

nimcp_pos_alibi_config_t nimcp_pos_alibi_default_config(void)
{
    nimcp_pos_alibi_config_t config = {
        .base = {
            .max_seq_length = 16384,
            .embedding_dim = NIMCP_LARGE_EMBEDDING_DIM,
            .cache_enabled = false,
            .thread_safe = true
        },
        .num_heads = NIMCP_ALIBI_DEFAULT_HEADS,
        .slope_base = 0.5F,
        .use_symmetric = false
    };
    return config;
}

nimcp_pos_relative_config_t nimcp_pos_relative_default_config(void)
{
    nimcp_pos_relative_config_t config = {
        .base = {
            .max_seq_length = 4096,
            .embedding_dim = NIMCP_LARGE_EMBEDDING_DIM,
            .cache_enabled = true,
            .thread_safe = true
        },
        .max_relative_pos = 128,
        .use_clipping = true,
        .init_std = 0.02F
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_pos_encoder_t* nimcp_pos_encoder_create(const nimcp_pos_config_t* config)
{
    /* Validate input */
    if (!config) {
        LOG_ERROR(LOG_MODULE, "NULL config provided to encoder create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_pos_encoder_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    int validation = nimcp_pos_validate_config(config);
    if (validation != NIMCP_POS_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Invalid configuration: error %d", validation);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_pos_encoder_create: validation failed");
        return NULL;
    }

    /* Allocate encoder */
    nimcp_pos_encoder_t* encoder = nimcp_calloc(1, sizeof(nimcp_pos_encoder_t));
    if (!encoder) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate encoder");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_pos_encoder_create: encoder is NULL");
        return NULL;
    }

    encoder->type = config->type;

    /* Extract base config based on type */
    const nimcp_pos_base_config_t* base_config = NULL;
    switch (config->type) {
        case NIMCP_POS_SINUSOIDAL:
            base_config = &config->config.sinusoidal.base;
            break;
        case NIMCP_POS_LEARNED:
            base_config = &config->config.learned.base;
            break;
        case NIMCP_POS_ROTARY:
            base_config = &config->config.rope.base;
            break;
        case NIMCP_POS_ALIBI:
            base_config = &config->config.alibi.base;
            break;
        case NIMCP_POS_RELATIVE:
            base_config = &config->config.relative.base;
            break;
        default:
            LOG_ERROR(LOG_MODULE, "Invalid encoding type: %d", config->type);
            nimcp_free(encoder);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_pos_encoder_create: operation failed");
            return NULL;
    }

    encoder->max_seq_length = base_config->max_seq_length;
    encoder->embedding_dim = base_config->embedding_dim;
    encoder->cache_enabled = base_config->cache_enabled;
    encoder->thread_safe = base_config->thread_safe;

    /* Initialize thread safety */
    if (encoder->thread_safe) {
        pthread_rwlock_init(&encoder->lock, NULL);
    }

    /* Type-specific initialization */
    bool init_ok = true;
    switch (config->type) {
        case NIMCP_POS_SINUSOIDAL:
            encoder->data.sinusoidal.frequency_base = config->config.sinusoidal.frequency_base;
            encoder->data.sinusoidal.frequency_scale = config->config.sinusoidal.frequency_scale;
            encoder->data.sinusoidal.inv_freq = compute_inv_frequencies(
                encoder->embedding_dim,
                encoder->data.sinusoidal.frequency_base,
                encoder->data.sinusoidal.frequency_scale
            );
            init_ok = (encoder->data.sinusoidal.inv_freq != NULL);
            break;

        case NIMCP_POS_LEARNED:
            encoder->data.learned.init_std = config->config.learned.init_std;
            encoder->data.learned.learning_rate = config->config.learned.learning_rate;
            encoder->data.learned.weight_decay = config->config.learned.weight_decay;
            init_ok = init_learned_embeddings(encoder);
            break;

        case NIMCP_POS_ROTARY:
            encoder->data.rope.rope_base = config->config.rope.rope_base;
            encoder->data.rope.rope_scaling = config->config.rope.rope_scaling;
            encoder->data.rope.rope_dim = config->config.rope.rope_dim;
            encoder->data.rope.use_ntk_scaling = config->config.rope.use_ntk_scaling;
            encoder->data.rope.ntk_factor = config->config.rope.ntk_factor;
            init_ok = precompute_rope_cache(encoder, encoder->max_seq_length);
            break;

        case NIMCP_POS_ALIBI:
            encoder->data.alibi.num_heads = config->config.alibi.num_heads;
            encoder->data.alibi.slope_base = config->config.alibi.slope_base;
            encoder->data.alibi.use_symmetric = config->config.alibi.use_symmetric;
            encoder->data.alibi.slopes = compute_alibi_slopes(
                encoder->data.alibi.num_heads,
                encoder->data.alibi.slope_base
            );
            init_ok = (encoder->data.alibi.slopes != NULL);
            break;

        case NIMCP_POS_RELATIVE:
            encoder->data.relative.max_relative_pos = config->config.relative.max_relative_pos;
            encoder->data.relative.use_clipping = config->config.relative.use_clipping;
            /* Relative embeddings initialization would go here */
            break;

        default:
            init_ok = false;
            break;
    }

    if (!init_ok) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize type-specific data");
        nimcp_pos_encoder_destroy(encoder);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_pos_encoder_create: init_ok is NULL");
        return NULL;
    }

    /* Create cache if enabled */
    if (encoder->cache_enabled) {
        encoder->cache = cache_create(encoder->embedding_dim);
        if (!encoder->cache) {
            LOG_WARN(LOG_MODULE, "Failed to create cache, continuing without caching");
            encoder->cache_enabled = false;
        }
    }

    LOG_INFO(LOG_MODULE, "Created %s encoder: dim=%u, max_len=%u",
             nimcp_pos_type_to_string(encoder->type),
             encoder->embedding_dim,
             encoder->max_seq_length);

    return encoder;
}

void nimcp_pos_encoder_destroy(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return;

    /* Free type-specific data */
    switch (encoder->type) {
        case NIMCP_POS_SINUSOIDAL:
            nimcp_free(encoder->data.sinusoidal.inv_freq);
            break;

        case NIMCP_POS_LEARNED:
            nimcp_free(encoder->data.learned.embeddings);
            nimcp_free(encoder->data.learned.gradients);
            break;

        case NIMCP_POS_ROTARY:
            nimcp_free(encoder->data.rope.cos_cache);
            nimcp_free(encoder->data.rope.sin_cache);
            break;

        case NIMCP_POS_ALIBI:
            nimcp_free(encoder->data.alibi.slopes);
            break;

        case NIMCP_POS_RELATIVE:
            nimcp_free(encoder->data.relative.key_embeddings);
            nimcp_free(encoder->data.relative.value_embeddings);
            break;

        default:
            break;
    }

    /* Free cache */
    cache_destroy(encoder->cache);

    /* Destroy lock */
    if (encoder->thread_safe) {
        pthread_rwlock_destroy(&encoder->lock);
    }

    nimcp_free(encoder);
    LOG_DEBUG(LOG_MODULE, "Destroyed encoder");
}

//=============================================================================
// Encoding Functions
//=============================================================================

int nimcp_pos_encode_position(
    nimcp_pos_encoder_t* encoder,
    uint32_t position,
    float* output
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!output) return NIMCP_POS_ERROR_NULL_PARAM;
    if (position >= encoder->max_seq_length) return NIMCP_POS_ERROR_INVALID_POS;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    /* Check cache first */
    if (encoder->cache && position < encoder->cache->cached_length) {
        memcpy(output,
               &encoder->cache->data[position * encoder->embedding_dim],
               encoder->embedding_dim * sizeof(float));
        encoder->cache->hits++;
        encoder->stats.cache_hits++;

        if (encoder->thread_safe) {
            pthread_rwlock_unlock(&encoder->lock);
        }
        return NIMCP_POS_SUCCESS;
    }

    if (encoder->cache) {
        encoder->cache->misses++;
        encoder->stats.cache_misses++;
    }

    /* Compute encoding based on type */
    switch (encoder->type) {
        case NIMCP_POS_SINUSOIDAL:
            encode_sinusoidal_position(
                encoder->data.sinusoidal.inv_freq,
                encoder->embedding_dim,
                position,
                output
            );
            break;

        case NIMCP_POS_LEARNED:
            memcpy(output,
                   &encoder->data.learned.embeddings[position * encoder->embedding_dim],
                   encoder->embedding_dim * sizeof(float));
            break;

        case NIMCP_POS_ROTARY: {
            /* Generate position embedding from RoPE cos/sin cache */
            uint32_t rope_dim = encoder->data.rope.rope_dim;
            if (rope_dim == 0) rope_dim = encoder->embedding_dim;
            uint32_t half_dim = rope_dim / 2;
            memset(output, 0, encoder->embedding_dim * sizeof(float));
            for (uint32_t i = 0; i < half_dim && (2 * i + 1) < encoder->embedding_dim; i++) {
                size_t cache_idx = position * half_dim + i;
                output[2 * i]     = encoder->data.rope.cos_cache[cache_idx];
                output[2 * i + 1] = encoder->data.rope.sin_cache[cache_idx];
            }
            break;
        }

        case NIMCP_POS_RELATIVE: {
            /* Generate simple sinusoidal position embedding for relative PE */
            memset(output, 0, encoder->embedding_dim * sizeof(float));
            for (uint32_t i = 0; i < encoder->embedding_dim; i++) {
                float freq = 1.0F / powf(10000.0F, (float)(i / 2 * 2) / (float)encoder->embedding_dim);
                float angle = (float)position * freq;
                output[i] = (i % 2 == 0) ? sinf(angle) : cosf(angle);
            }
            break;
        }

        default:
            if (encoder->thread_safe) {
                pthread_rwlock_unlock(&encoder->lock);
            }
            return NIMCP_POS_ERROR_INVALID_TYPE;
    }

    encoder->stats.total_encodings++;

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_encode_sequence(
    nimcp_pos_encoder_t* encoder,
    uint32_t start_pos,
    uint32_t seq_length,
    float* output
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!output) return NIMCP_POS_ERROR_NULL_PARAM;
    if (start_pos + seq_length > encoder->max_seq_length) {
        return NIMCP_POS_ERROR_INVALID_POS;
    }

    /* Encode each position */
    for (uint32_t i = 0; i < seq_length; i++) {
        int result = nimcp_pos_encode_position(
            encoder,
            start_pos + i,
            &output[i * encoder->embedding_dim]
        );
        if (result != NIMCP_POS_SUCCESS) return result;
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_apply_encoding(
    nimcp_pos_encoder_t* encoder,
    const float* input,
    uint32_t seq_length,
    float* output,
    bool additive
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!input) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!output) return NIMCP_POS_ERROR_NULL_PARAM;
    if (seq_length > encoder->max_seq_length) return NIMCP_POS_ERROR_INVALID_POS;

    uint32_t dim = encoder->embedding_dim;

    /* Temporary buffer for position encoding */
    float* pos_encoding = nimcp_calloc(dim, sizeof(float));
    if (!pos_encoding) return NIMCP_POS_ERROR_ALLOC_FAILED;

    for (uint32_t pos = 0; pos < seq_length; pos++) {
        /* Get position encoding */
        int result = nimcp_pos_encode_position(encoder, pos, pos_encoding);
        if (result != NIMCP_POS_SUCCESS) {
            nimcp_free(pos_encoding);
            return result;
        }

        /* Apply to input */
        const float* in_ptr = &input[pos * dim];
        float* out_ptr = &output[pos * dim];

        if (additive) {
            /* Add encoding to input */
            for (uint32_t i = 0; i < dim; i++) {
                out_ptr[i] = in_ptr[i] + pos_encoding[i];
            }
        } else {
            /* Copy input then encoding (concatenate would need different output) */
            memcpy(out_ptr, in_ptr, dim * sizeof(float));
        }
    }

    nimcp_free(pos_encoding);
    return NIMCP_POS_SUCCESS;
}

//=============================================================================
// RoPE-Specific Functions
//=============================================================================

int nimcp_pos_rope_apply(
    nimcp_pos_encoder_t* encoder,
    const float* query,
    const float* key,
    uint32_t position,
    float* query_out,
    float* key_out
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_ROTARY) return NIMCP_POS_ERROR_INVALID_TYPE;
    if (!query || !key) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!query_out || !key_out) return NIMCP_POS_ERROR_NULL_PARAM;
    if (position >= encoder->max_seq_length) return NIMCP_POS_ERROR_INVALID_POS;

    uint32_t dim = encoder->data.rope.rope_dim;
    if (dim == 0) dim = encoder->embedding_dim;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    /* Apply rotation to query */
    apply_rope_rotation(
        encoder->data.rope.cos_cache,
        encoder->data.rope.sin_cache,
        query, dim, position, query_out
    );

    /* Apply rotation to key */
    apply_rope_rotation(
        encoder->data.rope.cos_cache,
        encoder->data.rope.sin_cache,
        key, dim, position, key_out
    );

    encoder->stats.total_encodings += 2;

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_rope_apply_batch(
    nimcp_pos_encoder_t* encoder,
    const float* queries,
    const float* keys,
    uint32_t seq_length,
    uint32_t num_heads,
    float* queries_out,
    float* keys_out
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_ROTARY) return NIMCP_POS_ERROR_INVALID_TYPE;
    if (!queries || !keys) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!queries_out || !keys_out) return NIMCP_POS_ERROR_NULL_PARAM;
    if (seq_length > encoder->max_seq_length) return NIMCP_POS_ERROR_INVALID_POS;

    uint32_t dim = encoder->data.rope.rope_dim;
    if (dim == 0) dim = encoder->embedding_dim;

    uint32_t head_dim = dim / num_heads;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    /* Apply RoPE to each position and head */
    for (uint32_t pos = 0; pos < seq_length; pos++) {
        for (uint32_t h = 0; h < num_heads; h++) {
            size_t offset = (pos * num_heads + h) * head_dim;

            apply_rope_rotation(
                encoder->data.rope.cos_cache,
                encoder->data.rope.sin_cache,
                &queries[offset], head_dim, pos, &queries_out[offset]
            );

            apply_rope_rotation(
                encoder->data.rope.cos_cache,
                encoder->data.rope.sin_cache,
                &keys[offset], head_dim, pos, &keys_out[offset]
            );
        }
    }

    encoder->stats.total_encodings += seq_length * num_heads * 2;

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

//=============================================================================
// ALiBi-Specific Functions
//=============================================================================

int nimcp_pos_alibi_get_bias(
    nimcp_pos_encoder_t* encoder,
    uint32_t seq_length,
    float* bias_out
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_ALIBI) return NIMCP_POS_ERROR_INVALID_TYPE;
    if (!bias_out) return NIMCP_POS_ERROR_NULL_PARAM;

    uint32_t num_heads = encoder->data.alibi.num_heads;
    bool symmetric = encoder->data.alibi.use_symmetric;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    /* Compute bias matrix for each head */
    for (uint32_t h = 0; h < num_heads; h++) {
        float slope = encoder->data.alibi.slopes[h];
        float* head_bias = &bias_out[h * seq_length * seq_length];

        for (uint32_t i = 0; i < seq_length; i++) {
            for (uint32_t j = 0; j < seq_length; j++) {
                int diff = (int)i - (int)j;
                if (symmetric) {
                    diff = (diff < 0) ? -diff : diff;  /* abs(diff) */
                } else {
                    diff = (diff < 0) ? 0 : diff;  /* causal mask effect */
                }
                head_bias[i * seq_length + j] = -slope * (float)diff;
            }
        }
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_alibi_get_slopes(
    nimcp_pos_encoder_t* encoder,
    float* slopes_out
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_ALIBI) return NIMCP_POS_ERROR_INVALID_TYPE;
    if (!slopes_out) return NIMCP_POS_ERROR_NULL_PARAM;

    memcpy(slopes_out,
           encoder->data.alibi.slopes,
           encoder->data.alibi.num_heads * sizeof(float));

    return NIMCP_POS_SUCCESS;
}

//=============================================================================
// Learned Embedding Functions
//=============================================================================

int nimcp_pos_learned_backward(
    nimcp_pos_encoder_t* encoder,
    const float* grad_output,
    const uint32_t* positions,
    uint32_t seq_length,
    float* grad_embeddings
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_LEARNED) return NIMCP_POS_ERROR_INVALID_TYPE;
    if (!grad_output || !positions || !grad_embeddings) return NIMCP_POS_ERROR_NULL_PARAM;

    uint32_t dim = encoder->embedding_dim;

    if (encoder->thread_safe) {
        pthread_rwlock_wrlock(&encoder->lock);
    }

    /* Accumulate gradients for each position */
    for (uint32_t i = 0; i < seq_length; i++) {
        uint32_t pos = positions[i];
        if (pos >= encoder->max_seq_length) {
            if (encoder->thread_safe) {
                pthread_rwlock_unlock(&encoder->lock);
            }
            return NIMCP_POS_ERROR_INVALID_POS;
        }

        float* grad_ptr = &encoder->data.learned.gradients[pos * dim];
        const float* out_grad = &grad_output[i * dim];

        for (uint32_t d = 0; d < dim; d++) {
            grad_ptr[d] += out_grad[d];
        }
    }

    /* Copy to output if provided separately */
    if (grad_embeddings != encoder->data.learned.gradients) {
        memcpy(grad_embeddings,
               encoder->data.learned.gradients,
               encoder->max_seq_length * dim * sizeof(float));
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_learned_update(
    nimcp_pos_encoder_t* encoder,
    const float* gradients,
    float learning_rate
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (encoder->type != NIMCP_POS_LEARNED) return NIMCP_POS_ERROR_INVALID_TYPE;

    if (learning_rate == 0.0F) {
        learning_rate = encoder->data.learned.learning_rate;
    }

    const float* grads = gradients ? gradients : encoder->data.learned.gradients;
    float weight_decay = encoder->data.learned.weight_decay;
    uint32_t dim = encoder->embedding_dim;
    size_t size = encoder->max_seq_length * dim;

    if (encoder->thread_safe) {
        pthread_rwlock_wrlock(&encoder->lock);
    }

    /* Update embeddings: w = w - lr * (grad + wd * w) */
    for (size_t i = 0; i < size; i++) {
        float grad = grads[i] + weight_decay * encoder->data.learned.embeddings[i];
        encoder->data.learned.embeddings[i] -= learning_rate * grad;
    }

    /* Zero gradients */
    memset(encoder->data.learned.gradients, 0, size * sizeof(float));

    /* Invalidate cache if enabled */
    if (encoder->cache) {
        encoder->cache->cached_length = 0;
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

//=============================================================================
// Cache Management
//=============================================================================

int nimcp_pos_cache_precompute(
    nimcp_pos_encoder_t* encoder,
    uint32_t length
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (length > encoder->max_seq_length) return NIMCP_POS_ERROR_INVALID_POS;

    if (!encoder->cache_enabled || !encoder->cache) {
        return NIMCP_POS_ERROR_NOT_INIT;
    }

    if (encoder->thread_safe) {
        pthread_rwlock_wrlock(&encoder->lock);
    }

    bool ok = cache_precompute(encoder, length);

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return ok ? NIMCP_POS_SUCCESS : NIMCP_POS_ERROR_ALLOC_FAILED;
}

int nimcp_pos_cache_clear(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!encoder->cache) return NIMCP_POS_ERROR_NOT_INIT;

    if (encoder->thread_safe) {
        pthread_rwlock_wrlock(&encoder->lock);
    }

    nimcp_free(encoder->cache->data);
    encoder->cache->data = NULL;
    encoder->cache->cached_length = 0;

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_cache_stats(
    nimcp_pos_encoder_t* encoder,
    float* hit_rate,
    size_t* size_bytes
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    if (encoder->cache) {
        uint64_t total = encoder->cache->hits + encoder->cache->misses;
        *hit_rate = (total > 0) ? (float)encoder->cache->hits / (float)total : 0.0F;
        *size_bytes = encoder->cache->cached_length * encoder->embedding_dim * sizeof(float);
    } else {
        *hit_rate = 0.0F;
        *size_bytes = 0;
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

int nimcp_pos_get_stats(
    nimcp_pos_encoder_t* encoder,
    nimcp_pos_stats_t* stats
)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;
    if (!stats) return NIMCP_POS_ERROR_NULL_PARAM;

    if (encoder->thread_safe) {
        pthread_rwlock_rdlock(&encoder->lock);
    }

    *stats = encoder->stats;

    /* Add cache size */
    if (encoder->cache) {
        stats->cache_size_bytes = encoder->cache->cached_length *
                                  encoder->embedding_dim * sizeof(float);
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

int nimcp_pos_reset_stats(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return NIMCP_POS_ERROR_NULL_PARAM;

    if (encoder->thread_safe) {
        pthread_rwlock_wrlock(&encoder->lock);
    }

    memset(&encoder->stats, 0, sizeof(nimcp_pos_stats_t));

    if (encoder->cache) {
        encoder->cache->hits = 0;
        encoder->cache->misses = 0;
    }

    if (encoder->thread_safe) {
        pthread_rwlock_unlock(&encoder->lock);
    }

    return NIMCP_POS_SUCCESS;
}

nimcp_pos_encoding_type_t nimcp_pos_get_type(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return (nimcp_pos_encoding_type_t)-1;
    return encoder->type;
}

uint32_t nimcp_pos_get_dim(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return 0;
    return encoder->embedding_dim;
}

uint32_t nimcp_pos_get_max_length(nimcp_pos_encoder_t* encoder)
{
    if (!encoder) return 0;
    return encoder->max_seq_length;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_pos_type_to_string(nimcp_pos_encoding_type_t type)
{
    switch (type) {
        case NIMCP_POS_SINUSOIDAL: return "Sinusoidal";
        case NIMCP_POS_LEARNED:    return "Learned";
        case NIMCP_POS_ROTARY:     return "RoPE";
        case NIMCP_POS_ALIBI:      return "ALiBi";
        case NIMCP_POS_RELATIVE:   return "Relative";
        default:                   return "Unknown";
    }
}

int nimcp_pos_validate_config(const nimcp_pos_config_t* config)
{
    if (!config) return NIMCP_POS_ERROR_NULL_PARAM;

    /* Get base config */
    const nimcp_pos_base_config_t* base = NULL;
    switch (config->type) {
        case NIMCP_POS_SINUSOIDAL:
            base = &config->config.sinusoidal.base;
            break;
        case NIMCP_POS_LEARNED:
            base = &config->config.learned.base;
            break;
        case NIMCP_POS_ROTARY:
            base = &config->config.rope.base;
            break;
        case NIMCP_POS_ALIBI:
            base = &config->config.alibi.base;
            break;
        case NIMCP_POS_RELATIVE:
            base = &config->config.relative.base;
            break;
        default:
            return NIMCP_POS_ERROR_INVALID_TYPE;
    }

    /* Validate base parameters */
    if (base->max_seq_length == 0 || base->max_seq_length > NIMCP_POS_MAX_SEQ_LENGTH) {
        return NIMCP_POS_ERROR_INVALID_POS;
    }

    /* ALiBi doesn't use embedding_dim, so skip check for that type */
    if (config->type != NIMCP_POS_ALIBI) {
        if (base->embedding_dim == 0 || base->embedding_dim > NIMCP_POS_MAX_DIM) {
            return NIMCP_POS_ERROR_INVALID_DIM;
        }
    }

    /* Type-specific validation */
    switch (config->type) {
        case NIMCP_POS_SINUSOIDAL:
            if (config->config.sinusoidal.frequency_base <= 0) {
                return NIMCP_POS_ERROR_INVALID_DIM;
            }
            break;

        case NIMCP_POS_ALIBI:
            if (config->config.alibi.num_heads == 0) {
                return NIMCP_POS_ERROR_INVALID_DIM;
            }
            break;

        default:
            break;
    }

    return NIMCP_POS_SUCCESS;
}
