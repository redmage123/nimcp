/**
 * @file nimcp_cortical_layers_sleep_bridge.h
 * @brief Sleep-Cortical Layers Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and cortical layers
 * WHY:  Cortical layers show distinct activity patterns across sleep stages
 * HOW:  Sleep state modulates layer-specific activity, connectivity, and plasticity
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: All layers active, feedforward and feedback balanced
 *   * Layer IV: Strong thalamic input, high granular activity
 *   * Layer II/III: Active lateral integration and cortico-cortical communication
 *   * Layer V: Active motor output and decision signals
 *   * Layer VI: Active corticothalamic feedback
 *
 * - DROWSY: Reduced activity, beginning of slow oscillations
 *   * Layer IV: Reduced thalamic input responsiveness
 *   * Layer II/III: Decreased lateral integration
 *   * Layer V: Reduced output gain
 *   * Emergence of slow wave precursors
 *
 * - LIGHT_NREM: Spindles and K-complexes in layers II/III and V
 *   * Layer IV: Minimal thalamic processing
 *   * Layer II/III: Sleep spindles (12-15Hz bursts)
 *   * Layer V: K-complex generation
 *   * Layer VI: Maintained corticothalamic loops
 *
 * - DEEP_NREM: Slow oscillations (0.5-1Hz) synchronized across layers
 *   * All layers: UP states (depolarized) alternate with DOWN states (hyperpolarized)
 *   * Layer V: Strong burst firing during UP states
 *   * Layer VI: Deep hyperpolarization during DOWN states
 *   * Memory consolidation through coordinated replay
 *
 * - REM: Layer-specific activation resembling waking patterns
 *   * Layer II/III: High activity (similar to awake)
 *   * Layer V: Phasic activation with bursts
 *   * Layer VI: Active but with altered predictive signals
 *   * Dissociation between cortical layers and subcortical motor inhibition
 *
 * SLEEP EFFECTS ON LAMINAR PROCESSING:
 * - Feedforward flow (IV → II/III → V): Reduced during sleep, offline in deep NREM
 * - Feedback flow (VI → IV, I → II/III): Enhanced during NREM for consolidation
 * - Lateral connectivity (II/III): Reduced in NREM, active in REM
 * - Layer excitability: Modulated by sleep stage-specific neuromodulators
 *
 * REFERENCES:
 * - Steriade et al. (1993) "Sleep oscillations and their blockage by activating systems"
 * - Massimini et al. (2004) "The sleep slow oscillation as a traveling wave"
 * - Destexhe et al. (2007) "Are corticothalamic 'up' states fragments of wakefulness?"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_LAYERS_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_LAYERS_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

/* Layer activity modulation factors by sleep state */
#define LAYERS_SLEEP_ACTIVITY_AWAKE          1.0f
#define LAYERS_SLEEP_ACTIVITY_DROWSY         0.7f
#define LAYERS_SLEEP_ACTIVITY_LIGHT_NREM     0.3f
#define LAYERS_SLEEP_ACTIVITY_DEEP_NREM      0.1f  /* UP/DOWN oscillations */
#define LAYERS_SLEEP_ACTIVITY_REM            0.9f  /* REM paradox: high activity */

/* Feedforward/feedback balance */
#define LAYERS_SLEEP_FF_BALANCE_AWAKE        1.0f  /* Balanced */
#define LAYERS_SLEEP_FF_BALANCE_NREM         0.3f  /* Reduced feedforward */
#define LAYERS_SLEEP_FF_BALANCE_REM          0.8f  /* Mostly restored */

#define LAYERS_SLEEP_FB_BALANCE_AWAKE        1.0f  /* Balanced */
#define LAYERS_SLEEP_FB_BALANCE_NREM         1.5f  /* Enhanced feedback for consolidation */
#define LAYERS_SLEEP_FB_BALANCE_REM          1.1f  /* Slightly enhanced */

/* Layer-specific modulation multipliers */
#define LAYERS_SLEEP_L4_NREM_FACTOR          0.05f /* Layer IV nearly silent in NREM */
#define LAYERS_SLEEP_L23_NREM_FACTOR         0.15f /* Layer II/III reduced but spindles */
#define LAYERS_SLEEP_L5_NREM_FACTOR          0.20f /* Layer V bursts during UP states */
#define LAYERS_SLEEP_L6_NREM_FACTOR          0.10f /* Layer VI deep hyperpolarization */

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Configuration for cortical layers sleep bridge
 * WHY:  Allow customization of sleep effects on laminar processing
 * HOW:  Enable/disable specific modulations and set strength
 */
typedef struct {
    bool enable_activity_modulation;     /**< Modulate layer activity levels */
    bool enable_connectivity_modulation; /**< Modulate inter-layer connectivity */
    bool enable_layer_specific;          /**< Apply layer-specific effects */
    bool enable_slow_oscillations;       /**< Enable UP/DOWN states in deep NREM */
    float modulation_strength;           /**< Overall modulation strength [0.0-1.0] */
    float slow_oscillation_frequency;    /**< Slow wave frequency in Hz [0.5-1.0] */
} cortical_layers_sleep_config_t;

/**
 * WHAT: Layer-specific sleep effects
 * WHY:  Different layers respond differently to sleep states
 * HOW:  Per-layer activity and connectivity modulation factors
 */
typedef struct {
    float layer_activity[CC_LAYER_COUNT];      /**< Activity factor per layer [0.0-1.0] */
    float layer_excitability[CC_LAYER_COUNT];  /**< Excitability modulation */
    float layer_connectivity[CC_LAYER_COUNT];  /**< Connectivity strength modulation */
} layer_sleep_effects_t;

