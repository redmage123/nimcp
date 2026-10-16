//=============================================================================
// nimcp_homeostatic_pink_noise_bridge.h - Pink Noise Bridge for Homeostatic Plasticity
//=============================================================================
/**
 * @file nimcp_homeostatic_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with homeostatic plasticity mechanisms
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies biologically-realistic 1/f noise to homeostatic parameters
 * WHY:  Homeostatic set points fluctuate with pink noise:
 *       - Target rates vary over time with 1/f spectrum
 *       - Scaling time constants are not fixed
 *       - Enables adaptive robustness to perturbations
 *
 * HOW:  Generate pink noise samples and modulate:
 *       - Target firing rate (with bounds)
 *       - Scaling exponent
 *       - Time constants for averaging
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Set point fluctuations follow 1/f (Bhalla & Bhalla, 2014)
 * - Multi-timescale adaptation requires noise
 * - Homeostatic mechanisms operate over hours-days with noise
 * - Pink noise enables escape from pathological attractors
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HOMEOSTATIC_PINK_NOISE_BRIDGE_H
#define NIMCP_HOMEOSTATIC_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HOMEO_PINK_NOISE_DEFAULT_ALPHA       1.0f    /**< True pink noise */
#define HOMEO_PINK_NOISE_DEFAULT_AMPLITUDE   0.03f   /**< 3% modulation */
#define HOMEO_PINK_NOISE_DEFAULT_TARGET      5.0f    /**< 5 Hz target rate */

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Which homeostatic parameters receive noise modulation
 */
typedef enum {
    HOMEO_NOISE_TARGET_RATE      = 0x01, /**< Target firing rate */
    HOMEO_NOISE_SCALING_TAU      = 0x02, /**< Scaling time constant */
    HOMEO_NOISE_SCALING_EXP      = 0x04, /**< Scaling exponent */
    HOMEO_NOISE_RATE_AVG_TAU     = 0x08, /**< Rate averaging tau */
    HOMEO_NOISE_ALL              = 0x0F  /**< All parameters */
} homeo_noise_target_t;

/**
 * @brief Configuration for homeostatic pink noise bridge
 */
typedef struct {
    /* Pink noise parameters */
    float noise_alpha;              /**< Spectral exponent (default: 1.0) */
    float noise_amplitude;          /**< Base noise amplitude (default: 0.03) */
    float noise_sample_rate;        /**< Sampling rate in Hz (default: 1000.0) */

    /* Modulation targets */
    homeo_noise_target_t noise_targets; /**< Which params to modulate */

    /* Individual scaling factors */
    float target_rate_noise_scale;  /**< Scaling for target rate noise */
    float scaling_tau_noise_scale;  /**< Scaling for tau noise */
    float scaling_exp_noise_scale;  /**< Scaling for exponent noise */
    float rate_avg_tau_noise_scale; /**< Scaling for rate avg tau noise */

    /* Safety bounds */
    float target_rate_min;          /**< Minimum target rate (Hz) */
    float target_rate_max;          /**< Maximum target rate (Hz) */
    float scaling_tau_min;          /**< Minimum scaling tau (ms) */
    float scaling_tau_max;          /**< Maximum scaling tau (ms) */
    float scaling_exp_min;          /**< Minimum scaling exponent */
    float scaling_exp_max;          /**< Maximum scaling exponent */

    /* Control */
    bool enabled;                   /**< Enable/disable bridge */
    uint32_t seed;                  /**< Random seed */
} homeo_pink_noise_config_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief Pink noise bridge state for homeostatic plasticity
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    homeo_pink_noise_config_t config;

    /* Connected module */
    synaptic_scaling_params_t* scaling_params; /**< Synaptic scaling params */
    pink_noise_generator_t noise_gen;          /**< Pink noise generator */

    /* Modulated parameters (cached) */
    float noisy_target_rate;        /**< Current noisy target rate */
    float noisy_scaling_tau;        /**< Current noisy scaling tau */
    float noisy_scaling_exp;        /**< Current noisy scaling exponent */
    float noisy_rate_avg_tau;       /**< Current noisy rate averaging tau */

    /* Noise samples (current) */
    float target_rate_noise;        /**< Current target rate noise */
    float scaling_tau_noise;        /**< Current tau noise */
    float scaling_exp_noise;        /**< Current exponent noise */
    float rate_avg_tau_noise;       /**< Current rate avg tau noise */

    /* Statistics */
    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;
    float avg_target_rate_shift;

    /* State flags */
    bool is_enabled;
    bool params_connected;
    bool noise_connected;
} homeo_pink_noise_bridge_t;

/**
 * @brief Statistics from pink noise bridge
 */
typedef struct {
    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;
    float current_noisy_target_rate;
    float current_noisy_scaling_exp;
    float avg_target_rate_shift;
} homeo_pink_noise_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

static inline homeo_pink_noise_config_t homeo_pink_noise_default_config(void) {
    return (homeo_pink_noise_config_t){
        .noise_alpha = HOMEO_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = HOMEO_PINK_NOISE_DEFAULT_AMPLITUDE,
        .noise_sample_rate = 1000.0f,

        .noise_targets = HOMEO_NOISE_ALL,

        .target_rate_noise_scale = 1.0f,
        .scaling_tau_noise_scale = 0.5f,
        .scaling_exp_noise_scale = 0.3f,
        .rate_avg_tau_noise_scale = 0.4f,

        .target_rate_min = 0.5f,
        .target_rate_max = 20.0f,
        .scaling_tau_min = 1000.0f,   // 1 second
        .scaling_tau_max = 86400000.0f, // 24 hours in ms
        .scaling_exp_min = 0.5f,
        .scaling_exp_max = 2.0f,

        .enabled = true,
        .seed = 98765
    };
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise bridge for homeostatic plasticity
 */
homeo_pink_noise_bridge_t* homeo_pink_noise_create(
    const homeo_pink_noise_config_t* config
);

/**
 * @brief Destroy pink noise bridge
 */
void homeo_pink_noise_destroy(homeo_pink_noise_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to synaptic scaling parameters
 */
int homeo_pink_noise_connect_scaling(
    homeo_pink_noise_bridge_t* bridge,
    synaptic_scaling_params_t* params
);

/**
 * @brief Disconnect from parameters
 */
int homeo_pink_noise_disconnect(homeo_pink_noise_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Generate new noise and update modulated parameters
 */
int homeo_pink_noise_update(homeo_pink_noise_bridge_t* bridge);

/**
 * @brief Apply modulated parameters to scaling state
 */
int homeo_pink_noise_apply_to_state(
    homeo_pink_noise_bridge_t* bridge,
    synaptic_scaling_state_t* state
);

//=============================================================================
// Query Functions
//=============================================================================

float homeo_pink_noise_get_noisy_target_rate(const homeo_pink_noise_bridge_t* bridge);
float homeo_pink_noise_get_noisy_scaling_exp(const homeo_pink_noise_bridge_t* bridge);
int homeo_pink_noise_get_stats(
    const homeo_pink_noise_bridge_t* bridge,
    homeo_pink_noise_stats_t* stats
);

//=============================================================================
// Control Functions
//=============================================================================

int homeo_pink_noise_set_enabled(homeo_pink_noise_bridge_t* bridge, bool enabled);
int homeo_pink_noise_reset(homeo_pink_noise_bridge_t* bridge);
int homeo_pink_noise_set_amplitude(homeo_pink_noise_bridge_t* bridge, float amplitude);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HOMEOSTATIC_PINK_NOISE_BRIDGE_H
