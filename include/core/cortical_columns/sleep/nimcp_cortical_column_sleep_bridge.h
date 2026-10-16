/**
 * @file nimcp_cortical_column_sleep_bridge.h
 * @brief Sleep-Cortical Column Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and cortical columns
 * WHY:  Minicolumns and hypercolumns show state-dependent activity and competition
 * HOW:  Sleep state modulates column activation, lateral inhibition, and competition
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Strong competition, winner-take-all or sparse coding
 *   * Minicolumns: High selectivity, sharp tuning curves
 *   * Lateral inhibition: Strong Mexican hat profile
 *   * Competition: Active to maintain sparse representations
 *
 * - DROWSY: Reduced competition, broader tuning
 *   * Minicolumns: Reduced selectivity, wider tuning
 *   * Lateral inhibition: Weakened
 *   * Competition: Softmax transitions to broader distribution
 *
 * - LIGHT_NREM: Minimal external responsiveness
 *   * Minicolumns: Reduced receptive field sensitivity
 *   * Spindles: Synchronized bursts across columns
 *   * Competition: Minimal, allowing distributed activity
 *
 * - DEEP_NREM: Slow oscillations dominate
 *   * UP states: Coordinated activation across columns
 *   * DOWN states: Global suppression
 *   * Lateral inhibition: Suspended during slow waves
 *   * Memory replay: Reactivation of column assemblies
 *
 * - REM: Dream-like activation patterns
 *   * Minicolumns: Internal activation (not stimulus-driven)
 *   * Competition: Active but with altered dynamics
 *   * Hallucinatory patterns from spontaneous column activation
 *
 * SLEEP EFFECTS ON COLUMNAR PROCESSING:
 * - Receptive field gain: Reduced during sleep, minimal in deep NREM
 * - Lateral inhibition strength: Weakened in NREM, restored in REM
 * - Competition dynamics: WTA → Softmax → None across sleep deepening
 * - Activation threshold: Increased during sleep (less responsive)
 *
 * REFERENCES:
 * - Steriade & Timofeev (2003) "Neuronal plasticity in thalamocortical networks"
 * - Destexhe et al. (1999) "Spatiotemporal patterns of spindle oscillations"
 * - Peyrache et al. (2009) "Replay of rule-learning in rat prefrontal cortex"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_COLUMN_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_COLUMN_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

/* Receptive field gain by sleep state */
#define COLUMN_SLEEP_RF_GAIN_AWAKE          1.0f
#define COLUMN_SLEEP_RF_GAIN_DROWSY         0.7f
#define COLUMN_SLEEP_RF_GAIN_LIGHT_NREM     0.2f
#define COLUMN_SLEEP_RF_GAIN_DEEP_NREM      0.05f
#define COLUMN_SLEEP_RF_GAIN_REM            0.3f  /* Internal activation */

/* Lateral inhibition strength */
#define COLUMN_SLEEP_INHIB_AWAKE            1.0f
#define COLUMN_SLEEP_INHIB_DROWSY           0.7f
#define COLUMN_SLEEP_INHIB_NREM             0.2f  /* Weak during NREM */
#define COLUMN_SLEEP_INHIB_REM              0.8f

/* Competition temperature (higher = softer competition) */
#define COLUMN_SLEEP_TEMP_AWAKE             1.0f  /* Sharp WTA */
#define COLUMN_SLEEP_TEMP_DROWSY            2.0f  /* Softer */
#define COLUMN_SLEEP_TEMP_NREM              5.0f  /* Very soft */
#define COLUMN_SLEEP_TEMP_REM               1.5f  /* Moderate */

/* Activation threshold multipliers */
#define COLUMN_SLEEP_THRESH_AWAKE           1.0f
#define COLUMN_SLEEP_THRESH_DROWSY          1.2f
#define COLUMN_SLEEP_THRESH_LIGHT_NREM      1.5f
#define COLUMN_SLEEP_THRESH_DEEP_NREM       2.0f
#define COLUMN_SLEEP_THRESH_REM             1.1f

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Configuration for cortical column sleep bridge
 * WHY:  Allow customization of sleep effects on columnar processing
 * HOW:  Enable/disable specific modulations and set strength
 */
typedef struct {
    bool enable_receptive_field_modulation; /**< Modulate RF gain */
    bool enable_inhibition_modulation;      /**< Modulate lateral inhibition */
    bool enable_competition_modulation;     /**< Modulate competition dynamics */
    bool enable_threshold_modulation;       /**< Modulate activation threshold */
    float modulation_strength;              /**< Overall modulation strength [0.0-1.0] */
} cortical_column_sleep_config_t;

/**
 * WHAT: Computed sleep effects on cortical columns
 * WHY:  Centralized state for all sleep-induced changes
 * HOW:  Updated on sleep state transitions via callback
 */
