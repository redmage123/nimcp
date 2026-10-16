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
