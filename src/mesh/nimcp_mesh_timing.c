/**
 * @file nimcp_mesh_timing.c
 * @brief Hierarchical Pink Noise Timing Implementation
 *
 * WHAT: Bio-plausible timing with pink noise jitter implementation
 * WHY:  Prevent synchronized failures, enable biological realism
 * HOW:  Voss-McCartney algorithm for efficient pink noise generation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_timing.h"
#include "utils/error/nimcp_error_codes.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_math_constants.h"

/* Error code compatibility aliases */

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Number of octaves for Voss-McCartney algorithm */
#define VOSS_NUM_OCTAVES    16

/** @brief Minimum samples for validation */
#define MIN_VALIDATION_SAMPLES  64

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Voss-McCartney pink noise generator state
 */
typedef struct voss_generator {
    float octaves[VOSS_NUM_OCTAVES];    /**< Octave values */
    uint32_t counter;                    /**< Sample counter */
    uint32_t rng_state;                  /**< RNG state */
} voss_generator_t;

/**
 * @brief Per-level timing state
 */
typedef struct timing_level_state {
    mesh_timing_level_config_t config;
    voss_generator_t pink_gen;
    mesh_timing_level_stats_t stats;
    float adaptation_factor;
    float ema_latency;      /**< Exponential moving average of observed latency */
} timing_level_state_t;

/**
 * @brief Internal hierarchical timing context
 */
struct mesh_hierarchical_timing_internal {
    timing_level_state_t levels[MESH_TIMING_NUM_LEVELS];
    uint32_t seed;
    bool enable_adaptation;
    float adaptation_rate;
};

/* ============================================================================
 * Random Number Generation
 * ============================================================================ */

/**
 * @brief Simple xorshift32 RNG
 */
static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 * @brief Generate uniform random float in [0, 1)
 */
