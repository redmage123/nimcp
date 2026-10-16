/**
 * @file nimcp_financial_neuromod_bridge.h
 * @brief Financial Neuromodulator Bridge - Multi-Neurotransmitter Trading State
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling neuromodulatory influences on financial decision-making,
 *       tracking five key neurotransmitters (dopamine, serotonin, norepinephrine,
 *       acetylcholine, adenosine) and their effects on trading parameters.
 *
 * WHY:  Financial decisions are profoundly influenced by neurochemical state.
 *       The neuromodulatory system affects:
 *       - DOPAMINE (DA): Reward prediction, motivation, risk-seeking behavior
 *       - SEROTONIN (5-HT): Mood regulation, patience, goal persistence
 *       - NOREPINEPHRINE (NE): Arousal, attention, urgency response
 *       - ACETYLCHOLINE (ACh): Learning rate, memory encoding, attention focus
 *       - ADENOSINE: Fatigue signaling, need for cognitive rest
 *       By modeling these systems, we can modulate trading behavior based on
 *       simulated neurochemical state and apply appropriate parameter adjustments.
 *
 * HOW:  Neuromodulator levels are updated based on market events and outcomes.
 *       Effects on trading parameters (risk tolerance, patience, learning rate,
 *       arousal, fatigue) are computed via weighted influence functions.
 *       Archetype parameters can be modulated by the current neuromodulator state.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Neuromodulator Bridge                              |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Market Events        |       |  Outcome Feedback     |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | gains/losses          |       | prediction errors     |                |
 * |  | volatility changes    |       | satisfaction levels   |                |
 * |  | opportunity signals   |       | fatigue accumulation  |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Neuromodulator State Update                      |            |
 * |  |  event -> delta(DA, 5HT, NE, ACh, adenosine)             |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Effect Computation                               |            |
 * |  |  state -> (risk_tolerance, patience, learning_rate,      |            |
 * |  |           arousal_level, fatigue_level)                  |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Archetype Modulation                             |            |
 * |  |  archetype_params *= neuromod_effects                    |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * NEUROMODULATOR DYNAMICS:
 * - Dopamine: Phasic bursts on unexpected rewards, dips on prediction errors
 * - Serotonin: Tonic regulation of mood, depletes under chronic stress
 * - Norepinephrine: Spikes on salient events, drives arousal/vigilance
 * - Acetylcholine: Modulates attention and learning rate
 * - Adenosine: Accumulates with cognitive effort, signals fatigue
 *
 * @see nimcp_financial_investor_archetype.h
 * @see nimcp_financial_motivation_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_NEUROMOD_BRIDGE_H
#define NIMCP_FINANCIAL_NEUROMOD_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_NEUROMOD_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_NEUROMOD_BRIDGE_MAGIC      0x464E4D42  /* 'FNMB' */

/** Bio-async module ID for financial neuromodulator bridge */
#define BIO_MODULE_FINANCIAL_NEUROMOD        0x0399

/** Maximum history entries for neuromodulator time series */
#define FIN_NEUROMOD_MAX_HISTORY             512

/** Maximum description/label length */
#define FIN_NEUROMOD_DESC_LEN                128

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_NEUROMOD_ERROR_BASE              33300
#define FIN_NEUROMOD_ERR_OK                  0
#define FIN_NEUROMOD_ERR_NULL                (FIN_NEUROMOD_ERROR_BASE + 1)
#define FIN_NEUROMOD_ERR_INVALID_PARAM       (FIN_NEUROMOD_ERROR_BASE + 2)
#define FIN_NEUROMOD_ERR_NO_MEMORY           (FIN_NEUROMOD_ERROR_BASE + 3)
#define FIN_NEUROMOD_ERR_STATE               (FIN_NEUROMOD_ERROR_BASE + 4)
#define FIN_NEUROMOD_ERR_IMMUNE              (FIN_NEUROMOD_ERROR_BASE + 5)
#define FIN_NEUROMOD_ERR_BBB                 (FIN_NEUROMOD_ERROR_BASE + 6)
#define FIN_NEUROMOD_ERR_SUBSYSTEM           (FIN_NEUROMOD_ERROR_BASE + 7)
#define FIN_NEUROMOD_ERR_COMPUTE             (FIN_NEUROMOD_ERROR_BASE + 8)
#define FIN_NEUROMOD_ERR_RANGE               (FIN_NEUROMOD_ERROR_BASE + 9)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_NEUROMOD_OP_STATE_UNINITIALIZED = 0,
    FIN_NEUROMOD_OP_STATE_INITIALIZED,
    FIN_NEUROMOD_OP_STATE_ACTIVE,
    FIN_NEUROMOD_OP_STATE_DEGRADED,
    FIN_NEUROMOD_OP_STATE_ERROR
} fin_neuromod_op_state_t;

