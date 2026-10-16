/**
 * @file nimcp_kg_gc_agent.c
 * @brief KG Garbage Collection Subagent Implementation
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of the dedicated GC subagent that runs as a background
 * service for automatic knowledge graph garbage collection.
 */

#include "core/brain/nimcp_kg_gc_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_gc_agent, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief GC agent internal structure
 */
struct kg_gc_agent {
    brain_kg_t* kg;                           /**< Knowledge graph to manage */
    kg_gc_context_t* gc_ctx;                  /**< Underlying GC context */
    kg_gc_agent_config_t config;              /**< Agent configuration */

    /* State */
    kg_gc_agent_state_t state;                /**< Current agent state */
    kg_gc_priority_t current_priority;        /**< Active priority */
    nimcp_mutex_t* mutex;                     /**< Thread safety mutex */

    /* Thread management */
    nimcp_thread_t agent_thread;              /**< Background thread handle */
    bool thread_active;                       /**< Thread is running */
    bool stop_requested;                      /**< Stop flag */
    bool pause_requested;                     /**< Pause flag */

    /* Timing */
    uint64_t started_at;                      /**< Start timestamp */
    uint64_t last_gc_at;                      /**< Last GC completion */
    uint64_t next_gc_at;                      /**< Next scheduled GC */

    /* Statistics */
    uint32_t total_gc_runs;
    uint32_t successful_gc_runs;
    uint32_t failed_gc_runs;
    uint64_t total_items_collected;
    uint64_t total_bytes_reclaimed;
    uint64_t total_gc_duration_ms;

    /* Current GC state */
    float current_progress;
    uint32_t current_items_processed;
    char last_error[NIMCP_ERROR_BUFFER_SIZE];

