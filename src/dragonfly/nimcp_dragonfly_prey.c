/**
 * @file nimcp_dragonfly_prey.c
 * @brief Prey Classification and Behavior Prediction Implementation
 *
 * WHAT: Classifies prey type and predicts behavior patterns
 * WHY:  Enables adaptive hunting strategies per prey type
 * HOW:  Feature-based classification with behavioral models
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_prey.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_prey)

//=============================================================================
// Constants
//=============================================================================


#define MAX_TRACKED_PREY 16

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Motion history entry
 */
typedef struct {
    float position[3];
    float velocity[3];
    uint64_t timestamp_us;
} motion_history_t;

/**
 * @brief Tracked prey state
 */
typedef struct {
    uint32_t id;
    bool active;

    /* Observations */
    motion_history_t history[PREY_HISTORY_SIZE];
    uint32_t history_head;
    uint32_t history_count;

    /* Computed features */
    prey_features_t features;
    uint64_t last_feature_update_us;

    /* Classification */
    prey_classification_t classification;
    uint64_t last_classification_us;

    /* Tracking */
    float last_position[3];
    float last_velocity[3];
    uint64_t last_observation_us;
} tracked_prey_t;

/**
 * @brief Internal classifier structure
 */
struct dragonfly_prey_classifier_s {
    /* Configuration */
    prey_classifier_config_t config;

    /* Tracked prey */
    tracked_prey_t prey[MAX_TRACKED_PREY];
    uint32_t num_prey;

    /* Statistics */
    prey_classifier_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
};

//=============================================================================
// Feature Extraction
//=============================================================================

static void update_features(tracked_prey_t* prey) {
    if (prey->history_count < 3) return;

    prey_features_t* f = &prey->features;

    /* Compute motion statistics from history */
    float total_speed = 0.0f;
    float speed_sq_sum = 0.0f;
    float total_direction_change = 0.0f;
    uint32_t direction_changes = 0;
    float prev_heading = 0.0f;

    for (uint32_t i = 0; i < prey->history_count && i < PREY_HISTORY_SIZE; i++) {
        uint32_t idx = (prey->history_head + PREY_HISTORY_SIZE - i - 1) % PREY_HISTORY_SIZE;
        const motion_history_t* h = &prey->history[idx];

        float speed = vec3_length(h->velocity);
        total_speed += speed;
        speed_sq_sum += speed * speed;

        /* Compute heading */
        float heading = atan2f(h->velocity[1], h->velocity[0]);
        if (i > 0) {
            float heading_change = fabsf(heading - prev_heading);
            if (heading_change > (float)M_PI) {
                heading_change = 2.0f * (float)M_PI - heading_change;
            }
            total_direction_change += heading_change;
            direction_changes++;
        }
        prev_heading = heading;
    }

    uint32_t n = prey->history_count;
    f->avg_speed = total_speed / n;
    f->speed_variance = (speed_sq_sum / n) - (f->avg_speed * f->avg_speed);
    if (f->speed_variance < 0.0f) f->speed_variance = 0.0f;

    if (direction_changes > 0) {
        f->direction_variance = total_direction_change / direction_changes;
    }

    /* Estimate turn frequency from history timestamps */
    if (prey->history_count >= 2) {
        uint32_t first_idx = (prey->history_head + PREY_HISTORY_SIZE - prey->history_count) %
                             PREY_HISTORY_SIZE;
        uint32_t last_idx = (prey->history_head + PREY_HISTORY_SIZE - 1) % PREY_HISTORY_SIZE;
        float duration_s = (float)(prey->history[last_idx].timestamp_us -
                                   prey->history[first_idx].timestamp_us) / 1000000.0f;
        if (duration_s > 0.01f && direction_changes > 0) {
            f->turn_frequency_hz = (float)direction_changes / duration_s;
        }
    }

    f->turn_magnitude_rad = f->direction_variance;

    /* Motion smoothness: low variance = smooth */
    f->motion_smoothness = 1.0f / (1.0f + f->speed_variance + f->direction_variance);

    prey->last_feature_update_us = get_time_us();
}

//=============================================================================
// Classification Logic
//=============================================================================