/**
 * @brief Event types that trigger neuromodulator updates
 */
typedef enum {
    FIN_NEUROMOD_EVENT_NONE = 0,
    FIN_NEUROMOD_EVENT_GAIN,              /**< Realized profit */
    FIN_NEUROMOD_EVENT_LOSS,              /**< Realized loss */
    FIN_NEUROMOD_EVENT_UNEXPECTED_GAIN,   /**< Better than expected */
    FIN_NEUROMOD_EVENT_UNEXPECTED_LOSS,   /**< Worse than expected */
    FIN_NEUROMOD_EVENT_OPPORTUNITY,       /**< New opportunity detected */
    FIN_NEUROMOD_EVENT_MISSED_OPP,        /**< Missed opportunity */
    FIN_NEUROMOD_EVENT_VOLATILITY_SPIKE,  /**< Market volatility increase */
    FIN_NEUROMOD_EVENT_STRESS,            /**< General stress event */
    FIN_NEUROMOD_EVENT_COGNITIVE_EFFORT,  /**< Heavy cognitive load */
    FIN_NEUROMOD_EVENT_REST,              /**< Rest/recovery period */
    FIN_NEUROMOD_EVENT_COUNT
} fin_neuromod_event_type_t;

/**
 * @brief Arousal level categories
 */
typedef enum {
    FIN_AROUSAL_VERY_LOW = 0,   /**< Drowsy, inattentive */
    FIN_AROUSAL_LOW,            /**< Relaxed, suboptimal */
    FIN_AROUSAL_OPTIMAL,        /**< Alert, focused */
    FIN_AROUSAL_HIGH,           /**< Excited, elevated */
    FIN_AROUSAL_VERY_HIGH,      /**< Anxious, impaired */
    FIN_AROUSAL_COUNT
} fin_arousal_level_t;

/**
 * @brief Fatigue level categories
 */
typedef enum {
    FIN_FATIGUE_NONE = 0,       /**< Fully rested */
    FIN_FATIGUE_MILD,           /**< Slightly tired */
    FIN_FATIGUE_MODERATE,       /**< Noticeably fatigued */
    FIN_FATIGUE_SEVERE,         /**< Significantly impaired */
    FIN_FATIGUE_CRITICAL,       /**< Dangerous fatigue level */
    FIN_FATIGUE_COUNT
} fin_fatigue_level_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Neuromodulator state - levels of key neurotransmitters
 *
 * All values normalized to [0.0, 1.0] range representing relative activity.
 * Baseline (neutral) state is typically around 0.5 for each.
 */
typedef struct {
    float dopamine;       /**< Reward prediction error, motivation [0-1] */
    float serotonin;      /**< Mood, patience, goal persistence [0-1] */
    float norepinephrine; /**< Arousal, focus, urgency [0-1] */
    float acetylcholine;  /**< Attention, learning rate [0-1] */
    float adenosine;      /**< Fatigue, need for rest [0-1] */
} fin_neuromod_state_t;

/**
 * @brief Trading parameter effects derived from neuromodulator state
 *
 * These computed effects modulate actual trading behavior parameters.
 */
