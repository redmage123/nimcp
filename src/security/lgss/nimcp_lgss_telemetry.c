/**
 * @file nimcp_lgss_telemetry.c
 * @brief LGSS Telemetry and Audit Subsystem Implementation
 *
 * WHAT: Logging, auditing, and telemetry for the LGSS safety system
 * WHY:  Ensure all safety decisions are recorded for accountability
 * HOW:  Append-only log with hash chaining for tamper detection
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#include "security/lgss/nimcp_lgss_telemetry.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_telemetry, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * LOGGING
 *============================================================================*/

#define TELEM_LOG_DEBUG(fmt, ...) \
    NIMCP_LOG_DEBUG("LGSS-TELEM", fmt, ##__VA_ARGS__)
#define TELEM_LOG_INFO(fmt, ...) \
    NIMCP_LOG_INFO("LGSS-TELEM", fmt, ##__VA_ARGS__)
#define TELEM_LOG_WARN(fmt, ...) \
    NIMCP_LOG_WARN("LGSS-TELEM", fmt, ##__VA_ARGS__)
#define TELEM_LOG_ERROR(fmt, ...) \
    NIMCP_LOG_ERROR("LGSS-TELEM", fmt, ##__VA_ARGS__)

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Ring buffer for log entries
 */
typedef struct {
    lgss_telemetry_entry_t* entries;
    size_t capacity;
    size_t head;
    size_t count;
} lgss_telem_buffer_t;

/**
 * @brief Telemetry context structure
 */
struct lgss_telemetry {
    uint32_t magic;
    lgss_telemetry_config_t config;

    /* Ring buffer for memory logging */
    lgss_telem_buffer_t buffer;

    /* Sequence counter */
    uint64_t sequence;

    /* Last entry hash (for chain) */
    uint8_t last_hash[LGSS_TELEMETRY_HASH_SIZE];
    bool has_last_hash;

    /* Statistics */
    lgss_telemetry_stats_t stats;

    /* File handle (if logging to file) */
    FILE* log_file;

    /* Thread safety */
    void* mutex;

    /* Async logging state */
    bool async_running;
    void* async_thread;
    void* async_queue;
};

/*=============================================================================
 * HASH FUNCTIONS (Simple implementation - in production use crypto lib)
 *============================================================================*/

/**
 * @brief Simple hash function for entry chaining
 *
 * NOTE: In production, this should use SHA-256 from a crypto library
 */
static void compute_entry_hash(
    const lgss_telemetry_entry_t* entry,
    uint8_t hash[LGSS_TELEMETRY_HASH_SIZE])
{
    /* Simple hash for demo - in production use SHA-256 */
    uint64_t h1 = 0, h2 = 0, h3 = 0, h4 = 0;

    h1 = entry->sequence ^ entry->timestamp_us;
    h2 = (uint64_t)entry->event_type ^ ((uint64_t)entry->severity << 32);
    h3 = entry->rule_id ^ (uint64_t)(entry->confidence * 1000000.0f);

    /* Mix in description */
    for (size_t i = 0; i < strlen(entry->description) && i < 64; i++) {
        h4 ^= ((uint64_t)entry->description[i]) << ((i % 8) * 8);
    }

    /* Mix in previous hash */
    for (int i = 0; i < 8; i++) {
        h1 ^= ((uint64_t)entry->prev_hash[i]) << (i * 8);
        h2 ^= ((uint64_t)entry->prev_hash[i + 8]) << (i * 8);
        h3 ^= ((uint64_t)entry->prev_hash[i + 16]) << (i * 8);
        h4 ^= ((uint64_t)entry->prev_hash[i + 24]) << (i * 8);
    }

    /* Store in hash */
    memcpy(hash, &h1, 8);
    memcpy(hash + 8, &h2, 8);
    memcpy(hash + 16, &h3, 8);
    memcpy(hash + 24, &h4, 8);
}

/*=============================================================================
 * CONFIGURATION FUNCTIONS
 *============================================================================*/

int lgss_telemetry_config_init(lgss_telemetry_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(lgss_telemetry_config_t));

    config->enabled = true;
    config->log_to_file = false;
    config->log_to_memory = true;
    config->memory_buffer_size = LGSS_TELEMETRY_DEFAULT_BUFFER_SIZE;
    config->use_callback = false;
    config->async_logging = false;
    config->min_severity = LGSS_TELEM_SEVERITY_INFO;
    config->verify_chain = true;
    config->sync_on_write = false;
    config->include_timestamps = true;
    config->include_eval_details = true;

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

lgss_telemetry_t* lgss_telemetry_create(const lgss_telemetry_config_t* config)
{
    lgss_telemetry_t* telem = NULL;
    lgss_telemetry_config_t default_config;

    if (!config) {
        lgss_telemetry_config_init(&default_config);
        config = &default_config;
    }

    if (!config->enabled) {
        TELEM_LOG_INFO("Telemetry disabled by configuration");
        return NULL;
    }

    telem = nimcp_calloc(1, sizeof(lgss_telemetry_t));
    if (!telem) {
        TELEM_LOG_ERROR("Failed to allocate telemetry context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_telemetry_create: telem is NULL");
        return NULL;
    }

    telem->magic = NIMCP_LGSS_TELEMETRY_MAGIC;
    memcpy(&telem->config, config, sizeof(lgss_telemetry_config_t));

    /* Allocate ring buffer if memory logging enabled */
    if (config->log_to_memory && config->memory_buffer_size > 0) {
        telem->buffer.entries = nimcp_calloc(config->memory_buffer_size,
            sizeof(lgss_telemetry_entry_t));
        if (!telem->buffer.entries) {
            TELEM_LOG_ERROR("Failed to allocate log buffer");
            nimcp_free(telem);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss_telemetry_create: telem->buffer is NULL");
            return NULL;
        }
        telem->buffer.capacity = config->memory_buffer_size;
        telem->buffer.head = 0;
        telem->buffer.count = 0;
    }

    /* Open log file if file logging enabled */
    if (config->log_to_file && config->log_file_path[0]) {
        telem->log_file = fopen(config->log_file_path, "a");
        if (!telem->log_file) {
            TELEM_LOG_WARN("Failed to open log file: %s", config->log_file_path);
        }
    }

    telem->sequence = 0;
    telem->has_last_hash = false;
    memset(&telem->stats, 0, sizeof(lgss_telemetry_stats_t));
    telem->stats.chain_valid = true;

    TELEM_LOG_INFO("Telemetry subsystem created (buffer=%zu entries)",
        config->memory_buffer_size);

    return telem;
}

void lgss_telemetry_destroy(lgss_telemetry_t* telemetry)
{
    if (!telemetry) {
        return;
    }

    if (telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        TELEM_LOG_WARN("Destroying invalid telemetry context");
        return;
    }

    /* Stop async logging if running */
    if (telemetry->async_running) {
        lgss_telemetry_stop(telemetry);
    }

    /* Close log file */
    if (telemetry->log_file) {
        fclose(telemetry->log_file);
    }

    /* Free buffer */
    if (telemetry->buffer.entries) {
        nimcp_free(telemetry->buffer.entries);
    }

    telemetry->magic = 0;
    nimcp_free(telemetry);

    TELEM_LOG_INFO("Telemetry subsystem destroyed");
}

int lgss_telemetry_start(lgss_telemetry_t* telemetry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!telemetry->config.async_logging) {
        return 0; /* Sync mode, nothing to start */
    }

    /* TODO: Implement async logging thread */
    telemetry->async_running = true;

    return 0;
}

int lgss_telemetry_stop(lgss_telemetry_t* telemetry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!telemetry->async_running) {
        return 0;
    }

    /* TODO: Stop async logging thread */
    telemetry->async_running = false;

    /* Flush remaining entries */
    lgss_telemetry_flush(telemetry);

    return 0;
}

/*=============================================================================
 * LOGGING FUNCTIONS
 *============================================================================*/

/**
 * @brief Internal function to add entry to buffer
 */
static int add_entry_to_buffer(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_entry_t* entry)
{
    if (!telemetry->buffer.entries) {
        return 0; /* Buffer not allocated */
    }

    /* Copy previous hash */
    if (telemetry->has_last_hash) {
        memcpy(entry->prev_hash, telemetry->last_hash, LGSS_TELEMETRY_HASH_SIZE);
    } else {
        memset(entry->prev_hash, 0, LGSS_TELEMETRY_HASH_SIZE);
    }

    /* Compute entry hash */
    compute_entry_hash(entry, entry->entry_hash);

    /* Store for next entry */
    memcpy(telemetry->last_hash, entry->entry_hash, LGSS_TELEMETRY_HASH_SIZE);
    telemetry->has_last_hash = true;

    /* Add to ring buffer */
    size_t idx = telemetry->buffer.head;
    memcpy(&telemetry->buffer.entries[idx], entry, sizeof(lgss_telemetry_entry_t));

    telemetry->buffer.head = (telemetry->buffer.head + 1) %
        telemetry->buffer.capacity;
    if (telemetry->buffer.count < telemetry->buffer.capacity) {
        telemetry->buffer.count++;
    }

    return 0;
}

/**
 * @brief Internal function to write entry to file
 */
static int write_entry_to_file(
    lgss_telemetry_t* telemetry,
    const lgss_telemetry_entry_t* entry)
{
    if (!telemetry->log_file) {
        return 0;
    }

    char buffer[NIMCP_DEBUG_BUFFER_SIZE];
    int len = lgss_telemetry_format_entry(entry, buffer, sizeof(buffer));
    if (len < 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    fprintf(telemetry->log_file, "%s\n", buffer);

    if (telemetry->config.sync_on_write) {
        fflush(telemetry->log_file);
    }

    telemetry->stats.bytes_written += len + 1;

    return 0;
}

/**
 * @brief Internal function to invoke callback
 */
static void invoke_callback(
    lgss_telemetry_t* telemetry,
    const lgss_telemetry_entry_t* entry)
{
    if (telemetry->config.use_callback && telemetry->config.callback) {
        telemetry->config.callback(entry, telemetry->config.callback_user_data);
    }
}

int lgss_telemetry_log(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    lgss_telemetry_severity_t severity,
    const char* source,
    const char* description,
    ...)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (severity < telemetry->config.min_severity) {
        return 0; /* Below threshold */
    }

    lgss_telemetry_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.sequence = ++telemetry->sequence;
    entry.timestamp_us = nimcp_time_now_us();
    entry.event_type = event;
    entry.severity = severity;

    if (source) {
        strncpy(entry.source, source, LGSS_TELEMETRY_MAX_SOURCE - 1);
    }

    /* Format description */
    if (description) {
        va_list args;
        va_start(args, description);
        vsnprintf(entry.description, LGSS_TELEMETRY_MAX_DESC, description, args);
        va_end(args);
    }

    /* Add to buffer */
    add_entry_to_buffer(telemetry, &entry);

    /* Write to file */
    write_entry_to_file(telemetry, &entry);

    /* Invoke callback */
    invoke_callback(telemetry, &entry);

    /* Update stats */
    telemetry->stats.entries_logged++;

    if (telemetry->stats.oldest_entry_timestamp == 0) {
        telemetry->stats.oldest_entry_timestamp = entry.timestamp_us;
    }
    telemetry->stats.newest_entry_timestamp = entry.timestamp_us;

    return 0;
}

int lgss_telemetry_log_evaluation(
    lgss_telemetry_t* telemetry,
    const safety_action_context_t* context,
    const safety_evaluation_t* result,
    const char* source)
{
    if (!telemetry || !context || !result) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    lgss_telemetry_event_t event;
    lgss_telemetry_severity_t severity;

    switch (result->action) {
        case SAFETY_ACTION_DENY:
            event = LGSS_TELEM_ACTION_DENIED;
            severity = LGSS_TELEM_SEVERITY_WARNING;
            break;
        case SAFETY_ACTION_ESCALATE:
            event = LGSS_TELEM_ACTION_ESCALATED;
            severity = LGSS_TELEM_SEVERITY_WARNING;
            break;
        case SAFETY_ACTION_LOG:
            event = LGSS_TELEM_ACTION_LOGGED;
            severity = LGSS_TELEM_SEVERITY_INFO;
            break;
        case SAFETY_ACTION_WARN:
            event = LGSS_TELEM_ACTION_WARNED;
            severity = LGSS_TELEM_SEVERITY_INFO;
            break;
        default:
            event = LGSS_TELEM_ACTION_ALLOWED;
            severity = LGSS_TELEM_SEVERITY_DEBUG;
            break;
    }

    /* Create detailed entry */
    lgss_telemetry_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.sequence = ++telemetry->sequence;
    entry.timestamp_us = nimcp_time_now_us();
    entry.event_type = event;
    entry.severity = severity;
    entry.action = result->action;
    entry.confidence = result->confidence;
    entry.eval_time_us = result->evaluation_time_us;

    if (source) {
        strncpy(entry.source, source, LGSS_TELEMETRY_MAX_SOURCE - 1);
    }

    /* Include rule ID if triggered */
    if (result->num_triggered > 0 && result->triggered_rule_ids) {
        entry.rule_id = result->triggered_rule_ids[0];
    }

    /* Format description */
    snprintf(entry.description, LGSS_TELEMETRY_MAX_DESC,
        "Action: %s, Confidence: %.2f, Time: %lu us, %s",
        safety_action_name(result->action),
        result->confidence,
        result->evaluation_time_us,
        result->explanation);

    /* Add to buffer */
    add_entry_to_buffer(telemetry, &entry);

    /* Write to file */
    write_entry_to_file(telemetry, &entry);

    /* Invoke callback */
    invoke_callback(telemetry, &entry);

    /* Update stats */
    telemetry->stats.entries_logged++;
    telemetry->stats.newest_entry_timestamp = entry.timestamp_us;

    return 0;
}

int lgss_telemetry_log_rule_trigger(
    lgss_telemetry_t* telemetry,
    const safety_rule_t* rule,
    const safety_action_context_t* context)
{
    if (!telemetry || !rule) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    return lgss_telemetry_log(telemetry,
        LGSS_TELEM_RULE_TRIGGERED,
        LGSS_TELEM_SEVERITY_INFO,
        "SAFETY_KB",
        "Rule triggered: %s (id=%u, domain=%s, action=%s)",
        rule->name,
        rule->rule_id,
        safety_domain_name(rule->domain),
        safety_action_name(rule->action));
}

int lgss_telemetry_log_override(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    uint64_t command_id,
    const char* approver,
    const char* reason)
{
    if (!telemetry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "telemetry is NULL");

        return NIMCP_ERROR_NULL_POINTER;
    }

    return lgss_telemetry_log(telemetry,
        event,
        LGSS_TELEM_SEVERITY_WARNING,
        "OVERRIDE",
        "Override command %lu: approver=%s, reason=%s",
        command_id,
        approver ? approver : "N/A",
        reason ? reason : "N/A");
}

int lgss_telemetry_log_system(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    const char* description)
{
    if (!telemetry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "telemetry is NULL");

        return NIMCP_ERROR_NULL_POINTER;
    }

    lgss_telemetry_severity_t severity = LGSS_TELEM_SEVERITY_INFO;

    if (event == LGSS_TELEM_SYSTEM_HALT ||
        event == LGSS_TELEM_TAMPERING_DETECTED ||
        event == LGSS_TELEM_INTEGRITY_FAILED) {
        severity = LGSS_TELEM_SEVERITY_CRITICAL;
    }

    return lgss_telemetry_log(telemetry, event, severity, "SYSTEM", "%s",
        description ? description : "");
}

