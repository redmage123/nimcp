/**
 * @file nimcp_time_dilation.c
 * @brief Subjective Time Manipulation - Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Subjective time manipulation through processing speed modulation
 * WHY:  Enable enhanced reaction time and temporal resolution
 * HOW:  Variable processing rate, temporal buffering, "bullet time" simulation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Time perception is modulated by arousal and attention. During high-stress events,
 * increased norepinephrine enhances sensory processing and memory encoding,
 * creating the subjective experience of "time slowing down." This implementation
 * models these effects through processing rate modulation.
 *
 * REFERENCES:
 * - Stetson et al. (2007) "Does time really slow down during a frightening event?"
 * - Meck (2005) "Neuropsychology of timing and time perception"
 * - Eagleman (2008) "Human time perception and its illusions"
 */

#include "superhuman/nimcp_time_dilation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(time_dilation)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Event queue entry
 */
typedef struct {
    time_event_t event;         /**< Event data */
    float* data_copy;           /**< Owned copy of event data */
    bool active;                /**< Entry in use */
} event_entry_t;

/**
 * @brief Reaction time tracking
 */
typedef struct {
    float times_ms[64];         /**< Recent reaction times */
    uint32_t count;             /**< Number of recorded times */
    uint32_t index;             /**< Circular buffer index */
    float sum_ms;               /**< Sum for averaging */
    float best_ms;              /**< Best reaction time */
} reaction_tracker_t;

/**
 * @brief Internal time dilation system
 */
struct time_dilation_system {
    /* Configuration */
    time_dilation_config_t config;

    /* State */
    time_dilation_state_t state;
    time_dilation_stats_t stats;

    /* Event queue */
    event_entry_t* events;          /**< Event queue array */
    uint32_t event_capacity;        /**< Queue capacity */
    uint32_t event_count;           /**< Current event count */
    uint32_t next_event_id;         /**< Next event ID */

    /* Temporal buffers */
    time_buffer_t input_buffer;     /**< Input sample buffer */
    time_buffer_t output_buffer;    /**< Output sample buffer */

    /* Dilation control */
    float current_factor;           /**< Current dilation factor */
    float target_factor;            /**< Target dilation factor */
    uint64_t dilation_start_real;   /**< When dilation started (real time) */
    uint64_t accumulated_subjective; /**< Accumulated subjective time */

    /* Reaction tracking */
    reaction_tracker_t reactions;   /**< Reaction time tracker */

    /* Timing */
    uint64_t base_real_time_us;     /**< Base real time reference */
    uint64_t last_update_real_us;   /**< Last update real time */
    uint64_t last_update_subj_us;   /**< Last update subjective time */

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in microseconds
 * WHY:  High-resolution timing for dilation
 * HOW:  Use CLOCK_MONOTONIC for stable timing
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * WHAT: Get current time in milliseconds
 * WHY:  For logging and statistics
 * HOW:  Convert from microseconds
 */
static float get_time_ms(void) {
    return (float)get_time_us() / 1000.0f;
}

/**
 * WHAT: Get resolution in microseconds for level
 * WHY:  Convert resolution enum to actual value
 * HOW:  Map enum to predefined values
 */
static float resolution_to_us(time_resolution_t res) {
    switch (res) {
        case TIME_RESOLUTION_COARSE: return 100000.0f;  /* 100ms */
        case TIME_RESOLUTION_NORMAL: return 30000.0f;   /* 30ms */
        case TIME_RESOLUTION_FINE:   return 10000.0f;   /* 10ms */
        case TIME_RESOLUTION_ULTRA:  return 1000.0f;    /* 1ms */
        default:                     return 30000.0f;
    }
}

/**
 * WHAT: Allocate temporal buffer
 * WHY:  Store samples for temporal processing
 * HOW:  Circular buffer with timestamps
 */
static int alloc_buffer(time_buffer_t* buf, uint32_t capacity) {
    buf->samples = nimcp_calloc(capacity, sizeof(float));
    buf->timestamps_us = nimcp_calloc(capacity, sizeof(uint64_t));

    if (!buf->samples || !buf->timestamps_us) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(float),
                           "alloc_buffer: Failed to allocate temporal buffer");
        if (buf->samples) nimcp_free(buf->samples);
        if (buf->timestamps_us) nimcp_free(buf->timestamps_us);
        buf->samples = NULL;
        buf->timestamps_us = NULL;
        return TIME_DILATION_ERROR_NO_MEMORY;
    }

    buf->capacity = capacity;
    buf->count = 0;
    buf->head = 0;
    buf->tail = 0;
    buf->sample_rate = 1000.0f;  /* Default 1kHz */

    return TIME_DILATION_SUCCESS;
}

