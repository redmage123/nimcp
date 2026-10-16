//=============================================================================
// nimcp_pr_hypo_bridge.h - Prime Resonant Hypothalamus Bridge
//=============================================================================
/**
 * @file nimcp_pr_hypo_bridge.h
 * @brief Neuromodulator-quaternion mapping for PR Memory System
 *
 * WHAT: Hypothalamic neuromodulator integration with Prime Resonant memory
 * WHY:  Neuromodulators (dopamine, serotonin, norepinephrine, etc.) critically
 *       regulate memory encoding, consolidation, and retrieval strength
 * HOW:  Maps neuromodulator concentrations to quaternion state modulations,
 *       implements stress-based consolidation effects and reward signaling
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Hypothalamic-Memory Axis:
 *   +-----------------------------------------------------------------------+
 *   |  The hypothalamus coordinates neuroendocrine responses that modulate |
 *   |  memory function throughout the brain:                               |
 *   |                                                                       |
 *   |  +-------------------+                                                |
 *   |  |   Hypothalamus    |                                                |
 *   |  |  (Neuromodulator  |                                                |
 *   |  |     Hub)          |                                                |
 *   |  +--------+----------+                                                |
 *   |           |                                                           |
 *   |  +--------v----------+  +------------------+  +-------------------+   |
 *   |  | Dopamine Release  |  | Serotonin        |  | Norepinephrine    |   |
 *   |  | (VTA/Substantia   |  | (Raphe Nuclei)   |  | (Locus Coeruleus) |   |
 *   |  |  Nigra)           |  |                  |  |                   |   |
 *   |  +--------+----------+  +--------+---------+  +---------+---------+   |
 *   |           |                      |                      |             |
 *   |           v                      v                      v             |
 *   |  +--------+----------+  +--------+---------+  +---------+---------+   |
 *   |  | Reward Prediction |  | Mood/Emotional   |  | Arousal/Attention |   |
 *   |  | Memory Encoding   |  | Memory Valence   |  | Memory Salience   |   |
 *   |  +-------------------+  +------------------+  +-------------------+   |
 *   +-----------------------------------------------------------------------+
 *
 *   Neuromodulator-Quaternion Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Neuromodulator    | Quaternion Effect           | Memory Impact      |
 *   |--------------------|-----------------------------|--------------------|
 *   |  Dopamine (DA)     | Increases w (consolidation) | Reward encoding    |
 *   |                    | Modulates z (accessibility) | Habit formation    |
 *   |  Serotonin (5-HT)  | Modulates x (emotion)       | Mood valence       |
 *   |                    | Stabilizes w (consolidation)| Emotional memory   |
 *   |  Norepinephrine    | Increases y (salience)      | Arousal response   |
 *   |  (NE)              | Boosts z (accessibility)    | Flashbulb memory   |
 *   |  Acetylcholine     | Sharpens z (accessibility)  | Attention focus    |
 *   |  (ACh)             | Modulates y (salience)      | Learning readiness |
 *   |  Cortisol          | Inverted-U on w             | Stress response    |
 *   |  (Stress hormone)  | High levels impair memory   | Consolidation      |
 *   |  Oxytocin          | Positive x (emotion)        | Social memory      |
 *   |                    | Increases entanglement      | Bonding memories   |
 *   +-----------------------------------------------------------------------+
 *
 *   Stress-Memory Yerkes-Dodson Curve:
 *   +-----------------------------------------------------------------------+
 *   |  Memory Performance                                                   |
 *   |        ^                                                              |
 *   |        |           ***                                                |
 *   |        |         **   **    <- Optimal stress level                   |
 *   |        |        *       *                                             |
 *   |        |       *         *                                            |
 *   |        |      *           *                                           |
 *   |        |     *             *   <- High stress impairs                 |
 *   |        |    *               *                                         |
 *   |        |   *                 *                                        |
 *   |        +---|-----------------|-------------------------------->       |
 *   |          Low    Moderate    High     Cortisol Level                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Reward-Memory Integration:
 *   +-----------------------------------------------------------------------+
 *   |  1. Prediction Error: DA release when outcome > expected             |
 *   |  2. Memory Tag: High DA marks memory for consolidation               |
 *   |  3. Replay Priority: DA-tagged memories preferentially replayed      |
 *   |  4. Z-Ladder Boost: DA accelerates promotion to higher tiers         |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Neuromodulator application: O(1)
 * - Quaternion mapping: ~20ns per neuromodulator
 * - Stress modulation: O(1)
 * - Reward signal processing: O(1)
 *
 * MEMORY:
 * - pr_hypo_bridge_t: ~1KB base structure
 * - Neuromodulator history buffer: configurable
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks invoked without lock held
 *
 * INTEGRATION:
 * - Core: PR memory nodes, quaternion states
 * - Bio-Async: Neuromodulator channels
 * - Z-Ladder: Tier promotion modulation
 *
 * REFERENCES:
 * - McGaugh (2004): Memory consolidation and the amygdala
 * - Roozendaal & McGaugh (2011): Memory modulation by stress hormones
 * - Schultz (1998): Dopamine reward prediction error
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_HYPO_BRIDGE_H
#define NIMCP_PR_HYPO_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Number of neuromodulator types */
#define PR_HYPO_NEUROMOD_COUNT          8

