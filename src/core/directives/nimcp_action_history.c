/**
 * @file nimcp_action_history.c
 * @brief Action history tracking implementation
 *
 * WHAT: Thread-safe circular buffer implementation for action history tracking
 * WHY:  Provides efficient temporal action storage for combinatorial harm detection
 * HOW:  Circular buffer with mutex protection, time-windowed queries, and bio-async
 */

#include "core/directives/nimcp_action_history.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(action_history)

/**
 * @brief Circular buffer entry
 *
 * WHAT: Wrapper for action record with validity flag
 * WHY:  Allows marking entries as invalid without moving data
 * HOW:  Simple struct with valid flag and embedded record
 */
typedef struct {
    bool valid;                     /* Whether this entry is valid */
    action_record_t record;         /* The actual action record */
} history_entry_t;

/**
 * @brief Action history tracker structure
 *
 * WHAT: Complete state for action history tracking
 * WHY:  Encapsulates circular buffer, config, and synchronization
 * HOW:  Circular buffer with head/tail/count indices, mutex, bio-async context
 */
struct action_history_t {
    action_history_config_t config; /* Configuration parameters */
    history_entry_t* buffer;        /* Circular buffer of entries */
    uint32_t head;                  /* Write position (next insert) */
    uint32_t tail;                  /* Read position (oldest entry) */
    uint32_t count;                 /* Number of valid entries */
    uint32_t capacity;              /* Buffer capacity */
    uint32_t next_action_id;        /* Next action ID to assign */
    nimcp_platform_mutex_t* mutex;  /* Thread safety */
    bio_module_context_t bio_ctx;   /* Bio-async module context */
    bool bio_async_enabled;         /* Whether bio-async is active */
};

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Returns current time as millisecond timestamp
 * WHY:  Used for time-windowed queries and pruning
 * HOW:  Uses clock_gettime with CLOCK_MONOTONIC
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void action_history_default_config(action_history_config_t* config) {
    if (!config) {
        return;
    }

    config->max_history_size = ACTION_HISTORY_MAX_ENTRIES;
    config->time_window_ms = ACTION_HISTORY_DEFAULT_WINDOW_MS;
    config->auto_prune = true;
}

action_history_t* action_history_create(const action_history_config_t* config) {
    /* Use default config if none provided */
    action_history_config_t default_config;
    if (!config) {
        action_history_default_config(&default_config);
        config = &default_config;
    }

    /* Validate config */
    if (config->max_history_size == 0 ||
        config->max_history_size > ACTION_HISTORY_MAX_ENTRIES) {
        NIMCP_LOGGING_ERROR("Invalid max_history_size: %u", config->max_history_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_history_create: config is NULL");
        return NULL;
    }

    /* Allocate main structure */
    action_history_t* history = (action_history_t*)nimcp_malloc(sizeof(action_history_t));
    if (!history) {
        NIMCP_LOGGING_ERROR("Failed to allocate action_history_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "history is NULL");

        return NULL;
    }

    /* Initialize fields */
    history->config = *config;
    history->capacity = config->max_history_size;
    history->head = 0;
    history->tail = 0;
    history->count = 0;
    history->next_action_id = 1;
    history->bio_ctx = NULL;
    history->bio_async_enabled = false;

    /* Allocate circular buffer */
    history->buffer = (history_entry_t*)nimcp_malloc(
        sizeof(history_entry_t) * history->capacity);
    if (!history->buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate circular buffer");
        nimcp_free(history);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "action_history_create: history->buffer is NULL");
        return NULL;
    }

    /* Initialize all entries as invalid */
    for (uint32_t i = 0; i < history->capacity; i++) {
        history->buffer[i].valid = false;
    }

    /* Create mutex */
    history->mutex = nimcp_platform_mutex_create();
    if (!history->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(history->buffer);
        nimcp_free(history);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "action_history_create: history->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created action history (capacity=%u, window=%lu ms)",
                       history->capacity, config->time_window_ms);

    return history;
}

void action_history_destroy(action_history_t* history) {
    if (!history) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (history->bio_async_enabled) {
        action_history_disconnect_bio_async(history);
    }

    /* Destroy and free mutex */
    if (history->mutex) {
        nimcp_platform_mutex_destroy(history->mutex);
        nimcp_free(history->mutex);
    }

    /* Free circular buffer */
    if (history->buffer) {
        nimcp_free(history->buffer);
    }

    /* Free main structure */
    nimcp_free(history);

    NIMCP_LOGGING_INFO("Destroyed action history");
}

int action_history_record(action_history_t* history, const action_record_t* record) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!record) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    /* Auto-prune if enabled */
    if (history->config.auto_prune && history->count > 0) {
        uint64_t cutoff = get_current_timestamp_ms() - history->config.time_window_ms;

        /* Remove old entries from tail */
        while (history->count > 0 &&
               history->buffer[history->tail].valid &&
               history->buffer[history->tail].record.timestamp_ms < cutoff) {
            history->buffer[history->tail].valid = false;
            history->tail = (history->tail + 1) % history->capacity;
            history->count--;
        }
    }

    /* If buffer is full, overwrite oldest entry */
    if (history->count >= history->capacity) {
        history->buffer[history->tail].valid = false;
        history->tail = (history->tail + 1) % history->capacity;
        history->count--;
    }

    /* Write new entry at head */
    history->buffer[history->head].valid = true;
    history->buffer[history->head].record = *record;

    /* Assign action ID if not set */
    if (history->buffer[history->head].record.action_id == 0) {
        history->buffer[history->head].record.action_id = history->next_action_id++;
    }

    /* Update head and count */
    history->head = (history->head + 1) % history->capacity;
    history->count++;

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_DEBUG("Recorded action id=%u type=%s harm=%.3f blocked=%d",
                        record->action_id, record->action_type,
                        record->predicted_harm_score, record->was_blocked);

    return 0;
}

