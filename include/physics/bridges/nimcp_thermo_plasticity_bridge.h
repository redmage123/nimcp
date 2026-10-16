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
// nimcp_thermo_plasticity_bridge.h - Thermodynamics to Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_thermo_plasticity_bridge.h
 * @brief Temperature modulation of synaptic plasticity via Q10 effects
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges thermodynamics to synaptic plasticity mechanisms,
 *       applying Q10 temperature coefficients to learning rates.
 *
 * WHY:  Temperature profoundly affects plasticity:
 *       - STDP time windows scale with temperature (Q10 ~ 2.0-3.0)
 *       - LTP/LTD magnitudes are temperature-dependent
 *       - Protein synthesis for consolidation is thermally sensitive
 *       - Calcium dynamics follow Arrhenius kinetics
 *       - Enzymatic cascades (CaMKII, PKA, PKC) are temperature-modulated
 *
 * HOW:  - Monitors thermodynamic state (temperature, ATP)
 *       - Computes Q10-scaled plasticity parameters
 *       - Modulates STDP windows, learning rates, consolidation
 *       - Gates plasticity based on ATP availability
 *       - Tracks metabolic cost of synaptic modifications
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * TEMPERATURE EFFECTS ON PLASTICITY:
 * ----------------------------------
 * 1. STDP time windows (Q10 ~ 2.5):
 *    - Higher temp -> narrower STDP windows
 *    - Faster calcium dynamics = sharper timing dependence
 *    - tau_stdp(T) = tau_stdp_ref * Q10^((T_ref - T) / 10)
 *
 * 2. LTP magnitude (Q10 ~ 1.5):
 *    - Complex: CaMKII activation vs phosphatase activity
 *    - Moderate temperature sensitivity
 *
 * 3. LTD magnitude (Q10 ~ 1.8):
 *    - Calcineurin (PP2B) activity increases with temperature
 *    - Slightly more temperature-sensitive than LTP
 *
 * 4. Protein synthesis (Q10 ~ 3.0):
 *    - Late-phase LTP requires protein synthesis
 *    - Highly temperature-sensitive enzymatic process
 *    - Memory consolidation strongly affected by fever/hypothermia
 *
 * 5. Calcium influx (Q10 ~ 2.0):
 *    - NMDA receptor conductance is temperature-dependent
 *    - Determines LTP/LTD threshold
 *
 * ATP GATING OF PLASTICITY:
 * -------------------------
 * - Synaptic modification requires ATP for:
 *   - Kinase activation (CaMKII autophosphorylation)
 *   - Protein synthesis and trafficking
 *   - Receptor insertion/removal
 * - Low ATP reduces plasticity capacity
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

#ifndef NIMCP_THERMO_PLASTICITY_BRIDGE_H
#define NIMCP_THERMO_PLASTICITY_BRIDGE_H

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
#define THERMO_PLASTICITY_MODULE_NAME       "thermo_plasticity_bridge"

/** Reference temperature (Kelvin) - body temperature */
#define THERMO_PLASTICITY_TEMP_REF_K        310.15f

/** Q10 for STDP time window */
#define THERMO_PLASTICITY_Q10_STDP_TAU      2.5f

/** Q10 for LTP magnitude */
#define THERMO_PLASTICITY_Q10_LTP           1.5f

/** Q10 for LTD magnitude */
#define THERMO_PLASTICITY_Q10_LTD           1.8f

/** Q10 for protein synthesis */
#define THERMO_PLASTICITY_Q10_SYNTHESIS     3.0f

/** Q10 for calcium dynamics */
#define THERMO_PLASTICITY_Q10_CALCIUM       2.0f

/** Q10 for CaMKII kinetics */
#define THERMO_PLASTICITY_Q10_CAMKII        2.3f

/** ATP threshold for normal plasticity (fraction) */
#define THERMO_PLASTICITY_ATP_FULL          0.7f

/** ATP threshold for minimal plasticity (fraction) */
#define THERMO_PLASTICITY_ATP_MINIMAL       0.2f

/** ATP cost per LTP event (moles) */
#define THERMO_PLASTICITY_ATP_PER_LTP       5.0e-16f

/** ATP cost per LTD event (moles) */
#define THERMO_PLASTICITY_ATP_PER_LTD       3.0e-16f

/** ATP cost per consolidation (moles) */
#define THERMO_PLASTICITY_ATP_CONSOLIDATION 1.0e-14f

/** Default update interval (ms) */
#define THERMO_PLASTICITY_DEFAULT_UPDATE_MS 10.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity type for temperature modulation
 */
