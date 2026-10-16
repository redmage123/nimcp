#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_simd.c - SIMD Vectorized Pink Noise Generation
//=============================================================================

#include "plasticity/noise/nimcp_pink_noise_simd.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_simd)

// Check for SIMD availability at compile time
#if defined(__AVX2__)
    #include <immintrin.h>
    #define HAVE_AVX2 1
#elif defined(__SSE4_1__)
    #include <smmintrin.h>
    #define HAVE_SSE4 1
#elif defined(__ARM_NEON)
    #include <arm_neon.h>
    #define HAVE_NEON 1
#endif

//=============================================================================
// SIMD Detection
//=============================================================================

pink_simd_type_t pink_simd_detect(void) {
#if defined(HAVE_AVX2)
    return PINK_SIMD_AVX2;
#elif defined(HAVE_SSE4)
    return PINK_SIMD_SSE4;
#elif defined(HAVE_NEON)
    return PINK_SIMD_NEON;
#else
    return PINK_SIMD_NONE;
#endif
}

const char* pink_simd_type_name(pink_simd_type_t type) {
    switch (type) {
        case PINK_SIMD_SSE4: return "SSE4";
        case PINK_SIMD_AVX2: return "AVX2";
        case PINK_SIMD_AVX512: return "AVX512";
        case PINK_SIMD_NEON: return "NEON";
        default: return "scalar";
    }
}

//=============================================================================
// Configuration
//=============================================================================

pink_simd_config_t pink_simd_default_config(void) {
    pink_simd_config_t config = {0};
    config.preferred_simd = pink_simd_detect();
    config.vector_size = (config.preferred_simd == PINK_SIMD_AVX2) ? 8 : 4;
    config.alpha = 1.0f;
    config.amplitude = 0.05f;
    config.num_octaves = 16;
    config.seed = 42;
    config.enable_prefetch = true;
    return config;
}

//=============================================================================
// Internal: Scalar Voss-McCartney (fallback)
//=============================================================================

static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float uniform_from_uint(uint32_t x) {
    return (float)(x & 0xFFFFFF) / 16777216.0f - 0.5f;
}

static void voss_scalar(
    pink_simd_generator_t* gen,
    float* output,
    uint32_t num_samples
) {
    uint32_t num_octaves = gen->config.num_octaves;
    float scale = gen->config.amplitude / sqrtf((float)num_octaves);

    for (uint32_t i = 0; i < num_samples; i++) {
        float sum = 0.0f;
        uint32_t n = (uint32_t)(gen->total_samples + i);

        for (uint32_t oct = 0; oct < num_octaves; oct++) {
            uint32_t mask = (1U << oct) - 1;
            if ((n & mask) == 0) {
                gen->octave_values[oct] = uniform_from_uint(xorshift32(&gen->rng_state[0]));
            }
            sum += gen->octave_values[oct];
        }

        output[i] = sum * scale;
    }
}

//=============================================================================
// SIMD Implementations
//=============================================================================

#if defined(HAVE_AVX2)
static void voss_avx2(
    pink_simd_generator_t* gen,
    float* output,
    uint32_t num_samples
) {
    uint32_t num_octaves = gen->config.num_octaves;
    float scale = gen->config.amplitude / sqrtf((float)num_octaves);
    __m256 scale_vec = _mm256_set1_ps(scale);

    // Process 8 samples at a time
    uint32_t vec_samples = num_samples & ~7U;

    for (uint32_t i = 0; i < vec_samples; i += 8) {
        __m256 sum = _mm256_setzero_ps();

        for (uint32_t oct = 0; oct < num_octaves; oct++) {
            // Check if octave needs update for any of 8 samples
            uint32_t base_n = (uint32_t)(gen->total_samples + i);
            uint32_t mask = (1U << oct) - 1;

            for (uint32_t j = 0; j < 8; j++) {
                if (((base_n + j) & mask) == 0) {
                    gen->octave_values[oct * 8 + j] =
                        uniform_from_uint(xorshift32(&gen->rng_state[j]));
                }
            }

            __m256 oct_vals = _mm256_loadu_ps(&gen->octave_values[oct * 8]);
            sum = _mm256_add_ps(sum, oct_vals);
        }

        sum = _mm256_mul_ps(sum, scale_vec);
        _mm256_storeu_ps(&output[i], sum);
    }

    // Handle remainder
    for (uint32_t i = vec_samples; i < num_samples; i++) {
        float s = 0.0f;
        for (uint32_t oct = 0; oct < num_octaves; oct++) {
            s += gen->octave_values[oct * 8];
        }
        output[i] = s * scale;
    }
}
#endif