/** Default history buffer size */
#define PR_HYPO_DEFAULT_HISTORY_SIZE    256

/** Maximum neuromodulator concentration (normalized) */
#define PR_HYPO_MAX_CONCENTRATION       1.0f

/** Minimum neuromodulator concentration (normalized) */
#define PR_HYPO_MIN_CONCENTRATION       0.0f

/** Baseline neuromodulator level */
#define PR_HYPO_BASELINE_LEVEL          0.5f

/** Optimal stress level (Yerkes-Dodson peak) */
#define PR_HYPO_OPTIMAL_STRESS          0.5f

/** Stress decay rate per second */
#define PR_HYPO_STRESS_DECAY_RATE       0.1f

/** Reward signal decay rate per second */
#define PR_HYPO_REWARD_DECAY_RATE       0.2f

/** Default dopamine reward boost factor */
#define PR_HYPO_DA_REWARD_BOOST         0.3f

/** Default norepinephrine salience boost */
#define PR_HYPO_NE_SALIENCE_BOOST       0.4f

/** Default serotonin mood modulation factor */
#define PR_HYPO_5HT_MOOD_FACTOR         0.25f

/** Default acetylcholine attention factor */
#define PR_HYPO_ACH_ATTENTION_FACTOR    0.35f

/** Cortisol impairment threshold (above this impairs memory) */
#define PR_HYPO_CORTISOL_IMPAIR_THRESH  0.7f

/** Epsilon for floating-point comparisons */
#define PR_HYPO_EPSILON                 1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Neuromodulator types
 *
 * WHAT: Categories of neuromodulators affecting memory
 * WHY:  Each neuromodulator has distinct effects on memory encoding/retrieval
 * HOW:  Enum maps to specific quaternion modulation functions
 */
typedef enum {
    PR_NEUROMOD_DOPAMINE = 0,       /**< Dopamine - reward, motivation */
    PR_NEUROMOD_SEROTONIN,          /**< Serotonin (5-HT) - mood, emotional memory */
    PR_NEUROMOD_NOREPINEPHRINE,     /**< Norepinephrine - arousal, attention */
    PR_NEUROMOD_ACETYLCHOLINE,      /**< Acetylcholine - attention, learning */
    PR_NEUROMOD_CORTISOL,           /**< Cortisol - stress response */
    PR_NEUROMOD_OXYTOCIN,           /**< Oxytocin - social bonding */
    PR_NEUROMOD_ENDORPHIN,          /**< Endorphins - pain/pleasure modulation */
    PR_NEUROMOD_GABA,               /**< GABA - inhibitory, calming */
    PR_NEUROMOD_COUNT               /**< Number of neuromodulator types */
} pr_neuromod_type_t;

/**
 * @brief Reward signal types
 *
 * WHAT: Categories of reward/punishment signals
 * WHY:  Different rewards have different memory encoding effects
 */
typedef enum {
    PR_REWARD_POSITIVE = 0,         /**< Positive reward (DA boost) */
    PR_REWARD_NEGATIVE,             /**< Negative/aversive (NE boost) */
    PR_REWARD_NEUTRAL,              /**< Neutral outcome */
    PR_REWARD_PREDICTION_ERROR,     /**< Unexpected outcome (learning signal) */
    PR_REWARD_SOCIAL,               /**< Social reward (oxytocin boost) */
    PR_REWARD_TYPE_COUNT            /**< Number of reward types */
} pr_reward_type_t;

