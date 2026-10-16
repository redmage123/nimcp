/**
 * @file nimcp_dragonfly_multi_target.c
 * @brief Multi-Target Priority Queue and Rapid Switching Implementation
 *
 * WHAT: Manages priority queue of target candidates
 * WHY:  Enables rapid target switching on miss or escape
 * HOW:  Continuous parallel evaluation of alternative targets
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_multi_target.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_multi_target)

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

static inline float vec3_distance(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Copy detection position to output
 *
 * Detection already contains world coordinates in position[3].
 */
static inline void detection_to_position(
    const dragonfly_detection_t* detection,
    const float self_pos[3],
    float out_pos[3]
) {
    (void)self_pos;  /* Detection already in world coordinates */
    out_pos[0] = detection->position[0];
    out_pos[1] = detection->position[1];
    out_pos[2] = detection->position[2];
}

/**
 * @brief Convert motion direction to velocity vector
 */
static inline void detection_to_velocity(
    const dragonfly_detection_t* detection,
    float out_vel[3]
) {
    out_vel[0] = cosf(detection->motion_direction_rad) * detection->motion_speed;
    out_vel[1] = sinf(detection->motion_direction_rad) * detection->motion_speed;
    out_vel[2] = 0.0f;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_multi_target_s {
    /* Configuration */
    multi_target_config_t config;

    /* Target queue */
    queued_target_t targets[MULTI_TARGET_MAX_QUEUE];
    uint32_t num_targets;

    /* Primary target */
    uint32_t primary_index;
    bool has_primary;
    uint64_t primary_lock_time_us;

    /* Switch history */
    switch_event_t history[MULTI_TARGET_HISTORY_SIZE];
    uint32_t history_head;
    uint32_t history_count;

    /* State */
    multi_target_state_t state;

    /* Statistics */
    multi_target_stats_t stats;

    /* Self position cache */
    float self_position[3];

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_target_status_name(target_status_t status) {
    switch (status) {
        case TARGET_STATUS_CANDIDATE:  return "candidate";
        case TARGET_STATUS_PRIMARY:    return "primary";
        case TARGET_STATUS_BACKUP:     return "backup";
        case TARGET_STATUS_EVALUATING: return "evaluating";
        case TARGET_STATUS_REJECTED:   return "rejected";
        case TARGET_STATUS_LOST:       return "lost";
        default:                       return "unknown";
    }
}

const char* dragonfly_switch_reason_name(switch_reason_t reason) {
    switch (reason) {
        case SWITCH_TARGET_ESCAPED:   return "target_escaped";
        case SWITCH_TARGET_LOST:      return "target_lost";
        case SWITCH_BETTER_AVAILABLE: return "better_available";
        case SWITCH_OBSTRUCTION:      return "obstruction";
        case SWITCH_ENERGY_SAVE:      return "energy_save";
        case SWITCH_MANUAL:           return "manual";
        default:                      return "unknown";
    }
}

const char* dragonfly_priority_factor_name(priority_factor_t factor) {
    switch (factor) {
        case PRIORITY_DISTANCE:    return "distance";
        case PRIORITY_SIZE:        return "size";
        case PRIORITY_VELOCITY:    return "velocity";
        case PRIORITY_FEASIBILITY: return "feasibility";
        case PRIORITY_SALIENCE:    return "salience";
        case PRIORITY_ISOLATION:   return "isolation";
        default:                   return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

multi_target_config_t multi_target_default_config(void) {
    multi_target_config_t config = {
        /* Queue settings */
        .max_queue_size = MULTI_TARGET_MAX_QUEUE,
        .min_confidence_threshold = NIMCP_CONFIDENCE_LOW,
        .rejection_threshold = 0.1f,

        /* Priority weights */
        .priority_weights = {
            0.25f,  /* PRIORITY_DISTANCE */
            0.15f,  /* PRIORITY_SIZE */
            0.20f,  /* PRIORITY_VELOCITY */
            0.20f,  /* PRIORITY_FEASIBILITY */
            0.10f,  /* PRIORITY_SALIENCE */
            0.10f   /* PRIORITY_ISOLATION */
        },

        /* Switching parameters */
        .switch_hysteresis = 0.1f,
        .min_lock_time_s = 0.5f,
        .better_target_margin = 0.2f,

        /* Precomputation */
        .enable_parallel_evaluation = true,
        .evaluation_budget = 4,

        /* History */
        .track_switch_history = true,
        .recent_target_penalty = 0.2f
    };
    return config;
}

bool multi_target_validate_config(const multi_target_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multi_target_validate_config: config is NULL");
        return false;
    }

    if (config->max_queue_size == 0 || config->max_queue_size > MULTI_TARGET_MAX_QUEUE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multi_target_validate_config: config->max_queue_size is zero");
        return false;
    }
    if (config->min_confidence_threshold < 0.0f || config->min_confidence_threshold > 1.0f) {
        return false;
    }
    if (config->rejection_threshold < 0.0f || config->rejection_threshold > 1.0f) {
        return false;
    }

    /* Check weights sum to ~1 */
    float weight_sum = 0.0f;
    for (int i = 0; i < PRIORITY_COUNT; i++) {
        if (config->priority_weights[i] < 0.0f) {
            return false;
        }
        weight_sum += config->priority_weights[i];
    }
    if (weight_sum < 0.5f || weight_sum > 1.5f) {
        return false;
    }

    if (config->min_lock_time_s < 0.0f) {
        return false;
    }
    if (config->better_target_margin < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static int find_target_by_id(const dragonfly_multi_target_t mt, uint32_t id) {
    for (uint32_t i = 0; i < mt->num_targets; i++) {
        if (mt->targets[i].id == id) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

static void compute_priority_scores(
    dragonfly_multi_target_t mt,
    queued_target_t* target,
    const dragonfly_self_state_t* self_state
) {
    /* Distance score (closer is better) */
    float distance = vec3_distance(target->position, self_state->position);
    target->factor_scores[PRIORITY_DISTANCE] = 1.0f / (1.0f + distance * 0.5f);

    /* Size score (larger is better, within limits) */
    target->factor_scores[PRIORITY_SIZE] = nimcp_clampf(target->size * 10.0f, 0.0f, 1.0f);

    /* Velocity score (slower is easier) */
    float speed = vec3_length(target->velocity);
    target->factor_scores[PRIORITY_VELOCITY] = 1.0f / (1.0f + speed * 0.3f);

    /* Feasibility score */
    target->factor_scores[PRIORITY_FEASIBILITY] = target->success_probability;

    /* Salience score (based on confidence) */
    target->factor_scores[PRIORITY_SALIENCE] = target->confidence;

    /* Isolation score (placeholder - would need swarm data) */
    target->factor_scores[PRIORITY_ISOLATION] = 0.5f;

    /* Compute weighted sum */
    float priority = 0.0f;
    for (int i = 0; i < PRIORITY_COUNT; i++) {
        priority += target->factor_scores[i] * mt->config.priority_weights[i];
    }

    target->priority_score = nimcp_clampf(priority, 0.0f, 1.0f);
}

static void sort_targets_by_priority(dragonfly_multi_target_t mt) {
    /* Simple bubble sort (queue is small) */
    for (uint32_t i = 0; i < mt->num_targets; i++) {
        for (uint32_t j = i + 1; j < mt->num_targets; j++) {
            if (mt->targets[j].priority_score > mt->targets[i].priority_score) {
                queued_target_t temp = mt->targets[i];
                mt->targets[i] = mt->targets[j];
                mt->targets[j] = temp;
            }
        }
    }

    /* Update primary index if we have one */
    if (mt->has_primary) {
        for (uint32_t i = 0; i < mt->num_targets; i++) {
            if (mt->targets[i].status == TARGET_STATUS_PRIMARY) {
                mt->primary_index = i;
                break;
            }
        }
    }
}

static void record_switch_event(
    dragonfly_multi_target_t mt,
    uint32_t from_id,
    uint32_t to_id,
    switch_reason_t reason
) {
    if (!mt->config.track_switch_history) return;

    switch_event_t event = {
        .from_target_id = from_id,
        .to_target_id = to_id,
        .reason = reason,
        .switch_time_ms = 0.0f,  /* Would measure actual time */
        .timestamp_us = get_time_us()
    };

    mt->history[mt->history_head] = event;
    mt->history_head = (mt->history_head + 1) % MULTI_TARGET_HISTORY_SIZE;
    if (mt->history_count < MULTI_TARGET_HISTORY_SIZE) {
        mt->history_count++;
    }

    mt->stats.switch_reasons[reason]++;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_multi_target_t dragonfly_multi_target_create(
    const multi_target_config_t* config
) {
    multi_target_config_t cfg = config ? *config : multi_target_default_config();

    if (!multi_target_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_multi_target_create: invalid configuration");
        return NULL;
    }

    dragonfly_multi_target_t mt = nimcp_calloc(1, sizeof(struct dragonfly_multi_target_s));
    if (!mt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_multi_target_create: failed to allocate multi_target");
        return NULL;
    }

    mt->config = cfg;
    mt->creation_time_us = get_time_us();
    mt->primary_index = 0;
    mt->has_primary = false;

    mt->mutex = nimcp_mutex_create(NULL);
    if (!mt->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_multi_target_create: failed to create mutex");
        nimcp_free(mt);
        return NULL;
    }

    return mt;
}

void dragonfly_multi_target_destroy(dragonfly_multi_target_t mt) {
    if (!mt) return;

    if (mt->mutex) {
        nimcp_mutex_destroy(mt->mutex);
    }

    nimcp_free(mt);
}

int dragonfly_multi_target_reset(dragonfly_multi_target_t mt) {
    if (!mt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_reset: mt is NULL");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    mt->num_targets = 0;
    mt->has_primary = false;
    mt->primary_index = 0;
    mt->history_head = 0;
    mt->history_count = 0;
    memset(&mt->state, 0, sizeof(mt->state));

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

//=============================================================================
// Queue Management Functions
//=============================================================================

int dragonfly_multi_target_update(
    dragonfly_multi_target_t mt,
    const dragonfly_detection_t* detection,
    const dragonfly_self_state_t* self_state
) {
    if (!mt || !detection || !self_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_update: required parameter is NULL (mt, detection, self_state)");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    /* Update self position cache */
    memcpy(mt->self_position, self_state->position, sizeof(mt->self_position));

    /* Find existing target or add new one */
    int idx = find_target_by_id(mt, detection->id);

    if (idx >= 0) {
        /* Update existing target */
        queued_target_t* target = &mt->targets[idx];

        /* Copy position from detection (already in world coordinates) */
        detection_to_position(detection, self_state->position, target->position);
        /* Construct velocity from motion direction and speed */
        detection_to_velocity(detection, target->velocity);
        /* Use size directly from detection */
        target->size = detection->size;
        target->confidence = detection->contrast;  /* Use contrast as confidence proxy */
        target->last_seen_us = get_time_us();
        target->observations++;

        /* Recompute priority */
        compute_priority_scores(mt, target, self_state);
    } else if (mt->num_targets < mt->config.max_queue_size) {
        /* Add new target */
        if (detection->contrast >= mt->config.min_confidence_threshold) {
            queued_target_t* target = &mt->targets[mt->num_targets];

            target->id = detection->id;
            target->status = TARGET_STATUS_CANDIDATE;
            /* Copy position from detection (already in world coordinates) */
            detection_to_position(detection, self_state->position, target->position);
            /* Construct velocity from motion direction and speed */
            detection_to_velocity(detection, target->velocity);
            /* Use size directly from detection */
            target->size = detection->size;
            target->confidence = detection->contrast;  /* Use contrast as confidence proxy */
            target->first_seen_us = get_time_us();
            target->last_seen_us = target->first_seen_us;
            target->observations = 1;

            /* Estimate feasibility */
            float distance = vec3_distance(detection->position, self_state->position);
            float speed = detection->motion_speed;
            float closing_speed = self_state->max_speed - speed * 0.5f;
            target->intercept_time_s = (closing_speed > 0.01f) ?
                distance / closing_speed : distance / 0.01f;
            target->success_probability = 0.5f;  /* Initial estimate */
            target->energy_cost = target->intercept_time_s * 0.1f;

            compute_priority_scores(mt, target, self_state);

            mt->num_targets++;
            mt->stats.targets_queued++;
        }
    }

    /* Sort by priority */
    sort_targets_by_priority(mt);

    /* Update state */
    mt->state.num_targets = mt->num_targets;
    if (mt->num_targets > 1) {
        mt->state.has_backup = true;
        for (uint32_t i = 0; i < mt->num_targets; i++) {
            if (mt->targets[i].status != TARGET_STATUS_PRIMARY) {
                mt->state.backup_target_id = mt->targets[i].id;
                break;
            }
        }
    } else {
        mt->state.has_backup = false;
    }

    mt->last_update_us = get_time_us();

    /* Update average queue size */
    mt->stats.avg_queue_size = (mt->stats.avg_queue_size * mt->stats.targets_queued +
                                mt->num_targets) / (mt->stats.targets_queued + 1);

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

int dragonfly_multi_target_remove(
    dragonfly_multi_target_t mt,
    uint32_t target_id
) {
    if (!mt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_remove: mt is NULL");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    int idx = find_target_by_id(mt, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock(mt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_remove: validation failed");
        return -1;
    }

    /* Check if removing primary */
    if (mt->has_primary && mt->targets[idx].status == TARGET_STATUS_PRIMARY) {
        mt->has_primary = false;
    }

    /* Shift remaining targets */
    for (uint32_t i = idx; i < mt->num_targets - 1; i++) {
        mt->targets[i] = mt->targets[i + 1];
    }
    mt->num_targets--;

    /* Update state */
    mt->state.num_targets = mt->num_targets;

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

int dragonfly_multi_target_set_primary(
    dragonfly_multi_target_t mt,
    uint32_t target_id
) {
    if (!mt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_set_primary: mt is NULL");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    int idx = find_target_by_id(mt, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock(mt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_set_primary: validation failed");
        return -1;
    }

    /* Clear old primary */
    if (mt->has_primary) {
        mt->targets[mt->primary_index].status = TARGET_STATUS_CANDIDATE;
    }

    /* Set new primary */
    mt->targets[idx].status = TARGET_STATUS_PRIMARY;
    mt->primary_index = idx;
    mt->has_primary = true;
    mt->primary_lock_time_us = get_time_us();

    mt->state.primary_target_id = target_id;

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

int dragonfly_multi_target_evaluate(
    dragonfly_multi_target_t mt,
    const dragonfly_self_state_t* self_state
) {
    if (!mt || !self_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_evaluate: required parameter is NULL (mt, self_state)");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    uint64_t now = get_time_us();
    uint32_t evaluated = 0;

    for (uint32_t i = 0; i < mt->num_targets && evaluated < mt->config.evaluation_budget; i++) {
        queued_target_t* target = &mt->targets[i];

        /* Skip stale targets */
        if ((now - target->last_seen_us) > 1000000) {  /* 1 second timeout */
            target->status = TARGET_STATUS_LOST;
            continue;
        }

        /* Recompute priorities */
        compute_priority_scores(mt, target, self_state);

        /* Update status */
        if (target->status != TARGET_STATUS_PRIMARY) {
            if (target->success_probability < mt->config.rejection_threshold) {
                target->status = TARGET_STATUS_REJECTED;
                mt->stats.targets_rejected++;
            } else if (i == 1 && target->status != TARGET_STATUS_BACKUP) {
                target->status = TARGET_STATUS_BACKUP;
            } else {
                target->status = TARGET_STATUS_CANDIDATE;
            }
        }

        evaluated++;
    }

    /* Sort by updated priorities */
    sort_targets_by_priority(mt);

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

//=============================================================================
// Switching Functions
//=============================================================================

bool dragonfly_multi_target_should_switch(
    const dragonfly_multi_target_t mt,
    switch_reason_t* reason
) {
    if (!mt || mt->num_targets < 2 || !mt->has_primary) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);

    bool should_switch = false;
    switch_reason_t switch_reason = SWITCH_MANUAL;

    uint64_t now = get_time_us();
    float lock_duration_s = (float)(now - mt->primary_lock_time_us) / 1000000.0f;

    /* Check minimum lock time */
    if (lock_duration_s < mt->config.min_lock_time_s) {
        nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);
        return false;
    }

    const queued_target_t* primary = &mt->targets[mt->primary_index];

    /* Check if primary is lost */
    if (primary->status == TARGET_STATUS_LOST) {
        should_switch = true;
        switch_reason = SWITCH_TARGET_LOST;
    }
    /* Check if primary confidence is too low */
    else if (primary->confidence < mt->config.min_confidence_threshold) {
        should_switch = true;
        switch_reason = SWITCH_TARGET_ESCAPED;
    }
    /* Check if better target available */
    else {
        for (uint32_t i = 0; i < mt->num_targets; i++) {
            if (i == mt->primary_index) continue;

            const queued_target_t* candidate = &mt->targets[i];
            if (candidate->status == TARGET_STATUS_REJECTED ||
                candidate->status == TARGET_STATUS_LOST) continue;

            float margin = candidate->priority_score - primary->priority_score;
            if (margin > mt->config.better_target_margin + mt->config.switch_hysteresis) {
                should_switch = true;
                switch_reason = SWITCH_BETTER_AVAILABLE;
                break;
            }
        }
    }

    if (reason) {
        *reason = switch_reason;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return should_switch;
}

int dragonfly_multi_target_switch(
    dragonfly_multi_target_t mt,
    switch_reason_t reason,
    switch_event_t* event
) {
    if (!mt || mt->num_targets < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_switch: mt is NULL");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    /* Find best non-primary target */
    int best_idx = -1;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < mt->num_targets; i++) {
        if (i == mt->primary_index) continue;
        if (mt->targets[i].status == TARGET_STATUS_REJECTED ||
            mt->targets[i].status == TARGET_STATUS_LOST) continue;

        if (mt->targets[i].priority_score > best_score) {
            best_score = mt->targets[i].priority_score;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        nimcp_mutex_unlock(mt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_switch: validation failed");
        return -1;
    }

    /* Record switch */
    uint32_t old_id = mt->has_primary ? mt->targets[mt->primary_index].id : 0;
    uint32_t new_id = mt->targets[best_idx].id;

    record_switch_event(mt, old_id, new_id, reason);

    /* Update status */
    if (mt->has_primary) {
        mt->targets[mt->primary_index].status = TARGET_STATUS_CANDIDATE;
    }
    mt->targets[best_idx].status = TARGET_STATUS_PRIMARY;
    mt->primary_index = best_idx;
    mt->has_primary = true;
    mt->primary_lock_time_us = get_time_us();

    /* Update state */
    mt->state.primary_target_id = new_id;
    mt->state.switches_this_session++;
    mt->stats.switches_performed++;

    /* Fill event if provided */
    if (event) {
        event->from_target_id = old_id;
        event->to_target_id = new_id;
        event->reason = reason;
        event->switch_time_ms = 0.0f;  /* Would measure */
        event->timestamp_us = get_time_us();
    }

    /* Update average switch latency (EMA, no new latency measurement available) */
    if (mt->stats.switches_performed > 1) {
        mt->stats.avg_switch_latency_ms = mt->stats.avg_switch_latency_ms *
            (float)(mt->stats.switches_performed - 1) /
            (float)mt->stats.switches_performed;
    }

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

int dragonfly_multi_target_switch_to(
    dragonfly_multi_target_t mt,
    uint32_t target_id,
    switch_reason_t reason
) {
    if (!mt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_switch_to: mt is NULL");
        return -1;
    }

    nimcp_mutex_lock(mt->mutex);

    int idx = find_target_by_id(mt, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock(mt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_switch_to: validation failed");
        return -1;
    }

    /* Record switch */
    uint32_t old_id = mt->has_primary ? mt->targets[mt->primary_index].id : 0;

    record_switch_event(mt, old_id, target_id, reason);

    /* Update status */
    if (mt->has_primary) {
        mt->targets[mt->primary_index].status = TARGET_STATUS_CANDIDATE;
    }
    mt->targets[idx].status = TARGET_STATUS_PRIMARY;
    mt->primary_index = idx;
    mt->has_primary = true;
    mt->primary_lock_time_us = get_time_us();

    /* Update state */
    mt->state.primary_target_id = target_id;
    mt->state.switches_this_session++;
    mt->stats.switches_performed++;

    nimcp_mutex_unlock(mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_backup(
    const dragonfly_multi_target_t mt,
    queued_target_t* backup
) {
    if (!mt || !backup || mt->num_targets < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_backup: required parameter is NULL (mt, backup)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);

    /* Find best non-primary */
    for (uint32_t i = 0; i < mt->num_targets; i++) {
        if (i == mt->primary_index) continue;
        if (mt->targets[i].status == TARGET_STATUS_REJECTED ||
            mt->targets[i].status == TARGET_STATUS_LOST) continue;

        *backup = mt->targets[i];
        nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);
        return 0;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);
    return -1;  /* No suitable backup target found */
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_multi_target_get_primary(
    const dragonfly_multi_target_t mt,
    queued_target_t* primary
) {
    if (!mt || !primary || !mt->has_primary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_primary: required parameter is NULL (mt, primary, mt->has_primary)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);
    *primary = mt->targets[mt->primary_index];
    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_queue(
    const dragonfly_multi_target_t mt,
    queued_target_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
) {
    if (!mt || !targets || !num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_queue: required parameter is NULL (mt, targets, num_targets)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);

    uint32_t count = mt->num_targets < max_targets ? mt->num_targets : max_targets;
    memcpy(targets, mt->targets, count * sizeof(queued_target_t));
    *num_targets = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_by_id(
    const dragonfly_multi_target_t mt,
    uint32_t target_id,
    queued_target_t* target
) {
    if (!mt || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_by_id: required parameter is NULL (mt, target)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);

    int idx = find_target_by_id(mt, target_id);
    if (idx < 0) {
        nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_multi_target_get_by_id: validation failed");
        return -1;
    }

    *target = mt->targets[idx];

    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_state(
    const dragonfly_multi_target_t mt,
    multi_target_state_t* state
) {
    if (!mt || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_state: required parameter is NULL (mt, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);
    *state = mt->state;
    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_stats(
    const dragonfly_multi_target_t mt,
    multi_target_stats_t* stats
) {
    if (!mt || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_stats: required parameter is NULL (mt, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);
    *stats = mt->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}

int dragonfly_multi_target_get_history(
    const dragonfly_multi_target_t mt,
    switch_event_t* history,
    uint32_t max_events,
    uint32_t* num_events
) {
    if (!mt || !history || !num_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_multi_target_get_history: required parameter is NULL (mt, history, num_events)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)mt->mutex);

    uint32_t count = mt->history_count < max_events ? mt->history_count : max_events;

    /* Copy from ring buffer in reverse chronological order */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (mt->history_head + MULTI_TARGET_HISTORY_SIZE - 1 - i) % MULTI_TARGET_HISTORY_SIZE;
        history[i] = mt->history[idx];
    }

    *num_events = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)mt->mutex);

    return 0;
}
