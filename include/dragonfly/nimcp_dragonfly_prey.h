/**
 * @file nimcp_dragonfly_prey.h
 * @brief Prey Classification and Behavior Prediction
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies distinguish between different prey types and adjust
 * their hunting strategies accordingly. Small flies require different
 * interception approaches than larger moths or other dragonflies.
 *
 * WHAT: Classifies prey type and predicts behavior patterns
 * WHY:  Enables adaptive hunting strategies per prey type
 * HOW:  Feature-based classification with behavioral models
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_PREY_H
#define NIMCP_DRAGONFLY_PREY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_prey_classifier_s* dragonfly_prey_classifier_t;

//=============================================================================
// Constants
//=============================================================================

#define PREY_MAX_FEATURES 32      /**< Maximum feature vector size */
#define PREY_HISTORY_SIZE 64      /**< Motion history buffer size */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Prey type classification
 */
typedef enum {
    PREY_TYPE_UNKNOWN,        /**< Unclassified target */
    PREY_TYPE_MOSQUITO,       /**< Small, erratic flight */
    PREY_TYPE_FLY,            /**< Medium size, fast */
    PREY_TYPE_MOTH,           /**< Large, slower, predictable */
    PREY_TYPE_BUTTERFLY,      /**< Large, erratic */
    PREY_TYPE_BEE,            /**< Medium, aggressive */
    PREY_TYPE_DRAGONFLY,      /**< Large, fast, evasive */
    PREY_TYPE_DAMSELFLY,      /**< Medium, slower relative */
    PREY_TYPE_DEBRIS,         /**< Non-prey (false positive) */
    PREY_TYPE_BIRD,           /**< Too large (predator) */
    PREY_TYPE_COUNT
} prey_type_t;

/**
 * @brief Prey behavior pattern
 */
typedef enum {
    PREY_BEHAVIOR_LINEAR,     /**< Straight-line flight */
    PREY_BEHAVIOR_ZIGZAG,     /**< Alternating direction */
    PREY_BEHAVIOR_SPIRAL,     /**< Spiraling pattern */
    PREY_BEHAVIOR_HOVERING,   /**< Stationary hover */
    PREY_BEHAVIOR_FEEDING,    /**< Feeding flight pattern */
    PREY_BEHAVIOR_MATING,     /**< Mating flight pattern */
    PREY_BEHAVIOR_EVASIVE,    /**< Actively evading */
    PREY_BEHAVIOR_PATROLLING, /**< Territorial patrol */
    PREY_BEHAVIOR_RANDOM      /**< Unpredictable motion */
} prey_behavior_t;

/**
 * @brief Prey difficulty rating
 */
typedef enum {
    PREY_DIFFICULTY_EASY,     /**< High success probability */
    PREY_DIFFICULTY_MEDIUM,   /**< Moderate challenge */
    PREY_DIFFICULTY_HARD,     /**< Difficult to catch */
    PREY_DIFFICULTY_EXTREME   /**< Very low success probability */
} prey_difficulty_t;

/**
 * @brief Observed prey features
 */
typedef struct {
    /* Size and shape */
    float angular_size_rad;   /**< Angular size */
    float aspect_ratio;       /**< Width/height ratio */
    float estimated_size_m;   /**< Estimated actual size */

    /* Motion characteristics */
    float avg_speed;          /**< Average speed */
    float speed_variance;     /**< Speed variability */
    float direction_variance; /**< Direction variability */
    float turn_frequency_hz;  /**< How often it turns */
    float turn_magnitude_rad; /**< Average turn angle */

    /* Temporal patterns */
    float wingbeat_frequency; /**< Detected wingbeat freq */
    float motion_smoothness;  /**< Smoothness of trajectory */

    /* Visual features */
    float contrast;           /**< Visual contrast */
    float color_signature[3]; /**< RGB color estimate */
} prey_features_t;

/**
 * @brief Prey classification result
 */
typedef struct {
    prey_type_t type;              /**< Classified type */
    float type_confidence;         /**< Classification confidence [0,1] */
    float type_probabilities[PREY_TYPE_COUNT]; /**< Per-type probabilities */

    prey_behavior_t behavior;      /**< Current behavior pattern */
    float behavior_confidence;     /**< Behavior confidence */

    prey_difficulty_t difficulty;  /**< Estimated difficulty */
    float success_probability;     /**< Estimated catch success [0,1] */

    /* Recommended strategy */
    float optimal_lead_factor;     /**< Recommended lead factor */
    float recommended_speed;       /**< Recommended pursuit speed */
    float approach_angle_rad;      /**< Recommended approach angle */
    bool recommend_abort;          /**< Should abort pursuit? */
    const char* abort_reason;      /**< Reason if recommend_abort */
} prey_classification_t;

/**
 * @brief Prey behavior prediction
 */