/**
 * @brief Stress response states
 *
 * WHAT: Categories of stress levels
 * WHY:  Stress has inverted-U effect on memory (Yerkes-Dodson)
 */
typedef enum {
    PR_STRESS_LOW = 0,              /**< Low stress - suboptimal arousal */
    PR_STRESS_OPTIMAL,              /**< Optimal stress - peak performance */
    PR_STRESS_HIGH,                 /**< High stress - impaired consolidation */
    PR_STRESS_ACUTE,                /**< Acute stress - flashbulb encoding */
    PR_STRESS_CHRONIC               /**< Chronic stress - memory impairment */
} pr_stress_state_t;

/**
 * @brief Hypothalamus bridge error codes
 */
typedef enum {
    PR_HYPO_SUCCESS = 0,                    /**< Operation succeeded */
    PR_HYPO_ERROR_NULL_POINTER = -1,        /**< NULL pointer argument */
    PR_HYPO_ERROR_INVALID_NEUROMOD = -2,    /**< Invalid neuromodulator type */
    PR_HYPO_ERROR_NO_MEMORY = -3,           /**< Memory allocation failed */
    PR_HYPO_ERROR_NOT_INITIALIZED = -4,     /**< Bridge not initialized */
    PR_HYPO_ERROR_INVALID_CONFIG = -5,      /**< Invalid configuration */
    PR_HYPO_ERROR_OUT_OF_RANGE = -6,        /**< Value out of valid range */
    PR_HYPO_ERROR_BUFFER_FULL = -7          /**< History buffer full */
} pr_hypo_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Neuromodulator state
 *
 * WHAT: Current state of a single neuromodulator
 * WHY:  Track concentration, velocity, and effects
 */
typedef struct {
    pr_neuromod_type_t type;        /**< Neuromodulator type */
    float concentration;            /**< Current concentration [0, 1] */
    float baseline;                 /**< Baseline concentration */
    float velocity;                 /**< Rate of change per second */
    float decay_rate;               /**< Return-to-baseline rate */
    uint64_t last_update_ms;        /**< Last update timestamp */
} pr_neuromod_state_t;

/**
 * @brief Neuromodulator-quaternion mapping parameters
 *
 * WHAT: Parameters for mapping neuromodulator to quaternion effects
 * WHY:  Customize how each neuromodulator affects memory state
 */
typedef struct {
    float w_effect;                 /**< Effect on consolidation component */
    float x_effect;                 /**< Effect on emotional component */
    float y_effect;                 /**< Effect on salience component */
    float z_effect;                 /**< Effect on accessibility component */
    float threshold;                /**< Minimum level to have effect */
    float saturation;               /**< Maximum effective level */
    bool is_inverted_u;             /**< Uses inverted-U response curve */
} pr_neuromod_mapping_t;

/**
 * @brief Reward signal
 *
 * WHAT: A reward/punishment signal with associated memory effects
 * WHY:  Track reward signals for memory modulation
 */
typedef struct {
    pr_reward_type_t type;          /**< Reward type */
    float magnitude;                /**< Reward magnitude [-1, +1] */
    float prediction_error;         /**< Actual - expected [--1, +1] */
    uint64_t memory_id;             /**< Associated memory (if any) */
    uint64_t timestamp_ms;          /**< When reward occurred */
    float decay_factor;             /**< Current decay (starts at 1.0) */
} pr_reward_signal_t;

/**
 * @brief Stress modulation state
 *
 * WHAT: Current stress state and modulation parameters
 * WHY:  Track stress for Yerkes-Dodson memory effects
 */
typedef struct {
    pr_stress_state_t state;        /**< Current stress category */
    float cortisol_level;           /**< Current cortisol [0, 1] */
    float baseline_cortisol;        /**< Baseline cortisol level */
    float acute_stress_boost;       /**< Temporary acute stress boost */
    float chronic_stress_factor;    /**< Chronic stress accumulation */
    uint64_t stress_onset_ms;       /**< When current stress episode began */
    uint64_t last_update_ms;        /**< Last stress update timestamp */
    float performance_factor;       /**< Current Yerkes-Dodson factor [0, 1] */
} pr_stress_state_info_t;

