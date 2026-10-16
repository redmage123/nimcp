/**
 * @file nimcp_security_logging_bridge.c
 * @brief Security-Logging Bridge Implementation for Comprehensive Audit Trails
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional bridge connecting security systems with logging infrastructure
 * WHY:  Enable comprehensive audit trails, threat pattern detection from logs,
 *       and tamper-proof security event recording
 * HOW:  Security events flow to logging; log analysis feeds back threat intelligence
 *
 * BIOLOGICAL BASIS:
 * Modeled on the immune system's memory and logging mechanisms:
 * - Dendritic cells (security) present antigens to memory systems (logging)
 * - Memory B cells retain patterns of past threats (log analysis)
 * - Cytokine signaling (real-time alerts) coordinates immediate responses
 * - Complement system (audit trail) marks and tracks all foreign entities
 */

#include "security/logging/nimcp_security_logging_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_logging_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#define LOG_MODULE "SECURITY_LOGGING_BRIDGE"


/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define SECURITY_LOGGING_MODULE_NAME "security_logging_bridge"

/** @brief Maximum detected patterns to track */
#define MAX_DETECTED_PATTERNS 64

/** @brief Pattern analysis threshold (occurrences to detect pattern) */
#define PATTERN_THRESHOLD_MIN_OCCURRENCES 3

/** @brief Time correlation window for pattern detection (ms) */
#define PATTERN_TIME_WINDOW_MS 60000

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Stream callback registration
 */
typedef struct {
    security_log_stream_callback_t callback;
    void* user_data;
    uint32_t filter;
    bool active;
} stream_callback_entry_t;

/**
 * @brief Ring buffer for log entries
 */
typedef struct {
    security_log_entry_t* entries;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} log_entry_ring_buffer_t;

/**
 * @brief Internal bridge state (extends the public struct)
 */
typedef struct {
    /* Ring buffer for entries */
    log_entry_ring_buffer_t ring_buffer;

    /* Stream callbacks */
    stream_callback_entry_t stream_callbacks[SECURITY_LOG_MAX_STREAM_CALLBACKS];

    /* Pattern callback */
    security_pattern_callback_t pattern_callback;
    void* pattern_callback_user_data;

    /* Detected patterns */
    security_threat_pattern_t* detected_patterns;
    size_t pattern_count;
    size_t pattern_capacity;

    /* Sequence tracking */
    uint32_t next_sequence;
    uint64_t next_entry_id;
    uint64_t next_pattern_id;

    /* File handle */
    FILE* log_file;

    /* Performance tracking */
    uint64_t total_log_time_ns;
    uint64_t max_log_time_ns;
    uint64_t log_count;
} security_logging_internal_t;

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
uint64_t security_log_current_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t current_time_ms(void) {
    return security_log_current_time_ns() / 1000000ULL;
}

/**
 * @brief Get current thread ID
 */
static uint32_t get_thread_id(void) {
#ifdef _WIN32
    return (uint32_t)GetCurrentThreadId();
#else
    return (uint32_t)pthread_self();
#endif
}

/**
 * @brief Get internal state from bridge
 */
static security_logging_internal_t* get_internal(security_logging_bridge_t* bridge) {
    /* Internal data is stored after the main struct in contiguous memory */
    return (security_logging_internal_t*)((uint8_t*)bridge + sizeof(security_logging_bridge_t));
}

/**
 * @brief Get const internal state from bridge
 */
static const security_logging_internal_t* get_internal_const(const security_logging_bridge_t* bridge) {
    return (const security_logging_internal_t*)((const uint8_t*)bridge + sizeof(security_logging_bridge_t));
}

/**
 * @brief Initialize ring buffer
 */
static int ring_buffer_init(log_entry_ring_buffer_t* rb, size_t capacity) {
    if (!rb || capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ring_buffer_init: rb is NULL");
        return -1;
    }

    rb->entries = (security_log_entry_t*)nimcp_malloc(
        sizeof(security_log_entry_t) * capacity);
    if (!rb->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ring_buffer_init: rb->entries is NULL");
        return -1;
    }

    memset(rb->entries, 0, sizeof(security_log_entry_t) * capacity);
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;

    return 0;
}

/**
 * @brief Cleanup ring buffer
 */
static void ring_buffer_cleanup(log_entry_ring_buffer_t* rb) {
    if (rb && rb->entries) {
        nimcp_free(rb->entries);
        rb->entries = NULL;
    }
}

/**
 * @brief Push entry to ring buffer
 */
static int ring_buffer_push(log_entry_ring_buffer_t* rb,
                            const security_log_entry_t* entry,
                            bool overwrite_on_full) {
    if (!rb || !entry || !rb->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ring_buffer_push: required parameter is NULL (rb, entry, rb->entries)");
        return -1;
    }

    if (rb->count >= rb->capacity) {
        if (!overwrite_on_full) {
            return NIMCP_ERROR_OPERATION_FAILED;
        }
        /* Overwrite oldest: advance tail */
        rb->tail = (rb->tail + 1) % rb->capacity;
        rb->count--;
    }

    rb->entries[rb->head] = *entry;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;

    return 0;
}

/**
 * @brief Get entry at index (0 = oldest)
 */