/*=============================================================================
 * QUERY FUNCTIONS
 *============================================================================*/

int lgss_telemetry_get_recent(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_entry_t* entries,
    size_t max_entries,
    size_t offset)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC || !entries) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (!telemetry->buffer.entries || telemetry->buffer.count == 0) {
        return 0;
    }

    size_t available = telemetry->buffer.count;
    if (offset >= available) {
        return 0;
    }

    size_t to_copy = available - offset;
    if (to_copy > max_entries) {
        to_copy = max_entries;
    }

    /* Copy from ring buffer (most recent first) */
    for (size_t i = 0; i < to_copy; i++) {
        size_t idx = (telemetry->buffer.head + telemetry->buffer.capacity - 1 - offset - i) %
            telemetry->buffer.capacity;
        memcpy(&entries[i], &telemetry->buffer.entries[idx],
            sizeof(lgss_telemetry_entry_t));
    }

    return (int)to_copy;
}

int lgss_telemetry_query_by_type(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event_type,
    lgss_telemetry_entry_t* entries,
    size_t max_entries)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC || !entries) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (!telemetry->buffer.entries) {
        return 0;
    }

    size_t found = 0;
    for (size_t i = 0; i < telemetry->buffer.count && found < max_entries; i++) {
        size_t idx = (telemetry->buffer.head + telemetry->buffer.capacity - 1 - i) %
            telemetry->buffer.capacity;

        if (telemetry->buffer.entries[idx].event_type == event_type) {
            memcpy(&entries[found], &telemetry->buffer.entries[idx],
                sizeof(lgss_telemetry_entry_t));
            found++;
        }
    }

    return (int)found;
}

