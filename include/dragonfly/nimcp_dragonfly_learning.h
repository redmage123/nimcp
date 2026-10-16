/**
 * @file nimcp_dragonfly_learning.h
 * @brief Learning from Hunt Outcomes
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies improve hunting success through experience. Failed hunts
 * provide information about prey behavior patterns and environmental
 * challenges. This learning is reinforced through the dopaminergic system.
 *
 * WHAT: Tracks and learns from hunt successes and failures
 * WHY:  Enables continuous improvement of hunting strategies
 * HOW:  Episodic memory with pattern recognition and strategy adaptation
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_LEARNING_H
#define NIMCP_DRAGONFLY_LEARNING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_learning_s* dragonfly_learning_t;

//=============================================================================
// Constants
//=============================================================================

#define LEARNING_MAX_EPISODES 256     /**< Maximum stored episodes */
#define LEARNING_FEATURE_DIM 32       /**< Feature vector dimension */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Hunt outcome
 */
typedef enum {
    OUTCOME_SUCCESS,          /**< Successful catch */
    OUTCOME_MISS_CLOSE,       /**< Close miss (learning opportunity) */
    OUTCOME_MISS_FAR,         /**< Far miss */
    OUTCOME_ESCAPED,          /**< Target escaped */
    OUTCOME_ABORTED_SELF,     /**< Self-aborted */
    OUTCOME_ABORTED_EXTERNAL, /**< External abort (threat, energy) */
    OUTCOME_TIMEOUT           /**< Pursuit timeout */
} hunt_outcome_t;

/**
 * @brief Failure reason category
 */
typedef enum {
    FAIL_REASON_UNKNOWN,      /**< Unknown reason */
    FAIL_REASON_PREDICTION,   /**< Prediction error */
    FAIL_REASON_EVASION,      /**< Unexpected evasion */
    FAIL_REASON_SPEED,        /**< Target too fast */
    FAIL_REASON_ENDURANCE,    /**< Ran out of energy */
    FAIL_REASON_OBSTRUCTION,  /**< Obstacle interference */
    FAIL_REASON_DISTRACTION,  /**< Lost attention */
    FAIL_REASON_APPROACH,     /**< Wrong approach angle */
    FAIL_REASON_TIMING        /**< Timing error */
} failure_reason_t;

/**
 * @brief Hunt episode record
 */
typedef struct {
    /* Identification */
    uint64_t episode_id;              /**< Unique episode ID */
    uint64_t timestamp_us;            /**< Episode timestamp */

    /* Target characteristics */
    float target_size;                /**< Target size */
    float target_speed;               /**< Target average speed */
    float target_maneuverability;     /**< Target maneuverability */
    evasion_type_t evasion_type;      /**< Observed evasion type */
    uint32_t prey_type;               /**< Prey type classification */

    /* Hunt parameters */
    intercept_strategy_t strategy;    /**< Strategy used */
    float initial_range;              /**< Initial range to target */
    float initial_bearing_rad;        /**< Initial bearing */
    float pursuit_duration_s;         /**< Pursuit duration */
    float energy_expended;            /**< Energy used */

    /* Environmental context */
    float wind_speed;                 /**< Wind during hunt */
    float light_level;                /**< Light level */
    float time_of_day;                /**< Time of day [0-24] */

    /* Outcome */
    hunt_outcome_t outcome;           /**< Hunt outcome */
    failure_reason_t failure_reason;  /**< Reason if failed */
    float miss_distance;              /**< Miss distance (if missed) */
    float final_prediction_error;     /**< Final prediction error */

    /* Feature vector for learning */
    float features[LEARNING_FEATURE_DIM]; /**< Extracted features */
} hunt_episode_t;

/**
 * @brief Learned pattern
 */
typedef struct {
    /* Pattern identification */
    uint32_t pattern_id;              /**< Pattern ID */
    const char* description;          /**< Pattern description */

    /* Trigger conditions */
    float trigger_features[LEARNING_FEATURE_DIM]; /**< Trigger conditions */
    float feature_importance[LEARNING_FEATURE_DIM]; /**< Feature weights */

    /* Associated outcome */
    hunt_outcome_t typical_outcome;   /**< Typical outcome */
    float success_rate;               /**< Success rate with pattern */

    /* Recommended adaptation */
    intercept_strategy_t recommended_strategy; /**< Recommended strategy */
    float recommended_lead_factor;    /**< Recommended lead factor */
    float recommended_speed;          /**< Recommended pursuit speed */

    /* Confidence */
    uint32_t observation_count;       /**< Times observed */
    float confidence;                 /**< Pattern confidence [0,1] */
} learned_pattern_t;

/**
 * @brief Strategy effectiveness
 */
typedef struct {
    intercept_strategy_t strategy;    /**< Strategy */
    uint32_t attempts;                /**< Total attempts */
    uint32_t successes;               /**< Successful uses */
    float success_rate;               /**< Success rate */
    float avg_energy_cost;            /**< Average energy cost */
    float avg_pursuit_time_s;         /**< Average pursuit time */

    /* Contextual effectiveness */
    float effectiveness_by_prey[10];  /**< Per prey type */
    float effectiveness_by_evasion[8];/**< Per evasion type */
} strategy_effectiveness_t;