static void classify_prey(
    const prey_classifier_config_t* config,
    tracked_prey_t* prey
) {
    prey_features_t* f = &prey->features;
    prey_classification_t* c = &prey->classification;

    /* Reset probabilities */
    memset(c->type_probabilities, 0, sizeof(c->type_probabilities));

    /* Size-based scoring */
    float size = f->estimated_size_m > 0.0f ? f->estimated_size_m : f->angular_size_rad * 10.0f;

    if (size < 0.003f) {
        c->type_probabilities[PREY_TYPE_MOSQUITO] += 0.5f;
        c->type_probabilities[PREY_TYPE_FLY] += 0.3f;
    } else if (size < 0.008f) {
        c->type_probabilities[PREY_TYPE_FLY] += 0.5f;
        c->type_probabilities[PREY_TYPE_MOSQUITO] += 0.2f;
        c->type_probabilities[PREY_TYPE_BEE] += 0.2f;
    } else if (size < 0.015f) {
        c->type_probabilities[PREY_TYPE_BEE] += 0.3f;
        c->type_probabilities[PREY_TYPE_MOTH] += 0.3f;
        c->type_probabilities[PREY_TYPE_DAMSELFLY] += 0.2f;
    } else if (size < 0.03f) {
        c->type_probabilities[PREY_TYPE_MOTH] += 0.4f;
        c->type_probabilities[PREY_TYPE_BUTTERFLY] += 0.3f;
        c->type_probabilities[PREY_TYPE_DRAGONFLY] += 0.2f;
    } else if (size < 0.06f) {
        c->type_probabilities[PREY_TYPE_DRAGONFLY] += 0.4f;
        c->type_probabilities[PREY_TYPE_BUTTERFLY] += 0.3f;
    } else {
        c->type_probabilities[PREY_TYPE_BIRD] += 0.7f;
        c->type_probabilities[PREY_TYPE_DEBRIS] += 0.2f;
    }

    /* Motion-based scoring */
    if (f->direction_variance > 1.0f) {
        /* Erratic motion */
        c->type_probabilities[PREY_TYPE_MOSQUITO] *= 1.5f;
        c->type_probabilities[PREY_TYPE_BUTTERFLY] *= 1.3f;
        c->type_probabilities[PREY_TYPE_MOTH] *= 0.7f;
    } else if (f->direction_variance < 0.3f) {
        /* Smooth motion */
        c->type_probabilities[PREY_TYPE_MOTH] *= 1.4f;
        c->type_probabilities[PREY_TYPE_DRAGONFLY] *= 1.2f;
        c->type_probabilities[PREY_TYPE_MOSQUITO] *= 0.6f;
    }

    /* Speed-based scoring */
    if (f->avg_speed > 5.0f) {
        /* Fast */
        c->type_probabilities[PREY_TYPE_DRAGONFLY] *= 1.5f;
        c->type_probabilities[PREY_TYPE_FLY] *= 1.3f;
        c->type_probabilities[PREY_TYPE_MOTH] *= 0.5f;
    } else if (f->avg_speed < 1.0f) {
        /* Slow */
        c->type_probabilities[PREY_TYPE_MOTH] *= 1.4f;
        c->type_probabilities[PREY_TYPE_BUTTERFLY] *= 1.2f;
        c->type_probabilities[PREY_TYPE_DRAGONFLY] *= 0.5f;
    }

    /* Normalize and find best */
    float total = 0.0f;
    for (int i = 0; i < PREY_TYPE_COUNT; i++) {
        total += c->type_probabilities[i];
    }
    if (total > 0.0f) {
        for (int i = 0; i < PREY_TYPE_COUNT; i++) {
            c->type_probabilities[i] /= total;
        }
    }

    /* Find best type */
    float best_prob = 0.0f;
    prey_type_t best_type = PREY_TYPE_UNKNOWN;
    for (int i = 0; i < PREY_TYPE_COUNT; i++) {
        if (c->type_probabilities[i] > best_prob) {
            best_prob = c->type_probabilities[i];
            best_type = (prey_type_t)i;
        }
    }

    c->type = best_type;
    c->type_confidence = best_prob;

    /* Behavior classification */
    if (f->avg_speed < 0.5f && f->direction_variance < 0.2f) {
        c->behavior = PREY_BEHAVIOR_HOVERING;
    } else if (f->direction_variance > 1.5f) {
        c->behavior = PREY_BEHAVIOR_ZIGZAG;
    } else if (f->direction_variance < 0.3f) {
        c->behavior = PREY_BEHAVIOR_LINEAR;
    } else {
        c->behavior = PREY_BEHAVIOR_RANDOM;
    }
    c->behavior_confidence = 0.6f;  /* Moderate confidence */

    /* Difficulty assessment */
    float difficulty_score = 0.0f;
    difficulty_score += f->avg_speed * 0.1f;
    difficulty_score += f->direction_variance * 0.5f;
    difficulty_score += f->speed_variance * 0.3f;

    if (difficulty_score < 0.3f) {
        c->difficulty = PREY_DIFFICULTY_EASY;
        c->success_probability = 0.8f;
    } else if (difficulty_score < 0.6f) {
        c->difficulty = PREY_DIFFICULTY_MEDIUM;
        c->success_probability = 0.5f;
    } else if (difficulty_score < 1.0f) {
        c->difficulty = PREY_DIFFICULTY_HARD;
        c->success_probability = 0.3f;
    } else {
        c->difficulty = PREY_DIFFICULTY_EXTREME;
        c->success_probability = 0.1f;
    }

    /* Strategy recommendations */
    c->optimal_lead_factor = 1.0f + f->avg_speed * 0.1f;
    c->recommended_speed = f->avg_speed * 1.5f;
    c->approach_angle_rad = (float)(M_PI / 4.0);  /* 45 degrees */

    c->recommend_abort = (c->type == PREY_TYPE_BIRD ||
                          c->type == PREY_TYPE_DEBRIS ||
                          c->success_probability < config->abort_success_threshold);
    if (c->recommend_abort) {
        if (c->type == PREY_TYPE_BIRD) {
            c->abort_reason = "Target is predator";
        } else if (c->type == PREY_TYPE_DEBRIS) {
            c->abort_reason = "Not prey";
        } else {
            c->abort_reason = "Low success probability";
        }
    } else {
        c->abort_reason = NULL;
    }

    prey->last_classification_us = get_time_us();
}