/**
 * WHAT: Free temporal buffer
 * WHY:  Clean resource release
 * HOW:  Free arrays
 */
static void free_buffer(time_buffer_t* buf) {
    if (buf->samples) nimcp_free(buf->samples);
    if (buf->timestamps_us) nimcp_free(buf->timestamps_us);
    buf->samples = NULL;
    buf->timestamps_us = NULL;
    buf->capacity = 0;
    buf->count = 0;
}

/**
 * WHAT: Update dilation factor with ramping
 * WHY:  Smooth transitions between time rates
 * HOW:  Linear interpolation toward target
 */
static void update_dilation_factor(time_dilation_system_t* sys, float delta_s) {
    if (fabsf(sys->current_factor - sys->target_factor) < 0.01f) {
        sys->current_factor = sys->target_factor;
        return;
    }

    float ramp_amount = sys->config.dilation_ramp_rate * delta_s;
    if (sys->current_factor < sys->target_factor) {
        sys->current_factor += ramp_amount;
        if (sys->current_factor > sys->target_factor) {
            sys->current_factor = sys->target_factor;
        }
    } else {
        sys->current_factor -= ramp_amount;
        if (sys->current_factor < sys->target_factor) {
            sys->current_factor = sys->target_factor;
        }
    }

    sys->state.current_factor = sys->current_factor;
}

/**
 * WHAT: Find empty event slot
 * WHY:  Allocate space for new event
 * HOW:  Search for inactive entry
 */
