// nimcp_logging_part_helpers.c - helpers functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


/**
 * WHAT: One-time global mutex initialization
 * WHY:  Ensure thread-safe initialization of global logger mutex
 */
static void init_global_mutex(void) {
    nimcp_platform_mutex_init(&g_global_mutex, false);
}


//=============================================================================
// Memory Allocation Helpers
//=============================================================================

/**
 * WHAT: Allocate memory with unified memory support
 * WHY:  Use memory pool when available, fall back to malloc
 */
static void* log_alloc(nimcp_logger_t logger, size_t size) {
    if (logger && logger->memory_mgr) {
        size_t total = sizeof(log_alloc_header_t) + size;
        unified_mem_request_t req = {
            .size = total,
            .initial_data = NULL,
            .strategy = 3,  // UNIFIED_STRATEGY_POOL_DIRECT
            .enable_cow = false,
            .alignment = 0
        };
        unified_mem_handle_t handle = unified_mem_alloc(logger->memory_mgr, &req);
        if (handle) {
            void* base = unified_mem_write(handle);
            if (base) {
                log_alloc_header_t* header = (log_alloc_header_t*)base;
                header->handle = handle;
                header->size = size;
                return (char*)base + sizeof(log_alloc_header_t);
            }
            unified_mem_free(handle);
        }
    }

    // Fallback to malloc
    size_t total = sizeof(log_alloc_header_t) + size;
    void* base = nimcp_malloc(total);
    if (base) {
        log_alloc_header_t* header = (log_alloc_header_t*)base;
        header->handle = NULL;
        header->size = size;
        return (char*)base + sizeof(log_alloc_header_t);
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "log_alloc: validation failed");
    return NULL;
}


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Get current thread ID
 */
static uint32_t get_thread_id(void) {
#ifdef __linux__
    return (uint32_t)syscall(SYS_gettid);
#else
    return (uint32_t)(uintptr_t)pthread_self();
#endif
}


/**
 * WHAT: Get monotonic time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    return nimcp_platform_time_monotonic_ms() * 1000000ULL;
}


/**
 * WHAT: Get wall clock time in nanoseconds
 */
static uint64_t get_wall_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}


/**
 * WHAT: Create directory and all parent directories
 * WHY:  mkdir() only creates one level, we need recursive creation
 */
static int mkdir_p(const char* path) {
    char tmp[PATH_MAX];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;  // Permission denied or other error — caller has fallback
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;  // Permission denied or other error — caller has fallback
    }
    return 0;
}


/**
 * WHAT: Extract directory from file path
 */
static void get_directory(const char* path, char* dir, size_t dir_size) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char* d = dirname(tmp);
    snprintf(dir, dir_size, "%s", d);
}


/**
 * WHAT: Get basename from file path
 */
static const char* get_basename(const char* path) {
    const char* base = strrchr(path, '/');
    return base ? base + 1 : path;
}


/**
 * WHAT: Push entry to ring buffer (producer)
 * WHY:  Lock-free push for async logging
 * HOW:  CAS on write position
 */
static bool ring_buffer_push(log_ring_buffer_t* rb, const nimcp_log_entry_t* entry) {
    uint64_t write_pos, read_pos, next_pos;

    /* Teardown safety — symmetric with ring_buffer_pop. ring_buffer_destroy
     * zeroes capacity before freeing entries, so any push that arrives
     * mid-shutdown takes the "buffer full" branch and falls back to sync
     * writes (handled by the caller). */
    if (!rb || !rb->entries || rb->capacity == 0) {
        return false;
    }

    // Try to reserve a slot
    for (int retry = 0; retry < 3; retry++) {
        write_pos = nimcp_atomic_load_u64(&rb->write_pos, NIMCP_MEMORY_ORDER_ACQUIRE);
        read_pos = nimcp_atomic_load_u64(&rb->read_pos, NIMCP_MEMORY_ORDER_ACQUIRE);
        next_pos = write_pos + 1;

        // Check if buffer is full
        if (next_pos - read_pos >= rb->capacity) {
            nimcp_atomic_fetch_add_u64(&rb->drop_count, 1, NIMCP_MEMORY_ORDER_RELAXED);
            /* Buffer full is a normal condition in a bounded ring buffer.
             * Caller (process_entry) handles this by falling back to sync writes. */
            return false;  // Buffer full
        }

        // Try to claim the slot
        if (nimcp_atomic_compare_exchange_u64(&rb->write_pos, &write_pos, next_pos,
                                               NIMCP_MEMORY_ORDER_ACQ_REL)) {
            // Successfully claimed slot, copy entry
            size_t index = write_pos % rb->capacity;
            memcpy(&rb->entries[index], entry, sizeof(nimcp_log_entry_t));
            return true;
        }
        // CAS failed, retry
    }

    nimcp_atomic_fetch_add_u64(&rb->drop_count, 1, NIMCP_MEMORY_ORDER_RELAXED);
    /* CAS retry exhaustion under contention is normal, not an error.
     * Caller handles gracefully by falling back to sync writes. */
    return false;
}


