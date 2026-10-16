/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_thermo_thalamic_bridge.h - Thermodynamics to Thalamic Gating Bridge
//=============================================================================
/**
 * @file nimcp_thermo_thalamic_bridge.h
 * @brief Temperature modulation of thalamic gating and relay functions
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges thermodynamics to thalamic gating mechanisms,
 *       modeling temperature effects on thalamic relay and oscillations.
 *
 * WHY:  Temperature profoundly affects thalamic function:
 *       - T-type calcium channel kinetics (gating of burst mode)
 *       - H-current (Ih) controlling oscillation frequency
 *       - Thalamocortical oscillation dynamics
 *       - Sleep-wake state transitions
 *       - Sensory gating and relay efficiency
 *       - Thalamic metabolic demands
 *
 * HOW:  - Monitors thermodynamic state (temperature, ATP)
 *       - Modulates T-channel and H-current kinetics via Q10
 *       - Adjusts gating thresholds and transition rates
 *       - Tracks thalamic oscillation frequency shifts
 *       - Models metabolic constraints on thalamic function
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * TEMPERATURE EFFECTS ON THALAMIC FUNCTION:
 * -----------------------------------------
 * 1. T-type Calcium Channels (Q10 ~ 3.0):
 *    - Critical for burst firing mode
 *    - Temperature affects activation/inactivation kinetics
 *    - Higher temp -> faster kinetics -> shorter bursts
 *    - Controls thalamic oscillation initiation
 *
 * 2. H-current (Ih) (Q10 ~ 4.0):
 *    - Hyperpolarization-activated cation current
 *    - Sets oscillation frequency (delta, spindle, alpha)
 *    - Temperature shifts frequency range
 *    - Higher temp -> faster Ih -> higher oscillation freq
 *
 * 3. Thalamocortical Oscillations:
 *    - Delta (0.5-4 Hz): deep sleep, high temp sensitivity
 *    - Spindle (7-14 Hz): light sleep, moderate sensitivity
 *    - Alpha (8-13 Hz): awake relaxed, low sensitivity
 *    - Temperature shifts dominant frequency band
 *
 * 4. Relay Mode vs Burst Mode:
 *    - Relay: faithful transmission (awake)
 *    - Burst: rhythmic, filtered (sleep/anesthesia)
 *    - Temperature affects mode transition threshold
 *    - Fever can disrupt normal state transitions
 *
 * 5. ATP Requirements:
 *    - Thalamus has high metabolic rate
 *    - ATP depletion impairs gating precision
 *    - Affects sleep-wake state stability
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

#ifndef NIMCP_THERMO_THALAMIC_BRIDGE_H
#define NIMCP_THERMO_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Constants
//=============================================================================

/** Module name for logging */
#define THERMO_THALAMIC_MODULE_NAME         "thermo_thalamic_bridge"

/** Reference temperature (Kelvin) - body temperature */
#define THERMO_THALAMIC_TEMP_REF_K          310.15f

/** Q10 for T-type calcium channel kinetics */
#define THERMO_THALAMIC_Q10_T_CHANNEL       3.0f

/** Q10 for H-current (Ih) kinetics */
#define THERMO_THALAMIC_Q10_H_CURRENT       4.0f

/** Q10 for leak conductance */
#define THERMO_THALAMIC_Q10_LEAK            1.5f

/** Q10 for synaptic transmission */
#define THERMO_THALAMIC_Q10_SYNAPTIC        2.5f

/** Q10 for metabolic rate */
#define THERMO_THALAMIC_Q10_METABOLIC       2.0f

/** Reference delta frequency (Hz) */
#define THERMO_THALAMIC_DELTA_FREQ_REF      2.0f

/** Reference spindle frequency (Hz) */
#define THERMO_THALAMIC_SPINDLE_FREQ_REF    12.0f

/** Reference alpha frequency (Hz) */
#define THERMO_THALAMIC_ALPHA_FREQ_REF      10.0f

/** ATP threshold for normal gating */
#define THERMO_THALAMIC_ATP_FULL            0.7f

/** ATP threshold for degraded gating */
#define THERMO_THALAMIC_ATP_MINIMAL         0.3f

/** Default update interval (ms) */
#define THERMO_THALAMIC_DEFAULT_UPDATE_MS   10.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic firing mode
 */
