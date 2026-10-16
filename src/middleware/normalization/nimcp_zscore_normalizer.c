#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_zscore_normalizer.c - Z-Score Normalization Implementation
//=============================================================================
/**
 * @file nimcp_zscore_normalizer.c
 * @brief Z-Score normalization with tensor-accelerated batch operations
 *
 * WHAT: Statistical normalization (x - mean) / stddev
 * WHY:  Standardize features for neural network training
 * HOW:  Welford's online algorithm + tensor-accelerated batch ops
 *
 * @version 1.1.0 - Added tensor library integration
 */

#include "middleware/normalization/nimcp_zscore_normalizer.h"
#include "api/nimcp_api_exception.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "middleware/buffering/nimcp_circular_buffer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"



#define LOG_MODULE "nimcp_zscore_normalizer"
#define LOG_MODULE_ID 0x0523
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(zscore_normalizer)

typedef struct {
    float mean;
    float m2;  // Sum of squared differences (for Welford's algorithm)
    float variance;
    float stddev;
    size_t count;
    float min_value;
    float max_value;
    circular_buffer_t* window;  // For windowed statistics
} channel_stats_t;

struct zscore_normalizer {
    size_t num_channels;
    size_t window_size;
    float outlier_clip;
    channel_stats_t* channels;
    nimcp_mutex_t* mutex;
};

zscore_normalizer_t* zscore_normalizer_create(
    size_t num_channels,
    size_t window_size,
    float outlier_clip
) {
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "zscore_normalizer_create: num_channels is zero");
        return NULL;
    }

    zscore_normalizer_t* norm = nimcp_calloc(1, sizeof(zscore_normalizer_t));
    if (!norm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "norm is NULL");

        return NULL;

    }

    norm->num_channels = num_channels;
    norm->window_size = window_size;
    norm->outlier_clip = outlier_clip;

    norm->channels = nimcp_calloc(num_channels, sizeof(channel_stats_t));
    if (!norm->channels) {
        nimcp_free(norm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "zscore_normalizer_create: norm->channels is NULL");
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].mean = 0.0F;
        norm->channels[i].m2 = 0.0F;
        norm->channels[i].variance = 0.0F;
        norm->channels[i].stddev = 1.0F;
        norm->channels[i].count = 0;
        norm->channels[i].min_value = FLT_MAX;
        norm->channels[i].max_value = -FLT_MAX;

        if (window_size > 0) {
            norm->channels[i].window = circular_buffer_create(
                sizeof(float), window_size, OVERFLOW_OVERWRITE
            );
        } else {
            norm->channels[i].window = NULL;
        }
    }

    norm->mutex = nimcp_mutex_create(NULL);
    if (!norm->mutex) {
        for (size_t i = 0; i < num_channels; i++) {
            circular_buffer_destroy(norm->channels[i].window);
        }
        nimcp_free(norm->channels);
        nimcp_free(norm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "zscore_normalizer_create: failed to create mutex");
        return NULL;
    }

    return norm;
}

void zscore_normalizer_destroy(zscore_normalizer_t* normalizer) {
    if (!normalizer) return;

    if (normalizer->mutex) nimcp_mutex_free(normalizer->mutex);

    if (normalizer->channels) {
        for (size_t i = 0; i < normalizer->num_channels; i++) {
            circular_buffer_destroy(normalizer->channels[i].window);
        }
        nimcp_free(normalizer->channels);
    }

    nimcp_free(normalizer);
}

