/**
 * @file nimcp_curriculum_learning.c
 * @brief Curriculum Learning Implementation
 *
 * WHAT: Progressive difficulty ordering for training samples
 * WHY:  Improve training efficiency by starting with easy examples
 * HOW:  Self-paced learning with multiple pacing functions, difficulty
 *       estimation from training loss, and adaptive threshold scheduling
 *
 * IMPLEMENTATION NOTES:
 * - Difficulty scores stored per-sample, updated during training
 * - Pacing function converts difficulty to sample weights
 * - Threshold advances per-epoch based on schedule or adaptation
 * - Supports both hard selection and soft weighting
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#include "training/nimcp_curriculum_learning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"

#define LOG_MODULE "curriculum"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(curriculum_learning)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal curriculum context
 */
struct curriculum_ctx_s {
    /* Configuration */
    curriculum_config_t config;

    /* Sample count */
    uint32_t num_samples;

    /* Difficulty tracking */
    float* difficulties;                /**< Per-sample difficulty scores [num_samples] */
    float* difficulty_ema;              /**< EMA of difficulties for stability */
    uint32_t* update_counts;            /**< How many times each sample updated */
    bool* initialized;                  /**< Whether sample has been scored */

    /* Sorted indices */
    uint32_t* sorted_indices;           /**< Indices sorted by difficulty */
    bool sorted_valid;                  /**< Whether sorted_indices is current */

    /* Current state */
    curriculum_state_t state;           /**< Current curriculum state */
    float current_threshold;            /**< Current difficulty threshold */
    float current_pace;                 /**< Current pacing parameter (lambda) */
    uint32_t current_epoch;             /**< Current epoch number */

    /* Epoch tracking */
    float last_epoch_loss;              /**< Loss from last epoch */
    float best_epoch_loss;              /**< Best loss seen */
    uint32_t epochs_no_improve;         /**< Epochs without improvement */

    /* Statistics */
    curriculum_stats_t stats;

    /* Random state for anti-curriculum injection */
    uint64_t rng_state;
};

//=============================================================================
// Random Number Generator
//=============================================================================

static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float randf(uint64_t* state) {
    return (float)(xorshift64(state) >> 11) / (float)(1ULL << 53);
}

//=============================================================================
// Pacing Functions
//=============================================================================

/**
 * @brief Linear pacing: w(d) = max(0, 1 - d/lambda)
 */
static float pacing_linear(float difficulty, float pace) {
    if (pace <= 0.0f) return 0.0f;
    float weight = 1.0f - difficulty / pace;
    return (weight > 0.0f) ? weight : 0.0f;
}

/**
 * @brief Logarithmic pacing: w(d) = 1/ln(1 + d*exp(1/lambda))
 */
static float pacing_logarithmic(float difficulty, float pace) {
    if (pace <= 0.0f) return 0.0f;
    float exp_term = difficulty * expf(1.0f / pace);
    float log_term = logf(1.0f + exp_term);
    if (log_term < 1e-8f) return 1.0f;
    return 1.0f / log_term;
}

/**
 * @brief Mixture pacing (soft threshold): w(d) = 1/(1 + exp((d-lambda)/C))
 */
static float pacing_mixture(float difficulty, float pace, float c) {
    if (c <= 0.0f) c = 0.1f;
    float exp_term = expf((difficulty - pace) / c);
    return 1.0f / (1.0f + exp_term);
}

/**
 * @brief Binary pacing (hard threshold): w(d) = (d < lambda) ? 1 : 0
 */
static float pacing_binary(float difficulty, float pace) {
    return (difficulty < pace) ? 1.0f : 0.0f;
}

/**
 * @brief Soft threshold pacing (sigmoid): w(d) = sigmoid(-(d - lambda) * scale)
 */
static float pacing_soft_threshold(float difficulty, float pace, float scale) {
    float z = -(difficulty - pace) * scale;
    return 1.0f / (1.0f + expf(-z));
}

