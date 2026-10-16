/**
 * @file nimcp_health_emotion_bridge.h
 * @brief Health Agent Emotion Integration Bridge
 *
 * WHAT: Bidirectional emotion-health integration
 * WHY:  Emotional state affects appropriate health thresholds
 * HOW:  Query emotion system, adjust monitoring sensitivity
 *
 * BIOLOGICAL BASIS:
 * - Stress -> Lowered immune function -> More sensitive monitoring
 * - Inflammation -> Impaired cognition -> Conservative thresholds
 * - Positive affect -> Enhanced resilience -> Normal thresholds
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 *
 * @copyright Copyright (c) 2025 NIMCP Project
 */

#ifndef NIMCP_HEALTH_EMOTION_BRIDGE_H
#define NIMCP_HEALTH_EMOTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

struct emotional_system;
typedef struct emotional_system emotional_system_t;

struct emotion_immune_bridge;
typedef struct emotion_immune_bridge emotion_immune_bridge_t;

/*==============================================================================
 * EMOTION-ADJUSTED THRESHOLDS
 *============================================================================*/

/**
 * @brief Emotion-adjusted health thresholds
 *
 * These thresholds are dynamically adjusted based on emotional state
 */
typedef struct {
    float memory_warning_threshold;     /**< Adjusted based on stress */
    float memory_critical_threshold;    /**< Adjusted based on stress */
    float cpu_warning_threshold;        /**< Adjusted based on arousal */
    float cpu_critical_threshold;       /**< Adjusted based on arousal */
    float anomaly_sensitivity;          /**< Adjusted based on valence */
    float recovery_aggressiveness;      /**< Adjusted based on stability */
    float check_frequency_modifier;     /**< More frequent checks under stress */
    float response_delay_modifier;      /**< Faster response under negative affect */
} emotion_adjusted_thresholds_t;

/**
 * @brief Threshold adjustment factors based on emotional dimensions
 */
typedef struct {
    float high_stress_modifier;         /**< Modifier when arousal > 0.7 [0.8] */
    float negative_valence_modifier;    /**< Modifier when valence < -0.3 [1.2] */
    float positive_valence_modifier;    /**< Modifier when valence > 0.3 [0.9] */
    float instability_modifier;         /**< Modifier when stability < 0.4 [1.3] */
    float inflammation_modifier;        /**< Modifier based on inflammation [1.25] */
} threshold_adjustment_factors_t;

/**
 * @brief Get default threshold adjustment factors
 *
 * @param[out] factors Factors to initialize
 */
void health_emotion_default_factors(threshold_adjustment_factors_t* factors);

/**
 * @brief Compute emotion-adjusted health thresholds
 *
 * WHAT: Adjust health monitoring based on emotional state
 * WHY:  Stressed system needs more sensitive monitoring
 * HOW:  Query emotion system, scale thresholds
 *
 * THRESHOLD ADJUSTMENTS:
 * - High stress (arousal > 0.7): Lower warning thresholds by 20%
 * - Negative valence (< -0.3): Increase anomaly sensitivity
 * - High inflammation: Use conservative recovery actions
 * - Positive valence (> 0.3): Allow more aggressive optimization
 *
 * @param[in] agent Health agent
 * @param[in] emotion_system Emotion system
 * @param[in] factors Adjustment factors (NULL for defaults)
 * @param[out] thresholds Output adjusted thresholds
 * @return 0 on success, -1 on error
 */
int health_emotion_compute_thresholds(
    nimcp_health_agent_t* agent,
    const emotional_system_t* emotion_system,
    const threshold_adjustment_factors_t* factors,
    emotion_adjusted_thresholds_t* thresholds
);

/*==============================================================================
 * EMOTIONAL STATE QUERY
 *============================================================================*/

/**
 * @brief Current emotional state for health evaluation
 */
typedef struct {
    float valence;                      /**< Current emotional valence [-1, +1] */
    float arousal;                      /**< Current emotional arousal [0, 1] */
    float stability;                    /**< Emotional stability [0, 1] */
    float stress_index;                 /**< Derived stress measure [0, 1] */
    bool is_positive;                   /**< Valence > 0.2 */
    bool is_negative;                   /**< Valence < -0.2 */
    bool is_stressed;                   /**< Arousal > 0.7 */
    bool is_calm;                       /**< Arousal < 0.3 */
} health_emotion_state_t;

/**
 * @brief Query current emotional state for health decisions
 *
 * @param[in] emotion_system Emotion system
 * @param[out] state Output emotional state summary
 * @return 0 on success, -1 on error
 */
int health_emotion_get_state(
    const emotional_system_t* emotion_system,
    health_emotion_state_t* state
);

