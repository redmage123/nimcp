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