/**
 * @brief Apply pacing function based on configuration
 */
static float apply_pacing(
    const curriculum_ctx_t* ctx,
    float difficulty
) {
    const spl_config_t* spl = &ctx->config.spl;
    float pace = ctx->current_pace;

    switch (spl->pacing_type) {
        case PACING_LINEAR:
            return pacing_linear(difficulty, pace);

        case PACING_LOGARITHMIC:
            return pacing_logarithmic(difficulty, pace);

        case PACING_MIXTURE:
            return pacing_mixture(difficulty, pace, spl->threshold_c);

        case PACING_BINARY:
            return pacing_binary(difficulty, pace);

        case PACING_SOFT_THRESHOLD:
            return pacing_soft_threshold(difficulty, pace, 10.0f);

        default:
            return 1.0f;  /* No pacing */
    }
}

//=============================================================================
// Sorting Utilities
//=============================================================================

/**
 * @brief Comparison context for qsort
 */
typedef struct {
    const float* difficulties;
    bool ascending;
} sort_context_t;

static sort_context_t g_sort_ctx;  /* Global for qsort - protected by g_sort_mutex */
static nimcp_platform_mutex_t g_sort_mutex;
static nimcp_platform_once_t g_sort_mutex_once = NIMCP_PLATFORM_ONCE_INIT;
static void init_sort_mutex(void) {
    nimcp_platform_mutex_init(&g_sort_mutex, false);
}

/**
 * @brief Comparison function for sorting indices by difficulty
 */
static int compare_by_difficulty(const void* a, const void* b) {
    uint32_t idx_a = *(const uint32_t*)a;
    uint32_t idx_b = *(const uint32_t*)b;
    float diff_a = g_sort_ctx.difficulties[idx_a];
    float diff_b = g_sort_ctx.difficulties[idx_b];

    /* NOTE: qsort comparators must be fast and side-effect-free.
     * No NIMCP_THROW_TO_IMMUNE calls allowed here. */
    if (g_sort_ctx.ascending) {
        if (diff_a < diff_b) return -1;
        if (diff_a > diff_b) return 1;
    } else {
        if (diff_a > diff_b) return -1;
        if (diff_a < diff_b) return 1;
    }
    return 0;
}

/**
 * @brief Sort indices by difficulty
 */
static void sort_indices(curriculum_ctx_t* ctx, bool ascending) {
    if (!ctx->sorted_indices) return;

    /* Initialize indices */
    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        ctx->sorted_indices[i] = i;
    }

    /* Thread-safe: lock mutex around g_sort_ctx access */
    nimcp_platform_once(&g_sort_mutex_once, init_sort_mutex);
    nimcp_platform_mutex_lock(&g_sort_mutex);

    /* Set up comparison context */
    g_sort_ctx.difficulties = ctx->difficulties;
    g_sort_ctx.ascending = ascending;

    /* Sort */
    qsort(ctx->sorted_indices, ctx->num_samples, sizeof(uint32_t),
          compare_by_difficulty);

    nimcp_platform_mutex_unlock(&g_sort_mutex);

    ctx->sorted_valid = true;
}

//=============================================================================
// Statistics Helpers
//=============================================================================

/**
 * @brief Update statistics after difficulty changes
 */
static void update_stats(curriculum_ctx_t* ctx) {
    if (!ctx || !ctx->difficulties) return;

    float sum = 0.0f;
    float min_d = FLT_MAX;
    float max_d = -FLT_MAX;
    uint32_t count = 0;

    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        if (ctx->initialized[i]) {
            float d = ctx->difficulties[i];
            sum += d;
            if (d < min_d) min_d = d;
            if (d > max_d) max_d = d;
            count++;
        }
    }

    if (count > 0) {
        ctx->stats.avg_difficulty = sum / (float)count;
        ctx->stats.min_difficulty = min_d;
        ctx->stats.max_difficulty = max_d;
        ctx->stats.total_samples_scored = count;

        /* Compute std dev */
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < ctx->num_samples; i++) {
            if (ctx->initialized[i]) {
                float diff = ctx->difficulties[i] - ctx->stats.avg_difficulty;
                sum_sq += diff * diff;
            }
        }
        ctx->stats.difficulty_std = sqrtf(sum_sq / (float)count);
    }

    ctx->stats.current_difficulty_threshold = ctx->current_threshold;
    ctx->stats.current_state = ctx->state;
    ctx->stats.epochs_completed = ctx->current_epoch;
}