/*==============================================================================
 * HEALTH EVENT REPORTING TO EMOTION
 *============================================================================*/

/**
 * @brief Health event types for emotional reporting
 */
typedef enum {
    HEALTH_EMOTION_EVENT_MINOR_ANOMALY = 0,  /**< Minor issue detected */
    HEALTH_EMOTION_EVENT_MODERATE_ANOMALY,   /**< Moderate issue, action taken */
    HEALTH_EMOTION_EVENT_CRITICAL_ANOMALY,   /**< Critical issue, urgent action */
    HEALTH_EMOTION_EVENT_RECOVERY_SUCCESS,   /**< Recovery completed successfully */
    HEALTH_EMOTION_EVENT_RECOVERY_FAILURE,   /**< Recovery attempt failed */
    HEALTH_EMOTION_EVENT_SYSTEM_STABLE,      /**< Return to stable state */
    HEALTH_EMOTION_EVENT_PROLONGED_CRISIS,   /**< Extended crisis mode */
    HEALTH_EMOTION_EVENT_COUNT
} health_emotion_event_type_t;

/**
 * @brief Emotion mapping for health events
 */
typedef struct {
    float valence_delta;                /**< Change in valence [-1, +1] */
    float arousal_delta;                /**< Change in arousal [-1, +1] */
    uint32_t duration_ms;               /**< Duration of emotional effect */
    bool triggers_fear;                 /**< Event triggers fear response */
    bool triggers_relief;               /**< Event triggers relief */
    bool triggers_stress;               /**< Event triggers stress response */
} health_event_emotion_mapping_t;

/**
 * @brief Get emotion mapping for health event type
 *
 * EMOTION MAPPING:
 * - Minor anomaly -> Slight arousal increase
 * - Moderate anomaly -> Stress response (negative valence)
 * - Critical anomaly -> High arousal, fear response
 * - Recovery success -> Positive valence, reduced arousal
 *
 * @param[in] event_type Type of health event
 * @param[in] severity Event severity [0-1]
 * @param[out] mapping Output emotion mapping
 */
void health_emotion_get_event_mapping(
    health_emotion_event_type_t event_type,
    float severity,
    health_event_emotion_mapping_t* mapping
);

/**
 * @brief Report health events to emotion system
 *
 * WHAT: Generate emotional response to health anomalies
 * WHY:  Health crises should create appropriate stress response
 * HOW:  Map anomaly severity to emotional arousal
 *
 * @param[in] agent Health agent
 * @param[in,out] emotion_system Emotion system
 * @param[in] event_type Type of health event
 * @param[in] severity Event severity [0-1]
 * @return 0 on success, -1 on error
 */
int health_emotion_report_event(
    nimcp_health_agent_t* agent,
    emotional_system_t* emotion_system,
    health_emotion_event_type_t event_type,
    float severity
);

/*==============================================================================
 * EMOTION-BASED ACTION GATING
 *============================================================================*/

/**
 * @brief Recovery action types (for emotion gating)
 */
typedef enum {
    HEALTH_RECOVERY_NONE = 0,
    HEALTH_RECOVERY_LOG_ONLY,
    HEALTH_RECOVERY_CLEAR_CACHE,
    HEALTH_RECOVERY_REDUCE_LOAD,
    HEALTH_RECOVERY_PARTIAL_RESTART,
    HEALTH_RECOVERY_FULL_RESTART,
    HEALTH_RECOVERY_QUARANTINE,
    HEALTH_RECOVERY_ROLLBACK,
    HEALTH_RECOVERY_EMERGENCY_SHUTDOWN
} health_recovery_action_t;

/**
 * @brief Check if emotional state permits aggressive recovery
 *
 * WHAT: Verify emotional stability before invasive actions
 * WHY:  Don't make aggressive decisions during emotional crisis
 * HOW:  Check valence, arousal, stability metrics
 *
 * @param[in] emotion_system Emotion system
 * @param[in] action Proposed action
 * @return true if emotional state permits action
 */
bool health_emotion_permits_action(
    const emotional_system_t* emotion_system,
    health_recovery_action_t action
);

/**
 * @brief Get emotion-adjusted recovery recommendation
 *
 * @param[in] emotion_system Emotion system
 * @param[in] proposed_action Originally proposed action
 * @param[out] adjusted_action Emotion-adjusted action
 * @return 0 on success
 */
int health_emotion_adjust_recovery(
    const emotional_system_t* emotion_system,
    health_recovery_action_t proposed_action,
    health_recovery_action_t* adjusted_action
);

/*==============================================================================
 * SHADOW PATTERN DETECTION
 *============================================================================*/

/**
 * @brief Health agent shadow patterns
 *
 * Maladaptive behaviors the health agent might exhibit
 */
