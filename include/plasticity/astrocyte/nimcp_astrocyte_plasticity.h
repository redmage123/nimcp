/**
 * @file nimcp_astrocyte_plasticity.h
 * @brief Astrocyte-Mediated Synaptic Plasticity Integration (Tripartite Synapse)
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Astrocyte-neuron interactions affecting synaptic plasticity and transmission
 * WHY:  Astrocytes are active partners in synaptic function, not passive support cells.
 *       They modulate NMDA receptor activation, clear glutamate, release gliotransmitters,
 *       and coordinate network activity through calcium waves.
 * HOW:  Model tripartite synapse (pre/post neuron + astrocyte) with D-serine release,
 *       glutamate uptake, ATP/adenosine signaling, and astrocyte calcium dynamics.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * TRIPARTITE SYNAPSE MODEL:
 * ------------------------
 * Classical synapse: Presynaptic neuron ↔ Postsynaptic neuron
 * Tripartite synapse: Presynaptic neuron ↔ Postsynaptic neuron ↔ Astrocyte
 *
 * Astrocytes actively participate in synaptic transmission and plasticity through:
 * 1. Sensing neuronal activity (glutamate, ATP)
 * 2. Responding with calcium waves
 * 3. Releasing gliotransmitters (D-serine, glutamate, ATP, adenosine)
 * 4. Modulating synaptic strength and timing
 *
 * Reference: Araque et al. (1999) "Tripartite synapses: glia, the unacknowledged partner"
 *            Perea et al. (2009) "Tripartite synapses: astrocytes process and control"
 *
 * KEY ASTROCYTE FUNCTIONS IN PLASTICITY:
 * ---------------------------------------
 *
 * 1. D-SERINE AS NMDA CO-AGONIST:
 *    - NMDA receptors require both glutamate AND glycine-site co-agonist
 *    - In cortex/hippocampus, astrocyte-derived D-serine is primary co-agonist
 *    - Low D-serine → reduced NMDA activation → impaired LTP
 *    - D-serine depletion reduces LTP by 40-60%
 *    - D-serine modulates STDP timing window width
 *    Reference: Henneberger et al. (2010) "Long-term potentiation depends on release of D-serine"
 *               Papouin et al. (2012) "Synaptic and extrasynaptic NMDA receptors"
 *
 * 2. GLUTAMATE CLEARANCE:
 *    - Astrocytes express GLT-1 (EAAT2) and GLAST (EAAT1) transporters
 *    - Uptake ~90% of synaptic glutamate within 1-10 ms
 *    - Prevents spillover and excitotoxicity
 *    - Shapes synaptic transmission temporal precision
 *    - Impaired clearance → prolonged EPSC, increased LTD
 *    Reference: Tzingounis & Wadiche (2007) "Glutamate transporters"
 *               Valtcheva & Venance (2019) "Astrocytes gate Hebbian synaptic plasticity"
 *
 * 3. ATP/ADENOSINE SIGNALING:
 *    - Astrocytes release ATP in response to neuronal activity
 *    - ATP → adenosine via ectonucleotidases
 *    - Adenosine A1 receptor activation:
 *      * Suppresses synaptic transmission (presynaptic inhibition)
 *      * Modulates metaplasticity
 *      * Involved in sleep homeostasis
 *    - ATP P2Y receptor activation:
 *      * Propagates calcium waves between astrocytes
 *      * Synchronizes neuronal activity
 *    Reference: Halassa et al. (2009) "Astrocytic modulation of sleep homeostasis"
 *               Pascual et al. (2005) "Astrocytic purinergic signaling"
 *
 * 4. ASTROCYTE CALCIUM WAVES:
 *    - Astrocytes exhibit calcium oscillations and waves
 *    - Triggered by neuronal activity, metabotropic glutamate receptors
 *    - Propagate via gap junctions (connexins) and ATP release
 *    - Coordinate gliotransmitter release across synapses
 *    - Enable network-wide modulation (>100 μm spatial scale)
 *    - Different frequency bands for local vs. global signaling
 *    Reference: Cornell-Bell et al. (1990) "Glutamate induces calcium waves"
 *               Agulhon et al. (2008) "Calcium signaling in astrocytes"
 *
 * 5. SLEEP-DEPENDENT EFFECTS:
 *    - NREM sleep: Increased gliotransmitter release for consolidation
 *    - Noradrenaline in wake suppresses astrocyte calcium signaling
 *    - Sleep promotes glymphatic clearance (astrocyte volume changes)
 *    - Astrocyte contribution to slow waves and UP/DOWN states
 *    Reference: Ding et al. (2016) "Changes in the astrocyte-neuron ratio"
 *               Poskanzer & Yuste (2016) "Astrocytes regulate cortical state"
 *
 * 6. REACTIVE ASTROGLIOSIS (IMMUNE INTEGRATION):
 *    - Cytokines (IL-1β, TNF-α, IL-6) activate astrocytes
 *    - A1 reactive astrocytes: neurotoxic, reduced D-serine release
 *    - A2 reactive astrocytes: neuroprotective, support synaptogenesis
 *    - Chronic inflammation → persistent A1 state → synaptic loss
 *    - Impaired glutamate uptake → excitotoxicity risk
 *    Reference: Liddelow et al. (2017) "Neurotoxic reactive astrocytes"
 *               Vezzani et al. (2011) "Astrocyte activation in epilepsy"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           ASTROCYTE-PLASTICITY INTEGRATION (TRIPARTITE SYNAPSE)            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ASTROCYTE STATE                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   • D-serine level (0-1): NMDA co-agonist availability             │  ║
 * ║   │   • Glutamate uptake rate (0-1): Clearance efficiency              │  ║
 * ║   │   • ATP release level (0-1): Purinergic signaling                  │  ║
 * ║   │   • Calcium wave activity (0-1): Network coordination               │  ║
 * ║   │   • Reactive state (RESTING/A1/A2): Inflammation response           │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ASTROCYTE → PLASTICITY PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   D-SERINE ────────────▶ NMDA Co-activation                        │  ║
 * ║   │   (0.5-1.0)              • Full LTP: 1.0x                           │  ║
 * ║   │                          • Reduced LTP at 0.5: 0.6x                 │  ║
 * ║   │                          • STDP window narrowing                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   GLUTAMATE UPTAKE ─────▶ Synaptic Transmission                    │  ║
 * ║   │   (0.7-1.0 normal)       • Fast uptake: precise timing             │  ║
 * ║   │                          • Slow uptake: spillover, ↑LTD            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ATP/ADENOSINE ────────▶ Metaplasticity                           │  ║
 * ║   │   (0-0.5)                • A1R activation: ↓transmission           │  ║
 * ║   │                          • Sleep homeostasis                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   CALCIUM WAVES ────────▶ Network Synchronization                  │  ║
 * ║   │   (freq 0.01-1 Hz)       • Coordinate gliotransmitter release      │  ║
 * ║   │                          • Propagate 100-500 μm                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              PLASTICITY → ASTROCYTE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   High synaptic activity ─────▶ Astrocyte calcium ↑                │  ║
 * ║   │   Glutamate spillover ────────▶ mGluR activation                   │  ║
 * ║   │   LTP induction ──────────────▶ Trigger gliotransmitter release    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              SLEEP INTEGRATION                                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   AWAKE:       Low astrocyte Ca²⁺ (noradrenaline suppression)      │  ║
 * ║   │   DROWSY:      Increasing gliotransmitter tone                     │  ║
 * ║   │   NREM:        Peak D-serine, enhanced consolidation               │  ║
 * ║   │   REM:         Reduced astrocyte activity                          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE INTEGRATION (REACTIVE ASTROGLIOSIS)             │  ║
 * ║   │                                                                     │  ║
 * ║   │   IL-1β/TNF-α/IL-6 ───▶ A1 Reactive State                          │  ║
 * ║   │     • ↓D-serine release → impaired NMDA → ↓LTP                     │  ║
 * ║   │     • ↓Glutamate uptake → excitotoxicity risk                      │  ║
 * ║   │     • Neurotoxic factor release                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   IL-10 ──────────────▶ A2 Neuroprotective State                   │  ║
 * ║   │     • Support synaptogenesis                                       │  ║
 * ║   │     • Restore normal function                                      │  ║
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
 * - NIMCP_LOGGING_* for logging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTE_PLASTICITY_H
#define NIMCP_ASTROCYTE_PLASTICITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Astrocyte Modulation Parameters
 * ============================================================================ */