static security_log_entry_t* ring_buffer_get(log_entry_ring_buffer_t* rb, size_t index) {
    if (!rb || !rb->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ring_buffer_get: required parameter is NULL (rb, rb->entries)");
        return NULL;
    }
    if (index >= rb->count) {
        return NULL;
    }
    size_t actual_index = (rb->tail + index) % rb->capacity;
    return &rb->entries[actual_index];
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_log_category_name(security_log_category_t category) {
    static const char* names[] = {
        "THREAT",
        "ACCESS",
        "POLICY",
        "AUDIT",
        "BBB",
        "ANOMALY",
        "CRYPTO",
        "RATE_LIMIT"
    };
    if (category < SECURITY_LOG_CAT_COUNT) {
        return names[category];
    }
    return "UNKNOWN";
}

const char* security_log_severity_name(security_log_severity_t severity) {
    static const char* names[] = {
        "DEBUG",
        "INFO",
        "NOTICE",
        "WARNING",
        "ERROR",
        "CRITICAL",
        "ALERT",
        "EMERGENCY"
    };
    if (severity < SECURITY_LOG_SEV_COUNT) {
        return names[severity];
    }
    return "UNKNOWN";
}

const char* security_log_action_name(security_log_action_t action) {
    static const char* names[] = {
        "NONE",
        "ALLOW",
        "DENY",
        "BLOCK",
        "QUARANTINE",
        "ALERT",
        "TERMINATE",
        "LOCKDOWN"
    };
    if (action < SECURITY_LOG_ACTION_COUNT) {
        return names[action];
    }
    return "UNKNOWN";
}

const char* security_pattern_type_name(security_pattern_type_t type) {
    static const char* names[] = {
        "NONE",
        "SCAN",
        "BRUTE_FORCE",
        "DOS",
        "INJECTION",
        "EXFILTRATION",
        "LATERAL_MOVE",
        "PRIVILEGE_ESC",
        "CUSTOM"
    };
    if (type < SECURITY_PATTERN_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

const char* security_log_format_name(security_log_format_t format) {
    static const char* names[] = {
        "TEXT",
        "JSON",
        "SYSLOG",
        "CEF",
        "BINARY"
    };
    if (format < SECURITY_LOG_FORMAT_COUNT) {
        return names[format];
    }
    return "UNKNOWN";
}

security_log_severity_t security_threat_to_severity(nimcp_threat_level_t threat) {
    switch (threat) {
        case NIMCP_THREAT_NONE:     return SECURITY_LOG_SEV_INFO;
        case NIMCP_THREAT_LOW:      return SECURITY_LOG_SEV_NOTICE;
        case NIMCP_THREAT_MEDIUM:   return SECURITY_LOG_SEV_WARNING;
        case NIMCP_THREAT_HIGH:     return SECURITY_LOG_SEV_ERROR;
        case NIMCP_THREAT_CRITICAL: return SECURITY_LOG_SEV_CRITICAL;
        default:                    return SECURITY_LOG_SEV_WARNING;
    }
}

security_log_severity_t security_bbb_to_log_severity(bbb_severity_t bbb_severity) {
    switch (bbb_severity) {
        case BBB_SEVERITY_NONE:     return SECURITY_LOG_SEV_INFO;
        case BBB_SEVERITY_LOW:      return SECURITY_LOG_SEV_NOTICE;
        case BBB_SEVERITY_MEDIUM:   return SECURITY_LOG_SEV_WARNING;
        case BBB_SEVERITY_HIGH:     return SECURITY_LOG_SEV_ERROR;
        case BBB_SEVERITY_CRITICAL: return SECURITY_LOG_SEV_CRITICAL;
        default:                    return SECURITY_LOG_SEV_WARNING;
    }
}

int security_log_entry_init(
    security_log_entry_t* entry,
    security_log_category_t category,
    security_log_severity_t severity,
    const char* message
) {
    if (!entry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(entry, 0, sizeof(security_log_entry_t));

    entry->timestamp_ns = security_log_current_time_ns();
    entry->category = category;
    entry->severity = severity;
    entry->action = SECURITY_LOG_ACTION_NONE;
    entry->threat_level = NIMCP_THREAT_NONE;
    entry->bbb_threat = BBB_THREAT_NONE;
    entry->pattern = SECURITY_PATTERN_NONE;
    entry->source_thread_id = get_thread_id();
    entry->confidence_score = 1.0f;

    if (message) {
        strncpy(entry->message, message, SECURITY_LOG_MAX_MESSAGE_LEN - 1);
        entry->message[SECURITY_LOG_MAX_MESSAGE_LEN - 1] = '\0';
    }

    return 0;
}

void security_log_entry_print(const security_log_entry_t* entry) {
    if (!entry) return;

    printf("[%s][%s] %s\n",
           security_log_severity_name(entry->severity),
           security_log_category_name(entry->category),
           entry->message);

    if (entry->details[0]) {
        printf("  Details: %s\n", entry->details);
    }
    if (entry->source_module[0]) {
        printf("  Source: %s\n", entry->source_module);
    }
}

void security_logging_bridge_print_summary(const security_logging_bridge_t* bridge) {
    if (!bridge) return;

    printf("\n=== Security Logging Bridge Summary ===\n");
    printf("State: %s\n", bridge->state.active ? "Active" : "Inactive");
    printf("Total Entries: %lu\n", (unsigned long)bridge->stats.total_entries);
    printf("Entries Dropped: %lu\n", (unsigned long)bridge->stats.entries_dropped);
    printf("Patterns Detected: %lu\n", (unsigned long)bridge->stats.patterns_detected);
    printf("Buffer Utilization: %.1f%%\n", bridge->stats.buffer_utilization * 100.0f);

    printf("\nEntries by Category:\n");
    for (int i = 0; i < SECURITY_LOG_CAT_COUNT; i++) {
        printf("  %s: %lu\n",
               security_log_category_name((security_log_category_t)i),
               (unsigned long)bridge->stats.entries_by_category[i]);
    }

    printf("\nEntries by Severity:\n");
    for (int i = 0; i < SECURITY_LOG_SEV_COUNT; i++) {
        printf("  %s: %lu\n",
               security_log_severity_name((security_log_severity_t)i),
               (unsigned long)bridge->stats.entries_by_severity[i]);
    }
    printf("=====================================\n\n");
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int security_logging_default_config(security_logging_bridge_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(config, 0, sizeof(security_logging_bridge_config_t));

    /* Buffer settings */
    config->buffer_capacity = SECURITY_LOG_DEFAULT_BUFFER_SIZE;
    config->overwrite_on_full = true;

    /* Filtering */
    config->min_severity = SECURITY_LOG_SEV_INFO;
    config->enabled_categories = SECURITY_LOG_CAT_ALL;

    /* Output settings */
    config->log_to_console = false;
    config->log_to_file = false;
    config->format = SECURITY_LOG_FORMAT_JSON;

    /* Integration */
    config->enable_encrypted_audit = false;
    config->enable_nimcp_logging = true;

    /* Rotation */
    config->rotation.enabled = true;
    config->rotation.max_file_size_mb = 100;
    config->rotation.max_rotated_files = 10;
    config->rotation.rotation_interval_hours = 24;
    config->rotation.compress_rotated = false;

    /* Retention */
    config->retention.retention_days = SECURITY_LOG_DEFAULT_RETENTION_DAYS;
    config->retention.archive_before_delete = true;
    config->retention.min_archive_severity = SECURITY_LOG_SEV_WARNING;

    /* Pattern analysis */
    config->pattern_analysis.enabled = true;
    config->pattern_analysis.analysis_window_size = SECURITY_LOG_DEFAULT_ANALYSIS_WINDOW;
    config->pattern_analysis.analysis_interval_ms = 5000;
    config->pattern_analysis.min_pattern_confidence = 0.7f;
    config->pattern_analysis.min_occurrences = PATTERN_THRESHOLD_MIN_OCCURRENCES;
    config->pattern_analysis.feed_to_anomaly_detector = true;

    /* Bio-async */
    config->enable_bio_async = false;

    /* Performance */
    config->enable_timestamps = true;
    config->enable_metrics = true;

    return 0;
}

security_logging_bridge_t* security_logging_bridge_create(
    const security_logging_bridge_config_t* config
) {
    /* Use defaults if no config provided */
    security_logging_bridge_config_t default_config;
    if (!config) {
        security_logging_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge + internal state in one block */
    size_t total_size = sizeof(security_logging_bridge_t) +
                        sizeof(security_logging_internal_t);
    security_logging_bridge_t* bridge = (security_logging_bridge_t*)nimcp_malloc(total_size);
    if (!bridge) {
        LOG_MODULE_ERROR(SECURITY_LOGGING_MODULE_NAME, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, total_size);

    security_logging_internal_t* internal = get_internal(bridge);

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_LOGGING,
                         SECURITY_LOGGING_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_bridge_create: operation failed");
        return NULL;
    }

    /* Copy configuration */
    bridge->config = *config;

    /* Initialize ring buffer */
    if (ring_buffer_init(&internal->ring_buffer, config->buffer_capacity) != 0) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        LOG_MODULE_ERROR(SECURITY_LOGGING_MODULE_NAME, "Failed to allocate ring buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "security_logging_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate pattern storage */
    internal->pattern_capacity = MAX_DETECTED_PATTERNS;
    internal->detected_patterns = (security_threat_pattern_t*)nimcp_malloc(
        sizeof(security_threat_pattern_t) * internal->pattern_capacity);
    if (!internal->detected_patterns) {
        ring_buffer_cleanup(&internal->ring_buffer);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        LOG_MODULE_ERROR(SECURITY_LOGGING_MODULE_NAME, "Failed to allocate pattern storage");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_logging_bridge_create: internal->detected_patterns is NULL");
        return NULL;
    }
    memset(internal->detected_patterns, 0,
           sizeof(security_threat_pattern_t) * internal->pattern_capacity);

    /* Initialize sequence tracking */
    internal->next_sequence = 1;
    internal->next_entry_id = 1;
    internal->next_pattern_id = 1;

    /* Open log file if configured */
    if (config->log_to_file && config->log_file_path[0]) {
        internal->log_file = fopen(config->log_file_path, "a");
        if (!internal->log_file) {
            LOG_MODULE_WARN(SECURITY_LOGGING_MODULE_NAME,
                           "Failed to open log file: %s", config->log_file_path);
        }
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.logging_enabled = true;
    bridge->state.pattern_analysis_running = config->pattern_analysis.enabled;

    /* Initialize stats */
    bridge->stats.buffer_capacity = config->buffer_capacity;

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME,
                   "Security logging bridge created (capacity: %zu)",
                   config->buffer_capacity);

    return bridge;
}

void security_logging_bridge_destroy(security_logging_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "security_logging");

    /* Flush pending entries */
    security_logging_flush(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    /* Close log file */
    if (internal->log_file) {
        fclose(internal->log_file);
        internal->log_file = NULL;
    }

    /* Free pattern storage */
    if (internal->detected_patterns) {
        nimcp_free(internal->detected_patterns);
    }

    /* Cleanup ring buffer */
    ring_buffer_cleanup(&internal->ring_buffer);

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME, "Bridge destroyed");
}

int security_logging_bridge_reset(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    /* Reset ring buffer */
    internal->ring_buffer.head = 0;
    internal->ring_buffer.tail = 0;
    internal->ring_buffer.count = 0;

    /* Reset pattern storage */
    internal->pattern_count = 0;
    memset(internal->detected_patterns, 0,
           sizeof(security_threat_pattern_t) * internal->pattern_capacity);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.buffer_capacity = bridge->config.buffer_capacity;

    /* Reset effects */
    memset(&bridge->security_effects, 0, sizeof(bridge->security_effects));
    memset(&bridge->logging_effects, 0, sizeof(bridge->logging_effects));

    /* Reset state (keep active) */
    bridge->state.last_entry_time_ns = 0;
    bridge->state.last_analysis_time_ns = 0;
    bridge->state.pending_entries = 0;

    /* Reset base (already holding BRIDGE_LOCK, use unlocked variant) */
    bridge_base_reset_unlocked(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME, "Bridge reset");
    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int security_logging_connect_bbb(
    security_logging_bridge_t* bridge,
    bbb_system_t bbb
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);
    bridge->bbb_system = bbb;
    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME, "Connected BBB system");
    return 0;
}

int security_logging_connect_anomaly_detector(
    security_logging_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);
    bridge->anomaly_detector = detector;
    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME, "Connected anomaly detector");
    return 0;
}

int security_logging_connect_rate_limiter(
    security_logging_bridge_t* bridge,
    nimcp_rate_limiter_t limiter
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);
    bridge->rate_limiter = limiter;
    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME, "Connected rate limiter");
    return 0;
}

int security_logging_connect_encrypted_audit(
    security_logging_bridge_t* bridge,
    nimcp_encrypted_audit_t audit
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);
    bridge->config.encrypted_audit = audit;
    bridge->config.enable_encrypted_audit = (audit != NULL);
    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME, "Connected encrypted audit");
    return 0;
}

