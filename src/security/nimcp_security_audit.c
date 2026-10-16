/**
 * @file nimcp_security_audit.c
 * @brief Security Audit Logging and Reporting - Implementation
 *
 * WHAT: Implements tamper-evident audit logging with hash chains
 *       and comprehensive reporting.
 *
 * WHY:  Provides forensic-grade audit trail for security events,
 *       enabling incident analysis and compliance verification.
 *
 * HOW:  Ring buffer in memory with optional file persistence,
 *       cryptographic hash chain for integrity verification.
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_security_audit.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_audit"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_audit, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Audit log context (internal)
 */
struct nimcp_audit_log {
    nimcp_audit_config_t config;
    nimcp_audit_event_t* events;     /**< Ring buffer of events */
    uint32_t capacity;               /**< Buffer capacity */
    uint32_t count;                  /**< Current event count */
    uint32_t head;                   /**< Write position */
    uint64_t sequence;               /**< Next sequence number */
    nimcp_audit_stats_t stats;
    nimcp_audit_callback_t callback;
    void* callback_user_data;
    FILE* log_file;
    size_t file_bytes_written;
    uint8_t chain_hash[NIMCP_AUDIT_HASH_SIZE];
    bool initialized;
    nimcp_mutex_t lock;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void compute_event_hash(nimcp_audit_event_t* event);
static uint64_t get_timestamp_ns(void);
static void simple_hash(const void* data, size_t size, uint8_t* hash_out);
static bool write_event_to_file(nimcp_audit_log_t* audit, const nimcp_audit_event_t* event);
static bool matches_query(const nimcp_audit_event_t* event, const nimcp_audit_query_t* query);

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_audit_log_t* nimcp_audit_create(void) {
    nimcp_audit_log_t* audit = nimcp_calloc(1, sizeof(nimcp_audit_log_t));
    if (!audit) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_audit_create: failed to allocate audit log");
        return NULL;
    }

    if (nimcp_mutex_init(&audit->lock, NULL) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_audit_create: failed to init mutex");
        nimcp_free(audit);
        return NULL;
    }

    return audit;
}

nimcp_result_t nimcp_audit_init(
    nimcp_audit_log_t* audit,
    const nimcp_audit_config_t* config
) {
    if (!audit || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_audit_init: audit or config is NULL");
        return NIMCP_INVALID_PARAM;
    }
    if (audit->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&audit->lock);

    audit->config = *config;

    // Set defaults for unspecified values
    if (audit->config.max_memory_entries == 0) {
        audit->config.max_memory_entries = NIMCP_AUDIT_MAX_ENTRIES;
    }
    if (audit->config.rotation_size == 0) {
        audit->config.rotation_size = NIMCP_AUDIT_DEFAULT_ROTATION_SIZE;
    }

    // Allocate event buffer
    audit->capacity = audit->config.max_memory_entries;
    audit->events = nimcp_calloc(audit->capacity, sizeof(nimcp_audit_event_t));
    if (!audit->events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_audit_init: failed to allocate events buffer");
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_NO_MEMORY;
    }

    // Open log file if configured
    if ((audit->config.destinations & NIMCP_AUDIT_DEST_FILE) &&
        audit->config.log_file_path) {
        audit->log_file = fopen(audit->config.log_file_path, "a");
        if (!audit->log_file) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "nimcp_audit_init: failed to open log file");
            nimcp_free(audit->events);
            audit->events = NULL;
            nimcp_mutex_unlock(&audit->lock);
            return NIMCP_IO_ERROR;
        }
    }

    // Initialize chain hash
    memset(audit->chain_hash, 0, NIMCP_AUDIT_HASH_SIZE);

    audit->initialized = true;
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

void nimcp_audit_destroy(nimcp_audit_log_t* audit) {
    if (!audit) return;

    nimcp_mutex_lock(&audit->lock);

    if (audit->log_file) {
        fclose(audit->log_file);
        audit->log_file = NULL;
    }

    if (audit->events) {
        // Clear sensitive data
        memset(audit->events, 0, audit->capacity * sizeof(nimcp_audit_event_t));
        nimcp_free(audit->events);
        audit->events = NULL;
    }

    audit->initialized = false;
    nimcp_mutex_unlock(&audit->lock);
    nimcp_mutex_destroy(&audit->lock);

    nimcp_free(audit);
}

