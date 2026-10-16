/**
 * @file nimcp_dragonfly_sleep_bridge.c
 * @brief Sleep-Dragonfly Integration Bridge Implementation
 *
 * WHAT: Integrates dragonfly hunting with sleep/wake cycles
 * WHY:  Enables realistic circadian behavior and strategy learning
 * HOW:  Bidirectional sleep system communication
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_sleep_bridge)

#define LOG_MODULE "DRAGONFLY_SLEEP_BRIDGE"


//=============================================================================
// Constants
//=============================================================================

#define MAX_PENDING_EXPERIENCES 128

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_sleep_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    dragonfly_sleep_config_t config;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    sleep_orchestrator_t sleep;
    bool connected;

    /* Experience buffer */
    hunting_experience_t experiences[MAX_PENDING_EXPERIENCES];
    uint32_t num_experiences;
    uint32_t experience_head;

    /* Consolidated memory */
    consolidated_memory_t memory;

    /* Current state */
    dragonfly_sleep_state_t state;

    /* Statistics */
    dragonfly_sleep_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

dragonfly_sleep_config_t dragonfly_sleep_default_config(void) {
    dragonfly_sleep_config_t config = {
        /* Consolidation settings */
        .enable_memory_consolidation = true,
        .min_experiences_to_consolidate = 5,
        .consolidation_threshold = 0.5f,

        /* Circadian modulation */
        .enable_circadian_modulation = true,
        .dawn_activity_boost = 1.2f,
        .dusk_activity_boost = 1.3f,
        .night_activity_floor = 0.1f,

        /* Wake triggers */
        .prey_wake_threshold = 0.8f,
        .predator_wake_threshold = 0.5f,

        /* Rest triggers */
        .fatigue_rest_threshold = 0.8f,
        .hunt_failure_rest_boost = 0.1f
    };
    return config;
}