/* D-serine NMDA modulation */
#define ASTROCYTE_D_SERINE_BASELINE        0.8f   /**< Normal D-serine level */
#define ASTROCYTE_D_SERINE_NREM_BOOST      1.2f   /**< NREM sleep enhancement */
#define ASTROCYTE_D_SERINE_A1_REDUCTION    0.4f   /**< A1 reactive reduction */
#define ASTROCYTE_D_SERINE_LTP_THRESHOLD   0.5f   /**< Below this, LTP impaired */

/* Glutamate uptake rates */
#define ASTROCYTE_GLU_UPTAKE_BASELINE      0.9f   /**< Normal uptake (90% in 1-10ms) */
#define ASTROCYTE_GLU_UPTAKE_A1_IMPAIRED   0.5f   /**< A1 reactive state impairment */
#define ASTROCYTE_GLU_UPTAKE_FAST          0.95f  /**< Optimal uptake */
#define ASTROCYTE_GLU_UPTAKE_SLOW          0.6f   /**< Pathological slow uptake */

/* ATP/Adenosine signaling */
#define ASTROCYTE_ATP_BASELINE             0.2f   /**< Baseline ATP release */
#define ASTROCYTE_ATP_HIGH_ACTIVITY        0.6f   /**< During high synaptic activity */
#define ASTROCYTE_ADENOSINE_A1R_THRESHOLD  0.3f   /**< A1R activation threshold */

