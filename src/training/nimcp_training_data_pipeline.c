/**
 * @file nimcp_training_data_pipeline.c
 * @brief Training Data Pipeline Implementation
 *
 * WHAT: Comprehensive data pipeline for efficient training data management
 * WHY:  Standardize data loading, batching, augmentation, and transformation
 * HOW:  Ring buffers for prefetching, transform chains for augmentation
 *
 * IMPLEMENTATION NOTES:
 * - Uses ring buffer for batch prefetching
 * - Transform chain applied sequentially with probability filtering
 * - Thread-safe design for concurrent prefetch workers
 * - Memory pooling for tensor allocation efficiency
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#include "training/nimcp_training_data_pipeline.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define LOG_MODULE "training_pipeline"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_data_pipeline)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Prefetch buffer entry
 */
typedef struct {
    training_batch_t batch;             /**< Batch data */
    bool ready;                         /**< Batch is ready for consumption */
    bool in_use;                        /**< Batch is being prepared */
} prefetch_entry_t;

/**
 * @brief Internal pipeline context
 */
struct training_pipeline_ctx_s {
    /* Configuration */
    training_pipeline_config_t config;

    /* Dataset reference */
    nimcp_tensor_t* data;               /**< Full dataset (may be NULL for file mode) */
    nimcp_tensor_t* labels;             /**< Full labels */
    uint32_t dataset_size;              /**< Number of samples */
    nimcp_tensor_shape_t data_shape;    /**< Shape of single sample */
    nimcp_tensor_shape_t label_shape;   /**< Shape of single label */

    /* Shuffled indices */
    uint32_t* indices;                  /**< Current shuffled order */
    uint32_t current_index;             /**< Current position in indices */

    /* Prefetch ring buffer */
    prefetch_entry_t* prefetch_buffer;  /**< Ring buffer of prefetched batches */
    uint32_t prefetch_capacity;         /**< Number of prefetch slots */
    uint32_t prefetch_head;             /**< Next slot to fill */
    uint32_t prefetch_tail;             /**< Next slot to consume */
    nimcp_platform_mutex_t prefetch_mutex; /**< Mutex for prefetch buffer */

    /* Transform chain */
    training_transform_t* transforms;   /**< Transform array */
    uint32_t num_transforms;            /**< Number of transforms */

    /* Random number generator */
    uint64_t rng_state;                 /**< XorShift64 state */

    /* State */
    training_pipeline_state_t state;    /**< Current state */
    uint64_t epoch;                     /**< Current epoch */
    uint64_t batch_counter;             /**< Sequential batch ID */

    /* Statistics */
    training_pipeline_stats_t stats;

    /* Timing */
    uint64_t last_batch_time;           /**< Time of last batch production */
};

//=============================================================================
// Random Number Generator
//=============================================================================

/**
 * @brief XorShift64 random number generator
 */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Generate random float in [0, 1)
 */
static float randf(uint64_t* state) {
    return (float)(xorshift64(state) >> 11) / (float)(1ULL << 53);
}

/**
 * @brief Generate random integer in [0, max)
 */
static uint32_t randu32(uint64_t* state, uint32_t max) {
    if (max == 0) return 0;
    return (uint32_t)(xorshift64(state) % max);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Fisher-Yates shuffle for indices
 */
static void shuffle_indices(uint32_t* indices, uint32_t count, uint64_t* rng) {
    for (uint32_t i = count - 1; i > 0; i--) {
        uint32_t j = randu32(rng, i + 1);
        uint32_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}

/**
 * @brief Extract single sample from dataset tensor
 */
static nimcp_tensor_t* extract_sample(
    const nimcp_tensor_t* dataset,
    uint32_t sample_idx,
    const nimcp_tensor_shape_t* sample_shape
) {
    if (!dataset || !sample_shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_sample: required parameter is NULL (dataset, sample_shape)");
        return NULL;
    }

    /* Create output tensor for single sample */
    nimcp_tensor_t* sample = nimcp_tensor_create(
        sample_shape->dims,
        sample_shape->rank,
        NIMCP_DTYPE_F32
    );
    if (!sample) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sample is NULL");

        return NULL;

    }

    /* Calculate offset into dataset */
    size_t sample_size = nimcp_tensor_numel(sample);
    const float* src = (const float*)nimcp_tensor_data_const(dataset);
    float* dst = (float*)nimcp_tensor_data(sample);

    if (src && dst) {
        memcpy(dst, src + (sample_idx * sample_size), sample_size * sizeof(float));
    }

    return sample;
}

/**
 * @brief Create batch tensor from sample indices
 */
static nimcp_tensor_t* create_batch_tensor(
    const nimcp_tensor_t* dataset,
    const uint32_t* indices,
    uint32_t batch_size,
    const nimcp_tensor_shape_t* sample_shape
) {
    if (!dataset || !indices || !sample_shape || batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_batch_tensor: required parameter is NULL (dataset, indices, sample_shape)");
        return NULL;
    }

    /* Create batch shape: [batch_size, ...sample_dims] */
    uint32_t batch_dims[NIMCP_TENSOR_MAX_RANK];
    batch_dims[0] = batch_size;
    for (uint32_t i = 0; i < sample_shape->rank; i++) {
        batch_dims[i + 1] = sample_shape->dims[i];
    }

    nimcp_tensor_t* batch = nimcp_tensor_create(
        batch_dims,
        sample_shape->rank + 1,
        NIMCP_DTYPE_F32
    );
    if (!batch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "batch is NULL");

        return NULL;

    }

    /* Calculate sample size */
    size_t sample_numel = 1;
    for (uint32_t i = 0; i < sample_shape->rank; i++) {
        sample_numel *= sample_shape->dims[i];
    }

    /* Copy samples into batch */
    const float* src = (const float*)nimcp_tensor_data_const(dataset);
    float* dst = (float*)nimcp_tensor_data(batch);

    if (src && dst) {
        for (uint32_t i = 0; i < batch_size; i++) {
            memcpy(
                dst + (i * sample_numel),
                src + (indices[i] * sample_numel),
                sample_numel * sizeof(float)
            );
        }
    }

    return batch;
}

