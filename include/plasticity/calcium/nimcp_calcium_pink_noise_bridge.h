/**
 * @file nimcp_calcium_pink_noise_bridge.h
 * @brief Calcium Dynamics - Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between calcium dynamics and pink noise modulation
 * WHY:  Biological calcium transients exhibit 1/f noise due to stochastic channel
 *       openings, vesicle release variability, and multi-timescale calcium handling.
 *       Pink noise modulation creates realistic calcium fluctuations.
 * HOW:  Pink noise modulates calcium influx and transients; calcium concentration
 *       state modulates noise amplitude for dynamic variability.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CALCIUM VARIABILITY & 1/f NOISE (Thurley et al., 2012):
 * --------------------------------------------------------
 * 1. Stochastic Channel Openings:
 *    - NMDA receptors: stochastic gating with 1-10 ms timescales
 *    - VGCC (voltage-gated calcium channels): burst kinetics
 *    - IP3 receptors: puff dynamics with power-law statistics
 *    - Combined effect: 1/f spectrum in [Ca²⁺] fluctuations
 *    - Reference: Thurley et al. (2012) "Reliable encoding of stimulus
 *      intensities within random sequences of intracellular Ca²⁺ spikes"
 *
 * 2. Vesicle Release Variability:
 *    - Presynaptic release is stochastic
 *    - Postsynaptic calcium signals vary accordingly
 *    - Pink noise captures trial-to-trial variability
 *    - Reference: Faisal et al. (2008) "Noise in the nervous system"
 *
 * 3. Multi-Timescale Calcium Handling:
 *    - Fast buffering: sub-millisecond (calbindin, calmodulin)
 *    - Medium pumps: 10-100 ms (PMCA, NCX)
 *    - Slow stores: seconds (ER, mitochondria)
 *    - Natural 1/f spectrum from hierarchical timescales
 *    - Reference: Augustine et al. (2003) "Calcium signaling in neurons"
 *
 * 4. Calcium Microdomains:
 *    - Spatially heterogeneous [Ca²⁺]
 *    - Spine-dendrite variability
 *    - Pink noise approximates spatial fluctuations
 *    - Reference: Sabatini et al. (2002) "Analysis of calcium channels
 *      in single spines using optical fluctuation analysis"
 *
 * PINK NOISE EFFECTS ON CALCIUM:
 * -------------------------------
 * 1. Influx Modulation:
 *    - Modulate NMDA activation with pink noise
 *    - Creates realistic trial-to-trial variability
 *    - α = 1.0 matches measured fluctuations
 *
 * 2. Transient Amplitude Variability:
 *    - Calcium spike amplitude varies ±10-30%
 *    - Pink noise captures this variability
 *    - Amplitude modulation: 0.1-0.3 typical
 *
 * 3. Decay Time Variability:
 *    - Pump efficiency varies (fatigue, ATP)
 *    - Stochastic buffer binding
 *    - Pink noise modulates decay dynamics
 *
 * CALCIUM EFFECTS ON PINK NOISE:
 * -------------------------------
 * 1. Amplitude Scaling:
 *    - Low [Ca²⁺] → higher noise (more stochastic)
 *    - High [Ca²⁺] → lower noise (saturated channels)
 *    - Implements calcium-dependent noise modulation
 *
 * 2. State-Dependent Variability:
 *    - LTD regime: higher variability
 *    - LTP regime: lower variability (saturated)
 *    - Matches biological observations
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           CALCIUM DYNAMICS - PINK NOISE INTEGRATION BRIDGE                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              PINK NOISE → CALCIUM PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PINK NOISE   │                                                 │  ║
 * ║   │   │ GENERATOR    │                                                 │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   │ α = 1.0      │  ───────┐                                       │  ║
 * ║   │   │ amplitude    │         │                                       │  ║
 * ║   │   │ = 0.15       │         ├──→ NMDA Influx Modulation             │  ║
 * ║   │   └──────────────┘         │    (stochastic channel gating)        │  ║
 * ║   │                            │                                       │  ║
 * ║   │                            ├──→ Transient Amplitude Modulation     │  ║
 * ║   │                            │    (vesicle release variability)      │  ║
 * ║   │                            │                                       │  ║
 * ║   │                            └──→ Decay Time Modulation              │  ║
 * ║   │                                 (pump/buffer stochasticity)        │  ║
 * ║   │                                                                     │  ║
 * ║   │   MODULATION MODES:                                                │  ║
 * ║   │   ─────────────────────────────────────────                        │  ║
 * ║   │   INFLUX        → Modulate NMDA activation                         │  ║
 * ║   │   TRANSIENT     → Modulate Ca²⁺ spike amplitude                    │  ║
 * ║   │   DECAY         → Modulate pump/buffer rates                       │  ║
 * ║   │   COMPREHENSIVE → All of the above                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              CALCIUM → PINK NOISE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  [Ca²⁺]      │                                                 │  ║
 * ║   │   │  STATE       │                                                 │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   │ Low Ca²⁺     │ ──→ Increase noise amplitude (more stochastic) │  ║
 * ║   │   │ High Ca²⁺    │ ──→ Decrease noise amplitude (saturated)       │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   │ LTD regime   │ ──→ Higher variability                          │  ║
 * ║   │   │ LTP regime   │ ──→ Lower variability                           │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   AMPLITUDE SCALING:                                               │  ║
 * ║   │   amplitude = base_amplitude × (1.5 - 0.5 × normalized_ca)        │  ║
 * ║   │   where normalized_ca = [Ca²⁺] / [Ca²⁺]_max                       │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * TYPICAL PARAMETERS:
 * -------------------
 * - Pink noise α: 0.8-1.2 (1.0 = true pink)
 * - Base amplitude: 0.10-0.20 (10-20% modulation)
 * - Influx modulation strength: 0.15 (±15% NMDA variability)
 * - Transient modulation strength: 0.20 (±20% amplitude variability)
 * - Decay modulation strength: 0.10 (±10% decay variability)
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

#ifndef NIMCP_CALCIUM_PINK_NOISE_BRIDGE_H
#define NIMCP_CALCIUM_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/noise/nimcp_pink_noise.h"

/* Core utilities */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default pink noise parameters for calcium */
#define CALCIUM_PINK_NOISE_ALPHA_DEFAULT         1.0f    /**< True pink noise */
#define CALCIUM_PINK_NOISE_AMPLITUDE_DEFAULT     0.15f   /**< 15% modulation */
#define CALCIUM_PINK_NOISE_MIN_FREQ_DEFAULT      0.1f    /**< 0.1 Hz (10s timescale) */
#define CALCIUM_PINK_NOISE_MAX_FREQ_DEFAULT      100.0f  /**< 100 Hz (10ms timescale) */
#define CALCIUM_PINK_NOISE_SAMPLE_RATE_DEFAULT   1000.0f /**< 1 kHz sampling */