//=============================================================================
// Public API Implementation
//=============================================================================

int curriculum_default_config(curriculum_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "curriculum_default_config: config is NULL");
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return -1;
    }

    memset(config, 0, sizeof(curriculum_config_t));

    /* Strategy */
    config->strategy = CURRICULUM_STRATEGY_SELF_PACED;
    config->metric = DIFFICULTY_METRIC_LOSS;

    /* Self-paced defaults */
    config->spl.pacing_type = PACING_MIXTURE;
    config->spl.initial_pace = 0.5f;
    config->spl.pace_increment = CURRICULUM_DEFAULT_GROWTH_RATE;
    config->spl.pace_max = 2.0f;
    config->spl.threshold_c = CURRICULUM_DEFAULT_PACING_C;
    config->spl.use_hard_samples = false;

    /* Teacher-guided defaults */
    config->teacher.difficulty_schedule = NULL;
    config->teacher.schedule_length = 0;
    config->teacher.initial_fraction = 0.2f;
    config->teacher.growth_rate = CURRICULUM_DEFAULT_GROWTH_RATE;
    config->teacher.smooth_transition = true;

    /* General */
    config->warmup_epochs = 0;
    config->num_difficulty_bins = 10;
    config->cache_difficulties = true;
    config->update_frequency = 1;

    /* Anti-curriculum */
    config->inject_hard_samples = false;
    config->hard_sample_ratio = 0.05f;

    /* Logging */
    config->verbose = false;

    return 0;
}

curriculum_ctx_t* curriculum_create(
    uint32_t num_samples,
    const curriculum_config_t* config
) {
    if (num_samples == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "curriculum_create: num_samples cannot be 0");
        NIMCP_LOGGING_ERROR("num_samples cannot be 0");
        return NULL;
    }

    /* Use defaults if not provided */
    curriculum_config_t default_config;
    if (!config) {
        curriculum_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate context */
    curriculum_ctx_t* ctx = (curriculum_ctx_t*)nimcp_calloc(
        1, sizeof(curriculum_ctx_t)
    );
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(curriculum_ctx_t),
                          "curriculum_create: failed to allocate context");
        NIMCP_LOGGING_ERROR("Failed to allocate curriculum context");
        /* P2-CL-3: Removed redundant NIMCP_THROW_TO_IMMUNE after NIMCP_THROW_MEMORY */
        return NULL;
    }

    /* Copy config */
    memcpy(&ctx->config, config, sizeof(curriculum_config_t));
    ctx->num_samples = num_samples;

    /* Allocate difficulty arrays */
    ctx->difficulties = (float*)nimcp_calloc(num_samples, sizeof(float));
    ctx->difficulty_ema = (float*)nimcp_calloc(num_samples, sizeof(float));
    ctx->update_counts = (uint32_t*)nimcp_calloc(num_samples, sizeof(uint32_t));
    ctx->initialized = (bool*)nimcp_calloc(num_samples, sizeof(bool));
    /* P2-CL-2: Check integer overflow before allocation */
    if (num_samples > SIZE_MAX / sizeof(uint32_t)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "curriculum_create: num_samples too large, allocation overflow");
        curriculum_destroy(ctx);
        return NULL;
    }
    ctx->sorted_indices = (uint32_t*)nimcp_malloc(num_samples * sizeof(uint32_t));

    if (!ctx->difficulties || !ctx->difficulty_ema || !ctx->update_counts ||
        !ctx->initialized || !ctx->sorted_indices) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * sizeof(float),
                          "curriculum_create: failed to allocate difficulty arrays");
        NIMCP_LOGGING_ERROR("Failed to allocate difficulty arrays");
        curriculum_destroy(ctx);
        return NULL;
    }

    /* Initialize indices */
    for (uint32_t i = 0; i < num_samples; i++) {
        ctx->sorted_indices[i] = i;
        ctx->difficulties[i] = 0.5f;  /* Start at medium difficulty */
    }

    /* Initialize state */
    ctx->state = (config->warmup_epochs > 0) ? CURRICULUM_STATE_WARMUP : CURRICULUM_STATE_EASY;
    ctx->current_pace = config->spl.initial_pace;
    ctx->current_threshold = config->spl.initial_pace;
    ctx->current_epoch = 0;
    ctx->sorted_valid = false;

    ctx->last_epoch_loss = FLT_MAX;
    ctx->best_epoch_loss = FLT_MAX;
    ctx->epochs_no_improve = 0;

    /* Initialize RNG */
    ctx->rng_state = 0x12345678DEADBEEF;

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(curriculum_stats_t));
    ctx->stats.num_bins = config->num_difficulty_bins;
    ctx->stats.bin_counts = (float*)nimcp_calloc(config->num_difficulty_bins, sizeof(float));

    NIMCP_LOGGING_INFO("Created curriculum context: %u samples, strategy=%s",
                       num_samples, curriculum_strategy_name(config->strategy));

    return ctx;
}

