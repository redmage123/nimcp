/**
 * @file nimcp_dragonfly_emotion_bridge.h
 * @brief Emotion-Dragonfly Integration Bridge
 *
 * BIOLOGICAL REFERENCE:
 * While dragonfly emotions differ from mammalian ones, they exhibit
 * motivational states that influence behavior:
 * - Hunger drives hunting motivation
 * - Fear/startle responses to predators
 * - Frustration from missed targets
 * - Satisfaction from successful hunts
 *
 * WHAT: Integrates emotional/motivational states with hunting
 * WHY:  Enables realistic behavioral modulation by internal states
 * HOW:  Bidirectional communication with emotion system
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_EMOTION_BRIDGE_H
#define NIMCP_DRAGONFLY_EMOTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_emotion_bridge_s* dragonfly_emotion_bridge_t;
typedef struct emotion_system_s* emotion_system_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Motivational drive
 */
typedef enum {
    DRIVE_HUNGER,             /**< Hunger/feeding drive */
    DRIVE_FEAR,               /**< Fear/escape drive */
    DRIVE_CURIOSITY,          /**< Exploration drive */
    DRIVE_AGGRESSION,         /**< Territorial aggression */
    DRIVE_REST,               /**< Rest/recovery drive */
    DRIVE_MATING,             /**< Reproductive drive */
    DRIVE_COUNT
} motivational_drive_t;

/**
 * @brief Emotional valence
 */
typedef enum {
    VALENCE_VERY_NEGATIVE,    /**< Strong negative (fear, pain) */
    VALENCE_NEGATIVE,         /**< Negative (frustration) */
    VALENCE_NEUTRAL,          /**< Neutral state */
    VALENCE_POSITIVE,         /**< Positive (satisfaction) */
    VALENCE_VERY_POSITIVE     /**< Strong positive (success high) */
} emotional_valence_t;

/**
 * @brief Arousal level (activation)
 */
typedef enum {
    AROUSAL_DORMANT,          /**< Very low activation */
    AROUSAL_CALM,             /**< Relaxed state */
    AROUSAL_ALERT,            /**< Attentive state */
    AROUSAL_EXCITED,          /**< Heightened state */
    AROUSAL_FRENZIED          /**< Maximum activation */
} arousal_level_t;

/**
 * @brief Emotional state
 */
typedef struct {
    /* Core dimensions */
    emotional_valence_t valence;  /**< Positive/negative */
    arousal_level_t arousal;      /**< Activation level */
    float dominance;              /**< Control/dominance [0,1] */

    /* Drive levels */
    float drives[DRIVE_COUNT];    /**< Current drive levels [0,1] */
    motivational_drive_t primary_drive; /**< Dominant drive */

    /* Derived states */
    float motivation;             /**< Overall hunting motivation [0,1] */
    float confidence;             /**< Self-confidence [0,1] */
    float persistence;            /**< Willingness to persist [0,1] */
    float risk_tolerance;         /**< Risk tolerance [0,1] */
} emotional_state_t;

/**
 * @brief Emotional modulation of hunting
 */
typedef struct {
    /* Pursuit modulation */
    float pursuit_aggression;     /**< Pursuit aggressiveness [0,1] */
    float abort_threshold;        /**< Willingness to abort [0,1] */
    float target_selectivity;     /**< Target selection strictness [0,1] */

    /* Performance modulation */
    float focus_level;            /**< Attentional focus [0,1] */
    float reaction_speed;         /**< Reaction speed modifier [0,1] */
    float decision_speed;         /**< Decision making speed [0,1] */

    /* Strategic modulation */
    bool prefer_safe_targets;     /**< Prefer easy targets */
    bool accept_risky_pursuits;   /**< Accept difficult targets */
    float energy_investment;      /**< Energy willing to invest [0,1] */
} emotion_modulation_t;

/**
 * @brief Emotional event from hunting
 */
typedef struct {
    /* Event type */
    bool is_success;              /**< Hunt success/failure */
    bool is_escape;               /**< Target escaped */
    bool is_threat;               /**< Predator detected */
    bool is_competitor;           /**< Competitor detected */

    /* Context */
    float pursuit_duration_s;     /**< Pursuit duration */
    float miss_distance;          /**< How close we got */
    float energy_expended;        /**< Energy used */
    uint32_t consecutive_failures;/**< Failures in a row */

    /* Timestamp */
    uint64_t timestamp_us;        /**< Event time */
} emotional_event_t;

/**
 * @brief Emotion bridge configuration
 */
