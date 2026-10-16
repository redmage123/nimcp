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
// nimcp_ph_snn_bridge.h - pH Dynamics to SNN Neuronal Excitability Bridge
//=============================================================================
/**
 * @file nimcp_ph_snn_bridge.h
 * @brief Bridge connecting pH dynamics with Spiking Neural Network excitability
 *
 * WHAT: Bidirectional bridge between pH dynamics module and SNN systems for
 *       modulating neuronal excitability based on proton concentrations.
 *
 * WHY:  pH strongly influences neuronal excitability and network dynamics:
 *       - Acidosis depresses most ion channel conductances (except ASICs)
 *       - Alkalosis enhances NMDA receptor activity and excitability
 *       - ASIC activation during acidosis causes transient depolarization
 *       - Intracellular pH affects metabolic enzyme activity
 *
 * HOW:  Two-way integration:
 *       1. pH -> SNN: Modulate spike thresholds, conductances, time constants
 *       2. SNN -> pH: High activity causes activity-dependent acidification
 *       3. ASIC channels: Model acid-sensing ion channel activation
 *       4. Buffer capacity: Regulate pH recovery dynamics
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              SNN EFFECTS
 * ---------------------------------------------------------------
 * Extracellular acidosis (pH < 7.3)     -> Reduced NMDA current
 * Extracellular alkalosis (pH > 7.5)    -> Enhanced NMDA current
 * ASIC activation (pH < 7.0 transient)  -> Transient Na+ current
 * Intracellular acidosis                -> Reduced K+ conductance
 * Activity-dependent acid load          <- High firing rate
 * Vesicular pH drop                     <- Neurotransmitter release
 * ```
 *
 * KEY ASIC PROPERTIES:
 * - ASIC1a: Half-activation pH ~6.5, desensitization ~1s
 * - ASIC2a: Half-activation pH ~5.5, slower kinetics
 * - ASIC3:  Half-activation pH ~6.7, sustained component
 * - ASICs provide depolarizing current during transient acidification
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_SNN_BRIDGE_H
#define NIMCP_PH_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PH_SNN_MODULE_NAME          "ph_snn_bridge"

/** Maximum SNN populations to modulate */
#define PH_SNN_MAX_POPULATIONS      64

/** Maximum ASIC channel types */
#define PH_SNN_MAX_ASIC_TYPES       4

/** Normal extracellular pH */
#define PH_SNN_NORMAL_PH            7.4f

/** ASIC1a half-activation pH */
#define PH_SNN_ASIC1A_PH50          6.5f

/** ASIC2a half-activation pH */
#define PH_SNN_ASIC2A_PH50          5.5f

/** ASIC3 half-activation pH */
#define PH_SNN_ASIC3_PH50           6.7f

/** NMDA pH sensitivity midpoint */
#define PH_SNN_NMDA_PH50            7.3f

/** NMDA proton block slope */
#define PH_SNN_NMDA_SLOPE           0.2f

/** Activity-induced acidification rate (pH/Hz) */
#define PH_SNN_ACTIVITY_ACID_RATE   0.001f

/** pH recovery time constant (ms) */
#define PH_SNN_PH_TAU_RECOVERY      1000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief ASIC channel subtypes
 */
typedef enum {
    PH_SNN_ASIC_1A = 0,              /**< ASIC1a - main neuronal subtype */
    PH_SNN_ASIC_1B,                  /**< ASIC1b - peripheral */
    PH_SNN_ASIC_2A,                  /**< ASIC2a - low pH threshold */
    PH_SNN_ASIC_3,                   /**< ASIC3 - sustained component */
    PH_SNN_ASIC_COUNT
} ph_snn_asic_type_t;

/**
 * @brief pH modulation targets in SNN
 */
typedef enum {
    PH_SNN_MOD_THRESHOLD = 0,        /**< Spike threshold modulation */
    PH_SNN_MOD_NMDA_CONDUCTANCE,     /**< NMDA receptor block */
    PH_SNN_MOD_GABA_CONDUCTANCE,     /**< GABA receptor modulation */
    PH_SNN_MOD_MEMBRANE_TAU,         /**< Time constant modulation */
    PH_SNN_MOD_ASIC_CURRENT,         /**< ASIC current injection */
    PH_SNN_MOD_COUNT
} ph_snn_modulation_t;

/**
 * @brief SNN activity feedback types
 */
typedef enum {
    PH_SNN_FEEDBACK_FIRING_RATE = 0, /**< Population firing rate */
    PH_SNN_FEEDBACK_BURST_ACTIVITY,  /**< Burst mode activity */
    PH_SNN_FEEDBACK_SYNCHRONY,       /**< Population synchrony */
    PH_SNN_FEEDBACK_METABOLIC_LOAD   /**< Metabolic demand */
} ph_snn_feedback_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief ASIC channel model state
 */