void curriculum_destroy(curriculum_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->difficulties) nimcp_free(ctx->difficulties);
    if (ctx->difficulty_ema) nimcp_free(ctx->difficulty_ema);
    if (ctx->update_counts) nimcp_free(ctx->update_counts);
    if (ctx->initialized) nimcp_free(ctx->initialized);
    if (ctx->sorted_indices) nimcp_free(ctx->sorted_indices);
    if (ctx->stats.bin_counts) nimcp_free(ctx->stats.bin_counts);

    nimcp_free(ctx);
}

int curriculum_set_difficulties(
    curriculum_ctx_t* ctx,
    const float* scores
) {
    if (!ctx || !scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_set_difficulties: required parameter is NULL (ctx, scores)");
        return -1;
    }

    memcpy(ctx->difficulties, scores, ctx->num_samples * sizeof(float));
    memcpy(ctx->difficulty_ema, scores, ctx->num_samples * sizeof(float));

    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        ctx->initialized[i] = true;
        ctx->update_counts[i] = 1;
    }

    ctx->sorted_valid = false;
    update_stats(ctx);

    return 0;
}

int curriculum_update_difficulty(
    curriculum_ctx_t* ctx,
    uint32_t sample_idx,
    float loss,
    const nimcp_tensor_t* prediction,
    const nimcp_tensor_t* target
) {
    if (!ctx || sample_idx >= ctx->num_samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curriculum_update_difficulty: ctx is NULL");
        return -1;
    }

    (void)prediction;  /* May be used for confidence-based metrics */
    (void)target;

    /* Compute difficulty based on metric */
    float new_difficulty = 0.0f;

    switch (ctx->config.metric) {
        case DIFFICULTY_METRIC_LOSS:
            /* Higher loss = harder sample */
            new_difficulty = loss;
            break;

        case DIFFICULTY_METRIC_CONFIDENCE:
            /* Lower confidence = harder (loss as proxy) */
            new_difficulty = loss;
            break;

        case DIFFICULTY_METRIC_GRADIENT_NORM:
            /* Would need gradient info - use loss as fallback */
            new_difficulty = loss;
            break;

        default:
            new_difficulty = loss;
            break;
    }

    /* EMA update */
    float alpha = 0.3f;  /* EMA decay */
    if (ctx->initialized[sample_idx]) {
        ctx->difficulty_ema[sample_idx] =
            alpha * new_difficulty + (1.0f - alpha) * ctx->difficulty_ema[sample_idx];
    } else {
        ctx->difficulty_ema[sample_idx] = new_difficulty;
        ctx->initialized[sample_idx] = true;
    }

    ctx->difficulties[sample_idx] = ctx->difficulty_ema[sample_idx];
    ctx->update_counts[sample_idx]++;

    ctx->sorted_valid = false;

    return 0;
}

