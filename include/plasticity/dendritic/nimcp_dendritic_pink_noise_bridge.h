/**
 * @file nimcp_dendritic_pink_noise_bridge.h
 * @brief Dendritic Integration - Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between dendritic integration and pink noise modulation
 * WHY:  Ion channel noise exhibits 1/f spectrum, adding biologically realistic stochastic
 *       variability to dendritic computation and NMDA dynamics. Essential for realistic
 *       dendritic spike generation and synaptic integration under uncertainty.
 * HOW:  Pink noise adds channel noise to dendritic voltage, modulates NMDA conductance,
 *       perturbs calcium dynamics, and varies synaptic efficacy with 1/f statistics.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ION CHANNEL NOISE (Faisal et al., 2008):
 * -----------------------------------------
 * 1. 1/f Spectrum in Ion Channels:
 *    - Voltage-gated channels exhibit pink noise in open/closed transitions
 *    - Membrane voltage fluctuations follow 1/f power spectrum
 *    - Channel stochasticity impacts dendritic spike threshold
 *    - Reference: Faisal et al. (2008) "Noise in the nervous system"
 *
 * 2. NMDA Receptor Stochasticity:
 *    - NMDA channel opening is stochastic with 1/f kinetics
 *    - Mg²⁺ block/unblock exhibits pink noise statistics
 *    - Glutamate binding variability follows power law
 *    - Reference: Wyllie et al. (1998) "Single-channel analysis of NMDA receptors"
 *
 * 3. Dendritic Voltage Fluctuations:
 *    - Membrane potential exhibits 1/f noise (Destexhe et al., 2003)
 *    - Synaptic background noise has pink spectrum
 *    - Impacts coincidence detection and spike timing
 *    - Reference: Destexhe et al. (2003) "Fluctuating synaptic conductances"
 *
 * 4. Calcium Channel Noise:
 *    - Ca²⁺ channels exhibit stochastic gating with 1/f statistics
 *    - Impacts dendritic spike generation
 *    - Affects plasticity induction threshold
 *    - Reference: Schneggenburger & Neher (2000) "Intracellular calcium dependence"
 *
 * DENDRITIC → PINK NOISE PATHWAYS:
 * ---------------------------------
 * 1. Activity-Dependent Noise Scaling:
 *    - High dendritic activity → reduced noise (signal-to-noise improvement)
 *    - Low activity → increased noise (exploration)
 *    - Dendritic spike rate modulates noise amplitude
 *
 * 2. NMDA Conductance → Noise Gating:
 *    - Strong NMDA activation → reduced channel noise variability
 *    - Weak NMDA → increased stochasticity
 *    - Coincidence detection threshold influences noise level
 *
 * 3. Calcium Level → Noise Modulation:
 *    - High [Ca²⁺] → reduced noise (stable state)
 *    - Low [Ca²⁺] → increased noise (exploratory state)
 *
 * PINK NOISE → DENDRITIC PATHWAYS:
 * ---------------------------------
 * 1. Voltage Noise:
 *    - Add 1/f noise to dendritic voltage (channel noise simulation)
 *    - Impacts spike threshold crossings
 *    - Affects coincidence detection window
 *
 * 2. NMDA Conductance Noise:
 *    - Modulate NMDA conductance with pink noise
 *    - Simulate stochastic channel gating
 *    - Vary Mg²⁺ block/unblock timing
 *
 * 3. Synaptic Efficacy Noise:
 *    - Multiplicative noise on synaptic weights
 *    - Simulate vesicle release variability
 *    - Quantal fluctuations in glutamate release
 *
 * 4. Calcium Influx Noise:
 *    - Add noise to Ca²⁺ dynamics
 *    - Simulate stochastic channel opening
 *    - Impact plasticity induction variability
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                DENDRITIC-PINK NOISE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  DENDRITIC → PINK NOISE                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DENDRITIC   │                                                 │  ║
 * ║   │   │   STATE      │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ Activity → 0.8│  ───────┐                                      │  ║
 * ║   │   │ NMDA → 0.6   │          │                                      │  ║
 * ║   │   │ [Ca²⁺] → 0.7 │          ├──→ Reduce Noise Amplitude            │  ║
 * ║   │   │              │          │    (High SNR, Stable State)          │  ║
 * ║   │   └──────────────┘          │                                      │  ║
 * ║   │                             ▼                                      │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     PINK NOISE GENERATOR        │                             │  ║
 * ║   │   │  - Amplitude scaling            │                             │  ║
 * ║   │   │  - Spectral slope modulation    │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PINK NOISE → DENDRITIC                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  1/f NOISE   │ ──→ Voltage Fluctuations (±2-5 mV)              │  ║
 * ║   │   │  GENERATOR   │ ──→ NMDA Conductance Noise (±10%)               │  ║
 * ║   │   │              │ ──→ Synaptic Weight Variability (±15%)          │  ║
 * ║   │   │              │ ──→ Calcium Influx Noise (±20%)                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                             ▼                                      │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     DENDRITIC SYSTEM            │                             │  ║
 * ║   │   │  - Noisy voltage integration    │                             │  ║
 * ║   │   │  - Stochastic spike generation  │                             │  ║
 * ║   │   │  - Variable NMDA dynamics       │                             │  ║
 * ║   │   │  - Calcium fluctuations         │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
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

#ifndef NIMCP_DENDRITIC_PINK_NOISE_BRIDGE_H
#define NIMCP_DENDRITIC_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/noise/nimcp_pink_noise.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Noise amplitude ranges (mV for voltage, fractional for modulation) */
#define DENDRITIC_PINK_NOISE_VOLTAGE_MIN        0.5f    /**< Min voltage noise (mV) */
#define DENDRITIC_PINK_NOISE_VOLTAGE_MAX        5.0f    /**< Max voltage noise (mV) */
#define DENDRITIC_PINK_NOISE_VOLTAGE_DEFAULT    2.0f    /**< Default voltage noise (mV) */