bool zscore_normalizer_fit(
    zscore_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "zscore_normalizer_fit: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    channel_stats_t* stats = &normalizer->channels[channel];

    // If windowed, manage window
    if (stats->window) {
        circular_buffer_push(stats->window, &value);

        // Recalculate from window
        size_t size = circular_buffer_size(stats->window);
        float sum = 0.0F;
        float sum_sq = 0.0F;
        stats->min_value = FLT_MAX;
        stats->max_value = -FLT_MAX;

        for (size_t i = 0; i < size; i++) {
            float v;
            circular_buffer_peek(stats->window, i, &v);
            sum += v;
            sum_sq += v * v;
            if (v < stats->min_value) stats->min_value = v;
            if (v > stats->max_value) stats->max_value = v;
        }

        stats->count = size;
        stats->mean = sum / size;
        stats->variance = (size > 1) ? (sum_sq / size - stats->mean * stats->mean) : 0.0F;
        stats->stddev = sqrtf(stats->variance > 0.0F ? stats->variance : 0.0001F);
    } else {
        // Welford's online algorithm
        stats->count++;
        float delta = value - stats->mean;
        stats->mean += delta / stats->count;
        float delta2 = value - stats->mean;
        stats->m2 += delta * delta2;

        if (stats->count > 1) {
            stats->variance = stats->m2 / (stats->count - 1);
            stats->stddev = sqrtf(stats->variance > 0.0F ? stats->variance : 0.0001F);
        }

        if (value < stats->min_value) stats->min_value = value;
        if (value > stats->max_value) stats->max_value = value;
    }

    nimcp_mutex_unlock(normalizer->mutex);

    return true;
}

float zscore_normalizer_transform(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) return 0.0F;

    float z = (value - stats->mean) / stats->stddev;

    if (normalizer->outlier_clip > 0.0F) {
        if (z > normalizer->outlier_clip) z = normalizer->outlier_clip;
        if (z < -normalizer->outlier_clip) z = -normalizer->outlier_clip;
    }

    return z;
}

float zscore_normalizer_fit_transform(
    zscore_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    zscore_normalizer_fit(normalizer, channel, value);
    return zscore_normalizer_transform(normalizer, channel, value);
}

float zscore_normalizer_inverse_transform(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    float zscore
) {
    if (!normalizer || channel >= normalizer->num_channels) return zscore;

    const channel_stats_t* stats = &normalizer->channels[channel];
    return zscore * stats->stddev + stats->mean;
}

size_t zscore_normalizer_fit_batch(
    zscore_normalizer_t* normalizer,
    size_t channel,
    const float* values,
    size_t count
) {
    if (!normalizer || !values || count == 0) return 0;

    for (size_t i = 0; i < count; i++) {
        zscore_normalizer_fit(normalizer, channel, values[i]);
    }

    return count;
}

/**
 * @brief Batch transform using tensor operations
 *
 * WHAT: Transform batch of values using z-score normalization
 * WHY:  Efficient vectorized normalization for large batches
 * HOW:  output = (values - mean) / stddev using tensor ops
 */
size_t zscore_normalizer_transform_batch(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    const float* values,
    float* outputs,
    size_t count
) {
    if (!normalizer || !values || !outputs || count == 0) return 0;
    if (channel >= normalizer->num_channels) return 0;

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) {
        /* No statistics yet, output zeros */
        memset(outputs, 0, count * sizeof(float));
        return count;
    }

    /* Try tensor-accelerated batch transform for large batches */
    if (count >= 16) {
        uint32_t dims[] = {(uint32_t)count};
        nimcp_tensor_t* t = nimcp_tensor_from_data(values, dims, 1, NIMCP_DTYPE_F32, true);
        if (t) {
            /* z = (x - mean) / stddev */
            nimcp_tensor_add_scalar_(t, -stats->mean);
            nimcp_tensor_mul_scalar_(t, 1.0 / (double)stats->stddev);

            /* Apply clipping if configured */
            if (normalizer->outlier_clip > 0.0F) {
                float* data = (float*)nimcp_tensor_data(t);
                for (size_t i = 0; i < count; i++) {
                    if (data[i] > normalizer->outlier_clip) {
                        data[i] = normalizer->outlier_clip;
                    } else if (data[i] < -normalizer->outlier_clip) {
                        data[i] = -normalizer->outlier_clip;
                    }
                }
            }

            /* Copy result to output */
            memcpy(outputs, nimcp_tensor_data(t), count * sizeof(float));
            nimcp_tensor_destroy(t);
            return count;
        }
    }

    /* Fallback to scalar computation for small batches or if tensor fails */
    for (size_t i = 0; i < count; i++) {
        outputs[i] = zscore_normalizer_transform(normalizer, channel, values[i]);
    }

    return count;
}