/* Modulation strength defaults */
#define CALCIUM_PINK_NOISE_INFLUX_STRENGTH       0.15f   /**< ±15% NMDA variability */
#define CALCIUM_PINK_NOISE_TRANSIENT_STRENGTH    0.20f   /**< ±20% amplitude variability */
#define CALCIUM_PINK_NOISE_DECAY_STRENGTH        0.10f   /**< ±10% decay variability */

/* Calcium-dependent noise scaling */
#define CALCIUM_PINK_NOISE_LOW_CA_SCALE          1.5f    /**< Scale at low [Ca²⁺] */
#define CALCIUM_PINK_NOISE_HIGH_CA_SCALE         0.5f    /**< Scale at high [Ca²⁺] */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Pink noise modulation mode for calcium
 *
 * WHAT: Which aspects of calcium dynamics to modulate with pink noise
 * WHY:  Flexibility to target specific variability sources
 * HOW:  Enum for modulation targets
 */
typedef enum {
    CALCIUM_PINK_NOISE_MODE_INFLUX,         /**< Modulate NMDA influx only */
    CALCIUM_PINK_NOISE_MODE_TRANSIENT,      /**< Modulate Ca²⁺ amplitude only */
    CALCIUM_PINK_NOISE_MODE_DECAY,          /**< Modulate decay/pumps only */
    CALCIUM_PINK_NOISE_MODE_COMPREHENSIVE   /**< Modulate all aspects */
} calcium_pink_noise_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for calcium-pink noise integration
 *
 * WHAT: Parameters controlling noise modulation of calcium dynamics
 * WHY:  Customizable for different neuron types and experiments
 * HOW:  All pink noise and modulation parameters
 */
typedef struct {
    /* Pink noise parameters */
    float pink_noise_alpha;              /**< Spectral exponent (1.0 = pink) */
    float pink_noise_amplitude;          /**< Base amplitude [0.05-0.30] */
    float pink_noise_min_freq;           /**< Minimum frequency (Hz) */
    float pink_noise_max_freq;           /**< Maximum frequency (Hz) */
    float pink_noise_sample_rate;        /**< Sampling rate (Hz) */
    uint32_t pink_noise_seed;            /**< Random seed (0 = time-based) */

    /* Modulation configuration */
    calcium_pink_noise_mode_t mode;      /**< Modulation mode */
    float influx_modulation_strength;    /**< NMDA influx modulation [0-0.5] */
    float transient_modulation_strength; /**< Amplitude modulation [0-0.5] */
    float decay_modulation_strength;     /**< Decay modulation [0-0.5] */

    /* Calcium-dependent noise scaling */
    bool enable_ca_dependent_amplitude;  /**< Scale noise with [Ca²⁺] */
    float ca_low_scale;                  /**< Amplitude scale at low [Ca²⁺] */
    float ca_high_scale;                 /**< Amplitude scale at high [Ca²⁺] */

    /* Integration enables */
    bool enable_noise_modulation;        /**< Master enable for noise */
} calcium_pink_noise_config_t;

