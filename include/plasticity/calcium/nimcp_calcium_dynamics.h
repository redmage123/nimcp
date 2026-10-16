/**
 * @file nimcp_calcium_dynamics.h
 * @brief Calcium-Dependent Learning Rate Dynamics for Synaptic Plasticity
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Calcium-dependent learning rate modulation implementing omega function
 * WHY:  Intracellular calcium concentration determines direction and magnitude
 *       of synaptic plasticity (LTD at low [Ca²⁺], LTP at high [Ca²⁺])
 * HOW:  Model NMDA-mediated calcium influx, buffering, extrusion, and compute
 *       learning rate as nonlinear function of calcium concentration
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CALCIUM-DEPENDENT PLASTICITY (Bhalla & Bhatt 2007; Bhatt et al. 2016):
 * -----------------------------------------------------------------------
 * 1. Omega Function - Learning Rate Depends on [Ca²⁺]:
 *    - Very low [Ca²⁺] (<0.2 μM): No plasticity (baseline)
 *    - Low [Ca²⁺] (0.2-0.4 μM): LTD (depression, negative learning rate)
 *    - Medium [Ca²⁺] (0.4-0.6 μM): Transition zone (weak plasticity)
 *    - High [Ca²⁺] (0.6-1.0 μM): LTP (potentiation, positive learning rate)
 *    - Very high [Ca²⁺] (>1.0 μM): Saturated LTP
 *    - Reference: Shouval et al. (2002) "A unified model of NMDA receptor-dependent
 *      bidirectional synaptic plasticity"
 *
 * 2. NMDA Receptor-Mediated Calcium Influx:
 *    - Voltage-dependent Mg²⁺ block removal during depolarization
 *    - Calcium influx proportional to NMDA receptor activation
 *    - Postsynaptic spike timing determines influx magnitude
 *    - Reference: Jahr & Stevens (1990) "Voltage dependence of NMDA-activated
 *      macroscopic conductances predicted by single-channel kinetics"
 *
 * 3. Calcium Buffering:
 *    - Endogenous buffers (calbindin, calmodulin) bind calcium
 *    - Buffering capacity β ≈ 20-200 (dendritic spines)
 *    - Fast buffering (τ < 1 ms) shapes calcium transients
 *    - Reference: Helmchen et al. (1996) "Calcium dynamics in dendritic spines"
 *
 * 4. Calcium Extrusion (Pumps/Exchangers):
 *    - Plasma membrane Ca²⁺-ATPase (PMCA)
 *    - Na⁺/Ca²⁺ exchanger (NCX)
 *    - Sarco/endoplasmic reticulum Ca²⁺-ATPase (SERCA)
 *    - Extrusion rate constant τ_pump ≈ 10-100 ms
 *    - Reference: Scheuss et al. (2006) "Calcium handling in dendritic spines"
 *
 * 5. Sleep Modulation:
 *    - AWAKE: Standard calcium dynamics
 *    - NREM: Reduced calcium influx (downstate neurons)
 *    - Deep NREM: Minimal NMDA activity (synaptic consolidation)
 *    - REM: Normal calcium dynamics (replay-driven)
 *    - Reference: Diekelmann & Born (2010) "The memory function of sleep"
 *
 * 6. Immune Modulation:
 *    - Pro-inflammatory cytokines (IL-1β, TNF-α) reduce NMDA calcium currents
 *    - IL-1β reduces calcium influx by 20-30%
 *    - Chronic inflammation impairs calcium homeostasis
 *    - Reference: Viviani et al. (2003) "Interleukin-1β enhances NMDA
 *      receptor-mediated intracellular calcium increase"
 *
 * CALCIUM DYNAMICS MODEL:
 * ----------------------
 * d[Ca²⁺]/dt = J_influx - J_extrusion - J_buffering
 *
 * Where:
 *   J_influx = α * NMDA_activation * (1 - [Ca²⁺]/[Ca²⁺]_max)
 *   J_extrusion = pump_rate * [Ca²⁺]
 *   J_buffering = buffer_capacity * ([Ca²⁺] - [Ca²⁺]_baseline)
 *
 * OMEGA FUNCTION (Learning Rate):
 * -------------------------------
 * ω([Ca²⁺]) = ω_max * (([Ca²⁺] - θ_LTD) / (θ_LTP - θ_LTD)) ^ p
 *
 * Where:
 *   θ_LTD = 0.35 μM (LTD threshold)
 *   θ_LTP = 0.55 μM (LTP threshold)
 *   ω_max = maximum learning rate
 *   p = power (typically 2-3 for sigmoidal shape)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              CALCIUM-DEPENDENT LEARNING RATE DYNAMICS                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   NMDA Activation  ──────┐                                                ║
 * ║   (spike timing)         │                                                ║
 * ║                          ▼                                                ║
 * ║                   ┌──────────────┐                                        ║
 * ║                   │   Ca²⁺       │                                        ║
 * ║                   │   INFLUX     │──────┐                                 ║
 * ║                   └──────────────┘      │                                 ║
 * ║                                         │                                 ║
 * ║                                         ▼                                 ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              CALCIUM CONCENTRATION [Ca²⁺]                           │  ║
 * ║   │                                                                     │  ║
 * ║   │  Baseline: 0.1 μM                                                  │  ║
 * ║   │  Range:    0.0 - 2.0 μM                                            │  ║
 * ║   │  θ_LTD:    0.35 μM                                                 │  ║
 * ║   │  θ_LTP:    0.55 μM                                                 │  ║
 * ║   └─────┬───────────────────────────────────────┬────────────────┬─────┘  ║
 * ║         │                                       │                │        ║
 * ║         ▼                                       ▼                ▼        ║
 * ║   ┌──────────┐                           ┌──────────┐   ┌──────────────┐ ║
 * ║   │ BUFFERING│                           │ EXTRUSION│   │   OMEGA      │ ║
 * ║   │  (fast)  │                           │ (pumps)  │   │ FUNCTION     │ ║
 * ║   └──────────┘                           └──────────┘   │              │ ║
 * ║                                                          │ [Ca²⁺] → η  │ ║
 * ║                                                          └──────┬───────┘ ║
 * ║                                                                 │         ║
 * ║                                                                 ▼         ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                      LEARNING RATE (η)                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   [Ca²⁺] < 0.2 μM:    η = 0         (no plasticity)               │  ║
 * ║   │   [Ca²⁺] = 0.3 μM:    η < 0         (LTD)                          │  ║
 * ║   │   [Ca²⁺] = 0.45 μM:   η ≈ 0         (transition)                   │  ║
 * ║   │   [Ca²⁺] = 0.7 μM:    η > 0         (LTP)                          │  ║
 * ║   │   [Ca²⁺] > 1.0 μM:    η = η_max     (saturated)                    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * INTEGRATION BRIDGES:
 * -------------------
 * - Sleep Bridge: Sleep state modulates calcium influx and decay
 * - Immune Bridge: Inflammation affects NMDA calcium currents
 * - Bio-async: Calcium threshold crossings trigger inter-module messages
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

#ifndef NIMCP_CALCIUM_DYNAMICS_H
#define NIMCP_CALCIUM_DYNAMICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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
 * Constants - Calcium Dynamics
 * ============================================================================ */