/**
 * WHAT: Computed sleep effects on cortical layers
 * WHY:  Centralized state for all sleep-induced changes
 * HOW:  Updated on sleep state transitions via callback
 */
typedef struct {
    sleep_state_t current_state;           /**< Current sleep state */
    float global_activity_factor;          /**< Global activity modulation */
    float feedforward_balance;             /**< FF pathway strength [0.0-1.0] */
    float feedback_balance;                /**< FB pathway strength [0.0-1.0+] */
    float lateral_connectivity;            /**< Lateral connection strength */
    layer_sleep_effects_t layer_effects;   /**< Layer-specific effects */
    bool in_up_state;                      /**< Deep NREM UP/DOWN state */
    float slow_oscillation_phase;          /**< Current phase [0.0-2π] */
    float sleep_pressure;                  /**< Current sleep pressure [0.0-1.0] */
    bool layers_offline;                   /**< Layers in offline mode */
} cortical_layers_sleep_effects_t;

/**
 * WHAT: Opaque handle to cortical layers sleep bridge
 * WHY:  Encapsulation of internal state
 * HOW:  Forward declaration, implementation in .c file
 */
typedef struct cortical_layers_sleep_bridge_struct* cortical_layers_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default configuration for cortical layers sleep bridge
 * WHY:  Provide biologically realistic starting parameters
 * HOW:  Initialize config struct with sensible defaults
 *
 * @param config Configuration structure to initialize (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_layers_sleep_default_config(cortical_layers_sleep_config_t* config);

/**
 * WHAT: Create cortical layers sleep bridge
 * WHY:  Initialize bidirectional sleep-cortical layers integration
 * HOW:  Allocate bridge, register callback with sleep system
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param layers Laminar structure to integrate with (must be non-NULL)
 * @param sleep Sleep system (must be non-NULL)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - NULL layers or sleep
 * - Allocation failure
 * - Callback registration failure
 */
cortical_layers_sleep_bridge_t cortical_layers_sleep_bridge_create(
    const cortical_layers_sleep_config_t* config,
    laminar_structure_t* layers,
    sleep_system_t sleep);

/**
 * WHAT: Destroy cortical layers sleep bridge
 * WHY:  Clean up resources and unregister callbacks
 * HOW:  Unregister from sleep system, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void cortical_layers_sleep_bridge_destroy(cortical_layers_sleep_bridge_t bridge);

/* ========================================================================
 * STATE UPDATE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Update sleep effects on cortical layers
 * WHY:  Recompute effects when state or pressure changes
 * HOW:  Query sleep system, update internal effects structure
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(L) where L = number of layers
 * THREAD-SAFE: Yes
 *
 * NOTE: Called automatically via callback, but can be called manually
 */
int cortical_layers_sleep_update(cortical_layers_sleep_bridge_t bridge);

/**
 * WHAT: Apply sleep modulation to laminar structure
 * WHY:  Actively modify layer parameters based on sleep state
 * HOW:  Update layer connectivity, excitability, and activity factors
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(L²) where L = number of layers (connectivity updates)
 * THREAD-SAFE: Yes
 *
 * NOTE: Should be called regularly during processing loop
 */
int cortical_layers_sleep_apply_modulation(cortical_layers_sleep_bridge_t bridge);

/* ========================================================================
 * STATE ACCESS FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get current sleep effects on cortical layers
 * WHY:  Allow external modules to query sleep-induced changes
 * HOW:  Copy internal effects structure to output parameter
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @param effects Output structure (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(L) where L = number of layers
 * THREAD-SAFE: Yes
 */
int cortical_layers_sleep_get_effects(
    const cortical_layers_sleep_bridge_t bridge,
    cortical_layers_sleep_effects_t* effects);

/**
 * WHAT: Get global activity factor for cortical layers
 * WHY:  Quick access to overall activity modulation
 * HOW:  Return cached global factor
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return Activity factor [0.0-1.0] or -1.0f on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_layers_sleep_get_activity_factor(const cortical_layers_sleep_bridge_t bridge);

/**
 * WHAT: Check if cortical layers are in offline mode
 * WHY:  Determine if layers should process external input
 * HOW:  Return offline flag (true during deep NREM)
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return true if offline, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cortical_layers_sleep_is_offline(const cortical_layers_sleep_bridge_t bridge);

/**
 * WHAT: Check if currently in UP state (deep NREM)
 * WHY:  Coordinate processing with slow oscillations
 * HOW:  Return UP state flag
 *
 * @param bridge Bridge handle (must be non-NULL)
 * @return true if in UP state, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cortical_layers_sleep_is_up_state(const cortical_layers_sleep_bridge_t bridge);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get activity factor for a given sleep state
 * WHY:  Provide sleep state → activity mapping for external use
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Activity factor [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_layers_sleep_activity_for_state(sleep_state_t state);

/**
 * WHAT: Get feedforward balance for a given sleep state
 * WHY:  Provide sleep state → FF balance mapping
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Feedforward balance factor [0.0-1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_layers_sleep_feedforward_for_state(sleep_state_t state);

/**
 * WHAT: Get feedback balance for a given sleep state
 * WHY:  Provide sleep state → FB balance mapping
 * HOW:  Lookup table based on sleep state
 *
 * @param state Sleep state
 * @return Feedback balance factor [0.0-1.5+]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_layers_sleep_feedback_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_LAYERS_SLEEP_BRIDGE_H */