//=============================================================================
// Configuration Functions
//=============================================================================

prey_classifier_config_t prey_classifier_default_config(void) {
    prey_classifier_config_t config = {
        /* Classification thresholds */
        .min_confidence_threshold = NIMCP_CONFIDENCE_LOW,
        .reclassification_interval_ms = 200.0f,

        /* Feature weights */
        .size_weight = 0.4f,
        .motion_weight = 0.4f,
        .visual_weight = 0.2f,

        /* Behavior analysis */
        .history_window = 32,
        .behavior_switch_threshold = 0.5f,

        /* Strategy parameters */
        .abort_difficulty_threshold = 0.8f,
        .abort_success_threshold = 0.15f,

        /* Learning */
        .enable_learning = false,
        .learning_rate = 0.1f
    };
    return config;
}

bool prey_classifier_validate_config(const prey_classifier_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prey_classifier_validate_config: config is NULL");
        return false;
    }

    if (config->min_confidence_threshold < 0.0f ||
        config->min_confidence_threshold > 1.0f) return false;
    if (config->reclassification_interval_ms < 0.0f) {
        return false;
    }

    float weight_sum = config->size_weight + config->motion_weight + config->visual_weight;
    if (weight_sum < 0.5f || weight_sum > 1.5f) {
        return false;
    }

    if (config->history_window == 0 ||
        config->history_window > PREY_HISTORY_SIZE) return false;

    if (config->abort_success_threshold < 0.0f ||
        config->abort_success_threshold > 1.0f) return false;

    if (config->learning_rate < 0.0f || config->learning_rate > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_prey_classifier_t dragonfly_prey_classifier_create(
    const prey_classifier_config_t* config
) {
    prey_classifier_config_t cfg = config ? *config : prey_classifier_default_config();

    if (!prey_classifier_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_prey_classifier_create: invalid configuration");
        return NULL;
    }

    dragonfly_prey_classifier_t classifier = nimcp_calloc(1, sizeof(struct dragonfly_prey_classifier_s));
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_prey_classifier_create: failed to allocate classifier");
        return NULL;
    }

    classifier->config = cfg;
    classifier->creation_time_us = get_time_us();

    classifier->mutex = nimcp_mutex_create(NULL);
    if (!classifier->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_prey_classifier_create: failed to create mutex");
        nimcp_free(classifier);
        return NULL;
    }

    return classifier;
}

void dragonfly_prey_classifier_destroy(dragonfly_prey_classifier_t classifier) {
    if (!classifier) return;

    if (classifier->mutex) {
        nimcp_mutex_destroy(classifier->mutex);
    }

    nimcp_free(classifier);
}

int dragonfly_prey_classifier_reset(dragonfly_prey_classifier_t classifier) {
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_classifier_reset: classifier is NULL");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);

    memset(classifier->prey, 0, sizeof(classifier->prey));
    classifier->num_prey = 0;

    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

//=============================================================================
// Classification Functions
//=============================================================================

static tracked_prey_t* find_or_create_prey(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id
) {
    /* Find existing */
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (classifier->prey[i].active && classifier->prey[i].id == target_id) {
            return &classifier->prey[i];
        }
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (!classifier->prey[i].active) {
            tracked_prey_t* prey = &classifier->prey[i];
            memset(prey, 0, sizeof(*prey));
            prey->id = target_id;
            prey->active = true;
            classifier->num_prey++;
            return prey;
        }
    }

    /* Find oldest */
    uint64_t oldest_time = UINT64_MAX;
    uint32_t oldest_idx = 0;
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (classifier->prey[i].last_observation_us < oldest_time) {
            oldest_time = classifier->prey[i].last_observation_us;
            oldest_idx = i;
        }
    }

    tracked_prey_t* prey = &classifier->prey[oldest_idx];
    memset(prey, 0, sizeof(*prey));
    prey->id = target_id;
    prey->active = true;
    return prey;
}