/* Calcium concentration ranges (μM) */
#define CALCIUM_BASELINE_CONCENTRATION       0.1f     /**< Resting [Ca²⁺] */
#define CALCIUM_MIN_CONCENTRATION            0.0f     /**< Minimum [Ca²⁺] */
#define CALCIUM_MAX_CONCENTRATION            2.0f     /**< Maximum [Ca²⁺] */

/* Threshold concentrations (μM) for Omega function */
#define CALCIUM_THRESHOLD_LTD                0.35f    /**< LTD threshold */
#define CALCIUM_THRESHOLD_LTP                0.55f    /**< LTP threshold */
#define CALCIUM_THRESHOLD_LTP_SATURATION     1.0f     /**< LTP saturation */
#define CALCIUM_THRESHOLD_NO_PLASTICITY      0.2f     /**< Below this: no plasticity */

/* Calcium dynamics time constants (ms) */
#define CALCIUM_DECAY_TAU_DEFAULT            50.0f    /**< Decay time constant */
#define CALCIUM_PUMP_RATE_DEFAULT            0.02f    /**< Pump rate (1/ms) */
#define CALCIUM_BUFFER_CAPACITY_DEFAULT      50.0f    /**< Buffer capacity β */
#define CALCIUM_INFLUX_ALPHA_DEFAULT         0.1f     /**< Influx rate constant */