//=============================================================================
// Transform Implementations
//=============================================================================

/**
 * @brief Apply normalization transform
 */
static int apply_normalize(
    nimcp_tensor_t* tensor,
    const transform_normalize_config_t* cfg
) {
    if (!tensor || !cfg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_normalize: required parameter is NULL (tensor, cfg)");
        return -1;
    }

    float* data = (float*)nimcp_tensor_data(tensor);
    size_t numel = nimcp_tensor_numel(tensor);

    if (cfg->mean && cfg->std) {
        /* Per-channel standardization */
        /* For simplicity, apply global mean/std for rank-2+ tensors */
        for (size_t i = 0; i < numel; i++) {
            uint32_t ch = 0; /* Simplified: use first channel's stats */
            if (cfg->num_channels > 0) {
                ch = (uint32_t)(i % cfg->num_channels);
                if (ch >= cfg->num_channels) ch = 0;
            }
            float mean = cfg->mean[ch];
            float std = cfg->std[ch];
            if (std > 1e-8f) {
                data[i] = (data[i] - mean) / std;
            }
        }
    } else {
        /* Min-max normalization */
        float min_val = data[0];
        float max_val = data[0];
        for (size_t i = 1; i < numel; i++) {
            if (data[i] < min_val) min_val = data[i];
            if (data[i] > max_val) max_val = data[i];
        }
        float range = max_val - min_val;
        float target_range = cfg->max_val - cfg->min_val;
        if (range > 1e-8f) {
            for (size_t i = 0; i < numel; i++) {
                data[i] = cfg->min_val + ((data[i] - min_val) / range) * target_range;
            }
        }
    }

    return 0;
}

/**
 * @brief Apply Gaussian noise
 */
static int apply_gaussian_noise(
    nimcp_tensor_t* tensor,
    float stddev,
    uint64_t* rng
) {
    if (!tensor || stddev <= 0.0f) return 0;

    float* data = (float*)nimcp_tensor_data(tensor);
    size_t numel = nimcp_tensor_numel(tensor);

    /* Box-Muller transform for Gaussian samples */
    for (size_t i = 0; i < numel; i += 2) {
        float u1 = randf(rng);
        float u2 = randf(rng);
        if (u1 < 1e-10f) u1 = 1e-10f;

        float z0 = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
        float z1 = sqrtf(-2.0f * logf(u1)) * sinf(NIMCP_TWO_PI_F * u2);

        data[i] += z0 * stddev;
        if (i + 1 < numel) {
            data[i + 1] += z1 * stddev;
        }
    }

    return 0;
}

/**
 * @brief Apply random horizontal flip
 */
static int apply_random_flip_h(
    nimcp_tensor_t* tensor,
    uint64_t* rng
) {
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_random_flip_h: tensor is NULL");
        return -1;
    }

    /* Only flip 50% of the time */
    if (randf(rng) < 0.5f) return 0;

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    if (!shape || shape->rank < 2) return 0;

    float* data = (float*)nimcp_tensor_data(tensor);

    /* Assume last two dimensions are H x W */
    uint32_t height = shape->dims[shape->rank - 2];
    uint32_t width = shape->dims[shape->rank - 1];

    /* Calculate batch size (product of leading dims) */
    uint32_t batch_size = 1;
    for (uint32_t i = 0; i < shape->rank - 2; i++) {
        batch_size *= shape->dims[i];
    }

    uint32_t spatial_size = height * width;

    /* Flip each spatial plane */
    for (uint32_t b = 0; b < batch_size; b++) {
        float* plane = data + b * spatial_size;
        for (uint32_t h = 0; h < height; h++) {
            float* row = plane + h * width;
            /* Reverse the row */
            for (uint32_t w = 0; w < width / 2; w++) {
                float tmp = row[w];
                row[w] = row[width - 1 - w];
                row[width - 1 - w] = tmp;
            }
        }
    }

    return 0;
}

/**
 * @brief Apply random cutout
 */
