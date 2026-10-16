//=============================================================================
// nimcp_pr_logging_bridge.c - Prime Resonant Logging Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_logging_bridge.c
 * @brief Implementation of comprehensive logging for PR Memory System
 *
 * WHAT: Implements ring buffer logging, filtering, export functionality
 * WHY:  Enable debugging, analysis, and performance profiling
 * HOW:  Lock-free ring buffer, structured entries, multiple export formats
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_pr_logging_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_logging_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_logging_bridge module (instance-level) */
static inline void pr_logging_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_logging_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_logging_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_logging_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_LOGGING_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform Abstraction
//=============================================================================

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION pr_log_mutex_t;
    #define PR_LOG_MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define PR_LOG_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define PR_LOG_MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define PR_LOG_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))

    static uint64_t get_time_ns(void) {
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        return (uint64_t)((count.QuadPart * 1000000000ULL) / freq.QuadPart);
    }

    static uint64_t get_thread_id(void) {
        return (uint64_t)GetCurrentThreadId();
    }
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/syscall.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    typedef nimcp_mutex_t pr_log_mutex_t;
    #define PR_LOG_MUTEX_INIT(m) nimcp_mutex_init(&(m), NULL)
    #define PR_LOG_MUTEX_DESTROY(m) nimcp_mutex_destroy(&(m))
    #define PR_LOG_MUTEX_LOCK(m) nimcp_mutex_lock(&(m))
    #define PR_LOG_MUTEX_UNLOCK(m) nimcp_mutex_unlock(&(m))

    static uint64_t get_time_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)(ts.tv_sec * 1000000000ULL + ts.tv_nsec);
    }

    static uint64_t get_thread_id(void) {
        #ifdef __linux__
            return (uint64_t)syscall(SYS_gettid);
        #else
            return (uint64_t)(uintptr_t)pthread_self();
        #endif
    }
#endif

static uint64_t get_time_ms(void) {
    return get_time_ns() / 1000000ULL;
}

//=============================================================================
// Helper Functions
//=============================================================================

static void safe_strncpy(char* dest, const char* src, size_t size) {
    if (!dest || size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
}


/**
 * @brief Check if entry matches filter
 */
static bool entry_matches_filter(const pr_log_entry_t* entry,
                                  const pr_log_filter_t* filter) {
    if (!entry || !filter) return true;

    /* Time range */
    if (filter->start_time_ns > 0 && entry->timestamp_ns < filter->start_time_ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: validation failed");
        return false;
    }
    if (filter->end_time_ns > 0 && entry->timestamp_ns > filter->end_time_ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: validation failed");
        return false;
    }

    /* Level filter */
    if (entry->level < filter->min_level || entry->level > filter->max_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: validation failed");
        return false;
    }

    /* Category filter */
    if (filter->category_mask != 0) {
        uint32_t cat_bit = 1u << entry->category;
        if (!(filter->category_mask & cat_bit)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: validation failed");
            return false;
        }
    }

    /* Memory filter */
    if (filter->memory_id != 0 && entry->memory_id != filter->memory_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "safe_strncpy: validation failed");
        return false;
    }

    /* Tier filter */
    if (filter->tier != PR_MEMORY_TIER_COUNT && entry->tier != filter->tier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "safe_strncpy: validation failed");
        return false;
    }

    /* Metric filter */
    if (!isnan(filter->min_metric) && entry->metric_value < filter->min_metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: isnan is NULL");
        return false;
    }
    if (!isnan(filter->max_metric) && entry->metric_value > filter->max_metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strncpy: isnan is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Format timestamp as string
 */
static void format_timestamp(uint64_t timestamp_ns, char* buffer, size_t size) {
    uint64_t ms = timestamp_ns / 1000000ULL;
    uint64_t ns_rem = timestamp_ns % 1000000ULL;
    uint64_t sec = ms / 1000;
    uint64_t ms_rem = ms % 1000;

    snprintf(buffer, size, "%lu.%03lu.%06lu",
             (unsigned long)sec, (unsigned long)ms_rem, (unsigned long)ns_rem);
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal bridge structure
 */
struct pr_logging_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_logging_config_t config;

    /* Ring buffer */
    pr_log_entry_t* entries;
    size_t capacity;
    size_t write_idx;
    size_t count;
    uint64_t sequence_counter;
    pr_log_mutex_t buffer_mutex;
    bool buffer_mutex_initialized;

    /* File output */
    FILE* log_file;
    pr_log_mutex_t file_mutex;
    bool file_mutex_initialized;

    /* Duplicate detection */
    pr_log_entry_t last_entry;
    uint32_t duplicate_count;
    uint64_t last_entry_time_ns;

    /* Statistics */
    pr_logging_stats_t stats;
    pr_log_mutex_t stats_mutex;
    bool stats_mutex_initialized;

    /* State */
    bool initialized;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_logging_bridge, struct pr_logging_bridge_struct)

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_logging_config_t pr_logging_config_default(void) {
    pr_logging_config_t config;
    memset(&config, 0, sizeof(config));

    config.buffer_capacity = PR_LOG_DEFAULT_BUFFER_SIZE;
    config.overwrite_on_full = true;

    config.min_level = PR_LOG_LEVEL_DEBUG;
    config.enabled_categories = PR_LOG_CAT_ALL;

    config.log_to_console = false;
    config.log_to_file = false;
    config.file_format = PR_LOG_FORMAT_HUMAN;

    config.enable_timestamps = true;
    config.enable_thread_id = true;
    config.enable_signatures = false;
    config.flush_interval_ms = PR_LOG_DEFAULT_FLUSH_INTERVAL;

    config.filter_duplicates = true;
    config.duplicate_window_ms = 100;

    return config;
}

NIMCP_EXPORT bool pr_logging_config_validate(const pr_logging_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_logging_config_validate: config is NULL");
        return false;
    }

    if (config->buffer_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_logging_config_validate: config->buffer_capacity is zero");
        return false;
    }
    if (config->min_level >= PR_LOG_LEVEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_logging_config_validate: capacity exceeded");
        return false;
    }

    return true;
}