bool zscore_normalizer_get_stats(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    zscore_stats_t* stats
) {
    if (!normalizer || channel >= normalizer->num_channels || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "zscore_normalizer_get_stats: required parameter is NULL (normalizer, stats)");
        return false;
    }

    const channel_stats_t* ch = &normalizer->channels[channel];
    stats->mean = ch->mean;
    stats->variance = ch->variance;
    stats->stddev = ch->stddev;
    stats->sample_count = ch->count;
    stats->min_value = (ch->min_value == FLT_MAX) ? 0.0F : ch->min_value;
    stats->max_value = (ch->max_value == -FLT_MAX) ? 0.0F : ch->max_value;

    return true;
}

float zscore_normalizer_mean(const zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) return 0.0F;
    return normalizer->channels[channel].mean;
}

float zscore_normalizer_stddev(const zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) return 1.0F;
    return normalizer->channels[channel].stddev;
}

size_t zscore_normalizer_num_channels(const zscore_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}

bool zscore_normalizer_reset_channel(zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "zscore_normalizer_reset_channel: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    channel_stats_t* stats = &normalizer->channels[channel];
    stats->mean = 0.0F;
    stats->m2 = 0.0F;
    stats->variance = 0.0F;
    stats->stddev = 1.0F;
    stats->count = 0;
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;

    if (stats->window) {
        circular_buffer_clear(stats->window);
    }

    nimcp_mutex_unlock(normalizer->mutex);

    return true;
}

static void zscore_normalizer_reset_channel_unlocked(
    zscore_normalizer_t* normalizer,
    size_t channel
) {
    channel_stats_t* stats = &normalizer->channels[channel];
    stats->mean = 0.0F;
    stats->m2 = 0.0F;
    stats->variance = 0.0F;
    stats->stddev = 1.0F;
    stats->count = 0;
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;
    if (stats->window) {
        circular_buffer_clear(stats->window);
    }
}

void zscore_normalizer_reset_all(zscore_normalizer_t* normalizer) {
    if (!normalizer) return;

    nimcp_mutex_lock(normalizer->mutex);
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        zscore_normalizer_reset_channel_unlocked(normalizer, i);
    }
    nimcp_mutex_unlock(normalizer->mutex);
}

/* ============================================================================
 * Tensor-Based Operations
 * ============================================================================ */

/**
 * @brief Transform tensor in-place using z-score normalization
 *
 * WHAT: Normalize all elements of tensor using channel statistics
 * WHY:  Direct tensor API for neural network integration
 * HOW:  (tensor - mean) / stddev applied in-place
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index for statistics
 * @param tensor Tensor to normalize (modified in place)
 * @return true on success
 */
bool zscore_normalizer_transform_tensor(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    nimcp_tensor_t* tensor
) {
    if (!normalizer || !tensor || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "zscore_normalizer_transform_tensor: required parameter is NULL (normalizer, tensor)");
        return false;
    }

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "zscore_normalizer_transform_tensor: stats->count is zero");
        return false;
    }

    /* z = (x - mean) / stddev */
    nimcp_tensor_add_scalar_(tensor, -stats->mean);
    nimcp_tensor_mul_scalar_(tensor, 1.0 / (double)stats->stddev);

    /* Apply clipping if configured */
    if (normalizer->outlier_clip > 0.0F) {
        size_t numel = nimcp_tensor_numel(tensor);
        float* data = (float*)nimcp_tensor_data(tensor);
        for (size_t i = 0; i < numel; i++) {
            if (data[i] > normalizer->outlier_clip) {
                data[i] = normalizer->outlier_clip;
            } else if (data[i] < -normalizer->outlier_clip) {
                data[i] = -normalizer->outlier_clip;
            }
        }
    }

    return true;
}

/**
 * @brief Inverse transform tensor in-place
 *
 * WHAT: Denormalize tensor back to original scale
 * WHY:  Reconstruct original values from z-scores
 * HOW:  tensor * stddev + mean applied in-place
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index for statistics
 * @param tensor Tensor to denormalize (modified in place)
 * @return true on success
 */