typedef struct {
    float risk_tolerance;   /**< Modulated by DA, 5HT [0-1] */
    float patience;         /**< Modulated by 5HT [0-1] */
    float learning_rate;    /**< Modulated by ACh, DA [0-1] */
    float arousal_level;    /**< Modulated by NE [0-1] */
    float fatigue_level;    /**< Modulated by adenosine [0-1] */
} fin_neuromod_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t state_updates;          /**< Total neuromodulator state updates */
    uint64_t effect_computations;    /**< Effect computation calls */
    uint64_t archetype_modulations;  /**< Archetype modulation calls */
    uint64_t immune_checks;          /**< Immune system checks performed */
    uint64_t bbb_validations;        /**< BBB validations performed */
    uint64_t kg_messages_sent;       /**< KG messages published */
    uint64_t health_heartbeats;      /**< Health heartbeats sent */
} fin_neuromod_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Event descriptor for neuromodulator update
 */
typedef struct {
    fin_neuromod_event_type_t type;   /**< Event type */
    float magnitude;                   /**< Event magnitude [0.0-1.0] */
    float prediction_error;            /**< Prediction error (for DA) [-1,1] */
    uint64_t timestamp_ms;             /**< Event timestamp */
    char description[FIN_NEUROMOD_DESC_LEN]; /**< Optional description */
} fin_neuromod_event_t;

/**
 * @brief Detailed arousal analysis result
 */
typedef struct {
    fin_arousal_level_t level;        /**< Categorical arousal level */
    float raw_arousal;                /**< Raw arousal value [0-1] */
    float optimal_distance;           /**< Distance from optimal arousal */
    float performance_factor;         /**< Expected performance multiplier */
    char recommendation[FIN_NEUROMOD_DESC_LEN]; /**< Actionable recommendation */
} fin_arousal_result_t;

/**
 * @brief Detailed fatigue analysis result
 */
typedef struct {
    fin_fatigue_level_t level;        /**< Categorical fatigue level */
    float raw_fatigue;                /**< Raw fatigue value [0-1] */
    float cognitive_capacity;         /**< Remaining cognitive capacity [0-1] */
    float rest_urgency;               /**< Urgency of rest need [0-1] */
    uint32_t recommended_rest_min;    /**< Recommended rest in minutes */
    char recommendation[FIN_NEUROMOD_DESC_LEN]; /**< Actionable recommendation */
} fin_fatigue_result_t;

/**
 * @brief Archetype modulation parameters (input)
 */
typedef struct {
    float base_risk_tolerance;        /**< Archetype's base risk tolerance */
    float base_patience;              /**< Archetype's base patience */
    float base_learning_rate;         /**< Archetype's base learning rate */
    float base_concentration;         /**< Archetype's base concentration pref */
    float base_contrarian_tendency;   /**< Archetype's contrarian tendency */
} fin_archetype_params_t;

/**
 * @brief Archetype modulation result (output)
 */