/**
 * @brief Neuromodulator history entry
 *
 * WHAT: Record of neuromodulator state at a point in time
 * WHY:  Track neuromodulator dynamics for analysis
 */
typedef struct {
    uint64_t timestamp_ms;                          /**< Timestamp */
    float concentrations[PR_HYPO_NEUROMOD_COUNT];   /**< All concentrations */
    float stress_level;                             /**< Stress level */
    float reward_signal;                            /**< Active reward signal */
} pr_hypo_history_entry_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling hypothalamus bridge behavior
 * WHY:  Allow customization for different applications
 */
typedef struct {
    /* History tracking */
    size_t history_buffer_size;     /**< Max history entries to track */
    bool track_history;             /**< Enable history tracking */

    /* Neuromodulator parameters */
    float baseline_levels[PR_HYPO_NEUROMOD_COUNT]; /**< Baseline concentrations */
    float decay_rates[PR_HYPO_NEUROMOD_COUNT];     /**< Return-to-baseline rates */

    /* Stress parameters */
    float optimal_stress_level;     /**< Yerkes-Dodson optimal point */
    float stress_impairment_threshold; /**< Above this, memory impaired */
    float chronic_stress_factor;    /**< Chronic stress accumulation rate */

    /* Reward parameters */
    float reward_da_boost;          /**< DA boost per reward */
    float reward_decay_rate;        /**< Reward signal decay rate */
    float prediction_error_sensitivity; /**< Sensitivity to prediction error */

    /* Mapping parameters */
    pr_neuromod_mapping_t mappings[PR_HYPO_NEUROMOD_COUNT]; /**< Custom mappings */
    bool use_default_mappings;      /**< Use defaults if not customized */

    /* Integration */
    bool enable_z_ladder_boost;     /**< Enable promotion acceleration */
    float z_ladder_boost_factor;    /**< How much DA boosts promotion */
} pr_hypo_config_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the hypothalamus bridge
 * WHY:  Monitor neuromodulator dynamics and effects
 */
typedef struct {
    /* Neuromodulator statistics */
    uint64_t modulations_applied;   /**< Total modulations applied */
    uint64_t modulations_per_type[PR_HYPO_NEUROMOD_COUNT]; /**< Per-type counts */
    float peak_concentrations[PR_HYPO_NEUROMOD_COUNT];     /**< Peak levels seen */
    float avg_concentrations[PR_HYPO_NEUROMOD_COUNT];      /**< Running averages */

    /* Reward statistics */
    uint64_t rewards_processed;     /**< Total reward signals */
    uint64_t positive_rewards;      /**< Positive reward count */
    uint64_t negative_rewards;      /**< Negative reward count */
    float total_reward_magnitude;   /**< Sum of reward magnitudes */
    float avg_prediction_error;     /**< Average prediction error */

    /* Stress statistics */
    uint64_t stress_events;         /**< Total stress events */
    float avg_stress_level;         /**< Average stress level */
    float time_in_optimal_stress_ms;/**< Time spent in optimal zone */
    float time_in_high_stress_ms;   /**< Time spent in high stress */
    uint64_t flashbulb_triggers;    /**< Flashbulb memory triggers */

    /* Memory impact */
    uint64_t quaternions_modified;  /**< Quaternions modified */
    uint64_t memories_boosted;      /**< Memories receiving boost */
    uint64_t memories_impaired;     /**< Memories receiving impairment */
    float avg_consolidation_boost;  /**< Average w-component boost */

    /* Timing */
    uint64_t last_update_ms;        /**< Last activity timestamp */
} pr_hypo_stats_t;

/**
 * @brief Callback for neuromodulator changes
 *
 * @param type Neuromodulator that changed
 * @param old_level Previous concentration
 * @param new_level New concentration
 * @param user_data User-provided context
 */
typedef void (*pr_neuromod_callback_t)(
    pr_neuromod_type_t type,
    float old_level,
    float new_level,
    void* user_data
);

/**
 * @brief Callback for reward signals
 *
 * @param signal The reward signal
 * @param user_data User-provided context
 */
typedef void (*pr_reward_callback_t)(
    const pr_reward_signal_t* signal,
    void* user_data
);

