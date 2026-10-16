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
// nimcp_thermo_immune_bridge.h - Thermodynamics to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_thermo_immune_bridge.h
 * @brief Bidirectional integration of thermodynamics with brain immune system
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges thermodynamics with the brain immune system, modeling
 *       fever response, immune activation, and neuroimmune signaling.
 *
 * WHY:  Temperature and immunity are deeply interconnected:
 *       - Fever is an evolved defense mechanism (pyrogens -> hypothalamus)
 *       - Elevated temperature enhances immune function (to a point)
 *       - Immune activation generates metabolic heat
 *       - Cytokines modulate neural temperature setpoint
 *       - Neuroinflammation has thermodynamic costs
 *       - ATP availability affects immune response capacity
 *
 * HOW:  - Monitors thermodynamic state (temperature, ATP, heat)
 *       - Tracks immune activation level and cytokine signaling
 *       - Models fever response (setpoint elevation)
 *       - Computes immune function enhancement by temperature
 *       - Integrates metabolic costs of immune response
 *       - Provides bidirectional feedback loops
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * FEVER PHYSIOLOGY:
 * -----------------
 * 1. Pyrogenic Signaling:
 *    - Exogenous pyrogens (bacteria, viruses) -> immune cells
 *    - Immune cells release endogenous pyrogens (IL-1, IL-6, TNF-alpha)
 *    - Pyrogens act on hypothalamic thermostat
 *    - PGE2 elevates temperature setpoint
 *
 * 2. Temperature Setpoint:
 *    - Normal: ~37C (310K)
 *    - Low-grade fever: 37.5-38.5C (310.5-311.5K)
 *    - Moderate fever: 38.5-39.5C (311.5-312.5K)
 *    - High fever: 39.5-41C (312.5-314K)
 *    - Hyperpyrexia: >41C (>314K) - dangerous
 *
 * IMMUNE FUNCTION AND TEMPERATURE:
 * --------------------------------
 * 1. Enhanced Functions at Fever Temperatures:
 *    - Neutrophil chemotaxis (Q10 ~ 1.5)
 *    - Phagocytosis efficiency
 *    - Cytokine production
 *    - Antibody production
 *    - T-cell proliferation
 *
 * 2. Optimal Immune Temperature:
 *    - Most immune functions peak at 38-39C
 *    - Above 40C: protein denaturation begins
 *    - Above 41C: immune function degrades
 *
 * 3. Heat Shock Response:
 *    - HSP (Heat Shock Proteins) activated at fever temps
 *    - HSPs protect proteins and enhance immune function
 *    - Critical for surviving febrile episodes
 *
 * METABOLIC COSTS:
 * ----------------
 * - Fever increases metabolic rate ~13% per degree C
 * - Immune activation is highly ATP-demanding
 * - B-cell activation and antibody production require ATP
 * - Phagocytosis consumes significant ATP
 * - Cytokine production has metabolic cost
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

#ifndef NIMCP_THERMO_IMMUNE_BRIDGE_H
#define NIMCP_THERMO_IMMUNE_BRIDGE_H

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
#define THERMO_IMMUNE_MODULE_NAME           "thermo_immune_bridge"

/** Normal body temperature setpoint (Kelvin) */
#define THERMO_IMMUNE_NORMAL_SETPOINT_K     310.15f

/** Normal body temperature (Celsius) */
#define THERMO_IMMUNE_NORMAL_SETPOINT_C     37.0f

/** Maximum safe fever temperature (Kelvin) */
#define THERMO_IMMUNE_MAX_SAFE_FEVER_K      314.15f

/** Optimal immune function temperature (Kelvin) */
#define THERMO_IMMUNE_OPTIMAL_TEMP_K        311.65f

/** Q10 for immune cell activity */
#define THERMO_IMMUNE_Q10_CELL_ACTIVITY     2.0f

/** Q10 for cytokine production */
#define THERMO_IMMUNE_Q10_CYTOKINE          1.8f