int curriculum_update_difficulties_batch(
    curriculum_ctx_t* ctx,
    const uint32_t* sample_indices,
    const float* losses,
    uint32_t batch_size
) {
    if (!ctx || !sample_indices || !losses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_update_difficulties_batch: required parameter is NULL (ctx, sample_indices, losses)");
        return -1;
    }

    for (uint32_t i = 0; i < batch_size; i++) {
        curriculum_update_difficulty(ctx, sample_indices[i], losses[i], NULL, NULL);
    }

    return 0;
}

float curriculum_get_difficulty(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
) {
    if (!ctx || sample_idx >= ctx->num_samples) return -1.0f;
    if (!ctx->initialized[sample_idx]) return -1.0f;
    return ctx->difficulties[sample_idx];
}

float curriculum_get_percentile(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
) {
    if (!ctx || sample_idx >= ctx->num_samples) return -1.0f;
    if (!ctx->initialized[sample_idx]) return -1.0f;

    float difficulty = ctx->difficulties[sample_idx];
    uint32_t count_below = 0;

    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        if (ctx->initialized[i] && ctx->difficulties[i] < difficulty) {
            count_below++;
        }
    }

    return 100.0f * (float)count_below / (float)ctx->num_samples;
}

int curriculum_get_sample_weights(
    curriculum_ctx_t* ctx,
    float* weights
) {
    if (!ctx || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_get_sample_weights: required parameter is NULL (ctx, weights)");
        return -1;
    }

    /* If in warmup, all weights equal */
    if (ctx->state == CURRICULUM_STATE_WARMUP) {
        for (uint32_t i = 0; i < ctx->num_samples; i++) {
            weights[i] = 1.0f;
        }
        return 0;
    }

    /* If curriculum disabled, equal weights */
    if (ctx->config.strategy == CURRICULUM_STRATEGY_NONE) {
        for (uint32_t i = 0; i < ctx->num_samples; i++) {
            weights[i] = 1.0f;
        }
        return 0;
    }

    /* Apply pacing function to each sample */
    uint32_t selected = 0;
    uint32_t excluded = 0;

    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        float difficulty = ctx->difficulties[i];
        float weight = apply_pacing(ctx, difficulty);

        /* Anti-curriculum injection: occasionally include hard samples */
        if (ctx->config.inject_hard_samples && weight < 0.1f) {
            if (randf(&ctx->rng_state) < ctx->config.hard_sample_ratio) {
                weight = 0.5f;  /* Partial weight for hard samples */
            }
        }

        weights[i] = weight;

        if (weight > 0.01f) {
            selected++;
        } else {
            excluded++;
        }
    }

    ctx->stats.samples_selected = selected;
    ctx->stats.samples_excluded = excluded;

    return 0;
}

bool curriculum_should_include(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
) {
    if (!ctx || sample_idx >= ctx->num_samples) {
        return false;
    }

    /* Always include during warmup */
    if (ctx->state == CURRICULUM_STATE_WARMUP) return true;

    /* Always include if curriculum disabled */
    if (ctx->config.strategy == CURRICULUM_STRATEGY_NONE) return true;

    /* Always include if full curriculum */
    if (ctx->state == CURRICULUM_STATE_FULL) return true;

    float difficulty = ctx->difficulties[sample_idx];
    return difficulty <= ctx->current_threshold;
}