/**
 * WHAT: Pop entry from ring buffer (consumer)
 * WHY:  Single consumer, no CAS needed
 */
static bool ring_buffer_pop(log_ring_buffer_t* rb, nimcp_log_entry_t* entry) {
    /* Teardown safety: ring_buffer_destroy() sets rb->entries=NULL after
     * freeing the entry array. If the async writer thread is mid-pop when
     * destroy fires (which it shouldn't be under normal lifecycle, but a
     * crash-handler-driven shutdown can land here), we'd memcpy from a
     * NULL pointer and SIGSEGV. The pointer + capacity check is a cheap
     * fence — both must be valid OR we treat the buffer as empty. */
    if (!rb || !rb->entries || rb->capacity == 0) {
        return false;
    }

    uint64_t read_pos = nimcp_atomic_load_u64(&rb->read_pos, NIMCP_MEMORY_ORDER_ACQUIRE);
    uint64_t write_pos = nimcp_atomic_load_u64(&rb->write_pos, NIMCP_MEMORY_ORDER_ACQUIRE);

    if (read_pos >= write_pos) {
        /* P2-U7: Empty buffer is a normal condition (polled by async writer), not an error.
         * Removed false positive NIMCP_THROW_TO_IMMUNE. */
        return false;  // Buffer empty
    }

    size_t index = read_pos % rb->capacity;
    memcpy(entry, &rb->entries[index], sizeof(nimcp_log_entry_t));

    nimcp_atomic_store_u64(&rb->read_pos, read_pos + 1, NIMCP_MEMORY_ORDER_RELEASE);
    return true;
}


/**
 * WHAT: Get number of entries in ring buffer
 */
static size_t ring_buffer_size(log_ring_buffer_t* rb) {
    uint64_t write_pos = nimcp_atomic_load_u64(&rb->write_pos, NIMCP_MEMORY_ORDER_ACQUIRE);
    uint64_t read_pos = nimcp_atomic_load_u64(&rb->read_pos, NIMCP_MEMORY_ORDER_ACQUIRE);
    return (size_t)(write_pos - read_pos);
}


/**
 * WHAT: Try to consume a token
 * WHY:  Token bucket rate limiting
 * HOW:  Refill tokens based on elapsed time, then try to consume
 */
