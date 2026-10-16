#!/bin/bash
set -e

echo "Creating all middleware normalizer implementations..."

# Min-Max Normalizer Implementation
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_min_max_normalizer.c << 'EOF'
#include "nimcp_min_max_normalizer.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

typedef struct {
    float min_value;
    float max_value;
    float range;
    size_t count;
} channel_stats_t;

struct min_max_normalizer {
    size_t num_channels;
    float target_min;
    float target_max;
    bool use_percentiles;
    channel_stats_t* channels;
};

min_max_normalizer_t* minmax_normalizer_create(
    size_t num_channels,
    float target_min,
    float target_max,
    bool use_percentiles
) {
    if (num_channels == 0 || target_min >= target_max) return NULL;

    min_max_normalizer_t* norm = calloc(1, sizeof(min_max_normalizer_t));
    if (!norm) return NULL;

    norm->num_channels = num_channels;
    norm->target_min = target_min;
    norm->target_max = target_max;
    norm->use_percentiles = use_percentiles;

    norm->channels = calloc(num_channels, sizeof(channel_stats_t));
    if (!norm->channels) {
        free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].min_value = FLT_MAX;
        norm->channels[i].max_value = -FLT_MAX;
        norm->channels[i].range = 1.0f;
        norm->channels[i].count = 0;
    }

    return norm;
}

void minmax_normalizer_destroy(min_max_normalizer_t* normalizer) {
    if (!normalizer) return;
    free(normalizer->channels);
    free(normalizer);
}

bool minmax_normalizer_fit(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    channel_stats_t* stats = &normalizer->channels[channel];

    if (value < stats->min_value) stats->min_value = value;
    if (value > stats->max_value) stats->max_value = value;

    stats->range = stats->max_value - stats->min_value;
    if (stats->range < 1e-6f) stats->range = 1.0f;

    stats->count++;
    return true;
}

float minmax_normalizer_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) return normalizer->target_min;

    float normalized = (value - stats->min_value) / stats->range;
    return normalized * (normalizer->target_max - normalizer->target_min) + normalizer->target_min;
}

float minmax_normalizer_fit_transform(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    minmax_normalizer_fit(normalizer, channel, value);
    return minmax_normalizer_transform(normalizer, channel, value);
}

float minmax_normalizer_inverse_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float normalized_value
) {
    if (!normalizer || channel >= normalizer->num_channels) return normalized_value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    float unit_value = (normalized_value - normalizer->target_min) /
                       (normalizer->target_max - normalizer->target_min);
    return unit_value * stats->range + stats->min_value;
}

bool minmax_normalizer_get_stats(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    minmax_stats_t* stats
) {
    if (!normalizer || channel >= normalizer->num_channels || !stats) return false;

    const channel_stats_t* ch = &normalizer->channels[channel];
    stats->min_value = (ch->min_value == FLT_MAX) ? 0.0f : ch->min_value;
    stats->max_value = (ch->max_value == -FLT_MAX) ? 1.0f : ch->max_value;
    stats->range = ch->range;
    stats->sample_count = ch->count;

    return true;
}

bool minmax_normalizer_reset_channel(
    min_max_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    channel_stats_t* stats = &normalizer->channels[channel];
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;
    stats->range = 1.0f;
    stats->count = 0;

    return true;
}

void minmax_normalizer_reset_all(min_max_normalizer_t* normalizer) {
    if (!normalizer) return;
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        minmax_normalizer_reset_channel(normalizer, i);
    }
}

size_t minmax_normalizer_num_channels(const min_max_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}
EOF

echo "Min-Max normalizer created"

# Adaptive Normalizer Header
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_adaptive_normalizer.h << 'EOF'
#ifndef NIMCP_ADAPTIVE_NORMALIZER_H
#define NIMCP_ADAPTIVE_NORMALIZER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct adaptive_normalizer adaptive_normalizer_t;

adaptive_normalizer_t* adaptive_normalizer_create(
    size_t num_channels,
    float initial_learning_rate,
    float adaptation_rate
);

void adaptive_normalizer_destroy(adaptive_normalizer_t* normalizer);