/* Omega function parameters */
#define CALCIUM_OMEGA_MAX_LEARNING_RATE      0.01f    /**< Maximum LR */
#define CALCIUM_OMEGA_POWER                  2.5f     /**< Sigmoid power */

/* NMDA receptor parameters */
#define CALCIUM_NMDA_MAX_CONDUCTANCE         1.0f     /**< Max NMDA conductance */
#define CALCIUM_NMDA_MG_BLOCK_VOLTAGE        -65.0f   /**< Mg²⁺ block voltage (mV) */

/* Callback capacity */
#define CALCIUM_MAX_THRESHOLD_CALLBACKS      8        /**< Max callbacks */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Calcium threshold crossing types
 *
 * WHAT: Types of threshold crossings for callbacks
 * WHY:  Different thresholds trigger different plasticity mechanisms
 * HOW:  Enum for callback event types
 */
typedef enum {
    CALCIUM_CROSS_LTD_THRESHOLD_UP,      /**< Crossed θ_LTD upward (entering LTD zone) */
    CALCIUM_CROSS_LTD_THRESHOLD_DOWN,    /**< Crossed θ_LTD downward (leaving LTD zone) */
    CALCIUM_CROSS_LTP_THRESHOLD_UP,      /**< Crossed θ_LTP upward (entering LTP zone) */
    CALCIUM_CROSS_LTP_THRESHOLD_DOWN,    /**< Crossed θ_LTP downward (leaving LTP zone) */
    CALCIUM_CROSS_SATURATION_UP,         /**< Crossed saturation threshold upward */
    CALCIUM_CROSS_SATURATION_DOWN        /**< Crossed saturation threshold downward */
} calcium_threshold_crossing_t;

/**
 * @brief Calcium plasticity regime
 *
 * WHAT: Current plasticity state based on [Ca²⁺]
 * WHY:  Categorize plasticity direction and magnitude
 * HOW:  Threshold-based classification
 */
typedef enum {
    CALCIUM_REGIME_NONE,            /**< [Ca²⁺] < θ_no_plasticity: no change */
    CALCIUM_REGIME_LTD,             /**< θ_no_plasticity ≤ [Ca²⁺] < θ_LTD: depression */
    CALCIUM_REGIME_TRANSITION,      /**< θ_LTD ≤ [Ca²⁺] < θ_LTP: weak plasticity */
    CALCIUM_REGIME_LTP,             /**< θ_LTP ≤ [Ca²⁺] < saturation: potentiation */
    CALCIUM_REGIME_SATURATED        /**< [Ca²⁺] ≥ saturation: max LTP */
} calcium_plasticity_regime_t;

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Callback for threshold crossing events
 *
 * WHAT: User-provided function called when calcium crosses threshold
 * WHY:  Enable downstream plasticity mechanisms to respond to calcium events
 * HOW:  Function pointer with crossing type, concentration, and user data
 *
 * @param crossing_type Type of threshold crossing
 * @param ca_concentration Current calcium concentration (μM)
 * @param user_data User-provided context
 */