#define DENDRITIC_PINK_NOISE_NMDA_MIN           0.01f   /**< Min NMDA noise (fractional) */
#define DENDRITIC_PINK_NOISE_NMDA_MAX           0.20f   /**< Max NMDA noise (fractional) */
#define DENDRITIC_PINK_NOISE_NMDA_DEFAULT       0.10f   /**< Default NMDA noise (10%) */

#define DENDRITIC_PINK_NOISE_SYNAPSE_MIN        0.05f   /**< Min synaptic noise (fractional) */
#define DENDRITIC_PINK_NOISE_SYNAPSE_MAX        0.30f   /**< Max synaptic noise (fractional) */
#define DENDRITIC_PINK_NOISE_SYNAPSE_DEFAULT    0.15f   /**< Default synaptic noise (15%) */

#define DENDRITIC_PINK_NOISE_CALCIUM_MIN        0.05f   /**< Min calcium noise (fractional) */
#define DENDRITIC_PINK_NOISE_CALCIUM_MAX        0.40f   /**< Max calcium noise (fractional) */
#define DENDRITIC_PINK_NOISE_CALCIUM_DEFAULT    0.20f   /**< Default calcium noise (20%) */

/* Activity-dependent noise scaling factors */
#define DENDRITIC_ACTIVITY_NOISE_GAIN           -0.5f   /**< High activity reduces noise */
#define DENDRITIC_NMDA_NOISE_SUPPRESSION        0.7f    /**< Strong NMDA reduces noise */
#define DENDRITIC_CALCIUM_NOISE_SUPPRESSION     0.8f    /**< High Ca²⁺ reduces noise */

/* Spectral slope (alpha) modulation range */
#define DENDRITIC_PINK_NOISE_ALPHA_MIN          0.6f    /**< Min alpha (more white) */
#define DENDRITIC_PINK_NOISE_ALPHA_MAX          1.4f    /**< Max alpha (more red) */
#define DENDRITIC_PINK_NOISE_ALPHA_DEFAULT      1.0f    /**< Default alpha (pink) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Dendritic state → pink noise modulation effects
 *
 * WHAT: How dendritic activity modulates noise generation
 * WHY:  High activity/stable state should reduce noise for better SNR
 */
