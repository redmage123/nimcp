// nimcp_logging_part_io.c - io functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


//=============================================================================
// Async Writer Thread
//=============================================================================

/**
 * WHAT: Background writer thread function
 * WHY:  Process async log messages without blocking producers
 */
static void* async_writer_thread(void* arg) {
    nimcp_logger_t logger = (nimcp_logger_t)arg;
    nimcp_log_entry_t entry;

    while (nimcp_atomic_load_bool(&logger->async_running, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bool wrote = false;

        // Process all available entries
        while (ring_buffer_pop(&logger->ring_buffer, &entry)) {
            write_entry(logger, &entry);
            nimcp_atomic_fetch_add_u64(&logger->async_writes, 1, NIMCP_MEMORY_ORDER_RELAXED);
            wrote = true;
        }

        if (wrote) {
            nimcp_atomic_fetch_add_u64(&logger->flush_operations, 1, NIMCP_MEMORY_ORDER_RELAXED);
        }

        // Wait for more entries or timeout
        nimcp_platform_mutex_lock(&logger->async_mutex);
        if (nimcp_atomic_load_bool(&logger->async_running, NIMCP_MEMORY_ORDER_ACQUIRE)) {
            nimcp_platform_cond_timedwait(&logger->async_cond, &logger->async_mutex,
                                          logger->flush_interval_ms);
        }
        nimcp_platform_mutex_unlock(&logger->async_mutex);
    }

    // Flush remaining entries on shutdown
    while (ring_buffer_pop(&logger->ring_buffer, &entry)) {
        write_entry(logger, &entry);
    }

    /* P2-U10: Worker thread shutdown is normal behavior, not an error.
     * Removed false positive NIMCP_THROW_TO_IMMUNE. */
    return NULL;
}


//=============================================================================
// Core Logging API Implementation
//=============================================================================

void nimcp_log_write(
    nimcp_logger_t logger,
    log_level_t level,
    const char* module,
    const char* file,
    int line,
    const char* format,
    ...
) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(logger, level, module, file, line, format, args);
    va_end(args);
}


void nimcp_log_writev(
    nimcp_logger_t logger,
    log_level_t level,
    const char* module,
    const char* file,
    int line,
    const char* format,
    va_list args
) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        // Fallback to stderr if logger not initialized
        if (level >= LOG_LEVEL_WARN) {
            vfprintf(stderr, format, args);
            fprintf(stderr, "\n");
        }
        return;
    }

    // Check if should log
    if (!should_log(logger, level, module)) {
        return;
    }

    uint64_t start_time = get_time_ns();

    // Build log entry
    nimcp_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.level = level;
    entry.timestamp_ns = get_wall_time_ns();
    entry.thread_id = get_thread_id();
    entry.line = line;

    if (module) {
        snprintf(entry.module, sizeof(entry.module), "%s", module);
    }

    if (file) {
        snprintf(entry.file, sizeof(entry.file), "%s", file);
    }

    // Get context ID
    nimcp_platform_mutex_lock(&logger->context_mutex);
    if (logger->current_context[0] != '\0') {
        snprintf(entry.context_id, sizeof(entry.context_id), "%s", logger->current_context);
    }
    nimcp_platform_mutex_unlock(&logger->context_mutex);

    // Format message
    vsnprintf(entry.message, sizeof(entry.message), format, args);

    // Process entry
    process_entry(logger, &entry);

    // Update timing stats
    uint64_t elapsed = get_time_ns() - start_time;
    nimcp_atomic_fetch_add_u64(&logger->total_log_time_ns, elapsed, NIMCP_MEMORY_ORDER_RELAXED);

    uint64_t current_max = nimcp_atomic_load_u64(&logger->max_log_time_ns, NIMCP_MEMORY_ORDER_RELAXED);
    while (elapsed > current_max) {
        if (nimcp_atomic_compare_exchange_u64(&logger->max_log_time_ns, &current_max, elapsed,
                                               NIMCP_MEMORY_ORDER_ACQ_REL)) {
            break;
        }
    }
}


const char* nimcp_log_format_name(nimcp_log_format_t format) {
    switch (format) {
        case NIMCP_LOG_FORMAT_TEXT: return "text";
        case NIMCP_LOG_FORMAT_JSON: return "json";
        case NIMCP_LOG_FORMAT_COMPACT: return "compact";
        case NIMCP_LOG_FORMAT_SYSLOG: return "syslog";
        default: return "unknown";
    }
}
