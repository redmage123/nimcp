//=============================================================================
// nimcp_stdp_pink_noise_bridge.h - Pink Noise Bridge for Quantum STDP Optimizer
//=============================================================================
/**
 * @file nimcp_stdp_pink_noise_bridge.h
 * @brief Integrates quantum-inspired pink noise with STDP optimizer
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies quantum-inspired 1/f noise to STDP learning rate optimization
 * WHY:  Biological neural systems use pink noise for robust learning:
 *       - Multi-timescale exploration (fast + slow components)
 *       - Escape local minima via correlated perturbations
 *       - Matches synaptic variability (Faisal et al., 2008)
 *       - Quantum tunneling enhanced by 1/f noise spectrum
 *
 * HOW:  Connect pink_noise_quantum_bridge to quantum_stdp_optimizer,
 *       apply noise to learning rate parameters, track optimization metrics
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Synaptic strength fluctuates with 1/f noise (Câteau & Reyes, 2006)
 * - Metaplasticity varies on multiple timescales (Abraham & Bear, 1996)
 * - Learning rates adapt with pink noise to maintain homeostasis
 * - Quantum effects may enhance neural noise (Tegmark debates notwithstanding)
 *
 * INTEGRATION PATTERN:
 * ====================
 * 1. Quantum STDP optimizer provides base learning rate
 * 2. Pink noise bridge generates quantum-inspired 1/f noise
 * 3. Modulate learning rate: LR_effective = LR_base * (1 + α*noise)
 * 4. Apply noise to exploration radius for quantum tunneling
 * 5. Track noise impact on optimization convergence
 *
 * NOISE MODULATION STRATEGY:
 * ==========================
 * - Additive for learning rate: LR = LR_base + noise_lr
 * - Multiplicative for amplitudes: A_plus *= (1 + noise_a)
 * - Temperature-scaled: Noise amplitude ∝ current_temperature
 * - Frequency-dependent: Low-freq noise for slow adaptation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STDP_PINK_NOISE_BRIDGE_H
#define NIMCP_STDP_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/stdp/nimcp_quantum_stdp_optimizer.h"
#include "plasticity/noise/nimcp_pink_noise_quantum_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define STDP_PINK_NOISE_DEFAULT_ALPHA       1.0f    /**< True pink noise */
#define STDP_PINK_NOISE_DEFAULT_AMPLITUDE   0.05f   /**< 5% modulation */
#define STDP_PINK_NOISE_MIN_LR              0.0001f /**< Min learning rate */
#define STDP_PINK_NOISE_MAX_LR              0.5f    /**< Max learning rate */

//=============================================================================
// Noise Application Modes
//=============================================================================

/**
 * @brief How noise is applied to STDP parameters
 */
typedef enum {
    STDP_NOISE_ADDITIVE,        /**< LR = LR_base + noise */
    STDP_NOISE_MULTIPLICATIVE,  /**< LR = LR_base * (1 + noise) */
    STDP_NOISE_TEMPERATURE_SCALED, /**< Amplitude ∝ quantum temperature */
    STDP_NOISE_ADAPTIVE         /**< Switch mode based on optimization phase */
} stdp_noise_mode_t;

/**
 * @brief Which STDP parameters receive noise modulation
 */
typedef enum {
    STDP_NOISE_TARGET_LR        = 0x01, /**< Learning rate only */
    STDP_NOISE_TARGET_AMPLITUDES = 0x02, /**< A_plus, A_minus */
    STDP_NOISE_TARGET_TAUS      = 0x04, /**< Tau_plus, Tau_minus */
    STDP_NOISE_TARGET_ALL       = 0x07  /**< All parameters */
} stdp_noise_target_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for STDP pink noise bridge
 */