/**
 * @brief Callback for stress state changes
 *
 * @param old_state Previous stress state
 * @param new_state New stress state
 * @param cortisol_level Current cortisol level
 * @param user_data User-provided context
 */
typedef void (*pr_stress_callback_t)(
    pr_stress_state_t old_state,
    pr_stress_state_t new_state,
    float cortisol_level,
    void* user_data
);

/**
 * @brief Opaque hypothalamus bridge handle
 */
typedef struct pr_hypo_bridge_struct* pr_hypo_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults based on neuroscience literature
 * WHY:  Provides starting point for typical memory modulation
 *
 * @return Default configuration structure
 *
 * Default values include:
 * - Baseline neuromodulator levels: 0.5 for all
 * - Optimal stress level: 0.5 (Yerkes-Dodson midpoint)
 * - Standard decay rates for all neuromodulators
 */
NIMCP_EXPORT pr_hypo_config_t pr_hypo_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_hypo_config_validate(const pr_hypo_config_t* config);

/**
 * @brief Get default neuromodulator-quaternion mapping
 *
 * WHAT: Returns default mapping parameters for a neuromodulator type
 * WHY:  Allow inspection and customization of default mappings
 *
 * @param type Neuromodulator type
 * @return Default mapping for that type
 */
NIMCP_EXPORT pr_neuromod_mapping_t pr_hypo_default_mapping(pr_neuromod_type_t type);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create hypothalamus bridge
 *
 * WHAT: Allocates and initializes hypothalamus bridge
 * WHY:  Entry point for neuromodulator-memory integration
 * HOW:  Creates state arrays, initializes baselines, sets up mappings
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~1KB + history buffer
 *
 * EXAMPLE:
 * ```c
 * pr_hypo_config_t config = pr_hypo_config_default();
 * config.optimal_stress_level = 0.4f;  // Lower optimal stress
 * pr_hypo_bridge_t bridge = pr_hypo_bridge_create(&config);
 * ```
 */
NIMCP_EXPORT pr_hypo_bridge_t pr_hypo_bridge_create(const pr_hypo_config_t* config);

/**
 * @brief Destroy hypothalamus bridge
 *
 * WHAT: Deallocates bridge and all resources
 * WHY:  Clean resource release
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_hypo_bridge_destroy(pr_hypo_bridge_t bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Resets all neuromodulator levels to baseline
 * WHY:  Start fresh for new simulation
 *
 * @param bridge Hypothalamus bridge
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reset(pr_hypo_bridge_t bridge);

//=============================================================================
// Neuromodulator Functions
//=============================================================================

/**
 * @brief Set neuromodulator concentration
 *
 * WHAT: Directly set a neuromodulator's concentration
 * WHY:  Simulate external neuromodulator events
 *
 * @param bridge Hypothalamus bridge
 * @param type Neuromodulator type
 * @param concentration New concentration [0, 1]
 * @return PR_HYPO_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Simulate dopamine burst
 * pr_hypo_bridge_set_neuromod(bridge, PR_NEUROMOD_DOPAMINE, 0.9f);
 * ```
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_neuromod(
    pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    float concentration
);

/**
 * @brief Get current neuromodulator concentration
 *
 * @param bridge Hypothalamus bridge
 * @param type Neuromodulator type
 * @return Current concentration [0, 1], or -1.0f on error
 */
NIMCP_EXPORT float pr_hypo_bridge_get_neuromod(
    const pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type
);

