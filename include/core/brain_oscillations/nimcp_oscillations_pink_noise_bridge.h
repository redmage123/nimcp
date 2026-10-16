/**
 * @file nimcp_oscillations_pink_noise_bridge.h
 * @brief Brain Oscillations-Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Integration bridge adding 1/f pink noise to brain oscillation generators
 * WHY:  Pink noise (1/f spectrum) is fundamental to neural dynamics and oscillations.
 *       Real brain oscillations exhibit 1/f power spectral density (PSD), not pure
 *       sine waves. Pink noise provides:
 *       - Biological realism: Matches empirical EEG/LFP power spectra
 *       - Multi-timescale dynamics: Long-range temporal correlations
 *       - Criticality: Self-organized criticality in neural networks
 *       - Variability: Trial-to-trial variability observed in neural recordings
 * HOW:  Creates pink noise generator, adds 1/f background noise to oscillation
 *       activity buffer, modulates oscillation coherence and synchrony naturally.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * 1/f NOISE IN NEURAL OSCILLATIONS:
 * ----------------------------------
 * - Brain activity exhibits 1/f power spectral density across frequencies
 * - Reference: He (2014) "Scale-free brain activity: past, present and future"
 * - Reference: Miller et al. (2009) "Power-law scaling in the brain surface electric
 *   potential"
 * - 1/f noise arises from hierarchical network dynamics and criticality
 * - Reference: Bedard et al. (2006) "Does the 1/f frequency scaling of brain signals
 *   reflect self-organized critical states?"
 *
 * PINK NOISE AND OSCILLATION INTERACTIONS:
 * -----------------------------------------
 * - Pure oscillations (sine waves) are unbiological; real oscillations have noise
 * - Pink noise creates naturalistic trial-to-trial variability
 * - 1/f noise modulates oscillation amplitude → realistic amplitude fluctuations
 * - Pink noise maintains long-range temporal correlations in oscillations
 * - Reference: Linkenkaer-Hansen et al. (2001) "Long-range temporal correlations
 *   and scaling behavior in human brain oscillations"
 *
 * CRITICALITY AND 1/f NOISE:
 * --------------------------
 * - Self-organized criticality produces 1/f noise in neural networks
 * - Critical networks balance order and disorder → optimal computation
 * - 1/f PSD is hallmark of systems at criticality
 * - Reference: Beggs & Plenz (2003) "Neuronal avalanches in neocortical circuits"
 *
 * OSCILLATION BANDS AND PINK NOISE:
 * ----------------------------------
 * - Delta, theta, alpha, beta, gamma oscillations all sit on 1/f background
 * - Spectral peaks (oscillations) emerge from 1/f continuum
 * - Pink noise floor determines baseline variability in each band
 * - Reference: Buzsaki & Draguhn (2004) "Neuronal oscillations in cortical networks"
 *
 * COHERENCE AND SYNCHRONY MODULATION:
 * ------------------------------------
 * - Pink noise naturally reduces coherence (more realistic than pure sine waves)
 * - 1/f variability creates transient synchronization/desynchronization
 * - Noise-driven state transitions between synchronized/desynchronized states
 * - Reference: Deco et al. (2017) "The dynamics of resting fluctuations in the brain"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              BRAIN OSCILLATIONS-PINK NOISE BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               PINK NOISE → OSCILLATIONS PATHWAY                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  PINK NOISE GEN  │                                             │  ║
 * ║   │   │  ──────────────  │                                             │  ║
 * ║   │   │  α = 1.0 (1/f)   │                                             │  ║
 * ║   │   │  Multi-scale:    │                                             │  ║
 * ║   │   │   - Delta scale  │                                             │  ║
 * ║   │   │   - Theta scale  │                                             │  ║
 * ║   │   │   - Alpha scale  │                                             │  ║
 * ║   │   │   - Beta scale   │                                             │  ║
 * ║   │   │   - Gamma scale  │                                             │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │            │                                                        │  ║
 * ║   │            ▼                                                        │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  NOISE INJECTION │                                             │  ║
 * ║   │   │  ───────────────│                                              │  ║
 * ║   │   │  Add to activity │                                             │  ║
 * ║   │   │  buffer          │                                             │  ║
 * ║   │   │  A'(t) = A(t) +  │                                             │  ║
 * ║   │   │    amplitude *   │                                             │  ║
 * ║   │   │    pink_noise(t) │                                             │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │            │                                                        │  ║
 * ║   │            ▼                                                        │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  OSCILLATION     │                                             │  ║
 * ║   │   │  ANALYZER        │                                             │  ║
 * ║   │   │  ───────────────│                                              │  ║
 * ║   │   │  FFT analysis    │                                             │  ║
 * ║   │   │  Brain wave power│                                             │  ║
 * ║   │   │  Natural 1/f PSD │                                             │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   EFFECTS:                                                          │  ║
 * ║   │   - Realistic amplitude variability                                │  ║
 * ║   │   - Trial-to-trial variability                                     │  ║
 * ║   │   - Natural coherence reduction (not artifactually perfect)        │  ║
 * ║   │   - Long-range temporal correlations                               │  ║
 * ║   │   - Spectral peaks on 1/f background (biological)                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OSCILLATIONS_PINK_NOISE_BRIDGE_H
#define NIMCP_OSCILLATIONS_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise_multiscale.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Pink noise amplitude defaults for each oscillation band */
#define PINK_NOISE_DELTA_AMPLITUDE      0.08f   /**< Delta band noise (1-4 Hz): 8% variability */
#define PINK_NOISE_THETA_AMPLITUDE      0.06f   /**< Theta band noise (4-8 Hz): 6% variability */
#define PINK_NOISE_ALPHA_AMPLITUDE      0.05f   /**< Alpha band noise (8-13 Hz): 5% variability */
#define PINK_NOISE_BETA_AMPLITUDE       0.04f   /**< Beta band noise (13-30 Hz): 4% variability */
#define PINK_NOISE_GAMMA_AMPLITUDE      0.03f   /**< Gamma band noise (30-100 Hz): 3% variability */

