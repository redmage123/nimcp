#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_integration_buffer.c - Multi-Timescale Integration Implementation
//=============================================================================

#include "middleware/buffering/nimcp_integration_buffer.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_integration_buffer"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(integration_buffer)

#include <string.h>
#include <math.h>

/**
 * @brief Per-channel timescale buffer
 *
 * WHAT: Separate buffers for each timescale level
 * WHY:  Independent temporal representations
 * HOW:  Circular buffers for each level
 */
typedef struct {
    circular_buffer_t* buffers[TIMESCALE_COUNT];  /**< One buffer per timescale */
    sliding_window_t* windows[TIMESCALE_COUNT];   /**< Statistics windows */
    uint64_t last_timestamp[TIMESCALE_COUNT];     /**< Last update timestamp */
    size_t downsample_counters[TIMESCALE_COUNT];  /**< Downsampling counters */
} channel_buffers_t;

/**
 * @brief Multi-timescale integration buffer
 *
 * WHAT: Hierarchical temporal buffer system
 * WHY:  Multi-scale neural processing
 * HOW:  Pyramid of buffers with different integration rates
 */
struct integration_buffer {
    size_t capacities[TIMESCALE_COUNT];  /**< Capacity per level */
    size_t num_channels;                  /**< Number of channels */
    size_t downsample_ratios[TIMESCALE_COUNT]; /**< Downsampling ratios */
    channel_buffers_t* channels;          /**< Per-channel buffers */
};

//=============================================================================
// LIFECYCLE
//=============================================================================

integration_buffer_t* integration_buffer_create(
    size_t fast_size,
    size_t medium_size,
    size_t slow_size,
    size_t num_channels
) {
    // Guard: validate inputs
    if (fast_size == 0 || medium_size == 0 || slow_size == 0 || num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "integration_buffer_create: invalid size or channel count");
        return NULL;
    }

    // Allocate structure
    integration_buffer_t* buf = nimcp_calloc(1, sizeof(integration_buffer_t));
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "integration_buffer_create: failed to allocate buffer");
        return NULL;
    }

    // Configure capacities
    buf->capacities[TIMESCALE_FAST] = fast_size;
    buf->capacities[TIMESCALE_MEDIUM] = medium_size;
    buf->capacities[TIMESCALE_SLOW] = slow_size;
    buf->num_channels = num_channels;

    // Calculate downsample ratios (medium is 10x fast, slow is 10x medium)
    buf->downsample_ratios[TIMESCALE_FAST] = 1;
    buf->downsample_ratios[TIMESCALE_MEDIUM] = 10;
    buf->downsample_ratios[TIMESCALE_SLOW] = 100;

    // Allocate channel buffers
    buf->channels = nimcp_calloc(num_channels, sizeof(channel_buffers_t));
    if (!buf->channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "integration_buffer_create: failed to allocate channels");
        nimcp_free(buf);
        return NULL;
    }

    // Initialize each channel
    for (size_t ch = 0; ch < num_channels; ch++) {
        channel_buffers_t* cbuf = &buf->channels[ch];

        // Create buffers for each timescale
        cbuf->buffers[TIMESCALE_FAST] = circular_buffer_create(
            sizeof(timestamped_sample_t), fast_size, OVERFLOW_OVERWRITE
        );
        cbuf->buffers[TIMESCALE_MEDIUM] = circular_buffer_create(
            sizeof(timestamped_sample_t), medium_size, OVERFLOW_OVERWRITE
        );
        cbuf->buffers[TIMESCALE_SLOW] = circular_buffer_create(
            sizeof(timestamped_sample_t), slow_size, OVERFLOW_OVERWRITE
        );

        // Create sliding windows for statistics
        cbuf->windows[TIMESCALE_FAST] = sliding_window_create(fast_size, 0);
        cbuf->windows[TIMESCALE_MEDIUM] = sliding_window_create(medium_size, 0);
        cbuf->windows[TIMESCALE_SLOW] = sliding_window_create(slow_size, 0);

        // Check allocation
        bool alloc_failed = false;
        for (int i = 0; i < TIMESCALE_COUNT; i++) {
            if (!cbuf->buffers[i] || !cbuf->windows[i]) {
                alloc_failed = true;
                break;
            }
        }

        if (alloc_failed) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "integration_buffer_create: failed to allocate channel buffers");
            // Cleanup on failure
            for (size_t j = 0; j <= ch; j++) {
                for (int i = 0; i < TIMESCALE_COUNT; i++) {
                    circular_buffer_destroy(buf->channels[j].buffers[i]);
                    sliding_window_destroy(buf->channels[j].windows[i]);
                }
            }
            nimcp_free(buf->channels);
            nimcp_free(buf);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integration_buffer_create: operation failed");
            return NULL;
        }

        // Initialize counters
        memset(cbuf->downsample_counters, 0, sizeof(cbuf->downsample_counters));
        memset(cbuf->last_timestamp, 0, sizeof(cbuf->last_timestamp));
    }

    return buf;
}