/**
 * @brief Apply neuromodulator pulse (temporary boost)
 *
 * WHAT: Apply temporary neuromodulator boost that decays over time
 * WHY:  Model phasic neuromodulator release events
 *
 * @param bridge Hypothalamus bridge
 * @param type Neuromodulator type
 * @param pulse_magnitude Boost magnitude [0, 1]
 * @param decay_time_ms How long until decay to baseline
 * @return PR_HYPO_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Apply dopamine pulse that decays over 500ms
 * pr_hypo_bridge_apply_pulse(bridge, PR_NEUROMOD_DOPAMINE, 0.4f, 500);
 * ```
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_apply_pulse(
    pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    float pulse_magnitude,
    uint64_t decay_time_ms
);

/**
 * @brief Get full neuromodulator state
 *
 * @param bridge Hypothalamus bridge
 * @param type Neuromodulator type
 * @param state Output state structure
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_neuromod_state(
    const pr_hypo_bridge_t bridge,
    pr_neuromod_type_t type,
    pr_neuromod_state_t* state
);

/**
 * @brief Update neuromodulator decay (call periodically)
 *
 * WHAT: Update all neuromodulators for time passage
 * WHY:  Neuromodulators return to baseline over time
 *
 * @param bridge Hypothalamus bridge
 * @param elapsed_ms Milliseconds since last update
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_update(
    pr_hypo_bridge_t bridge,
    uint64_t elapsed_ms
);

//=============================================================================
// Quaternion Mapping Functions
//=============================================================================

/**
 * @brief Apply neuromodulator effects to quaternion
 *
 * WHAT: Modify quaternion based on current neuromodulator levels
 * WHY:  Core function for neuromodulator-memory integration
 * HOW:  Applies configured mappings from all active neuromodulators
 *
 * @param bridge Hypothalamus bridge
 * @param input Input quaternion state
 * @return Modified quaternion with neuromodulator effects
 *
 * COMPLEXITY: O(num_neuromodulators)
 *
 * Effects by neuromodulator:
 * - Dopamine: Boosts w (consolidation) and z (accessibility)
 * - Serotonin: Modulates x (emotion valence)
 * - Norepinephrine: Boosts y (salience)
 * - Acetylcholine: Boosts z (accessibility)
 * - Cortisol: Inverted-U effect on w
 *
 * EXAMPLE:
 * ```c
 * nimcp_quaternion_t state = node->state;
 * nimcp_quaternion_t modified = pr_hypo_bridge_apply_neuromodulator(
 *     bridge, state);
 * pr_memory_node_update_state(node, modified);
 * ```
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_apply_neuromodulator(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input
);

/**
 * @brief Apply specific neuromodulator to quaternion
 *
 * WHAT: Apply single neuromodulator effect to quaternion
 * WHY:  Fine-grained control over specific neuromodulator effects
 *
 * @param bridge Hypothalamus bridge
 * @param input Input quaternion
 * @param type Neuromodulator to apply
 * @return Modified quaternion
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_apply_single_neuromod(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input,
    pr_neuromod_type_t type
);

/**
 * @brief Map dopamine level to quaternion modification
 *
 * WHAT: Convert dopamine concentration to quaternion delta
 * WHY:  Dopamine-specific mapping for reward memory encoding
 *
 * @param bridge Hypothalamus bridge
 * @param da_level Dopamine concentration [0, 1]
 * @return Quaternion representing DA effect (to be applied to memory state)
 *
 * COMPLEXITY: O(1)
 *
 * Mapping:
 * - w: +boost (higher consolidation)
 * - x: slight positive shift
 * - y: no direct effect
 * - z: +boost (higher accessibility)
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_dopamine_to_quaternion(
    pr_hypo_bridge_t bridge,
    float da_level
);

/**
 * @brief Map serotonin level to quaternion modification
 *
 * @param bridge Hypothalamus bridge
 * @param serotonin_level Serotonin concentration [0, 1]
 * @return Quaternion representing 5-HT effect
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_serotonin_to_quaternion(
    pr_hypo_bridge_t bridge,
    float serotonin_level
);

/**
 * @brief Map norepinephrine level to quaternion modification
 *
 * @param bridge Hypothalamus bridge
 * @param ne_level Norepinephrine concentration [0, 1]
 * @return Quaternion representing NE effect
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_map_norepinephrine_to_quaternion(
    pr_hypo_bridge_t bridge,
    float ne_level
);

/**
 * @brief Get quaternion effect delta for current state
 *
 * WHAT: Get the quaternion modification that would be applied
 * WHY:  Inspect effects without modifying anything
 *
 * @param bridge Hypothalamus bridge
 * @return Quaternion delta representing current neuromodulator effects
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_get_effect_delta(
    const pr_hypo_bridge_t bridge
);

//=============================================================================
// Stress Modulation Functions
//=============================================================================

/**
 * @brief Apply stress modulation to memory
 *
 * WHAT: Modulate memory encoding/consolidation based on stress level
 * WHY:  Model Yerkes-Dodson inverted-U stress-memory relationship
 *
 * @param bridge Hypothalamus bridge
 * @param input Input quaternion state
 * @return Modified quaternion with stress effects
 *
 * Effects:
 * - Low stress: Slightly reduced consolidation
 * - Optimal stress: Peak consolidation boost
 * - High stress: Reduced consolidation, increased salience
 * - Acute stress: Flashbulb encoding (high salience, high consolidation)
 *
 * EXAMPLE:
 * ```c
 * // Apply stress modulation during memory encoding
 * nimcp_quaternion_t stressed = pr_hypo_bridge_stress_modulation(
 *     bridge, node->state);
 * pr_memory_node_update_state(node, stressed);
 * ```
 */