/**
 * @brief Learning recommendation
 */
typedef struct {
    /* Strategy recommendation */
    intercept_strategy_t recommended_strategy;
    float strategy_confidence;

    /* Parameter adjustments */
    float lead_factor_adjustment;     /**< Adjust lead factor */
    float speed_adjustment;           /**< Adjust pursuit speed */
    float aggression_adjustment;      /**< Adjust aggressiveness */

    /* Warnings */
    bool difficult_target;            /**< Target likely difficult */
    float predicted_success_rate;     /**< Predicted success rate */
    const char* advice;               /**< Textual advice */
} learning_recommendation_t;

/**
 * @brief Learning configuration
 */
typedef struct {
    /* Memory */
    uint32_t max_episodes;            /**< Maximum stored episodes */
    float episode_decay_rate;         /**< Old episode relevance decay */

    /* Pattern detection */
    float min_pattern_confidence;     /**< Min confidence for pattern */
    uint32_t min_observations;        /**< Min observations for pattern */
    float similarity_threshold;       /**< Episode similarity threshold */

    /* Learning rates */
    float strategy_learning_rate;     /**< Strategy effectiveness rate */
    float pattern_learning_rate;      /**< Pattern learning rate */
    float adaptation_rate;            /**< Parameter adaptation rate */

    /* Exploration vs exploitation */
    float exploration_rate;           /**< Willingness to try new things */
    float exploration_decay;          /**< Exploration decay over time */
} learning_config_t;

/**
 * @brief Learning statistics
 */
typedef struct {
    uint64_t episodes_recorded;       /**< Total episodes recorded */
    uint32_t patterns_learned;        /**< Patterns identified */
    float overall_success_rate;       /**< Overall success rate */
    float recent_success_rate;        /**< Recent success rate */
    float improvement_rate;           /**< Rate of improvement */
    uint64_t recommendations_given;   /**< Recommendations provided */
    uint64_t recommendations_followed;/**< Recommendations followed */
    float recommendation_accuracy;    /**< Recommendation accuracy */
} learning_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default learning configuration
 */
learning_config_t learning_default_config(void);

/**
 * @brief Validate learning configuration
 */
bool learning_validate_config(const learning_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create learning system
 */
dragonfly_learning_t dragonfly_learning_create(const learning_config_t* config);

/**
 * @brief Destroy learning system
 */
void dragonfly_learning_destroy(dragonfly_learning_t learning);

/**
 * @brief Reset learning system
 */
int dragonfly_learning_reset(dragonfly_learning_t learning);

//=============================================================================
// Episode Recording Functions
//=============================================================================

/**
 * @brief Record hunt episode
 */
int dragonfly_learning_record_episode(
    dragonfly_learning_t learning,
    const hunt_episode_t* episode
);

/**
 * @brief Begin tracking new hunt
 */
int dragonfly_learning_begin_hunt(
    dragonfly_learning_t learning,
    const dragonfly_target_info_t* target,
    intercept_strategy_t strategy
);

/**
 * @brief End hunt and record outcome
 */
int dragonfly_learning_end_hunt(
    dragonfly_learning_t learning,
    hunt_outcome_t outcome,
    failure_reason_t reason,
    float miss_distance
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get recommendation for current target
 */
int dragonfly_learning_get_recommendation(
    dragonfly_learning_t learning,
    const dragonfly_target_info_t* target,
    learning_recommendation_t* recommendation
);

/**
 * @brief Get strategy effectiveness
 */
int dragonfly_learning_get_strategy_stats(
    const dragonfly_learning_t learning,
    intercept_strategy_t strategy,
    strategy_effectiveness_t* stats
);

/**
 * @brief Get all strategy statistics
 */
int dragonfly_learning_get_all_strategy_stats(
    const dragonfly_learning_t learning,
    strategy_effectiveness_t* stats,
    uint32_t* num_strategies
);

/**
 * @brief Get learned patterns
 */
int dragonfly_learning_get_patterns(
    const dragonfly_learning_t learning,
    learned_pattern_t* patterns,
    uint32_t max_patterns,
    uint32_t* num_patterns
);

/**
 * @brief Get recent episodes
 */
int dragonfly_learning_get_recent_episodes(
    const dragonfly_learning_t learning,
    hunt_episode_t* episodes,
    uint32_t max_episodes,
    uint32_t* num_episodes
);

/**
 * @brief Get learning statistics
 */
int dragonfly_learning_get_stats(
    const dragonfly_learning_t learning,
    learning_stats_t* stats
);

/**
 * @brief Get hunt outcome name
 */
const char* dragonfly_hunt_outcome_name(hunt_outcome_t outcome);

/**
 * @brief Get failure reason name
 */
const char* dragonfly_failure_reason_name(failure_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_LEARNING_H */
