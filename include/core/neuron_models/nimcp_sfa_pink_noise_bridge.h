//=============================================================================
// nimcp_sfa_pink_noise_bridge.h - Pink Noise for Spike Frequency Adaptation
//=============================================================================
/**
 * @file nimcp_sfa_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with spike frequency adaptation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies 1/f noise to SFA parameters (adaptation time constant, strength)
 * WHY:  SFA dynamics show 1/f fluctuations in vivo (Pozzorini et al., 2013)
 * BIOLOGICAL: Adaptation currents have noisy kinetics
 */

#ifndef NIMCP_SFA_PINK_NOISE_BRIDGE_H
#define NIMCP_SFA_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SFA_PINK_NOISE_DEFAULT_ALPHA     1.0f
#define SFA_PINK_NOISE_DEFAULT_AMPLITUDE 0.04f
#define SFA_DEFAULT_TAU                  100.0f
#define SFA_DEFAULT_STRENGTH             0.1f

typedef struct {
    float noise_alpha;
    float noise_amplitude;
    float tau_noise_scale;
    float strength_noise_scale;
    float threshold_noise_scale;
    float tau_min;
    float tau_max;
    float strength_min;
    float strength_max;
    bool enabled;
    uint32_t seed;
} sfa_pink_noise_config_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    sfa_pink_noise_config_t config;
    void* neuron_state;
    pink_noise_generator_t noise_gen;
    float noisy_tau;
    float noisy_strength;
    float noisy_threshold;
    float tau_noise;
    float strength_noise;
    float threshold_noise;
    uint64_t samples_generated;
    bool is_enabled;
    bool neuron_connected;
    bool noise_connected;
} sfa_pink_noise_bridge_t;

static inline sfa_pink_noise_config_t sfa_pink_noise_default_config(void) {
    return (sfa_pink_noise_config_t){
        .noise_alpha = SFA_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = SFA_PINK_NOISE_DEFAULT_AMPLITUDE,
        .tau_noise_scale = 1.0f,
        .strength_noise_scale = 0.7f,
        .threshold_noise_scale = 0.5f,
        .tau_min = 10.0f,
        .tau_max = 1000.0f,
        .strength_min = 0.01f,
        .strength_max = 0.5f,
        .enabled = true,
        .seed = 66666
    };
}

sfa_pink_noise_bridge_t* sfa_pink_noise_create(const sfa_pink_noise_config_t* config);
void sfa_pink_noise_destroy(sfa_pink_noise_bridge_t* bridge);
int sfa_pink_noise_connect(sfa_pink_noise_bridge_t* bridge, void* neuron_state);
int sfa_pink_noise_update(sfa_pink_noise_bridge_t* bridge);
float sfa_pink_noise_get_tau(const sfa_pink_noise_bridge_t* bridge);
float sfa_pink_noise_get_strength(const sfa_pink_noise_bridge_t* bridge);
int sfa_pink_noise_reset(sfa_pink_noise_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SFA_PINK_NOISE_BRIDGE_H