/* Default global settings */
#define PINK_NOISE_DEFAULT_ALPHA        1.0f    /**< True pink noise (1/f) */
#define PINK_NOISE_DEFAULT_GLOBAL_AMP   0.05f   /**< 5% global noise amplitude */
#define PINK_NOISE_MIN_FREQUENCY        0.5f    /**< Min frequency (delta band) */
#define PINK_NOISE_MAX_FREQUENCY        100.0f  /**< Max frequency (gamma band) */

/* Coherence and synchrony modulation */
#define PINK_NOISE_COHERENCE_REDUCTION  0.15f   /**< Pink noise reduces coherence by ~15% */
#define PINK_NOISE_SYNCHRONY_REDUCTION  0.10f   /**< Pink noise reduces synchrony by ~10% */

/* Update intervals */
#define PINK_NOISE_UPDATE_INTERVAL_MS   10      /**< Update noise every 10ms (100Hz) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Pink noise injection parameters
 *
 * Controls how pink noise is added to oscillation activity
 */
typedef struct {
    /* Band-specific amplitudes */
    float delta_amplitude;      /**< Delta band noise amplitude [0-0.2] */
    float theta_amplitude;      /**< Theta band noise amplitude [0-0.2] */
    float alpha_amplitude;      /**< Alpha band noise amplitude [0-0.2] */
    float beta_amplitude;       /**< Beta band noise amplitude [0-0.2] */
    float gamma_amplitude;      /**< Gamma band noise amplitude [0-0.2] */

    /* Global settings */
    float global_amplitude;     /**< Global amplitude scaling [0-0.5] */
    float alpha_exponent;       /**< Spectral exponent (1.0 = pink) [0.5-2.0] */

    /* Coherence/synchrony effects */
    float coherence_reduction;  /**< How much noise reduces coherence [0-0.5] */
    float synchrony_reduction;  /**< How much noise reduces synchrony [0-0.5] */

    /* Current noise value */
    float current_noise_sample; /**< Latest noise sample from generator */
} pink_noise_injection_params_t;