int action_history_get_recent(action_history_t* history,
                               uint64_t time_window_ms,
                               action_record_t* out_records,
                               uint32_t max_count,
                               uint32_t* out_count) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!out_records || !out_count) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    *out_count = 0;

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    /* Calculate cutoff timestamp */
    uint64_t cutoff = 0;
    if (time_window_ms > 0) {
        uint64_t now = get_current_timestamp_ms();
        cutoff = now - time_window_ms;
    }

    /* Scan circular buffer from tail to head */
    uint32_t idx = history->tail;
    for (uint32_t i = 0; i < history->count && *out_count < max_count; i++) {
        if (history->buffer[idx].valid) {
            /* Check if within time window */
            if (time_window_ms == 0 ||
                history->buffer[idx].record.timestamp_ms >= cutoff) {
                out_records[*out_count] = history->buffer[idx].record;
                (*out_count)++;
            }
        }
        idx = (idx + 1) % history->capacity;
    }

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_DEBUG("Retrieved %u recent actions (window=%lu ms)",
                        *out_count, time_window_ms);

    return 0;
}

int action_history_get_by_type(action_history_t* history,
                                const char* action_type,
                                action_record_t* out_records,
                                uint32_t max_count,
                                uint32_t* out_count) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!action_type || !out_records || !out_count) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    *out_count = 0;

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    /* Scan circular buffer from tail to head */
    uint32_t idx = history->tail;
    for (uint32_t i = 0; i < history->count && *out_count < max_count; i++) {
        if (history->buffer[idx].valid) {
            /* Check if type matches */
            if (strncmp(history->buffer[idx].record.action_type,
                       action_type, ACTION_TYPE_MAX_LEN) == 0) {
                out_records[*out_count] = history->buffer[idx].record;
                (*out_count)++;
            }
        }
        idx = (idx + 1) % history->capacity;
    }

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_DEBUG("Retrieved %u actions of type '%s'",
                        *out_count, action_type);

    return 0;
}