int dragonfly_prey_observe(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    const float position[3],
    const float velocity[3],
    float angular_size,
    float contrast
) {
    if (!classifier || !position || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_observe: required parameter is NULL (classifier, position, velocity)");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);

    tracked_prey_t* prey = find_or_create_prey(classifier, target_id);

    /* Add to history */
    motion_history_t* h = &prey->history[prey->history_head];
    memcpy(h->position, position, sizeof(h->position));
    memcpy(h->velocity, velocity, sizeof(h->velocity));
    h->timestamp_us = get_time_us();

    prey->history_head = (prey->history_head + 1) % PREY_HISTORY_SIZE;
    if (prey->history_count < PREY_HISTORY_SIZE) {
        prey->history_count++;
    }

    /* Update last observation */
    memcpy(prey->last_position, position, sizeof(prey->last_position));
    memcpy(prey->last_velocity, velocity, sizeof(prey->last_velocity));
    prey->last_observation_us = h->timestamp_us;

    /* Update visual features */
    prey->features.angular_size_rad = angular_size;
    prey->features.contrast = contrast;

    /* Estimate size from angular size (assuming distance) */
    float distance = vec3_length(position);
    if (distance > 0.01f) {
        prey->features.estimated_size_m = angular_size * distance;
    }

    /* Update motion features periodically */
    update_features(prey);

    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

int dragonfly_prey_classify(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    prey_classification_t* classification
) {
    if (!classifier || !classification) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_classify: required parameter is NULL (classifier, classification)");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);

    /* Find prey */
    tracked_prey_t* prey = NULL;
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (classifier->prey[i].active && classifier->prey[i].id == target_id) {
            prey = &classifier->prey[i];
            break;
        }
    }

    if (!prey) {
        nimcp_mutex_unlock(classifier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_classify: prey is NULL");
        return -1;
    }

    /* Check if reclassification needed */
    uint64_t now = get_time_us();
    if (now - prey->last_classification_us >
        (uint64_t)(classifier->config.reclassification_interval_ms * 1000.0f)) {
        classify_prey(&classifier->config, prey);
        classifier->stats.classifications_made++;
        classifier->stats.type_counts[prey->classification.type]++;
        classifier->stats.avg_confidence =
            (classifier->stats.avg_confidence * (classifier->stats.classifications_made - 1) +
             prey->classification.type_confidence) / classifier->stats.classifications_made;
        if (prey->classification.recommend_abort) {
            classifier->stats.aborts_recommended++;
        }
    }

    *classification = prey->classification;

    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

int dragonfly_prey_predict(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    float horizon_s,
    prey_prediction_t* prediction
) {
    if (!classifier || !prediction || horizon_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_predict: required parameter is NULL (classifier, prediction)");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);

    /* Find prey */
    tracked_prey_t* prey = NULL;
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (classifier->prey[i].active && classifier->prey[i].id == target_id) {
            prey = &classifier->prey[i];
            break;
        }
    }

    if (!prey || prey->history_count < 2) {
        nimcp_mutex_unlock(classifier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_prey_predict: prey is NULL");
        return -1;
    }

    /* Simple constant-velocity prediction with noise based on behavior */
    prediction->predicted_position[0] = prey->last_position[0] +
                                        prey->last_velocity[0] * horizon_s;
    prediction->predicted_position[1] = prey->last_position[1] +
                                        prey->last_velocity[1] * horizon_s;
    prediction->predicted_position[2] = prey->last_position[2] +
                                        prey->last_velocity[2] * horizon_s;

    memcpy(prediction->predicted_velocity, prey->last_velocity,
           sizeof(prediction->predicted_velocity));
    prediction->prediction_time_s = horizon_s;

    /* Confidence based on motion predictability */
    prediction->confidence = prey->features.motion_smoothness;
    prediction->confidence *= 1.0f / (1.0f + horizon_s);  /* Decay with time */

    /* Evasion prediction */
    prediction->evasion_likely = (prey->classification.behavior == PREY_BEHAVIOR_EVASIVE ||
                                  prey->features.direction_variance > 1.0f);
    prediction->evasion_probability = prey->features.direction_variance * 0.3f;
    prediction->evasion_probability = nimcp_clampf(prediction->evasion_probability, 0.0f, 0.9f);

    /* Random expected evasion direction */
    prediction->expected_evasion_dir_rad = atan2f(prey->last_velocity[1],
                                                  prey->last_velocity[0]) +
                                           (float)(M_PI / 2.0);

    classifier->stats.behavior_predictions++;

    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

int dragonfly_prey_get_strategy(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    float self_speed,
    float self_accel,
    prey_classification_t* classification
) {
    if (!classifier || !classification) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_get_strategy: required parameter is NULL (classifier, classification)");
        return -1;
    }

    int result = dragonfly_prey_classify(classifier, target_id, classification);
    if (result != 0) return result;

    /* Adjust strategy based on own capabilities */
    if (self_speed > 0.0f) {
        float speed_ratio = classification->recommended_speed / self_speed;
        if (speed_ratio > 1.0f) {
            /* Target faster than us - need better lead */
            classification->optimal_lead_factor *= speed_ratio;
            classification->success_probability *= 0.8f;
        }
    }

    return 0;
}

int dragonfly_prey_report_outcome(
    dragonfly_prey_classifier_t classifier,
    uint32_t target_id,
    bool success
) {
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_report_outcome: classifier is NULL");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);

    if (success) {
        classifier->stats.correct_predictions++;
    }

    /* Find and deactivate prey */
    for (uint32_t i = 0; i < MAX_TRACKED_PREY; i++) {
        if (classifier->prey[i].active && classifier->prey[i].id == target_id) {
            classifier->prey[i].active = false;
            classifier->num_prey--;
            break;
        }
    }

    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int dragonfly_prey_classifier_get_stats(
    const dragonfly_prey_classifier_t classifier,
    prey_classifier_stats_t* stats
) {
    if (!classifier || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_classifier_get_stats: required parameter is NULL (classifier, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)classifier->mutex);
    *stats = classifier->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)classifier->mutex);

    return 0;
}

int dragonfly_prey_classifier_reset_stats(dragonfly_prey_classifier_t classifier) {
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_prey_classifier_reset_stats: classifier is NULL");
        return -1;
    }

    nimcp_mutex_lock(classifier->mutex);
    memset(&classifier->stats, 0, sizeof(classifier->stats));
    nimcp_mutex_unlock(classifier->mutex);

    return 0;
}

const char* dragonfly_prey_type_name(prey_type_t type) {
    switch (type) {
        case PREY_TYPE_UNKNOWN:    return "UNKNOWN";
        case PREY_TYPE_MOSQUITO:   return "MOSQUITO";
        case PREY_TYPE_FLY:        return "FLY";
        case PREY_TYPE_MOTH:       return "MOTH";
        case PREY_TYPE_BUTTERFLY:  return "BUTTERFLY";
        case PREY_TYPE_BEE:        return "BEE";
        case PREY_TYPE_DRAGONFLY:  return "DRAGONFLY";
        case PREY_TYPE_DAMSELFLY:  return "DAMSELFLY";
        case PREY_TYPE_DEBRIS:     return "DEBRIS";
        case PREY_TYPE_BIRD:       return "BIRD";
        default:                   return "INVALID";
    }
}

const char* dragonfly_prey_behavior_name(prey_behavior_t behavior) {
    switch (behavior) {
        case PREY_BEHAVIOR_LINEAR:     return "LINEAR";
        case PREY_BEHAVIOR_ZIGZAG:     return "ZIGZAG";
        case PREY_BEHAVIOR_SPIRAL:     return "SPIRAL";
        case PREY_BEHAVIOR_HOVERING:   return "HOVERING";
        case PREY_BEHAVIOR_FEEDING:    return "FEEDING";
        case PREY_BEHAVIOR_MATING:     return "MATING";
        case PREY_BEHAVIOR_EVASIVE:    return "EVASIVE";
        case PREY_BEHAVIOR_PATROLLING: return "PATROLLING";
        case PREY_BEHAVIOR_RANDOM:     return "RANDOM";
        default:                       return "UNKNOWN";
    }
}

const char* dragonfly_prey_difficulty_name(prey_difficulty_t difficulty) {
    switch (difficulty) {
        case PREY_DIFFICULTY_EASY:    return "EASY";
        case PREY_DIFFICULTY_MEDIUM:  return "MEDIUM";
        case PREY_DIFFICULTY_HARD:    return "HARD";
        case PREY_DIFFICULTY_EXTREME: return "EXTREME";
        default:                      return "UNKNOWN";
    }
}