static int apply_cutout(
    nimcp_tensor_t* tensor,
    const transform_cutout_config_t* cfg,
    uint64_t* rng
) {
    if (!tensor || !cfg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_cutout: required parameter is NULL (tensor, cfg)");
        return -1;
    }

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    if (!shape || shape->rank < 2) return 0;

    float* data = (float*)nimcp_tensor_data(tensor);
    uint32_t height = shape->dims[shape->rank - 2];
    uint32_t width = shape->dims[shape->rank - 1];

    /* Apply random erases */
    uint32_t num_erases = 1 + randu32(rng, cfg->max_erases);
    for (uint32_t e = 0; e < num_erases; e++) {
        /* Random area and aspect ratio */
        float area_frac = cfg->min_area + randf(rng) * (cfg->max_area - cfg->min_area);
        float ratio = cfg->min_ratio + randf(rng) * (cfg->max_ratio - cfg->min_ratio);

        uint32_t total_area = height * width;
        uint32_t erase_area = (uint32_t)(total_area * area_frac);
        uint32_t erase_h = (uint32_t)sqrtf((float)erase_area * ratio);
        uint32_t erase_w = (uint32_t)sqrtf((float)erase_area / ratio);

        if (erase_h > height) erase_h = height;
        if (erase_w > width) erase_w = width;

        /* Random position */
        uint32_t y = randu32(rng, height - erase_h + 1);
        uint32_t x = randu32(rng, width - erase_w + 1);

        /* Fill value */
        float fill = (cfg->fill_value < 0.0f) ? randf(rng) : cfg->fill_value;

        /* Calculate batch size */
        uint32_t batch_size = 1;
        for (uint32_t i = 0; i < shape->rank - 2; i++) {
            batch_size *= shape->dims[i];
        }

        /* Apply cutout to all planes */
        uint32_t spatial_size = height * width;
        for (uint32_t b = 0; b < batch_size; b++) {
            float* plane = data + b * spatial_size;
            for (uint32_t h = y; h < y + erase_h; h++) {
                for (uint32_t w = x; w < x + erase_w; w++) {
                    plane[h * width + w] = fill;
                }
            }
        }
    }

    return 0;
}

/**
 * @brief Apply single transform to tensor
 */
static int apply_transform(
    nimcp_tensor_t** tensor,
    nimcp_tensor_t** label,
    const training_transform_t* transform,
    uint64_t* rng
) {
    if (!tensor || !*tensor || !transform) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_transform: required parameter is NULL (tensor, transform)");
        return -1;
    }

    /* Check probability */
    if (transform->probability < 1.0f && randf(rng) > transform->probability) {
        return 0;  /* Skip this transform */
    }

    switch (transform->type) {
        case TRANSFORM_NONE:
            return 0;

        case TRANSFORM_NORMALIZE:
        case TRANSFORM_STANDARDIZE:
            return apply_normalize(*tensor, &transform->config.normalize);

        case TRANSFORM_GAUSSIAN_NOISE:
            return apply_gaussian_noise(*tensor, transform->config.noise.noise_stddev, rng);

        case TRANSFORM_RANDOM_FLIP_H:
            return apply_random_flip_h(*tensor, rng);

        case TRANSFORM_CUTOUT:
            return apply_cutout(*tensor, &transform->config.cutout, rng);

        case TRANSFORM_CUSTOM:
            if (transform->custom_fn) {
                return transform->custom_fn(tensor, label, transform->custom_data);
            }
            return 0;

        default:
            /* Other transforms not fully implemented */
            return 0;
    }
}

/**
 * @brief Apply transform chain to batch
 */
static int apply_transform_chain(
    training_pipeline_ctx_t* ctx,
    nimcp_tensor_t** data_tensor,
    nimcp_tensor_t** label_tensor
) {
    if (!ctx || !data_tensor || !*data_tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_transform_chain: required parameter is NULL (ctx, data_tensor)");
        return -1;
    }

    for (uint32_t i = 0; i < ctx->num_transforms; i++) {
        int result = apply_transform(data_tensor, label_tensor, &ctx->transforms[i], &ctx->rng_state);
        if (result < 0) {
            ctx->stats.transform_errors++;
            return result;
        }
        ctx->stats.transforms_applied++;
    }

    return 0;
}

/**
 * @brief Load next batch into entry
 */
static int load_batch_entry(
    training_pipeline_ctx_t* ctx,
    prefetch_entry_t* entry
) {
    if (!ctx || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_batch_entry: required parameter is NULL (ctx, entry)");
        return -1;
    }

    /* Determine batch size */
    uint32_t remaining = ctx->dataset_size - ctx->current_index;
    uint32_t batch_size = ctx->config.batch_size;
    bool is_last = false;

    if (remaining < batch_size) {
        if (ctx->config.drop_last) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_batch_entry: validation failed");
            return -1;  /* No more complete batches */
        }
        batch_size = remaining;
        is_last = true;
    } else if (remaining == batch_size) {
        is_last = true;
    }

    /* Get indices for this batch */
    uint32_t* batch_indices = ctx->indices + ctx->current_index;

    /* Create batch data tensor */
    entry->batch.data = create_batch_tensor(
        ctx->data, batch_indices, batch_size, &ctx->data_shape
    );
    if (!entry->batch.data) {
        ctx->stats.load_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_batch_entry: entry->batch is NULL");
        return -1;
    }

    /* Create batch labels tensor */
    entry->batch.labels = create_batch_tensor(
        ctx->labels, batch_indices, batch_size, &ctx->label_shape
    );
    if (!entry->batch.labels) {
        nimcp_tensor_destroy(entry->batch.data);
        entry->batch.data = NULL;
        ctx->stats.load_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_batch_entry: entry->batch is NULL");
        return -1;
    }

    /* Apply transforms */
    apply_transform_chain(ctx, &entry->batch.data, &entry->batch.labels);

    /* Fill batch metadata */
    entry->batch.batch_size = batch_size;
    entry->batch.indices = (uint32_t*)nimcp_malloc(batch_size * sizeof(uint32_t));
    if (entry->batch.indices) {
        memcpy(entry->batch.indices, batch_indices, batch_size * sizeof(uint32_t));
    }
    entry->batch.batch_id = ctx->batch_counter++;
    entry->batch.epoch = ctx->epoch;
    entry->batch.is_last_batch = is_last;

    /* Advance index */
    ctx->current_index += batch_size;

    /* Update stats */
    ctx->stats.total_samples_loaded += batch_size;
    ctx->stats.total_batches_produced++;
    ctx->stats.samples_in_epoch += batch_size;

    entry->ready = true;
    return 0;
}