int security_logging_connect_nimcp_logger(
    security_logging_bridge_t* bridge,
    nimcp_logger_t logger
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);
    bridge->config.nimcp_logger = logger;
    bridge->config.enable_nimcp_logging = (logger != NULL);
    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME, "Connected NIMCP logger");
    return 0;
}

int security_logging_disconnect_all(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    bridge->bbb_system = NULL;
    bridge->anomaly_detector = NULL;
    bridge->rate_limiter = NULL;
    bridge->config.encrypted_audit = NULL;
    bridge->config.nimcp_logger = NULL;
    bridge->config.enable_encrypted_audit = false;
    bridge->config.enable_nimcp_logging = false;

    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME, "All systems disconnected");
    return 0;
}

bool security_logging_is_connected(const security_logging_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->config.enable_nimcp_logging ||
           bridge->config.enable_encrypted_audit ||
           bridge->config.log_to_file ||
           bridge->config.log_to_console;
}

/* ============================================================================
 * Internal Logging Helper
 * ============================================================================ */

/**
 * @brief Internal function to process and store a log entry
 */
static int log_entry_internal(
    security_logging_bridge_t* bridge,
    security_log_entry_t* entry
) {
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "log_entry_internal: required parameter is NULL (bridge, entry)");
        return -1;
    }

    security_logging_internal_t* internal = get_internal(bridge);
    uint64_t start_time = security_log_current_time_ns();

    /* Check severity filter */
    if (entry->severity < bridge->config.min_severity) {
        bridge->stats.entries_filtered++;
        return 0;
    }

    /* Check category filter */
    if (!(bridge->config.enabled_categories & SECURITY_LOG_CAT_MASK(entry->category))) {
        bridge->stats.entries_filtered++;
        return 0;
    }

    /* Assign entry ID and sequence */
    entry->entry_id = internal->next_entry_id++;
    entry->sequence_number = internal->next_sequence++;

    /* Track if buffer will overwrite */
    bool will_overwrite = (internal->ring_buffer.count >= internal->ring_buffer.capacity)
                          && bridge->config.overwrite_on_full;

    /* Store in ring buffer */
    int result = ring_buffer_push(&internal->ring_buffer, entry,
                                  bridge->config.overwrite_on_full);
    if (result != 0) {
        bridge->stats.entries_dropped++;
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (will_overwrite) {
        bridge->stats.buffer_overwrites++;
    }

    /* Update statistics */
    bridge->stats.total_entries++;
    bridge->stats.entries_by_category[entry->category]++;
    bridge->stats.entries_by_severity[entry->severity]++;
    bridge->state.last_entry_time_ns = entry->timestamp_ns;

    /* Track maximum severity in security effects */
    if (entry->severity > bridge->security_effects.max_severity) {
        bridge->security_effects.max_severity = entry->severity;
    }

    /* Update buffer utilization */
    bridge->stats.current_buffer_size = internal->ring_buffer.count;
    bridge->stats.buffer_utilization =
        (float)internal->ring_buffer.count / (float)internal->ring_buffer.capacity;

    /* Invoke stream callbacks */
    for (int i = 0; i < SECURITY_LOG_MAX_STREAM_CALLBACKS; i++) {
        stream_callback_entry_t* cb = &internal->stream_callbacks[i];
        if (cb->active && cb->callback) {
            /* Check filter */
            if (cb->filter == 0 || (cb->filter & SECURITY_LOG_CAT_MASK(entry->category))) {
                bool cont = cb->callback(entry, cb->user_data);
                if (!cont) {
                    cb->active = false;
                }
                bridge->stats.stream_callbacks_invoked++;
            }
        }
    }

    /* Write to NIMCP logger */
    if (bridge->config.enable_nimcp_logging && bridge->config.nimcp_logger) {
        log_level_t nimcp_level = LOG_LEVEL_INFO;
        switch (entry->severity) {
            case SECURITY_LOG_SEV_DEBUG:     nimcp_level = LOG_LEVEL_DEBUG; break;
            case SECURITY_LOG_SEV_INFO:      nimcp_level = LOG_LEVEL_INFO; break;
            case SECURITY_LOG_SEV_NOTICE:    nimcp_level = LOG_LEVEL_INFO; break;
            case SECURITY_LOG_SEV_WARNING:   nimcp_level = LOG_LEVEL_WARN; break;
            case SECURITY_LOG_SEV_ERROR:     nimcp_level = LOG_LEVEL_ERROR; break;
            case SECURITY_LOG_SEV_CRITICAL:  nimcp_level = LOG_LEVEL_FATAL; break;
            case SECURITY_LOG_SEV_ALERT:     nimcp_level = LOG_LEVEL_FATAL; break;
            case SECURITY_LOG_SEV_EMERGENCY: nimcp_level = LOG_LEVEL_FATAL; break;
            default: nimcp_level = LOG_LEVEL_INFO;
        }
        nimcp_log_write(bridge->config.nimcp_logger, nimcp_level,
                       SECURITY_LOGGING_MODULE_NAME, NULL, 0,
                       "[%s] %s", security_log_category_name(entry->category),
                       entry->message);
        bridge->stats.nimcp_logger_writes++;
    }

    /* Write to encrypted audit */
    if (bridge->config.enable_encrypted_audit && bridge->config.encrypted_audit) {
        nimcp_audit_category_t audit_cat = NIMCP_AUDIT_THREAT;
        switch (entry->category) {
            case SECURITY_LOG_CAT_THREAT:     audit_cat = NIMCP_AUDIT_THREAT; break;
            case SECURITY_LOG_CAT_ACCESS:     audit_cat = NIMCP_AUDIT_AUTHORIZATION; break;
            case SECURITY_LOG_CAT_POLICY:     audit_cat = NIMCP_AUDIT_CONFIGURATION; break;
            case SECURITY_LOG_CAT_CRYPTO:     audit_cat = NIMCP_AUDIT_ENCRYPTION; break;
            default:                          audit_cat = NIMCP_AUDIT_SYSTEM;
        }
        nimcp_encrypted_audit_log(bridge->config.encrypted_audit,
                                  (nimcp_audit_severity_t)entry->severity,
                                  audit_cat, entry->message, NULL, 0);
        bridge->stats.encrypted_audit_writes++;
    }

    /* Write to file */
    if (bridge->config.log_to_file && internal->log_file) {
        char json_buf[NIMCP_JSON_BUFFER_SIZE];
        int len = security_logging_entry_to_json(entry, json_buf, sizeof(json_buf));
        if (len > 0) {
            fprintf(internal->log_file, "%s\n", json_buf);
            bridge->stats.file_writes++;
        }
    }

    /* Write to console */
    if (bridge->config.log_to_console) {
        security_log_entry_print(entry);
    }

    /* Update performance metrics */
    uint64_t elapsed = security_log_current_time_ns() - start_time;
    entry->processing_time_ns = elapsed;

    internal->total_log_time_ns += elapsed;
    internal->log_count++;
    if (elapsed > internal->max_log_time_ns) {
        internal->max_log_time_ns = elapsed;
    }
    bridge->stats.avg_log_time_ns = (float)internal->total_log_time_ns /
                                    (float)internal->log_count;
    bridge->stats.max_log_time_ns = (float)internal->max_log_time_ns;

    return 0;
}