static bool rate_limiter_try_acquire(log_rate_limiter_t* rl) {
    if (!rl->enabled) {
        return true;
    }

    uint64_t now = get_time_ns();
    uint64_t last_refill = nimcp_atomic_load_u64(&rl->last_refill_time_ns, NIMCP_MEMORY_ORDER_ACQUIRE);
    uint64_t elapsed_ns = now - last_refill;

    // Refill tokens (1 token per 1/rate seconds)
    if (elapsed_ns > 0 && rl->refill_rate > 0) {
        uint64_t tokens_to_add = (elapsed_ns * rl->refill_rate) / 1000000000ULL;
        if (tokens_to_add > 0) {
            uint64_t expected = nimcp_atomic_load_u64(&rl->tokens, NIMCP_MEMORY_ORDER_ACQUIRE);
            uint64_t desired;
            do {
                desired = expected + tokens_to_add;
                if (desired > rl->max_tokens) {
                    desired = rl->max_tokens;
                }
            } while (!nimcp_atomic_compare_exchange_u64(&rl->tokens, &expected, desired, NIMCP_MEMORY_ORDER_ACQ_REL));

            nimcp_atomic_store_u64(&rl->last_refill_time_ns, now, NIMCP_MEMORY_ORDER_RELEASE);
        }
    }

    // P2-U11: Use CAS loop for atomic token consumption to prevent over-consumption
    // under concurrent access. The previous load-then-sub was not atomic and could
    // allow more threads to consume tokens than available.
    {
        uint64_t current = nimcp_atomic_load_u64(&rl->tokens, NIMCP_MEMORY_ORDER_ACQUIRE);
        while (current > 0) {
            if (nimcp_atomic_compare_exchange_u64(&rl->tokens, &current, current - 1,
                                                   NIMCP_MEMORY_ORDER_ACQ_REL)) {
                return true;
            }
            // CAS failed, current updated by compare_exchange; retry
        }
    }

    /* P2-U9: Rate limiter depleted is normal throttling behavior, not an error.
     * Removed false positive NIMCP_THROW_TO_IMMUNE. */
    return false;
}


//=============================================================================
// Formatter Implementation
//=============================================================================

/**
 * WHAT: Format timestamp
 */
static void format_timestamp(uint64_t timestamp_ns, char* buf, size_t buf_size, bool include_ms) {
    time_t seconds = (time_t)(timestamp_ns / 1000000000ULL);
    struct tm tm_info;
    localtime_r(&seconds, &tm_info);

    if (include_ms) {
        uint32_t ms = (uint32_t)((timestamp_ns % 1000000000ULL) / 1000000ULL);
        snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d.%03u",
                 tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
                 tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms);
    } else {
        strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_info);
    }
}


/**
 * WHAT: Format log entry as text
 */
static size_t format_text(const nimcp_log_entry_t* entry, char* buf, size_t buf_size,
                          bool include_source, bool color, bool compact) {
    char timestamp[32];
    const char* color_start = "";
    const char* color_end = "";

    if (color && entry->level < LOG_LEVEL_COUNT) {
        color_start = ANSI_COLORS[entry->level];
        color_end = ANSI_RESET;
    }

    if (compact) {
        // Compact format: [L] message
        return snprintf(buf, buf_size, "%s[%s]%s %s\n",
                        color_start, LEVEL_NAMES_SHORT[entry->level], color_end,
                        entry->message);
    }

    format_timestamp(entry->timestamp_ns, timestamp, sizeof(timestamp), true);

    size_t len = 0;

    // Base format: [timestamp] [LEVEL] message
    if (entry->module[0] != '\0') {
        if (include_source && entry->file[0] != '\0') {
            len = snprintf(buf, buf_size, "[%s] %s[%-5s]%s [%s] [%s:%d] %s\n",
                           timestamp, color_start, LEVEL_NAMES[entry->level], color_end,
                           entry->module, get_basename(entry->file), entry->line,
                           entry->message);
        } else {
            len = snprintf(buf, buf_size, "[%s] %s[%-5s]%s [%s] %s\n",
                           timestamp, color_start, LEVEL_NAMES[entry->level], color_end,
                           entry->module, entry->message);
        }
    } else {
        if (include_source && entry->file[0] != '\0') {
            len = snprintf(buf, buf_size, "[%s] %s[%-5s]%s [%s:%d] %s\n",
                           timestamp, color_start, LEVEL_NAMES[entry->level], color_end,
                           get_basename(entry->file), entry->line, entry->message);
        } else {
            len = snprintf(buf, buf_size, "[%s] %s[%-5s]%s %s\n",
                           timestamp, color_start, LEVEL_NAMES[entry->level], color_end,
                           entry->message);
        }
    }

    return len;
}


/**
 * WHAT: Format log entry as JSON
 */
