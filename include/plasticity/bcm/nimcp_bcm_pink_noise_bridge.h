//=============================================================================
// nimcp_bcm_pink_noise_bridge.h - Pink Noise Bridge for BCM Plasticity
//=============================================================================
/**
 * @file nimcp_bcm_pink_noise_bridge.h
 * @brief Integrates 1/f pink noise with BCM learning rule
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies biologically-realistic 1/f noise to BCM plasticity parameters
 * WHY:  Cortical neurons exhibit pink noise in their firing patterns:
 *       - Threshold fluctuations follow 1/f spectrum
 *       - Enables multi-timescale adaptation
 *       - Improves robustness to local minima
 *       - Matches observed synaptic variability
 *
 * HOW:  Generate pink noise samples and modulate BCM parameters:
 *       - Learning rate: prevents stuck states
 *       - Threshold: dynamic sliding with noise
 *       - Time constants: variable adaptation speeds
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Cortical neurons fire with 1/f statistics (Bedard et al., 2006)
 * - Threshold fluctuations are pink, not white (Yu & Lewis, 2012)
 * - BCM thresholds slide with noisy dynamics in vivo
 * - Multi-timescale noise enables robust learning
 *
 * NOISE MODULATION STRATEGY:
 * ==========================
 * - Threshold: θ_noisy = θ * (1 + amplitude * noise)
 * - Learning rate: η_noisy = η * (1 + lr_scale * noise)
 * - Time constants: τ_noisy = τ * (1 + tau_scale * noise)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BCM_PINK_NOISE_BRIDGE_H
#define NIMCP_BCM_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BCM_PINK_NOISE_DEFAULT_ALPHA       1.0f    /**< True pink noise */
#define BCM_PINK_NOISE_DEFAULT_AMPLITUDE   0.05f   /**< 5% threshold modulation */
#define BCM_PINK_NOISE_MIN_THRESHOLD       0.01f   /**< Minimum threshold */
#define BCM_PINK_NOISE_MAX_THRESHOLD       1.0f    /**< Maximum threshold */
#define BCM_PINK_NOISE_DEFAULT_LR          0.01f   /**< Default learning rate */

//=============================================================================
// Noise Application Modes
//=============================================================================

/**
 * @brief How noise is applied to BCM parameters
 */
typedef enum {
    BCM_NOISE_ADDITIVE,         /**< θ = θ_base + noise */
    BCM_NOISE_MULTIPLICATIVE,   /**< θ = θ_base * (1 + noise) */
    BCM_NOISE_ADAPTIVE          /**< Switch mode based on activity level */
} bcm_noise_mode_t;

/**
 * @brief Which BCM parameters receive noise modulation
 */
typedef enum {
    BCM_NOISE_TARGET_THRESHOLD      = 0x01, /**< Sliding threshold only */
    BCM_NOISE_TARGET_LR             = 0x02, /**< Learning rate only */
    BCM_NOISE_TARGET_TIME_CONSTANTS = 0x04, /**< Time constants */
    BCM_NOISE_TARGET_ALL            = 0x07  /**< All parameters */
} bcm_noise_target_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for BCM pink noise bridge
 */
typedef struct {
    /* Pink noise parameters */
    float noise_alpha;              /**< Spectral exponent (default: 1.0) */
    float noise_amplitude;          /**< Base noise amplitude (default: 0.05) */
    float noise_sample_rate;        /**< Sampling rate in Hz (default: 1000.0) */

    /* Modulation parameters */
    bcm_noise_mode_t noise_mode;    /**< Noise application mode */
    bcm_noise_target_t noise_targets; /**< Which params to modulate */
    float threshold_noise_scale;    /**< Scaling for threshold noise */
    float lr_noise_scale;           /**< Scaling for LR noise (default: 0.5) */
    float tau_noise_scale;          /**< Scaling for τ noise (default: 0.3) */

    /* Activity-dependent modulation */
    bool activity_dependent;        /**< Scale noise by activity level */
    float low_activity_boost;       /**< Noise boost when activity is low */
    float high_activity_suppression; /**< Noise reduction when activity is high */

    /* Safety bounds */
    float threshold_min;            /**< Minimum threshold */
    float threshold_max;            /**< Maximum threshold */
    float lr_min;                   /**< Minimum learning rate */
    float lr_max;                   /**< Maximum learning rate */
    float tau_min;                  /**< Minimum time constant */
    float tau_max;                  /**< Maximum time constant */

    /* Control */
    bool enabled;                   /**< Enable/disable bridge */
    uint32_t seed;                  /**< Random seed */
} bcm_pink_noise_config_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief Pink noise bridge state connecting to BCM plasticity
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    bcm_pink_noise_config_t config;

    /* Connected module */
    bcm_params_t* bcm_params;       /**< BCM parameters to modulate */
    pink_noise_generator_t noise_gen; /**< Pink noise generator */

    /* Modulated parameters (cached) */
    float noisy_threshold;          /**< Current noisy threshold */
    float noisy_lr;                 /**< Current noisy learning rate */
    float noisy_threshold_tau;      /**< Current noisy threshold tau */
    float noisy_activity_tau;       /**< Current noisy activity tau */

    /* Noise samples (current) */
    float threshold_noise;          /**< Current threshold noise sample */
    float lr_noise;                 /**< Current LR noise sample */
    float tau_noise;                /**< Current tau noise sample */

    /* Activity tracking */
    float current_activity;         /**< Current activity level */
    float activity_ema;             /**< Exponential moving average of activity */

    /* Statistics */
    uint64_t samples_generated;     /**< Total noise samples */
    uint64_t parameters_modulated;  /**< Total parameter updates */
    float avg_noise_amplitude;      /**< Running average noise amplitude */
    float max_noise_amplitude;      /**< Maximum noise amplitude seen */
    float avg_threshold_shift;      /**< Average threshold change from noise */

    /* State flags */
    bool is_enabled;                /**< Currently active */
    bool bcm_connected;             /**< BCM params connected */
    bool noise_connected;           /**< Noise generator connected */
} bcm_pink_noise_bridge_t;