typedef struct {
    float activity_level;           /**< Overall dendritic activity [0-1] */
    float nmda_activation;          /**< NMDA receptor activation [0-1] */
    float calcium_level;            /**< Calcium concentration [0-1] */

    /* Computed noise scaling factors */
    float activity_noise_scale;     /**< Activity-dependent noise scaling [0-1] */
    float nmda_noise_suppression;   /**< NMDA-dependent noise reduction [0-1] */
    float calcium_noise_suppression; /**< Calcium-dependent noise reduction [0-1] */
    float combined_noise_scale;     /**< Combined scaling factor [0-1] */
} dendritic_noise_modulation_t;

/**
 * @brief Pink noise → dendritic effects
 *
 * WHAT: How pink noise perturbs dendritic computation
 * WHY:  Simulate ion channel stochasticity and synaptic variability
 */
typedef struct {
    /* Voltage noise */
    float voltage_noise_sample;     /**< Current voltage noise sample (mV) */
    float voltage_noise_amplitude;  /**< RMS voltage noise amplitude (mV) */

    /* NMDA conductance noise */
    float nmda_noise_sample;        /**< Current NMDA noise sample (fractional) */
    float nmda_noise_amplitude;     /**< RMS NMDA noise amplitude (fractional) */

    /* Synaptic efficacy noise */
    float synapse_noise_sample;     /**< Current synaptic noise sample (fractional) */
    float synapse_noise_amplitude;  /**< RMS synaptic noise amplitude (fractional) */

    /* Calcium influx noise */
    float calcium_noise_sample;     /**< Current calcium noise sample (fractional) */
    float calcium_noise_amplitude;  /**< RMS calcium noise amplitude (fractional) */

    /* Spectral properties */
    float effective_alpha;          /**< Current spectral slope */
    float effective_amplitude;      /**< Overall noise amplitude scaling */
} dendritic_pink_noise_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_voltage_noise;          /**< Add noise to dendritic voltage */
    bool enable_nmda_noise;             /**< Add noise to NMDA conductance */
    bool enable_synapse_noise;          /**< Add noise to synaptic weights */
    bool enable_calcium_noise;          /**< Add noise to calcium influx */
    bool enable_activity_modulation;    /**< Activity modulates noise amplitude */
    bool enable_nmda_modulation;        /**< NMDA activation modulates noise */
    bool enable_calcium_modulation;     /**< Calcium level modulates noise */

    /* Noise amplitudes */
    float voltage_noise_amplitude;      /**< Voltage noise RMS (mV) */
    float nmda_noise_amplitude;         /**< NMDA noise RMS (fractional) */
    float synapse_noise_amplitude;      /**< Synaptic noise RMS (fractional) */
    float calcium_noise_amplitude;      /**< Calcium noise RMS (fractional) */

    /* Spectral parameters */
    float pink_noise_alpha;             /**< Spectral slope (0=white, 1=pink, 2=red) */
    float pink_noise_sample_rate;       /**< Sampling rate (Hz) */
    float pink_noise_min_freq;          /**< Minimum frequency (Hz) */
    float pink_noise_max_freq;          /**< Maximum frequency (Hz) */

    /* Activity-dependent modulation gains */
    float activity_noise_gain;          /**< Activity → noise scaling gain */
    float nmda_suppression_factor;      /**< NMDA → noise suppression factor */
    float calcium_suppression_factor;   /**< Calcium → noise suppression factor */

    /* Random seed */
    uint32_t random_seed;               /**< Random seed (0 = use time) */
} dendritic_pink_noise_config_t;

/**
 * @brief Complete dendritic-pink noise bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    dendritic_tree_t dendritic_tree;
    pink_noise_generator_t noise_generator;

    /* Configuration */
    dendritic_pink_noise_config_t config;

    /* Current state */
    dendritic_noise_modulation_t dendritic_modulation;
    dendritic_pink_noise_effects_t noise_effects;

    /* Statistics */
    uint64_t total_updates;
    uint32_t voltage_noise_applications;
    uint32_t nmda_noise_applications;
    uint32_t synapse_noise_applications;
    uint32_t calcium_noise_applications;
    float avg_noise_amplitude;
    float avg_activity_scaling;

    } dendritic_pink_noise_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically realistic defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int dendritic_pink_noise_default_config(dendritic_pink_noise_config_t* config);