bool dragonfly_sleep_validate_config(const dragonfly_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_validate_config: config is NULL");
        return false;
    }

    if (config->consolidation_threshold < 0.0f ||
        config->consolidation_threshold > 1.0f) return false;

    if (config->dawn_activity_boost < 0.0f) {
        return false;
    }
    if (config->dusk_activity_boost < 0.0f) {
        return false;
    }
    if (config->night_activity_floor < 0.0f ||
        config->night_activity_floor > 1.0f) return false;

    if (config->prey_wake_threshold < 0.0f ||
        config->prey_wake_threshold > 1.0f) return false;
    if (config->predator_wake_threshold < 0.0f ||
        config->predator_wake_threshold > 1.0f) return false;

    if (config->fatigue_rest_threshold < 0.0f ||
        config->fatigue_rest_threshold > 1.0f) return false;

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_sleep_bridge_t dragonfly_sleep_bridge_create(
    const dragonfly_sleep_config_t* config
) {
    dragonfly_sleep_config_t cfg = config ? *config : dragonfly_sleep_default_config();

    if (!dragonfly_sleep_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_sleep_bridge_create: invalid configuration");
        return NULL;
    }

    dragonfly_sleep_bridge_t bridge = nimcp_calloc(1, sizeof(struct dragonfly_sleep_bridge_s));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_sleep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = cfg;
    bridge->state.is_hunting_allowed = true;
    bridge->state.activity_level = 1.0f;
    bridge->creation_time_us = get_time_us();

    /* Initialize memory with neutral priors */
    for (int i = 0; i < 5; i++) {
        bridge->memory.strategy_scores[i] = 0.5f;
    }

    if (bridge_base_init(&bridge->base, 0, "dragonfly_sleep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_sleep_bridge_create: failed to initialize base bridge");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_sleep_bridge_create: base bridge mutex is NULL");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_sleep_bridge_destroy(dragonfly_sleep_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_sleep");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int dragonfly_sleep_bridge_connect(
    dragonfly_sleep_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    sleep_orchestrator_t sleep
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_bridge_connect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = dragonfly;
    bridge->sleep = sleep;
    bridge->connected = (dragonfly != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_bridge_disconnect(dragonfly_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = NULL;
    bridge->sleep = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Experience Recording Functions
//=============================================================================

int dragonfly_sleep_record_experience(
    dragonfly_sleep_bridge_t bridge,
    const hunting_experience_t* experience
) {
    if (!bridge || !experience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_record_experience: required parameter is NULL (bridge, experience)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_experiences < MAX_PENDING_EXPERIENCES) {
        bridge->experiences[bridge->experience_head] = *experience;
        bridge->experience_head = (bridge->experience_head + 1) % MAX_PENDING_EXPERIENCES;
        bridge->num_experiences++;
        bridge->state.pending_experiences = bridge->num_experiences;
        bridge->stats.experiences_recorded++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_record_success(
    dragonfly_sleep_bridge_t bridge,
    uint32_t target_id,
    float miss_distance,
    intercept_strategy_t strategy
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_record_success: bridge is NULL");
        return -1;
    }

    hunting_experience_t exp = {0};
    exp.target_id = target_id;
    exp.strategy_used = strategy;
    exp.success = true;
    exp.miss_distance = miss_distance;
    exp.timestamp_us = get_time_us();

    return dragonfly_sleep_record_experience(bridge, &exp);
}

int dragonfly_sleep_record_failure(
    dragonfly_sleep_bridge_t bridge,
    uint32_t target_id,
    const char* reason,
    intercept_strategy_t strategy
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_record_failure: bridge is NULL");
        return -1;
    }

    hunting_experience_t exp = {0};
    exp.target_id = target_id;
    exp.strategy_used = strategy;
    exp.success = false;
    exp.failure_reason = reason;
    exp.timestamp_us = get_time_us();

    return dragonfly_sleep_record_experience(bridge, &exp);
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_sleep_bridge_update(
    dragonfly_sleep_bridge_t bridge,
    float dt_s
) {
    if (!bridge || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_sleep_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();

    /* Update circadian phase (simplified 24-hour cycle) */
    if (bridge->config.enable_circadian_modulation) {
        /* Get approximate time of day from system time */
        time_t t = time(NULL);
        struct tm* tm_info = localtime(&t);
        float hour = (float)tm_info->tm_hour + (float)tm_info->tm_min / 60.0f;
        bridge->state.circadian_phase = hour / 24.0f;

        /* Compute activity level based on time of day */
        float base_activity = 1.0f;

        /* Dawn boost (5-8 AM) */
        if (hour >= 5.0f && hour <= 8.0f) {
            base_activity *= bridge->config.dawn_activity_boost;
            bridge->state.is_peak_hunting_time = true;
        }
        /* Dusk boost (5-8 PM) */
        else if (hour >= 17.0f && hour <= 20.0f) {
            base_activity *= bridge->config.dusk_activity_boost;
            bridge->state.is_peak_hunting_time = true;
        }
        /* Night (9 PM - 5 AM) */
        else if (hour >= 21.0f || hour < 5.0f) {
            base_activity = bridge->config.night_activity_floor;
            bridge->state.is_peak_hunting_time = false;
        }
        else {
            bridge->state.is_peak_hunting_time = false;
        }

        /* Apply fatigue reduction */
        base_activity *= (1.0f - bridge->state.fatigue_level * 0.5f);

        bridge->state.activity_level = nimcp_clampf(base_activity, 0.0f, 1.5f);
        bridge->state.is_hunting_allowed = bridge->state.activity_level > 0.2f;
    }

    /* Natural fatigue decay during rest */
    if (!bridge->state.is_hunting_allowed) {
        bridge->state.fatigue_level *= (1.0f - dt_s * 0.1f);
        bridge->stats.total_rest_time_s += dt_s;
    }

    bridge->last_update_us = now;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_consolidate(dragonfly_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_consolidate: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_experiences < bridge->config.min_experiences_to_consolidate) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not enough experiences */
    }

    bridge->state.consolidation_in_progress = true;

    /* Process experiences and update memory */
    uint32_t start_idx = (bridge->experience_head + MAX_PENDING_EXPERIENCES -
                          bridge->num_experiences) % MAX_PENDING_EXPERIENCES;

    for (uint32_t i = 0; i < bridge->num_experiences; i++) {
        uint32_t idx = (start_idx + i) % MAX_PENDING_EXPERIENCES;
        hunting_experience_t* exp = &bridge->experiences[idx];

        /* Update strategy scores */
        if (exp->strategy_used < 5) {
            float old_score = bridge->memory.strategy_scores[exp->strategy_used];
            float new_val = exp->success ? 1.0f : 0.0f;
            uint32_t count = bridge->memory.strategy_counts[exp->strategy_used];

            bridge->memory.strategy_scores[exp->strategy_used] =
                (old_score * count + new_val) / (count + 1);
            bridge->memory.strategy_counts[exp->strategy_used]++;
        }

        bridge->memory.experiences_processed++;
    }

    /* Clear processed experiences */
    bridge->num_experiences = 0;
    bridge->state.pending_experiences = 0;

    /* Update consolidation metrics */
    bridge->memory.consolidation_quality = 0.8f;  /* Simplified */
    bridge->memory.last_consolidation_us = get_time_us();

    bridge->state.consolidation_in_progress = false;
    bridge->stats.consolidations_completed++;
    bridge->stats.avg_consolidation_quality =
        (bridge->stats.avg_consolidation_quality * (bridge->stats.consolidations_completed - 1) +
         bridge->memory.consolidation_quality) / bridge->stats.consolidations_completed;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_get_memory(
    const dragonfly_sleep_bridge_t bridge,
    consolidated_memory_t* memory
) {
    if (!bridge || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_get_memory: required parameter is NULL (bridge, memory)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *memory = bridge->memory;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

bool dragonfly_sleep_hunting_allowed(const dragonfly_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_hunting_allowed: bridge is NULL");
        return false;
    }
    return bridge->state.is_hunting_allowed;
}

float dragonfly_sleep_get_activity(const dragonfly_sleep_bridge_t bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.activity_level;
}

int dragonfly_sleep_recommend_strategy(
    const dragonfly_sleep_bridge_t bridge,
    uint32_t prey_type,
    float time_of_day,
    intercept_strategy_t* strategy,
    float* confidence
) {
    if (!bridge || !strategy || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_recommend_strategy: required parameter is NULL (bridge, strategy, confidence)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find best strategy from consolidated memory */
    float best_score = 0.0f;
    intercept_strategy_t best_strategy = INTERCEPT_PURSUIT;

    for (int i = 0; i < 5; i++) {
        if (bridge->memory.strategy_counts[i] > 0 &&
            bridge->memory.strategy_scores[i] > best_score) {
            best_score = bridge->memory.strategy_scores[i];
            best_strategy = (intercept_strategy_t)i;
        }
    }

    *strategy = best_strategy;
    *confidence = best_score;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_get_state(
    const dragonfly_sleep_bridge_t bridge,
    dragonfly_sleep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_sleep_get_stats(
    const dragonfly_sleep_bridge_t bridge,
    dragonfly_sleep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_sleep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}