    /* Callbacks */
    kg_gc_agent_complete_fn complete_callback;
    void* complete_callback_data;
    kg_gc_agent_escalate_fn escalate_callback;
    void* escalate_callback_data;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Get current hour of day (0-23)
 */
static uint32_t get_current_hour(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    return (uint32_t)tm_info->tm_hour;
}

/**
 * @brief Check if current time is in off-peak window
 */
static bool is_off_peak_time(const kg_gc_agent_config_t* config) {
    uint32_t hour = get_current_hour();

    if (config->off_peak_start_hour <= config->off_peak_end_hour) {
        /* Normal range (e.g., 2:00 - 6:00) */
        return hour >= config->off_peak_start_hour &&
               hour < config->off_peak_end_hour;
    } else {
        /* Wrapping range (e.g., 22:00 - 6:00) */
        return hour >= config->off_peak_start_hour ||
               hour < config->off_peak_end_hour;
    }
}

/**
 * @brief Calculate next GC time based on scheduling mode
 */
static uint64_t calculate_next_gc_time(kg_gc_agent_t* agent) {
    uint64_t now = get_current_timestamp_ms();

    switch (agent->config.scheduling_mode) {
        case KG_GC_SCHEDULE_PERIODIC:
            return now + (uint64_t)agent->config.interval_minutes * 60 * 1000;

        case KG_GC_SCHEDULE_THRESHOLD:
        case KG_GC_SCHEDULE_ADAPTIVE:
            /* Check at regular intervals */
            return now + (uint64_t)agent->config.check_interval_seconds * 1000;

        case KG_GC_SCHEDULE_OFF_PEAK:
            /* Schedule for next off-peak window */
            if (is_off_peak_time(&agent->config)) {
                return now + 60000; /* Check again in 1 minute */
            }
            /* Calculate time until next off-peak window */
            return now + 3600000; /* Fallback: 1 hour */

        case KG_GC_SCHEDULE_ON_DEMAND:
        default:
            return 0; /* No automatic scheduling */
    }
}

/**
 * @brief Check if GC should run now based on configuration
 */
static bool should_run_gc(kg_gc_agent_t* agent) {
    switch (agent->config.scheduling_mode) {
        case KG_GC_SCHEDULE_PERIODIC: {
            uint64_t now = get_current_timestamp_ms();
            return now >= agent->next_gc_at;
        }

        case KG_GC_SCHEDULE_THRESHOLD: {
            float fragmentation = kg_gc_get_fragmentation(agent->gc_ctx);
            return fragmentation >= agent->config.waste_threshold;
        }

        case KG_GC_SCHEDULE_OFF_PEAK:
            return is_off_peak_time(&agent->config);

        case KG_GC_SCHEDULE_ADAPTIVE: {
            /* Combine threshold and off-peak */
            float fragmentation = kg_gc_get_fragmentation(agent->gc_ctx);
            if (fragmentation >= agent->config.waste_threshold) {
                return true;
            }
            if (is_off_peak_time(&agent->config) && fragmentation > 0.05f) {
                return true;
            }
            return false;
        }

        case KG_GC_SCHEDULE_ON_DEMAND:
        default:
            return false;
    }
}

/**
 * @brief Execute a GC cycle
 */
static int execute_gc_cycle(kg_gc_agent_t* agent) {
    nimcp_mutex_lock(agent->mutex);
    agent->state = KG_GC_AGENT_COLLECTING;
    agent->current_progress = 0.0f;
    agent->current_items_processed = 0;
    nimcp_mutex_unlock(agent->mutex);

    uint64_t start_time = get_current_timestamp_ms();

    /* Run GC with configured targets */
    int result = kg_gc_run(agent->gc_ctx, agent->config.gc_targets);

    uint64_t duration = get_current_timestamp_ms() - start_time;

    nimcp_mutex_lock(agent->mutex);

    agent->total_gc_runs++;
    agent->total_gc_duration_ms += duration;

    if (result >= 0) {
        agent->successful_gc_runs++;
        agent->total_items_collected += (uint64_t)result;
        agent->last_error[0] = '\0';
    } else {
        agent->failed_gc_runs++;
        snprintf(agent->last_error, sizeof(agent->last_error),
                 "GC cycle failed with code %d", result);
    }

    agent->last_gc_at = get_current_timestamp_ms();
    agent->next_gc_at = calculate_next_gc_time(agent);
    agent->current_progress = 1.0f;

    /* Run compaction if enabled */
    if (agent->config.enable_compaction && result >= 0) {
        int compacted = kg_gc_compact(agent->gc_ctx);
        if (compacted > 0) {
            agent->total_bytes_reclaimed += (uint64_t)compacted;
        }
    }

    agent->state = KG_GC_AGENT_RUNNING;

    nimcp_mutex_unlock(agent->mutex);

    /* Call completion callback */
    if (agent->complete_callback) {
        kg_gc_stats_t stats;
        kg_gc_analyze(agent->gc_ctx, &stats);
        agent->complete_callback(agent, &stats, agent->complete_callback_data);
    }

    return result;
}

/**
 * @brief Agent thread main function
 */
static void* agent_thread_func(void* arg) {
    kg_gc_agent_t* agent = (kg_gc_agent_t*)arg;

    nimcp_mutex_lock(agent->mutex);
    agent->state = KG_GC_AGENT_RUNNING;
    agent->started_at = get_current_timestamp_ms();
    agent->next_gc_at = calculate_next_gc_time(agent);
    nimcp_mutex_unlock(agent->mutex);

    /* Run initial GC if configured */
    if (agent->config.run_on_startup) {
        execute_gc_cycle(agent);
    }

    /* Main agent loop */
    while (!agent->stop_requested) {
        /* Check for pause */
        if (agent->pause_requested) {
            nimcp_mutex_lock(agent->mutex);
            agent->state = KG_GC_AGENT_PAUSED;
            nimcp_mutex_unlock(agent->mutex);

            /* Sleep while paused */
            struct timespec sleep_time = { .tv_sec = 1, .tv_nsec = 0 };
            nanosleep(&sleep_time, NULL);
            continue;
        }

        /* Check if GC should run */
        if (should_run_gc(agent)) {
            execute_gc_cycle(agent);
        }

        /* Sleep for check interval */
        struct timespec sleep_time = {
            .tv_sec = agent->config.check_interval_seconds,
            .tv_nsec = 0
        };
        nanosleep(&sleep_time, NULL);
    }

    nimcp_mutex_lock(agent->mutex);
    agent->state = KG_GC_AGENT_STOPPED;
    nimcp_mutex_unlock(agent->mutex);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent_thread_func: operation failed");
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_gc_agent_default_config(kg_gc_agent_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    strncpy(config->agent_name, "kg-gc-agent", KG_GC_AGENT_MAX_NAME_LEN - 1);

    config->scheduling_mode = KG_GC_SCHEDULE_PERIODIC;
    config->interval_minutes = KG_GC_AGENT_DEFAULT_GC_INTERVAL_MIN;
    config->check_interval_seconds = KG_GC_AGENT_DEFAULT_CHECK_INTERVAL_SEC;
    config->waste_threshold = KG_GC_AGENT_DEFAULT_WASTE_THRESHOLD;

    config->priority = KG_GC_PRIORITY_NORMAL;
    config->cpu_limit_percent = 25.0f;
    config->max_duration_seconds = 300; /* 5 minutes max */

    config->gc_targets = KG_GC_ALL;

    config->off_peak_start_hour = 2;  /* 2 AM */
    config->off_peak_end_hour = 6;    /* 6 AM */
    config->activity_threshold_ops_per_sec = 10;

    config->enable_compaction = true;
    config->enable_metrics = true;
    config->enable_notifications = false;
    config->run_on_startup = false;

    return 0;
}

kg_gc_agent_t* kg_gc_agent_create(brain_kg_t* kg, const kg_gc_agent_config_t* config) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }

    kg_gc_agent_t* agent = nimcp_calloc(1, sizeof(kg_gc_agent_t));
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "agent is NULL");

        return NULL;
    }

    agent->kg = kg;

    /* Apply configuration */
    if (config) {
        memcpy(&agent->config, config, sizeof(kg_gc_agent_config_t));
    } else {
        kg_gc_agent_default_config(&agent->config);
    }

    /* Create underlying GC context */
    kg_gc_config_t gc_config;
    kg_gc_default_config(&gc_config);
    gc_config.gc_targets = agent->config.gc_targets;
    gc_config.enable_compaction = agent->config.enable_compaction;
    gc_config.max_gc_duration_ms = agent->config.max_duration_seconds * 1000;

    agent->gc_ctx = kg_gc_create(kg, &gc_config);
    if (!agent->gc_ctx) {
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_gc_agent_create: agent->gc_ctx is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    agent->mutex = nimcp_mutex_create(&attr);
    if (!agent->mutex) {
        kg_gc_destroy(agent->gc_ctx);
        nimcp_free(agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_gc_agent_create: agent->mutex is NULL");
        return NULL;
    }

    agent->state = KG_GC_AGENT_STOPPED;
    agent->current_priority = agent->config.priority;

    return agent;
}

void kg_gc_agent_destroy(kg_gc_agent_t* agent) {
    if (!agent) {
        return;
    }

    /* Stop if running */
    if (agent->state != KG_GC_AGENT_STOPPED) {
        kg_gc_agent_stop(agent);
    }

    if (agent->gc_ctx) {
        kg_gc_destroy(agent->gc_ctx);
    }

    if (agent->mutex) {
        nimcp_mutex_free(agent->mutex);
    }

    nimcp_free(agent);
}

int kg_gc_agent_start(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);

    if (agent->state != KG_GC_AGENT_STOPPED) {
        nimcp_mutex_unlock(agent->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_agent_start: validation failed");
        return -1; /* Already running */
    }

    agent->state = KG_GC_AGENT_STARTING;
    agent->stop_requested = false;
    agent->pause_requested = false;

    nimcp_mutex_unlock(agent->mutex);

    /* Create and start thread */
    nimcp_result_t result = nimcp_thread_create(&agent->agent_thread, agent_thread_func, agent, NULL);

    if (result != NIMCP_SUCCESS) {
        nimcp_mutex_lock(agent->mutex);
        agent->state = KG_GC_AGENT_STOPPED;
        nimcp_mutex_unlock(agent->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_agent_start: validation failed");
        return -1;
    }

    agent->thread_active = true;
    return 0;
}

int kg_gc_agent_stop(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);

    if (agent->state == KG_GC_AGENT_STOPPED) {
        nimcp_mutex_unlock(agent->mutex);
        return 0; /* Already stopped */
    }

    agent->state = KG_GC_AGENT_STOPPING;
    agent->stop_requested = true;

    nimcp_mutex_unlock(agent->mutex);

    /* Wait for thread to finish */
    if (agent->thread_active) {
        nimcp_thread_join(agent->agent_thread, NULL);
        agent->thread_active = false;
    }

    return 0;
}

int kg_gc_agent_pause(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->pause_requested = true;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

int kg_gc_agent_resume(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->pause_requested = false;
    agent->state = KG_GC_AGENT_RUNNING;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int kg_gc_agent_set_config(kg_gc_agent_t* agent, const kg_gc_agent_config_t* config) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_agent_set_config: required parameter is NULL (agent, config)");
        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    memcpy(&agent->config, config, sizeof(kg_gc_agent_config_t));
    agent->next_gc_at = calculate_next_gc_time(agent);
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

int kg_gc_agent_get_config(const kg_gc_agent_t* agent, kg_gc_agent_config_t* config) {
    if (!agent || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_agent_get_config: required parameter is NULL (agent, config)");
        return -1;
    }

    nimcp_mutex_lock(((kg_gc_agent_t*)agent)->mutex);
    memcpy(config, &agent->config, sizeof(kg_gc_agent_config_t));
    nimcp_mutex_unlock(((kg_gc_agent_t*)agent)->mutex);

    return 0;
}

int kg_gc_agent_set_priority(kg_gc_agent_t* agent, kg_gc_priority_t priority) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->current_priority = priority;
    agent->config.priority = priority;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

int kg_gc_agent_set_targets(kg_gc_agent_t* agent, uint32_t targets) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->config.gc_targets = targets;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

/* ============================================================================
 * Status and Monitoring API
 * ============================================================================ */

int kg_gc_agent_get_status(const kg_gc_agent_t* agent, kg_gc_agent_status_t* status) {
    if (!agent || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_agent_get_status: required parameter is NULL (agent, status)");
        return -1;
    }

    memset(status, 0, sizeof(*status));

    nimcp_mutex_lock(((kg_gc_agent_t*)agent)->mutex);

    status->state = agent->state;
    status->current_priority = agent->current_priority;

    status->agent_started_at = agent->started_at;
    status->last_gc_run_at = agent->last_gc_at;
    status->next_gc_scheduled_at = agent->next_gc_at;

    if (agent->started_at > 0) {
        status->uptime_seconds = (get_current_timestamp_ms() - agent->started_at) / 1000;
    }

    status->total_gc_runs = agent->total_gc_runs;
    status->successful_gc_runs = agent->successful_gc_runs;
    status->failed_gc_runs = agent->failed_gc_runs;
    status->total_items_collected = agent->total_items_collected;
    status->total_bytes_reclaimed = agent->total_bytes_reclaimed;
    status->total_gc_duration_ms = agent->total_gc_duration_ms;

    status->current_gc_progress = agent->current_progress;
    status->current_gc_items_processed = agent->current_items_processed;

    /* Calculate derived stats */
    float fragmentation = kg_gc_get_fragmentation(agent->gc_ctx);
    status->current_waste_level = fragmentation >= 0 ? fragmentation : 0;

    if (agent->total_gc_runs > 0) {
        status->avg_gc_duration_ms = (float)agent->total_gc_duration_ms /
                                     agent->total_gc_runs;
    }

    strncpy(status->last_error, agent->last_error, sizeof(status->last_error) - 1);

    nimcp_mutex_unlock(((kg_gc_agent_t*)agent)->mutex);

    return 0;
}

kg_gc_agent_state_t kg_gc_agent_get_state(const kg_gc_agent_t* agent) {
    if (!agent) {
        return KG_GC_AGENT_ERROR;
    }

    nimcp_mutex_lock(((kg_gc_agent_t*)agent)->mutex);
    kg_gc_agent_state_t state = agent->state;
    nimcp_mutex_unlock(((kg_gc_agent_t*)agent)->mutex);

    return state;
}

bool kg_gc_agent_is_running(const kg_gc_agent_t* agent) {
    if (!agent) {
        return false;
    }

    kg_gc_agent_state_t state = kg_gc_agent_get_state(agent);
    return state == KG_GC_AGENT_RUNNING ||
           state == KG_GC_AGENT_COLLECTING ||
           state == KG_GC_AGENT_PAUSED;
}

bool kg_gc_agent_is_collecting(const kg_gc_agent_t* agent) {
    return kg_gc_agent_get_state(agent) == KG_GC_AGENT_COLLECTING;
}

/* ============================================================================
 * Manual Control API
 * ============================================================================ */

int kg_gc_agent_trigger_now(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (agent->state != KG_GC_AGENT_RUNNING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_agent_trigger_now: validation failed");
        return -1; /* Not in a state to trigger */
    }

    if (agent->state == KG_GC_AGENT_COLLECTING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_agent_trigger_now: validation failed");
        return -1; /* Already collecting */
    }

    return execute_gc_cycle(agent);
}

int kg_gc_agent_request_escalation(
    kg_gc_agent_t* agent,
    kg_gc_priority_t priority,
    const char* reason
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    /* Check with callback if registered */
    if (agent->escalate_callback) {
        bool approved = agent->escalate_callback(
            agent,
            agent->current_priority,
            priority,
            reason,
            agent->escalate_callback_data
        );

        if (!approved) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "approved is NULL");


            return -1;
        }
    }

    return kg_gc_agent_set_priority(agent, priority);
}

int kg_gc_agent_cancel_current(kg_gc_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    if (agent->state != KG_GC_AGENT_COLLECTING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_agent_cancel_current: validation failed");
        return -1;
    }

    return kg_gc_cancel(agent->gc_ctx);
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int kg_gc_agent_set_complete_callback(
    kg_gc_agent_t* agent,
    kg_gc_agent_complete_fn callback,
    void* user_data
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->complete_callback = callback;
    agent->complete_callback_data = user_data;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

int kg_gc_agent_set_escalation_callback(
    kg_gc_agent_t* agent,
    kg_gc_agent_escalate_fn callback,
    void* user_data
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    nimcp_mutex_lock(agent->mutex);
    agent->escalate_callback = callback;
    agent->escalate_callback_data = user_data;
    nimcp_mutex_unlock(agent->mutex);

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* schedule_mode_strings[] = {
    "PERIODIC",
    "THRESHOLD",
    "OFF_PEAK",
    "ON_DEMAND",
    "ADAPTIVE"
};

const char* kg_gc_schedule_mode_to_string(kg_gc_schedule_mode_t mode) {
    if (mode >= 0 && mode <= KG_GC_SCHEDULE_ADAPTIVE) {
        return schedule_mode_strings[mode];
    }
    return "UNKNOWN";
}

static const char* priority_strings[] = {
    "LOW",
    "NORMAL",
    "HIGH",
    "CRITICAL"
};

const char* kg_gc_priority_to_string(kg_gc_priority_t priority) {
    if (priority >= 0 && priority <= KG_GC_PRIORITY_CRITICAL) {
        return priority_strings[priority];
    }
    return "UNKNOWN";
}

static const char* agent_state_strings[] = {
    "STOPPED",
    "STARTING",
    "RUNNING",
    "PAUSED",
    "COLLECTING",
    "STOPPING",
    "ERROR"
};

const char* kg_gc_agent_state_to_string(kg_gc_agent_state_t state) {
    if (state >= 0 && state <= KG_GC_AGENT_ERROR) {
        return agent_state_strings[state];
    }
    return "UNKNOWN";
}

void kg_gc_agent_reset_stats(kg_gc_agent_t* agent) {
    if (!agent) {
        return;
    }

    nimcp_mutex_lock(agent->mutex);

    agent->total_gc_runs = 0;
    agent->successful_gc_runs = 0;
    agent->failed_gc_runs = 0;
    agent->total_items_collected = 0;
    agent->total_bytes_reclaimed = 0;
    agent->total_gc_duration_ms = 0;

    nimcp_mutex_unlock(agent->mutex);
}