int curriculum_get_ordered_indices(
    const curriculum_ctx_t* ctx,
    uint32_t* indices,
    bool ascending
) {
    if (!ctx || !indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_get_ordered_indices: required parameter is NULL (ctx, indices)");
        return -1;
    }

    /* Need to sort */
    curriculum_ctx_t* mutable_ctx = (curriculum_ctx_t*)ctx;  /* For sorting */

    if (!mutable_ctx->sorted_valid) {
        sort_indices(mutable_ctx, ascending);
    } else {
        /* P2-CL-1: Read ascending flag inside g_sort_mutex to avoid data race */
        nimcp_platform_once(&g_sort_mutex_once, init_sort_mutex);
        nimcp_platform_mutex_lock(&g_sort_mutex);
        bool current_ascending = g_sort_ctx.ascending;
        nimcp_platform_mutex_unlock(&g_sort_mutex);
        if (current_ascending != ascending) {
            /* Wrong order, re-sort */
            sort_indices(mutable_ctx, ascending);
        }
    }

    memcpy(indices, ctx->sorted_indices, ctx->num_samples * sizeof(uint32_t));

    return 0;
}

int curriculum_select_samples(
    curriculum_ctx_t* ctx,
    uint32_t* indices,
    uint32_t max_samples,
    uint32_t* num_selected
) {
    if (!ctx || !indices || !num_selected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_select_samples: required parameter is NULL (ctx, indices, num_selected)");
        return -1;
    }

    /* Get ordered indices (easy first) */
    if (!ctx->sorted_valid) {
        sort_indices(ctx, true);
    }

    /* Select samples below threshold */
    uint32_t count = 0;

    for (uint32_t i = 0; i < ctx->num_samples && count < max_samples; i++) {
        uint32_t idx = ctx->sorted_indices[i];
        if (curriculum_should_include(ctx, idx)) {
            indices[count++] = idx;
        }
    }

    /* If anti-curriculum enabled, inject some hard samples */
    if (ctx->config.inject_hard_samples && count < max_samples) {
        uint32_t hard_to_inject = (uint32_t)(ctx->config.hard_sample_ratio * (float)count);
        if (hard_to_inject > max_samples - count) {
            hard_to_inject = max_samples - count;
        }

        /* Take from the hard end */
        for (uint32_t i = ctx->num_samples - 1; i > 0 && hard_to_inject > 0; i--) {
            uint32_t idx = ctx->sorted_indices[i];
            if (!curriculum_should_include(ctx, idx)) {
                indices[count++] = idx;
                hard_to_inject--;
            }
        }
    }

    *num_selected = count;
    ctx->stats.samples_selected = count;
    ctx->stats.samples_excluded = ctx->num_samples - count;

    return 0;
}