typedef void (*calcium_threshold_callback_t)(
    calcium_threshold_crossing_t crossing_type,
    float ca_concentration,
    void* user_data
);

/**
 * @brief Calcium dynamics configuration
 *
 * WHAT: Parameters controlling calcium dynamics
 * WHY:  Customizable for different neuron types and conditions
 * HOW:  All rate constants, thresholds, and capacities
 */
typedef struct {
    /* Concentration thresholds (μM) */
    float baseline_concentration;    /**< Resting [Ca²⁺] */
    float threshold_ltd;              /**< LTD threshold */
    float threshold_ltp;              /**< LTP threshold */
    float threshold_saturation;       /**< LTP saturation */
    float threshold_no_plasticity;    /**< Below this: no plasticity */
    float max_concentration;          /**< Maximum allowable [Ca²⁺] */

    /* Dynamics parameters */
    float decay_tau_ms;               /**< Decay time constant (ms) */
    float pump_rate;                  /**< Pump extrusion rate (1/ms) */
    float buffer_capacity;            /**< Buffering capacity β */
    float influx_alpha;               /**< Influx rate constant */

    /* Omega function parameters */
    float omega_max_learning_rate;    /**< Maximum learning rate */
    float omega_power;                /**< Power for sigmoid shape */

    /* NMDA receptor parameters */
    float nmda_max_conductance;       /**< Maximum NMDA conductance */
    float nmda_mg_block_voltage_mv;   /**< Mg²⁺ block voltage (mV) */

    /* Integration enables */
    bool enable_nmda_influx;          /**< Enable NMDA-mediated influx */
    bool enable_buffering;            /**< Enable calcium buffering */
    bool enable_pumps;                /**< Enable pump extrusion */
    bool enable_omega_function;       /**< Enable omega LR computation */
} calcium_config_t;

/**
 * @brief Calcium dynamics state
 *
 * WHAT: Current calcium concentration and dynamics
 * WHY:  Track temporal evolution of calcium
 * HOW:  State variables for differential equations
 */
typedef struct {
    /* Current state */
    float ca_concentration;           /**< Current [Ca²⁺] (μM) */
    float ca_concentration_prev;      /**< Previous [Ca²⁺] for crossing detection */
    calcium_plasticity_regime_t regime; /**< Current plasticity regime */

    /* Fluxes (for diagnostics) */
    float last_influx;                /**< Last influx flux */
    float last_extrusion;             /**< Last extrusion flux */
    float last_buffering;             /**< Last buffering flux */

    /* Omega function output */
    float current_learning_rate;      /**< Current η from omega function */

    /* NMDA state */
    float nmda_activation;            /**< NMDA receptor activation [0-1] */
    float postsynaptic_voltage_mv;    /**< Postsynaptic voltage (mV) */

    /* Statistics */
    uint64_t total_updates;           /**< Total update steps */
    uint32_t ltd_events;              /**< Count of LTD regime entries */
    uint32_t ltp_events;              /**< Count of LTP regime entries */
    uint64_t time_in_ltd_ms;          /**< Time spent in LTD regime */
    uint64_t time_in_ltp_ms;          /**< Time spent in LTP regime */
} calcium_state_t;

/**
 * @brief Calcium dynamics system
 */
typedef struct calcium_dynamics_struct* calcium_dynamics_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default calcium dynamics configuration
 *
 * WHAT: Provide biologically-based default parameters
 * WHY:  Easy initialization with evidence-based values
 * HOW:  Return struct with Shouval et al. (2002) parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int calcium_default_config(calcium_config_t* config);

/**
 * @brief Create calcium dynamics system
 *
 * WHAT: Initialize calcium concentration tracking and omega function
 * WHY:  Enable calcium-dependent learning rate modulation
 * HOW:  Allocate structure, initialize state to baseline
 *
 * @param config Configuration (NULL for defaults)
 * @return New calcium system or NULL on failure
 */