typedef enum {
    THERMO_THALAMIC_MODE_RELAY = 0,         /**< Tonic/relay mode (awake) */
    THERMO_THALAMIC_MODE_BURST,             /**< Burst mode (sleep) */
    THERMO_THALAMIC_MODE_TRANSITIONAL,      /**< Transitioning between modes */
    THERMO_THALAMIC_MODE_SUPPRESSED         /**< Activity suppressed */
} thermo_thalamic_mode_t;

/**
 * @brief Oscillation band
 */
typedef enum {
    THERMO_THALAMIC_BAND_DELTA = 0,         /**< Delta (0.5-4 Hz) */
    THERMO_THALAMIC_BAND_THETA,             /**< Theta (4-8 Hz) */
    THERMO_THALAMIC_BAND_ALPHA,             /**< Alpha (8-13 Hz) */
    THERMO_THALAMIC_BAND_SPINDLE,           /**< Sleep spindles (7-14 Hz) */
    THERMO_THALAMIC_BAND_BETA,              /**< Beta (13-30 Hz) */
    THERMO_THALAMIC_BAND_GAMMA,             /**< Gamma (30-100 Hz) */
    THERMO_THALAMIC_BAND_COUNT
} thermo_thalamic_band_t;

/**
 * @brief Vigilance state
 */
typedef enum {
    THERMO_THALAMIC_STATE_AWAKE = 0,        /**< Alert, awake */
    THERMO_THALAMIC_STATE_DROWSY,           /**< Drowsy, pre-sleep */
    THERMO_THALAMIC_STATE_LIGHT_SLEEP,      /**< Light sleep (N1-N2) */
    THERMO_THALAMIC_STATE_DEEP_SLEEP,       /**< Deep sleep (N3) */
    THERMO_THALAMIC_STATE_REM               /**< REM sleep */
} thermo_thalamic_state_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-thalamic bridge
 *
 * WHAT: All parameters controlling temperature effects on thalamus
 * WHY:  Allows tuning temperature sensitivity of thalamic function
 * HOW:  Q10 values, mode thresholds, oscillation parameters
 */