bool adaptive_normalizer_fit(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
);

float adaptive_normalizer_transform(
    const adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
);

float adaptive_normalizer_fit_transform(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
);

bool adaptive_normalizer_reset_channel(
    adaptive_normalizer_t* normalizer,
    size_t channel
);

void adaptive_normalizer_reset_all(adaptive_normalizer_t* normalizer);

size_t adaptive_normalizer_num_channels(const adaptive_normalizer_t* normalizer);

#ifdef __cplusplus
}
#endif

#endif
EOF

# Adaptive Normalizer Implementation
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_adaptive_normalizer.c << 'EOF'
#include "nimcp_adaptive_normalizer.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float mean;
    float variance;
    float learning_rate;
    size_t count;
} adaptive_channel_t;

struct adaptive_normalizer {
    size_t num_channels;
    float initial_learning_rate;
    float adaptation_rate;
    adaptive_channel_t* channels;
};

adaptive_normalizer_t* adaptive_normalizer_create(
    size_t num_channels,
    float initial_learning_rate,
    float adaptation_rate
) {
    if (num_channels == 0) return NULL;

    adaptive_normalizer_t* norm = calloc(1, sizeof(adaptive_normalizer_t));
    if (!norm) return NULL;

    norm->num_channels = num_channels;
    norm->initial_learning_rate = initial_learning_rate;
    norm->adaptation_rate = adaptation_rate;

    norm->channels = calloc(num_channels, sizeof(adaptive_channel_t));
    if (!norm->channels) {
        free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].mean = 0.0f;
        norm->channels[i].variance = 1.0f;
        norm->channels[i].learning_rate = initial_learning_rate;
        norm->channels[i].count = 0;
    }

    return norm;
}

void adaptive_normalizer_destroy(adaptive_normalizer_t* normalizer) {
    if (!normalizer) return;
    free(normalizer->channels);
    free(normalizer);
}

bool adaptive_normalizer_fit(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    adaptive_channel_t* ch = &normalizer->channels[channel];

    float lr = ch->learning_rate;
    ch->mean = (1.0f - lr) * ch->mean + lr * value;

    float diff = value - ch->mean;
    ch->variance = (1.0f - lr) * ch->variance + lr * diff * diff;

    // Adapt learning rate based on variance change
    ch->learning_rate *= (1.0f + normalizer->adaptation_rate * fabsf(diff));
    if (ch->learning_rate > 0.5f) ch->learning_rate = 0.5f;
    if (ch->learning_rate < 0.001f) ch->learning_rate = 0.001f;

    ch->count++;
    return true;
}

float adaptive_normalizer_transform(
    const adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    const adaptive_channel_t* ch = &normalizer->channels[channel];
    if (ch->count == 0 || ch->variance < 0.0001f) return 0.0f;

    return (value - ch->mean) / sqrtf(ch->variance);
}

float adaptive_normalizer_fit_transform(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    adaptive_normalizer_fit(normalizer, channel, value);
    return adaptive_normalizer_transform(normalizer, channel, value);
}

bool adaptive_normalizer_reset_channel(
    adaptive_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    adaptive_channel_t* ch = &normalizer->channels[channel];
    ch->mean = 0.0f;
    ch->variance = 1.0f;
    ch->learning_rate = normalizer->initial_learning_rate;
    ch->count = 0;

    return true;
}

void adaptive_normalizer_reset_all(adaptive_normalizer_t* normalizer) {
    if (!normalizer) return;
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        adaptive_normalizer_reset_channel(normalizer, i);
    }
}

size_t adaptive_normalizer_num_channels(const adaptive_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}
EOF

# Homeostatic Normalizer Header
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_homeostatic_normalizer.h << 'EOF'
#ifndef NIMCP_HOMEOSTATIC_NORMALIZER_H
#define NIMCP_HOMEOSTATIC_NORMALIZER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct homeostatic_normalizer homeostatic_normalizer_t;

homeostatic_normalizer_t* homeostatic_normalizer_create(
    size_t num_channels,
    float target_activity,
    float time_constant
);

