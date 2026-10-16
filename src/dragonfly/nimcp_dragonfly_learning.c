/**
 * @file nimcp_dragonfly_learning.c
 * @brief Learning from Hunt Outcomes Implementation
 *
 * WHAT: Tracks and learns from hunt successes and failures
 * WHY:  Enables continuous improvement of hunting strategies
 * HOW:  Episodic memory with pattern recognition and strategy adaptation
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_learning.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_learning)

/* Helper to compute 3D vector magnitude */
static inline float vec3_len(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Constants
//=============================================================================

#define NUM_STRATEGIES 5  /* Number of intercept strategies */
#define MAX_PATTERNS 32   /* Maximum learned patterns */

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_learning_s {
    /* Configuration */
    learning_config_t config;

    /* Episode memory (ring buffer) */
    hunt_episode_t episodes[LEARNING_MAX_EPISODES];
    uint32_t episode_head;
    uint32_t episode_count;
    uint64_t next_episode_id;

    /* Current hunt tracking */
    bool hunt_in_progress;
    hunt_episode_t current_hunt;
    uint64_t hunt_start_us;

    /* Strategy statistics */
    strategy_effectiveness_t strategy_stats[NUM_STRATEGIES];

    /* Learned patterns */
    learned_pattern_t patterns[MAX_PATTERNS];
    uint32_t num_patterns;

    /* Statistics */
    learning_stats_t stats;

    /* Exploration */
    float current_exploration_rate;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_hunt_outcome_name(hunt_outcome_t outcome) {
    switch (outcome) {
        case OUTCOME_SUCCESS:          return "success";
        case OUTCOME_MISS_CLOSE:       return "miss_close";
        case OUTCOME_MISS_FAR:         return "miss_far";
        case OUTCOME_ESCAPED:          return "escaped";
        case OUTCOME_ABORTED_SELF:     return "aborted_self";
        case OUTCOME_ABORTED_EXTERNAL: return "aborted_external";
        case OUTCOME_TIMEOUT:          return "timeout";
        default:                       return "unknown";
    }
}

const char* dragonfly_failure_reason_name(failure_reason_t reason) {
    switch (reason) {
        case FAIL_REASON_UNKNOWN:     return "unknown";
        case FAIL_REASON_PREDICTION:  return "prediction";
        case FAIL_REASON_EVASION:     return "evasion";
        case FAIL_REASON_SPEED:       return "speed";
        case FAIL_REASON_ENDURANCE:   return "endurance";
        case FAIL_REASON_OBSTRUCTION: return "obstruction";
        case FAIL_REASON_DISTRACTION: return "distraction";
        case FAIL_REASON_APPROACH:    return "approach";
        case FAIL_REASON_TIMING:      return "timing";
        default:                      return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

learning_config_t learning_default_config(void) {
    learning_config_t config = {
        /* Memory */
        .max_episodes = LEARNING_MAX_EPISODES,
        .episode_decay_rate = NIMCP_LEARNING_RATE_DEFAULT,

        /* Pattern detection */
        .min_pattern_confidence = 0.6f,
        .min_observations = 5,
        .similarity_threshold = 0.7f,

        /* Learning rates */
        .strategy_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .pattern_learning_rate = 0.05f,
        .adaptation_rate = 0.1f,

        /* Exploration vs exploitation */
        .exploration_rate = 0.2f,
        .exploration_decay = 0.995f
    };
    return config;
}

bool learning_validate_config(const learning_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "learning_validate_config: config is NULL");
        return false;
    }

    if (config->max_episodes == 0 || config->max_episodes > LEARNING_MAX_EPISODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learning_validate_config: config->max_episodes is zero");
        return false;
    }
    if (config->episode_decay_rate < 0.0f || config->episode_decay_rate > 1.0f) {
        return false;
    }

    if (config->min_pattern_confidence < 0.0f || config->min_pattern_confidence > 1.0f) {
        return false;
    }
    if (config->similarity_threshold < 0.0f || config->similarity_threshold > 1.0f) {
        return false;
    }

    if (config->strategy_learning_rate < 0.0f || config->strategy_learning_rate > 1.0f) {
        return false;
    }
    if (config->pattern_learning_rate < 0.0f || config->pattern_learning_rate > 1.0f) {
        return false;
    }
    if (config->adaptation_rate < 0.0f || config->adaptation_rate > 1.0f) {
        return false;
    }

    if (config->exploration_rate < 0.0f || config->exploration_rate > 1.0f) {
        return false;
    }
    if (config->exploration_decay < 0.0f || config->exploration_decay > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static float compute_feature_similarity(
    const float features_a[LEARNING_FEATURE_DIM],
    const float features_b[LEARNING_FEATURE_DIM]
) {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < LEARNING_FEATURE_DIM; i++) {
        dot += features_a[i] * features_b[i];
        norm_a += features_a[i] * features_a[i];
        norm_b += features_b[i] * features_b[i];
    }

    if (norm_a < 1e-6f || norm_b < 1e-6f) return 0.0f;

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

static void extract_features(
    const hunt_episode_t* episode,
    float features[LEARNING_FEATURE_DIM]
) {
    memset(features, 0, sizeof(float) * LEARNING_FEATURE_DIM);

    /* Target features (0-7) */
    features[0] = episode->target_size;
    features[1] = episode->target_speed / 10.0f;  /* Normalize */
    features[2] = episode->target_maneuverability;
    features[3] = (float)episode->evasion_type / 8.0f;
    features[4] = (float)episode->prey_type / 10.0f;

    /* Hunt features (8-15) */
    features[8] = (float)episode->strategy / 5.0f;
    features[9] = episode->initial_range / 5.0f;
    features[10] = (episode->initial_bearing_rad + M_PI) / (2.0f * M_PI);
    features[11] = episode->pursuit_duration_s / 10.0f;
    features[12] = episode->energy_expended;

    /* Environmental features (16-23) */
    features[16] = episode->wind_speed / 10.0f;
    features[17] = episode->light_level;
    features[18] = episode->time_of_day / 24.0f;

    /* Outcome features (24-31) */
    features[24] = (episode->outcome == OUTCOME_SUCCESS) ? 1.0f : 0.0f;
    features[25] = episode->miss_distance / 1.0f;
    features[26] = episode->final_prediction_error;
}

static void update_strategy_stats(
    dragonfly_learning_t learning,
    const hunt_episode_t* episode
) {
    if (episode->strategy >= NUM_STRATEGIES) return;

    strategy_effectiveness_t* stats = &learning->strategy_stats[episode->strategy];
    float lr = learning->config.strategy_learning_rate;

    stats->strategy = episode->strategy;
    stats->attempts++;

    if (episode->outcome == OUTCOME_SUCCESS) {
        stats->successes++;
    }

    stats->success_rate = (float)stats->successes / (float)stats->attempts;

    /* Update average costs */
    stats->avg_energy_cost = (stats->avg_energy_cost * (stats->attempts - 1) +
                              episode->energy_expended) / stats->attempts;
    stats->avg_pursuit_time_s = (stats->avg_pursuit_time_s * (stats->attempts - 1) +
                                  episode->pursuit_duration_s) / stats->attempts;

    /* Update contextual effectiveness */
    if (episode->prey_type < 10) {
        float old = stats->effectiveness_by_prey[episode->prey_type];
        float new_val = (episode->outcome == OUTCOME_SUCCESS) ? 1.0f : 0.0f;
        stats->effectiveness_by_prey[episode->prey_type] = old + lr * (new_val - old);
    }

    if (episode->evasion_type < 8) {
        float old = stats->effectiveness_by_evasion[episode->evasion_type];
        float new_val = (episode->outcome == OUTCOME_SUCCESS) ? 1.0f : 0.0f;
        stats->effectiveness_by_evasion[episode->evasion_type] = old + lr * (new_val - old);
    }
}

static void try_learn_pattern(dragonfly_learning_t learning) {
    if (learning->episode_count < learning->config.min_observations) return;

    /* Look for clusters of similar episodes */
    float centroid[LEARNING_FEATURE_DIM] = {0};
    uint32_t cluster_count = 0;
    uint32_t successes = 0;

    /* Use most recent episode as seed */
    uint32_t seed_idx = (learning->episode_head + LEARNING_MAX_EPISODES - 1) % LEARNING_MAX_EPISODES;
    hunt_episode_t* seed = &learning->episodes[seed_idx];

    float seed_features[LEARNING_FEATURE_DIM];
    extract_features(seed, seed_features);

    /* Find similar episodes */
    for (uint32_t i = 0; i < learning->episode_count; i++) {
        uint32_t idx = (learning->episode_head + LEARNING_MAX_EPISODES - 1 - i) % LEARNING_MAX_EPISODES;
        hunt_episode_t* ep = &learning->episodes[idx];

        float ep_features[LEARNING_FEATURE_DIM];
        extract_features(ep, ep_features);

        float sim = compute_feature_similarity(seed_features, ep_features);
        if (sim >= learning->config.similarity_threshold) {
            for (int j = 0; j < LEARNING_FEATURE_DIM; j++) {
                centroid[j] += ep_features[j];
            }
            cluster_count++;
            if (ep->outcome == OUTCOME_SUCCESS) {
                successes++;
            }
        }
    }

    if (cluster_count < learning->config.min_observations) return;

    /* Normalize centroid */
    for (int i = 0; i < LEARNING_FEATURE_DIM; i++) {
        centroid[i] /= (float)cluster_count;
    }

    float success_rate = (float)successes / (float)cluster_count;
    float confidence = (float)cluster_count / (float)learning->episode_count;

    if (confidence < learning->config.min_pattern_confidence) return;

    /* Add or update pattern */
    if (learning->num_patterns < MAX_PATTERNS) {
        learned_pattern_t* pattern = &learning->patterns[learning->num_patterns];
        pattern->pattern_id = learning->num_patterns;
        pattern->description = (success_rate > 0.5f) ? "successful_pattern" : "failure_pattern";

        memcpy(pattern->trigger_features, centroid, sizeof(centroid));

        /* Feature importance (simplified: use variance) */
        for (int i = 0; i < LEARNING_FEATURE_DIM; i++) {
            pattern->feature_importance[i] = 1.0f / LEARNING_FEATURE_DIM;
        }

        pattern->typical_outcome = (success_rate > 0.5f) ? OUTCOME_SUCCESS : OUTCOME_MISS_FAR;
        pattern->success_rate = success_rate;
        pattern->recommended_strategy = seed->strategy;
        pattern->recommended_lead_factor = 1.0f;
        pattern->recommended_speed = 1.0f;
        pattern->observation_count = cluster_count;
        pattern->confidence = confidence;

        learning->num_patterns++;
        learning->stats.patterns_learned++;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_learning_t dragonfly_learning_create(const learning_config_t* config) {
    learning_config_t cfg = config ? *config : learning_default_config();

    if (!learning_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_learning_create: invalid configuration");
        return NULL;
    }

    dragonfly_learning_t learning = nimcp_calloc(1, sizeof(struct dragonfly_learning_s));
    if (!learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_learning_create: failed to allocate learning");
        return NULL;
    }

    learning->config = cfg;
    learning->creation_time_us = get_time_us();
    learning->current_exploration_rate = cfg.exploration_rate;

    /* Initialize strategy stats */
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        learning->strategy_stats[i].strategy = (intercept_strategy_t)i;
        learning->strategy_stats[i].success_rate = 0.5f;  /* Prior */
    }

    learning->mutex = nimcp_mutex_create(NULL);
    if (!learning->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_learning_create: failed to create mutex");
        nimcp_free(learning);
        return NULL;
    }

    return learning;
}

void dragonfly_learning_destroy(dragonfly_learning_t learning) {
    if (!learning) return;

    if (learning->mutex) {
        nimcp_mutex_destroy(learning->mutex);
    }

    nimcp_free(learning);
}

int dragonfly_learning_reset(dragonfly_learning_t learning) {
    if (!learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_reset: learning is NULL");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    learning->episode_head = 0;
    learning->episode_count = 0;
    learning->next_episode_id = 0;
    learning->hunt_in_progress = false;
    learning->num_patterns = 0;

    for (int i = 0; i < NUM_STRATEGIES; i++) {
        memset(&learning->strategy_stats[i], 0, sizeof(strategy_effectiveness_t));
        learning->strategy_stats[i].strategy = (intercept_strategy_t)i;
        learning->strategy_stats[i].success_rate = 0.5f;
    }

    memset(&learning->stats, 0, sizeof(learning->stats));
    learning->current_exploration_rate = learning->config.exploration_rate;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

//=============================================================================
// Episode Recording Functions
//=============================================================================

int dragonfly_learning_record_episode(
    dragonfly_learning_t learning,
    const hunt_episode_t* episode
) {
    if (!learning || !episode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_record_episode: required parameter is NULL (learning, episode)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    /* Store episode */
    learning->episodes[learning->episode_head] = *episode;
    learning->episodes[learning->episode_head].episode_id = learning->next_episode_id++;
    learning->episodes[learning->episode_head].timestamp_us = get_time_us();

    /* Extract and store features */
    extract_features(episode, learning->episodes[learning->episode_head].features);

    learning->episode_head = (learning->episode_head + 1) % LEARNING_MAX_EPISODES;
    if (learning->episode_count < LEARNING_MAX_EPISODES) {
        learning->episode_count++;
    }

    /* Update strategy statistics */
    update_strategy_stats(learning, episode);

    /* Update overall statistics */
    learning->stats.episodes_recorded++;

    if (episode->outcome == OUTCOME_SUCCESS) {
        learning->stats.overall_success_rate =
            (learning->stats.overall_success_rate * (learning->stats.episodes_recorded - 1) + 1.0f) /
            learning->stats.episodes_recorded;
    } else {
        learning->stats.overall_success_rate =
            (learning->stats.overall_success_rate * (learning->stats.episodes_recorded - 1)) /
            learning->stats.episodes_recorded;
    }

    /* Try to learn patterns */
    try_learn_pattern(learning);

    /* Decay exploration rate */
    learning->current_exploration_rate *= learning->config.exploration_decay;
    learning->current_exploration_rate = nimcp_clampf(learning->current_exploration_rate, 0.01f, 1.0f);

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_begin_hunt(
    dragonfly_learning_t learning,
    const dragonfly_target_info_t* target,
    intercept_strategy_t strategy
) {
    if (!learning || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_begin_hunt: required parameter is NULL (learning, target)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    learning->hunt_in_progress = true;
    learning->hunt_start_us = get_time_us();

    memset(&learning->current_hunt, 0, sizeof(learning->current_hunt));
    /* Derive values from actual dragonfly_target_info_t fields */
    float target_speed = vec3_len(target->velocity);
    float initial_range = vec3_len(target->position);
    float bearing = atan2f(target->position[1], target->position[0]);
    /* Estimate maneuverability from evasion type */
    float maneuverability = (target->evasion_type >= EVASION_JINK) ? 0.5f : 0.2f;
    if (target->evasion_type >= EVASION_SPIRAL) maneuverability = 0.8f;

    learning->current_hunt.target_size = target->confidence;  /* Use confidence as size proxy */
    learning->current_hunt.target_speed = target_speed;
    learning->current_hunt.target_maneuverability = maneuverability;
    learning->current_hunt.strategy = strategy;
    learning->current_hunt.initial_range = initial_range;
    learning->current_hunt.initial_bearing_rad = bearing;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_end_hunt(
    dragonfly_learning_t learning,
    hunt_outcome_t outcome,
    failure_reason_t reason,
    float miss_distance
) {
    if (!learning || !learning->hunt_in_progress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_end_hunt: required parameter is NULL (learning, learning->hunt_in_progress)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    uint64_t now = get_time_us();
    learning->current_hunt.pursuit_duration_s = (float)(now - learning->hunt_start_us) / 1000000.0f;
    learning->current_hunt.outcome = outcome;
    learning->current_hunt.failure_reason = reason;
    learning->current_hunt.miss_distance = miss_distance;

    learning->hunt_in_progress = false;

    /* Record the episode */
    nimcp_mutex_unlock(learning->mutex);
    dragonfly_learning_record_episode(learning, &learning->current_hunt);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_learning_get_recommendation(
    dragonfly_learning_t learning,
    const dragonfly_target_info_t* target,
    learning_recommendation_t* recommendation
) {
    if (!learning || !target || !recommendation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_recommendation: required parameter is NULL (learning, target, recommendation)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    /* Create feature vector for current situation */
    hunt_episode_t query = {0};
    /* Derive values from actual dragonfly_target_info_t fields */
    float target_speed = vec3_len(target->velocity);
    float initial_range = vec3_len(target->position);
    float maneuverability = (target->evasion_type >= EVASION_JINK) ? 0.5f : 0.2f;
    if (target->evasion_type >= EVASION_SPIRAL) maneuverability = 0.8f;

    query.target_size = target->confidence;  /* Use confidence as size proxy */
    query.target_speed = target_speed;
    query.target_maneuverability = maneuverability;
    query.initial_range = initial_range;

    float query_features[LEARNING_FEATURE_DIM];
    extract_features(&query, query_features);

    /* Find best matching pattern */
    float best_match = -1.0f;
    learned_pattern_t* best_pattern = NULL;

    for (uint32_t i = 0; i < learning->num_patterns; i++) {
        float sim = compute_feature_similarity(query_features, learning->patterns[i].trigger_features);
        if (sim > best_match) {
            best_match = sim;
            best_pattern = &learning->patterns[i];
        }
    }

    /* Find best strategy */
    intercept_strategy_t best_strategy = INTERCEPT_PURSUIT;
    float best_rate = 0.0f;

    for (int i = 0; i < NUM_STRATEGIES; i++) {
        if (learning->strategy_stats[i].success_rate > best_rate) {
            best_rate = learning->strategy_stats[i].success_rate;
            best_strategy = (intercept_strategy_t)i;
        }
    }

    /* Fill recommendation */
    if (best_pattern && best_match > learning->config.similarity_threshold) {
        recommendation->recommended_strategy = best_pattern->recommended_strategy;
        recommendation->strategy_confidence = best_pattern->confidence * best_match;
        recommendation->predicted_success_rate = best_pattern->success_rate;
        recommendation->lead_factor_adjustment = best_pattern->recommended_lead_factor - 1.0f;
        recommendation->speed_adjustment = best_pattern->recommended_speed - 1.0f;

        recommendation->difficult_target = (best_pattern->success_rate < 0.3f);
        recommendation->advice = best_pattern->description;
    } else {
        recommendation->recommended_strategy = best_strategy;
        recommendation->strategy_confidence = best_rate;
        recommendation->predicted_success_rate = best_rate;
        recommendation->lead_factor_adjustment = 0.0f;
        recommendation->speed_adjustment = 0.0f;

        recommendation->difficult_target = (maneuverability > 0.7f);
        recommendation->advice = "using_best_strategy";
    }

    /* Exploration: sometimes recommend random strategy */
    if (nimcp_rand_uniform() < learning->current_exploration_rate) {
        recommendation->recommended_strategy = (intercept_strategy_t)(nimcp_rand_uint(NUM_STRATEGIES));
        recommendation->strategy_confidence *= 0.5f;
        recommendation->advice = "exploring";
    }

    recommendation->aggression_adjustment = 0.0f;

    learning->stats.recommendations_given++;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_get_strategy_stats(
    const dragonfly_learning_t learning,
    intercept_strategy_t strategy,
    strategy_effectiveness_t* stats
) {
    if (!learning || !stats || strategy >= NUM_STRATEGIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_strategy_stats: required parameter is NULL (learning, stats)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);
    *stats = learning->strategy_stats[strategy];
    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_get_all_strategy_stats(
    const dragonfly_learning_t learning,
    strategy_effectiveness_t* stats,
    uint32_t* num_strategies
) {
    if (!learning || !stats || !num_strategies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_all_strategy_stats: required parameter is NULL (learning, stats, num_strategies)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    memcpy(stats, learning->strategy_stats, sizeof(learning->strategy_stats));
    *num_strategies = NUM_STRATEGIES;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_get_patterns(
    const dragonfly_learning_t learning,
    learned_pattern_t* patterns,
    uint32_t max_patterns,
    uint32_t* num_patterns
) {
    if (!learning || !patterns || !num_patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_patterns: required parameter is NULL (learning, patterns, num_patterns)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    uint32_t count = learning->num_patterns < max_patterns ?
                     learning->num_patterns : max_patterns;
    memcpy(patterns, learning->patterns, count * sizeof(learned_pattern_t));
    *num_patterns = count;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_get_recent_episodes(
    const dragonfly_learning_t learning,
    hunt_episode_t* episodes,
    uint32_t max_episodes,
    uint32_t* num_episodes
) {
    if (!learning || !episodes || !num_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_recent_episodes: required parameter is NULL (learning, episodes, num_episodes)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);

    uint32_t count = learning->episode_count < max_episodes ?
                     learning->episode_count : max_episodes;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (learning->episode_head + LEARNING_MAX_EPISODES - 1 - i) % LEARNING_MAX_EPISODES;
        episodes[i] = learning->episodes[idx];
    }

    *num_episodes = count;

    nimcp_mutex_unlock(learning->mutex);

    return 0;
}

int dragonfly_learning_get_stats(
    const dragonfly_learning_t learning,
    learning_stats_t* stats
) {
    if (!learning || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_learning_get_stats: required parameter is NULL (learning, stats)");
        return -1;
    }

    nimcp_mutex_lock(learning->mutex);
    *stats = learning->stats;
    nimcp_mutex_unlock(learning->mutex);

    return 0;
}