typedef struct {
    ph_snn_asic_type_t type;         /**< ASIC subtype */
    float ph50;                      /**< Half-activation pH */
    float hill_coefficient;          /**< Hill coefficient */
    float g_max;                     /**< Maximum conductance (nS) */
    float tau_activation;            /**< Activation time constant (ms) */
    float tau_desensitization;       /**< Desensitization time constant (ms) */
    float tau_recovery;              /**< Recovery from desens. (ms) */
    float activation;                /**< Current activation (0-1) */
    float desensitization;           /**< Desensitization state (0-1) */
    float current;                   /**< Current ASIC current (pA) */
    bool enabled;                    /**< Channel enabled */
} ph_snn_asic_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** pH monitoring parameters */
    bool monitor_extracellular;      /**< Monitor extracellular pH */
    bool monitor_intracellular;      /**< Monitor intracellular pH */
    float ph_sample_interval_ms;     /**< pH sampling interval */

    /** ASIC channel parameters */
    bool enable_asic_channels;       /**< Enable ASIC modeling */
    float asic1a_density;            /**< ASIC1a density factor */
    float asic3_density;             /**< ASIC3 density factor */
    float asic_threshold_ph;         /**< pH threshold for ASIC activation */

    /** NMDA proton block parameters */
    bool enable_nmda_block;          /**< Enable pH-dependent NMDA block */
    float nmda_ph50;                 /**< NMDA half-block pH */
    float nmda_slope;                /**< NMDA block slope */

    /** Activity feedback parameters */
    bool enable_activity_feedback;   /**< Enable SNN->pH feedback */
    float acid_production_rate;      /**< Acid per Hz of activity */
    float ph_recovery_tau;           /**< Recovery time constant (ms) */

    /** Threshold modulation */
    float acidosis_threshold_shift;  /**< Threshold shift per pH unit below 7.4 */
    float alkalosis_threshold_shift; /**< Threshold shift per pH unit above 7.4 */

    /** Update parameters */
    float update_interval_ms;        /**< Bridge update interval */
} ph_snn_config_t;

/**
 * @brief pH state for SNN modulation
 */
typedef struct {
    float extracellular_ph;          /**< Extracellular pH */
    float intracellular_ph;          /**< Intracellular pH */
    float ph_deviation;              /**< Deviation from normal */
    float acid_load;                 /**< Current acid load */
    float buffer_capacity;           /**< Available buffering */
    float nmda_block_factor;         /**< NMDA proton block (0-1) */
    float threshold_modifier;        /**< Threshold shift (mV) */
    float timestamp_ms;              /**< Timestamp */
} ph_snn_ph_state_t;

/**
 * @brief SNN modulation output
 */
typedef struct {
    /** Conductance modifiers */
    float nmda_conductance_factor;   /**< NMDA conductance (0-1) */
    float gaba_conductance_factor;   /**< GABA conductance (0-1) */

    /** Threshold modulation */
    float threshold_shift_mv;        /**< Spike threshold shift (mV) */

    /** Time constant modulation */
    float tau_membrane_factor;       /**< Membrane tau modifier */

    /** ASIC currents */
    float asic_current_pa;           /**< Total ASIC current (pA) */
    float asic_transient_depol_mv;   /**< Transient depolarization */

    /** Excitability index */
    float excitability_index;        /**< Combined excitability (0-2) */

    /** Per-population modifiers */
    uint32_t affected_populations;   /**< Number of affected populations */
} ph_snn_modulation_output_t;

/**
 * @brief Activity feedback from SNN
 */
typedef struct {
    uint32_t population_id;          /**< Source population */
    float firing_rate_hz;            /**< Current firing rate */
    float burst_fraction;            /**< Fraction in burst mode */
    float synchrony_index;           /**< Population synchrony (0-1) */
    float metabolic_demand;          /**< Estimated metabolic load */
    float duration_ms;               /**< Activity duration */
} ph_snn_activity_report_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t ph_updates;             /**< pH state updates */
    uint64_t snn_modulations;        /**< SNN modulations applied */
    uint64_t asic_activations;       /**< ASIC activation events */
    uint64_t activity_feedbacks;     /**< Activity feedback processed */
    float total_acid_produced;       /**< Total acid from activity */
    float total_acid_buffered;       /**< Total acid buffered */
    float avg_nmda_block;            /**< Average NMDA block */
    float avg_threshold_shift;       /**< Average threshold shift */
    float max_asic_current;          /**< Maximum ASIC current */
    float last_update_ms;            /**< Last update timestamp */
} ph_snn_stats_t;