/* Calcium wave parameters */
#define ASTROCYTE_CA_WAVE_FREQ_LOW         0.01f  /**< Hz - slow global waves */
#define ASTROCYTE_CA_WAVE_FREQ_HIGH        1.0f   /**< Hz - fast local waves */
#define ASTROCYTE_CA_WAVE_PROPAGATION      200.0f /**< μm - typical propagation distance */

/* Timing constants */
#define ASTROCYTE_GLU_UPTAKE_TIME_MS       5.0f   /**< Average glutamate uptake time */
#define ASTROCYTE_D_SERINE_RELEASE_MS      50.0f  /**< D-serine release time constant */
#define ASTROCYTE_CA_WAVE_VELOCITY         20.0f  /**< μm/s - calcium wave propagation */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Astrocyte reactive states
 *
 * BIOLOGICAL BASIS:
 * Astrocytes can adopt different functional states in response to injury/inflammation
 * Reference: Liddelow et al. (2017) "Neurotoxic reactive astrocytes"
 */
typedef enum {
    ASTROCYTE_RESTING = 0,    /**< Normal homeostatic function */
    ASTROCYTE_A1_REACTIVE,    /**< Neurotoxic state (pro-inflammatory) */
    ASTROCYTE_A2_REACTIVE,    /**< Neuroprotective state (anti-inflammatory) */
    ASTROCYTE_MIXED_REACTIVE  /**< Mixed A1/A2 phenotype */
} astrocyte_reactive_state_t;

/**
 * @brief Gliotransmitter types
 */
typedef enum {
    GLIOTRANSMITTER_D_SERINE = 0,  /**< NMDA co-agonist */
    GLIOTRANSMITTER_GLUTAMATE,     /**< Excitatory neurotransmitter */
    GLIOTRANSMITTER_ATP,           /**< Purinergic signaling */
    GLIOTRANSMITTER_ADENOSINE,     /**< Neuromodulator */
    GLIOTRANSMITTER_COUNT
} gliotransmitter_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Astrocyte state tracking
 */
typedef struct {
    /* Gliotransmitter levels (0-1 normalized) */
    float d_serine_level;           /**< D-serine availability */
    float glutamate_uptake_rate;    /**< Glutamate clearance efficiency */
    float atp_release_level;        /**< ATP signaling strength */
    float adenosine_level;          /**< Adenosine concentration */

    /* Calcium dynamics */
    float calcium_baseline;         /**< Baseline [Ca²⁺] */
    float calcium_current;          /**< Current [Ca²⁺] */
    float calcium_wave_frequency;   /**< Hz - wave oscillation frequency */
    float calcium_wave_amplitude;   /**< Amplitude of waves */
    bool calcium_wave_active;       /**< Whether wave is propagating */

    /* Reactive state */
    astrocyte_reactive_state_t reactive_state;
    float a1_factor;                /**< A1 phenotype strength (0-1) */
    float a2_factor;                /**< A2 phenotype strength (0-1) */

    /* Spatial coverage */
    uint32_t num_synapses_covered;  /**< Number of synapses in astrocyte domain */
    float coverage_radius_um;       /**< Spatial coverage radius */

    /* Temporal state */
    uint64_t last_update_ms;        /**< Last state update timestamp */
    float delta_time_s;             /**< Time since last update */
} astrocyte_state_t;

/**
 * @brief Plasticity modulation effects from astrocyte
 */