void homeostatic_normalizer_destroy(homeostatic_normalizer_t* normalizer);

bool homeostatic_normalizer_update(
    homeostatic_normalizer_t* normalizer,
    size_t channel,
    float activity,
    float dt
);

float homeostatic_normalizer_get_scaling(
    const homeostatic_normalizer_t* normalizer,
    size_t channel
);

float homeostatic_normalizer_apply(
    const homeostatic_normalizer_t* normalizer,
    size_t channel,
    float value
);

bool homeostatic_normalizer_reset_channel(
    homeostatic_normalizer_t* normalizer,
    size_t channel
);

void homeostatic_normalizer_reset_all(homeostatic_normalizer_t* normalizer);

size_t homeostatic_normalizer_num_channels(const homeostatic_normalizer_t* normalizer);

#ifdef __cplusplus
}
#endif

#endif
EOF

# Homeostatic Normalizer Implementation
cat > /home/bbrelin/nimcp/src/middleware/normalization/nimcp_homeostatic_normalizer.c << 'EOF'
#include "nimcp_homeostatic_normalizer.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    float current_activity;
    float scaling_factor;
    float accumulated_error;
} homeostatic_channel_t;

struct homeostatic_normalizer {
    size_t num_channels;
    float target_activity;
    float time_constant;
    homeostatic_channel_t* channels;
};

homeostatic_normalizer_t* homeostatic_normalizer_create(
    size_t num_channels,
    float target_activity,
    float time_constant
) {
    if (num_channels == 0 || time_constant <= 0.0f) return NULL;

    homeostatic_normalizer_t* norm = calloc(1, sizeof(homeostatic_normalizer_t));
    if (!norm) return NULL;

    norm->num_channels = num_channels;
    norm->target_activity = target_activity;
    norm->time_constant = time_constant;

    norm->channels = calloc(num_channels, sizeof(homeostatic_channel_t));
    if (!norm->channels) {
        free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].current_activity = 0.0f;
        norm->channels[i].scaling_factor = 1.0f;
        norm->channels[i].accumulated_error = 0.0f;
    }

    return norm;
}

void homeostatic_normalizer_destroy(homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return;
    free(normalizer->channels);
    free(normalizer);
}

bool homeostatic_normalizer_update(
    homeostatic_normalizer_t* normalizer,
    size_t channel,
    float activity,
    float dt
) {
    if (!normalizer || channel >= normalizer->num_channels || dt <= 0.0f) return false;

    homeostatic_channel_t* ch = &normalizer->channels[channel];

    // Update activity with exponential smoothing
    float alpha = dt / normalizer->time_constant;
    ch->current_activity = (1.0f - alpha) * ch->current_activity + alpha * activity;

    // Calculate error
    float error = normalizer->target_activity - ch->current_activity;
    ch->accumulated_error += error * dt;

    // Update scaling factor (homeostatic plasticity)
    ch->scaling_factor = 1.0f + 0.1f * ch->accumulated_error;

    // Clamp scaling factor
    if (ch->scaling_factor < 0.1f) ch->scaling_factor = 0.1f;
    if (ch->scaling_factor > 10.0f) ch->scaling_factor = 10.0f;

    return true;
}

float homeostatic_normalizer_get_scaling(
    const homeostatic_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) return 1.0f;
    return normalizer->channels[channel].scaling_factor;
}

float homeostatic_normalizer_apply(
    const homeostatic_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;
    return value * normalizer->channels[channel].scaling_factor;
}

bool homeostatic_normalizer_reset_channel(
    homeostatic_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) return false;

    homeostatic_channel_t* ch = &normalizer->channels[channel];
    ch->current_activity = 0.0f;
    ch->scaling_factor = 1.0f;
    ch->accumulated_error = 0.0f;

    return true;
}

void homeostatic_normalizer_reset_all(homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return;
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        homeostatic_normalizer_reset_channel(normalizer, i);
    }
}

size_t homeostatic_normalizer_num_channels(const homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}
EOF

echo "All normalizer implementations created successfully!"