typedef struct {
    /* Drive parameters */
    float hunger_decay_rate;          /**< Hunger increase rate */
    float hunger_satisfaction;        /**< Hunger decrease per success */
    float fear_decay_rate;            /**< Fear decay rate */
    float frustration_buildup_rate;   /**< Frustration increase rate */

    /* Thresholds */
    float hunt_motivation_threshold;  /**< Min motivation to hunt */
    float fear_abort_threshold;       /**< Fear level to abort */
    float frustration_rest_threshold; /**< Frustration to rest */

    /* Modulation strengths */
    float hunger_pursuit_strength;    /**< Hunger effect on pursuit */
    float fear_performance_penalty;   /**< Fear effect on performance */
    float confidence_performance_bonus; /**< Confidence effect */

    /* Learning */
    float success_confidence_boost;   /**< Confidence gain per success */
    float failure_confidence_penalty; /**< Confidence loss per failure */
    float emotional_learning_rate;    /**< Emotional learning rate */

    /* Homeostasis */
    bool enable_emotional_homeostasis;/**< Enable emotional regulation */
    float homeostasis_rate;           /**< Regulation rate */
} dragonfly_emotion_config_t;

/**
 * @brief Emotion bridge statistics
 */
typedef struct {
    uint64_t emotional_events;        /**< Total events processed */
    uint64_t positive_events;         /**< Positive events */
    uint64_t negative_events;         /**< Negative events */
    float avg_valence;                /**< Average emotional valence */
    float avg_arousal;                /**< Average arousal level */
    float avg_motivation;             /**< Average motivation */
    uint64_t fear_aborts;             /**< Times aborted due to fear */
    uint64_t frustration_rests;       /**< Times rested due to frustration */
} dragonfly_emotion_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default emotion bridge configuration
 */
dragonfly_emotion_config_t dragonfly_emotion_default_config(void);

/**
 * @brief Validate emotion bridge configuration
 */
bool dragonfly_emotion_validate_config(const dragonfly_emotion_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create emotion bridge
 */
dragonfly_emotion_bridge_t dragonfly_emotion_bridge_create(
    const dragonfly_emotion_config_t* config
);

/**
 * @brief Destroy emotion bridge
 */
void dragonfly_emotion_bridge_destroy(dragonfly_emotion_bridge_t bridge);

/**
 * @brief Connect to systems
 */
int dragonfly_emotion_bridge_connect(
    dragonfly_emotion_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    emotion_system_t emotion
);

/**
 * @brief Disconnect from systems
 */
int dragonfly_emotion_bridge_disconnect(dragonfly_emotion_bridge_t bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update emotion bridge
 */
int dragonfly_emotion_bridge_update(
    dragonfly_emotion_bridge_t bridge,
    float dt_s
);

/**
 * @brief Process emotional event
 */
int dragonfly_emotion_process_event(
    dragonfly_emotion_bridge_t bridge,
    const emotional_event_t* event
);

/**
 * @brief Report hunt success
 */
int dragonfly_emotion_report_success(
    dragonfly_emotion_bridge_t bridge,
    float satisfaction_level
);

/**
 * @brief Report hunt failure
 */
int dragonfly_emotion_report_failure(
    dragonfly_emotion_bridge_t bridge,
    float frustration_level,
    const char* reason
);

/**
 * @brief Report predator detection
 */
int dragonfly_emotion_report_threat(
    dragonfly_emotion_bridge_t bridge,
    float threat_level,
    const float threat_position[3]
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get emotional state
 */
int dragonfly_emotion_get_state(
    const dragonfly_emotion_bridge_t bridge,
    emotional_state_t* state
);

/**
 * @brief Get hunting modulation
 */
int dragonfly_emotion_get_modulation(
    const dragonfly_emotion_bridge_t bridge,
    emotion_modulation_t* modulation
);

/**
 * @brief Check if hunting is emotionally appropriate
 */
bool dragonfly_emotion_should_hunt(const dragonfly_emotion_bridge_t bridge);

/**
 * @brief Get hunting motivation level
 */
float dragonfly_emotion_get_motivation(const dragonfly_emotion_bridge_t bridge);

/**
 * @brief Get current drive level
 */
float dragonfly_emotion_get_drive(
    const dragonfly_emotion_bridge_t bridge,
    motivational_drive_t drive
);

/**
 * @brief Get emotion bridge statistics
 */
int dragonfly_emotion_get_stats(
    const dragonfly_emotion_bridge_t bridge,
    dragonfly_emotion_stats_t* stats
);

/**
 * @brief Get drive name
 */
const char* dragonfly_drive_name(motivational_drive_t drive);

/**
 * @brief Get valence name
 */
const char* dragonfly_valence_name(emotional_valence_t valence);

/**
 * @brief Get arousal name
 */
const char* dragonfly_arousal_name(arousal_level_t arousal);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_EMOTION_BRIDGE_H */