/**
 * @brief Statistics from pink noise bridge
 */
typedef struct {
    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;
    float current_noisy_threshold;
    float current_noisy_lr;
    float avg_threshold_shift;
} bcm_pink_noise_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default pink noise bridge configuration
 *
 * WHAT: Returns sensible defaults for BCM noise modulation
 * WHY:  Easy integration without parameter tuning
 * HOW:  Based on cortical noise measurements
 *
 * @return Default configuration
 */
static inline bcm_pink_noise_config_t bcm_pink_noise_default_config(void) {
    return (bcm_pink_noise_config_t){
        .noise_alpha = BCM_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = BCM_PINK_NOISE_DEFAULT_AMPLITUDE,
        .noise_sample_rate = 1000.0f,

        .noise_mode = BCM_NOISE_MULTIPLICATIVE,
        .noise_targets = BCM_NOISE_TARGET_ALL,
        .threshold_noise_scale = 1.0f,
        .lr_noise_scale = 0.5f,
        .tau_noise_scale = 0.3f,

        .activity_dependent = true,
        .low_activity_boost = 1.5f,
        .high_activity_suppression = 0.5f,

        .threshold_min = BCM_PINK_NOISE_MIN_THRESHOLD,
        .threshold_max = BCM_PINK_NOISE_MAX_THRESHOLD,
        .lr_min = 0.0001f,
        .lr_max = 0.1f,
        .tau_min = 10.0f,
        .tau_max = 1000.0f,

        .enabled = true,
        .seed = 54321
    };
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise bridge for BCM plasticity
 *
 * WHAT: Initialize bridge connecting pink noise to BCM parameters
 * WHY:  Enable biologically-realistic stochastic threshold dynamics
 * HOW:  Create noise generator, allocate state, setup connections
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
bcm_pink_noise_bridge_t* bcm_pink_noise_create(
    const bcm_pink_noise_config_t* config
);

/**
 * @brief Destroy pink noise bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy noise generator, free state
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void bcm_pink_noise_destroy(bcm_pink_noise_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to BCM parameters
 *
 * WHAT: Link bridge to BCM learning rule parameters
 * WHY:  Enable parameter modulation
 * HOW:  Store pointer to BCM params
 *
 * @param bridge Pink noise bridge
 * @param params BCM parameters to modulate
 * @return 0 on success, negative on error
 */
int bcm_pink_noise_connect_bcm(
    bcm_pink_noise_bridge_t* bridge,
    bcm_params_t* params
);

/**
 * @brief Disconnect from BCM
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int bcm_pink_noise_disconnect_bcm(bcm_pink_noise_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Generate new noise samples and update modulated parameters
 *
 * WHAT: Main update function - generates noise and applies modulation
 * WHY:  Called each timestep to update noisy parameters
 * HOW:  Sample pink noise, apply to threshold/LR/tau
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int bcm_pink_noise_update(bcm_pink_noise_bridge_t* bridge);

/**
 * @brief Update with activity level
 *
 * WHAT: Update noise modulation considering current activity
 * WHY:  Activity-dependent noise scaling
 * HOW:  Boost noise when activity low, suppress when high
 *
 * @param bridge Pink noise bridge
 * @param activity Current activity level [0-1]
 * @return 0 on success
 */
int bcm_pink_noise_update_with_activity(
    bcm_pink_noise_bridge_t* bridge,
    float activity
);

/**
 * @brief Apply modulated parameters to BCM synapse
 *
 * WHAT: Apply current noisy values to synapse
 * WHY:  Transfer noise effects to actual learning
 * HOW:  Copy noisy params to synapse threshold
 *
 * @param bridge Pink noise bridge
 * @param synapse BCM synapse to modulate
 * @return 0 on success
 */
int bcm_pink_noise_apply_to_synapse(
    bcm_pink_noise_bridge_t* bridge,
    bcm_synapse_t* synapse
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current noisy threshold
 *
 * @param bridge Pink noise bridge
 * @return Noisy threshold value
 */
float bcm_pink_noise_get_noisy_threshold(const bcm_pink_noise_bridge_t* bridge);

/**
 * @brief Get current noisy learning rate
 *
 * @param bridge Pink noise bridge
 * @return Noisy learning rate
 */
float bcm_pink_noise_get_noisy_lr(const bcm_pink_noise_bridge_t* bridge);

/**
 * @brief Get current threshold noise sample
 *
 * @param bridge Pink noise bridge
 * @return Current noise sample
 */
float bcm_pink_noise_get_threshold_noise(const bcm_pink_noise_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Pink noise bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int bcm_pink_noise_get_stats(
    const bcm_pink_noise_bridge_t* bridge,
    bcm_pink_noise_stats_t* stats
);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * @brief Enable/disable the bridge
 *
 * @param bridge Pink noise bridge
 * @param enabled true to enable, false to disable
 * @return 0 on success
 */
int bcm_pink_noise_set_enabled(
    bcm_pink_noise_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Reset bridge state and statistics
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int bcm_pink_noise_reset(bcm_pink_noise_bridge_t* bridge);

/**
 * @brief Set noise amplitude
 *
 * @param bridge Pink noise bridge
 * @param amplitude New noise amplitude
 * @return 0 on success
 */
int bcm_pink_noise_set_amplitude(
    bcm_pink_noise_bridge_t* bridge,
    float amplitude
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BCM_PINK_NOISE_BRIDGE_H