/** Q10 for phagocytosis */
#define THERMO_IMMUNE_Q10_PHAGOCYTOSIS      1.5f

/** Q10 for antibody production */
#define THERMO_IMMUNE_Q10_ANTIBODY          1.6f

/** Metabolic rate increase per degree K */
#define THERMO_IMMUNE_METABOLIC_RATE_PER_K  0.13f

/** ATP cost per immune activation (moles) */
#define THERMO_IMMUNE_ATP_PER_ACTIVATION    1.0e-14f

/** Heat generated per immune event (J) */
#define THERMO_IMMUNE_HEAT_PER_EVENT        1.0e-18f

/** Default update interval (ms) */
#define THERMO_IMMUNE_DEFAULT_UPDATE_MS     100.0f

/** Fever induction delay (ms) */
#define THERMO_IMMUNE_FEVER_DELAY_MS        30000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Fever severity level
 */
typedef enum {
    THERMO_IMMUNE_FEVER_NONE = 0,           /**< No fever (< 37.5C) */
    THERMO_IMMUNE_FEVER_LOW,                /**< Low-grade (37.5-38.5C) */
    THERMO_IMMUNE_FEVER_MODERATE,           /**< Moderate (38.5-39.5C) */
    THERMO_IMMUNE_FEVER_HIGH,               /**< High (39.5-41C) */
    THERMO_IMMUNE_FEVER_DANGEROUS           /**< Hyperpyrexia (>41C) */
} thermo_immune_fever_level_t;

/**
 * @brief Immune activation state
 */
typedef enum {
    THERMO_IMMUNE_STATE_QUIESCENT = 0,      /**< Immune system at rest */
    THERMO_IMMUNE_STATE_ALERT,              /**< Initial pathogen detection */
    THERMO_IMMUNE_STATE_ACTIVATED,          /**< Active immune response */
    THERMO_IMMUNE_STATE_INFLAMED,           /**< Significant inflammation */
    THERMO_IMMUNE_STATE_RESOLVING           /**< Resolution phase */
} thermo_immune_state_t;

/**
 * @brief Cytokine type (simplified)
 */
typedef enum {
    THERMO_IMMUNE_CYTOKINE_IL1 = 0,         /**< Interleukin-1 */
    THERMO_IMMUNE_CYTOKINE_IL6,             /**< Interleukin-6 */
    THERMO_IMMUNE_CYTOKINE_TNF,             /**< Tumor Necrosis Factor */
    THERMO_IMMUNE_CYTOKINE_INTERFERON,      /**< Interferons */
    THERMO_IMMUNE_CYTOKINE_IL10,            /**< Interleukin-10 (anti-inflammatory) */
    THERMO_IMMUNE_CYTOKINE_COUNT
} thermo_immune_cytokine_t;

/**
 * @brief Immune cell type
 */
typedef enum {
    THERMO_IMMUNE_CELL_MICROGLIA = 0,       /**< Brain-resident macrophages */
    THERMO_IMMUNE_CELL_ASTROCYTE,           /**< Astrocyte immune function */
    THERMO_IMMUNE_CELL_TCELL,               /**< T-lymphocytes */
    THERMO_IMMUNE_CELL_BCELL,               /**< B-lymphocytes */
    THERMO_IMMUNE_CELL_COUNT
} thermo_immune_cell_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-immune bridge
 *
 * WHAT: All parameters controlling thermodynamic-immune integration
 * WHY:  Allows tuning fever response and immune function modeling
 * HOW:  Temperature thresholds, Q10 values, metabolic costs
 */
