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
// nimcp_neurogenesis_thalamic_bridge.h - Neurogenesis to Thalamic Gating Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_thalamic_bridge.h
 * @brief Bridge between neurogenesis and thalamic gating systems
 *
 * WHAT: Connects neurogenesis with thalamic gating mechanisms, coordinating
 *       attention and state-dependent modulation of new neuron integration.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (new neuron development and survival)
 *       - Thalamic gating (attention, arousal, state modulation)
 *       - Sleep-dependent consolidation of new neurons
 *
 * HOW:  Bidirectional integration:
 *       1. Thalamic -> Neurogenesis: Arousal gates proliferation
 *       2. Neurogenesis -> Thalamic: New neurons register for gating
 *       3. Sleep states modulate integration and survival
 *       4. Attention signals guide activity-dependent survival
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          THALAMIC GATING
 * -----------------------------------------------------------------
 * Stem cell proliferation            <- Sleep/wake cycle modulation
 * Neuronal differentiation           <- Arousal state influence
 * Synaptic integration               <- Attention-gated plasticity
 * Activity-dependent survival        <- Thalamic relay of activity
 * Sleep-dependent consolidation      <- SWS/REM replay
 * Hippocampal neurogenesis           <- Reticular formation arousal
 * ```
 *
 * STATE-DEPENDENT MODULATION:
 * - Wake/High arousal: Enhanced activity, survival assessment
 * - Quiet wake: Normal integration
 * - NREM sleep: Consolidation, replay-driven survival
 * - REM sleep: Synaptic refinement
 * - Stress arousal: Suppressed proliferation
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_THALAMIC_BRIDGE_H
#define NIMCP_NEUROGENESIS_THALAMIC_BRIDGE_H

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
#define NEUROGENESIS_THALAMIC_MODULE_NAME       "neurogenesis_thalamic_bridge"

/** Maximum gated neurons */
#define NEUROGENESIS_THALAMIC_MAX_NEURONS       256

/** Maximum attention targets */
#define NEUROGENESIS_THALAMIC_MAX_ATTENTION     64

/** Default arousal threshold for proliferation */
#define NEUROGENESIS_THALAMIC_AROUSAL_THRESH    0.4f

/** Default attention gain for survival */
#define NEUROGENESIS_THALAMIC_ATTENTION_GAIN    1.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Arousal/vigilance state
 */
typedef enum {
    NG_THAL_STATE_DEEP_SLEEP = 0,    /**< Deep NREM sleep */
    NG_THAL_STATE_LIGHT_SLEEP,       /**< Light NREM sleep */
    NG_THAL_STATE_REM,               /**< REM sleep */
    NG_THAL_STATE_DROWSY,            /**< Drowsy, low arousal */
    NG_THAL_STATE_QUIET_WAKE,        /**< Relaxed wakefulness */
    NG_THAL_STATE_ALERT,             /**< Normal alert state */
    NG_THAL_STATE_HIGH_AROUSAL,      /**< High arousal/attention */
    NG_THAL_STATE_STRESS             /**< Stress-induced hyperarousal */
} ng_thalamic_state_t;

/**
 * @brief Gating mode for new neurons
 */
typedef enum {
    NG_THAL_GATE_FULL = 0,           /**< Full sensory relay */
    NG_THAL_GATE_ATTENUATED,         /**< Reduced gain */
    NG_THAL_GATE_BURST,              /**< Burst mode (sleep) */
    NG_THAL_GATE_BLOCKED             /**< Blocked (deep sleep) */
} ng_thalamic_gate_mode_t;

/**
 * @brief Attention modulation type
 */
typedef enum {
    NG_THAL_ATTEND_SPATIAL = 0,      /**< Spatial attention */
    NG_THAL_ATTEND_FEATURE,          /**< Feature-based attention */
    NG_THAL_ATTEND_NOVELTY,          /**< Novelty-driven attention */
    NG_THAL_ATTEND_GLOBAL            /**< Global arousal modulation */
} ng_thalamic_attention_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-thalamic bridge
 */
typedef struct {
    /** Arousal modulation */
    float arousal_proliferation_threshold;   /**< Min arousal for proliferation */
    float arousal_proliferation_gain;        /**< Arousal boost to proliferation */
    float stress_suppression_factor;         /**< Stress suppression of neurogenesis */
    bool enable_circadian_modulation;        /**< Circadian rhythm effects */

    /** Sleep modulation */
    float nrem_consolidation_boost;          /**< NREM boost to consolidation */
    float rem_refinement_factor;             /**< REM synaptic refinement */
    float sleep_survival_boost;              /**< Sleep boost to survival */
    uint32_t sleep_cycle_steps;              /**< Steps per sleep cycle */

    /** Attention gating */
    float attention_survival_gain;           /**< Attention boost to survival */
    float attention_decay_rate;              /**< Attention decay rate */
    float min_attention_threshold;           /**< Min attention for effect */
    bool enable_competitive_attention;       /**< Neurons compete for attention */

    /** Thalamic relay */
    float relay_gain_alert;                  /**< Gain in alert state */
    float relay_gain_drowsy;                 /**< Gain in drowsy state */
    float relay_gain_sleep;                  /**< Gain in sleep */
    bool enable_burst_mode_sleep;            /**< Burst mode during sleep */

    /** Update parameters */
    float update_interval_ms;                /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_thalamic_config_t;

/**
 * @brief Thalamic state information
 */
typedef struct {
    ng_thalamic_state_t state;               /**< Current arousal state */
    float arousal_level;                     /**< Arousal level (0-1) */
    float attention_level;                   /**< Global attention (0-1) */
    ng_thalamic_gate_mode_t gate_mode;       /**< Current gating mode */
    float circadian_phase;                   /**< Circadian phase (0-2pi) */
    float stress_level;                      /**< Stress level (0-1) */
    uint64_t state_duration;                 /**< Steps in current state */
    bool in_sleep_cycle;                     /**< Currently in sleep */
} ng_thalamic_state_info_t;

/**
 * @brief Neuron gating state
 */
typedef struct {
    uint32_t neuron_id;                      /**< Neuron identifier */
    ng_thalamic_gate_mode_t gate_mode;       /**< Current gate mode */
    float relay_gain;                        /**< Current relay gain */
    float attention_weight;                  /**< Attention received */
    float arousal_modulation;                /**< Arousal effect */
    float survival_modifier;                 /**< State effect on survival */
    bool is_attended;                        /**< Currently attended */
} ng_thalamic_neuron_state_t;

/**
 * @brief Attention target specification
 */
typedef struct {
    ng_thalamic_attention_t type;            /**< Attention type */
    float position[3];                       /**< Spatial target (if spatial) */
    uint32_t feature_id;                     /**< Feature ID (if feature) */
    float intensity;                         /**< Attention intensity */
    float radius;                            /**< Attention radius */
    uint64_t start_time;                     /**< When attention started */
    uint64_t duration;                       /**< Attention duration */
} ng_thalamic_attention_target_t;

/**
 * @brief Sleep-dependent consolidation event
 */
typedef struct {
    uint32_t neuron_id;                      /**< Neuron consolidated */
    ng_thalamic_state_t sleep_stage;         /**< Which sleep stage */
    float consolidation_strength;            /**< Consolidation amount */
    float survival_boost;                    /**< Survival boost received */
    bool replay_occurred;                    /**< Memory replay happened */
} ng_thalamic_consolidation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t state_transitions;              /**< State transitions */
    uint64_t attention_events;               /**< Attention events */
    uint64_t consolidation_events;           /**< Consolidation events */
    uint64_t neurons_gated;                  /**< Neurons with gating applied */
    float avg_arousal;                       /**< Average arousal level */
    float avg_attention;                     /**< Average attention level */
    float time_in_sleep;                     /**< Fraction of time in sleep */
    float time_in_alert;                     /**< Fraction in alert state */
    float avg_survival_modifier;             /**< Average survival modifier */
    uint64_t update_count;                   /**< Total updates */
    float last_update_ms;                    /**< Last update timestamp */
} ng_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct ng_thalamic_bridge_struct ng_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_default_config(ng_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-thalamic bridge
 *
 * WHAT: Creates bridge for arousal/attention modulation of neurogenesis
 * WHY:  Enable state-dependent regulation of new neurons
 * HOW:  Tracks states, applies gating, modulates survival
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_thalamic_bridge_t* ng_thalamic_bridge_create(
    const ng_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_thalamic_bridge_destroy(ng_thalamic_bridge_t* bridge);

//=============================================================================
// State API
//=============================================================================

/**
 * @brief Set arousal/vigilance state
 *
 * WHAT: Updates current arousal state
 * WHY:  Arousal modulates neurogenesis processes
 * HOW:  Sets state, updates gating parameters
 *
 * @param bridge Bridge handle
 * @param state New arousal state
 * @param arousal_level Arousal level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_set_state(
    ng_thalamic_bridge_t* bridge,
    ng_thalamic_state_t state,
    float arousal_level
);

/**
 * @brief Get current state information
 *
 * @param bridge Bridge handle
 * @param info Output state info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_get_state(
    const ng_thalamic_bridge_t* bridge,
    ng_thalamic_state_info_t* info
);

/**
 * @brief Set stress level
 *
 * WHAT: Updates stress level affecting neurogenesis
 * WHY:  Stress suppresses neurogenesis
 * HOW:  Modulates proliferation and survival thresholds
 *
 * @param bridge Bridge handle
 * @param stress Stress level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_set_stress(
    ng_thalamic_bridge_t* bridge,
    float stress
);

/**
 * @brief Advance circadian phase
 *
 * @param bridge Bridge handle
 * @param dt_hours Time step in hours
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_advance_circadian(
    ng_thalamic_bridge_t* bridge,
    float dt_hours
);

//=============================================================================
// Gating API
//=============================================================================

/**
 * @brief Register neuron for thalamic gating
 *
 * WHAT: Adds neuron to gating system
 * WHY:  New neurons need state-dependent gating
 * HOW:  Creates gating entry with default parameters
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_register_neuron(
    ng_thalamic_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get gating state for neuron
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_get_neuron_state(
    const ng_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    ng_thalamic_neuron_state_t* state
);

/**
 * @brief Get relay gain for neuron
 *
 * WHAT: Returns current relay gain for neuron's inputs
 * WHY:  State-dependent modulation of input strength
 * HOW:  Based on arousal state and attention
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Relay gain multiplier
 */
NIMCP_EXPORT float ng_thalamic_get_relay_gain(
    const ng_thalamic_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get survival modifier for neuron
 *
 * WHAT: Returns state-dependent survival modifier
 * WHY:  States affect survival probability
 * HOW:  Combines arousal, attention, sleep effects
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Survival modifier
 */
NIMCP_EXPORT float ng_thalamic_get_survival_modifier(
    const ng_thalamic_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Check if proliferation is state-allowed
 *
 * @param bridge Bridge handle
 * @return true if current state allows proliferation
 */
NIMCP_EXPORT bool ng_thalamic_can_proliferate(
    const ng_thalamic_bridge_t* bridge
);

//=============================================================================
// Attention API
//=============================================================================

/**
 * @brief Set attention target
 *
 * WHAT: Defines attention focus for neurogenesis modulation
 * WHY:  Attended neurons have survival advantage
 * HOW:  Creates attention field affecting nearby neurons
 *
 * @param bridge Bridge handle
 * @param target Attention target specification
 * @param target_id Output target ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_set_attention(
    ng_thalamic_bridge_t* bridge,
    const ng_thalamic_attention_target_t* target,
    uint32_t* target_id
);

/**
 * @brief Remove attention target
 *
 * @param bridge Bridge handle
 * @param target_id Target to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_remove_attention(
    ng_thalamic_bridge_t* bridge,
    uint32_t target_id
);

/**
 * @brief Get attention weight for neuron
 *
 * WHAT: Returns attention received by neuron
 * WHY:  Attention modulates survival
 * HOW:  Computed from active attention targets
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Attention weight (0-1)
 */
NIMCP_EXPORT float ng_thalamic_get_attention(
    const ng_thalamic_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Update attention decay
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_decay_attention(
    ng_thalamic_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Sleep/Consolidation API
//=============================================================================

/**
 * @brief Trigger sleep consolidation cycle
 *
 * WHAT: Initiates consolidation for sleep-dependent survival
 * WHY:  Sleep consolidates new neuron integration
 * HOW:  Applies consolidation boosts based on sleep stage
 *
 * @param bridge Bridge handle
 * @param events Output consolidation events (optional)
 * @param max_events Maximum events to return
 * @return Number of neurons consolidated
 */
NIMCP_EXPORT int ng_thalamic_consolidate(
    ng_thalamic_bridge_t* bridge,
    ng_thalamic_consolidation_t* events,
    uint32_t max_events
);

/**
 * @brief Trigger memory replay for neurons
 *
 * WHAT: Simulates sleep replay for new neurons
 * WHY:  Replay strengthens active neurons
 * HOW:  Boosts survival for replayed neurons
 *
 * @param bridge Bridge handle
 * @param neuron_ids Neurons to replay
 * @param count Number of neurons
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_replay(
    ng_thalamic_bridge_t* bridge,
    const uint32_t* neuron_ids,
    uint32_t count
);

/**
 * @brief Enter sleep cycle
 *
 * @param bridge Bridge handle
 * @param duration_steps Sleep duration in steps
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_enter_sleep(
    ng_thalamic_bridge_t* bridge,
    uint32_t duration_steps
);

/**
 * @brief Exit sleep cycle
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_exit_sleep(ng_thalamic_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process state transitions, update gating, decay attention
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_update(
    ng_thalamic_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_reset(ng_thalamic_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_thalamic_get_stats(
    const ng_thalamic_bridge_t* bridge,
    ng_thalamic_stats_t* stats
);

/**
 * @brief Get state name string
 *
 * @param state Arousal state
 * @return State name
 */
NIMCP_EXPORT const char* ng_thalamic_state_name(ng_thalamic_state_t state);

/**
 * @brief Get gate mode name string
 *
 * @param mode Gate mode
 * @return Mode name
 */
NIMCP_EXPORT const char* ng_thalamic_gate_mode_name(ng_thalamic_gate_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_THALAMIC_BRIDGE_H */