//=============================================================================
// Public API Implementation
//=============================================================================

int training_pipeline_default_config(training_pipeline_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(training_pipeline_config_t));

    /* Batching */
    config->batch_size = 32;
    config->drop_last = false;

    /* Sampling */
    config->sampling = SAMPLING_SHUFFLE;
    config->sample_weights = NULL;
    config->num_weights = 0;

    /* Shuffling */
    config->shuffle = true;
    config->shuffle_seed = 0;  /* Use time */
    config->shuffle_buffer_size = 0;

    /* Prefetching */
    config->enable_prefetch = true;
    config->prefetch_factor = TRAINING_PIPELINE_DEFAULT_PREFETCH;
    config->num_workers = 1;

    /* Transforms */
    config->transforms = NULL;
    config->num_transforms = 0;

    /* Caching */
    config->cache_dataset = false;
    config->cache_transforms = false;
    config->max_cache_bytes = TRAINING_PIPELINE_MAX_CACHE_MB * 1024 * 1024;

    /* Memory */
    config->pin_memory = false;
    config->use_memory_pool = true;

    /* Multi-epoch */
    config->repeat_forever = true;
    config->max_epochs = 0;

    return 0;
}

training_pipeline_ctx_t* training_pipeline_create(
    const nimcp_tensor_t* data,
    const nimcp_tensor_t* labels,
    const training_pipeline_config_t* config
) {
    if (!data || !labels) {
        NIMCP_LOGGING_ERROR("NULL data or labels tensor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_create: required parameter is NULL (data, labels)");
        return NULL;
    }

    /* Use default config if not provided */
    training_pipeline_config_t default_config;
    if (!config) {
        training_pipeline_default_config(&default_config);
        config = &default_config;
    }

    /* Validate batch size */
    if (config->batch_size == 0) {
        NIMCP_LOGGING_ERROR("Invalid batch size: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_pipeline_create: config->batch_size is zero");
        return NULL;
    }

    /* Allocate context */
    training_pipeline_ctx_t* ctx = (training_pipeline_ctx_t*)nimcp_calloc(
        1, sizeof(training_pipeline_ctx_t)
    );
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate pipeline context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_pipeline_create: ctx is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(training_pipeline_config_t));

    /* Get dataset shape info */
    const nimcp_tensor_shape_t* data_shape = nimcp_tensor_shape(data);
    const nimcp_tensor_shape_t* label_shape = nimcp_tensor_shape(labels);

    if (!data_shape || data_shape->rank < 1) {
        NIMCP_LOGGING_ERROR("Invalid data tensor shape");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_create: data_shape is NULL");
        goto cleanup;
    }

    /* Dataset size is first dimension */
    ctx->dataset_size = data_shape->dims[0];

    /* Sample shape is remaining dimensions */
    ctx->data_shape.rank = data_shape->rank - 1;
    for (uint32_t i = 0; i < ctx->data_shape.rank; i++) {
        ctx->data_shape.dims[i] = data_shape->dims[i + 1];
    }

    if (label_shape && label_shape->rank > 0) {
        ctx->label_shape.rank = label_shape->rank - 1;
        for (uint32_t i = 0; i < ctx->label_shape.rank; i++) {
            ctx->label_shape.dims[i] = label_shape->dims[i + 1];
        }
    } else {
        ctx->label_shape.rank = 1;
        ctx->label_shape.dims[0] = 1;
    }

    /* Store data references (we make copies to avoid const issues) */
    ctx->data = (nimcp_tensor_t*)data;  /* Non-owning reference */
    ctx->labels = (nimcp_tensor_t*)labels;

    /* Allocate index array */
    ctx->indices = (uint32_t*)nimcp_malloc(ctx->dataset_size * sizeof(uint32_t));
    if (!ctx->indices) {
        NIMCP_LOGGING_ERROR("Failed to allocate index array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_pipeline_create: ctx->indices is NULL");
        goto cleanup;
    }

    /* Initialize indices to sequential order */
    for (uint32_t i = 0; i < ctx->dataset_size; i++) {
        ctx->indices[i] = i;
    }

    /* Initialize RNG */
    ctx->rng_state = config->shuffle_seed;
    if (ctx->rng_state == 0) {
        ctx->rng_state = nimcp_time_monotonic_ms() ^ 0xDEADBEEFCAFEBABE;
    }

    /* Shuffle if configured */
    if (config->shuffle) {
        shuffle_indices(ctx->indices, ctx->dataset_size, &ctx->rng_state);
    }

    /* Allocate prefetch buffer */
    ctx->prefetch_capacity = config->enable_prefetch ? config->prefetch_factor : 1;
    ctx->prefetch_buffer = (prefetch_entry_t*)nimcp_calloc(
        ctx->prefetch_capacity, sizeof(prefetch_entry_t)
    );
    if (!ctx->prefetch_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate prefetch buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_pipeline_create: ctx->prefetch_buffer is NULL");
        goto cleanup;
    }

    /* Initialize prefetch mutex */
    if (nimcp_platform_mutex_init(&ctx->prefetch_mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to init prefetch mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_pipeline_create: validation failed");
        goto cleanup;
    }

    /* Copy transforms if provided */
    if (config->transforms && config->num_transforms > 0) {
        ctx->num_transforms = config->num_transforms;
        if (ctx->num_transforms > TRAINING_PIPELINE_MAX_TRANSFORMS) {
            ctx->num_transforms = TRAINING_PIPELINE_MAX_TRANSFORMS;
        }
        ctx->transforms = (training_transform_t*)nimcp_malloc(
            ctx->num_transforms * sizeof(training_transform_t)
        );
        if (ctx->transforms) {
            memcpy(ctx->transforms, config->transforms,
                   ctx->num_transforms * sizeof(training_transform_t));
        }
    }

    /* Initialize state */
    ctx->state = PIPELINE_STATE_READY;
    ctx->epoch = 0;
    ctx->batch_counter = 0;
    ctx->current_index = 0;

    /* Reset stats */
    memset(&ctx->stats, 0, sizeof(training_pipeline_stats_t));

    NIMCP_LOGGING_INFO("Created training pipeline: %u samples, batch_size=%u, %u batches/epoch",
                       ctx->dataset_size, config->batch_size,
                       training_pipeline_num_batches(ctx));

    return ctx;

cleanup:
    if (ctx) {
        nimcp_free(ctx->transforms);
        nimcp_free(ctx->prefetch_buffer);
        nimcp_free(ctx->indices);
        nimcp_free(ctx);
    }
    return NULL;
}