/** Opaque bridge handle */
typedef struct ph_snn_bridge_struct ph_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_default_config(ph_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-SNN bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_snn_bridge_t* ph_snn_bridge_create(
    const ph_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_snn_bridge_destroy(ph_snn_bridge_t* bridge);

//=============================================================================
// pH Input API (pH Dynamics -> SNN)
//=============================================================================

/**
 * @brief Update pH state from pH dynamics module
 *
 * WHAT: Receives current pH values from pH dynamics
 * WHY:  pH state determines SNN modulation
 * HOW:  Stores pH state, computes modulation factors
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Current extracellular pH
 * @param intracellular_ph Current intracellular pH
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_update_ph(
    ph_snn_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph
);

/**
 * @brief Apply transient pH drop (acid pulse)
 *
 * WHAT: Models rapid acidification events
 * WHY:  Transient acidosis activates ASICs
 * HOW:  Applies pH drop, triggers ASIC response
 *
 * @param bridge Bridge handle
 * @param ph_drop Magnitude of pH drop
 * @param duration_ms Duration of transient
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_apply_acid_pulse(
    ph_snn_bridge_t* bridge,
    float ph_drop,
    float duration_ms
);

/**
 * @brief Get current pH-dependent NMDA block factor
 *
 * @param bridge Bridge handle
 * @param block_factor Output: NMDA block (0=full block, 1=no block)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_nmda_block(
    const ph_snn_bridge_t* bridge,
    float* block_factor
);

//=============================================================================
// SNN Modulation API
//=============================================================================

/**
 * @brief Compute SNN modulation based on current pH state
 *
 * WHAT: Calculates all pH-dependent SNN parameter modulations
 * WHY:  Single call to get all modulation values
 * HOW:  Evaluates NMDA block, threshold shift, ASIC currents
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation values
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_compute_modulation(
    ph_snn_bridge_t* bridge,
    ph_snn_modulation_output_t* modulation
);

/**
 * @brief Get spike threshold modifier
 *
 * @param bridge Bridge handle
 * @param threshold_shift Output: threshold shift in mV
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_threshold_modifier(
    const ph_snn_bridge_t* bridge,
    float* threshold_shift
);

/**
 * @brief Get ASIC current for injection into SNN neuron
 *
 * @param bridge Bridge handle
 * @param neuron_id Target neuron ID
 * @param asic_current Output: ASIC current in pA
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_asic_current(
    const ph_snn_bridge_t* bridge,
    uint32_t neuron_id,
    float* asic_current
);

/**
 * @brief Get excitability index combining all pH effects
 *
 * @param bridge Bridge handle
 * @param excitability Output: excitability index (1.0 = normal)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_excitability(
    const ph_snn_bridge_t* bridge,
    float* excitability
);

//=============================================================================
// ASIC Channel API
//=============================================================================

/**
 * @brief Update ASIC channel states
 *
 * WHAT: Updates activation/desensitization of ASIC channels
 * WHY:  ASIC dynamics depend on pH history
 * HOW:  Euler integration of ASIC kinetics
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_update_asic(
    ph_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Get ASIC channel state
 *
 * @param bridge Bridge handle
 * @param asic_type ASIC subtype
 * @param state Output: ASIC state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_asic_state(
    const ph_snn_bridge_t* bridge,
    ph_snn_asic_type_t asic_type,
    ph_snn_asic_state_t* state
);

/**
 * @brief Set ASIC channel density for a neuron type
 *
 * @param bridge Bridge handle
 * @param asic_type ASIC subtype
 * @param density Density factor (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_set_asic_density(
    ph_snn_bridge_t* bridge,
    ph_snn_asic_type_t asic_type,
    float density
);

//=============================================================================
// Activity Feedback API (SNN -> pH)
//=============================================================================

/**
 * @brief Report SNN activity for pH feedback
 *
 * WHAT: Receives activity report from SNN population
 * WHY:  Neural activity causes acidification
 * HOW:  Converts activity to acid production
 *
 * @param bridge Bridge handle
 * @param activity Activity report
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_report_activity(
    ph_snn_bridge_t* bridge,
    const ph_snn_activity_report_t* activity
);

/**
 * @brief Get activity-induced acid production
 *
 * @param bridge Bridge handle
 * @param acid_load Output: acid load (proton equivalents)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_acid_production(
    const ph_snn_bridge_t* bridge,
    float* acid_load
);

/**
 * @brief Reset accumulated acid load
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_reset_acid_load(ph_snn_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process pH changes, update ASIC kinetics
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_update(
    ph_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_reset(ph_snn_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current pH state
 *
 * @param bridge Bridge handle
 * @param state Output pH state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_ph_state(
    const ph_snn_bridge_t* bridge,
    ph_snn_ph_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_snn_get_stats(
    const ph_snn_bridge_t* bridge,
    ph_snn_stats_t* stats
);

/**
 * @brief Check if pH is in acidotic range
 *
 * @param bridge Bridge handle
 * @return true if extracellular pH < 7.35
 */
NIMCP_EXPORT bool ph_snn_is_acidotic(const ph_snn_bridge_t* bridge);

/**
 * @brief Check if pH is in alkalotic range
 *
 * @param bridge Bridge handle
 * @return true if extracellular pH > 7.45
 */
NIMCP_EXPORT bool ph_snn_is_alkalotic(const ph_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_SNN_BRIDGE_H */