/**
 * @brief Complete oscillations-pink noise bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_oscillation_analyzer_t* oscillation_analyzer;
    pink_noise_generator_t pink_noise_gen;
    pink_noise_multiscale_t* multiscale_gen;

    /* Configuration */
    pink_noise_injection_params_t injection_params;
    pink_noise_config_t noise_config;
    pink_noise_multiscale_config_t multiscale_config;

    /* State */
    bool enabled;                   /**< Is pink noise injection enabled */
    bool use_multiscale;            /**< Use multi-scale pink noise */
    float sample_rate_hz;           /**< Oscillation analyzer sampling rate */
    uint64_t last_update_ms;        /**< Last time noise was updated */
    uint64_t total_samples_injected; /**< Total noise samples injected */

    /* Statistics */
    uint64_t total_updates;         /**< Total update calls */
    uint32_t noise_injections;      /**< Number of noise injections */
    float avg_noise_amplitude;      /**< Running average of noise amplitude */
    float peak_noise_amplitude;     /**< Peak noise amplitude observed */

    } oscillations_pink_noise_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Noise characteristics */
    float alpha_exponent;           /**< Spectral exponent [0.5-2.0] (default: 1.0) */
    float global_amplitude;         /**< Global noise amplitude [0-0.5] (default: 0.05) */

    /* Band-specific amplitudes */
    float delta_amplitude;          /**< Delta noise [0-0.2] (default: 0.08) */
    float theta_amplitude;          /**< Theta noise [0-0.2] (default: 0.06) */
    float alpha_amplitude;          /**< Alpha noise [0-0.2] (default: 0.05) */
    float beta_amplitude;           /**< Beta noise [0-0.2] (default: 0.04) */
    float gamma_amplitude;          /**< Gamma noise [0-0.2] (default: 0.03) */

    /* Coherence/synchrony effects */
    float coherence_reduction;      /**< Coherence reduction [0-0.5] (default: 0.15) */
    float synchrony_reduction;      /**< Synchrony reduction [0-0.5] (default: 0.10) */

    /* Multi-scale options */
    bool use_multiscale;            /**< Use multi-scale pink noise (default: false) */
    uint32_t num_scales;            /**< Number of scales if multiscale (default: 5) */

    /* Generation options */
    pink_noise_method_t method;     /**< Noise generation method (default: VOSS) */
    uint32_t seed;                  /**< Random seed (0=time-based) */
} oscillations_pink_noise_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration for pink noise injection
 * WHY:  Easy initialization with biologically realistic parameters
 * HOW:  Return struct with evidence-based defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_default_config(oscillations_pink_noise_config_t* config);

/**
 * @brief Create oscillations-pink noise bridge
 *
 * WHAT: Initialize pink noise integration with brain oscillations
 * WHY:  Add biologically realistic 1/f noise to oscillation dynamics
 * HOW:  Create pink noise generator, allocate structure, link to analyzer
 *
 * @param config Configuration (NULL for defaults)
 * @param oscillation_analyzer Brain oscillation analyzer to inject noise into
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) + pink noise generator state
 */
oscillations_pink_noise_bridge_t* oscillations_pink_noise_bridge_create(
    const oscillations_pink_noise_config_t* config,
    brain_oscillation_analyzer_t* oscillation_analyzer
);

/**
 * @brief Destroy oscillations-pink noise bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Destroy pink noise generators, free structure
 *
 * @param bridge Bridge to destroy
 *
 * NOTE: Does not destroy oscillation_analyzer (caller owns it)
 */