int lgss_telemetry_query_by_time(
    lgss_telemetry_t* telemetry,
    uint64_t start_us,
    uint64_t end_us,
    lgss_telemetry_entry_t* entries,
    size_t max_entries)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC || !entries) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (!telemetry->buffer.entries) {
        return 0;
    }

    size_t found = 0;
    for (size_t i = 0; i < telemetry->buffer.count && found < max_entries; i++) {
        size_t idx = (telemetry->buffer.head + telemetry->buffer.capacity - 1 - i) %
            telemetry->buffer.capacity;

        uint64_t ts = telemetry->buffer.entries[idx].timestamp_us;
        if (ts >= start_us && ts <= end_us) {
            memcpy(&entries[found], &telemetry->buffer.entries[idx],
                sizeof(lgss_telemetry_entry_t));
            found++;
        }
    }

    return (int)found;
}

/*=============================================================================
 * VERIFICATION FUNCTIONS
 *============================================================================*/

int lgss_telemetry_verify_chain(lgss_telemetry_t* telemetry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!telemetry->buffer.entries || telemetry->buffer.count < 2) {
        return 0; /* Not enough entries to verify */
    }

    telemetry->stats.chain_verifications++;

    /* Verify chain from oldest to newest */
    for (size_t i = 1; i < telemetry->buffer.count; i++) {
        size_t prev_idx = (telemetry->buffer.head + telemetry->buffer.capacity - telemetry->buffer.count + i - 1) %
            telemetry->buffer.capacity;
        size_t curr_idx = (prev_idx + 1) % telemetry->buffer.capacity;

        uint8_t expected_hash[LGSS_TELEMETRY_HASH_SIZE];
        compute_entry_hash(&telemetry->buffer.entries[prev_idx], expected_hash);

        if (memcmp(telemetry->buffer.entries[curr_idx].prev_hash,
                   expected_hash, LGSS_TELEMETRY_HASH_SIZE) != 0) {
            telemetry->stats.chain_failures++;
            telemetry->stats.chain_valid = false;
            TELEM_LOG_ERROR("Chain verification failed at entry %zu", i);
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    telemetry->stats.chain_valid = true;
    return 0;
}

int lgss_telemetry_find_chain_break(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_entry_t* entry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_telemetry_find_chain_break: required parameter is NULL (telemetry, entry)");
        return -1;
    }

    if (!telemetry->buffer.entries || telemetry->buffer.count < 2) {
        return 1; /* No break found (not enough entries) */
    }

    for (size_t i = 1; i < telemetry->buffer.count; i++) {
        size_t prev_idx = (telemetry->buffer.head + telemetry->buffer.capacity - telemetry->buffer.count + i - 1) %
            telemetry->buffer.capacity;
        size_t curr_idx = (prev_idx + 1) % telemetry->buffer.capacity;

        uint8_t expected_hash[LGSS_TELEMETRY_HASH_SIZE];
        compute_entry_hash(&telemetry->buffer.entries[prev_idx], expected_hash);

        if (memcmp(telemetry->buffer.entries[curr_idx].prev_hash,
                   expected_hash, LGSS_TELEMETRY_HASH_SIZE) != 0) {
            memcpy(entry, &telemetry->buffer.entries[curr_idx],
                sizeof(lgss_telemetry_entry_t));
            return 0; /* Break found */
        }
    }

    return 1; /* No break found */
}

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