int curriculum_start_epoch(curriculum_ctx_t* ctx) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    ctx->current_epoch++;

    /* Check if warmup is complete */
    if (ctx->state == CURRICULUM_STATE_WARMUP) {
        if (ctx->current_epoch >= ctx->config.warmup_epochs) {
            ctx->state = CURRICULUM_STATE_EASY;
            if (ctx->config.verbose) {
                NIMCP_LOGGING_INFO("Curriculum: warmup complete, starting easy phase");
            }
        }
        return 0;
    }

    /* Advance pace/threshold based on strategy */
    switch (ctx->config.strategy) {
        case CURRICULUM_STRATEGY_SELF_PACED:
            /* Increase pace parameter */
            ctx->current_pace += ctx->config.spl.pace_increment;
            if (ctx->current_pace > ctx->config.spl.pace_max) {
                ctx->current_pace = ctx->config.spl.pace_max;
                ctx->state = CURRICULUM_STATE_FULL;
            }
            ctx->current_threshold = ctx->current_pace;
            break;

        case CURRICULUM_STRATEGY_TEACHER_GUIDED:
            /* Follow schedule */
            if (ctx->config.teacher.difficulty_schedule &&
                ctx->current_epoch < ctx->config.teacher.schedule_length) {
                ctx->current_threshold =
                    ctx->config.teacher.difficulty_schedule[ctx->current_epoch];
            } else {
                /* Grow by fixed rate */
                float fraction = ctx->config.teacher.initial_fraction +
                    ctx->current_epoch * ctx->config.teacher.growth_rate;
                if (fraction > 1.0f) fraction = 1.0f;
                ctx->current_threshold = fraction;
            }
            break;

        default:
            /* Other strategies: grow linearly */
            ctx->current_threshold += 0.1f;
            if (ctx->current_threshold > 1.5f) {
                ctx->current_threshold = 1.5f;
                ctx->state = CURRICULUM_STATE_FULL;
            }
            break;
    }

    /* Update state based on threshold */
    if (ctx->state != CURRICULUM_STATE_FULL) {
        if (ctx->current_threshold < 0.5f) {
            ctx->state = CURRICULUM_STATE_EASY;
        } else if (ctx->current_threshold < 1.0f) {
            ctx->state = CURRICULUM_STATE_MEDIUM;
        } else {
            ctx->state = CURRICULUM_STATE_FULL;
        }
    }

    /* Invalidate sorted cache */
    ctx->sorted_valid = false;

    if (ctx->config.verbose) {
        NIMCP_LOGGING_INFO("Curriculum: epoch %u, state=%s, threshold=%.3f",
                          ctx->current_epoch, curriculum_state_name(ctx->state),
                          ctx->current_threshold);
    }

    return 0;
}

int curriculum_end_epoch(
    curriculum_ctx_t* ctx,
    float epoch_loss,
    float epoch_accuracy
) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    (void)epoch_accuracy;  /* Could be used for adaptive pacing */

    /* Track improvement */
    if (epoch_loss < ctx->best_epoch_loss - 1e-4f) {
        ctx->best_epoch_loss = epoch_loss;
        ctx->epochs_no_improve = 0;
    } else {
        ctx->epochs_no_improve++;
    }

    ctx->last_epoch_loss = epoch_loss;

    /* Update statistics */
    update_stats(ctx);

    return 0;
}

curriculum_state_t curriculum_get_state(const curriculum_ctx_t* ctx) {
    return ctx ? ctx->state : CURRICULUM_STATE_WARMUP;
}

float curriculum_get_threshold(const curriculum_ctx_t* ctx) {
    return ctx ? ctx->current_threshold : 0.0f;
}

int curriculum_set_state(curriculum_ctx_t* ctx, curriculum_state_t state) {
    if (!ctx || state >= CURRICULUM_STATE_COUNT) {
        /* P2-CL-4: Use NIMCP_ERROR_INVALID_PARAM instead of NIMCP_ERROR_OUT_OF_RANGE */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curriculum_set_state: invalid ctx or state");
        return -1;
    }

    ctx->state = state;

    /* Set threshold based on state */
    switch (state) {
        case CURRICULUM_STATE_WARMUP:
            ctx->current_threshold = FLT_MAX;  /* Include all */
            break;
        case CURRICULUM_STATE_EASY:
            ctx->current_threshold = 0.33f;
            break;
        case CURRICULUM_STATE_MEDIUM:
            ctx->current_threshold = 0.66f;
            break;
        case CURRICULUM_STATE_FULL:
            ctx->current_threshold = FLT_MAX;
            break;
        default:
            break;
    }

    return 0;
}

int curriculum_get_stats(
    const curriculum_ctx_t* ctx,
    curriculum_stats_t* stats
) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }
    *stats = ctx->stats;
    return 0;
}