void oscillations_pink_noise_bridge_destroy(oscillations_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Enable/Disable API
 * ============================================================================ */

/**
 * @brief Enable pink noise injection
 *
 * WHAT: Start adding pink noise to oscillation activity
 * WHY:  Turn on biologically realistic noise
 * HOW:  Set enabled flag, reset noise generator
 *
 * @param bridge Oscillations-pink noise bridge
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_enable(oscillations_pink_noise_bridge_t* bridge);

/**
 * @brief Disable pink noise injection
 *
 * WHAT: Stop adding pink noise to oscillation activity
 * WHY:  Compare noisy vs clean oscillations
 * HOW:  Clear enabled flag
 *
 * @param bridge Oscillations-pink noise bridge
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_disable(oscillations_pink_noise_bridge_t* bridge);

/**
 * @brief Check if pink noise is enabled
 *
 * @param bridge Oscillations-pink noise bridge
 * @return true if enabled, false otherwise
 */
bool oscillations_pink_noise_is_enabled(const oscillations_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Pink Noise → Oscillations API
 * ============================================================================ */

/**
 * @brief Inject pink noise into oscillation activity
 *
 * WHAT: Add 1/f noise sample to current oscillation activity
 * WHY:  Create biologically realistic variability in oscillations
 * HOW:  Generate noise sample, add to activity buffer via record_value()
 *
 * @param bridge Oscillations-pink noise bridge
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL MODEL:
 * - Generate noise from 1/f generator
 * - Scale by current dominant band amplitude
 * - Add to oscillation analyzer activity buffer
 * - Result: oscillations with naturalistic 1/f background
 *
 * COMPLEXITY: O(1) for single-scale, O(num_scales) for multi-scale
 */
int oscillations_pink_noise_inject(oscillations_pink_noise_bridge_t* bridge);

/**
 * @brief Apply pink noise effects to oscillation metrics
 *
 * WHAT: Modulate coherence and synchrony based on noise level
 * WHY:  Pink noise naturally reduces perfect phase-locking
 * HOW:  Reduce coherence/synchrony proportional to noise amplitude
 *
 * @param bridge Oscillations-pink noise bridge
 * @param analysis Oscillation analysis to modulate (in/out)
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - Coherence reduced by coherence_reduction factor
 * - Synchrony reduced by synchrony_reduction factor
 * - Maintains biological realism (no perfect synchronization)
 *
 * COMPLEXITY: O(1)
 */
int oscillations_pink_noise_apply_effects(
    oscillations_pink_noise_bridge_t* bridge,
    oscillation_analysis_t* analysis
);

/**
 * @brief Update bridge state (call periodically)
 *
 * WHAT: Update pink noise state, inject into oscillations if enabled
 * WHY:  Maintain continuous noise injection
 * HOW:  Check update interval, generate noise, inject if enabled
 *
 * @param bridge Oscillations-pink noise bridge
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 *
 * RECOMMENDED: Call at oscillation analyzer sampling rate
 * - Example: 250 Hz analyzer → call every 4ms
 *
 * COMPLEXITY: O(1) or O(num_scales) if multiscale
 */
int oscillations_pink_noise_bridge_update(
    oscillations_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set global noise amplitude
 *
 * WHAT: Adjust overall pink noise intensity
 * WHY:  Control strength of 1/f background
 * HOW:  Update global_amplitude parameter
 *
 * @param bridge Oscillations-pink noise bridge
 * @param amplitude New global amplitude [0-0.5]
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_set_amplitude(
    oscillations_pink_noise_bridge_t* bridge,
    float amplitude
);

/**
 * @brief Set band-specific noise amplitude
 *
 * WHAT: Adjust noise for specific frequency band
 * WHY:  Different bands may need different noise levels
 * HOW:  Update band-specific amplitude parameter
 *
 * @param bridge Oscillations-pink noise bridge
 * @param band Brain wave band to modify
 * @param amplitude New amplitude for this band [0-0.2]
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_set_band_amplitude(
    oscillations_pink_noise_bridge_t* bridge,
    brain_wave_band_t band,
    float amplitude
);

/**
 * @brief Set alpha exponent (spectral slope)
 *
 * WHAT: Adjust spectral slope of noise
 * WHY:  Explore different noise colors (white α=0, pink α=1, red α=2)
 * HOW:  Update alpha parameter, recreate generator
 *
 * @param bridge Oscillations-pink noise bridge
 * @param alpha New spectral exponent [0.5-2.0]
 * @return 0 on success, -1 on error
 *
 * NOTE: Requires recreating noise generator (expensive)
 */
int oscillations_pink_noise_set_alpha(
    oscillations_pink_noise_bridge_t* bridge,
    float alpha
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current injection parameters
 *
 * @param bridge Oscillations-pink noise bridge
 * @param params Output injection parameters
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_get_params(
    const oscillations_pink_noise_bridge_t* bridge,
    pink_noise_injection_params_t* params
);

/**
 * @brief Get latest noise sample value
 *
 * @param bridge Oscillations-pink noise bridge
 * @return Current noise sample, or 0.0 if disabled/error
 */
float oscillations_pink_noise_get_current_sample(
    const oscillations_pink_noise_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve pink noise injection statistics
 * WHY:  Monitor noise characteristics over time
 * HOW:  Return stats: total injections, avg amplitude, peak amplitude
 *
 * @param bridge Oscillations-pink noise bridge
 * @param total_injections Output: total noise injections (can be NULL)
 * @param avg_amplitude Output: average noise amplitude (can be NULL)
 * @param peak_amplitude Output: peak noise amplitude (can be NULL)
 * @return 0 on success, -1 on error
 */
int oscillations_pink_noise_get_stats(
    const oscillations_pink_noise_bridge_t* bridge,
    uint32_t* total_injections,
    float* avg_amplitude,
    float* peak_amplitude
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OSCILLATIONS_PINK_NOISE_BRIDGE_H */