/* ============================================================================
 * Core Logging Functions
 * ============================================================================ */

int security_logging_log_entry(
    security_logging_bridge_t* bridge,
    const security_log_entry_t* entry
) {
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_log_entry: required parameter is NULL (bridge, entry)");
        return -1;
    }
    if (!bridge->state.logging_enabled) return 0;

    BRIDGE_LOCK(bridge);

    /* Make a mutable copy */
    security_log_entry_t mutable_entry = *entry;
    int result = log_entry_internal(bridge, &mutable_entry);

    BRIDGE_UNLOCK(bridge);
    return result;
}

int security_logging_log_threat(
    security_logging_bridge_t* bridge,
    nimcp_threat_level_t threat,
    bbb_threat_type_t bbb_threat,
    const char* source,
    const char* message,
    security_log_action_t action
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_log_threat: required parameter is NULL (bridge, message)");
        return -1;
    }
    if (!bridge->state.logging_enabled) return 0;

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_THREAT,
                            security_threat_to_severity(threat), message);

    entry.threat_level = threat;
    entry.bbb_threat = bbb_threat;
    entry.action = action;

    if (source) {
        strncpy(entry.source_module, source, SECURITY_LOG_MAX_SOURCE_LEN - 1);
    }

    BRIDGE_LOCK(bridge);
    bridge->security_effects.threat_events++;
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_access(
    security_logging_bridge_t* bridge,
    bool allowed,
    const char* subject,
    const char* object,
    const char* message
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_ACCESS,
                            allowed ? SECURITY_LOG_SEV_INFO : SECURITY_LOG_SEV_WARNING,
                            message ? message : (allowed ? "Access granted" : "Access denied"));

    entry.action = allowed ? SECURITY_LOG_ACTION_ALLOW : SECURITY_LOG_ACTION_DENY;

    if (subject) {
        snprintf(entry.source_id, SECURITY_LOG_MAX_SOURCE_LEN, "%s", subject);
    }
    if (object) {
        snprintf(entry.details, SECURITY_LOG_MAX_DETAILS_LEN, "Object: %s", object);
    }

    BRIDGE_LOCK(bridge);
    bridge->security_effects.access_events++;
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_policy(
    security_logging_bridge_t* bridge,
    const char* policy_id,
    security_log_action_t action,
    const char* target,
    const char* message
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_POLICY,
                            SECURITY_LOG_SEV_INFO,
                            message ? message : "Policy enforcement");

    entry.action = action;

    if (policy_id) {
        snprintf(entry.source_id, SECURITY_LOG_MAX_SOURCE_LEN, "policy:%s", policy_id);
    }
    if (target) {
        snprintf(entry.details, SECURITY_LOG_MAX_DETAILS_LEN, "Target: %s", target);
    }

    BRIDGE_LOCK(bridge);
    bridge->security_effects.policy_events++;
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_bbb(
    security_logging_bridge_t* bridge,
    bbb_threat_type_t threat,
    bbb_severity_t severity,
    bbb_action_t action,
    const char* details
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    char message[SECURITY_LOG_MAX_MESSAGE_LEN];
    snprintf(message, sizeof(message), "BBB: %s detected",
             bbb_threat_type_name(threat));

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_BBB,
                            security_bbb_to_log_severity(severity), message);

    entry.bbb_threat = threat;
    entry.threat_level = (nimcp_threat_level_t)severity;

    /* Map BBB action to log action */
    switch (action) {
        case BBB_ACTION_ALLOW:      entry.action = SECURITY_LOG_ACTION_ALLOW; break;
        case BBB_ACTION_LOG:        entry.action = SECURITY_LOG_ACTION_ALERT; break;
        case BBB_ACTION_BLOCK:      entry.action = SECURITY_LOG_ACTION_BLOCK; break;
        case BBB_ACTION_QUARANTINE: entry.action = SECURITY_LOG_ACTION_QUARANTINE; break;
        case BBB_ACTION_TERMINATE:  entry.action = SECURITY_LOG_ACTION_TERMINATE; break;
        case BBB_ACTION_LOCKDOWN:   entry.action = SECURITY_LOG_ACTION_LOCKDOWN; break;
        default:                    entry.action = SECURITY_LOG_ACTION_NONE;
    }

    strncpy(entry.source_module, "BBB", SECURITY_LOG_MAX_SOURCE_LEN - 1);

    if (details) {
        strncpy(entry.details, details, SECURITY_LOG_MAX_DETAILS_LEN - 1);
    }

    BRIDGE_LOCK(bridge);
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_anomaly(
    security_logging_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    const void* input,
    const char* message
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    security_log_severity_t severity = SECURITY_LOG_SEV_INFO;
    if (result) {
        if (result->anomaly_score > 0.8f) severity = SECURITY_LOG_SEV_CRITICAL;
        else if (result->anomaly_score > 0.6f) severity = SECURITY_LOG_SEV_WARNING;
        else if (result->anomaly_score > 0.4f) severity = SECURITY_LOG_SEV_NOTICE;
    }

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_ANOMALY, severity,
                            message ? message : "Anomaly detected");

    if (result) {
        entry.anomaly_score = result->anomaly_score;
        entry.confidence_score = result->confidence;
        snprintf(entry.details, SECURITY_LOG_MAX_DETAILS_LEN,
                 "Score: %.3f, Confidence: %.3f, Content: %.3f, Behavior: %.3f",
                 result->anomaly_score, result->confidence,
                 result->content_score, result->behavior_score);
    }

    strncpy(entry.source_module, "AnomalyDetector", SECURITY_LOG_MAX_SOURCE_LEN - 1);

    BRIDGE_LOCK(bridge);
    int res = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return res;
}