static event_entry_t* find_empty_slot(time_dilation_system_t* sys) {
    for (uint32_t i = 0; i < sys->event_capacity; i++) {
        if (!sys->events[i].active) {
            return &sys->events[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Find event by ID
 * WHY:  Retrieve specific event
 * HOW:  Search active entries
 */
static event_entry_t* find_event_by_id(time_dilation_system_t* sys, uint32_t id) {
    for (uint32_t i = 0; i < sys->event_capacity; i++) {
        if (sys->events[i].active && sys->events[i].event.event_id == id) {
            return &sys->events[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Get highest priority pending event
 * WHY:  Process events by importance
 * HOW:  Search for highest priority unprocessed event
 */
static event_entry_t* get_next_event(time_dilation_system_t* sys) {
    event_entry_t* best = NULL;
    time_event_priority_t best_priority = TIME_PRIORITY_LOW;

    for (uint32_t i = 0; i < sys->event_capacity; i++) {
        if (sys->events[i].active && !sys->events[i].event.processed) {
            if (!best || sys->events[i].event.priority > best_priority) {
                best = &sys->events[i];
                best_priority = sys->events[i].event.priority;
            }
        }
    }

    return best;
}

/**
 * WHAT: Record reaction time
 * WHY:  Track performance statistics
 * HOW:  Update circular buffer and statistics
 */
static void record_reaction_time(time_dilation_system_t* sys, float time_ms) {
    reaction_tracker_t* r = &sys->reactions;

    /* Update circular buffer */
    if (r->count < 64) {
        r->times_ms[r->count] = time_ms;
        r->count++;
    } else {
        r->sum_ms -= r->times_ms[r->index];
        r->times_ms[r->index] = time_ms;
    }

    r->sum_ms += time_ms;
    r->index = (r->index + 1) % 64;

    /* Track best time */
    if (time_ms < r->best_ms || r->best_ms == 0.0f) {
        r->best_ms = time_ms;
        sys->stats.best_reaction_time_ms = time_ms;
    }

    /* Update statistics */
    sys->stats.total_reactions++;
    if (r->count > 0) {
        sys->stats.avg_reaction_time_ms = r->sum_ms / r->count;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int time_dilation_default_config(time_dilation_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_default_config: config is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    /* Dilation settings */
    config->mode = TIME_MODE_ADAPTIVE;
    config->base_dilation_factor = TIME_DILATION_DEFAULT;
    config->max_dilation_factor = TIME_DILATION_MAX_FACTOR;
    config->min_dilation_factor = TIME_DILATION_MIN_FACTOR;
    config->dilation_ramp_rate = 2.0f;  /* 2x per second ramp */

    /* Resolution settings */
    config->resolution = TIME_RESOLUTION_FINE;
    config->resolution_us = 10000.0f;   /* 10ms */

    /* Trigger settings */
    config->enable_auto_triggers = true;
    config->threat_threshold = 0.7f;
    config->novelty_threshold = 0.8f;
    config->complexity_threshold = 0.9f;

    /* Buffer settings */
    config->input_buffer_size = TIME_DILATION_BUFFER_SIZE;
    config->output_buffer_size = TIME_DILATION_BUFFER_SIZE;
    config->enable_interpolation = true;

    /* Resource limits */
    config->max_processing_load = 0.8f;
    config->max_dilation_duration_ms = 30000;  /* 30 seconds max */
    config->energy_budget = 100.0f;

    /* Reaction optimization */
    config->enable_prediction = true;
    config->enable_precomputation = true;
    config->prediction_horizon_ms = 100.0f;

    return TIME_DILATION_SUCCESS;
}

time_dilation_system_t* time_dilation_create(const time_dilation_config_t* config) {
    time_dilation_system_t* sys = nimcp_calloc(1, sizeof(time_dilation_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate time dilation system");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(time_dilation_system_t),
                           "time_dilation_create: Failed to allocate system");
        return NULL;
    }

    /* Apply configuration */
    time_dilation_config_t default_cfg;
    if (!config) {
        time_dilation_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Allocate event queue */
    sys->event_capacity = TIME_DILATION_MAX_EVENTS;
    sys->events = nimcp_calloc(sys->event_capacity, sizeof(event_entry_t));
    if (!sys->events) {
        NIMCP_LOGGING_ERROR("Failed to allocate event queue");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->event_capacity * sizeof(event_entry_t),
                           "time_dilation_create: Failed to allocate event queue");
        time_dilation_destroy(sys);
        return NULL;
    }
    sys->next_event_id = 1;

    /* Allocate temporal buffers */
    if (alloc_buffer(&sys->input_buffer, config->input_buffer_size) != 0) {
        NIMCP_LOGGING_ERROR("Failed to allocate input buffer");
        /* Exception already thrown in alloc_buffer */
        time_dilation_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "time_dilation_create: validation failed");
        return NULL;
    }

    if (alloc_buffer(&sys->output_buffer, config->output_buffer_size) != 0) {
        NIMCP_LOGGING_ERROR("Failed to allocate output buffer");
        /* Exception already thrown in alloc_buffer */
        time_dilation_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "time_dilation_create: validation failed");
        return NULL;
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                              "time_dilation_create: Failed to create mutex%s", "");
        time_dilation_destroy(sys);
        return NULL;
    }

    /* Initialize timing */
    uint64_t now = get_time_us();
    sys->base_real_time_us = now;
    sys->last_update_real_us = now;
    sys->last_update_subj_us = now;
    sys->accumulated_subjective = 0;

    /* Initialize dilation */
    sys->current_factor = TIME_DILATION_DEFAULT;
    sys->target_factor = TIME_DILATION_DEFAULT;

    /* Initialize state */
    sys->state.current_factor = TIME_DILATION_DEFAULT;
    sys->state.target_factor = TIME_DILATION_DEFAULT;
    sys->state.active_mode = TIME_MODE_NORMAL;
    sys->state.real_time_us = now;
    sys->state.subjective_time_us = now;
    sys->state.effective_resolution_us = resolution_to_us(config->resolution);
    sys->state.energy_remaining = config->energy_budget;
    sys->state.is_initialized = true;

    /* Initialize reaction tracker */
    sys->reactions.best_ms = 0.0f;
    sys->reactions.count = 0;
    sys->reactions.index = 0;
    sys->reactions.sum_ms = 0.0f;

    NIMCP_LOGGING_INFO("Time dilation system created: mode=%d, resolution=%.0fus",
                       config->mode, config->resolution_us);

    return sys;
}

void time_dilation_destroy(time_dilation_system_t* system) {
    if (!system) return;

    /* Free events with data copies */
    if (system->events) {
        for (uint32_t i = 0; i < system->event_capacity; i++) {
            if (system->events[i].data_copy) {
                nimcp_free(system->events[i].data_copy);
            }
        }
        nimcp_free(system->events);
    }

    /* Free buffers */
    free_buffer(&system->input_buffer);
    free_buffer(&system->output_buffer);

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Time dilation system destroyed");
}

int time_dilation_reset(time_dilation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_reset: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Clear events */
    for (uint32_t i = 0; i < system->event_capacity; i++) {
        if (system->events[i].data_copy) {
            nimcp_free(system->events[i].data_copy);
            system->events[i].data_copy = NULL;
        }
        system->events[i].active = false;
    }
    system->event_count = 0;

    /* Reset timing */
    uint64_t now = get_time_us();
    system->base_real_time_us = now;
    system->last_update_real_us = now;
    system->last_update_subj_us = now;
    system->accumulated_subjective = 0;

    /* Reset dilation */
    system->current_factor = TIME_DILATION_DEFAULT;
    system->target_factor = TIME_DILATION_DEFAULT;

    /* Reset state */
    system->state.current_factor = TIME_DILATION_DEFAULT;
    system->state.target_factor = TIME_DILATION_DEFAULT;
    system->state.active_mode = TIME_MODE_NORMAL;
    system->state.real_time_us = now;
    system->state.subjective_time_us = now;
    system->state.dilation_start_us = 0;
    system->state.dilation_duration_us = 0;
    system->state.pending_events = 0;
    system->state.processed_events = 0;
    system->state.event_throughput = 0.0f;
    system->state.processing_load = 0.0f;
    system->state.is_dilating = false;
    system->state.energy_remaining = system->config.energy_budget;

    /* Clear buffers */
    system->input_buffer.count = 0;
    system->input_buffer.head = 0;
    system->input_buffer.tail = 0;
    system->output_buffer.count = 0;
    system->output_buffer.head = 0;
    system->output_buffer.tail = 0;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Time dilation system reset");
    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int time_dilation_set_config(time_dilation_system_t* system,
                             const time_dilation_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_set_config: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->config = *config;
    system->state.effective_resolution_us = resolution_to_us(config->resolution);
    nimcp_platform_mutex_unlock(system->mutex);

    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_config(const time_dilation_system_t* system,
                             time_dilation_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_config: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    *config = system->config;
    return TIME_DILATION_SUCCESS;
}

int time_dilation_set_mode(time_dilation_system_t* system,
                           time_dilation_mode_t mode) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_set_mode: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->config.mode = mode;
    system->state.active_mode = mode;

    /* Set target factor based on mode */
    switch (mode) {
        case TIME_MODE_NORMAL:
            system->target_factor = 1.0f;
            break;
        case TIME_MODE_ACCELERATED:
            system->target_factor = 3.0f;
            break;
        case TIME_MODE_BULLET_TIME:
            system->target_factor = system->config.max_dilation_factor;
            break;
        case TIME_MODE_ADAPTIVE:
        case TIME_MODE_CUSTOM:
            /* Keep current target */
            break;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_set_factor(time_dilation_system_t* system, float factor) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_set_factor: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    factor = nimcp_clampf(factor, system->config.min_dilation_factor,
                     system->config.max_dilation_factor);

    nimcp_platform_mutex_lock(system->mutex);

    system->target_factor = factor;
    system->state.target_factor = factor;
    system->config.mode = TIME_MODE_CUSTOM;
    system->state.active_mode = TIME_MODE_CUSTOM;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_set_resolution(time_dilation_system_t* system,
                                 time_resolution_t resolution) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_set_resolution: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->config.resolution = resolution;
    system->config.resolution_us = resolution_to_us(resolution);
    system->state.effective_resolution_us = system->config.resolution_us;
    system->stats.resolution_adjustments++;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Time Dilation Control Implementation
 * ============================================================================ */

int time_dilation_activate(time_dilation_system_t* system,
                           time_dilation_trigger_t trigger,
                           float factor) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_activate: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Use default factor if not specified */
    if (factor <= 0.0f) {
        switch (trigger) {
            case TIME_TRIGGER_THREAT:
                factor = system->config.max_dilation_factor;
                break;
            case TIME_TRIGGER_NOVELTY:
                factor = 3.0f;
                break;
            case TIME_TRIGGER_COMPLEXITY:
                factor = 2.0f;
                break;
            case TIME_TRIGGER_MOTION:
                factor = 4.0f;
                break;
            case TIME_TRIGGER_DECISION:
                factor = 5.0f;
                break;
            default:
                factor = system->config.base_dilation_factor;
                break;
        }
    }

    factor = nimcp_clampf(factor, system->config.min_dilation_factor,
                     system->config.max_dilation_factor);

    /* Check energy budget */
    if (system->state.energy_remaining <= 0.0f) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_LOGGING_WARN("Time dilation blocked: energy exhausted");
        return TIME_DILATION_ERROR_RESOURCE_EXHAUSTED;
    }

    /* Set target */
    system->target_factor = factor;
    system->state.target_factor = factor;
    system->state.last_trigger = trigger;

    /* Start dilation if not already active */
    if (!system->state.is_dilating) {
        system->state.is_dilating = true;
        system->state.dilation_start_us = get_time_us();
        system->dilation_start_real = system->state.dilation_start_us;
        system->stats.total_dilation_events++;

        /* Update trigger statistics */
        switch (trigger) {
            case TIME_TRIGGER_THREAT:
                system->stats.threat_triggers++;
                break;
            case TIME_TRIGGER_NOVELTY:
                system->stats.novelty_triggers++;
                break;
            case TIME_TRIGGER_MANUAL:
                system->stats.manual_triggers++;
                break;
            default:
                break;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Time dilation activated: trigger=%d, factor=%.1f", trigger, factor);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_deactivate(time_dilation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_deactivate: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->target_factor = TIME_DILATION_DEFAULT;
    system->state.target_factor = TIME_DILATION_DEFAULT;
    system->state.is_dilating = false;

    /* Record dilation duration */
    if (system->state.dilation_start_us > 0) {
        uint64_t duration = get_time_us() - system->state.dilation_start_us;
        system->state.dilation_duration_us = duration;
        system->stats.total_dilation_time_us += duration;
        system->state.dilation_start_us = 0;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Time dilation deactivated");
    return TIME_DILATION_SUCCESS;
}

int time_dilation_check_triggers(time_dilation_system_t* system,
                                 float threat_level,
                                 float novelty_level,
                                 float complexity_level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_check_triggers: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }
    if (!system->config.enable_auto_triggers) return TIME_DILATION_SUCCESS;

    /* Check threat trigger (highest priority) */
    if (threat_level >= system->config.threat_threshold) {
        return time_dilation_activate(system, TIME_TRIGGER_THREAT, 0.0f);
    }

    /* Check novelty trigger */
    if (novelty_level >= system->config.novelty_threshold) {
        return time_dilation_activate(system, TIME_TRIGGER_NOVELTY, 0.0f);
    }

    /* Check complexity trigger */
    if (complexity_level >= system->config.complexity_threshold) {
        return time_dilation_activate(system, TIME_TRIGGER_COMPLEXITY, 0.0f);
    }

    /* No triggers active - deactivate if dilating */
    if (system->state.is_dilating && system->config.mode == TIME_MODE_ADAPTIVE) {
        time_dilation_deactivate(system);
    }

    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Event Processing Implementation
 * ============================================================================ */

int time_dilation_submit_event(time_dilation_system_t* system,
                               const float* data,
                               uint32_t data_size,
                               time_event_priority_t priority,
                               float deadline_ms,
                               uint32_t* event_id) {
    if (!system || !data || !event_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_submit_event: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find empty slot */
    event_entry_t* entry = find_empty_slot(system);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                              "time_dilation_submit_event: event buffer full");
        nimcp_platform_mutex_unlock(system->mutex);
        return TIME_DILATION_ERROR_BUFFER_FULL;
    }

    /* Copy data */
    entry->data_copy = nimcp_malloc(data_size * sizeof(float));
    if (!entry->data_copy) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, data_size * sizeof(float),
                           "time_dilation_submit_event: Failed to copy event data");
        nimcp_platform_mutex_unlock(system->mutex);
        return TIME_DILATION_ERROR_NO_MEMORY;
    }
    memcpy(entry->data_copy, data, data_size * sizeof(float));

    /* Fill event */
    uint64_t now = get_time_us();
    entry->event.event_id = system->next_event_id++;
    entry->event.real_time_us = now;
    entry->event.subjective_time_us = system->state.subjective_time_us;
    entry->event.data = entry->data_copy;
    entry->event.data_size = data_size;
    entry->event.priority = priority;
    entry->event.processed = false;
    entry->event.processing_deadline_ms = deadline_ms;
    entry->active = true;

    system->event_count++;
    system->state.pending_events = system->event_count;

    *event_id = entry->event.event_id;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_process(time_dilation_system_t* system,
                          float real_delta_ms,
                          uint32_t* events_processed) {
    if (!system || !events_processed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_process: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    uint64_t start_time = get_time_us();
    *events_processed = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Update timing */
    uint64_t real_delta_us = (uint64_t)(real_delta_ms * 1000.0f);
    system->state.real_time_us += real_delta_us;

    /* Update dilation factor with ramping */
    update_dilation_factor(system, real_delta_ms / 1000.0f);

    /* Compute subjective time advancement */
    uint64_t subjective_delta_us = (uint64_t)(real_delta_us * system->current_factor);
    system->state.subjective_time_us += subjective_delta_us;
    system->accumulated_subjective += subjective_delta_us;

    /* Consume energy during dilation */
    if (system->state.is_dilating && system->current_factor > 1.1f) {
        float energy_cost = (system->current_factor - 1.0f) * real_delta_ms * 0.001f;
        system->state.energy_consumed += energy_cost;
        system->state.energy_remaining -= energy_cost;
        system->stats.total_energy_consumed += energy_cost;

        /* Check duration limit */
        if (system->state.dilation_start_us > 0) {
            uint64_t duration_ms = (get_time_us() - system->state.dilation_start_us) / 1000;
            if (duration_ms > system->config.max_dilation_duration_ms) {
                system->target_factor = TIME_DILATION_DEFAULT;
                NIMCP_LOGGING_WARN("Time dilation duration limit exceeded");
            }
        }

        /* Check energy limit */
        if (system->state.energy_remaining <= 0.0f) {
            system->target_factor = TIME_DILATION_DEFAULT;
            system->state.is_dilating = false;
            NIMCP_LOGGING_WARN("Time dilation energy exhausted");
        }
    }

    /* Process events in dilated time */
    uint32_t max_events = (uint32_t)(system->current_factor * 10);  /* Scale with dilation */
    float current_deadline = get_time_ms() + real_delta_ms;

    while (*events_processed < max_events) {
        event_entry_t* entry = get_next_event(system);
        if (!entry) break;

        /* Check if we have time */
        float now_ms = get_time_ms();
        if (now_ms >= current_deadline) break;

        /* Check event deadline */
        bool deadline_met = true;
        if (entry->event.processing_deadline_ms > 0.0f) {
            float elapsed_ms = (now_ms * 1000.0f - entry->event.real_time_us) / 1000.0f;
            if (elapsed_ms > entry->event.processing_deadline_ms) {
                deadline_met = false;
                system->stats.deadline_misses++;
            }
        }

        /* Mark as processed */
        entry->event.processed = true;
        (*events_processed)++;
        system->stats.total_events_processed++;

        /* Track latency */
        float latency_us = (float)(get_time_us() - entry->event.real_time_us);
        system->stats.avg_event_latency_us =
            system->stats.avg_event_latency_us * 0.99f + latency_us * 0.01f;
        if (latency_us < system->stats.min_event_latency_us ||
            system->stats.min_event_latency_us == 0.0f) {
            system->stats.min_event_latency_us = latency_us;
        }
    }

    /* Update state */
    system->state.processed_events = *events_processed;
    system->state.pending_events = system->event_count - *events_processed;
    system->state.event_throughput = (*events_processed) / (real_delta_ms / 1000.0f + 0.001f);

    /* Update statistics */
    if (system->state.is_dilating) {
        system->state.dilation_duration_us = get_time_us() - system->state.dilation_start_us;
    }

    system->stats.avg_dilation_factor =
        system->stats.avg_dilation_factor * 0.99f + system->current_factor * 0.01f;
    if (system->current_factor > system->stats.max_dilation_achieved) {
        system->stats.max_dilation_achieved = system->current_factor;
    }
    if (system->current_factor < system->stats.min_dilation_achieved ||
        system->stats.min_dilation_achieved == 0.0f) {
        system->stats.min_dilation_achieved = system->current_factor;
    }

    /* Update deadline hit rate */
    if (system->stats.total_events_processed > 0) {
        system->stats.deadline_hit_rate = 1.0f -
            ((float)system->stats.deadline_misses / system->stats.total_events_processed);
    }

    /* Compute processing load */
    uint64_t end_time = get_time_us();
    float processing_time_ms = (float)(end_time - start_time) / 1000.0f;
    system->state.processing_load = processing_time_ms / (real_delta_ms + 0.001f);
    system->stats.avg_processing_load =
        system->stats.avg_processing_load * 0.99f + system->state.processing_load * 0.01f;
    if (system->state.processing_load > system->stats.peak_processing_load) {
        system->stats.peak_processing_load = system->state.processing_load;
    }

    system->last_update_real_us = end_time;
    system->last_update_subj_us = system->state.subjective_time_us;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_event(time_dilation_system_t* system,
                            time_event_t* event) {
    if (!system || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_event: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find oldest processed event */
    event_entry_t* oldest = NULL;
    uint64_t oldest_time = UINT64_MAX;

    for (uint32_t i = 0; i < system->event_capacity; i++) {
        if (system->events[i].active && system->events[i].event.processed) {
            if (system->events[i].event.real_time_us < oldest_time) {
                oldest_time = system->events[i].event.real_time_us;
                oldest = &system->events[i];
            }
        }
    }

    if (!oldest) {
        nimcp_platform_mutex_unlock(system->mutex);
        return TIME_DILATION_ERROR_BUFFER_EMPTY;
    }

    /* Copy event (but not data - caller must use before next call) */
    *event = oldest->event;

    /* Free entry */
    if (oldest->data_copy) {
        nimcp_free(oldest->data_copy);
        oldest->data_copy = NULL;
        event->data = NULL;  /* Prevent use-after-free: data was freed above */
    }
    oldest->active = false;
    system->event_count--;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

int time_dilation_clear_events(time_dilation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_clear_events: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->event_capacity; i++) {
        if (system->events[i].data_copy) {
            nimcp_free(system->events[i].data_copy);
            system->events[i].data_copy = NULL;
        }
        system->events[i].active = false;
    }
    system->event_count = 0;
    system->state.pending_events = 0;

    nimcp_platform_mutex_unlock(system->mutex);
    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Temporal Conversion Implementation
 * ============================================================================ */

int time_dilation_real_to_subjective(time_dilation_system_t* system,
                                     uint64_t real_time_us,
                                     uint64_t* subjective_time_us) {
    if (!system || !subjective_time_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_real_to_subjective: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    /* Convert using current dilation factor */
    /* Note: This is an approximation - true conversion would track history */
    int64_t delta = (int64_t)real_time_us - (int64_t)system->base_real_time_us;
    *subjective_time_us = system->base_real_time_us +
                          (uint64_t)(delta * system->current_factor);

    return TIME_DILATION_SUCCESS;
}

int time_dilation_subjective_to_real(time_dilation_system_t* system,
                                     uint64_t subjective_time_us,
                                     uint64_t* real_time_us) {
    if (!system || !real_time_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_subjective_to_real: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    /* Inverse conversion */
    if (system->current_factor < 0.001f) {
        return TIME_DILATION_ERROR_INVALID_STATE;
    }

    int64_t delta = (int64_t)subjective_time_us - (int64_t)system->base_real_time_us;
    *real_time_us = system->base_real_time_us +
                    (uint64_t)(delta / system->current_factor);

    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_subjective_time(time_dilation_system_t* system,
                                      uint64_t* subjective_time_us) {
    if (!system || !subjective_time_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_subjective_time: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    *subjective_time_us = system->state.subjective_time_us;
    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_resolution(const time_dilation_system_t* system,
                                 float* resolution_us) {
    if (!system || !resolution_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_resolution: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    /* Effective resolution improves with dilation */
    *resolution_us = system->state.effective_resolution_us / system->current_factor;

    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Reaction Time Implementation
 * ============================================================================ */

int time_dilation_start_reaction(time_dilation_system_t* system,
                                 time_reaction_t* reaction) {
    if (!system || !reaction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_start_reaction: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    memset(reaction, 0, sizeof(time_reaction_t));
    reaction->stimulus_time_ms = get_time_ms();
    reaction->dilation_factor = system->current_factor;

    return TIME_DILATION_SUCCESS;
}

int time_dilation_record_response(time_dilation_system_t* system,
                                  time_reaction_t* reaction) {
    if (!system || !reaction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_record_response: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    reaction->response_time_ms = get_time_ms();
    reaction->decision_time_ms = reaction->response_time_ms - reaction->stimulus_time_ms;

    return TIME_DILATION_SUCCESS;
}

int time_dilation_complete_reaction(time_dilation_system_t* system,
                                    time_reaction_t* reaction,
                                    bool is_accurate) {
    if (!system || !reaction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_complete_reaction: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    float complete_time = get_time_ms();
    reaction->motor_time_ms = complete_time - reaction->response_time_ms;
    reaction->reaction_time_ms = complete_time - reaction->stimulus_time_ms;
    reaction->is_accurate = is_accurate;

    /* Record if accurate */
    if (is_accurate) {
        record_reaction_time(system, reaction->reaction_time_ms);

        /* Compute improvement vs baseline (assume 250ms baseline) */
        float baseline_ms = 250.0f;
        system->stats.reaction_improvement =
            (baseline_ms - system->stats.avg_reaction_time_ms) / baseline_ms;
    }

    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_avg_reaction(const time_dilation_system_t* system,
                                   float* avg_ms) {
    if (!system || !avg_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_avg_reaction: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    if (system->reactions.count == 0) {
        *avg_ms = 0.0f;
        return TIME_DILATION_SUCCESS;
    }

    *avg_ms = system->reactions.sum_ms / system->reactions.count;
    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * State and Statistics Implementation
 * ============================================================================ */

int time_dilation_get_state(const time_dilation_system_t* system,
                            time_dilation_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_state: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    *state = system->state;
    return TIME_DILATION_SUCCESS;
}

int time_dilation_get_stats(const time_dilation_system_t* system,
                            time_dilation_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_get_stats: NULL parameter");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    *stats = system->stats;
    return TIME_DILATION_SUCCESS;
}

int time_dilation_reset_stats(time_dilation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "time_dilation_reset_stats: system is NULL");
        return TIME_DILATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(time_dilation_stats_t));
    memset(&system->reactions, 0, sizeof(reaction_tracker_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return TIME_DILATION_SUCCESS;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

bool time_dilation_is_active(const time_dilation_system_t* system) {
    if (!system) {
        return false;
    }
    return system->state.is_dilating;
}

float time_dilation_get_factor(const time_dilation_system_t* system) {
    if (!system) return TIME_DILATION_DEFAULT;
    return system->current_factor;
}

const char* time_dilation_error_string(time_dilation_error_t error) {
    switch (error) {
        case TIME_DILATION_SUCCESS:              return "Success";
        case TIME_DILATION_ERROR_NULL_POINTER:   return "Null pointer";
        case TIME_DILATION_ERROR_INVALID_PARAM:  return "Invalid parameter";
        case TIME_DILATION_ERROR_NO_MEMORY:      return "Memory allocation failed";
        case TIME_DILATION_ERROR_NOT_INITIALIZED: return "System not initialized";
        case TIME_DILATION_ERROR_INVALID_STATE:  return "Invalid state";
        case TIME_DILATION_ERROR_BUFFER_FULL:    return "Buffer full";
        case TIME_DILATION_ERROR_BUFFER_EMPTY:   return "Buffer empty";
        case TIME_DILATION_ERROR_LIMIT_EXCEEDED: return "Limit exceeded";
        case TIME_DILATION_ERROR_RESOURCE_EXHAUSTED: return "Resource exhausted";
        default:                                  return "Unknown error";
    }
}
