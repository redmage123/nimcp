//=============================================================================
// nimcp_structural_pink_noise_bridge.h - Pink Noise for Structural Plasticity
//=============================================================================
/**
 * @file nimcp_structural_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with structural plasticity
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies 1/f noise to spine growth/pruning dynamics
 * WHY:  Spine turnover exhibits 1/f noise characteristics
 * BIOLOGICAL: Holtmaat et al. 2005 - spine dynamics follow 1/f
 */

#ifndef NIMCP_STRUCTURAL_PINK_NOISE_BRIDGE_H
#define NIMCP_STRUCTURAL_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STRUCT_PINK_NOISE_DEFAULT_ALPHA     1.0f
#define STRUCT_PINK_NOISE_DEFAULT_AMPLITUDE 0.05f

typedef struct {
    float noise_alpha;
    float noise_amplitude;
    float growth_noise_scale;
    float prune_noise_scale;
    bool enabled;
    uint32_t seed;
} struct_pink_noise_config_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    struct_pink_noise_config_t config;
    void* structural_state;
    pink_noise_generator_t noise_gen;
    float noisy_growth_rate;
    float noisy_prune_rate;
    float growth_noise;
    float prune_noise;
    uint64_t samples_generated;
    bool is_enabled;
    bool struct_connected;
    bool noise_connected;
} struct_pink_noise_bridge_t;

static inline struct_pink_noise_config_t struct_pink_noise_default_config(void) {
    return (struct_pink_noise_config_t){
        .noise_alpha = STRUCT_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = STRUCT_PINK_NOISE_DEFAULT_AMPLITUDE,
        .growth_noise_scale = 1.0f,
        .prune_noise_scale = 0.8f,
        .enabled = true,
        .seed = 33333
    };
}

struct_pink_noise_bridge_t* struct_pink_noise_create(const struct_pink_noise_config_t* config);
void struct_pink_noise_destroy(struct_pink_noise_bridge_t* bridge);
int struct_pink_noise_connect(struct_pink_noise_bridge_t* bridge, void* state);
int struct_pink_noise_update(struct_pink_noise_bridge_t* bridge);
float struct_pink_noise_get_growth_rate(const struct_pink_noise_bridge_t* bridge);
float struct_pink_noise_get_prune_rate(const struct_pink_noise_bridge_t* bridge);
int struct_pink_noise_reset(struct_pink_noise_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_STRUCTURAL_PINK_NOISE_BRIDGE_H