typedef enum {
    HEALTH_SHADOW_NONE = 0,

    /* Paranoid patterns */
    HEALTH_SHADOW_HYPERVIGILANCE,       /**< Too many false positives */
    HEALTH_SHADOW_OVERREACTION,         /**< Aggressive recovery for minor issues */

    /* Avoidant patterns */
    HEALTH_SHADOW_DENIAL,               /**< Ignoring real anomalies */
    HEALTH_SHADOW_PROCRASTINATION,      /**< Delaying necessary actions */

    /* Perfectionist patterns */
    HEALTH_SHADOW_OBSESSIVE_CHECKING,   /**< Excessive validation loops */
    HEALTH_SHADOW_NEVER_GOOD_ENOUGH,    /**< Can't accept stable state */

    /* Dependent patterns */
    HEALTH_SHADOW_OVER_ESCALATION,      /**< Always asks for human help */
    HEALTH_SHADOW_DECISION_PARALYSIS,   /**< Can't choose recovery action */

    HEALTH_SHADOW_COUNT
} health_shadow_pattern_t;

/**
 * @brief Shadow pattern detection result
 */
typedef struct {
    health_shadow_pattern_t pattern;    /**< Detected pattern */
    float intensity;                    /**< Pattern intensity [0-1] */
    uint32_t occurrence_count;          /**< How many times observed */
    uint64_t first_observed_us;         /**< When first observed */
    uint64_t last_observed_us;          /**< When last observed */
    char evidence[256];                 /**< Description of evidence */
} shadow_detection_result_t;

/**
 * @brief Detect shadow patterns in health agent
 *
 * WHAT: Identify maladaptive health agent behaviors
 * WHY:  Shadow patterns impair effective health monitoring
 * HOW:  Analyze decision history for patterns
 *
 * @param[in] agent Health agent
 * @param[out] detected_patterns Output array of detected patterns
 * @param[in] max_patterns Max patterns to return
 * @param[out] num_detected Output number of detected patterns
 * @return 0 on success
 */
int health_agent_detect_shadow_patterns(
    const nimcp_health_agent_t* agent,
    shadow_detection_result_t* detected_patterns,
    uint32_t max_patterns,
    uint32_t* num_detected
);

/**
 * @brief Shadow intervention type
 */
typedef enum {
    SHADOW_INTERVENTION_NONE = 0,
    SHADOW_INTERVENTION_RAISE_THRESHOLD,     /**< For hypervigilance */
    SHADOW_INTERVENTION_LOWER_THRESHOLD,     /**< For denial */
    SHADOW_INTERVENTION_LIMIT_FREQUENCY,     /**< For obsessive checking */
    SHADOW_INTERVENTION_FORCE_DEFAULT,       /**< For decision paralysis */
    SHADOW_INTERVENTION_REDUCE_ESCALATION,   /**< For over-escalation */
    SHADOW_INTERVENTION_REQUIRE_ACTION,      /**< For procrastination */
    SHADOW_INTERVENTION_ACCEPT_GOOD_ENOUGH   /**< For perfectionism */
} shadow_intervention_type_t;

/**
 * @brief Apply shadow pattern intervention
 *
 * WHAT: Correct maladaptive health agent behavior
 * WHY:  Restore effective health monitoring
 * HOW:  CBT-style cognitive restructuring
 *
 * INTERVENTIONS:
 * - HYPERVIGILANCE: Raise anomaly thresholds
 * - DENIAL: Lower thresholds, force acknowledgment
 * - OBSESSIVE_CHECKING: Limit check frequency
 * - DECISION_PARALYSIS: Use default action, log for review
 *
 * @param[in,out] agent Health agent
 * @param[in] pattern Pattern to address
 * @return 0 on success
 */
int health_agent_intervene_shadow(
    nimcp_health_agent_t* agent,
    health_shadow_pattern_t pattern
);

/**
 * @brief Get intervention type for shadow pattern
 *
 * @param[in] pattern Shadow pattern
 * @return Recommended intervention type
 */
shadow_intervention_type_t health_shadow_get_intervention(
    health_shadow_pattern_t pattern
);

/**
 * @brief Get shadow pattern name
 *
 * @param[in] pattern Shadow pattern
 * @return Human-readable name
 */
const char* health_shadow_pattern_name(health_shadow_pattern_t pattern);

/*==============================================================================
 * IMMUNE-EMOTION-HEALTH TRIANGLE
 *============================================================================*/

/* Forward declaration for inflammation level */
typedef enum {
    INFLAMMATION_NONE = 0,
    INFLAMMATION_LOW,
    INFLAMMATION_MODERATE,
    INFLAMMATION_HIGH,
    INFLAMMATION_SEVERE
} brain_inflammation_level_t;