NIMCP_EXPORT nimcp_quaternion_t pr_hypo_bridge_stress_modulation(
    pr_hypo_bridge_t bridge,
    nimcp_quaternion_t input
);

/**
 * @brief Set cortisol/stress level
 *
 * WHAT: Set current stress hormone level
 * WHY:  Simulate stress response
 *
 * @param bridge Hypothalamus bridge
 * @param cortisol_level Cortisol concentration [0, 1]
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_stress_level(
    pr_hypo_bridge_t bridge,
    float cortisol_level
);

/**
 * @brief Get current stress state
 *
 * @param bridge Hypothalamus bridge
 * @param state Output stress state information
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_stress_state(
    const pr_hypo_bridge_t bridge,
    pr_stress_state_info_t* state
);

/**
 * @brief Trigger acute stress response
 *
 * WHAT: Simulate sudden acute stress event
 * WHY:  Model flashbulb memory encoding triggers
 *
 * @param bridge Hypothalamus bridge
 * @param intensity Stress intensity [0, 1]
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_trigger_acute_stress(
    pr_hypo_bridge_t bridge,
    float intensity
);

/**
 * @brief Calculate Yerkes-Dodson performance factor
 *
 * WHAT: Get current stress-performance factor
 * WHY:  Inspect stress effect without modifying memory
 *
 * @param bridge Hypothalamus bridge
 * @return Performance factor [0, 1] where 1.0 = optimal
 */
NIMCP_EXPORT float pr_hypo_bridge_get_performance_factor(
    const pr_hypo_bridge_t bridge
);

//=============================================================================
// Reward Signal Functions
//=============================================================================

/**
 * @brief Process reward signal
 *
 * WHAT: Process a reward/punishment signal and update neuromodulators
 * WHY:  Reward signals drive dopaminergic memory modulation
 *
 * @param bridge Hypothalamus bridge
 * @param type Reward type
 * @param magnitude Reward magnitude [-1, +1]
 * @param prediction_error Prediction error (actual - expected) [-1, +1]
 * @return PR_HYPO_SUCCESS or error code
 *
 * Effects:
 * - Positive reward: DA boost
 * - Negative reward: NE boost
 * - Prediction error: DA proportional to surprise
 * - Social reward: Oxytocin boost
 *
 * EXAMPLE:
 * ```c
 * // Unexpected positive outcome
 * pr_hypo_bridge_reward_signal(bridge,
 *     PR_REWARD_POSITIVE, 0.8f, 0.5f);  // Better than expected
 * ```
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reward_signal(
    pr_hypo_bridge_t bridge,
    pr_reward_type_t type,
    float magnitude,
    float prediction_error
);

/**
 * @brief Process reward signal with memory association
 *
 * WHAT: Process reward and associate with specific memory
 * WHY:  Tag memory for preferential consolidation
 *
 * @param bridge Hypothalamus bridge
 * @param type Reward type
 * @param magnitude Reward magnitude [-1, +1]
 * @param prediction_error Prediction error [-1, +1]
 * @param memory_id Associated memory node ID
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reward_signal_with_memory(
    pr_hypo_bridge_t bridge,
    pr_reward_type_t type,
    float magnitude,
    float prediction_error,
    uint64_t memory_id
);

/**
 * @brief Get current reward signal state
 *
 * @param bridge Hypothalamus bridge
 * @param signal Output current reward signal (if any)
 * @return true if active reward signal, false if none
 */
NIMCP_EXPORT bool pr_hypo_bridge_get_reward_signal(
    const pr_hypo_bridge_t bridge,
    pr_reward_signal_t* signal
);