int action_history_get_stats(action_history_t* history,
                              action_history_stats_t* stats) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!stats) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Initialize stats */
    memset(stats, 0, sizeof(action_history_stats_t));
    stats->oldest_timestamp_ms = UINT64_MAX;
    stats->newest_timestamp_ms = 0;
    stats->max_harm_score = 0.0f;

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    /* Handle empty history */
    if (history->count == 0) {
        stats->oldest_timestamp_ms = 0;
        stats->newest_timestamp_ms = 0;
        nimcp_platform_mutex_unlock(history->mutex);
        return 0;
    }

    /* Track unique types */
    char unique_types[ACTION_HISTORY_MAX_ENTRIES][ACTION_TYPE_MAX_LEN];
    uint32_t unique_count = 0;

    /* Scan circular buffer */
    float total_harm = 0.0f;
    uint32_t idx = history->tail;
    for (uint32_t i = 0; i < history->count; i++) {
        if (!history->buffer[idx].valid) {
            idx = (idx + 1) % history->capacity;
            continue;
        }

        const action_record_t* rec = &history->buffer[idx].record;
        stats->total_records++;

        /* Count blocked actions */
        if (rec->was_blocked) {
            stats->blocked_count++;
        }

        /* Update timestamps */
        if (rec->timestamp_ms < stats->oldest_timestamp_ms) {
            stats->oldest_timestamp_ms = rec->timestamp_ms;
        }
        if (rec->timestamp_ms > stats->newest_timestamp_ms) {
            stats->newest_timestamp_ms = rec->timestamp_ms;
        }

        /* Update harm scores */
        total_harm += rec->predicted_harm_score;
        if (rec->predicted_harm_score > stats->max_harm_score) {
            stats->max_harm_score = rec->predicted_harm_score;
        }

        /* Track unique types */
        bool type_exists = false;
        for (uint32_t j = 0; j < unique_count; j++) {
            if (strncmp(unique_types[j], rec->action_type, ACTION_TYPE_MAX_LEN) == 0) {
                type_exists = true;
                break;
            }
        }
        if (!type_exists && unique_count < ACTION_HISTORY_MAX_ENTRIES) {
            strncpy(unique_types[unique_count], rec->action_type, ACTION_TYPE_MAX_LEN);
            unique_count++;
        }

        idx = (idx + 1) % history->capacity;
    }

    stats->unique_types = unique_count;
    if (stats->total_records > 0) {
        stats->avg_harm_score = total_harm / stats->total_records;
    }

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_DEBUG("Stats: total=%u blocked=%u types=%u avg_harm=%.3f",
                        stats->total_records, stats->blocked_count,
                        stats->unique_types, stats->avg_harm_score);

    return 0;
}

int action_history_prune(action_history_t* history, uint64_t older_than_ms) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    uint32_t pruned = 0;

    /* Remove old entries from tail */
    while (history->count > 0 &&
           history->buffer[history->tail].valid &&
           history->buffer[history->tail].record.timestamp_ms < older_than_ms) {
        history->buffer[history->tail].valid = false;
        history->tail = (history->tail + 1) % history->capacity;
        history->count--;
        pruned++;
    }

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_DEBUG("Pruned %u entries older than %lu ms",
                        pruned, older_than_ms);

    return (int)pruned;
}

int action_history_clear(action_history_t* history) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(history->mutex);

    /* Mark all entries as invalid */
    for (uint32_t i = 0; i < history->capacity; i++) {
        history->buffer[i].valid = false;
    }

    /* Reset indices */
    history->head = 0;
    history->tail = 0;
    history->count = 0;

    nimcp_platform_mutex_unlock(history->mutex);

    NIMCP_LOGGING_INFO("Cleared all action history");

    return 0;
}

int action_history_connect_bio_async(action_history_t* history) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard: already connected */
    if (history->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        return 0;
    }

    /* Create module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_ACTION_HISTORY,
        .module_name = "action_history",
        .inbox_capacity = 32,
        .user_data = history
    };

    /* Register with bio-async router */
    history->bio_ctx = bio_router_register_module(&info);
    if (history->bio_ctx) {
        history->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
    }
}

int action_history_disconnect_bio_async(action_history_t* history) {
    /* Guard: validate inputs */
    if (!history) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard: not connected */
    if (!history->bio_async_enabled) {
        return 0;
    }

    /* Unregister from bio-async router */
    if (history->bio_ctx) {
        bio_router_unregister_module(history->bio_ctx);
        history->bio_ctx = NULL;
    }

    history->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool action_history_is_bio_async_connected(const action_history_t* history) {
    if (!history) {
        return false;
    }

    return history->bio_async_enabled;
}