int security_logging_log_rate_limit(
    security_logging_bridge_t* bridge,
    const char* client_id,
    bool allowed,
    const nimcp_client_stats_t* stats
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    char message[SECURITY_LOG_MAX_MESSAGE_LEN];
    snprintf(message, sizeof(message), "Rate limit %s for client: %s",
             allowed ? "check passed" : "exceeded", client_id ? client_id : "unknown");

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_RATE_LIMIT,
                            allowed ? SECURITY_LOG_SEV_DEBUG : SECURITY_LOG_SEV_WARNING,
                            message);

    entry.action = allowed ? SECURITY_LOG_ACTION_ALLOW : SECURITY_LOG_ACTION_DENY;

    if (client_id) {
        strncpy(entry.source_id, client_id, SECURITY_LOG_MAX_SOURCE_LEN - 1);
    }

    if (stats) {
        snprintf(entry.details, SECURITY_LOG_MAX_DETAILS_LEN,
                 "Allowed: %lu, Denied: %lu, Violations: %u, Tokens: %u",
                 (unsigned long)stats->requests_allowed,
                 (unsigned long)stats->requests_denied,
                 stats->violations, stats->tokens_available);
    }

    strncpy(entry.source_module, "RateLimiter", SECURITY_LOG_MAX_SOURCE_LEN - 1);

    BRIDGE_LOCK(bridge);
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_crypto(
    security_logging_bridge_t* bridge,
    const char* operation,
    bool success,
    const char* key_id,
    const char* message
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.logging_enabled) return 0;

    char msg[SECURITY_LOG_MAX_MESSAGE_LEN];
    snprintf(msg, sizeof(msg), "Crypto %s: %s",
             operation ? operation : "operation",
             success ? "succeeded" : "failed");

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_CRYPTO,
                            success ? SECURITY_LOG_SEV_INFO : SECURITY_LOG_SEV_ERROR,
                            msg);

    entry.action = success ? SECURITY_LOG_ACTION_NONE : SECURITY_LOG_ACTION_ALERT;

    if (key_id) {
        snprintf(entry.source_id, SECURITY_LOG_MAX_SOURCE_LEN, "key:%s", key_id);
    }
    if (message) {
        strncpy(entry.details, message, SECURITY_LOG_MAX_DETAILS_LEN - 1);
    }

    strncpy(entry.source_module, "Crypto", SECURITY_LOG_MAX_SOURCE_LEN - 1);

    BRIDGE_LOCK(bridge);
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

int security_logging_log_audit(
    security_logging_bridge_t* bridge,
    security_log_severity_t severity,
    const char* source,
    const char* message,
    const char* details
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_log_audit: required parameter is NULL (bridge, message)");
        return -1;
    }
    if (!bridge->state.logging_enabled) return 0;

    security_log_entry_t entry;
    security_log_entry_init(&entry, SECURITY_LOG_CAT_AUDIT, severity, message);

    if (source) {
        strncpy(entry.source_module, source, SECURITY_LOG_MAX_SOURCE_LEN - 1);
    }
    if (details) {
        strncpy(entry.details, details, SECURITY_LOG_MAX_DETAILS_LEN - 1);
    }

    BRIDGE_LOCK(bridge);
    int result = log_entry_internal(bridge, &entry);
    BRIDGE_UNLOCK(bridge);

    return result;
}

/* ============================================================================
 * Streaming Functions
 * ============================================================================ */