static size_t format_json(const nimcp_log_entry_t* entry, char* buf, size_t buf_size) {
    char timestamp[32];
    format_timestamp(entry->timestamp_ns, timestamp, sizeof(timestamp), true);

    // Escape message for JSON
    char escaped_msg[NIMCP_LOG_MAX_MESSAGE_LEN * 2];
    const char* src = entry->message;
    char* dst = escaped_msg;
    char* end = escaped_msg + sizeof(escaped_msg) - 1;

    while (*src && dst < end) {
        switch (*src) {
            case '"': if (dst + 2 <= end) { *dst++ = '\\'; *dst++ = '"'; } break;
            case '\\': if (dst + 2 <= end) { *dst++ = '\\'; *dst++ = '\\'; } break;
            case '\n': if (dst + 2 <= end) { *dst++ = '\\'; *dst++ = 'n'; } break;
            case '\r': if (dst + 2 <= end) { *dst++ = '\\'; *dst++ = 'r'; } break;
            case '\t': if (dst + 2 <= end) { *dst++ = '\\'; *dst++ = 't'; } break;
            default: *dst++ = *src; break;
        }
        src++;
    }
    *dst = '\0';

    return snprintf(buf, buf_size,
        "{\"timestamp\":\"%s\",\"level\":\"%s\",\"thread\":%u,"
        "\"module\":\"%s\",\"file\":\"%s\",\"line\":%d,"
        "\"context\":\"%s\",\"seq\":%u,\"message\":\"%s\"}\n",
        timestamp, LEVEL_NAMES[entry->level], entry->thread_id,
        entry->module, entry->file[0] ? get_basename(entry->file) : "",
        entry->line, entry->context_id, entry->sequence, escaped_msg);
}


/**
 * WHAT: Format log entry for syslog
 */
static size_t format_syslog(const nimcp_log_entry_t* entry, char* buf, size_t buf_size) {
    // Syslog format: <priority>timestamp hostname app[pid]: message
    return snprintf(buf, buf_size, "%s[%u]: %s\n",
                    entry->module[0] ? entry->module : "nimcp",
                    entry->thread_id, entry->message);
}


//=============================================================================
// Output Destination Implementation
//=============================================================================

/**
 * WHAT: Write entry to file
 */
static void write_to_file(nimcp_logger_t logger, const nimcp_log_entry_t* entry,
                          const char* formatted, size_t len) {
    if (!logger->file) return;

    nimcp_platform_mutex_lock(&logger->file_mutex);

    size_t written = fwrite(formatted, 1, len, logger->file);
    fflush(logger->file);

    if (written > 0) {
        logger->current_file_size += written;
        nimcp_atomic_fetch_add_u64(&logger->bytes_written, written, NIMCP_MEMORY_ORDER_RELAXED);
    }

    // Check for rotation
    if (logger->rotation.mode != NIMCP_LOG_ROTATE_NONE) {
        bool should_rotate = false;

        if (logger->rotation.mode == NIMCP_LOG_ROTATE_SIZE ||
            logger->rotation.mode == NIMCP_LOG_ROTATE_BOTH) {
            if (logger->current_file_size >= logger->rotation.max_file_size) {
                should_rotate = true;
            }
        }

        if (logger->rotation.mode == NIMCP_LOG_ROTATE_TIME ||
            logger->rotation.mode == NIMCP_LOG_ROTATE_BOTH) {
            time_t now = time(NULL);
            time_t interval = 0;
            switch (logger->rotation.interval) {
                case NIMCP_LOG_ROTATE_HOURLY: interval = 3600; break;
                case NIMCP_LOG_ROTATE_DAILY: interval = 86400; break;
                case NIMCP_LOG_ROTATE_WEEKLY: interval = 604800; break;
                case NIMCP_LOG_ROTATE_MONTHLY: interval = 2592000; break;
            }
            if (interval > 0 && (now - logger->last_rotation_time) >= interval) {
                should_rotate = true;
            }
        }

        if (should_rotate) {
            nimcp_platform_mutex_unlock(&logger->file_mutex);
            nimcp_log_rotate(logger);
            return;
        }
    }

    nimcp_platform_mutex_unlock(&logger->file_mutex);
}