typedef struct {
    /* NMDA receptor modulation */
    float nmda_coagonist_factor;    /**< D-serine effect on NMDA (0-1.5) */
    float ltp_capacity_modulation;  /**< LTP magnitude scaling */
    float stdp_window_modulation;   /**< STDP timing window scaling */

    /* Synaptic transmission modulation */
    float glutamate_clearance_time; /**< Time constant for Glu clearance (ms) */
    float spillover_factor;         /**< Spillover to neighbors (0-1) */
    float epsc_duration_factor;     /**< EPSC prolongation factor */

    /* Metaplasticity modulation */
    float a1r_inhibition;           /**< A1R-mediated inhibition (0-1) */
    float transmission_suppression; /**< Overall transmission reduction */
    float plasticity_threshold_shift; /**< BCM-like threshold modulation */

    /* Network coordination */
    float synchronization_factor;   /**< Calcium wave-mediated sync (0-1) */
    float spatial_correlation;      /**< Spatial correlation of modulation */
} astrocyte_plasticity_effects_t;

/**
 * @brief Astrocyte configuration
 */
typedef struct {
    /* Baseline parameters */
    float baseline_d_serine;
    float baseline_glu_uptake;
    float baseline_atp_release;
    float baseline_calcium;

    /* Calcium wave parameters */
    float ca_wave_freq_min;
    float ca_wave_freq_max;
    float ca_wave_propagation_velocity; /**< μm/s */
    float ca_wave_trigger_threshold;    /**< Activity threshold for wave */

    /* Release kinetics */
    float d_serine_release_time_ms;
    float glu_uptake_time_ms;
    float atp_release_time_ms;

    /* Spatial parameters */
    float coverage_radius_um;
    uint32_t max_synapses_per_astrocyte;

    /* Feature enables */
    bool enable_d_serine_modulation;
    bool enable_glutamate_uptake;
    bool enable_atp_signaling;
    bool enable_calcium_waves;
    bool enable_reactive_astrogliosis;

    /* Callback for gliotransmitter release events */
    void (*gliotransmitter_callback)(gliotransmitter_type_t type,
                                      float amount,
                                      void* user_data);
    void* callback_user_data;
} astrocyte_config_t;

/**
 * @brief Astrocyte plasticity system
 */
typedef struct astrocyte_plasticity_struct* astrocyte_plasticity_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default astrocyte configuration
 *
 * WHAT: Provide biologically-realistic default parameters
 * WHY:  Easy initialization with evidence-based values
 * HOW:  Return struct with values from literature
 *
 * @param config Output configuration struct
 * @return 0 on success, -1 on error
 */
int astrocyte_plasticity_default_config(astrocyte_config_t* config);

/**
 * @brief Create astrocyte plasticity system
 *
 * WHAT: Initialize astrocyte-synapse integration
 * WHY:  Enable tripartite synapse modeling
 * HOW:  Allocate structure, initialize state, create mutex
 *
 * @param config Configuration (NULL for defaults)
 * @param num_astrocytes Number of astrocyte domains to simulate
 * @return New system or NULL on failure
 */
astrocyte_plasticity_t astrocyte_plasticity_create(
    const astrocyte_config_t* config,
    uint32_t num_astrocytes
);

/**
 * @brief Destroy astrocyte plasticity system
 *
 * WHAT: Clean up resources
 * WHY:  Proper deallocation
 * HOW:  Free all structures and mutex
 *
 * @param astro System to destroy
 */
void astrocyte_plasticity_destroy(astrocyte_plasticity_t astro);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update astrocyte state based on synaptic activity
 *
 * WHAT: Advance astrocyte dynamics one timestep
 * WHY:  Astrocyte state evolves based on neuronal activity
 * HOW:  Update calcium, gliotransmitter levels, calcium waves
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Which astrocyte to update
 * @param synaptic_activity Synaptic activity level (0-1)
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int astrocyte_plasticity_update(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    float synaptic_activity,
    uint64_t delta_ms
);

/**
 * @brief Trigger calcium wave in astrocyte
 *
 * WHAT: Initiate calcium wave propagation
 * WHY:  High neuronal activity triggers astrocyte calcium waves
 * HOW:  Set calcium wave state, will propagate to neighbors
 *
 * @param astro Astrocyte system
 * @param source_id Source astrocyte ID
 * @param amplitude Wave amplitude (0-1)
 * @return 0 on success
 */
int astrocyte_plasticity_trigger_calcium_wave(
    astrocyte_plasticity_t astro,
    uint32_t source_id,
    float amplitude
);

