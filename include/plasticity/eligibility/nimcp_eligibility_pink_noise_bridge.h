//=============================================================================
// nimcp_eligibility_pink_noise_bridge.h - Pink Noise for Eligibility Traces
//=============================================================================
/**
 * @file nimcp_eligibility_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with eligibility trace dynamics
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies biologically-realistic 1/f noise to eligibility traces
 * WHY:  Eligibility traces in biology have noisy decay characteristics:
 *       - Trace decay varies with 1/f spectrum
 *       - Consolidation thresholds fluctuate
 *       - Multi-timescale noise enables robust credit assignment
 *
 * BIOLOGICAL BASIS:
 * - Eligibility trace noise follows 1/f (Gerstner et al., 2018)
 * - Dopamine modulates traces with inherent noise
 * - Stochastic consolidation improves generalization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ELIGIBILITY_PINK_NOISE_BRIDGE_H
#define NIMCP_ELIGIBILITY_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ELIG_PINK_NOISE_DEFAULT_ALPHA       1.0f
#define ELIG_PINK_NOISE_DEFAULT_AMPLITUDE   0.04f

typedef enum {
    ELIG_NOISE_TARGET_DECAY     = 0x01,
    ELIG_NOISE_TARGET_THRESHOLD = 0x02,
    ELIG_NOISE_TARGET_BOOST     = 0x04,
    ELIG_NOISE_TARGET_ALL       = 0x07
} elig_noise_target_t;

typedef struct {
    float noise_alpha;
    float noise_amplitude;
    elig_noise_target_t noise_targets;
    float decay_noise_scale;
    float threshold_noise_scale;
    float boost_noise_scale;
    float decay_min;
    float decay_max;
    bool enabled;
    uint32_t seed;
} elig_pink_noise_config_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    elig_pink_noise_config_t config;
    void* eligibility_state;
    pink_noise_generator_t noise_gen;
    float noisy_decay_rate;
    float noisy_threshold;
    float noisy_boost;
    float decay_noise;
    float threshold_noise;
    float boost_noise;
    uint64_t samples_generated;
    float avg_noise_amplitude;
    bool is_enabled;
    bool elig_connected;
    bool noise_connected;
} elig_pink_noise_bridge_t;

static inline elig_pink_noise_config_t elig_pink_noise_default_config(void) {
    return (elig_pink_noise_config_t){
        .noise_alpha = ELIG_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = ELIG_PINK_NOISE_DEFAULT_AMPLITUDE,
        .noise_targets = ELIG_NOISE_TARGET_ALL,
        .decay_noise_scale = 1.0f,
        .threshold_noise_scale = 0.5f,
        .boost_noise_scale = 0.3f,
        .decay_min = 0.001f,
        .decay_max = 0.5f,
        .enabled = true,
        .seed = 22222
    };
}

elig_pink_noise_bridge_t* elig_pink_noise_create(const elig_pink_noise_config_t* config);
void elig_pink_noise_destroy(elig_pink_noise_bridge_t* bridge);
int elig_pink_noise_connect(elig_pink_noise_bridge_t* bridge, void* elig_state);
int elig_pink_noise_update(elig_pink_noise_bridge_t* bridge);
float elig_pink_noise_get_noisy_decay(const elig_pink_noise_bridge_t* bridge);
int elig_pink_noise_reset(elig_pink_noise_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ELIGIBILITY_PINK_NOISE_BRIDGE_H