/**
 * WHAT: Write entry to console
 */
static void write_to_console(nimcp_logger_t logger, const nimcp_log_entry_t* entry,
                             bool use_color) {
    char buf[NIMCP_LOG_MAX_MESSAGE_LEN + 256];
    size_t len = format_text(entry, buf, sizeof(buf),
                             logger->include_source_location, use_color, false);

    // Use stderr for WARN and above, stdout for others
    FILE* out = (entry->level >= LOG_LEVEL_WARN) ? stderr : stdout;
    fwrite(buf, 1, len, out);
    fflush(out);
}


/**
 * WHAT: Write entry to syslog
 */
static void write_to_syslog(nimcp_logger_t logger, const nimcp_log_entry_t* entry) {
#ifdef __linux__
    if (!logger->syslog_opened) {
        openlog("nimcp", LOG_PID | LOG_NDELAY, LOG_USER);
        logger->syslog_opened = true;
    }

    int priority;
    switch (entry->level) {
        case LOG_LEVEL_TRACE:
        case LOG_LEVEL_DEBUG: priority = LOG_DEBUG; break;
        case LOG_LEVEL_INFO: priority = LOG_INFO; break;
        case LOG_LEVEL_WARN: priority = LOG_WARNING; break;
        case LOG_LEVEL_ERROR: priority = LOG_ERR; break;
        case LOG_LEVEL_FATAL: priority = LOG_CRIT; break;
        default: priority = LOG_INFO; break;
    }

    syslog(priority, "[%s] %s", entry->module[0] ? entry->module : "nimcp", entry->message);
#else
    (void)logger;
    (void)entry;
#endif
}


/**
 * WHAT: Write entry to callback
 */
static void write_to_callback(nimcp_logger_t logger, const nimcp_log_entry_t* entry) {
    if (logger->callback) {
        logger->callback(
            entry->level,
            entry->timestamp_ns / 1000000000ULL,
            entry->module[0] ? entry->module : NULL,
            entry->file[0] ? entry->file : NULL,
            entry->line,
            entry->message,
            logger->callback_user_data
        );
    }
}


/**
 * WHAT: Write entry to all configured destinations
 */
static void write_entry(nimcp_logger_t logger, const nimcp_log_entry_t* entry) {
    char formatted[NIMCP_LOG_MAX_MESSAGE_LEN + 512];
    size_t len = 0;

    // Determine color support
    bool use_color = false;
    if (logger->color_mode == NIMCP_LOG_COLOR_ON) {
        use_color = true;
    } else if (logger->color_mode == NIMCP_LOG_COLOR_AUTO) {
        use_color = nimcp_log_is_tty();
    }

    // Format message based on format type
    switch (logger->format) {
        case NIMCP_LOG_FORMAT_JSON:
            len = format_json(entry, formatted, sizeof(formatted));
            break;
        case NIMCP_LOG_FORMAT_COMPACT:
            len = format_text(entry, formatted, sizeof(formatted),
                              false, use_color, true);
            break;
        case NIMCP_LOG_FORMAT_SYSLOG:
            len = format_syslog(entry, formatted, sizeof(formatted));
            break;
        case NIMCP_LOG_FORMAT_TEXT:
        default:
            len = format_text(entry, formatted, sizeof(formatted),
                              logger->include_source_location, false, false);
            break;
    }

    // Write to file
    if (logger->destinations & NIMCP_LOG_DEST_FILE) {
        write_to_file(logger, entry, formatted, len);
    }

    // Write to console
    if (logger->destinations & NIMCP_LOG_DEST_CONSOLE) {
        write_to_console(logger, entry, use_color);
    }

    // Write to syslog
    if (logger->destinations & NIMCP_LOG_DEST_SYSLOG) {
        write_to_syslog(logger, entry);
    }

    // Write to callback
    if (logger->destinations & NIMCP_LOG_DEST_CALLBACK) {
        write_to_callback(logger, entry);
    }
}


//=============================================================================
// Core Logging Functions
//=============================================================================