calcium_dynamics_t calcium_create(const calcium_config_t* config);

/**
 * @brief Destroy calcium dynamics system
 *
 * WHAT: Clean up calcium system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure and mutex
 *
 * @param calcium Calcium system to destroy
 */
void calcium_destroy(calcium_dynamics_t calcium);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update calcium dynamics for time step
 *
 * WHAT: Advance calcium concentration by delta_ms
 * WHY:  Model temporal dynamics of calcium influx/extrusion
 * HOW:  Integrate differential equations: d[Ca²⁺]/dt = J_in - J_out
 *
 * @param calcium Calcium system
 * @param delta_ms Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
int calcium_update(calcium_dynamics_t calcium, float delta_ms);

/**
 * @brief Trigger NMDA-mediated calcium influx
 *
 * WHAT: Add calcium from NMDA receptor activation
 * WHY:  Spike timing drives calcium influx
 * HOW:  Compute voltage-dependent Mg²⁺ block, add proportional Ca²⁺
 *
 * @param calcium Calcium system
 * @param nmda_activation NMDA activation level [0-1]
 * @param postsynaptic_voltage_mv Postsynaptic voltage (mV)
 * @return 0 on success, -1 on error
 */
int calcium_trigger_nmda_influx(
    calcium_dynamics_t calcium,
    float nmda_activation,
    float postsynaptic_voltage_mv
);

/**
 * @brief Manually set calcium concentration
 *
 * WHAT: Override current calcium level
 * WHY:  For testing or external calcium sources (VGCC, IP3)
 * HOW:  Clamp to valid range, update state
 *
 * @param calcium Calcium system
 * @param concentration New [Ca²⁺] (μM)
 * @return 0 on success, -1 on error
 */
int calcium_set_concentration(calcium_dynamics_t calcium, float concentration);

/**
 * @brief Reset calcium to baseline
 *
 * WHAT: Return calcium to resting concentration
 * WHY:  Simulate prolonged silence or reset for new trial
 * HOW:  Set concentration to baseline, clear transients
 *
 * @param calcium Calcium system
 * @return 0 on success
 */
int calcium_reset(calcium_dynamics_t calcium);

/* ============================================================================
 * Omega Function API (Learning Rate)
 * ============================================================================ */

/**
 * @brief Compute learning rate from current calcium concentration
 *
 * WHAT: Apply omega function to get η from [Ca²⁺]
 * WHY:  Calcium determines plasticity direction and magnitude
 * HOW:  ω([Ca²⁺]) = ω_max * ((Ca - θ_LTD)/(θ_LTP - θ_LTD))^p
 *
 * @param calcium Calcium system
 * @return Learning rate η (negative for LTD, positive for LTP)
 */
float calcium_compute_learning_rate(const calcium_dynamics_t calcium);

/**
 * @brief Get current plasticity regime
 *
 * WHAT: Classify current [Ca²⁺] into regime (none/LTD/transition/LTP/saturated)
 * WHY:  Quick check of plasticity state
 * HOW:  Threshold comparison
 *
 * @param calcium Calcium system
 * @return Plasticity regime
 */
calcium_plasticity_regime_t calcium_get_regime(const calcium_dynamics_t calcium);

/**
 * @brief Check if calcium is in LTP regime
 *
 * WHAT: Test if [Ca²⁺] ≥ θ_LTP
 * WHY:  Quick boolean check for LTP
 * HOW:  Compare concentration to threshold
 *
 * @param calcium Calcium system
 * @return true if in LTP regime
 */
bool calcium_is_ltp(const calcium_dynamics_t calcium);

/**
 * @brief Check if calcium is in LTD regime
 *
 * WHAT: Test if θ_no_plasticity ≤ [Ca²⁺] < θ_LTD
 * WHY:  Quick boolean check for LTD
 * HOW:  Compare concentration to thresholds
 *
 * @param calcium Calcium system
 * @return true if in LTD regime
 */