/**
 * @brief Calculate memory boost from current reward state
 *
 * WHAT: Get consolidation boost for memory encoding right now
 * WHY:  Determine how much current reward state boosts new memories
 *
 * @param bridge Hypothalamus bridge
 * @return Consolidation boost factor [0, 1]
 */
NIMCP_EXPORT float pr_hypo_bridge_get_reward_boost(
    const pr_hypo_bridge_t bridge
);

//=============================================================================
// Memory Integration Functions
//=============================================================================

/**
 * @brief Apply full hypothalamic modulation to memory node
 *
 * WHAT: Apply all neuromodulator effects to a memory node
 * WHY:  Convenience function for full modulation
 *
 * @param bridge Hypothalamus bridge
 * @param node Memory node to modify
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_modulate_memory(
    pr_hypo_bridge_t bridge,
    pr_memory_node_t* node
);

/**
 * @brief Get promotion boost from current neuromodulator state
 *
 * WHAT: Calculate Z-ladder promotion boost from DA levels
 * WHY:  DA accelerates memory consolidation/promotion
 *
 * @param bridge Hypothalamus bridge
 * @return Promotion boost factor [0, 1]
 */
NIMCP_EXPORT float pr_hypo_bridge_get_promotion_boost(
    const pr_hypo_bridge_t bridge
);

/**
 * @brief Check if current state triggers flashbulb encoding
 *
 * WHAT: Determine if current neuromodulator state indicates flashbulb memory
 * WHY:  High NE + stress triggers flashbulb memory encoding
 *
 * @param bridge Hypothalamus bridge
 * @return true if flashbulb encoding should be triggered
 */
NIMCP_EXPORT bool pr_hypo_bridge_is_flashbulb_state(
    const pr_hypo_bridge_t bridge
);

//=============================================================================
// Callback Functions
//=============================================================================

/**
 * @brief Set callback for neuromodulator changes
 *
 * @param bridge Hypothalamus bridge
 * @param callback Function to call on change (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_neuromod_callback(
    pr_hypo_bridge_t bridge,
    pr_neuromod_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for reward signals
 *
 * @param bridge Hypothalamus bridge
 * @param callback Function to call on reward (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_reward_callback(
    pr_hypo_bridge_t bridge,
    pr_reward_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for stress state changes
 *
 * @param bridge Hypothalamus bridge
 * @param callback Function to call on stress change (NULL to disable)
 * @param user_data Context passed to callback
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_set_stress_callback(
    pr_hypo_bridge_t bridge,
    pr_stress_callback_t callback,
    void* user_data
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Hypothalamus bridge
 * @param stats Output statistics structure
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_stats(
    const pr_hypo_bridge_t bridge,
    pr_hypo_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Hypothalamus bridge
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_reset_stats(
    pr_hypo_bridge_t bridge
);

/**
 * @brief Get neuromodulator history
 *
 * @param bridge Hypothalamus bridge
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @param count Output: actual count returned
 * @return PR_HYPO_SUCCESS or error code
 */
NIMCP_EXPORT pr_hypo_error_t pr_hypo_bridge_get_history(
    const pr_hypo_bridge_t bridge,
    pr_hypo_history_entry_t* entries,
    size_t max_entries,
    size_t* count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_hypo_error_string(pr_hypo_error_t error);

/**
 * @brief Get neuromodulator type name
 *
 * @param type Neuromodulator type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_neuromod_type_name(pr_neuromod_type_t type);

/**
 * @brief Get reward type name
 *
 * @param type Reward type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_reward_type_name(pr_reward_type_t type);

/**
 * @brief Get stress state name
 *
 * @param state Stress state
 * @return Human-readable state name
 */
NIMCP_EXPORT const char* pr_stress_state_name(pr_stress_state_t state);

/**
 * @brief Print bridge state summary
 *
 * @param bridge Hypothalamus bridge
 */
NIMCP_EXPORT void pr_hypo_bridge_print_summary(const pr_hypo_bridge_t bridge);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_hypo_current_time_ms(void);

/**
 * @brief Validate bridge internal consistency
 *
 * @param bridge Hypothalamus bridge
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool pr_hypo_bridge_validate(const pr_hypo_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_HYPO_BRIDGE_H