void curriculum_reset_stats(curriculum_ctx_t* ctx) {
    if (!ctx) return;

    /* Save bin_counts pointer before memset zeroes it */
    float* saved_bins = ctx->stats.bin_counts;
    uint32_t num_bins = ctx->config.num_difficulty_bins;

    memset(&ctx->stats, 0, sizeof(curriculum_stats_t));
    ctx->stats.num_bins = num_bins;

    /* Restore bin_counts pointer and zero the bins */
    ctx->stats.bin_counts = saved_bins;
    if (saved_bins) {
        memset(saved_bins, 0, num_bins * sizeof(float));
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* curriculum_strategy_name(curriculum_strategy_t strategy) {
    switch (strategy) {
        case CURRICULUM_STRATEGY_NONE:           return "None";
        case CURRICULUM_STRATEGY_SELF_PACED:     return "SelfPaced";
        case CURRICULUM_STRATEGY_TEACHER_GUIDED: return "TeacherGuided";
        case CURRICULUM_STRATEGY_TRANSFER:       return "Transfer";
        case CURRICULUM_STRATEGY_UNCERTAINTY:    return "Uncertainty";
        case CURRICULUM_STRATEGY_LOSS_BASED:     return "LossBased";
        case CURRICULUM_STRATEGY_GRADIENT_NORM:  return "GradientNorm";
        case CURRICULUM_STRATEGY_CONFIDENCE:     return "Confidence";
        case CURRICULUM_STRATEGY_ANTI_CURRICULUM: return "AntiCurriculum";
        case CURRICULUM_STRATEGY_HYBRID:         return "Hybrid";
        default:                                 return "Unknown";
    }
}

const char* curriculum_metric_name(difficulty_metric_t metric) {
    switch (metric) {
        case DIFFICULTY_METRIC_LOSS:        return "Loss";
        case DIFFICULTY_METRIC_CONFIDENCE:  return "Confidence";
        case DIFFICULTY_METRIC_GRADIENT_NORM: return "GradientNorm";
        case DIFFICULTY_METRIC_ENTROPY:     return "Entropy";
        case DIFFICULTY_METRIC_MARGIN:      return "Margin";
        case DIFFICULTY_METRIC_VARIANCE:    return "Variance";
        case DIFFICULTY_METRIC_TEACHER:     return "Teacher";
        case DIFFICULTY_METRIC_CUSTOM:      return "Custom";
        default:                            return "Unknown";
    }
}

const char* curriculum_pacing_name(pacing_function_t pacing) {
    switch (pacing) {
        case PACING_LINEAR:         return "Linear";
        case PACING_LOGARITHMIC:    return "Logarithmic";
        case PACING_MIXTURE:        return "Mixture";
        case PACING_BINARY:         return "Binary";
        case PACING_SOFT_THRESHOLD: return "SoftThreshold";
        default:                    return "Unknown";
    }
}

const char* curriculum_state_name(curriculum_state_t state) {
    switch (state) {
        case CURRICULUM_STATE_WARMUP: return "Warmup";
        case CURRICULUM_STATE_EASY:   return "Easy";
        case CURRICULUM_STATE_MEDIUM: return "Medium";
        case CURRICULUM_STATE_FULL:   return "Full";
        default:                      return "Unknown";
    }
}

int curriculum_compute_difficulty_from_loss(
    const float* losses,
    uint32_t num_samples,
    float* difficulties
) {
    if (!losses || !difficulties || num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curriculum_compute_difficulty_from_loss: required parameter is NULL (losses, difficulties)");
        return -1;
    }

    /* Find min/max for normalization */
    float min_loss = losses[0];
    float max_loss = losses[0];

    for (uint32_t i = 1; i < num_samples; i++) {
        if (losses[i] < min_loss) min_loss = losses[i];
        if (losses[i] > max_loss) max_loss = losses[i];
    }

    float range = max_loss - min_loss;
    if (range < 1e-8f) range = 1.0f;

    /* Normalize to [0, 1] */
    for (uint32_t i = 0; i < num_samples; i++) {
        difficulties[i] = (losses[i] - min_loss) / range;
    }

    return 0;
}
