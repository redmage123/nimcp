//=============================================================================
// nimcp_min_max_normalizer.h - Min-Max Normalization
//=============================================================================

#ifndef NIMCP_MIN_MAX_NORMALIZER_H
#define NIMCP_MIN_MAX_NORMALIZER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_min_max_normalizer.h
 * @brief Min-max scaling to [0, 1] or custom range
 *
 * WHAT: Scale values to specific range based on min/max
 * WHY:  Bounded outputs, useful for activation functions
 * HOW:  x_norm = (x - min) / (max - min)
 */

typedef struct min_max_normalizer min_max_normalizer_t;

typedef struct {
    float min_value;
    float max_value;
    float range;
    size_t sample_count;
} minmax_stats_t;

min_max_normalizer_t* minmax_normalizer_create(
    size_t num_channels,
    float target_min,
    float target_max,
    bool use_percentiles
);

void minmax_normalizer_destroy(min_max_normalizer_t* normalizer);

bool minmax_normalizer_fit(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
);

float minmax_normalizer_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float value
);

float minmax_normalizer_fit_transform(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
);

float minmax_normalizer_inverse_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float normalized_value
);

bool minmax_normalizer_get_stats(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    minmax_stats_t* stats
);

bool minmax_normalizer_reset_channel(
    min_max_normalizer_t* normalizer,
    size_t channel
);

void minmax_normalizer_reset_all(min_max_normalizer_t* normalizer);

size_t minmax_normalizer_num_channels(const min_max_normalizer_t* normalizer);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIN_MAX_NORMALIZER_H
