//=============================================================================
// nimcp_metaplasticity_pink_noise_bridge.h - Pink Noise for Metaplasticity
//=============================================================================
/**
 * @file nimcp_metaplasticity_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with metaplasticity thresholds
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies biologically-realistic 1/f noise to metaplasticity parameters
 * WHY:  Metaplastic thresholds fluctuate with pink noise:
 *       - Threshold drift follows 1/f spectrum
 *       - Multi-timescale adaptation requires noise
 *       - Enables exploration of plasticity landscape
 *
 * BIOLOGICAL BASIS:
 * - Threshold fluctuations observed in vivo (Mockett & Bhalla, 2002)
 * - BCM threshold drifts with 1/f characteristics
 * - Neuromodulator effects are inherently noisy
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_METAPLASTICITY_PINK_NOISE_BRIDGE_H
#define NIMCP_METAPLASTICITY_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define META_PINK_NOISE_DEFAULT_ALPHA       1.0f
#define META_PINK_NOISE_DEFAULT_AMPLITUDE   0.04f
#define META_PINK_NOISE_DEFAULT_THETA       1.0f

//=============================================================================
// Configuration
//=============================================================================

typedef enum {
    META_NOISE_TARGET_THETA_BASE    = 0x01,
    META_NOISE_TARGET_THETA_EFF     = 0x02,
    META_NOISE_TARGET_TAU_BASE      = 0x04,
    META_NOISE_TARGET_TAU_HIST      = 0x08,
    META_NOISE_TARGET_ALL           = 0x0F
} meta_noise_target_t;

typedef struct {
    float noise_alpha;
    float noise_amplitude;
    float noise_sample_rate;
    meta_noise_target_t noise_targets;
    float theta_base_noise_scale;
    float theta_eff_noise_scale;
    float tau_noise_scale;
    float theta_min;
    float theta_max;
    float tau_min;
    float tau_max;
    bool enabled;
    uint32_t seed;
} meta_pink_noise_config_t;

//=============================================================================
// Bridge State
//=============================================================================

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    meta_pink_noise_config_t config;
    void* metaplasticity_state;
    pink_noise_generator_t noise_gen;

    float noisy_theta_baseline;
    float noisy_theta_effective;
    float noisy_baseline_tau;
    float noisy_history_tau;

    float theta_base_noise;
    float theta_eff_noise;
    float tau_noise;

    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;

    bool is_enabled;
    bool meta_connected;
    bool noise_connected;
} meta_pink_noise_bridge_t;

typedef struct {
    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;
    float current_noisy_theta_base;
    float current_noisy_theta_eff;
} meta_pink_noise_stats_t;

//=============================================================================
// API Functions
//=============================================================================

static inline meta_pink_noise_config_t meta_pink_noise_default_config(void) {
    return (meta_pink_noise_config_t){
        .noise_alpha = META_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = META_PINK_NOISE_DEFAULT_AMPLITUDE,
        .noise_sample_rate = 1000.0f,
        .noise_targets = META_NOISE_TARGET_ALL,
        .theta_base_noise_scale = 1.0f,
        .theta_eff_noise_scale = 0.8f,
        .tau_noise_scale = 0.3f,
        .theta_min = 0.01f,
        .theta_max = 10.0f,
        .tau_min = 60000.0f,
        .tau_max = 86400000.0f,
        .enabled = true,
        .seed = 11111
    };
}

meta_pink_noise_bridge_t* meta_pink_noise_create(const meta_pink_noise_config_t* config);
void meta_pink_noise_destroy(meta_pink_noise_bridge_t* bridge);
int meta_pink_noise_connect_meta(meta_pink_noise_bridge_t* bridge, void* meta_state);
int meta_pink_noise_disconnect(meta_pink_noise_bridge_t* bridge);
int meta_pink_noise_update(meta_pink_noise_bridge_t* bridge);
float meta_pink_noise_get_noisy_theta_baseline(const meta_pink_noise_bridge_t* bridge);
float meta_pink_noise_get_noisy_theta_effective(const meta_pink_noise_bridge_t* bridge);
int meta_pink_noise_get_stats(const meta_pink_noise_bridge_t* bridge, meta_pink_noise_stats_t* stats);
int meta_pink_noise_set_enabled(meta_pink_noise_bridge_t* bridge, bool enabled);
int meta_pink_noise_reset(meta_pink_noise_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_METAPLASTICITY_PINK_NOISE_BRIDGE_H
