#!/bin/bash
#=============================================================================
# Generate Middleware Implementation Files
#=============================================================================

# Z-Score Normalizer Implementation
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_zscore_normalizer.c << 'EOF'
//=============================================================================
// nimcp_zscore_normalizer.c - Z-Score Normalization Implementation
//=============================================================================

#include "nimcp_zscore_normalizer.h"
#include "../buffering/nimcp_circular_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

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
};

zscore_normalizer_t* zscore_normalizer_create(
    size_t num_channels,
    size_t window_size,
    float outlier_clip
) {
    if (num_channels == 0) return NULL;

    zscore_normalizer_t* norm = calloc(1, sizeof(zscore_normalizer_t));
    if (!norm) return NULL;

    norm->num_channels = num_channels;
    norm->window_size = window_size;
    norm->outlier_clip = outlier_clip;

    norm->channels = calloc(num_channels, sizeof(channel_stats_t));
    if (!norm->channels) {
        free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].mean = 0.0f;
        norm->channels[i].m2 = 0.0f;
        norm->channels[i].variance = 0.0f;
        norm->channels[i].stddev = 1.0f;
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

    return norm;
}

void zscore_normalizer_destroy(zscore_normalizer_t* normalizer) {
    if (!normalizer) return;

    if (normalizer->channels) {
        for (size_t i = 0; i < normalizer->num_channels; i++) {
            circular_buffer_destroy(normalizer->channels[i].window);
        }
        free(normalizer->channels);
    }

    free(normalizer);
}

bool zscore_normalizer_fit(
    zscore_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    channel_stats_t* stats = &normalizer->channels[channel];

    // If windowed, manage window
    if (stats->window) {
        circular_buffer_push(stats->window, &value);

        // Recalculate from window
        size_t size = circular_buffer_size(stats->window);
        float sum = 0.0f;
        float sum_sq = 0.0f;
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
        stats->variance = (size > 1) ? (sum_sq / size - stats->mean * stats->mean) : 0.0f;
        stats->stddev = sqrtf(stats->variance > 0.0f ? stats->variance : 0.0001f);
    } else {
        // Welford's online algorithm
        stats->count++;
        float delta = value - stats->mean;
        stats->mean += delta / stats->count;
        float delta2 = value - stats->mean;
        stats->m2 += delta * delta2;

        if (stats->count > 1) {
            stats->variance = stats->m2 / (stats->count - 1);
            stats->stddev = sqrtf(stats->variance > 0.0f ? stats->variance : 0.0001f);
        }

        if (value < stats->min_value) stats->min_value = value;
        if (value > stats->max_value) stats->max_value = value;
    }

    return true;
}

float zscore_normalizer_transform(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) return 0.0f;

    float z = (value - stats->mean) / stats->stddev;

    if (normalizer->outlier_clip > 0.0f) {
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

size_t zscore_normalizer_transform_batch(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    const float* values,
    float* outputs,
    size_t count
) {
    if (!normalizer || !values || !outputs || count == 0) return 0;

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
    if (!normalizer || channel >= normalizer->num_channels || !stats) return false;

    const channel_stats_t* ch = &normalizer->channels[channel];
    stats->mean = ch->mean;
    stats->variance = ch->variance;
    stats->stddev = ch->stddev;
    stats->sample_count = ch->count;
    stats->min_value = (ch->min_value == FLT_MAX) ? 0.0f : ch->min_value;
    stats->max_value = (ch->max_value == -FLT_MAX) ? 0.0f : ch->max_value;

    return true;
}

float zscore_normalizer_mean(const zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) return 0.0f;
    return normalizer->channels[channel].mean;
}

float zscore_normalizer_stddev(const zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) return 1.0f;
    return normalizer->channels[channel].stddev;
}

size_t zscore_normalizer_num_channels(const zscore_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}

bool zscore_normalizer_reset_channel(zscore_normalizer_t* normalizer, size_t channel) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    channel_stats_t* stats = &normalizer->channels[channel];
    stats->mean = 0.0f;
    stats->m2 = 0.0f;
    stats->variance = 0.0f;
    stats->stddev = 1.0f;
    stats->count = 0;
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;

    if (stats->window) {
        circular_buffer_clear(stats->window);
    }

    return true;
}

void zscore_normalizer_reset_all(zscore_normalizer_t* normalizer) {
    if (!normalizer) return;

    for (size_t i = 0; i < normalizer->num_channels; i++) {
        zscore_normalizer_reset_channel(normalizer, i);
    }
}
EOF

echo "Z-Score normalizer implementation created"

chmod +x /home/bbrelin/nimcp/generate_middleware_impl.sh