typedef struct {
    sleep_state_t current_state;           /**< Current sleep state */
    float receptive_field_gain;            /**< RF sensitivity factor */
    float lateral_inhibition_strength;     /**< Inhibition strength */
    float competition_temperature;         /**< Competition softness */
    float activation_threshold_factor;     /**< Threshold multiplier */
    cc_competition_mode_t competition_mode; /**< Current competition mode */
    bool columns_offline;                  /**< Offline during deep NREM */
    bool in_spindle;                       /**< Currently in sleep spindle */
    float sleep_pressure;                  /**< Current sleep pressure */
} cortical_column_sleep_effects_t;

/**
 * WHAT: Opaque handle to cortical column sleep bridge
 * WHY:  Encapsulation of internal state
 * HOW:  Forward declaration, implementation in .c file
 */
typedef struct cortical_column_sleep_bridge_struct* cortical_column_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default configuration for cortical column sleep bridge
 * WHY:  Provide biologically realistic starting parameters
 * HOW:  Initialize config struct with sensible defaults
 *
 * @param config Configuration structure to initialize (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_column_sleep_default_config(cortical_column_sleep_config_t* config);

/**
 * WHAT: Create cortical column sleep bridge
 * WHY:  Initialize bidirectional sleep-column integration
 * HOW:  Allocate bridge, register callback with sleep system
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param hypercolumn Hypercolumn to integrate with (must be non-NULL)
 * @param sleep Sleep system (must be non-NULL)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL hypercolumn or sleep
 * - Allocation failure
 * - Callback registration failure
 */
cortical_column_sleep_bridge_t cortical_column_sleep_bridge_create(
    const cortical_column_sleep_config_t* config,
    hypercolumn_t* hypercolumn,
    sleep_system_t sleep);

/**
 * WHAT: Destroy cortical column sleep bridge
 * WHY:  Clean up resources and unregister callbacks
 * HOW:  Unregister from sleep system, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void cortical_column_sleep_bridge_destroy(cortical_column_sleep_bridge_t bridge);

/* ========================================================================
 * STATE UPDATE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Update sleep effects on cortical columns
 * WHY:  Recompute effects when state or pressure changes
 * HOW:  Query sleep system, update internal effects structure
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * NOTE: Called automatically via callback, but can be called manually
 */
int cortical_column_sleep_update(cortical_column_sleep_bridge_t bridge);

/**
 * WHAT: Apply sleep modulation to hypercolumn
 * WHY:  Actively modify column parameters based on sleep state
 * HOW:  Update competition mode, inhibition, and thresholds
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(M) where M = number of minicolumns
 * THREAD-SAFE: Yes
 *
 * NOTE: Should be called before hypercolumn_compute()
 */
int cortical_column_sleep_apply_modulation(cortical_column_sleep_bridge_t bridge);

/* ========================================================================
 * STATE ACCESS FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get current sleep effects on cortical columns
 * WHY:  Allow external modules to query sleep-induced changes
 * HOW:  Copy internal effects structure to output parameter
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @param effects Output structure (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_column_sleep_get_effects(
    const cortical_column_sleep_bridge_t bridge,
    cortical_column_sleep_effects_t* effects);

/**
 * WHAT: Get receptive field gain factor
 * WHY:  Quick access to RF sensitivity modulation
 * HOW:  Return cached gain factor
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return RF gain factor [0.0-1.0] or -1.0f on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_column_sleep_get_rf_gain(const cortical_column_sleep_bridge_t bridge);

/**
 * WHAT: Get lateral inhibition strength
 * WHY:  Quick access to inhibition modulation
 * HOW:  Return cached inhibition strength
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return Inhibition strength [0.0-1.0] or -1.0f on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_column_sleep_get_inhibition(const cortical_column_sleep_bridge_t bridge);

/**
 * WHAT: Check if cortical columns are in offline mode
 * WHY:  Determine if columns should respond to external input
 * HOW:  Return offline flag (true during deep NREM)
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return true if offline, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cortical_column_sleep_is_offline(const cortical_column_sleep_bridge_t bridge);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get RF gain for a given sleep state
 * WHY:  Provide sleep state → RF gain mapping for external use
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return RF gain factor [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_column_sleep_rf_gain_for_state(sleep_state_t state);

/**
 * WHAT: Get lateral inhibition for a given sleep state
 * WHY:  Provide sleep state → inhibition mapping
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Inhibition strength [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_column_sleep_inhibition_for_state(sleep_state_t state);

/**
 * WHAT: Get competition temperature for a given sleep state
 * WHY:  Provide sleep state → competition temperature mapping
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Competition temperature [1.0-5.0+]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_column_sleep_temperature_for_state(sleep_state_t state);

/**
 * WHAT: Get competition mode for a given sleep state
 * WHY:  Provide sleep state → competition mode mapping
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Competition mode
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
cc_competition_mode_t cortical_column_sleep_competition_mode_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_COLUMN_SLEEP_BRIDGE_H */