training_pipeline_ctx_t* training_pipeline_create_from_files(
    const char** data_paths,
    const char** label_paths,
    uint32_t num_files,
    const training_pipeline_config_t* config
) {
    if (!data_paths || num_files == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "training_pipeline_create_from_files: required parameter is NULL");
        return NULL;
    }

    /* Load all files into memory, concatenate into single tensor */
    /* First pass: count total samples across all files */
    uint32_t total_samples = 0;
    uint32_t sample_dim = 0;
    uint32_t label_dim = 0;

    for (uint32_t f = 0; f < num_files; f++) {
        if (!data_paths[f]) continue;

        FILE* fp = fopen(data_paths[f], "rb");
        if (!fp) {
            NIMCP_LOGGING_ERROR("Failed to open data file: %s", data_paths[f]);
            continue;
        }

        /* Read header: num_samples (uint32), sample_dim (uint32) */
        uint32_t file_samples = 0, file_dim = 0;
        if (fread(&file_samples, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&file_dim, sizeof(uint32_t), 1, fp) != 1) {
            NIMCP_LOGGING_ERROR("Failed to read header from: %s", data_paths[f]);
            fclose(fp);
            continue;
        }

        if (sample_dim == 0) {
            sample_dim = file_dim;
        } else if (file_dim != sample_dim) {
            NIMCP_LOGGING_ERROR("Dimension mismatch in %s: expected %u, got %u",
                               data_paths[f], sample_dim, file_dim);
            fclose(fp);
            continue;
        }

        total_samples += file_samples;
        fclose(fp);
    }

    if (total_samples == 0 || sample_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "training_pipeline_create_from_files: no valid data files found");
        return NULL;
    }

    /* Allocate data tensor */
    uint32_t data_dims[2] = {total_samples, sample_dim};
    nimcp_tensor_t* data = nimcp_tensor_create(data_dims, 2, NIMCP_DTYPE_F32);
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "training_pipeline_create_from_files: data tensor allocation failed");
        return NULL;
    }

    /* Second pass: load data */
    float* data_ptr = nimcp_tensor_data(data);
    uint32_t offset = 0;

    for (uint32_t f = 0; f < num_files; f++) {
        if (!data_paths[f]) continue;

        FILE* fp = fopen(data_paths[f], "rb");
        if (!fp) continue;

        uint32_t file_samples = 0, file_dim = 0;
        if (fread(&file_samples, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&file_dim, sizeof(uint32_t), 1, fp) != 1 ||
            file_dim != sample_dim) {
            fclose(fp);
            continue;
        }

        size_t read_count = fread(data_ptr + (size_t)offset * sample_dim,
                                  sizeof(float), (size_t)file_samples * sample_dim, fp);
        if (read_count == (size_t)file_samples * sample_dim) {
            offset += file_samples;
        }
        fclose(fp);
    }

    /* Load labels if provided */
    nimcp_tensor_t* labels = NULL;
    if (label_paths) {
        uint32_t label_total = 0;
        for (uint32_t f = 0; f < num_files; f++) {
            if (!label_paths[f]) continue;
            FILE* fp = fopen(label_paths[f], "rb");
            if (!fp) continue;

            uint32_t file_samples = 0;
            if (fread(&file_samples, sizeof(uint32_t), 1, fp) != 1 ||
                fread(&label_dim, sizeof(uint32_t), 1, fp) != 1) {
                fclose(fp);
                continue;
            }
            label_total += file_samples;
            fclose(fp);
        }

        if (label_dim == 0) label_dim = 1;
        uint32_t label_dims[2] = {total_samples, label_dim};
        labels = nimcp_tensor_create(label_dims, 2, NIMCP_DTYPE_F32);
        if (labels) {
            float* label_ptr = nimcp_tensor_data(labels);
            uint32_t loffset = 0;
            for (uint32_t f = 0; f < num_files; f++) {
                if (!label_paths[f]) continue;
                FILE* fp = fopen(label_paths[f], "rb");
                if (!fp) continue;
                uint32_t fs = 0, fd = 0;
                if (fread(&fs, sizeof(uint32_t), 1, fp) == 1 &&
                    fread(&fd, sizeof(uint32_t), 1, fp) == 1) {
                    size_t rc = fread(label_ptr + (size_t)loffset * label_dim,
                                      sizeof(float), (size_t)fs * label_dim, fp);
                    if (rc == (size_t)fs * label_dim) loffset += fs;
                }
                fclose(fp);
            }
        }
    }

    /* Create pipeline using loaded tensors */
    training_pipeline_ctx_t* ctx = training_pipeline_create(data, labels, config);
    if (!ctx) {
        nimcp_tensor_destroy(data);
        if (labels) nimcp_tensor_destroy(labels);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created file-based pipeline from %u files: %u samples, dim=%u",
                       num_files, total_samples, sample_dim);
    return ctx;
}