NIMCP_EXPORT pr_log_filter_t pr_log_filter_default(void) {
    pr_log_filter_t filter;
    memset(&filter, 0, sizeof(filter));

    filter.min_level = PR_LOG_LEVEL_TRACE;
    filter.max_level = PR_LOG_LEVEL_FATAL;
    filter.category_mask = 0;  /* All categories */
    filter.tier = PR_MEMORY_TIER_COUNT;  /* All tiers */
    filter.min_metric = NAN;
    filter.max_metric = NAN;

    return filter;
}

NIMCP_EXPORT pr_log_filter_t pr_log_filter_for_memory(uint64_t memory_id) {
    pr_log_filter_t filter = pr_log_filter_default();
    filter.memory_id = memory_id;
    return filter;
}

NIMCP_EXPORT pr_log_filter_t pr_log_filter_for_time_range(
    uint64_t start_ns,
    uint64_t end_ns
) {
    pr_log_filter_t filter = pr_log_filter_default();
    filter.start_time_ns = start_ns;
    filter.end_time_ns = end_ns;
    return filter;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_logging_bridge_t pr_logging_bridge_create(
    const pr_logging_config_t* config
) {
    pr_logging_config_t cfg;
    if (config) {
        if (!pr_logging_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_logging_bridge_create: pr_logging_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_logging_config_default();
    }

    /* Allocate bridge */
    pr_logging_bridge_t bridge = (pr_logging_bridge_t)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->config = cfg;

    /* Allocate ring buffer */
    bridge->entries = (pr_log_entry_t*)nimcp_calloc(cfg.buffer_capacity,
                                               sizeof(pr_log_entry_t));
    if (!bridge->entries) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_logging_bridge_create: bridge->entries is NULL");
        return NULL;
    }
    bridge->capacity = cfg.buffer_capacity;
    bridge->write_idx = 0;
    bridge->count = 0;
    bridge->sequence_counter = 0;

    /* Initialize mutexes */
    PR_LOG_MUTEX_INIT(bridge->buffer_mutex);
    bridge->buffer_mutex_initialized = true;

    PR_LOG_MUTEX_INIT(bridge->file_mutex);
    bridge->file_mutex_initialized = true;

    PR_LOG_MUTEX_INIT(bridge->stats_mutex);
    bridge->stats_mutex_initialized = true;

    /* Open log file if configured */
    if (cfg.log_to_file && cfg.log_file_path[0] != '\0') {
        bridge->log_file = fopen(cfg.log_file_path, "a");
        /* Non-fatal if file open fails */
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.buffer_capacity = cfg.buffer_capacity;

    bridge->initialized = true;

    return bridge;
}

NIMCP_EXPORT void pr_logging_bridge_destroy(pr_logging_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_logging");

    /* Close log file */
    if (bridge->log_file) {
        fflush(bridge->log_file);
        fclose(bridge->log_file);
    }

    /* Free ring buffer */
    if (bridge->entries) {
        nimcp_free(bridge->entries);
    }

    /* Destroy mutexes */
    if (bridge->buffer_mutex_initialized) {
        PR_LOG_MUTEX_DESTROY(bridge->buffer_mutex);
    }
    if (bridge->file_mutex_initialized) {
        PR_LOG_MUTEX_DESTROY(bridge->file_mutex);
    }
    if (bridge->stats_mutex_initialized) {
        PR_LOG_MUTEX_DESTROY(bridge->stats_mutex);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_flush(pr_logging_bridge_t bridge) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    if (bridge->log_file) {
        PR_LOG_MUTEX_LOCK(bridge->file_mutex);
        fflush(bridge->log_file);
        PR_LOG_MUTEX_UNLOCK(bridge->file_mutex);

        PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.file_flushes++;
        PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_clear(pr_logging_bridge_t bridge) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    PR_LOG_MUTEX_LOCK(bridge->buffer_mutex);
    bridge->write_idx = 0;
    bridge->count = 0;
    memset(bridge->entries, 0, bridge->capacity * sizeof(pr_log_entry_t));
    PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

    PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.current_size = 0;
    bridge->stats.buffer_utilization = 0.0f;
    PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_set_level(
    pr_logging_bridge_t bridge,
    pr_log_level_t level
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;
    if (level >= PR_LOG_LEVEL_COUNT) return PR_LOG_ERROR_INVALID_CONFIG;

    bridge->config.min_level = level;
    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_set_category(
    pr_logging_bridge_t bridge,
    pr_log_category_t category,
    bool enabled
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;
    if (category >= PR_LOG_CAT_COUNT) return PR_LOG_ERROR_INVALID_CONFIG;

    uint32_t mask = 1u << category;
    if (enabled) {
        bridge->config.enabled_categories |= mask;
    } else {
        bridge->config.enabled_categories &= ~mask;
    }

    return PR_LOG_SUCCESS;
}

//=============================================================================
// Core Logging Functions
//=============================================================================

/**
 * @brief Internal logging function (assumes level/category already checked)
 */
static pr_log_error_t log_entry_internal(pr_logging_bridge_t bridge,
                                          const pr_log_entry_t* entry) {
    uint64_t start_ns = get_time_ns();

    PR_LOG_MUTEX_LOCK(bridge->buffer_mutex);

    /* Check for duplicate */
    if (bridge->config.filter_duplicates && bridge->count > 0) {
        uint64_t window_ns = bridge->config.duplicate_window_ms * 1000000ULL;
        if (entry->timestamp_ns - bridge->last_entry_time_ns < window_ns) {
            if (entry->level == bridge->last_entry.level &&
                entry->category == bridge->last_entry.category &&
                entry->memory_id == bridge->last_entry.memory_id &&
                strcmp(entry->message, bridge->last_entry.message) == 0) {
                bridge->duplicate_count++;
                PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

                PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
                bridge->stats.duplicates_filtered++;
                PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

                return PR_LOG_SUCCESS;
            }
        }
    }

    /* Check buffer space */
    if (bridge->count >= bridge->capacity) {
        if (!bridge->config.overwrite_on_full) {
            PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

            PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
            bridge->stats.entries_dropped++;
            PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

            return PR_LOG_ERROR_BUFFER_FULL;
        }
        /* Overwrite oldest */
        PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.overwrites++;
        PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);
    }

    /* Write entry */
    pr_log_entry_t* dest = &bridge->entries[bridge->write_idx];
    *dest = *entry;
    dest->sequence_number = bridge->sequence_counter++;
    if (bridge->config.enable_timestamps && dest->timestamp_ns == 0) {
        dest->timestamp_ns = start_ns;
    }
    if (bridge->config.enable_thread_id && dest->thread_id == 0) {
        dest->thread_id = get_thread_id();
    }

    /* Update indices */
    bridge->write_idx = (bridge->write_idx + 1) % bridge->capacity;
    if (bridge->count < bridge->capacity) {
        bridge->count++;
    }

    /* Update duplicate tracking */
    bridge->last_entry = *dest;
    bridge->last_entry_time_ns = dest->timestamp_ns;
    bridge->duplicate_count = 0;

    PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

    /* Console output */
    if (bridge->config.log_to_console) {
        char buffer[NIMCP_ERROR_BUFFER_LARGE];
        pr_log_entry_to_string(dest, buffer, sizeof(buffer));
        printf("%s\n", buffer);
    }

    /* File output */
    if (bridge->log_file) {
        PR_LOG_MUTEX_LOCK(bridge->file_mutex);
        char buffer[NIMCP_ERROR_BUFFER_LARGE];
        pr_log_entry_to_string(dest, buffer, sizeof(buffer));
        fprintf(bridge->log_file, "%s\n", buffer);

        PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
        bridge->stats.file_writes++;
        PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

        PR_LOG_MUTEX_UNLOCK(bridge->file_mutex);
    }

    /* Update statistics */
    uint64_t end_ns = get_time_ns();
    PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.total_entries++;
    bridge->stats.entries_by_level[entry->level]++;
    bridge->stats.entries_by_category[entry->category]++;
    bridge->stats.current_size = bridge->count;
    bridge->stats.buffer_utilization =
        (float)bridge->count / (float)bridge->capacity;
    bridge->stats.last_entry_time_ns = dest->timestamp_ns;

    /* Update timing stats */
    float log_time_ns = (float)(end_ns - start_ns);
    uint64_t n = bridge->stats.total_entries;
    bridge->stats.avg_log_time_ns =
        (bridge->stats.avg_log_time_ns * (float)(n - 1) + log_time_ns) / (float)n;
    if (log_time_ns > bridge->stats.max_log_time_ns) {
        bridge->stats.max_log_time_ns = log_time_ns;
    }
    PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log(
    pr_logging_bridge_t bridge,
    pr_log_level_t level,
    pr_log_category_t category,
    const char* message
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    /* Check level */
    if (level < bridge->config.min_level) return PR_LOG_SUCCESS;

    /* Check category */
    uint32_t cat_mask = 1u << category;
    if (!(bridge->config.enabled_categories & cat_mask)) return PR_LOG_SUCCESS;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = level;
    entry.category = category;
    entry.timestamp_ns = bridge->config.enable_timestamps ? get_time_ns() : 0;
    entry.thread_id = bridge->config.enable_thread_id ? get_thread_id() : 0;
    entry.tier = PR_MEMORY_TIER_COUNT;
    entry.state = quat_identity();

    if (message) {
        safe_strncpy(entry.message, message, sizeof(entry.message));
    }

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_memory(
    pr_logging_bridge_t bridge,
    pr_log_level_t level,
    pr_log_category_t category,
    uint64_t memory_id,
    const char* message
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    if (level < bridge->config.min_level) return PR_LOG_SUCCESS;

    uint32_t cat_mask = 1u << category;
    if (!(bridge->config.enabled_categories & cat_mask)) return PR_LOG_SUCCESS;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = level;
    entry.category = category;
    entry.memory_id = memory_id;
    entry.timestamp_ns = bridge->config.enable_timestamps ? get_time_ns() : 0;
    entry.thread_id = bridge->config.enable_thread_id ? get_thread_id() : 0;
    entry.tier = PR_MEMORY_TIER_COUNT;
    entry.state = quat_identity();

    if (message) {
        safe_strncpy(entry.message, message, sizeof(entry.message));
    }

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_entry(
    pr_logging_bridge_t bridge,
    const pr_log_entry_t* entry
) {
    if (!bridge || !entry) return PR_LOG_ERROR_NULL_POINTER;

    if (entry->level < bridge->config.min_level) return PR_LOG_SUCCESS;

    uint32_t cat_mask = 1u << entry->category;
    if (!(bridge->config.enabled_categories & cat_mask)) return PR_LOG_SUCCESS;

    return log_entry_internal(bridge, entry);
}

//=============================================================================
// Memory Operation Logging Functions
//=============================================================================

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_encode(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    uint64_t encoding_time_ns
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_INFO;
    entry.category = PR_LOG_CAT_ENCODE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.metric_value = (float)encoding_time_ns / 1000.0f;  /* us */

    if (node) {
        entry.memory_id = node->node_id;
        entry.tier = node->tier;
        entry.state = node->state;
        snprintf(entry.message, sizeof(entry.message),
                 "Encoded memory %lu (tier=%s, encoding=%luus)",
                 (unsigned long)node->node_id,
                 pr_memory_tier_name(node->tier),
                 (unsigned long)(encoding_time_ns / 1000));
    } else {
        snprintf(entry.message, sizeof(entry.message),
                 "Encoded memory (encoding=%luus)",
                 (unsigned long)(encoding_time_ns / 1000));
    }

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_retrieve(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    float resonance_score,
    uint64_t retrieval_time_ns
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_DEBUG;
    entry.category = PR_LOG_CAT_RETRIEVE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.metric_value = resonance_score;
    entry.secondary_value = (float)retrieval_time_ns / 1000.0f;

    if (node) {
        entry.memory_id = node->node_id;
        entry.tier = node->tier;
        entry.state = node->state;
        snprintf(entry.message, sizeof(entry.message),
                 "Retrieved memory %lu (resonance=%.3f, latency=%luus)",
                 (unsigned long)node->node_id, resonance_score,
                 (unsigned long)(retrieval_time_ns / 1000));
    } else {
        snprintf(entry.message, sizeof(entry.message),
                 "Retrieved memory (resonance=%.3f, latency=%luus)",
                 resonance_score, (unsigned long)(retrieval_time_ns / 1000));
    }

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_consolidate(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    float old_strength,
    float new_strength
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_DEBUG;
    entry.category = PR_LOG_CAT_CONSOLIDATE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.metric_value = new_strength;
    entry.secondary_value = old_strength;

    if (node) {
        entry.memory_id = node->node_id;
        entry.tier = node->tier;
        entry.state = node->state;
    }

    snprintf(entry.message, sizeof(entry.message),
             "Consolidated memory %lu (strength: %.3f -> %.3f)",
             (unsigned long)entry.memory_id, old_strength, new_strength);

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_promote(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_INFO;
    entry.category = PR_LOG_CAT_PROMOTE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.tier = to_tier;

    if (node) {
        entry.memory_id = node->node_id;
        entry.state = node->state;
    }

    snprintf(entry.message, sizeof(entry.message),
             "Promoted memory %lu (%s -> %s)",
             (unsigned long)entry.memory_id,
             pr_memory_tier_name(from_tier),
             pr_memory_tier_name(to_tier));

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_demote(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_INFO;
    entry.category = PR_LOG_CAT_DEMOTE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.tier = to_tier;

    if (node) {
        entry.memory_id = node->node_id;
        entry.state = node->state;
    }

    snprintf(entry.message, sizeof(entry.message),
             "Demoted memory %lu (%s -> %s)",
             (unsigned long)entry.memory_id,
             pr_memory_tier_name(from_tier),
             pr_memory_tier_name(to_tier));

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_decay(
    pr_logging_bridge_t bridge,
    uint64_t memory_id,
    float old_strength,
    float new_strength,
    uint64_t elapsed_time_ms
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_TRACE;
    entry.category = PR_LOG_CAT_DECAY;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.memory_id = memory_id;
    entry.metric_value = new_strength;
    entry.secondary_value = old_strength;
    entry.tier = PR_MEMORY_TIER_COUNT;
    entry.state = quat_identity();

    snprintf(entry.message, sizeof(entry.message),
             "Decay memory %lu (%.3f -> %.3f, elapsed=%lums)",
             (unsigned long)memory_id, old_strength, new_strength,
             (unsigned long)elapsed_time_ms);

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_entangle(
    pr_logging_bridge_t bridge,
    uint64_t memory_id_1,
    uint64_t memory_id_2,
    float resonance
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_DEBUG;
    entry.category = PR_LOG_CAT_ENTANGLE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.memory_id = memory_id_1;
    entry.correlation_id = memory_id_2;
    entry.metric_value = resonance;
    entry.tier = PR_MEMORY_TIER_COUNT;
    entry.state = quat_identity();

    snprintf(entry.message, sizeof(entry.message),
             "Entangled %lu <-> %lu (resonance=%.3f)",
             (unsigned long)memory_id_1, (unsigned long)memory_id_2, resonance);

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_state_change(
    pr_logging_bridge_t bridge,
    uint64_t memory_id,
    nimcp_quaternion_t old_state,
    nimcp_quaternion_t new_state,
    const char* reason
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_TRACE;
    entry.category = PR_LOG_CAT_STATE;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.memory_id = memory_id;
    entry.state = new_state;
    entry.tier = PR_MEMORY_TIER_COUNT;

    snprintf(entry.message, sizeof(entry.message),
             "State change %lu: (%.2f,%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f,%.2f) [%s]",
             (unsigned long)memory_id,
             old_state.w, old_state.x, old_state.y, old_state.z,
             new_state.w, new_state.x, new_state.y, new_state.z,
             reason ? reason : "");

    return log_entry_internal(bridge, &entry);
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_error(
    pr_logging_bridge_t bridge,
    pr_log_category_t category,
    int error_code,
    const char* message,
    uint64_t memory_id
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.level = PR_LOG_LEVEL_ERROR;
    entry.category = category;
    entry.timestamp_ns = get_time_ns();
    entry.thread_id = get_thread_id();
    entry.memory_id = memory_id;
    entry.flags = (uint32_t)error_code;
    entry.tier = PR_MEMORY_TIER_COUNT;
    entry.state = quat_identity();

    if (message) {
        snprintf(entry.message, sizeof(entry.message),
                 "ERROR[%d]: %s (memory=%lu)",
                 error_code, message, (unsigned long)memory_id);
    } else {
        snprintf(entry.message, sizeof(entry.message),
                 "ERROR[%d] (memory=%lu)", error_code, (unsigned long)memory_id);
    }

    return log_entry_internal(bridge, &entry);
}

//=============================================================================
// Export Functions
//=============================================================================

/**
 * @brief Write entry in JSON format
 */
static size_t write_entry_json(const pr_log_entry_t* entry, char* buffer,
                                size_t size, bool first) {
    char ts_buf[32];
    format_timestamp(entry->timestamp_ns, ts_buf, sizeof(ts_buf));

    int n = snprintf(buffer, size,
        "%s{\"seq\":%lu,\"ts\":\"%s\",\"level\":\"%s\",\"cat\":\"%s\","
        "\"mem\":%lu,\"tier\":\"%s\",\"state\":[%.3f,%.3f,%.3f,%.3f],"
        "\"metric\":%.4f,\"msg\":\"%s\"}",
        first ? "" : ",\n",
        (unsigned long)entry->sequence_number,
        ts_buf,
        pr_log_level_name(entry->level),
        pr_log_category_name(entry->category),
        (unsigned long)entry->memory_id,
        entry->tier < PR_MEMORY_TIER_COUNT ? pr_memory_tier_name(entry->tier) : "N/A",
        entry->state.w, entry->state.x, entry->state.y, entry->state.z,
        entry->metric_value,
        entry->message);

    return (n > 0 && (size_t)n < size) ? (size_t)n : 0;
}

/**
 * @brief Write entry in CSV format
 */
static size_t write_entry_csv(const pr_log_entry_t* entry, char* buffer,
                               size_t size) {
    int n = snprintf(buffer, size,
        "%lu,%lu,%s,%s,%lu,%s,%.3f,%.3f,%.3f,%.3f,%.4f,\"%s\"\n",
        (unsigned long)entry->sequence_number,
        (unsigned long)entry->timestamp_ns,
        pr_log_level_name(entry->level),
        pr_log_category_name(entry->category),
        (unsigned long)entry->memory_id,
        entry->tier < PR_MEMORY_TIER_COUNT ? pr_memory_tier_name(entry->tier) : "N/A",
        entry->state.w, entry->state.x, entry->state.y, entry->state.z,
        entry->metric_value,
        entry->message);

    return (n > 0 && (size_t)n < size) ? (size_t)n : 0;
}

/**
 * @brief Write entry in human-readable format
 */
static size_t write_entry_human(const pr_log_entry_t* entry, char* buffer,
                                 size_t size) {
    char ts_buf[32];
    format_timestamp(entry->timestamp_ns, ts_buf, sizeof(ts_buf));

    int n = snprintf(buffer, size,
        "[%s] %s/%s mem=%lu: %s\n",
        ts_buf,
        pr_log_level_name(entry->level),
        pr_log_category_name(entry->category),
        (unsigned long)entry->memory_id,
        entry->message);

    return (n > 0 && (size_t)n < size) ? (size_t)n : 0;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_trace(
    pr_logging_bridge_t bridge,
    const char* file_path,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    pr_export_result_t* result
) {
    if (!bridge || !file_path) return PR_LOG_ERROR_NULL_POINTER;

    FILE* file = fopen(file_path, "w");
    if (!file) return PR_LOG_ERROR_IO;

    pr_log_error_t err = pr_logging_bridge_export_to_file(bridge, file, format,
                                                           filter, result);

    fclose(file);

    if (result) {
        safe_strncpy(result->file_path, file_path, sizeof(result->file_path));
    }

    return err;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_to_file(
    pr_logging_bridge_t bridge,
    FILE* file,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    pr_export_result_t* result
) {
    if (!bridge || !file) return PR_LOG_ERROR_NULL_POINTER;
    if (format >= PR_LOG_FORMAT_COUNT) return PR_LOG_ERROR_INVALID_FORMAT;

    pr_log_filter_t flt = filter ? *filter : pr_log_filter_default();

    if (result) {
        memset(result, 0, sizeof(*result));
    }

    size_t exported = 0;
    size_t bytes = 0;
    uint64_t first_ts = 0;
    uint64_t last_ts = 0;
    char buffer[NIMCP_LOG_BUFFER_SIZE];

    /* Write header */
    if (format == PR_LOG_FORMAT_JSON) {
        fprintf(file, "{\"entries\":[\n");
        bytes += 14;
    } else if (format == PR_LOG_FORMAT_CSV) {
        const char* header = "seq,timestamp_ns,level,category,memory_id,tier,w,x,y,z,metric,message\n";
        fputs(header, file);
        bytes += strlen(header);
    }

    PR_LOG_MUTEX_LOCK(bridge->buffer_mutex);

    /* Iterate through ring buffer */
    size_t read_idx = 0;
    if (bridge->count >= bridge->capacity) {
        read_idx = bridge->write_idx;  /* Start at oldest */
    }

    for (size_t i = 0; i < bridge->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->count > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)bridge->count);
        }

        pr_log_entry_t* entry = &bridge->entries[read_idx];
        read_idx = (read_idx + 1) % bridge->capacity;

        if (!entry_matches_filter(entry, &flt)) continue;

        if (flt.max_entries > 0 && exported >= flt.max_entries) break;

        size_t written = 0;
        bool first = (exported == 0);

        switch (format) {
            case PR_LOG_FORMAT_JSON:
                written = write_entry_json(entry, buffer, sizeof(buffer), first);
                break;
            case PR_LOG_FORMAT_CSV:
                written = write_entry_csv(entry, buffer, sizeof(buffer));
                break;
            case PR_LOG_FORMAT_HUMAN:
            default:
                written = write_entry_human(entry, buffer, sizeof(buffer));
                break;
        }

        if (written > 0) {
            fwrite(buffer, 1, written, file);
            bytes += written;

            if (first_ts == 0) first_ts = entry->timestamp_ns;
            last_ts = entry->timestamp_ns;
            exported++;
        }
    }

    PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

    /* Write footer */
    if (format == PR_LOG_FORMAT_JSON) {
        fprintf(file, "\n]}\n");
        bytes += 4;
    }

    /* Update statistics */
    PR_LOG_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.exports_completed++;
    bridge->stats.entries_exported += exported;
    bridge->stats.export_bytes_written += bytes;
    bridge->stats.last_export_time_ns = get_time_ns();
    PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

    if (result) {
        result->error = PR_LOG_SUCCESS;
        result->entries_exported = exported;
        result->bytes_written = bytes;
        result->first_timestamp_ns = first_ts;
        result->last_timestamp_ns = last_ts;
    }

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_to_buffer(
    pr_logging_bridge_t bridge,
    char* buffer,
    size_t buffer_size,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    size_t* bytes_written
) {
    if (!bridge || !buffer || !bytes_written) return PR_LOG_ERROR_NULL_POINTER;
    if (format >= PR_LOG_FORMAT_COUNT) return PR_LOG_ERROR_INVALID_FORMAT;

    pr_log_filter_t flt = filter ? *filter : pr_log_filter_default();

    size_t offset = 0;
    size_t exported = 0;
    char entry_buf[NIMCP_LOG_BUFFER_SIZE];

    /* Write header */
    if (format == PR_LOG_FORMAT_JSON && offset + 14 < buffer_size) {
        memcpy(buffer + offset, "{\"entries\":[\n", 13);
        offset += 13;
    }

    PR_LOG_MUTEX_LOCK(bridge->buffer_mutex);

    size_t read_idx = 0;
    if (bridge->count >= bridge->capacity) {
        read_idx = bridge->write_idx;
    }

    for (size_t i = 0; i < bridge->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->count > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)bridge->count);
        }

        pr_log_entry_t* entry = &bridge->entries[read_idx];
        read_idx = (read_idx + 1) % bridge->capacity;

        if (!entry_matches_filter(entry, &flt)) continue;
        if (flt.max_entries > 0 && exported >= flt.max_entries) break;

        size_t written = 0;
        bool first = (exported == 0);

        switch (format) {
            case PR_LOG_FORMAT_JSON:
                written = write_entry_json(entry, entry_buf, sizeof(entry_buf), first);
                break;
            case PR_LOG_FORMAT_CSV:
                written = write_entry_csv(entry, entry_buf, sizeof(entry_buf));
                break;
            default:
                written = write_entry_human(entry, entry_buf, sizeof(entry_buf));
                break;
        }

        if (written > 0 && offset + written < buffer_size) {
            memcpy(buffer + offset, entry_buf, written);
            offset += written;
            exported++;
        } else {
            break;  /* Buffer full */
        }
    }

    PR_LOG_MUTEX_UNLOCK(bridge->buffer_mutex);

    /* Write footer */
    if (format == PR_LOG_FORMAT_JSON && offset + 4 < buffer_size) {
        memcpy(buffer + offset, "\n]}\n", 4);
        offset += 4;
    }

    buffer[offset] = '\0';
    *bytes_written = offset;

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT size_t pr_logging_bridge_estimate_export_size(
    const pr_logging_bridge_t bridge,
    pr_log_format_t format,
    const pr_log_filter_t* filter
) {
    if (!bridge) return 0;

    size_t count = pr_logging_bridge_count_entries(bridge, filter);

    size_t per_entry = 0;
    switch (format) {
        case PR_LOG_FORMAT_JSON:
            per_entry = 256;
            break;
        case PR_LOG_FORMAT_CSV:
            per_entry = 200;
            break;
        case PR_LOG_FORMAT_BINARY:
            per_entry = sizeof(pr_log_entry_t);
            break;
        default:
            per_entry = 150;
            break;
    }

    return count * per_entry + 100;  /* Add header/footer overhead */
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_entries(
    const pr_logging_bridge_t bridge,
    const pr_log_filter_t* filter,
    pr_log_entry_t* entries,
    size_t max_entries,
    size_t* count
) {
    if (!bridge || !entries || !count) return PR_LOG_ERROR_NULL_POINTER;

    pr_log_filter_t flt = filter ? *filter : pr_log_filter_default();
    *count = 0;

    PR_LOG_MUTEX_LOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    size_t read_idx = 0;
    if (bridge->count >= bridge->capacity) {
        read_idx = bridge->write_idx;
    }

    for (size_t i = 0; i < bridge->count && *count < max_entries; i++) {
        pr_log_entry_t* entry = &bridge->entries[read_idx];
        read_idx = (read_idx + 1) % bridge->capacity;

        if (!entry_matches_filter(entry, &flt)) continue;
        if (flt.max_entries > 0 && *count >= flt.max_entries) break;

        entries[*count] = *entry;
        (*count)++;
    }

    PR_LOG_MUTEX_UNLOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_recent(
    const pr_logging_bridge_t bridge,
    pr_log_entry_t* entries,
    size_t max_entries,
    size_t* count
) {
    if (!bridge || !entries || !count) return PR_LOG_ERROR_NULL_POINTER;

    *count = 0;

    PR_LOG_MUTEX_LOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    size_t to_copy = bridge->count < max_entries ? bridge->count : max_entries;

    /* Read most recent entries (backwards from write pointer) */
    for (size_t i = 0; i < to_copy; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_copy > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)to_copy);
        }

        size_t idx = (bridge->write_idx + bridge->capacity - to_copy + i)
                     % bridge->capacity;
        entries[i] = bridge->entries[idx];
    }
    *count = to_copy;

    PR_LOG_MUTEX_UNLOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT size_t pr_logging_bridge_count_entries(
    const pr_logging_bridge_t bridge,
    const pr_log_filter_t* filter
) {
    if (!bridge) return 0;

    if (!filter) {
        PR_LOG_MUTEX_LOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);
        size_t count = bridge->count;
        PR_LOG_MUTEX_UNLOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);
        return count;
    }

    size_t count = 0;

    PR_LOG_MUTEX_LOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    size_t read_idx = 0;
    if (bridge->count >= bridge->capacity) {
        read_idx = bridge->write_idx;
    }

    for (size_t i = 0; i < bridge->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->count > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)bridge->count);
        }

        pr_log_entry_t* entry = &bridge->entries[read_idx];
        read_idx = (read_idx + 1) % bridge->capacity;

        if (entry_matches_filter(entry, filter)) {
            count++;
            if (filter->max_entries > 0 && count >= filter->max_entries) break;
        }
    }

    PR_LOG_MUTEX_UNLOCK(((pr_logging_bridge_t)bridge)->buffer_mutex);

    return count;
}

NIMCP_EXPORT size_t pr_logging_bridge_get_entry_count(
    const pr_logging_bridge_t bridge
) {
    return pr_logging_bridge_count_entries(bridge, NULL);
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_stats(
    const pr_logging_bridge_t bridge,
    pr_logging_stats_t* stats
) {
    if (!bridge || !stats) return PR_LOG_ERROR_NULL_POINTER;

    PR_LOG_MUTEX_LOCK(((pr_logging_bridge_t)bridge)->stats_mutex);
    *stats = bridge->stats;
    PR_LOG_MUTEX_UNLOCK(((pr_logging_bridge_t)bridge)->stats_mutex);

    return PR_LOG_SUCCESS;
}

NIMCP_EXPORT pr_log_error_t pr_logging_bridge_reset_stats(
    pr_logging_bridge_t bridge
) {
    if (!bridge) return PR_LOG_ERROR_NULL_POINTER;

    PR_LOG_MUTEX_LOCK(bridge->stats_mutex);

    size_t cap = bridge->stats.buffer_capacity;
    size_t size = bridge->stats.current_size;
    float util = bridge->stats.buffer_utilization;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.buffer_capacity = cap;
    bridge->stats.current_size = size;
    bridge->stats.buffer_utilization = util;

    PR_LOG_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_LOG_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_log_error_string(pr_log_error_t error) {
    switch (error) {
        case PR_LOG_SUCCESS: return "Success";
        case PR_LOG_ERROR_NULL_POINTER: return "Null pointer";
        case PR_LOG_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_LOG_ERROR_NOT_INITIALIZED: return "Bridge not initialized";
        case PR_LOG_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_LOG_ERROR_BUFFER_FULL: return "Buffer full";
        case PR_LOG_ERROR_IO: return "I/O error";
        case PR_LOG_ERROR_INVALID_FORMAT: return "Invalid format";
        case PR_LOG_ERROR_FILTER_MISMATCH: return "No entries match filter";
        default: return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_log_level_name(pr_log_level_t level) {
    switch (level) {
        case PR_LOG_LEVEL_TRACE: return "TRACE";
        case PR_LOG_LEVEL_DEBUG: return "DEBUG";
        case PR_LOG_LEVEL_INFO: return "INFO";
        case PR_LOG_LEVEL_WARN: return "WARN";
        case PR_LOG_LEVEL_ERROR: return "ERROR";
        case PR_LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* pr_log_category_name(pr_log_category_t category) {
    switch (category) {
        case PR_LOG_CAT_ENCODE: return "ENCODE";
        case PR_LOG_CAT_RETRIEVE: return "RETRIEVE";
        case PR_LOG_CAT_CONSOLIDATE: return "CONSOLIDATE";
        case PR_LOG_CAT_PROMOTE: return "PROMOTE";
        case PR_LOG_CAT_DEMOTE: return "DEMOTE";
        case PR_LOG_CAT_DECAY: return "DECAY";
        case PR_LOG_CAT_ENTANGLE: return "ENTANGLE";
        case PR_LOG_CAT_RESONANCE: return "RESONANCE";
        case PR_LOG_CAT_STATE: return "STATE";
        case PR_LOG_CAT_CREATE: return "CREATE";
        case PR_LOG_CAT_DESTROY: return "DESTROY";
        case PR_LOG_CAT_CLONE: return "CLONE";
        case PR_LOG_CAT_SYNC: return "SYNC";
        case PR_LOG_CAT_ERROR: return "ERROR";
        case PR_LOG_CAT_PERFORMANCE: return "PERF";
        case PR_LOG_CAT_SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* pr_log_format_name(pr_log_format_t format) {
    switch (format) {
        case PR_LOG_FORMAT_JSON: return "JSON";
        case PR_LOG_FORMAT_CSV: return "CSV";
        case PR_LOG_FORMAT_BINARY: return "Binary";
        case PR_LOG_FORMAT_HUMAN: return "Human";
        default: return "Unknown";
    }
}

NIMCP_EXPORT char* pr_log_entry_to_string(
    const pr_log_entry_t* entry,
    char* buffer,
    size_t buffer_size
) {
    if (!entry || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_log_entry_to_string: required parameter is NULL (entry, buffer)");
        return NULL;
    }

    char ts_buf[32];
    format_timestamp(entry->timestamp_ns, ts_buf, sizeof(ts_buf));

    snprintf(buffer, buffer_size,
             "[%s] %5s %-11s mem=%-6lu %s",
             ts_buf,
             pr_log_level_name(entry->level),
             pr_log_category_name(entry->category),
             (unsigned long)entry->memory_id,
             entry->message);

    return buffer;
}

NIMCP_EXPORT void pr_log_entry_print(const pr_log_entry_t* entry) {
    if (!entry) {
        printf("pr_log_entry: NULL\n");
        return;
    }

    char buffer[NIMCP_ERROR_BUFFER_LARGE];
    pr_log_entry_to_string(entry, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

NIMCP_EXPORT void pr_logging_bridge_print_summary(const pr_logging_bridge_t bridge) {
    if (!bridge) {
        printf("pr_logging_bridge: NULL\n");
        return;
    }

    pr_logging_stats_t stats;
    pr_logging_bridge_get_stats(bridge, &stats);

    printf("=== Logging Bridge Summary ===\n");
    printf("\nBuffer Status:\n");
    printf("  Capacity: %zu entries\n", stats.buffer_capacity);
    printf("  Current: %zu entries (%.1f%% full)\n",
           stats.current_size, stats.buffer_utilization * 100.0f);
    printf("  Overwrites: %lu\n", (unsigned long)stats.overwrites);
    printf("  Dropped: %lu\n", (unsigned long)stats.entries_dropped);

    printf("\nEntry Statistics:\n");
    printf("  Total: %lu\n", (unsigned long)stats.total_entries);
    printf("  Duplicates filtered: %lu\n", (unsigned long)stats.duplicates_filtered);

    printf("\nBy Level:\n");
    for (int i = 0; i < PR_LOG_LEVEL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_LOG_LEVEL_COUNT > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)PR_LOG_LEVEL_COUNT);
        }

        if (stats.entries_by_level[i] > 0) {
            printf("  %s: %lu\n", pr_log_level_name((pr_log_level_t)i),
                   (unsigned long)stats.entries_by_level[i]);
        }
    }

    printf("\nBy Category:\n");
    for (int i = 0; i < PR_LOG_CAT_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_LOG_CAT_COUNT > 256) {
            pr_logging_bridge_heartbeat("pr_logging_b_loop",
                             (float)(i + 1) / (float)PR_LOG_CAT_COUNT);
        }

        if (stats.entries_by_category[i] > 0) {
            printf("  %s: %lu\n", pr_log_category_name((pr_log_category_t)i),
                   (unsigned long)stats.entries_by_category[i]);
        }
    }

    printf("\nExport Statistics:\n");
    printf("  Exports: %lu\n", (unsigned long)stats.exports_completed);
    printf("  Entries exported: %lu\n", (unsigned long)stats.entries_exported);
    printf("  Bytes written: %lu\n", (unsigned long)stats.export_bytes_written);

    printf("\nPerformance:\n");
    printf("  Avg log time: %.1f ns\n", stats.avg_log_time_ns);
    printf("  Max log time: %.1f ns\n", stats.max_log_time_ns);
    printf("  File writes: %lu\n", (unsigned long)stats.file_writes);
    printf("  File flushes: %lu\n", (unsigned long)stats.file_flushes);

    printf("==============================\n");
}

NIMCP_EXPORT uint64_t pr_log_current_time_ns(void) {
    return get_time_ns();
}

NIMCP_EXPORT uint64_t pr_log_current_time_ms(void) {
    return get_time_ms();
}

NIMCP_EXPORT bool pr_logging_bridge_validate(const pr_logging_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_logging_bridge_validate: bridge is NULL");
        return false;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_logging_bridge_validate: bridge->initialized is NULL");
        return false;
    }
    if (!bridge->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_logging_bridge_validate: bridge->entries is NULL");
        return false;
    }
    if (bridge->capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_logging_bridge_validate: bridge->capacity is zero");
        return false;
    }
    if (bridge->count > bridge->capacity) {
        return false;
    }
    if (bridge->write_idx >= bridge->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_logging_bridge_validate: capacity exceeded");
        return false;
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_logging_bridge_set_instance_health_agent(
    pr_logging_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_logging_bridge_training_begin(pr_logging_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_logging_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_logging_bridge_heartbeat_instance(bridge->health_agent, "pr_logging_bridge_training_begin", 0.0f);
    return 0;
}

int pr_logging_bridge_training_end(pr_logging_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_logging_bridge_training_end: NULL argument");
        return -1;
    }
    pr_logging_bridge_heartbeat_instance(bridge->health_agent, "pr_logging_bridge_training_end", 1.0f);
    return 0;
}

int pr_logging_bridge_training_step(pr_logging_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_logging_bridge_training_step: NULL argument");
        return -1;
    }
    pr_logging_bridge_heartbeat_instance(bridge->health_agent, "pr_logging_bridge_training_step", progress);
    return 0;
}