typedef struct {
    float predicted_position[3];   /**< Predicted position */
    float predicted_velocity[3];   /**< Predicted velocity */
    float prediction_time_s;       /**< Prediction horizon */
    float confidence;              /**< Prediction confidence */

    /* Evasion prediction */
    bool evasion_likely;           /**< Likely to evade? */
    float evasion_probability;     /**< Evasion probability */
    float expected_evasion_dir_rad;/**< Expected evasion direction */
} prey_prediction_t;

/**
 * @brief Prey classifier configuration
 */
typedef struct {
    /* Classification thresholds */
    float min_confidence_threshold;  /**< Minimum confidence to classify */
    float reclassification_interval_ms; /**< Time between reclassifications */

    /* Feature weights */
    float size_weight;               /**< Weight for size features */
    float motion_weight;             /**< Weight for motion features */
    float visual_weight;             /**< Weight for visual features */

    /* Behavior analysis */
    uint32_t history_window;         /**< Motion history window size */
    float behavior_switch_threshold; /**< Threshold to switch behavior */

    /* Strategy parameters */
    float abort_difficulty_threshold;/**< Difficulty to recommend abort */
    float abort_success_threshold;   /**< Success prob to recommend abort */

    /* Learning */
    bool enable_learning;            /**< Enable online learning */
    float learning_rate;             /**< Classifier learning rate */
} prey_classifier_config_t;

/**
 * @brief Prey classifier statistics
 */
typedef struct {
    uint64_t classifications_made;   /**< Total classifications */
    uint64_t behavior_predictions;   /**< Behavior predictions made */
    uint64_t correct_predictions;    /**< Correct predictions (if known) */
    float avg_confidence;            /**< Average classification confidence */
    uint32_t type_counts[PREY_TYPE_COUNT]; /**< Count per prey type */
    uint32_t aborts_recommended;     /**< Times abort recommended */
} prey_classifier_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default prey classifier configuration
 */
prey_classifier_config_t prey_classifier_default_config(void);

/**
 * @brief Validate prey classifier configuration
 */
bool prey_classifier_validate_config(const prey_classifier_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create prey classifier
 */
dragonfly_prey_classifier_t dragonfly_prey_classifier_create(
    const prey_classifier_config_t* config
);

/**
 * @brief Destroy prey classifier
 */
void dragonfly_prey_classifier_destroy(dragonfly_prey_classifier_t classifier);

/**
 * @brief Reset prey classifier
 */
int dragonfly_prey_classifier_reset(dragonfly_prey_classifier_t classifier);

//=============================================================================
// Classification Functions
//=============================================================================

/**
 * @brief Update with new observation
 *
 * @param classifier Prey classifier handle
 * @param target_id Target identifier
 * @param position Current position
 * @param velocity Current velocity
 * @param angular_size Angular size (rad)
 * @param contrast Visual contrast
 * @return 0 on success, -1 on error
 */
int dragonfly_prey_observe(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    const float position[3],
    const float velocity[3],
    float angular_size,
    float contrast
);

/**
 * @brief Get prey classification
 *
 * @param classifier Prey classifier handle
 * @param target_id Target identifier
 * @param classification Output classification
 * @return 0 on success, -1 on error
 */
int dragonfly_prey_classify(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    prey_classification_t* classification
);

/**
 * @brief Get prey behavior prediction
 *
 * @param classifier Prey classifier handle
 * @param target_id Target identifier
 * @param horizon_s Prediction horizon (seconds)
 * @param prediction Output prediction
 * @return 0 on success, -1 on error
 */
int dragonfly_prey_predict(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    float horizon_s,
    prey_prediction_t* prediction
);

/**
 * @brief Get hunting strategy recommendation
 *
 * @param classifier Prey classifier handle
 * @param target_id Target identifier
 * @param self_speed Own maximum speed
 * @param self_accel Own maximum acceleration
 * @param classification Output includes strategy recommendation
 * @return 0 on success, -1 on error
 */
int dragonfly_prey_get_strategy(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    float self_speed,
    float self_accel,
    prey_classification_t* classification
);

/**
 * @brief Report hunt outcome for learning
 *
 * @param classifier Prey classifier handle
 * @param target_id Target identifier
 * @param success Whether hunt was successful
 * @return 0 on success, -1 on error
 */
int dragonfly_prey_report_outcome(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    bool success
);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get prey classifier statistics
 */
int dragonfly_prey_classifier_get_stats(
    const dragonfly_prey_classifier_t classifier,
    prey_classifier_stats_t* stats
);

/**
 * @brief Reset prey classifier statistics
 */
int dragonfly_prey_classifier_reset_stats(dragonfly_prey_classifier_t classifier);

/**
 * @brief Get prey type name
 */
const char* dragonfly_prey_type_name(prey_type_t type);

/**
 * @brief Get prey behavior name
 */
const char* dragonfly_prey_behavior_name(prey_behavior_t behavior);

/**
 * @brief Get prey difficulty name
 */
const char* dragonfly_prey_difficulty_name(prey_difficulty_t difficulty);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_PREY_H */