/**
 * @brief Create dendritic-pink noise bridge
 *
 * WHAT: Initialize bidirectional dendritic-noise integration
 * WHY:  Enable realistic ion channel noise simulation
 * HOW:  Allocate structure, create pink noise generator, link dendritic tree
 *
 * @param config Configuration (NULL for defaults)
 * @param dendritic_tree Dendritic tree system
 * @return New bridge or NULL on failure
 */
dendritic_pink_noise_bridge_t* dendritic_pink_noise_bridge_create(
    const dendritic_pink_noise_config_t* config,
    dendritic_tree_t dendritic_tree
);

/**
 * @brief Destroy dendritic-pink noise bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure and noise generator (doesn't destroy dendritic tree)
 *
 * @param bridge Bridge to destroy
 */
void dendritic_pink_noise_bridge_destroy(dendritic_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Dendritic → Pink Noise API
 * ============================================================================ */

/**
 * @brief Update dendritic activity modulation of noise
 *
 * WHAT: Compute how dendritic state modulates noise generation
 * WHY:  High activity/stable state reduces noise for better SNR
 * HOW:  Read dendritic tree state, compute scaling factors
 *
 * @param bridge Dendritic-pink noise bridge
 * @return 0 on success
 */
int dendritic_pink_noise_update_modulation(dendritic_pink_noise_bridge_t* bridge);

/**
 * @brief Compute activity-dependent noise scaling
 *
 * WHAT: Calculate noise amplitude reduction from dendritic activity
 * WHY:  Active dendrites have better signal-to-noise ratio
 * HOW:  Map activity level to noise scaling factor [0-1]
 *
 * @param bridge Dendritic-pink noise bridge
 * @return Noise scaling factor [0-1]
 */
float dendritic_pink_noise_compute_activity_scaling(
    const dendritic_pink_noise_bridge_t* bridge
);

/**
 * @brief Compute NMDA-dependent noise suppression
 *
 * WHAT: Calculate noise reduction from strong NMDA activation
 * WHY:  Strong NMDA locks in coincidence detection, reducing stochasticity
 * HOW:  Map NMDA activation to noise suppression factor [0-1]
 *
 * @param bridge Dendritic-pink noise bridge
 * @return Noise suppression factor [0-1]
 */
float dendritic_pink_noise_compute_nmda_suppression(
    const dendritic_pink_noise_bridge_t* bridge
);

/* ============================================================================
 * Pink Noise → Dendritic API
 * ============================================================================ */

/**
 * @brief Generate and apply pink noise to dendritic voltage
 *
 * WHAT: Add 1/f noise to dendritic compartment voltages
 * WHY:  Simulate ion channel noise in membrane voltage
 * HOW:  Generate noise sample, scale by activity, add to voltage
 *
 * @param bridge Dendritic-pink noise bridge
 * @param branch_id Branch ID
 * @param compartment_id Compartment ID within branch
 * @param voltage_noise_out Output voltage noise (mV)
 * @return 0 on success
 */
int dendritic_pink_noise_apply_voltage_noise(
    dendritic_pink_noise_bridge_t* bridge,
    uint32_t branch_id,
    uint32_t compartment_id,
    float* voltage_noise_out
);

/**
 * @brief Generate and apply pink noise to NMDA conductance
 *
 * WHAT: Add 1/f noise to NMDA receptor conductance
 * WHY:  Simulate stochastic channel gating and Mg²⁺ block variability
 * HOW:  Generate noise, apply multiplicatively to NMDA conductance
 *
 * @param bridge Dendritic-pink noise bridge
 * @param base_conductance Base NMDA conductance (nS)
 * @param noisy_conductance_out Output noisy conductance (nS)
 * @return 0 on success
 */
int dendritic_pink_noise_apply_nmda_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_conductance,
    float* noisy_conductance_out
);