static float rand_uniform(uint32_t* state) {
    return (float)(xorshift32(state) & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * @brief Generate standard normal using Box-Muller transform
 */
static float rand_normal(uint32_t* state) {
    float u1 = rand_uniform(state);
    float u2 = rand_uniform(state);

    /* Avoid log(0) */
    if (u1 < 1e-10f) u1 = 1e-10f;

    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

/* ============================================================================
 * Voss-McCartney Pink Noise Generator
 * ============================================================================ */

/**
 * @brief Initialize Voss-McCartney generator
 */
static void voss_init(voss_generator_t* gen, uint32_t seed) {
    if (!gen) return;

    gen->counter = 0;
    gen->rng_state = seed ? seed : (uint32_t)time(NULL);

    /* Initialize octaves with random values */
    for (int i = 0; i < VOSS_NUM_OCTAVES; i++) {
        gen->octaves[i] = rand_normal(&gen->rng_state);
    }
}

/**
 * @brief Generate next pink noise sample using Voss-McCartney
 *
 * The algorithm updates octave i when (counter % 2^i) == 0,
 * giving each octave a different update frequency.
 */
static float voss_next(voss_generator_t* gen) {
    if (!gen) return 0.0f;

    uint32_t changed = gen->counter ^ (gen->counter + 1);
    gen->counter++;

    /* Update octaves based on which bits changed */
    for (int i = 0; i < VOSS_NUM_OCTAVES; i++) {
        if (changed & (1u << i)) {
            gen->octaves[i] = rand_normal(&gen->rng_state);
        }
    }

    /* Sum all octaves */
    float sum = 0.0f;
    for (int i = 0; i < VOSS_NUM_OCTAVES; i++) {
        sum += gen->octaves[i];
    }

    /* Normalize to approximately [-1, 1] */
    return sum / (float)VOSS_NUM_OCTAVES;
}

/**
 * @brief Reset Voss generator
 */
static void voss_reset(voss_generator_t* gen, uint32_t seed) {
    voss_init(gen, seed);
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

mesh_hierarchical_timing_config_t mesh_timing_default_config(void) {
    mesh_hierarchical_timing_config_t config;

    config.levels[MESH_TIMING_LEVEL_SYSTEM] = (mesh_timing_level_config_t){
        .base_interval_ms = MESH_TIMING_SYSTEM_BASE_MS,
        .jitter_amplitude_ms = MESH_TIMING_SYSTEM_JITTER_MS,
        .min_interval_ms = MESH_TIMING_SYSTEM_MIN_MS,
        .max_interval_ms = MESH_TIMING_SYSTEM_MAX_MS,
        .pink_alpha = MESH_TIMING_PINK_ALPHA
    };

    config.levels[MESH_TIMING_LEVEL_HEMISPHERE] = (mesh_timing_level_config_t){
        .base_interval_ms = MESH_TIMING_HEMISPHERE_BASE_MS,
        .jitter_amplitude_ms = MESH_TIMING_HEMISPHERE_JITTER_MS,
        .min_interval_ms = MESH_TIMING_HEMISPHERE_MIN_MS,
        .max_interval_ms = MESH_TIMING_HEMISPHERE_MAX_MS,
        .pink_alpha = MESH_TIMING_PINK_ALPHA
    };

    config.levels[MESH_TIMING_LEVEL_LAYER] = (mesh_timing_level_config_t){
        .base_interval_ms = MESH_TIMING_LAYER_BASE_MS,
        .jitter_amplitude_ms = MESH_TIMING_LAYER_JITTER_MS,
        .min_interval_ms = MESH_TIMING_LAYER_MIN_MS,
        .max_interval_ms = MESH_TIMING_LAYER_MAX_MS,
        .pink_alpha = MESH_TIMING_PINK_ALPHA
    };

    config.levels[MESH_TIMING_LEVEL_ORDERING] = (mesh_timing_level_config_t){
        .base_interval_ms = MESH_TIMING_ORDERING_BASE_MS,
        .jitter_amplitude_ms = MESH_TIMING_ORDERING_JITTER_MS,
        .min_interval_ms = MESH_TIMING_ORDERING_MIN_MS,
        .max_interval_ms = MESH_TIMING_ORDERING_MAX_MS,
        .pink_alpha = MESH_TIMING_PINK_ALPHA
    };

    config.random_seed = MESH_TIMING_DEFAULT_SEED;
    config.enable_adaptation = false;
    config.adaptation_rate = 0.1f;

    return config;
}

mesh_timing_level_config_t mesh_timing_default_level_config(mesh_timing_level_t level) {
    mesh_hierarchical_timing_config_t full = mesh_timing_default_config();

    if (level >= MESH_TIMING_NUM_LEVELS) {
        return full.levels[0];
    }
    return full.levels[level];
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

mesh_hierarchical_timing_t mesh_timing_create(
    const mesh_hierarchical_timing_config_t* config
) {
    mesh_hierarchical_timing_t timing = (mesh_hierarchical_timing_t)nimcp_calloc(
        1, sizeof(struct mesh_hierarchical_timing_internal));
    if (!timing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_timing_create: timing is NULL");
        return NULL;
    }

    mesh_hierarchical_timing_config_t cfg = config ? *config : mesh_timing_default_config();

    timing->seed = cfg.random_seed ? cfg.random_seed : (uint32_t)time(NULL);
    timing->enable_adaptation = cfg.enable_adaptation;
    timing->adaptation_rate = cfg.adaptation_rate;

    /* Initialize each level */
    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        timing->levels[i].config = cfg.levels[i];
        timing->levels[i].adaptation_factor = 1.0f;
        timing->levels[i].ema_latency = cfg.levels[i].base_interval_ms;

        /* Initialize pink noise generator with unique seed per level */
        voss_init(&timing->levels[i].pink_gen, timing->seed + (uint32_t)i * 12345);

        /* Reset stats */
        memset(&timing->levels[i].stats, 0, sizeof(mesh_timing_level_stats_t));
    }

    return timing;
}

void mesh_timing_destroy(mesh_hierarchical_timing_t timing) {
    nimcp_free(timing);
}

/* ============================================================================
 * Interval Generation
 * ============================================================================ */

float mesh_timing_next_interval(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        return MESH_TIMING_LAYER_BASE_MS; /* Safe default */
    }

    timing_level_state_t* state = &timing->levels[level];
    mesh_timing_level_config_t* cfg = &state->config;

    /* Generate pink noise sample */
    float pink = voss_next(&state->pink_gen);

    /* Apply jitter: base + (pink * jitter_amplitude) */
    float base = cfg->base_interval_ms * state->adaptation_factor;
    float interval = base + (pink * cfg->jitter_amplitude_ms);

    /* Clamp to [min, max] */
    if (interval < cfg->min_interval_ms) {
        interval = cfg->min_interval_ms;
    }
    if (interval > cfg->max_interval_ms) {
        interval = cfg->max_interval_ms;
    }

    /* Update statistics */
    state->stats.samples_generated++;

    double n = (double)state->stats.samples_generated;
    double old_mean = state->stats.mean_interval_ms;
    state->stats.mean_interval_ms = (float)(old_mean + (interval - old_mean) / n);

    /* Online variance (Welford's algorithm) */
    if (n > 1) {
        double delta = interval - old_mean;
        double delta2 = interval - state->stats.mean_interval_ms;
        double M2 = state->stats.std_interval_ms * state->stats.std_interval_ms * (n - 1);
        M2 += delta * delta2;
        state->stats.std_interval_ms = (float)sqrt(M2 / (n - 1));
    }

    if (state->stats.samples_generated == 1 || interval < state->stats.min_generated_ms) {
        state->stats.min_generated_ms = interval;
    }
    if (interval > state->stats.max_generated_ms) {
        state->stats.max_generated_ms = interval;
    }

    return interval;
}

uint64_t mesh_timing_next_interval_ns(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    float ms = mesh_timing_next_interval(timing, level);
    return (uint64_t)(ms * 1000000.0f);
}

nimcp_error_t mesh_timing_generate_batch(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float* intervals,
    size_t count
) {
    if (!timing || !intervals || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (level >= MESH_TIMING_NUM_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < count; i++) {
        intervals[i] = mesh_timing_next_interval(timing, level);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Heartbeat and Timeout Functions
 * ============================================================================ */

float mesh_timing_heartbeat_interval(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    return mesh_timing_next_interval(timing, level);
}

float mesh_timing_election_timeout(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        return MESH_TIMING_LAYER_BASE_MS * 3.0f;
    }

    /* Election timeout = 2-3x heartbeat interval with jitter */
    float base_heartbeat = timing->levels[level].config.base_interval_ms;
    float pink = voss_next(&timing->levels[level].pink_gen);

    /* Random multiplier in [2, 3] with pink noise variation */
    float multiplier = 2.5f + (pink * 0.5f);

    float timeout = base_heartbeat * multiplier * timing->levels[level].adaptation_factor;

    /* Clamp */
    float min_timeout = timing->levels[level].config.min_interval_ms * 2.0f;
    float max_timeout = timing->levels[level].config.max_interval_ms * 3.0f;

    if (timeout < min_timeout) timeout = min_timeout;
    if (timeout > max_timeout) timeout = max_timeout;

    return timeout;
}

float mesh_timing_transaction_timeout(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float base_timeout
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        return base_timeout;
    }

    /* Add level-appropriate jitter to base timeout */
    float pink = voss_next(&timing->levels[level].pink_gen);
    float jitter_factor = timing->levels[level].config.jitter_amplitude_ms /
                          timing->levels[level].config.base_interval_ms;

    float timeout = base_timeout * (1.0f + pink * jitter_factor);

    if (timeout < base_timeout * 0.5f) timeout = base_timeout * 0.5f;
    if (timeout > base_timeout * 2.0f) timeout = base_timeout * 2.0f;

    return timeout;
}

/* ============================================================================
 * Adaptive Timing
 * ============================================================================ */

nimcp_error_t mesh_timing_report_latency(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    float observed_ms
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!timing->enable_adaptation) {
        return NIMCP_SUCCESS;
    }

    timing_level_state_t* state = &timing->levels[level];

    /* Exponential moving average update */
    float alpha = timing->adaptation_rate;
    state->ema_latency = alpha * observed_ms + (1.0f - alpha) * state->ema_latency;

    /* Update adaptation factor */
    float target = state->config.base_interval_ms;
    if (state->ema_latency > target) {
        /* Increase timing to match observed latency */
        state->adaptation_factor = state->ema_latency / target;
    } else {
        /* Gradually return to baseline */
        state->adaptation_factor = 0.9f * state->adaptation_factor + 0.1f * 1.0f;
    }

    /* Clamp adaptation factor */
    if (state->adaptation_factor < 0.5f) state->adaptation_factor = 0.5f;
    if (state->adaptation_factor > 3.0f) state->adaptation_factor = 3.0f;

    return NIMCP_SUCCESS;
}

float mesh_timing_get_adaptation_factor(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        return 1.0f;
    }
    return timing->levels[level].adaptation_factor;
}

nimcp_error_t mesh_timing_reset_adaptation(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level
) {
    if (!timing) return NIMCP_ERROR_INVALID_PARAM;

    if (level >= MESH_TIMING_NUM_LEVELS) {
        /* Reset all levels */
        for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
            timing->levels[i].adaptation_factor = 1.0f;
            timing->levels[i].ema_latency = timing->levels[i].config.base_interval_ms;
        }
    } else {
        timing->levels[level].adaptation_factor = 1.0f;
        timing->levels[level].ema_latency = timing->levels[level].config.base_interval_ms;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Configuration Update
 * ============================================================================ */

nimcp_error_t mesh_timing_update_level_config(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    const mesh_timing_level_config_t* config
) {
    if (!timing || !config || level >= MESH_TIMING_NUM_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    timing->levels[level].config = *config;
    timing->levels[level].ema_latency = config->base_interval_ms;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_timing_reseed(
    mesh_hierarchical_timing_t timing,
    uint32_t seed
) {
    if (!timing) return NIMCP_ERROR_INVALID_PARAM;

    timing->seed = seed ? seed : (uint32_t)time(NULL);

    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        voss_reset(&timing->levels[i].pink_gen, timing->seed + (uint32_t)i * 12345);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Debugging
 * ============================================================================ */

nimcp_error_t mesh_timing_get_stats(
    mesh_hierarchical_timing_t timing,
    mesh_hierarchical_timing_stats_t* stats
) {
    if (!timing || !stats) return NIMCP_ERROR_INVALID_PARAM;

    memset(stats, 0, sizeof(mesh_hierarchical_timing_stats_t));

    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        stats->levels[i] = timing->levels[i].stats;
        stats->total_samples += timing->levels[i].stats.samples_generated;
    }

    /* Compute overall jitter factor */
    float total_jitter = 0.0f;
    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        if (timing->levels[i].config.base_interval_ms > 0) {
            total_jitter += timing->levels[i].stats.std_interval_ms /
                           timing->levels[i].config.base_interval_ms;
        }
    }
    stats->overall_jitter_factor = total_jitter / MESH_TIMING_NUM_LEVELS;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_timing_get_level_stats(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    mesh_timing_level_stats_t* stats
) {
    if (!timing || !stats || level >= MESH_TIMING_NUM_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *stats = timing->levels[level].stats;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_timing_reset_stats(mesh_hierarchical_timing_t timing) {
    if (!timing) return NIMCP_ERROR_INVALID_PARAM;

    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        memset(&timing->levels[i].stats, 0, sizeof(mesh_timing_level_stats_t));
    }

    return NIMCP_SUCCESS;
}

bool mesh_timing_validate_pink_noise(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    uint32_t num_samples
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_timing_validate_pink_noise: timing is NULL");
        return false;
    }
    if (num_samples < MIN_VALIDATION_SAMPLES) {
        num_samples = MIN_VALIDATION_SAMPLES;
    }

    /* Generate samples and check basic statistical properties */
    float* samples = (float*)nimcp_malloc(num_samples * sizeof(float));
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_timing_validate_pink_noise: samples is NULL");
        return false;
    }

    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        samples[i] = voss_next(&timing->levels[level].pink_gen);
        sum += samples[i];
        sum_sq += samples[i] * samples[i];
    }

    float mean = sum / num_samples;
    float variance = (sum_sq / num_samples) - (mean * mean);
    float std = sqrtf(variance > 0 ? variance : 0);

    nimcp_free(samples);

    /* Pink noise should have:
     * - Mean close to 0 (< 0.3)
     * - Std around 0.5-1.5 (normalized Voss output)
     */
    bool mean_ok = fabsf(mean) < 0.3f;
    bool std_ok = std > 0.1f && std < 2.0f;

    return mean_ok && std_ok;
}

void mesh_timing_print_debug(mesh_hierarchical_timing_t timing) {
    if (!timing) {
        printf("Mesh Timing: NULL context\n");
        return;
    }

    printf("=== Mesh Hierarchical Timing Debug ===\n");
    printf("Seed: %u\n", timing->seed);
    printf("Adaptation: %s (rate: %.2f)\n",
           timing->enable_adaptation ? "enabled" : "disabled",
           timing->adaptation_rate);

    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        timing_level_state_t* state = &timing->levels[i];
        printf("\nLevel %d (%s):\n", i, mesh_timing_level_to_string((mesh_timing_level_t)i));
        printf("  Base: %.1f ms, Jitter: %.1f ms\n",
               state->config.base_interval_ms, state->config.jitter_amplitude_ms);
        printf("  Range: [%.1f, %.1f] ms\n",
               state->config.min_interval_ms, state->config.max_interval_ms);
        printf("  Adaptation factor: %.3f\n", state->adaptation_factor);
        printf("  Samples: %llu, Mean: %.2f ms, Std: %.2f ms\n",
               (unsigned long long)state->stats.samples_generated,
               state->stats.mean_interval_ms,
               state->stats.std_interval_ms);
    }
    printf("======================================\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_timing_level_to_string(mesh_timing_level_t level) {
    switch (level) {
        case MESH_TIMING_LEVEL_SYSTEM:     return "SYSTEM";
        case MESH_TIMING_LEVEL_HEMISPHERE: return "HEMISPHERE";
        case MESH_TIMING_LEVEL_LAYER:      return "LAYER";
        case MESH_TIMING_LEVEL_ORDERING:   return "ORDERING";
        default:                           return "UNKNOWN";
    }
}

mesh_timing_level_t mesh_timing_level_from_coord_level(uint32_t coord_level) {
    /* Map coordinator levels to timing levels */
    /* COORD_LEVEL_SYSTEM=0, HEMISPHERE=1, LAYER=2, ORDERING=3 */
    if (coord_level >= MESH_TIMING_NUM_LEVELS) {
        return MESH_TIMING_LEVEL_LAYER; /* Default to layer */
    }
    return (mesh_timing_level_t)coord_level;
}

float mesh_timing_expected_convergence(
    mesh_hierarchical_timing_t timing,
    mesh_timing_level_t level,
    uint32_t num_participants
) {
    if (!timing || level >= MESH_TIMING_NUM_LEVELS || num_participants == 0) {
        return 0.0f;
    }

    /* Gossip convergence is O(log N) rounds */
    float rounds = log2f((float)num_participants);
    if (rounds < 1.0f) rounds = 1.0f;

    /* Each round takes approximately one timing interval */
    float interval = timing->levels[level].config.base_interval_ms *
                    timing->levels[level].adaptation_factor;

    /* Add jitter buffer (1.5x for safety) */
    return rounds * interval * 1.5f;
}