void integration_buffer_destroy(integration_buffer_t* buffer) {
    if (!buffer) return;

    // Free channel buffers
    if (buffer->channels) {
        for (size_t ch = 0; ch < buffer->num_channels; ch++) {
            for (int i = 0; i < TIMESCALE_COUNT; i++) {
                circular_buffer_destroy(buffer->channels[ch].buffers[i]);
                sliding_window_destroy(buffer->channels[ch].windows[i]);
            }
        }
        nimcp_free(buffer->channels);
    }

    nimcp_free(buffer);
}

//=============================================================================
// DATA OPERATIONS
//=============================================================================

bool integration_buffer_add(
    integration_buffer_t* buffer,
    size_t channel,
    float value,
    uint64_t timestamp
) {
    // Guard: validate inputs
    if (!buffer || channel >= buffer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "integration_buffer_add: buffer is NULL");
        return false;
    }

    channel_buffers_t* cbuf = &buffer->channels[channel];

    // Create timestamped sample
    timestamped_sample_t sample = { .value = value, .timestamp = timestamp };

    // Always add to fast buffer
    circular_buffer_push(cbuf->buffers[TIMESCALE_FAST], &sample);
    sliding_window_add(cbuf->windows[TIMESCALE_FAST], value);
    cbuf->last_timestamp[TIMESCALE_FAST] = timestamp;
    cbuf->downsample_counters[TIMESCALE_FAST]++;

    // Downsample to medium buffer (every 10 samples)
    if (cbuf->downsample_counters[TIMESCALE_FAST] % buffer->downsample_ratios[TIMESCALE_MEDIUM] == 0) {
        circular_buffer_push(cbuf->buffers[TIMESCALE_MEDIUM], &sample);
        sliding_window_add(cbuf->windows[TIMESCALE_MEDIUM], value);
        cbuf->last_timestamp[TIMESCALE_MEDIUM] = timestamp;
        cbuf->downsample_counters[TIMESCALE_MEDIUM]++;
    }

    // Downsample to slow buffer (every 100 samples)
    if (cbuf->downsample_counters[TIMESCALE_FAST] % buffer->downsample_ratios[TIMESCALE_SLOW] == 0) {
        circular_buffer_push(cbuf->buffers[TIMESCALE_SLOW], &sample);
        sliding_window_add(cbuf->windows[TIMESCALE_SLOW], value);
        cbuf->last_timestamp[TIMESCALE_SLOW] = timestamp;
        cbuf->downsample_counters[TIMESCALE_SLOW]++;
    }

    return true;
}

float integration_buffer_get_latest(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || channel >= buffer->num_channels) {
        return 0.0F;
    }

    channel_buffers_t* cbuf = &buffer->channels[channel];

    // Get most recent sample
    timestamped_sample_t sample;
    if (circular_buffer_peek(cbuf->buffers[level],
                            circular_buffer_size(cbuf->buffers[level]) - 1,
                            &sample)) {
        return sample.value;
    }

    return 0.0F;
}