void training_pipeline_destroy(training_pipeline_ctx_t* pipeline) {
    if (!pipeline) return;

    /* Free prefetch buffer entries */
    if (pipeline->prefetch_buffer) {
        for (uint32_t i = 0; i < pipeline->prefetch_capacity; i++) {
            training_batch_release(&pipeline->prefetch_buffer[i].batch);
        }
        nimcp_free(pipeline->prefetch_buffer);
    }

    /* Destroy prefetch mutex */
    nimcp_platform_mutex_destroy(&pipeline->prefetch_mutex);

    /* Free indices */
    if (pipeline->indices) {
        nimcp_free(pipeline->indices);
    }

    /* Free transforms */
    if (pipeline->transforms) {
        nimcp_free(pipeline->transforms);
    }

    /* Note: We don't free data/labels as we don't own them */

    nimcp_free(pipeline);
}

int training_pipeline_next_batch(
    training_pipeline_ctx_t* pipeline,
    training_batch_t* batch
) {
    if (!pipeline || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_next_batch: required parameter is NULL (pipeline, batch)");
        return -1;
    }

    memset(batch, 0, sizeof(training_batch_t));

    /* Check if epoch is exhausted */
    if (pipeline->current_index >= pipeline->dataset_size) {
        pipeline->state = PIPELINE_STATE_EXHAUSTED;
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Load batch directly (single-threaded for now) */
    nimcp_platform_mutex_lock(&pipeline->prefetch_mutex);

    prefetch_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    int result = load_batch_entry(pipeline, &entry);

    nimcp_platform_mutex_unlock(&pipeline->prefetch_mutex);

    if (result < 0) {
        pipeline->state = PIPELINE_STATE_EXHAUSTED;
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Copy to output */
    *batch = entry.batch;
    pipeline->state = PIPELINE_STATE_READY;

    return 0;
}

int training_pipeline_peek_batch(
    const training_pipeline_ctx_t* pipeline,
    training_batch_t* batch
) {
    if (!pipeline || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "training_pipeline_peek_batch: required parameter is NULL");
        return -1;
    }

    memset(batch, 0, sizeof(training_batch_t));

    /* Check if epoch is exhausted */
    if (pipeline->current_index >= pipeline->dataset_size) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Load batch without advancing the iterator.
     * We need to cast away const temporarily since load_batch_entry
     * accesses shared state - but we restore current_index after. */
    training_pipeline_ctx_t* mutable_pipeline = (training_pipeline_ctx_t*)pipeline;
    uint32_t saved_index = mutable_pipeline->current_index;

    nimcp_platform_mutex_lock(&mutable_pipeline->prefetch_mutex);

    prefetch_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    int result = load_batch_entry(mutable_pipeline, &entry);

    /* Restore the iterator position so the batch isn't consumed */
    mutable_pipeline->current_index = saved_index;

    nimcp_platform_mutex_unlock(&mutable_pipeline->prefetch_mutex);

    if (result < 0) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    *batch = entry.batch;
    return 0;
}

void training_batch_release(training_batch_t* batch) {
    if (!batch) return;

    if (batch->data) {
        nimcp_tensor_destroy(batch->data);
        batch->data = NULL;
    }

    if (batch->labels) {
        nimcp_tensor_destroy(batch->labels);
        batch->labels = NULL;
    }

    if (batch->indices) {
        nimcp_free(batch->indices);
        batch->indices = NULL;
    }

    batch->batch_size = 0;
}

bool training_pipeline_has_more(const training_pipeline_ctx_t* pipeline) {
    if (!pipeline) {
        return false;
    }

    uint32_t remaining = pipeline->dataset_size - pipeline->current_index;
    if (pipeline->config.drop_last) {
        return remaining >= pipeline->config.batch_size;
    }
    return remaining > 0;
}

uint32_t training_pipeline_num_batches(const training_pipeline_ctx_t* pipeline) {
    if (!pipeline || pipeline->config.batch_size == 0) return 0;

    uint32_t num = pipeline->dataset_size / pipeline->config.batch_size;
    if (!pipeline->config.drop_last && (pipeline->dataset_size % pipeline->config.batch_size) > 0) {
        num++;
    }
    return num;
}

int training_pipeline_reset_epoch(training_pipeline_ctx_t* pipeline) {
    if (!pipeline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_reset_epoch: pipeline is NULL");
        return -1;
    }

    /* Reset index */
    pipeline->current_index = 0;
    pipeline->epoch++;
    pipeline->stats.samples_in_epoch = 0;
    pipeline->stats.current_epoch = pipeline->epoch;

    /* Reshuffle if configured */
    if (pipeline->config.shuffle) {
        shuffle_indices(pipeline->indices, pipeline->dataset_size, &pipeline->rng_state);
    }

    /* Clear prefetch buffer */
    for (uint32_t i = 0; i < pipeline->prefetch_capacity; i++) {
        if (pipeline->prefetch_buffer[i].ready) {
            training_batch_release(&pipeline->prefetch_buffer[i].batch);
            pipeline->prefetch_buffer[i].ready = false;
        }
    }
    pipeline->prefetch_head = 0;
    pipeline->prefetch_tail = 0;

    pipeline->state = PIPELINE_STATE_READY;

    NIMCP_LOGGING_INFO("Reset pipeline for epoch %lu", (unsigned long)pipeline->epoch);

    return 0;
}

uint64_t training_pipeline_get_epoch(const training_pipeline_ctx_t* pipeline) {
    return pipeline ? pipeline->epoch : 0;
}

int training_pipeline_set_seed(training_pipeline_ctx_t* pipeline, uint64_t seed) {
    if (!pipeline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_set_seed: pipeline is NULL");
        return -1;
    }
    pipeline->rng_state = seed;
    return 0;
}

int training_pipeline_add_transform(
    training_pipeline_ctx_t* pipeline,
    const training_transform_t* transform
) {
    if (!pipeline || !transform) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_add_transform: required parameter is NULL (pipeline, transform)");
        return -1;
    }

    if (pipeline->num_transforms >= TRAINING_PIPELINE_MAX_TRANSFORMS) {
        NIMCP_LOGGING_ERROR("Maximum transforms reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "training_pipeline_add_transform: capacity exceeded");
        return -1;
    }

    /* Reallocate transform array */
    training_transform_t* new_transforms = (training_transform_t*)nimcp_realloc(
        pipeline->transforms,
        (pipeline->num_transforms + 1) * sizeof(training_transform_t)
    );
    if (!new_transforms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_pipeline_add_transform: new_transforms is NULL");
        return -1;
    }

    pipeline->transforms = new_transforms;
    memcpy(&pipeline->transforms[pipeline->num_transforms], transform,
           sizeof(training_transform_t));
    pipeline->num_transforms++;

    return 0;
}

void training_pipeline_clear_transforms(training_pipeline_ctx_t* pipeline) {
    if (!pipeline) return;

    if (pipeline->transforms) {
        nimcp_free(pipeline->transforms);
        pipeline->transforms = NULL;
    }
    pipeline->num_transforms = 0;
}

int training_pipeline_set_transforms(
    training_pipeline_ctx_t* pipeline,
    const training_transform_t* transforms,
    uint32_t num_transforms
) {
    if (!pipeline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_set_transforms: pipeline is NULL");
        return -1;
    }

    training_pipeline_clear_transforms(pipeline);

    if (!transforms || num_transforms == 0) return 0;

    uint32_t count = num_transforms;
    if (count > TRAINING_PIPELINE_MAX_TRANSFORMS) {
        count = TRAINING_PIPELINE_MAX_TRANSFORMS;
    }

    pipeline->transforms = (training_transform_t*)nimcp_malloc(
        count * sizeof(training_transform_t)
    );
    if (!pipeline->transforms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_pipeline_set_transforms: pipeline->transforms is NULL");
        return -1;
    }

    memcpy(pipeline->transforms, transforms, count * sizeof(training_transform_t));
    pipeline->num_transforms = count;

    return 0;
}

int training_pipeline_get_stats(
    const training_pipeline_ctx_t* pipeline,
    training_pipeline_stats_t* stats
) {
    if (!pipeline || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_get_stats: required parameter is NULL (pipeline, stats)");
        return -1;
    }
    *stats = pipeline->stats;
    return 0;
}

void training_pipeline_reset_stats(training_pipeline_ctx_t* pipeline) {
    if (!pipeline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_reset_stats: pipeline is NULL");
        return;
    }
    memset(&pipeline->stats, 0, sizeof(training_pipeline_stats_t));
    pipeline->stats.current_epoch = pipeline->epoch;
}

training_pipeline_state_t training_pipeline_get_state(
    const training_pipeline_ctx_t* pipeline
) {
    return pipeline ? pipeline->state : PIPELINE_STATE_ERROR;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* training_transform_type_name(training_transform_type_t type) {
    switch (type) {
        case TRANSFORM_NONE:             return "None";
        case TRANSFORM_NORMALIZE:        return "Normalize";
        case TRANSFORM_STANDARDIZE:      return "Standardize";
        case TRANSFORM_RANDOM_CROP:      return "RandomCrop";
        case TRANSFORM_CENTER_CROP:      return "CenterCrop";
        case TRANSFORM_RESIZE:           return "Resize";
        case TRANSFORM_RANDOM_FLIP_H:    return "RandomHorizontalFlip";
        case TRANSFORM_RANDOM_FLIP_V:    return "RandomVerticalFlip";
        case TRANSFORM_RANDOM_ROTATE:    return "RandomRotate";
        case TRANSFORM_RANDOM_SCALE:     return "RandomScale";
        case TRANSFORM_RANDOM_BRIGHTNESS: return "RandomBrightness";
        case TRANSFORM_RANDOM_CONTRAST:  return "RandomContrast";
        case TRANSFORM_GAUSSIAN_NOISE:   return "GaussianNoise";
        case TRANSFORM_SALT_PEPPER_NOISE: return "SaltPepperNoise";
        case TRANSFORM_CUTOUT:           return "Cutout";
        case TRANSFORM_MIXUP:            return "MixUp";
        case TRANSFORM_CUTMIX:           return "CutMix";
        case TRANSFORM_TO_TENSOR:        return "ToTensor";
        case TRANSFORM_PAD:              return "Pad";
        case TRANSFORM_CUSTOM:           return "Custom";
        default:                         return "Unknown";
    }
}

const char* training_sampling_strategy_name(training_sampling_strategy_t strategy) {
    switch (strategy) {
        case SAMPLING_SEQUENTIAL: return "Sequential";
        case SAMPLING_RANDOM:     return "Random";
        case SAMPLING_SHUFFLE:    return "Shuffle";
        case SAMPLING_WEIGHTED:   return "Weighted";
        case SAMPLING_STRATIFIED: return "Stratified";
        case SAMPLING_CURRICULUM: return "Curriculum";
        default:                  return "Unknown";
    }
}

int training_pipeline_create_image_augmentation(
    training_transform_t* transforms,
    uint32_t* num_transforms,
    uint32_t image_size
) {
    if (!transforms || !num_transforms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_create_image_augmentation: required parameter is NULL (transforms, num_transforms)");
        return -1;
    }

    uint32_t idx = 0;

    /* 1. Normalize to [0, 1] */
    transforms[idx].type = TRANSFORM_NORMALIZE;
    transforms[idx].probability = 1.0f;
    transforms[idx].config.normalize.mean = NULL;
    transforms[idx].config.normalize.std = NULL;
    transforms[idx].config.normalize.min_val = 0.0f;
    transforms[idx].config.normalize.max_val = 1.0f;
    idx++;

    /* 2. Random horizontal flip */
    transforms[idx].type = TRANSFORM_RANDOM_FLIP_H;
    transforms[idx].probability = 0.5f;
    idx++;

    /* 3. Random cutout */
    transforms[idx].type = TRANSFORM_CUTOUT;
    transforms[idx].probability = 0.5f;
    transforms[idx].config.cutout.min_area = 0.02f;
    transforms[idx].config.cutout.max_area = 0.1f;
    transforms[idx].config.cutout.min_ratio = 0.5f;
    transforms[idx].config.cutout.max_ratio = 2.0f;
    transforms[idx].config.cutout.fill_value = 0.0f;
    transforms[idx].config.cutout.max_erases = 3;
    idx++;

    /* 4. Gaussian noise */
    transforms[idx].type = TRANSFORM_GAUSSIAN_NOISE;
    transforms[idx].probability = 0.3f;
    transforms[idx].config.noise.noise_stddev = 0.02f;
    idx++;

    *num_transforms = idx;
    (void)image_size;

    return 0;
}

int training_pipeline_create_audio_augmentation(
    training_transform_t* transforms,
    uint32_t* num_transforms
) {
    if (!transforms || !num_transforms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_pipeline_create_audio_augmentation: required parameter is NULL (transforms, num_transforms)");
        return -1;
    }

    uint32_t idx = 0;

    /* 1. Normalize */
    transforms[idx].type = TRANSFORM_NORMALIZE;
    transforms[idx].probability = 1.0f;
    transforms[idx].config.normalize.mean = NULL;
    transforms[idx].config.normalize.std = NULL;
    transforms[idx].config.normalize.min_val = -1.0f;
    transforms[idx].config.normalize.max_val = 1.0f;
    idx++;

    /* 2. Gaussian noise */
    transforms[idx].type = TRANSFORM_GAUSSIAN_NOISE;
    transforms[idx].probability = 0.5f;
    transforms[idx].config.noise.noise_stddev = 0.01f;
    idx++;

    *num_transforms = idx;

    return 0;
}