typedef struct {
    /* Pink noise parameters */
    float noise_alpha;              /**< Spectral exponent (default: 1.0) */
    float noise_amplitude;          /**< Base noise amplitude (default: 0.05) */
    float noise_sample_rate;        /**< Sampling rate in Hz (default: 1000.0) */
    pink_quantum_method_t quantum_method; /**< Quantum noise method */

    /* Modulation parameters */
    stdp_noise_mode_t noise_mode;   /**< Noise application mode */
    stdp_noise_target_t noise_targets; /**< Which params to modulate */
    float lr_noise_scale;           /**< Scaling for LR noise (default: 1.0) */
    float amplitude_noise_scale;    /**< Scaling for A± noise (default: 0.5) */
    float tau_noise_scale;          /**< Scaling for τ± noise (default: 0.3) */

    /* Temperature coupling */
    bool couple_to_temperature;     /**< Scale noise by quantum temp */
    float min_temperature_threshold; /**< Below this, disable noise */

    /* Safety bounds */
    float lr_min;                   /**< Minimum learning rate */
    float lr_max;                   /**< Maximum learning rate */
    float a_min;                    /**< Minimum amplitude */
    float a_max;                    /**< Maximum amplitude */
    float tau_min;                  /**< Minimum time constant */
    float tau_max;                  /**< Maximum time constant */

    /* Control */
    bool enabled;                   /**< Enable/disable bridge */
    uint32_t seed;                  /**< Random seed */
} stdp_pink_noise_config_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief Pink noise bridge state connecting to STDP optimizer
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    stdp_pink_noise_config_t config;

    /* Connected modules */
    qstdp_optimizer_t stdp_optimizer;    /**< Quantum STDP optimizer */
    pink_quantum_bridge_t* pink_bridge;  /**< Quantum pink noise generator */

    /* Modulated parameters (cached) */
    float noisy_lr;                 /**< Current noisy learning rate */
    float noisy_a_plus;             /**< Current noisy LTP amplitude */
    float noisy_a_minus;            /**< Current noisy LTD amplitude */
    float noisy_tau_plus;           /**< Current noisy LTP tau */
    float noisy_tau_minus;          /**< Current noisy LTD tau */

    /* Noise samples (current) */
    float lr_noise;                 /**< Current LR noise sample */
    float a_plus_noise;             /**< Current A+ noise sample */
    float a_minus_noise;            /**< Current A- noise sample */
    float tau_plus_noise;           /**< Current τ+ noise sample */
    float tau_minus_noise;          /**< Current τ- noise sample */

    /* Statistics */
    uint64_t samples_generated;     /**< Total noise samples */
    uint64_t parameters_modulated;  /**< Total parameter updates */
    float avg_noise_amplitude;      /**< Running average noise amplitude */
    float max_noise_amplitude;      /**< Maximum noise amplitude seen */
    float noise_impact_on_energy;   /**< Correlation: noise vs energy change */

    /* State flags */
    bool is_enabled;                /**< Currently active */
    bool optimizer_connected;       /**< STDP optimizer connected */
    bool noise_connected;           /**< Pink noise bridge connected */
} stdp_pink_noise_bridge_t;

/**
 * @brief Statistics from pink noise bridge
 */
typedef struct {
    uint64_t samples_generated;
    uint64_t parameters_modulated;
    float avg_noise_amplitude;
    float max_noise_amplitude;
    float current_noisy_lr;
    float noise_impact_on_energy;
    pink_quantum_stats_t quantum_stats; /**< From pink quantum bridge */
} stdp_pink_noise_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default pink noise bridge configuration
 *
 * WHAT: Returns sensible defaults for STDP noise modulation
 * WHY:  Easy integration without parameter tuning
 * HOW:  Based on biological synaptic variability studies
 *
 * @return Default configuration
 */