int lgss_telemetry_get_stats(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_stats_t* stats)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &telemetry->stats, sizeof(lgss_telemetry_stats_t));
    return 0;
}

int lgss_telemetry_flush(lgss_telemetry_t* telemetry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (telemetry->log_file) {
        fflush(telemetry->log_file);
        telemetry->stats.flush_count++;
    }

    return 0;
}

int lgss_telemetry_clear(lgss_telemetry_t* telemetry)
{
    if (!telemetry || telemetry->magic != NIMCP_LGSS_TELEMETRY_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    TELEM_LOG_WARN("Clearing telemetry buffer (audit trail destroyed)");

    if (telemetry->buffer.entries) {
        memset(telemetry->buffer.entries, 0,
            telemetry->buffer.capacity * sizeof(lgss_telemetry_entry_t));
        telemetry->buffer.head = 0;
        telemetry->buffer.count = 0;
    }

    telemetry->sequence = 0;
    telemetry->has_last_hash = false;
    memset(telemetry->last_hash, 0, LGSS_TELEMETRY_HASH_SIZE);
    memset(&telemetry->stats, 0, sizeof(lgss_telemetry_stats_t));
    telemetry->stats.chain_valid = true;

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* lgss_telemetry_event_name(lgss_telemetry_event_t event)
{
    switch (event) {
        case LGSS_TELEM_EVALUATION_START:    return "EVAL_START";
        case LGSS_TELEM_EVALUATION_COMPLETE: return "EVAL_COMPLETE";
        case LGSS_TELEM_EVALUATION_ERROR:    return "EVAL_ERROR";
        case LGSS_TELEM_EVALUATION_TIMEOUT:  return "EVAL_TIMEOUT";
        case LGSS_TELEM_ACTION_ALLOWED:      return "ACTION_ALLOWED";
        case LGSS_TELEM_ACTION_DENIED:       return "ACTION_DENIED";
        case LGSS_TELEM_ACTION_ESCALATED:    return "ACTION_ESCALATED";
        case LGSS_TELEM_ACTION_LOGGED:       return "ACTION_LOGGED";
        case LGSS_TELEM_ACTION_WARNED:       return "ACTION_WARNED";
        case LGSS_TELEM_RULE_TRIGGERED:      return "RULE_TRIGGERED";
        case LGSS_TELEM_RULE_LOADED:         return "RULE_LOADED";
        case LGSS_TELEM_RULE_COMPILED:       return "RULE_COMPILED";
        case LGSS_TELEM_KB_CREATED:          return "KB_CREATED";
        case LGSS_TELEM_KB_LOCKED:           return "KB_LOCKED";
        case LGSS_TELEM_KB_DESTROYED:        return "KB_DESTROYED";
        case LGSS_TELEM_INTEGRITY_VERIFIED:  return "INTEGRITY_OK";
        case LGSS_TELEM_INTEGRITY_FAILED:    return "INTEGRITY_FAIL";
        case LGSS_TELEM_OVERRIDE_REQUESTED:  return "OVERRIDE_REQ";
        case LGSS_TELEM_OVERRIDE_APPROVED:   return "OVERRIDE_APPROVED";
        case LGSS_TELEM_OVERRIDE_REJECTED:   return "OVERRIDE_REJECTED";
        case LGSS_TELEM_OVERRIDE_EXECUTED:   return "OVERRIDE_EXEC";
        case LGSS_TELEM_ESCALATION_CREATED:  return "ESCALATION_CREATED";
        case LGSS_TELEM_ESCALATION_RESOLVED: return "ESCALATION_RESOLVED";
        case LGSS_TELEM_ESCALATION_EXPIRED:  return "ESCALATION_EXPIRED";
        case LGSS_TELEM_SYSTEM_START:        return "SYSTEM_START";
        case LGSS_TELEM_SYSTEM_STOP:         return "SYSTEM_STOP";
        case LGSS_TELEM_SYSTEM_HALT:         return "SYSTEM_HALT";
        case LGSS_TELEM_SYSTEM_RESET:        return "SYSTEM_RESET";
        case LGSS_TELEM_SAFETY_VIOLATION:    return "SAFETY_VIOLATION";
        case LGSS_TELEM_TAMPERING_DETECTED:  return "TAMPERING";
        case LGSS_TELEM_DECEPTION_DETECTED:  return "DECEPTION";
        case LGSS_TELEM_NEUROMOD_SIGNAL:     return "NEUROMOD_SIGNAL";
        case LGSS_TELEM_PLASTICITY_UPDATE:   return "PLASTICITY_UPDATE";
        default:                             return "UNKNOWN";
    }
}

const char* lgss_telemetry_severity_name(lgss_telemetry_severity_t severity)
{
    switch (severity) {
        case LGSS_TELEM_SEVERITY_DEBUG:    return "DEBUG";
        case LGSS_TELEM_SEVERITY_INFO:     return "INFO";
        case LGSS_TELEM_SEVERITY_WARNING:  return "WARN";
        case LGSS_TELEM_SEVERITY_ERROR:    return "ERROR";
        case LGSS_TELEM_SEVERITY_CRITICAL: return "CRIT";
        default:                           return "?";
    }
}

int lgss_telemetry_format_entry(
    const lgss_telemetry_entry_t* entry,
    char* buffer,
    size_t buffer_size)
{
    if (!entry || !buffer || buffer_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return snprintf(buffer, buffer_size,
        "[%lu] %lu %s/%s [%s] %s",
        entry->sequence,
        entry->timestamp_us,
        lgss_telemetry_severity_name(entry->severity),
        lgss_telemetry_event_name(entry->event_type),
        entry->source,
        entry->description);
}