int security_logging_register_stream(
    security_logging_bridge_t* bridge,
    security_log_stream_callback_t callback,
    void* user_data,
    uint32_t filter
) {
    if (!bridge || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_register_stream: required parameter is NULL (bridge, callback)");
        return -1;
    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    /* Find free slot */
    for (int i = 0; i < SECURITY_LOG_MAX_STREAM_CALLBACKS; i++) {
        if (!internal->stream_callbacks[i].active) {
            internal->stream_callbacks[i].callback = callback;
            internal->stream_callbacks[i].user_data = user_data;
            internal->stream_callbacks[i].filter = filter;
            internal->stream_callbacks[i].active = true;
            bridge->state.active_stream_callbacks++;

            BRIDGE_UNLOCK(bridge);
            return i;
        }
    }

    BRIDGE_UNLOCK(bridge);
    return -1;  /* No free slots */
}

int security_logging_unregister_stream(
    security_logging_bridge_t* bridge,
    int callback_id
) {
    if (!bridge || callback_id < 0 || callback_id >= SECURITY_LOG_MAX_STREAM_CALLBACKS) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    if (internal->stream_callbacks[callback_id].active) {
        internal->stream_callbacks[callback_id].active = false;
        internal->stream_callbacks[callback_id].callback = NULL;
        internal->stream_callbacks[callback_id].user_data = NULL;
        bridge->state.active_stream_callbacks--;
    }

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_logging_register_pattern_callback(
    security_logging_bridge_t* bridge,
    security_pattern_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "security_logging_register_pattern_callback: callback is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);
    internal->pattern_callback = callback;
    internal->pattern_callback_user_data = user_data;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Pattern Analysis Functions
 * ============================================================================ */

int security_logging_analyze_patterns(security_logging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.pattern_analysis.enabled) return 0;

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);
    uint64_t current_time = current_time_ms();
    int patterns_found = 0;

    /* Simple pattern detection: count events by category in time window */
    uint32_t category_counts[SECURITY_LOG_CAT_COUNT] = {0};
    uint32_t threat_counts[NIMCP_THREAT_CRITICAL + 1] = {0};

    size_t window_size = bridge->config.pattern_analysis.analysis_window_size;
    if (window_size > internal->ring_buffer.count) {
        window_size = internal->ring_buffer.count;
    }

    /* Analyze recent entries */
    for (size_t i = 0; i < window_size; i++) {
        size_t idx = internal->ring_buffer.count - 1 - i;
        security_log_entry_t* entry = ring_buffer_get(&internal->ring_buffer, idx);
        if (!entry) continue;

        /* Check time window */
        uint64_t entry_time_ms = entry->timestamp_ns / 1000000ULL;
        if (current_time - entry_time_ms > PATTERN_TIME_WINDOW_MS) break;

        category_counts[entry->category]++;
        if (entry->threat_level <= NIMCP_THREAT_CRITICAL) {
            threat_counts[entry->threat_level]++;
        }
    }

    /* Detect brute force pattern: many access denials */
    if (category_counts[SECURITY_LOG_CAT_ACCESS] >=
        bridge->config.pattern_analysis.min_occurrences) {
        /* Check if mostly denials would require tracking action types */
        security_threat_pattern_t* pattern = NULL;

        /* Find or create pattern */
        for (size_t i = 0; i < internal->pattern_count; i++) {
            if (internal->detected_patterns[i].type == SECURITY_PATTERN_BRUTE_FORCE) {
                pattern = &internal->detected_patterns[i];
                break;
            }
        }

        if (!pattern && internal->pattern_count < internal->pattern_capacity) {
            pattern = &internal->detected_patterns[internal->pattern_count++];
            memset(pattern, 0, sizeof(security_threat_pattern_t));
            pattern->pattern_id = internal->next_pattern_id++;
            pattern->type = SECURITY_PATTERN_BRUTE_FORCE;
            pattern->first_seen_ns = security_log_current_time_ns();
            strncpy(pattern->signature, "repeated_access_attempts",
                    SECURITY_LOG_MAX_PATTERN_LEN - 1);
            strncpy(pattern->description, "Multiple access attempts detected",
                    SECURITY_LOG_MAX_MESSAGE_LEN - 1);
        }

        if (pattern) {
            pattern->last_seen_ns = security_log_current_time_ns();
            pattern->occurrence_count++;
            pattern->confidence = nimcp_clampf(
                (float)category_counts[SECURITY_LOG_CAT_ACCESS] / 10.0f,
                0.0f, 1.0f);
            patterns_found++;
        }
    }

    /* Detect DoS pattern: many rate limit events */
    if (category_counts[SECURITY_LOG_CAT_RATE_LIMIT] >=
        bridge->config.pattern_analysis.min_occurrences * 2) {
        security_threat_pattern_t* pattern = NULL;

        for (size_t i = 0; i < internal->pattern_count; i++) {
            if (internal->detected_patterns[i].type == SECURITY_PATTERN_DOS) {
                pattern = &internal->detected_patterns[i];
                break;
            }
        }

        if (!pattern && internal->pattern_count < internal->pattern_capacity) {
            pattern = &internal->detected_patterns[internal->pattern_count++];
            memset(pattern, 0, sizeof(security_threat_pattern_t));
            pattern->pattern_id = internal->next_pattern_id++;
            pattern->type = SECURITY_PATTERN_DOS;
            pattern->first_seen_ns = security_log_current_time_ns();
            strncpy(pattern->signature, "rate_limit_flood",
                    SECURITY_LOG_MAX_PATTERN_LEN - 1);
            strncpy(pattern->description, "Possible denial of service attack",
                    SECURITY_LOG_MAX_MESSAGE_LEN - 1);
        }

        if (pattern) {
            pattern->last_seen_ns = security_log_current_time_ns();
            pattern->occurrence_count++;
            pattern->confidence = nimcp_clampf(
                (float)category_counts[SECURITY_LOG_CAT_RATE_LIMIT] / 20.0f,
                0.0f, 1.0f);
            patterns_found++;
        }
    }

    /* Detect injection pattern: threat events with injection type */
    if (threat_counts[NIMCP_THREAT_HIGH] + threat_counts[NIMCP_THREAT_CRITICAL] >=
        bridge->config.pattern_analysis.min_occurrences) {
        security_threat_pattern_t* pattern = NULL;

        for (size_t i = 0; i < internal->pattern_count; i++) {
            if (internal->detected_patterns[i].type == SECURITY_PATTERN_INJECTION) {
                pattern = &internal->detected_patterns[i];
                break;
            }
        }

        if (!pattern && internal->pattern_count < internal->pattern_capacity) {
            pattern = &internal->detected_patterns[internal->pattern_count++];
            memset(pattern, 0, sizeof(security_threat_pattern_t));
            pattern->pattern_id = internal->next_pattern_id++;
            pattern->type = SECURITY_PATTERN_INJECTION;
            pattern->first_seen_ns = security_log_current_time_ns();
            strncpy(pattern->signature, "high_severity_threats",
                    SECURITY_LOG_MAX_PATTERN_LEN - 1);
            strncpy(pattern->description, "Multiple high-severity threats detected",
                    SECURITY_LOG_MAX_MESSAGE_LEN - 1);
        }

        if (pattern) {
            pattern->last_seen_ns = security_log_current_time_ns();
            pattern->occurrence_count++;
            uint32_t high_count = threat_counts[NIMCP_THREAT_HIGH] +
                                  threat_counts[NIMCP_THREAT_CRITICAL];
            pattern->severity_avg = (float)(NIMCP_THREAT_HIGH * threat_counts[NIMCP_THREAT_HIGH] +
                                            NIMCP_THREAT_CRITICAL * threat_counts[NIMCP_THREAT_CRITICAL]) /
                                   (float)high_count;
            pattern->confidence = nimcp_clampf((float)high_count / 5.0f, 0.0f, 1.0f);
            patterns_found++;
        }
    }

    /* Update statistics */
    bridge->stats.patterns_detected = internal->pattern_count;
    bridge->stats.analysis_runs++;
    bridge->state.last_analysis_time_ns = security_log_current_time_ns();

    /* Invoke pattern callbacks */
    if (patterns_found > 0 && internal->pattern_callback) {
        for (size_t i = 0; i < internal->pattern_count; i++) {
            security_threat_pattern_t* pattern = &internal->detected_patterns[i];
            if (pattern->confidence >= bridge->config.pattern_analysis.min_pattern_confidence) {
                internal->pattern_callback(pattern, internal->pattern_callback_user_data);
                bridge->stats.pattern_callbacks_invoked++;
            }
        }
    }

    /* Update logging effects */
    bridge->logging_effects.patterns_detected = patterns_found;
    bridge->logging_effects.threat_trend_score = nimcp_clampf(
        (float)(threat_counts[NIMCP_THREAT_HIGH] + threat_counts[NIMCP_THREAT_CRITICAL]) / 10.0f,
        0.0f, 1.0f);
    bridge->logging_effects.escalation_recommended =
        (threat_counts[NIMCP_THREAT_CRITICAL] >= 2);

    BRIDGE_UNLOCK(bridge);

    return patterns_found;
}

int security_logging_get_patterns(
    const security_logging_bridge_t* bridge,
    security_threat_pattern_t* patterns,
    size_t max_count,
    size_t* actual_count
) {
    if (!bridge || !patterns || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_get_patterns: required parameter is NULL (bridge, patterns, actual_count)");
        return -1;
    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    const security_logging_internal_t* internal = get_internal_const(bridge);

    size_t copy_count = internal->pattern_count;
    if (copy_count > max_count) copy_count = max_count;

    memcpy(patterns, internal->detected_patterns,
           sizeof(security_threat_pattern_t) * copy_count);
    *actual_count = copy_count;

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return 0;
}

int security_logging_clear_patterns(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);
    internal->pattern_count = 0;
    memset(internal->detected_patterns, 0,
           sizeof(security_threat_pattern_t) * internal->pattern_capacity);
    bridge->stats.patterns_detected = 0;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_logging_feed_pattern_to_detector(
    security_logging_bridge_t* bridge,
    const security_threat_pattern_t* pattern
) {
    if (!bridge || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_feed_pattern_to_detector: required parameter is NULL (bridge, pattern)");
        return -1;
    }
    if (!bridge->anomaly_detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_feed_pattern_to_detector: bridge->anomaly_detector is NULL");
        return -1;
    }

    /* Feed pattern as training sample to anomaly detector */
    /* The pattern signature can be used as a feature for future detection */
    nimcp_anomaly_train(bridge->anomaly_detector,
                        pattern->signature,
                        strlen(pattern->signature),
                        false);  /* Train as anomalous sample */

    LOG_MODULE_DEBUG(SECURITY_LOGGING_MODULE_NAME,
                    "Fed pattern %s to anomaly detector",
                    security_pattern_type_name(pattern->type));

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int security_logging_query_entries(
    const security_logging_bridge_t* bridge,
    uint64_t start_time,
    uint64_t end_time,
    uint32_t categories,
    security_log_severity_t min_severity,
    security_log_entry_t* entries,
    size_t max_count,
    size_t* actual_count
) {
    if (!bridge || !entries || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_query_entries: required parameter is NULL (bridge, entries, actual_count)");
        return -1;
    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    const security_logging_internal_t* internal = get_internal_const(bridge);
    size_t found = 0;

    for (size_t i = 0; i < internal->ring_buffer.count && found < max_count; i++) {
        security_log_entry_t* entry = ring_buffer_get(
            (log_entry_ring_buffer_t*)&internal->ring_buffer, i);
        if (!entry) continue;

        /* Time filter */
        if (start_time > 0 && entry->timestamp_ns < start_time) continue;
        if (end_time > 0 && entry->timestamp_ns > end_time) continue;

        /* Category filter */
        if (categories != 0 && !(categories & SECURITY_LOG_CAT_MASK(entry->category))) {
            continue;
        }

        /* Severity filter */
        if (entry->severity < min_severity) continue;

        entries[found++] = *entry;
    }

    *actual_count = found;

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return 0;
}

int security_logging_get_recent(
    const security_logging_bridge_t* bridge,
    size_t count,
    security_log_entry_t* entries,
    size_t* actual_count
) {
    if (!bridge || !entries || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_get_recent: required parameter is NULL (bridge, entries, actual_count)");
        return -1;
    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    const security_logging_internal_t* internal = get_internal_const(bridge);

    size_t available = internal->ring_buffer.count;
    size_t copy_count = (count < available) ? count : available;

    /* Get most recent entries (from end of buffer) */
    for (size_t i = 0; i < copy_count; i++) {
        size_t idx = available - copy_count + i;
        security_log_entry_t* entry = ring_buffer_get(
            (log_entry_ring_buffer_t*)&internal->ring_buffer, idx);
        if (entry) {
            entries[i] = *entry;
        }
    }

    *actual_count = copy_count;

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return 0;
}

int security_logging_search(
    const security_logging_bridge_t* bridge,
    const char* search_term,
    security_log_entry_t* entries,
    size_t max_count,
    size_t* actual_count
) {
    if (!bridge || !search_term || !entries || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_search: required parameter is NULL (bridge, search_term, entries, actual_count)");
        return -1;
    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    const security_logging_internal_t* internal = get_internal_const(bridge);
    size_t found = 0;

    for (size_t i = 0; i < internal->ring_buffer.count && found < max_count; i++) {
        security_log_entry_t* entry = ring_buffer_get(
            (log_entry_ring_buffer_t*)&internal->ring_buffer, i);
        if (!entry) continue;

        /* Search in message and details */
        if (strstr(entry->message, search_term) != NULL ||
            strstr(entry->details, search_term) != NULL ||
            strstr(entry->source_module, search_term) != NULL ||
            strstr(entry->source_id, search_term) != NULL) {
            entries[found++] = *entry;
        }
    }

    *actual_count = found;

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return 0;
}

size_t security_logging_count_entries(
    const security_logging_bridge_t* bridge,
    uint64_t start_time,
    uint64_t end_time,
    uint32_t categories,
    security_log_severity_t min_severity
) {
    if (!bridge) return 0;

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    const security_logging_internal_t* internal = get_internal_const(bridge);
    size_t count = 0;

    for (size_t i = 0; i < internal->ring_buffer.count; i++) {
        security_log_entry_t* entry = ring_buffer_get(
            (log_entry_ring_buffer_t*)&internal->ring_buffer, i);
        if (!entry) continue;

        /* Time filter */
        if (start_time > 0 && entry->timestamp_ns < start_time) continue;
        if (end_time > 0 && entry->timestamp_ns > end_time) continue;

        /* Category filter */
        if (categories != 0 && !(categories & SECURITY_LOG_CAT_MASK(entry->category))) {
            continue;
        }

        /* Severity filter */
        if (entry->severity < min_severity) continue;

        count++;
    }

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return count;
}

/* ============================================================================
 * Export Functions
 * ============================================================================ */

int security_logging_export_to_file(
    security_logging_bridge_t* bridge,
    const char* file_path,
    security_log_format_t format,
    uint64_t start_time,
    uint64_t end_time
) {
    if (!bridge || !file_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_export_to_file: required parameter is NULL (bridge, file_path)");
        return -1;
    }

    FILE* fp = fopen(file_path, "w");
    if (!fp) {
        LOG_MODULE_ERROR(SECURITY_LOGGING_MODULE_NAME,
                        "Failed to open export file: %s", file_path);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);
    int exported = 0;
    char buffer[NIMCP_PATH_BUFFER_SIZE];

    /* Write header for JSON format */
    if (format == SECURITY_LOG_FORMAT_JSON) {
        fprintf(fp, "[\n");
    }

    for (size_t i = 0; i < internal->ring_buffer.count; i++) {
        security_log_entry_t* entry = ring_buffer_get(&internal->ring_buffer, i);
        if (!entry) continue;

        /* Time filter */
        if (start_time > 0 && entry->timestamp_ns < start_time) continue;
        if (end_time > 0 && entry->timestamp_ns > end_time) continue;

        if (format == SECURITY_LOG_FORMAT_JSON) {
            if (exported > 0) fprintf(fp, ",\n");
            int len = security_logging_entry_to_json(entry, buffer, sizeof(buffer));
            if (len > 0) {
                fprintf(fp, "  %s", buffer);
                exported++;
            }
        } else {
            /* Text format */
            fprintf(fp, "[%lu][%s][%s] %s\n",
                    (unsigned long)entry->timestamp_ns,
                    security_log_severity_name(entry->severity),
                    security_log_category_name(entry->category),
                    entry->message);
            if (entry->details[0]) {
                fprintf(fp, "  Details: %s\n", entry->details);
            }
            exported++;
        }
    }

    /* Close JSON array */
    if (format == SECURITY_LOG_FORMAT_JSON) {
        fprintf(fp, "\n]\n");
    }

    BRIDGE_UNLOCK(bridge);

    fclose(fp);

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME,
                   "Exported %d entries to %s", exported, file_path);

    return exported;
}

int security_logging_entry_to_json(
    const security_log_entry_t* entry,
    char* buffer,
    size_t buffer_size
) {
    if (!entry || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_entry_to_json: required parameter is NULL (entry, buffer)");
        return -1;
    }

    int written = snprintf(buffer, buffer_size,
        "{"
        "\"entry_id\":%lu,"
        "\"timestamp_ns\":%lu,"
        "\"sequence\":%u,"
        "\"category\":\"%s\","
        "\"severity\":\"%s\","
        "\"action\":\"%s\","
        "\"threat_level\":%d,"
        "\"source_module\":\"%s\","
        "\"source_id\":\"%s\","
        "\"message\":\"%s\","
        "\"confidence\":%.3f,"
        "\"anomaly_score\":%.3f"
        "}",
        (unsigned long)entry->entry_id,
        (unsigned long)entry->timestamp_ns,
        entry->sequence_number,
        security_log_category_name(entry->category),
        security_log_severity_name(entry->severity),
        security_log_action_name(entry->action),
        (int)entry->threat_level,
        entry->source_module,
        entry->source_id,
        entry->message,
        entry->confidence_score,
        entry->anomaly_score
    );

    return (written < (int)buffer_size) ? written : -1;
}

int security_logging_rotate(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    if (internal->log_file) {
        fclose(internal->log_file);

        /* Rename current log file with timestamp */
        if (bridge->config.log_file_path[0]) {
            char rotated_path[NIMCP_METRICS_PATH_SIZE];
            time_t now = time(NULL);
            struct tm tm_buf;
            struct tm* tm_info = localtime_r(&now, &tm_buf);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

            snprintf(rotated_path, sizeof(rotated_path), "%s.%s",
                     bridge->config.log_file_path, timestamp);

            rename(bridge->config.log_file_path, rotated_path);

            /* Open new log file */
            internal->log_file = fopen(bridge->config.log_file_path, "w");
        }

        bridge->stats.file_rotations++;
    }

    BRIDGE_UNLOCK(bridge);

    LOG_MODULE_INFO(SECURITY_LOGGING_MODULE_NAME, "Log rotated");
    return 0;
}

int security_logging_flush(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    security_logging_internal_t* internal = get_internal(bridge);

    if (internal->log_file) {
        fflush(internal->log_file);
    }

    bridge->state.pending_entries = 0;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int security_logging_get_stats(
    const security_logging_bridge_t* bridge,
    security_logging_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);

    return 0;
}

int security_logging_reset_stats(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK(bridge);

    size_t capacity = bridge->stats.buffer_capacity;
    size_t current_size = bridge->stats.current_buffer_size;
    float utilization = bridge->stats.buffer_utilization;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Preserve buffer info */
    bridge->stats.buffer_capacity = capacity;
    bridge->stats.current_buffer_size = current_size;
    bridge->stats.buffer_utilization = utilization;

    /* Reset internal tracking */
    security_logging_internal_t* internal = get_internal(bridge);
    internal->total_log_time_ns = 0;
    internal->max_log_time_ns = 0;
    internal->log_count = 0;

    BRIDGE_UNLOCK(bridge);
    return 0;
}

int security_logging_get_effects(
    const security_logging_bridge_t* bridge,
    security_to_logging_effects_t* security_effects,
    logging_to_security_effects_t* logging_effects
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    BRIDGE_LOCK((security_logging_bridge_t*)bridge);

    if (security_effects) {
        *security_effects = bridge->security_effects;
    }
    if (logging_effects) {
        *logging_effects = bridge->logging_effects;
    }

    BRIDGE_UNLOCK((security_logging_bridge_t*)bridge);
    return 0;
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

int security_logging_update(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->state.active) return 0;

    uint64_t current_time = current_time_ms();

    /* Check if pattern analysis is due */
    uint64_t last_analysis_ms = bridge->state.last_analysis_time_ns / 1000000ULL;
    if (bridge->config.pattern_analysis.enabled &&
        (current_time - last_analysis_ms) >= bridge->config.pattern_analysis.analysis_interval_ms) {
        security_logging_analyze_patterns(bridge);
    }

    /* Record update */
    bridge_base_record_update(&bridge->base);

    return 0;
}

int security_logging_apply_modulation(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    /* Feed detected patterns to anomaly detector */
    if (bridge->config.pattern_analysis.feed_to_anomaly_detector &&
        bridge->anomaly_detector) {
        BRIDGE_LOCK(bridge);

        security_logging_internal_t* internal = get_internal(bridge);

        for (size_t i = 0; i < internal->pattern_count; i++) {
            security_threat_pattern_t* pattern = &internal->detected_patterns[i];
            if (pattern->confidence >= bridge->config.pattern_analysis.min_pattern_confidence) {
                security_logging_feed_pattern_to_detector(bridge, pattern);
            }
        }

        BRIDGE_UNLOCK(bridge);
    }

    return 0;
}

/* ============================================================================
 * Bio-Async Functions
 * ============================================================================ */

int security_logging_connect_bio_async(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_logging_disconnect_bio_async(security_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_logging_is_bio_async_connected(const security_logging_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge_base_is_bio_async_connected(&bridge->base);
}

uint32_t security_logging_process_inbox(
    security_logging_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) return 0;
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process incoming bio-async messages */
    uint32_t processed = 0;
    /* Would implement message processing here if bio-async is fully connected */

    return processed;
}

int security_logging_broadcast_event(
    security_logging_bridge_t* bridge,
    const security_log_entry_t* entry
) {
    if (!bridge || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_broadcast_event: required parameter is NULL (bridge, entry)");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) {
        return -1;
    }

    /* Broadcast high-severity events via bio-async */
    if (entry->severity >= SECURITY_LOG_SEV_ERROR) {
        nimcp_error_t err = bio_router_broadcast(
            bridge->base.bio_ctx,
            entry,
            sizeof(security_log_entry_t)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_MODULE_WARN(SECURITY_LOGGING_MODULE_NAME,
                           "Failed to broadcast security event");
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    return 0;
}