typedef struct {
    float modulated_risk_tolerance;   /**< Neuromod-adjusted risk tolerance */
    float modulated_patience;         /**< Neuromod-adjusted patience */
    float modulated_learning_rate;    /**< Neuromod-adjusted learning rate */
    float modulated_concentration;    /**< Neuromod-adjusted concentration */
    float modulated_contrarian;       /**< Neuromod-adjusted contrarian */
    float overall_modulation_factor;  /**< Net modulation strength */
    char modulation_summary[FIN_NEUROMOD_DESC_LEN]; /**< Summary of changes */
} fin_archetype_modulation_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Baseline neuromodulator levels */
    float baseline_dopamine;          /**< Baseline DA level [0.0-1.0] */
    float baseline_serotonin;         /**< Baseline 5HT level [0.0-1.0] */
    float baseline_norepinephrine;    /**< Baseline NE level [0.0-1.0] */
    float baseline_acetylcholine;     /**< Baseline ACh level [0.0-1.0] */
    float baseline_adenosine;         /**< Baseline adenosine level [0.0-1.0] */

    /* Decay rates (per-second half-life factors) */
    float dopamine_decay;             /**< DA decay rate [0.0-1.0] */
    float serotonin_decay;            /**< 5HT decay rate [0.0-1.0] */
    float norepinephrine_decay;       /**< NE decay rate [0.0-1.0] */
    float acetylcholine_decay;        /**< ACh decay rate [0.0-1.0] */
    float adenosine_decay;            /**< Adenosine decay rate (during rest) */
    float adenosine_accumulation;     /**< Adenosine accumulation rate (activity) */

    /* Event sensitivity multipliers */
    float gain_sensitivity;           /**< Response to gains */
    float loss_sensitivity;           /**< Response to losses (typically > gain) */
    float stress_sensitivity;         /**< Response to stress events */
    float effort_sensitivity;         /**< Cognitive effort effect on adenosine */

    /* Effect computation weights */
    float da_risk_weight;             /**< DA contribution to risk tolerance */
    float serotonin_risk_weight;      /**< 5HT contribution to risk tolerance */
    float serotonin_patience_weight;  /**< 5HT contribution to patience */
    float ach_learning_weight;        /**< ACh contribution to learning rate */
    float da_learning_weight;         /**< DA contribution to learning rate */
    float ne_arousal_weight;          /**< NE contribution to arousal */
    float adenosine_fatigue_weight;   /**< Adenosine contribution to fatigue */

    /* Arousal optimization (Yerkes-Dodson) */
    float optimal_arousal;            /**< Optimal arousal level [0.0-1.0] */
    float arousal_tolerance;          /**< Tolerance around optimal [0.0-0.5] */

    /* Fatigue thresholds */
    float fatigue_mild_threshold;     /**< Threshold for mild fatigue */
    float fatigue_moderate_threshold; /**< Threshold for moderate fatigue */
    float fatigue_severe_threshold;   /**< Threshold for severe fatigue */
    float fatigue_critical_threshold; /**< Threshold for critical fatigue */

    /* Integration settings */
    bool enable_immune_integration;   /**< Enable immune system checks */
    bool enable_bbb_validation;       /**< Enable BBB validation */
    bool enable_kg_messaging;         /**< Enable KG messaging */
    bool enable_health_monitoring;    /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;             /**< Verbose debug output */
} fin_neuromod_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief State changed callback
 */
typedef void (*fin_neuromod_state_callback_t)(
    const fin_neuromod_state_t* old_state,
    const fin_neuromod_state_t* new_state,
    const fin_neuromod_event_t* event,
    void* user_data
);

/**
 * @brief Fatigue warning callback
 */
typedef void (*fin_neuromod_fatigue_callback_t)(
    const fin_fatigue_result_t* fatigue,
    void* user_data
);

/**
 * @brief Arousal warning callback (when outside optimal range)
 */
typedef void (*fin_neuromod_arousal_callback_t)(
    const fin_arousal_result_t* arousal,
    void* user_data
);

/* ============================================================================
 * Forward Declarations for Subsystems
 * ============================================================================ */

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial neuromodulator bridge handle
 */
typedef struct financial_neuromod_bridge financial_neuromod_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_default_config(fin_neuromod_config_t* config);

/**
 * @brief Create financial neuromodulator bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_neuromod_bridge_t* financial_neuromod_bridge_create(
    const fin_neuromod_config_t* config
);

/**
 * @brief Destroy financial neuromodulator bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_neuromod_bridge_destroy(financial_neuromod_bridge_t* bridge);

/**
 * @brief Reset bridge state to baseline
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_reset(financial_neuromod_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_neuromod_bridge_set_immune(
    financial_neuromod_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_neuromod_bridge_set_bbb(
    financial_neuromod_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_neuromod_bridge_set_health_agent(
    financial_neuromod_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_neuromod_bridge_set_kg_wiring(
    financial_neuromod_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_neuromod_bridge_set_logger(
    financial_neuromod_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_neuromod_bridge_set_security(
    financial_neuromod_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_neuromod_bridge_set_ethics(
    financial_neuromod_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_neuromod_bridge_set_lgss(
    financial_neuromod_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_neuromod_bridge_set_coordinator(
    financial_neuromod_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_neuromod_bridge_set_bio_router(
    financial_neuromod_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Neuromodulator State Updates
 * ============================================================================ */