nimcp_audit_config_t nimcp_audit_default_config(void) {
    nimcp_audit_config_t config = {
        .destinations = NIMCP_AUDIT_DEST_MEMORY,
        .log_file_path = NULL,
        .rotation_size = NIMCP_AUDIT_DEFAULT_ROTATION_SIZE,
        .max_memory_entries = NIMCP_AUDIT_MAX_ENTRIES,
        .min_severity = NIMCP_AUDIT_SEV_INFO,
        .enable_chain_verification = true,
        .enable_timestamps = true,
        .enable_stack_traces = false,
        .synchronous_write = false
    };
    return config;
}

//=============================================================================
// Event Logging Functions
//=============================================================================

nimcp_result_t nimcp_audit_log(
    nimcp_audit_log_t* audit,
    nimcp_audit_category_t category,
    nimcp_audit_severity_t severity,
    nimcp_audit_outcome_t outcome,
    const char* source,
    const char* message
) {
    if (!audit || !source || !message) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    // Check minimum severity
    if (severity < audit->config.min_severity) {
        return NIMCP_SUCCESS;  // Filtered out
    }

    nimcp_mutex_lock(&audit->lock);

    // Get write position
    uint32_t pos = audit->head;

    // Create event
    nimcp_audit_event_t* event = &audit->events[pos];
    memset(event, 0, sizeof(nimcp_audit_event_t));

    event->sequence = audit->sequence++;
    event->timestamp = audit->config.enable_timestamps ? get_timestamp_ns() : 0;
    event->category = category;
    event->severity = severity;
    event->outcome = outcome;

    strncpy(event->source, source, NIMCP_AUDIT_MAX_SOURCE_LEN - 1);
    strncpy(event->message, message, NIMCP_AUDIT_MAX_MSG_LEN - 1);

    // Copy previous hash for chain
    if (audit->config.enable_chain_verification) {
        memcpy(event->prev_hash, audit->chain_hash, NIMCP_AUDIT_HASH_SIZE);
        compute_event_hash(event);
        memcpy(audit->chain_hash, event->hash, NIMCP_AUDIT_HASH_SIZE);
    }

    // Advance write position
    audit->head = (audit->head + 1) % audit->capacity;
    if (audit->count < audit->capacity) {
        audit->count++;
    } else {
        audit->stats.dropped_events++;
    }

    // Update statistics
    audit->stats.total_events++;
    if (category < 10) {
        audit->stats.events_by_category[category]++;
    }
    if (severity < 8) {
        audit->stats.events_by_severity[severity]++;
    }

    // Write to file if configured
    if ((audit->config.destinations & NIMCP_AUDIT_DEST_FILE) && audit->log_file) {
        write_event_to_file(audit, event);
    }

    // Call callback if configured
    if ((audit->config.destinations & NIMCP_AUDIT_DEST_CALLBACK) && audit->callback) {
        audit->callback(event, audit->callback_user_data);
    }

    nimcp_mutex_unlock(&audit->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_logf(
    nimcp_audit_log_t* audit,
    nimcp_audit_category_t category,
    nimcp_audit_severity_t severity,
    nimcp_audit_outcome_t outcome,
    const char* source,
    const char* format,
    ...
) {
    char message[NIMCP_AUDIT_MAX_MSG_LEN];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    return nimcp_audit_log(audit, category, severity, outcome, source, message);
}

nimcp_result_t nimcp_audit_log_event(
    nimcp_audit_log_t* audit,
    const nimcp_audit_event_t* event
) {
    if (!audit || !event) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    return nimcp_audit_log(audit, event->category, event->severity,
                          event->outcome, event->source, event->message);
}

nimcp_result_t nimcp_audit_log_access(
    nimcp_audit_log_t* audit,
    uint32_t subject_id,
    uint32_t object_id,
    const char* action,
    bool granted,
    const char* reason
) {
    if (!audit || !action) return NIMCP_INVALID_PARAM;

    char message[NIMCP_AUDIT_MAX_MSG_LEN];
    snprintf(message, sizeof(message),
             "Subject %u %s %s on object %u: %s",
             subject_id,
             granted ? "granted" : "denied",
             action,
             object_id,
             reason ? reason : "");

    return nimcp_audit_log(audit,
                          NIMCP_AUDIT_CAT_ACCESS,
                          granted ? NIMCP_AUDIT_SEV_INFO : NIMCP_AUDIT_SEV_WARNING,
                          granted ? NIMCP_AUDIT_OUTCOME_SUCCESS : NIMCP_AUDIT_OUTCOME_DENIED,
                          "access_control",
                          message);
}

nimcp_result_t nimcp_audit_log_threat(
    nimcp_audit_log_t* audit,
    nimcp_threat_level_t threat_level,
    const char* threat_type,
    const char* details
) {
    if (!audit || !threat_type) return NIMCP_INVALID_PARAM;

    nimcp_audit_severity_t severity;
    switch (threat_level) {
        case NIMCP_THREAT_LOW:      severity = NIMCP_AUDIT_SEV_NOTICE; break;
        case NIMCP_THREAT_MEDIUM:   severity = NIMCP_AUDIT_SEV_WARNING; break;
        case NIMCP_THREAT_HIGH:     severity = NIMCP_AUDIT_SEV_ERROR; break;
        case NIMCP_THREAT_CRITICAL: severity = NIMCP_AUDIT_SEV_CRITICAL; break;
        default:                     severity = NIMCP_AUDIT_SEV_INFO; break;
    }

    char message[NIMCP_AUDIT_MAX_MSG_LEN];
    snprintf(message, sizeof(message),
             "THREAT [%s]: %s - %s",
             nimcp_threat_level_name(threat_level),
             threat_type,
             details ? details : "");

    return nimcp_audit_log(audit,
                          NIMCP_AUDIT_CAT_THREAT,
                          severity,
                          NIMCP_AUDIT_OUTCOME_UNKNOWN,
                          "threat_detection",
                          message);
}

nimcp_result_t nimcp_audit_log_config_change(
    nimcp_audit_log_t* audit,
    const char* component,
    const char* parameter,
    const char* old_value,
    const char* new_value
) {
    if (!audit || !component || !parameter) return NIMCP_INVALID_PARAM;

    char message[NIMCP_AUDIT_MAX_MSG_LEN];
    snprintf(message, sizeof(message),
             "Config change: %s.%s changed from '%s' to '%s'",
             component,
             parameter,
             old_value ? old_value : "(null)",
             new_value ? new_value : "(null)");

    return nimcp_audit_log(audit,
                          NIMCP_AUDIT_CAT_CONFIGURATION,
                          NIMCP_AUDIT_SEV_NOTICE,
                          NIMCP_AUDIT_OUTCOME_SUCCESS,
                          "configuration",
                          message);
}

//=============================================================================
// Query and Retrieval
//=============================================================================

nimcp_result_t nimcp_audit_query(
    nimcp_audit_log_t* audit,
    const nimcp_audit_query_t* query,
    nimcp_audit_event_t** results,
    uint32_t* count
) {
    if (!audit || !query || !results || !count) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&audit->lock);

    // Count matching events first
    uint32_t matches = 0;
    for (uint32_t i = 0; i < audit->count; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        if (matches_query(&audit->events[idx], query)) {
            matches++;
        }
    }

    if (matches == 0) {
        *results = NULL;
        *count = 0;
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_SUCCESS;
    }

    // Limit results
    if (query->max_results > 0 && matches > query->max_results) {
        matches = query->max_results;
    }

    // Allocate result array
    *results = nimcp_calloc(matches, sizeof(nimcp_audit_event_t));
    if (!*results) {
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_NO_MEMORY;
    }

    // Copy matching events
    uint32_t copied = 0;
    for (uint32_t i = 0; i < audit->count && copied < matches; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        if (matches_query(&audit->events[idx], query)) {
            (*results)[copied++] = audit->events[idx];
        }
    }

    *count = copied;
    nimcp_mutex_unlock(&audit->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_get_recent(
    nimcp_audit_log_t* audit,
    uint32_t count,
    nimcp_audit_event_t** events,
    uint32_t* actual
) {
    if (!audit || !events || !actual) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&audit->lock);

    uint32_t to_copy = (count < audit->count) ? count : audit->count;

    if (to_copy == 0) {
        *events = NULL;
        *actual = 0;
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_SUCCESS;
    }

    *events = nimcp_calloc(to_copy, sizeof(nimcp_audit_event_t));
    if (!*events) {
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_NO_MEMORY;
    }

    // Copy most recent events
    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (audit->head + audit->capacity - to_copy + i) % audit->capacity;
        (*events)[i] = audit->events[idx];
    }

    *actual = to_copy;
    nimcp_mutex_unlock(&audit->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_get_by_sequence(
    nimcp_audit_log_t* audit,
    uint64_t sequence,
    nimcp_audit_event_t* event
) {
    if (!audit || !event) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    nimcp_mutex_lock(&audit->lock);

    for (uint32_t i = 0; i < audit->count; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        if (audit->events[idx].sequence == sequence) {
            *event = audit->events[idx];
            nimcp_mutex_unlock(&audit->lock);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(&audit->lock);
    return NIMCP_NOT_FOUND;
}

//=============================================================================
// Chain Verification
//=============================================================================

bool nimcp_audit_verify_chain(
    nimcp_audit_log_t* audit,
    uint64_t* first_broken
) {
    if (!audit || !audit->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_audit_verify_chain: required parameter is NULL (audit, audit->initialized)");
        return false;
    }
    if (!audit->config.enable_chain_verification) return true;

    nimcp_mutex_lock(&audit->lock);

    if (audit->count < 2) {
        nimcp_mutex_unlock(&audit->lock);
        return true;
    }

    audit->stats.chain_verifications++;

    uint8_t expected_prev[NIMCP_AUDIT_HASH_SIZE] = {0};

    for (uint32_t i = 0; i < audit->count; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        nimcp_audit_event_t* event = &audit->events[idx];

        // Verify previous hash matches
        if (i > 0 && memcmp(event->prev_hash, expected_prev, NIMCP_AUDIT_HASH_SIZE) != 0) {
            audit->stats.chain_failures++;
            if (first_broken) *first_broken = event->sequence;
            nimcp_mutex_unlock(&audit->lock);
            return false;
        }

        // Verify this event's hash
        uint8_t computed[NIMCP_AUDIT_HASH_SIZE];
        uint8_t saved_hash[NIMCP_AUDIT_HASH_SIZE];
        memcpy(saved_hash, event->hash, NIMCP_AUDIT_HASH_SIZE);
        compute_event_hash(event);

        if (memcmp(event->hash, saved_hash, NIMCP_AUDIT_HASH_SIZE) != 0) {
            audit->stats.chain_failures++;
            if (first_broken) *first_broken = event->sequence;
            memcpy(event->hash, saved_hash, NIMCP_AUDIT_HASH_SIZE);  // Restore
            nimcp_mutex_unlock(&audit->lock);
            return false;
        }

        memcpy(expected_prev, event->hash, NIMCP_AUDIT_HASH_SIZE);
    }

    nimcp_mutex_unlock(&audit->lock);
    return true;
}

bool nimcp_audit_verify_event(
    nimcp_audit_log_t* audit,
    uint64_t sequence
) {
    nimcp_audit_event_t event;
    if (nimcp_audit_get_by_sequence(audit, sequence, &event) != NIMCP_SUCCESS) {
        return false;
    }

    // Recompute and verify hash
    uint8_t saved_hash[NIMCP_AUDIT_HASH_SIZE];
    memcpy(saved_hash, event.hash, NIMCP_AUDIT_HASH_SIZE);
    compute_event_hash(&event);

    return memcmp(event.hash, saved_hash, NIMCP_AUDIT_HASH_SIZE) == 0;
}

//=============================================================================
// Reporting
//=============================================================================

int nimcp_audit_generate_report(
    nimcp_audit_log_t* audit,
    const nimcp_audit_query_t* query,
    char* buffer,
    size_t size
) {
    if (!audit || !buffer || size == 0) return 0;

    nimcp_mutex_lock(&audit->lock);

    int written = 0;
    written += snprintf(buffer + written, size - written,
                       "=== NIMCP Security Audit Report ===\n\n");

    // Statistics summary
    written += snprintf(buffer + written, size - written,
                       "Summary:\n"
                       "  Total Events: %lu\n"
                       "  Dropped Events: %lu\n"
                       "  Chain Verifications: %lu\n"
                       "  Chain Failures: %lu\n\n",
                       (unsigned long)audit->stats.total_events,
                       (unsigned long)audit->stats.dropped_events,
                       (unsigned long)audit->stats.chain_verifications,
                       (unsigned long)audit->stats.chain_failures);

    // Events by category
    written += snprintf(buffer + written, size - written, "Events by Category:\n");
    for (int i = 0; i < 10; i++) {
        if (audit->stats.events_by_category[i] > 0) {
            written += snprintf(buffer + written, size - written,
                               "  %s: %lu\n",
                               nimcp_audit_category_name((nimcp_audit_category_t)i),
                               (unsigned long)audit->stats.events_by_category[i]);
        }
    }

    // Events by severity
    written += snprintf(buffer + written, size - written, "\nEvents by Severity:\n");
    for (int i = 0; i < 8; i++) {
        if (audit->stats.events_by_severity[i] > 0) {
            written += snprintf(buffer + written, size - written,
                               "  %s: %lu\n",
                               nimcp_audit_severity_name((nimcp_audit_severity_t)i),
                               (unsigned long)audit->stats.events_by_severity[i]);
        }
    }

    // Recent critical events
    written += snprintf(buffer + written, size - written,
                       "\nRecent Critical Events:\n");

    uint32_t critical_count = 0;
    for (uint32_t i = audit->count; i > 0 && critical_count < 10; i--) {
        uint32_t idx = (audit->head + audit->capacity - i) % audit->capacity;
        nimcp_audit_event_t* event = &audit->events[idx];

        if (event->severity >= NIMCP_AUDIT_SEV_ERROR) {
            char event_str[NIMCP_ERROR_BUFFER_SIZE];
            nimcp_audit_format_event(event, event_str, sizeof(event_str));
            written += snprintf(buffer + written, size - written,
                               "  %s\n", event_str);
            critical_count++;
        }
    }

    if (critical_count == 0) {
        written += snprintf(buffer + written, size - written,
                           "  (none)\n");
    }

    nimcp_mutex_unlock(&audit->lock);
    return written;
}

int nimcp_audit_security_summary(
    nimcp_audit_log_t* audit,
    char* buffer,
    size_t size
) {
    if (!audit || !buffer || size == 0) return 0;

    nimcp_mutex_lock(&audit->lock);

    int written = 0;

    // Count threat events
    uint64_t threats = audit->stats.events_by_category[NIMCP_AUDIT_CAT_THREAT];
    uint64_t access_denials = 0;
    uint64_t integrity_issues = audit->stats.events_by_category[NIMCP_AUDIT_CAT_INTEGRITY];

    // Count access denials
    for (uint32_t i = 0; i < audit->count; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        if (audit->events[idx].category == NIMCP_AUDIT_CAT_ACCESS &&
            audit->events[idx].outcome == NIMCP_AUDIT_OUTCOME_DENIED) {
            access_denials++;
        }
    }

    const char* status = "NORMAL";
    if (threats > 0 || integrity_issues > 0) {
        status = "ALERT";
    }

    written += snprintf(buffer + written, size - written,
                       "Security Status: %s\n"
                       "  Threats Detected: %lu\n"
                       "  Access Denials: %lu\n"
                       "  Integrity Issues: %lu\n"
                       "  Chain Integrity: %s\n",
                       status,
                       (unsigned long)threats,
                       (unsigned long)access_denials,
                       (unsigned long)integrity_issues,
                       audit->stats.chain_failures == 0 ? "INTACT" : "COMPROMISED");

    nimcp_mutex_unlock(&audit->lock);
    return written;
}

nimcp_result_t nimcp_audit_export(
    nimcp_audit_log_t* audit,
    const char* filepath,
    const nimcp_audit_query_t* query
) {
    if (!audit || !filepath) return NIMCP_INVALID_PARAM;
    if (!audit->initialized) return NIMCP_INVALID_STATE;

    FILE* f = fopen(filepath, "w");
    if (!f) return NIMCP_IO_ERROR;

    nimcp_mutex_lock(&audit->lock);

    for (uint32_t i = 0; i < audit->count; i++) {
        uint32_t idx = (audit->head + audit->capacity - audit->count + i) % audit->capacity;
        nimcp_audit_event_t* event = &audit->events[idx];

        if (!query || matches_query(event, query)) {
            char event_str[NIMCP_LOG_BUFFER_SIZE];
            nimcp_audit_format_event(event, event_str, sizeof(event_str));
            fprintf(f, "%s\n", event_str);
        }
    }

    nimcp_mutex_unlock(&audit->lock);
    fclose(f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Log Management
//=============================================================================

nimcp_result_t nimcp_audit_rotate(nimcp_audit_log_t* audit) {
    if (!audit) return NIMCP_INVALID_PARAM;
    if (!audit->log_file) return NIMCP_SUCCESS;

    nimcp_mutex_lock(&audit->lock);

    fclose(audit->log_file);

    // Rename current log
    char backup_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(backup_path, sizeof(backup_path), "%s.%lu",
             audit->config.log_file_path, (unsigned long)time(NULL));
    rename(audit->config.log_file_path, backup_path);

    // Open new log
    audit->log_file = fopen(audit->config.log_file_path, "a");
    audit->file_bytes_written = 0;
    audit->stats.file_rotations++;

    nimcp_mutex_unlock(&audit->lock);
    return audit->log_file ? NIMCP_SUCCESS : NIMCP_IO_ERROR;
}

nimcp_result_t nimcp_audit_flush(nimcp_audit_log_t* audit) {
    if (!audit) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&audit->lock);
    if (audit->log_file) {
        fflush(audit->log_file);
    }
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_clear(nimcp_audit_log_t* audit) {
    if (!audit) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&audit->lock);

    memset(audit->events, 0, audit->capacity * sizeof(nimcp_audit_event_t));
    audit->count = 0;
    audit->head = 0;
    memset(audit->chain_hash, 0, NIMCP_AUDIT_HASH_SIZE);

    nimcp_mutex_unlock(&audit->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_set_callback(
    nimcp_audit_log_t* audit,
    nimcp_audit_callback_t callback,
    void* user_data
) {
    if (!audit) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&audit->lock);
    audit->callback = callback;
    audit->callback_user_data = user_data;
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics
//=============================================================================

nimcp_result_t nimcp_audit_get_stats(
    nimcp_audit_log_t* audit,
    nimcp_audit_stats_t* stats
) {
    if (!audit || !stats) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&audit->lock);
    *stats = audit->stats;
    stats->bytes_written = audit->file_bytes_written;
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_audit_reset_stats(nimcp_audit_log_t* audit) {
    if (!audit) return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&audit->lock);
    memset(&audit->stats, 0, sizeof(nimcp_audit_stats_t));
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_audit_category_name(nimcp_audit_category_t category) {
    switch (category) {
        case NIMCP_AUDIT_CAT_ACCESS:         return "ACCESS";
        case NIMCP_AUDIT_CAT_AUTHENTICATION: return "AUTHENTICATION";
        case NIMCP_AUDIT_CAT_AUTHORIZATION:  return "AUTHORIZATION";
        case NIMCP_AUDIT_CAT_INTEGRITY:      return "INTEGRITY";
        case NIMCP_AUDIT_CAT_CONFIGURATION:  return "CONFIGURATION";
        case NIMCP_AUDIT_CAT_POLICY:         return "POLICY";
        case NIMCP_AUDIT_CAT_THREAT:         return "THREAT";
        case NIMCP_AUDIT_CAT_SYSTEM:         return "SYSTEM";
        case NIMCP_AUDIT_CAT_NEURAL:         return "NEURAL";
        case NIMCP_AUDIT_CAT_DIRECTIVE:      return "DIRECTIVE";
        default:                              return "UNKNOWN";
    }
}

const char* nimcp_audit_severity_name(nimcp_audit_severity_t severity) {
    switch (severity) {
        case NIMCP_AUDIT_SEV_DEBUG:     return "DEBUG";
        case NIMCP_AUDIT_SEV_INFO:      return "INFO";
        case NIMCP_AUDIT_SEV_NOTICE:    return "NOTICE";
        case NIMCP_AUDIT_SEV_WARNING:   return "WARNING";
        case NIMCP_AUDIT_SEV_ERROR:     return "ERROR";
        case NIMCP_AUDIT_SEV_CRITICAL:  return "CRITICAL";
        case NIMCP_AUDIT_SEV_ALERT:     return "ALERT";
        case NIMCP_AUDIT_SEV_EMERGENCY: return "EMERGENCY";
        default:                         return "UNKNOWN";
    }
}

const char* nimcp_audit_outcome_name(nimcp_audit_outcome_t outcome) {
    switch (outcome) {
        case NIMCP_AUDIT_OUTCOME_SUCCESS: return "SUCCESS";
        case NIMCP_AUDIT_OUTCOME_FAILURE: return "FAILURE";
        case NIMCP_AUDIT_OUTCOME_DENIED:  return "DENIED";
        case NIMCP_AUDIT_OUTCOME_ERROR:   return "ERROR";
        case NIMCP_AUDIT_OUTCOME_UNKNOWN: return "UNKNOWN";
        default:                           return "UNKNOWN";
    }
}

int nimcp_audit_format_event(
    const nimcp_audit_event_t* event,
    char* buffer,
    size_t size
) {
    if (!event || !buffer || size == 0) return 0;

    return snprintf(buffer, size,
                   "[%lu] %s/%s/%s [%s] %s",
                   (unsigned long)event->sequence,
                   nimcp_audit_category_name(event->category),
                   nimcp_audit_severity_name(event->severity),
                   nimcp_audit_outcome_name(event->outcome),
                   event->source,
                   event->message);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void simple_hash(const void* data, size_t size, uint8_t* hash_out) {
    memset(hash_out, 0, NIMCP_AUDIT_HASH_SIZE);

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t h1 = 0x6a09e667f3bcc908ULL;
    uint64_t h2 = 0xbb67ae8584caa73bULL;
    uint64_t h3 = 0x3c6ef372fe94f82bULL;
    uint64_t h4 = 0xa54ff53a5f1d36f1ULL;

    for (size_t i = 0; i < size; i++) {
        h1 = (h1 ^ bytes[i]) * 0x100000001b3ULL;
        h2 = (h2 ^ bytes[i]) * 0x100000001b3ULL;
        h3 = (h3 ^ bytes[i]) * 0x100000001b3ULL;
        h4 = (h4 ^ bytes[i]) * 0x100000001b3ULL;
        h1 ^= h1 >> 33;
        h2 ^= h2 >> 33;
        h3 ^= h3 >> 33;
        h4 ^= h4 >> 33;
    }

    memcpy(hash_out, &h1, 8);
    memcpy(hash_out + 8, &h2, 8);
    memcpy(hash_out + 16, &h3, 8);
    memcpy(hash_out + 24, &h4, 8);
}

static void compute_event_hash(nimcp_audit_event_t* event) {
    // Hash everything except the hash field itself
    size_t hash_offset = offsetof(nimcp_audit_event_t, hash);
    simple_hash(event, hash_offset, event->hash);
}

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static bool write_event_to_file(nimcp_audit_log_t* audit, const nimcp_audit_event_t* event) {
    if (!audit->log_file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "write_event_to_file: audit->log_file is NULL");
        return false;
    }

    char buffer[NIMCP_LOG_BUFFER_SIZE];
    int len = nimcp_audit_format_event(event, buffer, sizeof(buffer) - 1);
    buffer[len] = '\n';

    size_t written = fwrite(buffer, 1, len + 1, audit->log_file);
    audit->file_bytes_written += written;
    audit->stats.bytes_written += written;

    if (audit->config.synchronous_write) {
        fflush(audit->log_file);
    }

    // Check for rotation
    if (audit->file_bytes_written >= audit->config.rotation_size) {
        nimcp_mutex_unlock(&audit->lock);
        nimcp_audit_rotate(audit);
        nimcp_mutex_lock(&audit->lock);
    }

    return written == (size_t)(len + 1);
}

static bool matches_query(const nimcp_audit_event_t* event, const nimcp_audit_query_t* query) {
    if (!query) return true;

    // Time range filter
    if (query->start_time > 0 && event->timestamp < query->start_time) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }
    if (query->end_time > 0 && event->timestamp > query->end_time) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }

    // Category filter
    if ((int)query->category >= 0 && event->category != query->category) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: capacity exceeded");
        return false;
    }

    // Severity filter
    if (event->severity < query->min_severity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }

    // Subject filter
    if (query->subject_id > 0 && event->subject_id != query->subject_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }

    // Source pattern filter
    if (query->source_pattern && strstr(event->source, query->source_pattern) == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }

    // Message pattern filter
    if (query->message_pattern && strstr(event->message, query->message_pattern) == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_query: validation failed");
        return false;
    }

    return true;
}