#if defined(HAVE_SSE4)
static void voss_sse4(
    pink_simd_generator_t* gen,
    float* output,
    uint32_t num_samples
) {
    uint32_t num_octaves = gen->config.num_octaves;
    float scale = gen->config.amplitude / sqrtf((float)num_octaves);
    __m128 scale_vec = _mm_set1_ps(scale);

    uint32_t vec_samples = num_samples & ~3U;

    for (uint32_t i = 0; i < vec_samples; i += 4) {
        __m128 sum = _mm_setzero_ps();

        for (uint32_t oct = 0; oct < num_octaves; oct++) {
            uint32_t base_n = (uint32_t)(gen->total_samples + i);
            uint32_t mask = (1U << oct) - 1;

            for (uint32_t j = 0; j < 4; j++) {
                if (((base_n + j) & mask) == 0) {
                    gen->octave_values[oct * 4 + j] =
                        uniform_from_uint(xorshift32(&gen->rng_state[j]));
                }
            }

            __m128 oct_vals = _mm_loadu_ps(&gen->octave_values[oct * 4]);
            sum = _mm_add_ps(sum, oct_vals);
        }

        sum = _mm_mul_ps(sum, scale_vec);
        _mm_storeu_ps(&output[i], sum);
    }

    for (uint32_t i = vec_samples; i < num_samples; i++) {
        float s = 0.0f;
        for (uint32_t oct = 0; oct < num_octaves; oct++) {
            s += gen->octave_values[oct * 4];
        }
        output[i] = s * scale;
    }
}
#endif

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_simd_generator_t* pink_simd_create(const pink_simd_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    pink_simd_generator_t* gen = nimcp_calloc(1, sizeof(pink_simd_generator_t));
    if (!gen) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gen is NULL");

        return NULL;

    }

    memcpy(&gen->config, config, sizeof(pink_simd_config_t));
    gen->active_simd = pink_simd_detect();

    // Allocate aligned memory
    uint32_t vec_size = config->vector_size;
    uint32_t num_octaves = config->num_octaves;

    gen->octave_values = aligned_alloc(64, num_octaves * vec_size * sizeof(float));
    gen->output_buffer = aligned_alloc(64, 4096 * sizeof(float));
    gen->octave_counters = nimcp_calloc(num_octaves, sizeof(uint32_t));

    if (!gen->octave_values || !gen->output_buffer || !gen->octave_counters) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "pink_simd_create: failed to allocate buffers");
        pink_simd_destroy(gen);
        return NULL;
    }

    memset(gen->octave_values, 0, num_octaves * vec_size * sizeof(float));
    gen->buffer_size = 4096;

    // Initialize RNG states
    for (uint32_t i = 0; i < 8; i++) {
        gen->rng_state[i] = config->seed + i * NIMCP_LCG_INCREMENT;
    }

    NIMCP_LOGGING_INFO("Created SIMD pink noise generator (%s)",
                       pink_simd_type_name(gen->active_simd));
    return gen;
}

void pink_simd_destroy(pink_simd_generator_t* gen) {
    if (!gen) return;
    if (gen->octave_values) nimcp_free(gen->octave_values);
    if (gen->output_buffer) nimcp_free(gen->output_buffer);
    if (gen->octave_counters) nimcp_free(gen->octave_counters);
    nimcp_free(gen);
}

//=============================================================================
// Generation Functions
//=============================================================================

int pink_simd_generate_batch(
    pink_simd_generator_t* gen,
    float* output,
    uint32_t num_samples
) {
    if (!gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_simd_generate_batch: gen is NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_simd_generate_batch: output is NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "pink_simd_generate_batch: num_samples is 0");
        return -1;
    }

#if defined(HAVE_AVX2)
    if (gen->active_simd == PINK_SIMD_AVX2) {
        voss_avx2(gen, output, num_samples);
    } else
#endif
#if defined(HAVE_SSE4)
    if (gen->active_simd == PINK_SIMD_SSE4) {
        voss_sse4(gen, output, num_samples);
    } else
#endif
    {
        voss_scalar(gen, output, num_samples);
    }

    gen->total_samples += num_samples;
    return 0;
}

const float* pink_simd_generate_aligned(
    pink_simd_generator_t* gen,
    uint32_t num_samples
) {
    if (!gen || num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_simd_generate_aligned: gen is NULL");
        return NULL;
    }

    if (num_samples > gen->buffer_size) {
        num_samples = gen->buffer_size;
    }

    pink_simd_generate_batch(gen, gen->output_buffer, num_samples);
    return gen->output_buffer;
}

float pink_simd_generate_sample(pink_simd_generator_t* gen) {
    if (!gen) return 0.0f;

    float sample;
    pink_simd_generate_batch(gen, &sample, 1);
    return sample;
}

int pink_simd_reset(pink_simd_generator_t* gen, uint32_t new_seed) {
    if (!gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_simd_reset: gen is NULL");
        return -1;
    }

    uint32_t seed = (new_seed != 0) ? new_seed : gen->config.seed;
    for (uint32_t i = 0; i < 8; i++) {
        gen->rng_state[i] = seed + i * NIMCP_LCG_INCREMENT;
    }

    memset(gen->octave_values, 0,
           gen->config.num_octaves * gen->config.vector_size * sizeof(float));
    gen->total_samples = 0;

    return 0;
}

int pink_simd_get_stats(const pink_simd_generator_t* gen, pink_simd_stats_t* stats) {
    if (!gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_simd_get_stats: gen is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_simd_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(pink_simd_stats_t));
    stats->total_samples = gen->total_samples;
    stats->simd_type = gen->active_simd;
    stats->vector_width = gen->config.vector_size;

    return 0;
}