bool zscore_normalizer_inverse_transform_tensor(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    nimcp_tensor_t* tensor
) {
    if (!normalizer || !tensor || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "zscore_normalizer_inverse_transform_tensor: required parameter is NULL (normalizer, tensor)");
        return false;
    }

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "zscore_normalizer_inverse_transform_tensor: stats->count is zero");
        return false;
    }

    /* x = z * stddev + mean */
    nimcp_tensor_mul_scalar_(tensor, (double)stats->stddev);
    nimcp_tensor_add_scalar_(tensor, stats->mean);

    return true;
}

/**
 * @brief Compute statistics from tensor data
 *
 * WHAT: Fit normalizer using tensor elements
 * WHY:  Efficient batch fitting from tensor data
 * HOW:  Uses tensor mean/var operations
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index to update
 * @param tensor Tensor containing values to fit
 * @return Number of elements processed
 */
size_t zscore_normalizer_fit_tensor(
    zscore_normalizer_t* normalizer,
    size_t channel,
    const nimcp_tensor_t* tensor
) {
    if (!normalizer || !tensor || channel >= normalizer->num_channels) {
        return 0;
    }

    size_t numel = nimcp_tensor_numel(tensor);
    if (numel == 0) {
        return 0;
    }

    nimcp_mutex_lock(normalizer->mutex);

    /* Compute mean and variance using tensor operations */
    nimcp_tensor_t* mean_t = nimcp_tensor_mean(tensor);
    nimcp_tensor_t* var_t = nimcp_tensor_var(tensor, true);  /* Sample variance */

    if (!mean_t || !var_t) {
        /* Fallback to scalar computation */
        const float* data = (const float*)nimcp_tensor_data_const(tensor);
        for (size_t i = 0; i < numel; i++) {
            zscore_normalizer_fit(normalizer, channel, data[i]);
        }
        if (mean_t) nimcp_tensor_destroy(mean_t);
        if (var_t) nimcp_tensor_destroy(var_t);
        return numel;
    }

    /* Update channel statistics directly */
    channel_stats_t* stats = &normalizer->channels[channel];
    double new_mean = nimcp_tensor_get_flat(mean_t, 0);
    double new_var = nimcp_tensor_get_flat(var_t, 0);

    /* Combine with existing statistics using weighted update */
    if (stats->count > 0) {
        size_t n1 = stats->count;
        size_t n2 = numel;
        size_t n = n1 + n2;
        double delta = new_mean - stats->mean;

        /* Combined mean */
        stats->mean = (float)((n1 * stats->mean + n2 * new_mean) / n);

        /* Combined variance (Parallel algorithm) */
        double m2_1 = stats->variance * (n1 - 1);
        double m2_2 = new_var * (n2 - 1);
        double m2 = m2_1 + m2_2 + delta * delta * n1 * n2 / n;
        stats->variance = (float)(m2 / (n - 1));
        stats->count = n;
    } else {
        stats->mean = (float)new_mean;
        stats->variance = (float)new_var;
        stats->count = numel;
    }

    stats->stddev = sqrtf(stats->variance > 0.0F ? stats->variance : 0.0001F);

    /* Update min/max */
    nimcp_tensor_t* min_t = nimcp_tensor_min(tensor);
    nimcp_tensor_t* max_t = nimcp_tensor_max(tensor);
    if (min_t) {
        float min_val = (float)nimcp_tensor_get_flat(min_t, 0);
        if (min_val < stats->min_value) stats->min_value = min_val;
        nimcp_tensor_destroy(min_t);
    }
    if (max_t) {
        float max_val = (float)nimcp_tensor_get_flat(max_t, 0);
        if (max_val > stats->max_value) stats->max_value = max_val;
        nimcp_tensor_destroy(max_t);
    }

    nimcp_tensor_destroy(mean_t);
    nimcp_tensor_destroy(var_t);

    nimcp_mutex_unlock(normalizer->mutex);

    return numel;
}
