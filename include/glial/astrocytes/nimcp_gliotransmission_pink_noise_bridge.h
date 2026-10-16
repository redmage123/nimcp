//=============================================================================
// nimcp_gliotransmission_pink_noise_bridge.h - Pink Noise for Gliotransmission
//=============================================================================
/**
 * @file nimcp_gliotransmission_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with gliotransmitter release
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies 1/f noise to astrocyte gliotransmitter dynamics
 * WHY:  Gliotransmitter release (glutamate, ATP, D-serine) is stochastic
 * BIOLOGICAL: Astrocyte calcium waves follow 1/f dynamics
 */

#ifndef NIMCP_GLIOTRANSMISSION_PINK_NOISE_BRIDGE_H
#define NIMCP_GLIOTRANSMISSION_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLIO_PINK_NOISE_DEFAULT_ALPHA     1.0f
#define GLIO_PINK_NOISE_DEFAULT_AMPLITUDE 0.06f

typedef struct {
    float noise_alpha;
    float noise_amplitude;
    float glutamate_noise_scale;
    float atp_noise_scale;
    float dserine_noise_scale;
    float calcium_noise_scale;
    bool enabled;
    uint32_t seed;
} glio_pink_noise_config_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    glio_pink_noise_config_t config;
    void* astrocyte_state;
    pink_noise_generator_t noise_gen;
    float noisy_glutamate_release;
    float noisy_atp_release;
    float noisy_dserine_release;
    float noisy_calcium_wave;
    float glu_noise;
    float atp_noise;
    float dser_noise;
    float ca_noise;
    uint64_t samples_generated;
    bool is_enabled;
    bool astro_connected;
    bool noise_connected;
} glio_pink_noise_bridge_t;

static inline glio_pink_noise_config_t glio_pink_noise_default_config(void) {
    return (glio_pink_noise_config_t){
        .noise_alpha = GLIO_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = GLIO_PINK_NOISE_DEFAULT_AMPLITUDE,
        .glutamate_noise_scale = 1.0f,
        .atp_noise_scale = 0.8f,
        .dserine_noise_scale = 0.6f,
        .calcium_noise_scale = 1.2f,
        .enabled = true,
        .seed = 55555
    };
}

glio_pink_noise_bridge_t* glio_pink_noise_create(const glio_pink_noise_config_t* config);
void glio_pink_noise_destroy(glio_pink_noise_bridge_t* bridge);
int glio_pink_noise_connect(glio_pink_noise_bridge_t* bridge, void* astro_state);
int glio_pink_noise_update(glio_pink_noise_bridge_t* bridge);
float glio_pink_noise_get_glutamate(const glio_pink_noise_bridge_t* bridge);
float glio_pink_noise_get_calcium_wave(const glio_pink_noise_bridge_t* bridge);
int glio_pink_noise_reset(glio_pink_noise_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GLIOTRANSMISSION_PINK_NOISE_BRIDGE_H