/**
 * @brief Pink noise effects on calcium dynamics
 *
 * WHAT: Current noise modulation factors
 * WHY:  Track how pink noise affects calcium parameters
 * HOW:  Computed modulation values
 */
typedef struct {
    /* Current noise sample */
    float current_noise_sample;          /**< Latest pink noise value */

    /* Modulation factors (multipliers) */
    float influx_modulation;             /**< NMDA influx multiplier [0.5-1.5] */
    float transient_modulation;          /**< Amplitude multiplier [0.5-1.5] */
    float decay_modulation;              /**< Decay rate multiplier [0.5-1.5] */

    /* Calcium-scaled amplitude */
    float effective_amplitude;           /**< Amplitude after Ca²⁺ scaling */
    float ca_concentration_normalized;   /**< [Ca²⁺] normalized to [0,1] */
} calcium_pink_noise_effects_t;

/**
 * @brief Statistics for calcium-pink noise integration
 */
typedef struct {
    uint64_t total_updates;              /**< Total update steps */
    uint32_t noise_samples_generated;    /**< Noise samples generated */

    /* Noise statistics */
    float avg_noise_amplitude;           /**< Average noise amplitude */
    float max_noise_amplitude;           /**< Maximum noise amplitude */
    float min_noise_amplitude;           /**< Minimum noise amplitude */

    /* Modulation statistics */
    float avg_influx_modulation;         /**< Average influx modulation */
    float avg_transient_modulation;      /**< Average transient modulation */
    float avg_decay_modulation;          /**< Average decay modulation */
} calcium_pink_noise_stats_t;

/**
 * @brief Complete calcium-pink noise bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    calcium_pink_noise_config_t config;

    /* System handles */
    calcium_dynamics_t calcium;          /**< Calcium dynamics system */
    pink_noise_generator_t noise_gen;    /**< Pink noise generator */

    /* Current state */
    calcium_pink_noise_effects_t effects;
    calcium_pink_noise_stats_t stats;

    /* Integration state */
    bool noise_enabled;                  /**< Noise currently active */
    bool calcium_connected;              /**< Calcium system connected */

    } calcium_pink_noise_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_default_config(calcium_pink_noise_config_t* config);

/**
 * @brief Create calcium-pink noise bridge
 *
 * WHAT: Initialize bidirectional calcium-pink noise integration
 * WHY:  Enable realistic stochastic calcium fluctuations
 * HOW:  Allocate structure, create pink noise generator
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
calcium_pink_noise_bridge_t* calcium_pink_noise_bridge_create(
    const calcium_pink_noise_config_t* config
);

/**
 * @brief Destroy calcium-pink noise bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Destroy noise generator, free structure
 *
 * @param bridge Bridge to destroy
 */