/**
 * WHAT: Check if message should be logged (level + module filter)
 */
static bool should_log(nimcp_logger_t logger, log_level_t level, const char* module) {
    // Check OFF level (never log)
    if (level >= LOG_LEVEL_OFF) {
        /* P2-U8: Level being OFF is a normal configuration, not an error.
         * Removed false positive NIMCP_THROW_TO_IMMUNE. */
        return false;
    }

    // Check module-specific filter first (takes precedence over global level)
    if (module && module[0] != '\0' && logger->module_filter_count > 0) {
        nimcp_platform_mutex_lock(&logger->filter_mutex);
        for (size_t i = 0; i < logger->module_filter_count; i++) {
            if (strcmp(logger->module_filters[i].module, module) == 0) {
                bool result = logger->module_filters[i].enabled &&
                              level >= logger->module_filters[i].level;
                nimcp_platform_mutex_unlock(&logger->filter_mutex);
                return result;
            }
        }
        nimcp_platform_mutex_unlock(&logger->filter_mutex);
    }

    // Check global level
    if (level < logger->level) {
        /* P2-U8: Message below configured log level is a normal filter, not an error.
         * Removed false positive NIMCP_THROW_TO_IMMUNE. */
        return false;
    }

    return true;
}


/**
 * WHAT: Process a log entry
 */
static void process_entry(nimcp_logger_t logger, nimcp_log_entry_t* entry) {
    // Apply custom filter
    if (logger->filter) {
        if (!logger->filter(entry->level, entry->module, entry->message, logger->filter_user_data)) {
            nimcp_atomic_fetch_add_u64(&logger->messages_filtered, 1, NIMCP_MEMORY_ORDER_RELAXED);
            return;
        }
    }

    // Apply rate limiting (bypass for ERROR and FATAL - critical messages must always be logged)
    if (entry->level < LOG_LEVEL_ERROR && !rate_limiter_try_acquire(&logger->rate_limiter)) {
        nimcp_atomic_fetch_add_u64(&logger->rate_limit_hits, 1, NIMCP_MEMORY_ORDER_RELAXED);
        nimcp_atomic_fetch_add_u64(&logger->messages_dropped, 1, NIMCP_MEMORY_ORDER_RELAXED);
        return;
    }

    // Add sequence number
    entry->sequence = nimcp_atomic_fetch_add_u32(&logger->sequence, 1, NIMCP_MEMORY_ORDER_RELAXED);

    // Update statistics
    nimcp_atomic_fetch_add_u64(&logger->messages_logged, 1, NIMCP_MEMORY_ORDER_RELAXED);
    if (entry->level < LOG_LEVEL_COUNT) {
        nimcp_atomic_fetch_add_u64(&logger->level_counts[entry->level], 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    // Route to async or sync path
    bool use_async = (logger->async_mode == NIMCP_LOG_ASYNC_ON) ||
                     (logger->async_mode == NIMCP_LOG_ASYNC_HYBRID &&
                      entry->level < LOG_LEVEL_ERROR);

    if (use_async && logger->ring_buffer.entries) {
        if (!ring_buffer_push(&logger->ring_buffer, entry)) {
            // Buffer full, fall back to sync
            nimcp_atomic_fetch_add_u64(&logger->buffer_overflows, 1, NIMCP_MEMORY_ORDER_RELAXED);
            write_entry(logger, entry);
            nimcp_atomic_fetch_add_u64(&logger->sync_writes, 1, NIMCP_MEMORY_ORDER_RELAXED);
        } else {
            // Signal writer thread
            nimcp_platform_mutex_lock(&logger->async_mutex);
            nimcp_platform_cond_signal(&logger->async_cond);
            nimcp_platform_mutex_unlock(&logger->async_mutex);
        }
    } else {
        write_entry(logger, entry);
        nimcp_atomic_fetch_add_u64(&logger->sync_writes, 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    // Record security interaction
    if (logger->security_ctx && logger->security_module_id != 0) {
        nimcp_sec_record_interaction(logger->security_ctx, logger->security_module_id, true, 0.1);
    }
}