/**
 * @brief Release gliotransmitter from astrocyte
 *
 * WHAT: Trigger gliotransmitter release event
 * WHY:  Manual control of astrocyte signaling (e.g., from calcium event)
 * HOW:  Update gliotransmitter level, invoke callback if registered
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @param type Gliotransmitter type
 * @param amount Amount to release (0-1)
 * @return 0 on success
 */
int astrocyte_plasticity_release_gliotransmitter(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    gliotransmitter_type_t type,
    float amount
);

/* ============================================================================
 * Astrocyte → Plasticity API
 * ============================================================================ */

/**
 * @brief Compute plasticity modulation effects from astrocyte state
 *
 * WHAT: Calculate how astrocyte state affects synaptic plasticity
 * WHY:  D-serine, glutamate uptake, ATP all modulate learning
 * HOW:  Map astrocyte state to plasticity parameter scaling factors
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Which astrocyte
 * @param effects Output effects structure
 * @return 0 on success
 */
int astrocyte_plasticity_get_effects(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_plasticity_effects_t* effects
);

/**
 * @brief Get D-serine modulation factor for NMDA receptor
 *
 * WHAT: Calculate NMDA co-agonist availability
 * WHY:  D-serine is required for full NMDA activation
 * HOW:  Return scaling factor based on D-serine level
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return D-serine factor (0-1.5)
 */
float astrocyte_plasticity_get_d_serine_factor(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/**
 * @brief Get glutamate clearance time constant
 *
 * WHAT: Return glutamate uptake time constant
 * WHY:  Affects synaptic transmission temporal precision
 * HOW:  Map uptake rate to time constant (ms)
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return Clearance time in ms
 */
float astrocyte_plasticity_get_glu_clearance_time(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/**
 * @brief Get A1 receptor inhibition level
 *
 * WHAT: Calculate adenosine A1R-mediated transmission suppression
 * WHY:  ATP/adenosine modulates metaplasticity
 * HOW:  Convert adenosine level to inhibition factor
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return A1R inhibition (0-1)
 */
float astrocyte_plasticity_get_a1r_inhibition(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/* ============================================================================
 * Plasticity → Astrocyte API
 * ============================================================================ */

/**
 * @brief Notify astrocyte of synaptic glutamate release
 *
 * WHAT: Signal astrocyte that glutamate was released at synapse
 * WHY:  Glutamate activates astrocyte mGluRs, triggers calcium
 * HOW:  Increase calcium based on glutamate amount
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte covering this synapse
 * @param glutamate_amount Amount released (arbitrary units)
 * @return 0 on success
 */
int astrocyte_plasticity_notify_glutamate_release(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    float glutamate_amount
);

/**
 * @brief Notify astrocyte of LTP induction
 *
 * WHAT: Signal astrocyte that LTP occurred at synapse
 * WHY:  Strong synaptic activity triggers gliotransmitter release
 * HOW:  Trigger calcium wave, increase D-serine release
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return 0 on success
 */
int astrocyte_plasticity_notify_ltp_induction(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/* ============================================================================
 * Reactive Astrogliosis API
 * ============================================================================ */

/**
 * @brief Set astrocyte reactive state
 *
 * WHAT: Transition astrocyte to reactive phenotype
 * WHY:  Inflammation triggers A1/A2 reactive states
 * HOW:  Update reactive_state, adjust gliotransmitter production
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @param state Target reactive state
 * @param intensity Reaction intensity (0-1)
 * @return 0 on success
 */
int astrocyte_plasticity_set_reactive_state(
    astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_reactive_state_t state,
    float intensity
);

/**
 * @brief Get current reactive state
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return Current reactive state
 */
astrocyte_reactive_state_t astrocyte_plasticity_get_reactive_state(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get astrocyte state
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @param state Output state structure
 * @return 0 on success
 */
int astrocyte_plasticity_get_state(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id,
    astrocyte_state_t* state
);

/**
 * @brief Check if calcium wave is active
 *
 * @param astro Astrocyte system
 * @param astrocyte_id Astrocyte ID
 * @return true if wave is propagating
 */
bool astrocyte_plasticity_is_calcium_wave_active(
    const astrocyte_plasticity_t astro,
    uint32_t astrocyte_id
);

/**
 * @brief Get number of astrocytes in system
 *
 * @param astro Astrocyte system
 * @return Number of astrocytes
 */
uint32_t astrocyte_plasticity_get_num_astrocytes(
    const astrocyte_plasticity_t astro
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTE_PLASTICITY_H */