void calcium_pink_noise_bridge_destroy(calcium_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect calcium dynamics system to bridge
 *
 * WHAT: Link calcium system for noise modulation
 * WHY:  Bridge needs calcium system to modulate
 * HOW:  Store calcium pointer, validate connection
 *
 * @param bridge Calcium-pink noise bridge
 * @param calcium Calcium dynamics system
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_connect_calcium(
    calcium_pink_noise_bridge_t* bridge,
    calcium_dynamics_t calcium
);

/**
 * @brief Disconnect calcium system
 *
 * WHAT: Unlink calcium system from bridge
 * WHY:  Clean disconnection without destroying bridge
 * HOW:  Clear calcium pointer, disable modulation
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success
 */
int calcium_pink_noise_disconnect_calcium(calcium_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Enable/Disable API
 * ============================================================================ */

/**
 * @brief Enable pink noise modulation
 *
 * WHAT: Activate noise modulation of calcium dynamics
 * WHY:  Allow runtime enable/disable of stochasticity
 * HOW:  Set noise_enabled flag
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_enable(calcium_pink_noise_bridge_t* bridge);

/**
 * @brief Disable pink noise modulation
 *
 * WHAT: Deactivate noise modulation (deterministic calcium)
 * WHY:  Allow runtime enable/disable of stochasticity
 * HOW:  Clear noise_enabled flag, reset modulation to 1.0
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_disable(calcium_pink_noise_bridge_t* bridge);

/**
 * @brief Check if noise modulation is enabled
 *
 * @param bridge Calcium-pink noise bridge
 * @return true if enabled
 */
bool calcium_pink_noise_is_enabled(const calcium_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Update API (Pink Noise → Calcium)
 * ============================================================================ */

/**
 * @brief Update pink noise modulation of calcium
 *
 * WHAT: Generate new noise sample and apply to calcium dynamics
 * WHY:  Advance stochastic calcium modulation
 * HOW:  Generate pink noise, compute modulation factors, apply to calcium
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_update(calcium_pink_noise_bridge_t* bridge);

/**
 * @brief Apply pink noise modulation to calcium influx
 *
 * WHAT: Modulate NMDA activation with pink noise
 * WHY:  Simulate stochastic channel gating
 * HOW:  Multiply NMDA activation by (1 + strength × noise)
 *
 * @param bridge Calcium-pink noise bridge
 * @param nmda_activation Input NMDA activation [0-1]
 * @return Modulated NMDA activation
 */
float calcium_pink_noise_modulate_influx(
    const calcium_pink_noise_bridge_t* bridge,
    float nmda_activation
);

/**
 * @brief Apply pink noise modulation to calcium transient
 *
 * WHAT: Modulate calcium spike amplitude with pink noise
 * WHY:  Simulate vesicle release variability
 * HOW:  Multiply amplitude by (1 + strength × noise)
 *
 * @param bridge Calcium-pink noise bridge
 * @param ca_amplitude Input calcium amplitude
 * @return Modulated calcium amplitude
 */
float calcium_pink_noise_modulate_transient(
    const calcium_pink_noise_bridge_t* bridge,
    float ca_amplitude
);

/**
 * @brief Apply pink noise modulation to calcium decay
 *
 * WHAT: Modulate pump/buffer rates with pink noise
 * WHY:  Simulate stochastic calcium clearance
 * HOW:  Multiply decay rate by (1 + strength × noise)
 *
 * @param bridge Calcium-pink noise bridge
 * @param decay_rate Input decay rate
 * @return Modulated decay rate
 */
float calcium_pink_noise_modulate_decay(
    const calcium_pink_noise_bridge_t* bridge,
    float decay_rate
);

/* ============================================================================
 * Calcium → Pink Noise Feedback API
 * ============================================================================ */

/**
 * @brief Update noise amplitude based on calcium concentration
 *
 * WHAT: Scale noise amplitude with [Ca²⁺] level
 * WHY:  Low calcium → more stochastic; high calcium → less stochastic
 * HOW:  Interpolate between ca_low_scale and ca_high_scale
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_update_ca_dependent_amplitude(
    calcium_pink_noise_bridge_t* bridge
);

/**
 * @brief Get effective noise amplitude (after Ca²⁺ scaling)
 *
 * @param bridge Calcium-pink noise bridge
 * @return Effective amplitude
 */
float calcium_pink_noise_get_effective_amplitude(
    const calcium_pink_noise_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current pink noise effects
 *
 * @param bridge Calcium-pink noise bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_get_effects(
    const calcium_pink_noise_bridge_t* bridge,
    calcium_pink_noise_effects_t* effects
);

/**
 * @brief Get current statistics
 *
 * @param bridge Calcium-pink noise bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_get_stats(
    const calcium_pink_noise_bridge_t* bridge,
    calcium_pink_noise_stats_t* stats
);

/**
 * @brief Get current noise sample value
 *
 * @param bridge Calcium-pink noise bridge
 * @return Current noise sample
 */
float calcium_pink_noise_get_current_sample(
    const calcium_pink_noise_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register as bio-async module for noise events
 * WHY:  Enable inter-module messaging for noise modulation
 * HOW:  Register with bio_router using BIO_MODULE_CALCIUM_PINK_NOISE
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_connect_bio_async(calcium_pink_noise_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Calcium-pink noise bridge
 * @return 0 on success
 */
int calcium_pink_noise_disconnect_bio_async(calcium_pink_noise_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Calcium-pink noise bridge
 * @return true if connected
 */
bool calcium_pink_noise_is_bio_async_connected(
    const calcium_pink_noise_bridge_t* bridge
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Reset pink noise generator with new seed
 *
 * WHAT: Restart noise sequence with new or same seed
 * WHY:  Reproducibility or new trial initialization
 * HOW:  Reset noise generator state
 *
 * @param bridge Calcium-pink noise bridge
 * @param new_seed New random seed (0 = use configured seed)
 * @return 0 on success, -1 on error
 */
int calcium_pink_noise_reset(
    calcium_pink_noise_bridge_t* bridge,
    uint32_t new_seed
);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Check if config parameters are valid
 * WHY:  Prevent invalid noise generation
 * HOW:  Range checks on all parameters
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int calcium_pink_noise_validate_config(const calcium_pink_noise_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CALCIUM_PINK_NOISE_BRIDGE_H */