/**
 * @brief Generate and apply pink noise to synaptic efficacy
 *
 * WHAT: Add 1/f noise to synaptic weights
 * WHY:  Simulate vesicle release variability and quantal fluctuations
 * HOW:  Generate noise, apply multiplicatively to synaptic weight
 *
 * @param bridge Dendritic-pink noise bridge
 * @param base_weight Base synaptic weight
 * @param noisy_weight_out Output noisy weight
 * @return 0 on success
 */
int dendritic_pink_noise_apply_synapse_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_weight,
    float* noisy_weight_out
);

/**
 * @brief Generate and apply pink noise to calcium influx
 *
 * WHAT: Add 1/f noise to calcium influx rate
 * WHY:  Simulate stochastic calcium channel opening
 * HOW:  Generate noise, apply multiplicatively to Ca²⁺ influx
 *
 * @param bridge Dendritic-pink noise bridge
 * @param base_influx Base calcium influx rate (μM/ms)
 * @param noisy_influx_out Output noisy influx rate (μM/ms)
 * @return 0 on success
 */
int dendritic_pink_noise_apply_calcium_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_influx,
    float* noisy_influx_out
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update dendritic-pink noise bridge (both directions)
 *
 * WHAT: Process all dendritic-noise interactions
 * WHY:  Advance coupled state machine
 * HOW:  Update modulation, generate noise, apply to dendritic system
 *
 * @param bridge Dendritic-pink noise bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int dendritic_pink_noise_bridge_update(
    dendritic_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current dendritic noise modulation state
 *
 * @param bridge Dendritic-pink noise bridge
 * @param modulation Output modulation structure
 * @return 0 on success
 */
int dendritic_pink_noise_get_modulation(
    const dendritic_pink_noise_bridge_t* bridge,
    dendritic_noise_modulation_t* modulation
);

/**
 * @brief Get current pink noise effects on dendritic system
 *
 * @param bridge Dendritic-pink noise bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int dendritic_pink_noise_get_effects(
    const dendritic_pink_noise_bridge_t* bridge,
    dendritic_pink_noise_effects_t* effects
);

/**
 * @brief Get current effective noise amplitude
 *
 * WHAT: Query overall noise amplitude after activity modulation
 * WHY:  Monitor noise scaling dynamics
 * HOW:  Return combined noise amplitude factor
 *
 * @param bridge Dendritic-pink noise bridge
 * @return Effective noise amplitude [0-1]
 */
float dendritic_pink_noise_get_effective_amplitude(
    const dendritic_pink_noise_bridge_t* bridge
);

/**
 * @brief Get current activity-dependent noise scaling
 *
 * @param bridge Dendritic-pink noise bridge
 * @return Activity scaling factor [0-1]
 */
float dendritic_pink_noise_get_activity_scaling(
    const dendritic_pink_noise_bridge_t* bridge
);

/**
 * @brief Enable/disable specific noise channels
 *
 * WHAT: Dynamically toggle noise application to different channels
 * WHY:  Experimental control and ablation studies
 * HOW:  Update configuration flags
 *
 * @param bridge Dendritic-pink noise bridge
 * @param enable_voltage Enable voltage noise
 * @param enable_nmda Enable NMDA noise
 * @param enable_synapse Enable synaptic noise
 * @param enable_calcium Enable calcium noise
 * @return 0 on success
 */
int dendritic_pink_noise_set_enables(
    dendritic_pink_noise_bridge_t* bridge,
    bool enable_voltage,
    bool enable_nmda,
    bool enable_synapse,
    bool enable_calcium
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed noise coordination
 * HOW:  Register with bio_router using BIO_MODULE_PINK_NOISE_DENDRITIC
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int dendritic_pink_noise_connect_bio_async(dendritic_pink_noise_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int dendritic_pink_noise_disconnect_bio_async(dendritic_pink_noise_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool dendritic_pink_noise_is_bio_async_connected(
    const dendritic_pink_noise_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITIC_PINK_NOISE_BRIDGE_H */