typedef enum {
    THERMO_PLASTICITY_TYPE_LTP = 0,         /**< Long-term potentiation */
    THERMO_PLASTICITY_TYPE_LTD,             /**< Long-term depression */
    THERMO_PLASTICITY_TYPE_STDP,            /**< Spike-timing dependent */
    THERMO_PLASTICITY_TYPE_HOMEO,           /**< Homeostatic plasticity */
    THERMO_PLASTICITY_TYPE_META,            /**< Metaplasticity */
    THERMO_PLASTICITY_TYPE_COUNT
} thermo_plasticity_type_t;

/**
 * @brief Consolidation phase
 */
typedef enum {
    THERMO_PLASTICITY_PHASE_EARLY = 0,      /**< Early-phase (protein independent) */
    THERMO_PLASTICITY_PHASE_INTERMEDIATE,   /**< Intermediate (local synthesis) */
    THERMO_PLASTICITY_PHASE_LATE            /**< Late-phase (requires synthesis) */
} thermo_plasticity_phase_t;

/**
 * @brief Temperature effect mode
 */
typedef enum {
    THERMO_PLASTICITY_MODE_DISABLED = 0,    /**< No temperature effects */
    THERMO_PLASTICITY_MODE_Q10,             /**< Q10 exponential scaling */
    THERMO_PLASTICITY_MODE_ARRHENIUS,       /**< Full Arrhenius kinetics */
    THERMO_PLASTICITY_MODE_ADAPTIVE         /**< Adaptive based on history */
} thermo_plasticity_mode_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-plasticity bridge
 *
 * WHAT: All parameters controlling temperature effects on plasticity
 * WHY:  Allows tuning temperature sensitivity for different learning rules
 * HOW:  Q10 values, ATP thresholds, and feature flags
 */