typedef struct {
    /* Reference values */
    float reference_temp_k;                 /**< Reference temperature (K) */

    /* Q10 coefficients */
    float q10_t_channel;                    /**< Q10 for T-channel kinetics */
    float q10_h_current;                    /**< Q10 for H-current kinetics */
    float q10_leak;                         /**< Q10 for leak conductance */
    float q10_synaptic;                     /**< Q10 for synaptic transmission */
    float q10_metabolic;                    /**< Q10 for metabolic rate */

    /* Mode transition parameters */
    float relay_burst_threshold_mv;         /**< Membrane threshold for mode */
    float temp_mode_sensitivity;            /**< Temperature effect on mode */
    float mode_transition_rate;             /**< Mode transition speed */

    /* Oscillation parameters */
    float delta_freq_ref;                   /**< Reference delta frequency */
    float spindle_freq_ref;                 /**< Reference spindle frequency */
    float alpha_freq_ref;                   /**< Reference alpha frequency */
    float freq_temp_coefficient;            /**< Freq change per degree K */

    /* ATP parameters */
    float atp_full_threshold;               /**< ATP for full gating [0,1] */
    float atp_minimal_threshold;            /**< ATP for minimal gating */
    float atp_per_burst;                    /**< ATP cost per burst (moles) */
    float atp_per_relay;                    /**< ATP cost per relay (moles) */

    /* Gating parameters */
    float base_gate_probability;            /**< Baseline gate-open prob [0,1] */
    float temp_gate_sensitivity;            /**< Temperature effect on gating */
    float min_gate_probability;             /**< Minimum gate probability */
    float max_gate_probability;             /**< Maximum gate probability */

    /* Temperature limits */
    float hypothermia_threshold_k;          /**< Start of hypothermia effects */
    float hyperthermia_threshold_k;         /**< Start of hyperthermia effects */
    float seizure_threshold_k;              /**< Temperature risking seizure */

    /* Feature flags */
    bool enable_mode_modulation;            /**< Modulate relay/burst mode */
    bool enable_oscillation_scaling;        /**< Scale oscillation frequencies */
    bool enable_gating_modulation;          /**< Modulate sensory gating */
    bool enable_atp_tracking;               /**< Track ATP consumption */
    bool enable_state_tracking;             /**< Track vigilance states */
    bool enable_thermal_protection;         /**< Protect at extreme temps */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_thalamic_config_t;

//=============================================================================
// Thalamic Modulation Structure
//=============================================================================

/**
 * @brief Temperature-modulated thalamic parameters
 *
 * WHAT: Scaled thalamic parameters based on current temperature
 * WHY:  Provides ready-to-use parameters for thalamic simulation
 * HOW:  Q10 scaling applied to reference values
 */
typedef struct {
    /* Temperature state */
    float current_temp_k;                   /**< Current temperature (K) */
    float temp_deviation;                   /**< Deviation from reference */

    /* Channel kinetics factors */
    float t_channel_factor;                 /**< T-channel kinetics factor */
    float h_current_factor;                 /**< H-current kinetics factor */
    float leak_factor;                      /**< Leak conductance factor */
    float synaptic_factor;                  /**< Synaptic transmission factor */

    /* Mode state */
    thermo_thalamic_mode_t current_mode;    /**< Current firing mode */
    float relay_probability;                /**< Probability of relay mode */
    float burst_probability;                /**< Probability of burst mode */
    float mode_transition_rate;             /**< Current transition rate */

    /* Oscillation frequencies (temperature-scaled) */
    float scaled_delta_freq;                /**< Scaled delta frequency */
    float scaled_spindle_freq;              /**< Scaled spindle frequency */
    float scaled_alpha_freq;                /**< Scaled alpha frequency */
    thermo_thalamic_band_t dominant_band;   /**< Dominant oscillation band */

    /* Gating state */
    float gate_probability;                 /**< Current gate-open probability */
    float relay_efficiency;                 /**< Sensory relay efficiency [0,1] */
    float filtering_strength;               /**< Thalamic filtering [0,1] */

    /* Vigilance state */
    thermo_thalamic_state_t vigilance;      /**< Current vigilance state */
    float arousal_level;                    /**< Arousal level [0,1] */

    /* ATP state */
    float atp_level;                        /**< Current ATP [0,1] */
    float atp_gate;                         /**< ATP gating factor [0,1] */
    float metabolic_rate_factor;            /**< Metabolic rate scaling */

    /* Protection state */
    bool thermal_warning;                   /**< Temperature warning active */
    bool seizure_risk;                      /**< Elevated seizure risk */
    float protection_factor;                /**< Activity reduction [0,1] */

    /* Timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
} thermo_thalamic_modulation_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t mode_transitions;              /**< Mode transition count */
    uint64_t gating_events;                 /**< Gating events processed */

    /* Temperature stats */
    float min_temp_observed_k;              /**< Minimum temperature */
    float max_temp_observed_k;              /**< Maximum temperature */
    float avg_temp_k;                       /**< Average temperature */

    /* Mode stats */
    uint64_t time_relay_us;                 /**< Time in relay mode */
    uint64_t time_burst_us;                 /**< Time in burst mode */
    uint64_t time_transition_us;            /**< Time transitioning */
    float avg_relay_probability;            /**< Average relay probability */

    /* Oscillation stats */
    float avg_delta_freq;                   /**< Average delta frequency */
    float avg_spindle_freq;                 /**< Average spindle frequency */
    float avg_alpha_freq;                   /**< Average alpha frequency */
    uint64_t band_time[THERMO_THALAMIC_BAND_COUNT]; /**< Time per band */

    /* Gating stats */
    float avg_gate_probability;             /**< Average gate probability */
    float avg_relay_efficiency;             /**< Average relay efficiency */
    uint64_t gates_opened;                  /**< Gate open events */
    uint64_t gates_blocked;                 /**< Gate block events */

    /* ATP stats */
    double total_atp_consumed;              /**< Total ATP consumed (moles) */
    float avg_atp_level;                    /**< Average ATP level */
    uint64_t atp_limited_events;            /**< Events limited by ATP */

    /* Vigilance stats */
    uint64_t time_awake_us;                 /**< Time awake */
    uint64_t time_sleep_us;                 /**< Time asleep */
    uint64_t state_transitions;             /**< State transition count */

    /* Warning stats */
    uint64_t thermal_warnings;              /**< Thermal warning count */
    uint64_t seizure_risk_events;           /**< Seizure risk events */

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_thalamic_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_thalamic_bridge_struct thermo_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with biologically-plausible defaults
 * WHY:  Simplifies bridge creation
 * HOW:  Sets Q10 values from literature, typical oscillation params
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_default_config(
    thermo_thalamic_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-thalamic bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables temperature modulation of thalamic function
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_thalamic_bridge_t* thermo_thalamic_bridge_create(
    const thermo_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_thalamic_bridge_destroy(
    thermo_thalamic_bridge_t* bridge
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to thermodynamic state
 *
 * WHAT: Link bridge to thermodynamics module
 * WHY:  Enables real-time temperature/ATP monitoring
 * HOW:  Stores reference to thermodynamic state
 *
 * @param bridge    Bridge handle
 * @param thermo    Thermodynamic state to monitor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_connect_thermo(
    thermo_thalamic_bridge_t* bridge,
    const nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of thalamic modulation
 * WHY:  Recomputes scaling factors based on current state
 * HOW:  Reads temperature/ATP, applies Q10 scaling
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_update(
    thermo_thalamic_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_set_temperature(
    thermo_thalamic_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set ATP level directly
 *
 * @param bridge    Bridge handle
 * @param atp_level ATP level as fraction [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_set_atp(
    thermo_thalamic_bridge_t* bridge,
    float atp_level
);

/**
 * @brief Set current vigilance state
 *
 * @param bridge    Bridge handle
 * @param state     Vigilance state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_set_vigilance(
    thermo_thalamic_bridge_t* bridge,
    thermo_thalamic_state_t state
);

/**
 * @brief Register gating event for tracking
 *
 * @param bridge    Bridge handle
 * @param passed    Whether signal passed the gate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_register_gating(
    thermo_thalamic_bridge_t* bridge,
    bool passed
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_reset(thermo_thalamic_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current modulation parameters
 *
 * WHAT: Retrieve temperature-scaled thalamic parameters
 * WHY:  For applying modulation to thalamic simulation
 * HOW:  Copies current modulation state to output
 *
 * @param bridge        Bridge handle
 * @param modulation    Output modulation structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_get_modulation(
    const thermo_thalamic_bridge_t* bridge,
    thermo_thalamic_modulation_t* modulation
);

/**
 * @brief Get current firing mode
 *
 * @param bridge    Bridge handle
 * @return Current firing mode
 */
NIMCP_EXPORT thermo_thalamic_mode_t thermo_thalamic_get_mode(
    const thermo_thalamic_bridge_t* bridge
);

/**
 * @brief Get gate probability
 *
 * @param bridge    Bridge handle
 * @return Gate-open probability [0,1]
 */
NIMCP_EXPORT float thermo_thalamic_get_gate_probability(
    const thermo_thalamic_bridge_t* bridge
);

/**
 * @brief Get scaled oscillation frequency
 *
 * @param bridge    Bridge handle
 * @param band      Oscillation band
 * @return Temperature-scaled frequency (Hz), or -1 on error
 */
NIMCP_EXPORT float thermo_thalamic_get_frequency(
    const thermo_thalamic_bridge_t* bridge,
    thermo_thalamic_band_t band
);

/**
 * @brief Check if should gate (random test)
 *
 * WHAT: Probabilistic gating decision
 * WHY:  For stochastic gating simulation
 * HOW:  Compares random value to gate probability
 *
 * @param bridge    Bridge handle
 * @return true if gate should pass signal
 */
NIMCP_EXPORT bool thermo_thalamic_check_gate(
    thermo_thalamic_bridge_t* bridge
);

/**
 * @brief Check seizure risk
 *
 * @param bridge    Bridge handle
 * @return true if seizure risk is elevated
 */
NIMCP_EXPORT bool thermo_thalamic_is_seizure_risk(
    const thermo_thalamic_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_get_stats(
    const thermo_thalamic_bridge_t* bridge,
    thermo_thalamic_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_thalamic_reset_stats(
    thermo_thalamic_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get mode name
 *
 * @param mode  Firing mode
 * @return Mode name string
 */
NIMCP_EXPORT const char* thermo_thalamic_mode_name(
    thermo_thalamic_mode_t mode
);

/**
 * @brief Get band name
 *
 * @param band  Oscillation band
 * @return Band name string
 */
NIMCP_EXPORT const char* thermo_thalamic_band_name(
    thermo_thalamic_band_t band
);

/**
 * @brief Get vigilance state name
 *
 * @param state Vigilance state
 * @return State name string
 */
NIMCP_EXPORT const char* thermo_thalamic_state_name(
    thermo_thalamic_state_t state
);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_thalamic_print_summary(
    const thermo_thalamic_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_THALAMIC_BRIDGE_H */