typedef struct {
    /* Temperature setpoints */
    float normal_setpoint_k;                /**< Normal body temperature (K) */
    float max_safe_fever_k;                 /**< Maximum safe fever (K) */
    float optimal_immune_temp_k;            /**< Optimal immune function (K) */

    /* Fever thresholds (Kelvin) */
    float fever_low_threshold_k;            /**< Low-grade fever start */
    float fever_moderate_threshold_k;       /**< Moderate fever start */
    float fever_high_threshold_k;           /**< High fever start */
    float fever_dangerous_threshold_k;      /**< Hyperpyrexia start */

    /* Q10 coefficients */
    float q10_cell_activity;                /**< Q10 for immune cell activity */
    float q10_cytokine;                     /**< Q10 for cytokine production */
    float q10_phagocytosis;                 /**< Q10 for phagocytosis */
    float q10_antibody;                     /**< Q10 for antibody production */

    /* Fever dynamics */
    float fever_induction_delay_ms;         /**< Time to develop fever */
    float fever_resolution_rate;            /**< Rate of fever resolution */
    float max_setpoint_elevation_k;         /**< Maximum setpoint increase */
    float cytokine_fever_sensitivity;       /**< Cytokine -> fever mapping */

    /* Metabolic parameters */
    float metabolic_rate_per_k;             /**< Metabolic increase per degree */
    float atp_per_activation;               /**< ATP cost per activation (moles) */
    float heat_per_event;                   /**< Heat per immune event (J) */

    /* ATP thresholds */
    float atp_full_threshold;               /**< ATP for full immune function */
    float atp_minimal_threshold;            /**< ATP for minimal function */

    /* Immune function parameters */
    float base_immune_activity;             /**< Baseline activity level [0,1] */
    float max_immune_activity;              /**< Maximum activity level [0,1] */
    float activity_decay_rate;              /**< Activity decay without stimulus */

    /* Feature flags */
    bool enable_fever_response;             /**< Model fever response */
    bool enable_immune_enhancement;         /**< Temperature enhances immunity */
    bool enable_metabolic_heat;             /**< Track immune heat generation */
    bool enable_atp_tracking;               /**< Track ATP consumption */
    bool enable_cytokine_signaling;         /**< Model cytokine effects */
    bool enable_hsp_response;               /**< Model heat shock proteins */
    bool enable_thermal_protection;         /**< Protect at dangerous temps */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_immune_config_t;

//=============================================================================
// Immune State Structure
//=============================================================================

/**
 * @brief Current immune-thermodynamic state
 *
 * WHAT: Complete state of immune-thermodynamic integration
 * WHY:  Tracks fever, immune activation, and metabolic effects
 * HOW:  Updated each timestep based on inputs
 */
typedef struct {
    /* Temperature state */
    float current_temp_k;                   /**< Current temperature (K) */
    float temperature_setpoint_k;           /**< Current setpoint (K) */
    float setpoint_elevation_k;             /**< Elevation above normal */
    thermo_immune_fever_level_t fever_level;/**< Current fever level */

    /* Immune state */
    thermo_immune_state_t immune_state;     /**< Current activation state */
    float immune_activity;                  /**< Overall activity level [0,1] */
    float inflammation_level;               /**< Inflammation magnitude [0,1] */

    /* Immune function scaling (temperature-modulated) */
    float cell_activity_factor;             /**< Immune cell activity scaling */
    float cytokine_factor;                  /**< Cytokine production scaling */
    float phagocytosis_factor;              /**< Phagocytosis scaling */
    float antibody_factor;                  /**< Antibody production scaling */
    float overall_immune_factor;            /**< Combined immune scaling */

    /* Cytokine levels (relative, 0-1) */
    float cytokine_levels[THERMO_IMMUNE_CYTOKINE_COUNT];
    float pyrogenic_signal;                 /**< Combined fever signal [0,1] */

    /* Heat shock response */
    float hsp_level;                        /**< Heat shock protein level [0,1] */
    bool hsp_activated;                     /**< HSP response active */

    /* Metabolic state */
    float metabolic_rate_factor;            /**< Metabolic rate multiplier */
    float heat_generation_rate;             /**< Immune heat generation (W) */
    float atp_consumption_rate;             /**< ATP consumption rate */

    /* ATP state */
    float atp_level;                        /**< Current ATP [0,1] */
    float atp_gate;                         /**< ATP gating factor [0,1] */

    /* Protection state */
    bool danger_warning;                    /**< Dangerous temperature warning */
    float protection_factor;                /**< Activity reduction [0,1] */

    /* Timing */
    float fever_duration_s;                 /**< Time in fever state */
    float activation_duration_s;            /**< Time immune activated */
    uint64_t last_update_us;                /**< Last update timestamp */
} thermo_immune_state_data_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t fever_episodes;                /**< Fever episode count */
    uint64_t immune_activations;            /**< Immune activation count */

    /* Temperature stats */
    float min_temp_observed_k;              /**< Minimum temperature */
    float max_temp_observed_k;              /**< Maximum temperature */
    float avg_temp_k;                       /**< Average temperature */
    float peak_fever_k;                     /**< Peak fever temperature */

    /* Fever timing */
    float total_fever_time_s;               /**< Total time febrile */
    float avg_fever_duration_s;             /**< Average fever duration */
    float max_fever_duration_s;             /**< Maximum fever duration */

    /* Fever level breakdown */
    uint64_t time_no_fever_us;              /**< Time without fever */
    uint64_t time_low_fever_us;             /**< Time in low-grade fever */
    uint64_t time_moderate_fever_us;        /**< Time in moderate fever */
    uint64_t time_high_fever_us;            /**< Time in high fever */
    uint64_t time_dangerous_us;             /**< Time in dangerous range */

    /* Immune function stats */
    float avg_immune_activity;              /**< Average immune activity */
    float peak_immune_activity;             /**< Peak immune activity */
    float avg_immune_factor;                /**< Average temp enhancement */

    /* Metabolic stats */
    double total_immune_heat_j;             /**< Total heat from immunity */
    double total_atp_consumed;              /**< Total ATP consumed (moles) */
    float avg_metabolic_factor;             /**< Average metabolic multiplier */

    /* HSP stats */
    uint64_t hsp_activations;               /**< HSP activation count */
    float total_hsp_time_s;                 /**< Total HSP active time */

    /* Warning stats */
    uint64_t danger_warnings;               /**< Dangerous temp warnings */

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_immune_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_immune_bridge_struct thermo_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with physiological defaults
 * WHY:  Simplifies bridge creation
 * HOW:  Sets typical fever thresholds, Q10 values
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_default_config(
    thermo_immune_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-immune bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables thermodynamic-immune integration
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_immune_bridge_t* thermo_immune_bridge_create(
    const thermo_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_immune_bridge_destroy(
    thermo_immune_bridge_t* bridge
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to thermodynamic state
 *
 * WHAT: Link bridge to thermodynamics module
 * WHY:  Enables bidirectional temperature/immune tracking
 * HOW:  Stores reference to thermodynamic state
 *
 * @param bridge    Bridge handle
 * @param thermo    Thermodynamic state to monitor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_connect_thermo(
    thermo_immune_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of immune-thermodynamic state
 * WHY:  Processes fever dynamics, immune enhancement
 * HOW:  Reads temperature, updates immune function scaling
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_update(
    thermo_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_set_temperature(
    thermo_immune_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set immune activation state
 *
 * WHAT: Trigger or update immune activation
 * WHY:  External immune stimulation
 * HOW:  Sets activation level, initiates fever if enabled
 *
 * @param bridge        Bridge handle
 * @param activation    Activation level [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_set_activation(
    thermo_immune_bridge_t* bridge,
    float activation
);

/**
 * @brief Set cytokine level
 *
 * WHAT: Set level of specific cytokine
 * WHY:  Model cytokine signaling effects
 * HOW:  Updates cytokine, recalculates pyrogenic signal
 *
 * @param bridge    Bridge handle
 * @param cytokine  Cytokine type
 * @param level     Level [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_set_cytokine(
    thermo_immune_bridge_t* bridge,
    thermo_immune_cytokine_t cytokine,
    float level
);

/**
 * @brief Induce fever response
 *
 * WHAT: Trigger fever with specified magnitude
 * WHY:  External fever induction (pyrogen exposure)
 * HOW:  Elevates temperature setpoint
 *
 * @param bridge        Bridge handle
 * @param magnitude     Fever magnitude [0,1] (0=none, 1=max)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_induce_fever(
    thermo_immune_bridge_t* bridge,
    float magnitude
);

/**
 * @brief Resolve fever
 *
 * WHAT: Begin fever resolution process
 * WHY:  Model fever breaking/antipyretic effect
 * HOW:  Gradually returns setpoint to normal
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_resolve_fever(
    thermo_immune_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_reset(thermo_immune_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current immune-thermodynamic state
 *
 * WHAT: Retrieve complete immune-thermodynamic state
 * WHY:  For monitoring and simulation
 * HOW:  Copies current state to output
 *
 * @param bridge    Bridge handle
 * @param state     Output state structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_get_state(
    const thermo_immune_bridge_t* bridge,
    thermo_immune_state_data_t* state
);

/**
 * @brief Get current fever level
 *
 * @param bridge    Bridge handle
 * @return Current fever level
 */
NIMCP_EXPORT thermo_immune_fever_level_t thermo_immune_get_fever_level(
    const thermo_immune_bridge_t* bridge
);

/**
 * @brief Get immune function scaling factor
 *
 * WHAT: Get overall temperature enhancement of immunity
 * WHY:  Apply to immune system simulation
 * HOW:  Returns combined scaling factor
 *
 * @param bridge    Bridge handle
 * @return Immune function scaling [0,1+]
 */
NIMCP_EXPORT float thermo_immune_get_immune_factor(
    const thermo_immune_bridge_t* bridge
);

/**
 * @brief Get specific immune function scaling
 *
 * @param bridge    Bridge handle
 * @param cell_type Cell/function type
 * @return Specific function scaling
 */
NIMCP_EXPORT float thermo_immune_get_cell_factor(
    const thermo_immune_bridge_t* bridge,
    thermo_immune_cell_t cell_type
);

/**
 * @brief Get current temperature setpoint
 *
 * @param bridge    Bridge handle
 * @return Current setpoint (K)
 */
NIMCP_EXPORT float thermo_immune_get_setpoint(
    const thermo_immune_bridge_t* bridge
);

/**
 * @brief Check if fever is active
 *
 * @param bridge    Bridge handle
 * @return true if fever is active
 */
NIMCP_EXPORT bool thermo_immune_is_febrile(
    const thermo_immune_bridge_t* bridge
);

/**
 * @brief Check if temperature is dangerous
 *
 * @param bridge    Bridge handle
 * @return true if temperature is dangerous
 */
NIMCP_EXPORT bool thermo_immune_is_dangerous(
    const thermo_immune_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_get_stats(
    const thermo_immune_bridge_t* bridge,
    thermo_immune_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_immune_reset_stats(
    thermo_immune_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert Celsius to Kelvin
 *
 * @param celsius   Temperature in Celsius
 * @return Temperature in Kelvin
 */
NIMCP_EXPORT float thermo_immune_celsius_to_kelvin(float celsius);

/**
 * @brief Convert Kelvin to Celsius
 *
 * @param kelvin    Temperature in Kelvin
 * @return Temperature in Celsius
 */
NIMCP_EXPORT float thermo_immune_kelvin_to_celsius(float kelvin);

/**
 * @brief Get fever level name
 *
 * @param level Fever level
 * @return Level name string
 */
NIMCP_EXPORT const char* thermo_immune_fever_name(
    thermo_immune_fever_level_t level
);

/**
 * @brief Get immune state name
 *
 * @param state Immune state
 * @return State name string
 */
NIMCP_EXPORT const char* thermo_immune_state_name(
    thermo_immune_state_t state
);

/**
 * @brief Get cytokine name
 *
 * @param cytokine  Cytokine type
 * @return Cytokine name string
 */
NIMCP_EXPORT const char* thermo_immune_cytokine_name(
    thermo_immune_cytokine_t cytokine
);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_immune_print_summary(
    const thermo_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_IMMUNE_BRIDGE_H */