/**
 * @brief Update neuromodulator levels based on event
 *
 * Modifies the internal neuromodulator state based on the given event.
 * This is the primary interface for driving neuromodulator dynamics.
 *
 * @param bridge Bridge handle
 * @param event Event causing the update
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_update(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_event_t* event
);

/**
 * @brief Apply time-based decay to neuromodulator state
 *
 * Should be called periodically to simulate natural decay/recovery.
 *
 * @param bridge Bridge handle
 * @param elapsed_ms Milliseconds elapsed since last decay call
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_decay(
    financial_neuromod_bridge_t* bridge,
    uint64_t elapsed_ms
);

/**
 * @brief Get current neuromodulator state
 *
 * @param bridge Bridge handle
 * @param out_state Output state
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_get_state(
    const financial_neuromod_bridge_t* bridge,
    fin_neuromod_state_t* out_state
);

/**
 * @brief Set neuromodulator state directly (for testing/calibration)
 *
 * @param bridge Bridge handle
 * @param state State to set
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_set_state(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_state_t* state
);

/* ============================================================================
 * Core API - Effect Computation
 * ============================================================================ */

/**
 * @brief Compute trading parameter effects from current state
 *
 * Translates the current neuromodulator levels into actionable
 * trading parameter modifiers.
 *
 * @param bridge Bridge handle
 * @param out_effects Output effects
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_compute_effects(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_effects_t* out_effects
);

/**
 * @brief Compute effects from a specific state (without using internal state)
 *
 * Useful for simulation or prediction of effects from hypothetical states.
 *
 * @param bridge Bridge handle
 * @param state Neuromodulator state to compute effects for
 * @param out_effects Output effects
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_compute_effects_from_state(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_state_t* state,
    fin_neuromod_effects_t* out_effects
);

/* ============================================================================
 * Core API - Archetype Modulation
 * ============================================================================ */

/**
 * @brief Modulate archetype parameters based on current neuromodulator state
 *
 * Takes base archetype parameters and adjusts them according to the
 * current neuromodulatory effects.
 *
 * @param bridge Bridge handle
 * @param base_params Base archetype parameters
 * @param out_modulated Output modulated parameters
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_modulate_archetype(
    financial_neuromod_bridge_t* bridge,
    const fin_archetype_params_t* base_params,
    fin_archetype_modulation_t* out_modulated
);

/* ============================================================================
 * Analysis API
 * ============================================================================ */

/**
 * @brief Analyze current arousal state
 *
 * Evaluates arousal level relative to optimal and provides recommendations.
 *
 * @param bridge Bridge handle
 * @param out_arousal Output arousal analysis
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_analyze_arousal(
    financial_neuromod_bridge_t* bridge,
    fin_arousal_result_t* out_arousal
);

/**
 * @brief Analyze current fatigue state
 *
 * Evaluates fatigue level and provides rest recommendations.
 *
 * @param bridge Bridge handle
 * @param out_fatigue Output fatigue analysis
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_analyze_fatigue(
    financial_neuromod_bridge_t* bridge,
    fin_fatigue_result_t* out_fatigue
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set state changed callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_set_state_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_state_callback_t callback,
    void* user_data
);

/**
 * @brief Set fatigue warning callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_set_fatigue_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_fatigue_callback_t callback,
    void* user_data
);

/**
 * @brief Set arousal warning callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_set_arousal_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_arousal_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_neuromod_op_state_t financial_neuromod_bridge_get_op_state(
    const financial_neuromod_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_get_stats(
    const financial_neuromod_bridge_t* bridge,
    fin_neuromod_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_neuromod_bridge_reset_stats(financial_neuromod_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_neuromod_bridge_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param bridge Bridge handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_neuromod_bridge_heartbeat(
    financial_neuromod_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_neuromod_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get operational state name
 *
 * @param state Operational state
 * @return String name (static)
 */
const char* fin_neuromod_op_state_name(fin_neuromod_op_state_t state);

/**
 * @brief Get event type name
 *
 * @param type Event type
 * @return String name (static)
 */
const char* fin_neuromod_event_name(fin_neuromod_event_type_t type);

/**
 * @brief Get arousal level name
 *
 * @param level Arousal level
 * @return String name (static)
 */
const char* fin_neuromod_arousal_name(fin_arousal_level_t level);

/**
 * @brief Get fatigue level name
 *
 * @param level Fatigue level
 * @return String name (static)
 */
const char* fin_neuromod_fatigue_name(fin_fatigue_level_t level);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_neuromod_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_NEUROMOD_BRIDGE_H */