static inline stdp_pink_noise_config_t stdp_pink_noise_default_config(void) {
    return (stdp_pink_noise_config_t){
        .noise_alpha = STDP_PINK_NOISE_DEFAULT_ALPHA,
        .noise_amplitude = STDP_PINK_NOISE_DEFAULT_AMPLITUDE,
        .noise_sample_rate = 1000.0f,
        .quantum_method = PINK_QUANTUM_HYBRID,

        .noise_mode = STDP_NOISE_MULTIPLICATIVE,
        .noise_targets = STDP_NOISE_TARGET_ALL,
        .lr_noise_scale = 1.0f,
        .amplitude_noise_scale = 0.5f,
        .tau_noise_scale = 0.3f,

        .couple_to_temperature = true,
        .min_temperature_threshold = 0.05f,

        .lr_min = STDP_PINK_NOISE_MIN_LR,
        .lr_max = STDP_PINK_NOISE_MAX_LR,
        .a_min = 1e-5f,
        .a_max = 0.1f,
        .tau_min = 5.0f,
        .tau_max = 100.0f,

        .enabled = true,
        .seed = 12345
    };
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise bridge for STDP optimizer
 *
 * WHAT: Initialize bridge connecting quantum noise to STDP optimization
 * WHY:  Enable biologically-realistic stochastic learning rate adaptation
 * HOW:  Create pink quantum bridge, allocate state, setup connections
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
stdp_pink_noise_bridge_t* stdp_pink_noise_create(
    const stdp_pink_noise_config_t* config
);

/**
 * @brief Destroy pink noise bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy pink bridge, free state
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void stdp_pink_noise_destroy(stdp_pink_noise_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect STDP optimizer to bridge
 *
 * WHAT: Attach quantum STDP optimizer for parameter modulation
 * WHY:  Bridge needs access to optimizer state
 * HOW:  Store optimizer reference, validate connection
 *
 * @param bridge Pink noise bridge
 * @param optimizer Quantum STDP optimizer
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_connect_optimizer(
    stdp_pink_noise_bridge_t* bridge,
    qstdp_optimizer_t optimizer
);

/**
 * @brief Disconnect STDP optimizer
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_disconnect_optimizer(stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Check if optimizer is connected
 *
 * @param bridge Pink noise bridge
 * @return true if optimizer connected
 */
bool stdp_pink_noise_has_optimizer(const stdp_pink_noise_bridge_t* bridge);

//=============================================================================
// Noise Generation and Application
//=============================================================================

/**
 * @brief Update noise samples from quantum pink noise generator
 *
 * WHAT: Generate new noise samples for all STDP parameters
 * WHY:  Refresh noise for current optimization step
 * HOW:  Call pink_quantum_generate_sample for each parameter
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_update_samples(stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Apply noise to STDP parameters
 *
 * WHAT: Modulate STDP optimizer parameters with quantum pink noise
 * WHY:  Inject biologically-realistic stochasticity for robust learning
 * HOW:  Read base params from optimizer, apply noise, clamp to bounds
 *
 * ALGORITHM:
 *   1. Get base parameters from optimizer
 *   2. For each target parameter:
 *      - Generate quantum pink noise sample
 *      - Apply based on mode (additive/multiplicative/temperature-scaled)
 *      - Scale by parameter-specific noise factor
 *   3. Clamp modulated values to safety bounds
 *   4. Return noisy parameters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_apply_modulation(stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Return current noisy learning rate
 * WHY:  Access modulated parameter for STDP application
 * HOW:  Return cached noisy_lr from last apply_modulation
 *
 * @param bridge Pink noise bridge
 * @return Noisy learning rate
 */
float stdp_pink_noise_get_noisy_lr(const stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Get all modulated parameters
 *
 * WHAT: Retrieve all noisy STDP parameters
 * WHY:  Batch access to modulated values
 * HOW:  Copy cached noisy parameters to output
 *
 * @param bridge Pink noise bridge
 * @param lr Output: noisy learning rate
 * @param a_plus Output: noisy LTP amplitude
 * @param a_minus Output: noisy LTD amplitude
 * @param tau_plus Output: noisy LTP time constant
 * @param tau_minus Output: noisy LTD time constant
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_get_noisy_params(
    const stdp_pink_noise_bridge_t* bridge,
    float* lr,
    float* a_plus,
    float* a_minus,
    float* tau_plus,
    float* tau_minus
);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * @brief Enable/disable pink noise bridge
 *
 * @param bridge Pink noise bridge
 * @param enabled Whether bridge is enabled
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_set_enabled(
    stdp_pink_noise_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Check if bridge is enabled
 *
 * @param bridge Pink noise bridge
 * @return true if enabled and operational
 */
bool stdp_pink_noise_is_enabled(const stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Set noise application mode
 *
 * @param bridge Pink noise bridge
 * @param mode Noise mode to use
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_set_mode(
    stdp_pink_noise_bridge_t* bridge,
    stdp_noise_mode_t mode
);

/**
 * @brief Set which parameters receive noise
 *
 * @param bridge Pink noise bridge
 * @param targets Bitfield of parameters to modulate
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_set_targets(
    stdp_pink_noise_bridge_t* bridge,
    stdp_noise_target_t targets
);

/**
 * @brief Set quantum noise method
 *
 * @param bridge Pink noise bridge
 * @param method Quantum method to use
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_set_quantum_method(
    stdp_pink_noise_bridge_t* bridge,
    pink_quantum_method_t method
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Pink noise bridge
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_get_stats(
    const stdp_pink_noise_bridge_t* bridge,
    stdp_pink_noise_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_reset_stats(stdp_pink_noise_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear all state and reseed noise generator
 * WHY:  Start fresh noise sequence
 * HOW:  Reset pink quantum bridge, clear cached params
 *
 * @param bridge Pink noise bridge
 * @param new_seed New random seed (0 = use configured)
 * @return 0 on success, negative on error
 */
int stdp_pink_noise_reset(
    stdp_pink_noise_bridge_t* bridge,
    uint32_t new_seed
);

/**
 * @brief Get noise mode name as string
 *
 * @param mode Noise mode
 * @return Human-readable name
 */
const char* stdp_noise_mode_name(stdp_noise_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_STDP_PINK_NOISE_BRIDGE_H