size_t integration_buffer_get_time_range(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel,
    uint64_t start_time,
    uint64_t end_time,
    timestamped_sample_t* samples,
    size_t max_samples
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || channel >= buffer->num_channels ||
        !samples || max_samples == 0 || start_time > end_time) {
        return 0;
    }

    channel_buffers_t* cbuf = &buffer->channels[channel];
    size_t buf_size = circular_buffer_size(cbuf->buffers[level]);
    size_t found = 0;

    // Scan buffer for samples in time range
    for (size_t i = 0; i < buf_size && found < max_samples; i++) {
        timestamped_sample_t sample;
        if (circular_buffer_peek(cbuf->buffers[level], i, &sample)) {
            if (sample.timestamp >= start_time && sample.timestamp <= end_time) {
                samples[found++] = sample;
            }
        }
    }

    return found;
}

size_t integration_buffer_get_all_samples(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel,
    timestamped_sample_t* samples,
    size_t max_samples
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || channel >= buffer->num_channels ||
        !samples || max_samples == 0) {
        return 0;
    }

    channel_buffers_t* cbuf = &buffer->channels[channel];
    size_t buf_size = circular_buffer_size(cbuf->buffers[level]);
    size_t to_copy = (buf_size < max_samples) ? buf_size : max_samples;

    // Copy all samples
    for (size_t i = 0; i < to_copy; i++) {
        if (!circular_buffer_peek(cbuf->buffers[level], i, &samples[i])) {
            return i;
        }
    }

    return to_copy;
}

//=============================================================================
// STATISTICS
//=============================================================================

float integration_buffer_mean(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || channel >= buffer->num_channels) {
        return 0.0F;
    }

    return sliding_window_mean(buffer->channels[channel].windows[level]);
}

float integration_buffer_variance(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || channel >= buffer->num_channels) {
        return 0.0F;
    }

    return sliding_window_variance(buffer->channels[channel].windows[level]);
}

float integration_buffer_trend(
    const integration_buffer_t* buffer,
    size_t channel
) {
    // Guard: validate inputs
    if (!buffer || channel >= buffer->num_channels) return 0.0F;

    // Trend = slow mean - fast mean (positive = increasing over time)
    float slow_mean = integration_buffer_mean(buffer, TIMESCALE_SLOW, channel);
    float fast_mean = integration_buffer_mean(buffer, TIMESCALE_FAST, channel);

    return slow_mean - fast_mean;
}

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

size_t integration_buffer_capacity(
    const integration_buffer_t* buffer,
    timescale_level_t level
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT) return 0;

    return buffer->capacities[level];
}

size_t integration_buffer_count(
    const integration_buffer_t* buffer,
    timescale_level_t level
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT || buffer->num_channels == 0) {
        return 0;
    }

    return circular_buffer_size(buffer->channels[0].buffers[level]);
}

size_t integration_buffer_num_channels(const integration_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return 0;

    return buffer->num_channels;
}

//=============================================================================
// MANAGEMENT
//=============================================================================

void integration_buffer_clear(integration_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return;

    for (size_t ch = 0; ch < buffer->num_channels; ch++) {
        for (int level = 0; level < TIMESCALE_COUNT; level++) {
            circular_buffer_clear(buffer->channels[ch].buffers[level]);
            sliding_window_clear(buffer->channels[ch].windows[level]);
        }
        memset(buffer->channels[ch].downsample_counters, 0,
               sizeof(buffer->channels[ch].downsample_counters));
        memset(buffer->channels[ch].last_timestamp, 0,
               sizeof(buffer->channels[ch].last_timestamp));
    }
}

void integration_buffer_clear_level(
    integration_buffer_t* buffer,
    timescale_level_t level
) {
    // Guard: validate inputs
    if (!buffer || level >= TIMESCALE_COUNT) return;

    for (size_t ch = 0; ch < buffer->num_channels; ch++) {
        circular_buffer_clear(buffer->channels[ch].buffers[level]);
        sliding_window_clear(buffer->channels[ch].windows[level]);
        buffer->channels[ch].downsample_counters[level] = 0;
        buffer->channels[ch].last_timestamp[level] = 0;
    }
}