bool calcium_is_ltd(const calcium_dynamics_t calcium);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current calcium concentration
 *
 * @param calcium Calcium system
 * @return Current [Ca²⁺] (μM)
 */
float calcium_get_concentration(const calcium_dynamics_t calcium);

/**
 * @brief Get current learning rate
 *
 * @param calcium Calcium system
 * @return Current η from omega function
 */
float calcium_get_learning_rate(const calcium_dynamics_t calcium);

/**
 * @brief Get calcium dynamics state
 *
 * @param calcium Calcium system
 * @param state Output state structure
 * @return 0 on success, -1 on error
 */
int calcium_get_state(const calcium_dynamics_t calcium, calcium_state_t* state);

/**
 * @brief Get calcium configuration
 *
 * @param calcium Calcium system
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int calcium_get_config(const calcium_dynamics_t calcium, calcium_config_t* config);

/* ============================================================================
 * Callback API
 * ============================================================================ */

/**
 * @brief Register callback for threshold crossings
 *
 * WHAT: Add user callback for calcium threshold events
 * WHY:  Enable downstream modules to react to calcium changes
 * HOW:  Store callback pointer and user data
 *
 * @param calcium Calcium system
 * @param callback Callback function
 * @param user_data User-provided context
 * @return 0 on success, -1 if callback array full
 */
int calcium_register_threshold_callback(
    calcium_dynamics_t calcium,
    calcium_threshold_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister threshold callback
 *
 * @param calcium Calcium system
 * @param callback Callback to remove
 * @return 0 on success, -1 if not found
 */
int calcium_unregister_threshold_callback(
    calcium_dynamics_t calcium,
    calcium_threshold_callback_t callback
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect calcium system to bio-async router
 *
 * WHAT: Register as bio-async module for calcium events
 * WHY:  Enable inter-module messaging for threshold crossings
 * HOW:  Register with bio_router using BIO_MODULE_CALCIUM_DYNAMICS
 *
 * @param calcium Calcium system
 * @return 0 on success, -1 on error
 */
int calcium_connect_bio_async(calcium_dynamics_t calcium);

/**
 * @brief Disconnect from bio-async router
 *
 * @param calcium Calcium system
 * @return 0 on success
 */
int calcium_disconnect_bio_async(calcium_dynamics_t calcium);

/**
 * @brief Check if bio-async is connected
 *
 * @param calcium Calcium system
 * @return true if connected
 */
bool calcium_is_bio_async_connected(const calcium_dynamics_t calcium);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute NMDA Mg²⁺ block factor
 *
 * WHAT: Voltage-dependent Mg²⁺ block removal
 * WHY:  NMDA receptors blocked at rest, unblocked during depolarization
 * HOW:  Jahr-Stevens model: 1/(1 + [Mg²⁺]*exp(-0.062*V)/3.57)
 *
 * @param voltage_mv Postsynaptic voltage (mV)
 * @return Mg²⁺ block factor [0-1] (1 = fully unblocked)
 */
float calcium_compute_mg_block(float voltage_mv);

/**
 * @brief Compute omega function directly from concentration
 *
 * WHAT: Evaluate omega function for arbitrary [Ca²⁺]
 * WHY:  Utility for testing or external use
 * HOW:  Apply omega formula with given parameters
 *
 * @param ca_concentration Calcium concentration (μM)
 * @param threshold_ltd LTD threshold (μM)
 * @param threshold_ltp LTP threshold (μM)
 * @param omega_max Maximum learning rate
 * @param power Sigmoid power
 * @return Learning rate η
 */
float calcium_omega_function(
    float ca_concentration,
    float threshold_ltd,
    float threshold_ltp,
    float omega_max,
    float power
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CALCIUM_DYNAMICS_H */