/* Forward declaration for psychological state */
struct health_agent_psych_state;
typedef struct health_agent_psych_state health_agent_psych_state_t;

/**
 * @brief Unified immune-emotion-health state
 */
typedef struct {
    /* Immune state */
    brain_inflammation_level_t inflammation_level;
    float immune_health_score;
    uint32_t active_threats;

    /* Emotional state */
    float valence;
    float arousal;
    float emotional_stability;

    /* Health state */
    float overall_health_score;
    uint32_t active_anomalies;
    float agent_stress;                 /**< Health agent's own stress */
    float agent_confidence;             /**< Health agent's decision confidence */

    /* Derived metrics */
    float combined_stress_index;        /**< Unified stress measure */
    float system_resilience;            /**< Capacity to handle issues */
    float recovery_capacity;            /**< Available recovery resources */
} immune_emotion_health_state_t;

/**
 * @brief Update unified state from all systems
 *
 * @param[in] agent Health agent
 * @param[in] emotion_system Emotion system (may be NULL)
 * @param[in] emotion_immune_bridge Emotion-immune bridge (may be NULL)
 * @param[out] state Output unified state
 * @return 0 on success
 */
int health_emotion_update_unified_state(
    const nimcp_health_agent_t* agent,
    const emotional_system_t* emotion_system,
    const emotion_immune_bridge_t* emotion_immune_bridge,
    immune_emotion_health_state_t* state
);

/**
 * @brief Compute combined stress index
 *
 * WHAT: Single metric combining immune/emotion/health stress
 * WHY:  Unified measure for decision thresholds
 * HOW:  Weighted combination with cross-system effects
 *
 * FORMULA:
 * stress = w1*inflammation + w2*arousal + w3*anomaly_load
 *        + cross_effect(inflammation, emotion)
 *        + cross_effect(health, emotion)
 *
 * @param[in] state Current unified state
 * @return Combined stress index [0-1]
 */
float health_emotion_compute_combined_stress(
    const immune_emotion_health_state_t* state
);

/**
 * @brief Get recovery recommendation based on unified state
 *
 * WHAT: Recommend recovery action considering all systems
 * WHY:  Holistic view improves recovery decisions
 * HOW:  Factor in immune capacity, emotional state, health agent stability
 *
 * @param[in] state Unified state
 * @param[in] base_recommendation Original recommendation
 * @param[out] adjusted_recommendation Adjusted recommendation
 * @return 0 on success
 */
int health_emotion_get_holistic_recommendation(
    const immune_emotion_health_state_t* state,
    health_recovery_action_t base_recommendation,
    health_recovery_action_t* adjusted_recommendation
);

/*==============================================================================
 * HEALTH-EMOTION BRIDGE STATISTICS
 *============================================================================*/

/**
 * @brief Health-emotion integration statistics
 */
typedef struct {
    /* Threshold adjustments */
    uint64_t threshold_computations;
    uint64_t stress_based_adjustments;
    uint64_t valence_based_adjustments;

    /* Event reporting */
    uint64_t events_reported;
    uint64_t minor_events;
    uint64_t moderate_events;
    uint64_t critical_events;
    uint64_t recovery_successes;
    uint64_t recovery_failures;

    /* Action gating */
    uint64_t action_permits;
    uint64_t action_blocks;
    uint64_t action_adjustments;

    /* Shadow detection */
    uint64_t shadow_checks;
    uint64_t shadows_detected;
    uint64_t interventions_applied;

    /* Unified state */
    float avg_combined_stress;
    float max_combined_stress;
    float avg_resilience;
} health_emotion_stats_t;

/**
 * @brief Get health-emotion integration statistics
 *
 * @param[in] agent Health agent
 * @param[out] stats Output statistics
 * @return 0 on success
 */
int health_emotion_get_stats(
    const nimcp_health_agent_t* agent,
    health_emotion_stats_t* stats
);

/**
 * @brief Reset health-emotion statistics
 *
 * @param[in,out] agent Health agent
 */
void health_emotion_reset_stats(nimcp_health_agent_t* agent);

/*==============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get event type name
 *
 * @param[in] event_type Event type
 * @return Human-readable name
 */
const char* health_emotion_event_name(health_emotion_event_type_t event_type);

/**
 * @brief Get recovery action name (emotion bridge version)
 *
 * @param[in] action Recovery action
 * @return Human-readable name
 */
const char* health_emotion_recovery_action_name(health_recovery_action_t action);

/**
 * @brief Get inflammation level name
 *
 * @param[in] level Inflammation level
 * @return Human-readable name
 */
const char* health_inflammation_level_name(brain_inflammation_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_EMOTION_BRIDGE_H */