typedef struct {
    /* Operating mode */
    thermo_plasticity_mode_t mode;          /**< Temperature effect mode */

    /* Reference values */
    float reference_temp_k;                 /**< Reference temperature (K) */

    /* Q10 coefficients */
    float q10_stdp_tau;                     /**< Q10 for STDP time window */
    float q10_ltp;                          /**< Q10 for LTP magnitude */
    float q10_ltd;                          /**< Q10 for LTD magnitude */
    float q10_synthesis;                    /**< Q10 for protein synthesis */
    float q10_calcium;                      /**< Q10 for calcium dynamics */
    float q10_camkii;                       /**< Q10 for CaMKII kinetics */

    /* ATP gating parameters */
    float atp_full_threshold;               /**< ATP for full plasticity [0,1] */
    float atp_minimal_threshold;            /**< ATP for minimal plasticity [0,1] */
    float atp_per_ltp;                      /**< ATP cost per LTP (moles) */
    float atp_per_ltd;                      /**< ATP cost per LTD (moles) */
    float atp_per_consolidation;            /**< ATP cost per consolidation (moles) */

    /* STDP parameters (reference values) */
    float ref_ltp_window_ms;                /**< Reference LTP window (ms) */
    float ref_ltd_window_ms;                /**< Reference LTD window (ms) */
    float ref_ltp_amplitude;                /**< Reference LTP amplitude */
    float ref_ltd_amplitude;                /**< Reference LTD amplitude */

    /* Consolidation parameters */
    float consolidation_temp_sensitivity;   /**< How much temp affects consolidation */
    float synthesis_delay_ms;               /**< Protein synthesis delay (ms) */
    float synthesis_duration_ms;            /**< Synthesis window duration (ms) */

    /* Safety limits */
    float min_temp_k;                       /**< Minimum temperature for plasticity */
    float max_temp_k;                       /**< Maximum temperature for plasticity */
    float hyperthermia_threshold_k;         /**< Temperature inhibiting plasticity */
    float hypothermia_threshold_k;          /**< Temperature slowing plasticity */

    /* Feature flags */
    bool enable_stdp_scaling;               /**< Scale STDP windows */
    bool enable_amplitude_scaling;          /**< Scale LTP/LTD amplitudes */
    bool enable_atp_gating;                 /**< Gate plasticity by ATP */
    bool enable_consolidation_scaling;      /**< Scale consolidation rate */
    bool enable_calcium_scaling;            /**< Scale calcium dynamics */
    bool enable_thermal_protection;         /**< Protect from extreme temps */
    bool enable_atp_tracking;               /**< Track ATP consumption */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_plasticity_config_t;

//=============================================================================
// Plasticity Modulation Structure
//=============================================================================

/**
 * @brief Temperature-modulated plasticity parameters
 *
 * WHAT: Scaled plasticity parameters based on current temperature
 * WHY:  Provides ready-to-use parameters for plasticity simulation
 * HOW:  Q10 scaling applied to reference values
 */
typedef struct {
    /* Temperature state */
    float current_temp_k;                   /**< Current temperature (K) */
    float temp_deviation;                   /**< Deviation from reference (K) */

    /* STDP window scaling */
    float ltp_window_factor;                /**< LTP window scaling factor */
    float ltd_window_factor;                /**< LTD window scaling factor */
    float scaled_ltp_window_ms;             /**< Scaled LTP window (ms) */
    float scaled_ltd_window_ms;             /**< Scaled LTD window (ms) */

    /* Amplitude scaling */
    float ltp_amplitude_factor;             /**< LTP amplitude scaling */
    float ltd_amplitude_factor;             /**< LTD amplitude scaling */
    float scaled_ltp_amplitude;             /**< Scaled LTP amplitude */
    float scaled_ltd_amplitude;             /**< Scaled LTD amplitude */

    /* Kinetic scaling */
    float calcium_factor;                   /**< Calcium dynamics scaling */
    float camkii_factor;                    /**< CaMKII kinetics scaling */
    float synthesis_factor;                 /**< Protein synthesis scaling */

    /* ATP gating */
    float atp_level;                        /**< Current ATP level [0,1] */
    float atp_gate;                         /**< ATP gating factor [0,1] */
    bool atp_sufficient;                    /**< ATP above minimal threshold */

    /* Combined modulation */
    float effective_ltp_rate;               /**< Combined LTP rate modifier */
    float effective_ltd_rate;               /**< Combined LTD rate modifier */
    float consolidation_rate;               /**< Consolidation rate modifier */

    /* Thermal protection */
    bool thermal_protection_active;         /**< Protection engaged */
    float protection_factor;                /**< Plasticity reduction [0,1] */

    /* Phase-specific modulation */
    float early_phase_factor;               /**< Early-phase scaling */
    float late_phase_factor;                /**< Late-phase scaling */

    /* Timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
} thermo_plasticity_modulation_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t ltp_events;                    /**< LTP events tracked */
    uint64_t ltd_events;                    /**< LTD events tracked */
    uint64_t consolidation_events;          /**< Consolidation events */

    /* Temperature stats */
    float min_temp_observed_k;              /**< Minimum temperature seen */
    float max_temp_observed_k;              /**< Maximum temperature seen */
    float avg_temp_k;                       /**< Average temperature */
    float temp_variance;                    /**< Temperature variance */

    /* Modulation stats */
    float avg_ltp_window_factor;            /**< Average LTP window factor */
    float avg_ltd_window_factor;            /**< Average LTD window factor */
    float avg_ltp_amplitude_factor;         /**< Average LTP amplitude factor */
    float avg_ltd_amplitude_factor;         /**< Average LTD amplitude factor */

    /* ATP stats */
    double total_atp_consumed;              /**< Total ATP consumed (moles) */
    float avg_atp_level;                    /**< Average ATP level */
    uint64_t atp_limited_events;            /**< Events limited by ATP */

    /* Protection stats */
    uint64_t thermal_protection_events;     /**< Times protection triggered */
    float total_protection_time_s;          /**< Time in protection mode */

    /* Consolidation stats */
    uint64_t early_phase_events;            /**< Early phase consolidations */
    uint64_t late_phase_events;             /**< Late phase consolidations */
    uint64_t failed_consolidations;         /**< Failed due to conditions */

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_plasticity_bridge_struct thermo_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with biologically-plausible defaults
 * WHY:  Simplifies bridge creation with sensible starting point
 * HOW:  Sets Q10 values from literature, enables common features
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_default_config(
    thermo_plasticity_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-plasticity bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables temperature modulation of plasticity
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_plasticity_bridge_t* thermo_plasticity_bridge_create(
    const thermo_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_plasticity_bridge_destroy(
    thermo_plasticity_bridge_t* bridge
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to thermodynamic state
 *
 * WHAT: Link bridge to thermodynamics module for temperature/ATP input
 * WHY:  Enables real-time thermodynamic tracking
 * HOW:  Stores reference to thermodynamic state for polling
 *
 * @param bridge    Bridge handle
 * @param thermo    Thermodynamic state to monitor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_connect_thermo(
    thermo_plasticity_bridge_t* bridge,
    const nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of plasticity modulation
 * WHY:  Recomputes scaling factors based on current state
 * HOW:  Reads temperature/ATP, applies Q10 scaling
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_update(
    thermo_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * WHAT: Manually set operating temperature
 * WHY:  For use without connected thermodynamic state
 * HOW:  Updates internal temperature, recomputes modulation
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_set_temperature(
    thermo_plasticity_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set ATP level directly
 *
 * WHAT: Manually set ATP availability
 * WHY:  For use without connected thermodynamic state
 * HOW:  Updates internal ATP level, recomputes gating
 *
 * @param bridge    Bridge handle
 * @param atp_level ATP level as fraction [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_set_atp(
    thermo_plasticity_bridge_t* bridge,
    float atp_level
);

/**
 * @brief Register plasticity event for ATP tracking
 *
 * WHAT: Record plasticity event for metabolic accounting
 * WHY:  LTP/LTD/consolidation consume ATP
 * HOW:  Deducts ATP cost, updates statistics
 *
 * @param bridge    Bridge handle
 * @param type      Type of plasticity event
 * @param magnitude Event magnitude (for scaling cost)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_register_event(
    thermo_plasticity_bridge_t* bridge,
    thermo_plasticity_type_t type,
    float magnitude
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_reset(thermo_plasticity_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current modulation parameters
 *
 * WHAT: Retrieve temperature-scaled plasticity parameters
 * WHY:  For applying modulation to plasticity simulation
 * HOW:  Copies current modulation state to output
 *
 * @param bridge        Bridge handle
 * @param modulation    Output modulation structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_get_modulation(
    const thermo_plasticity_bridge_t* bridge,
    thermo_plasticity_modulation_t* modulation
);

/**
 * @brief Get STDP window scaling factors
 *
 * @param bridge        Bridge handle
 * @param ltp_factor    Output: LTP window factor
 * @param ltd_factor    Output: LTD window factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_get_stdp_factors(
    const thermo_plasticity_bridge_t* bridge,
    float* ltp_factor,
    float* ltd_factor
);

/**
 * @brief Get amplitude scaling factors
 *
 * @param bridge        Bridge handle
 * @param ltp_amp       Output: LTP amplitude factor
 * @param ltd_amp       Output: LTD amplitude factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_get_amplitude_factors(
    const thermo_plasticity_bridge_t* bridge,
    float* ltp_amp,
    float* ltd_amp
);

/**
 * @brief Get effective learning rates
 *
 * WHAT: Get combined temperature+ATP modulated learning rates
 * WHY:  Single value incorporating all modulation
 * HOW:  Returns product of temperature and ATP factors
 *
 * @param bridge            Bridge handle
 * @param effective_ltp     Output: effective LTP rate [0,1+]
 * @param effective_ltd     Output: effective LTD rate [0,1+]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_get_effective_rates(
    const thermo_plasticity_bridge_t* bridge,
    float* effective_ltp,
    float* effective_ltd
);

/**
 * @brief Get consolidation rate modifier
 *
 * @param bridge    Bridge handle
 * @param phase     Consolidation phase
 * @return Consolidation rate factor, or 0 on error
 */
NIMCP_EXPORT float thermo_plasticity_get_consolidation_rate(
    const thermo_plasticity_bridge_t* bridge,
    thermo_plasticity_phase_t phase
);

/**
 * @brief Check if plasticity is permitted
 *
 * WHAT: Check if conditions allow plasticity
 * WHY:  Quick check before attempting learning
 * HOW:  Verifies temperature and ATP are acceptable
 *
 * @param bridge    Bridge handle
 * @param type      Type of plasticity to check
 * @return true if plasticity is permitted
 */
NIMCP_EXPORT bool thermo_plasticity_is_permitted(
    const thermo_plasticity_bridge_t* bridge,
    thermo_plasticity_type_t type
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_get_stats(
    const thermo_plasticity_bridge_t* bridge,
    thermo_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_plasticity_reset_stats(
    thermo_plasticity_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Compute Q10 scaling for arbitrary parameter
 *
 * @param q10               Q10 coefficient
 * @param current_temp_k    Current temperature (K)
 * @param reference_temp_k  Reference temperature (K)
 * @return Scaling factor
 */
NIMCP_EXPORT float thermo_plasticity_compute_q10(
    float q10,
    float current_temp_k,
    float reference_temp_k
);

/**
 * @brief Get plasticity type name
 *
 * @param type  Plasticity type
 * @return Type name string
 */
NIMCP_EXPORT const char* thermo_plasticity_type_name(
    thermo_plasticity_type_t type
);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_plasticity_print_summary(
    const thermo_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_PLASTICITY_BRIDGE_H */