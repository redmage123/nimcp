// nimcp_logging_part_accessors.c - accessors functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_log_config_t nimcp_log_default_config(void) {
    nimcp_log_config_t config = {
        .level = LOG_LEVEL_INFO,
        .destinations = NIMCP_LOG_DEST_FILE | NIMCP_LOG_DEST_CONSOLE,
        .format = NIMCP_LOG_FORMAT_TEXT,
        .file_path = NULL,
        .append_mode = true,
        .async_mode = NIMCP_LOG_ASYNC_ON,
        .buffer_size = NIMCP_LOG_DEFAULT_BUFFER_SIZE,
        .flush_interval_ms = NIMCP_LOG_DEFAULT_FLUSH_INTERVAL_MS,
        .rotation = {
            .mode = NIMCP_LOG_ROTATE_SIZE,
            .max_file_size = NIMCP_LOG_DEFAULT_MAX_FILE_SIZE,
            .interval = NIMCP_LOG_ROTATE_DAILY,
            .max_rotated_files = NIMCP_LOG_DEFAULT_MAX_ROTATED_FILES,
            .compress_rotated = false
        },
        .rate_limit = {
            .enabled = true,
            .max_per_second = NIMCP_LOG_DEFAULT_RATE_LIMIT,
            .burst_size = NIMCP_LOG_DEFAULT_RATE_LIMIT * 2,
            .per_module = false
        },
        .color_mode = NIMCP_LOG_COLOR_AUTO,
        .callback = NULL,
        .callback_user_data = NULL,
        .filter = NULL,
        .filter_user_data = NULL,
        .include_source_location = true,
        .memory_manager = NULL,
        .security_context = NULL
    };
    return config;
}


nimcp_logger_t nimcp_log_get_global(void) {
    return g_global_logger;
}


void nimcp_log_set_global(nimcp_logger_t logger) {
    nimcp_platform_once(&g_global_once, init_global_mutex);
    nimcp_platform_mutex_lock(&g_global_mutex);
    g_global_logger = logger;
    g_global_initialized = (logger != NULL);
    nimcp_platform_mutex_unlock(&g_global_mutex);
}


//=============================================================================
// Configuration API Implementation
//=============================================================================

void nimcp_log_set_level(nimcp_logger_t logger, log_level_t level) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->level = level;
}


log_level_t nimcp_log_get_level(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return LOG_LEVEL_INFO;
    return logger->level;
}


bool nimcp_log_is_level_enabled(nimcp_logger_t logger, log_level_t level) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        return false;
    }
    return level >= logger->level && level < LOG_LEVEL_OFF;
}


void nimcp_log_set_destinations(nimcp_logger_t logger, uint32_t destinations) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->destinations = destinations;
}


void nimcp_log_set_format(nimcp_logger_t logger, nimcp_log_format_t format) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->format = format;
}


void nimcp_log_set_color_mode(nimcp_logger_t logger, nimcp_log_color_mode_t mode) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->color_mode = mode;
}


void nimcp_log_set_callback(
    nimcp_logger_t logger,
    nimcp_log_output_callback_t callback,
    void* user_data
) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->callback = callback;
    logger->callback_user_data = user_data;
}


void nimcp_log_set_filter(
    nimcp_logger_t logger,
    nimcp_log_filter_t filter,
    void* user_data
) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    logger->filter = filter;
    logger->filter_user_data = user_data;
}


const char* nimcp_log_get_context_id(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_log_get_context_id: logger is NULL");
        return NULL;
    }

    return logger->current_context[0] != '\0' ? logger->current_context : NULL;
}


void nimcp_log_set_context_id(nimcp_logger_t logger, const char* context_id) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;

    nimcp_platform_mutex_lock(&logger->context_mutex);
    if (context_id) {
        snprintf(logger->current_context, sizeof(logger->current_context), "%s", context_id);
    } else {
        logger->current_context[0] = '\0';
    }
    nimcp_platform_mutex_unlock(&logger->context_mutex);
}


void nimcp_log_set_rotation(nimcp_logger_t logger, const nimcp_log_rotation_config_t* config) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !config) return;
    logger->rotation = *config;
}


const char* nimcp_log_get_file_path(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_log_get_file_path: logger is NULL");
        return NULL;
    }
    return logger->file_path[0] ? logger->file_path : NULL;
}


int nimcp_log_set_file_path(nimcp_logger_t logger, const char* path) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_log_set_file_path: required parameter is NULL (logger, path)");
        return -1;
    }

    nimcp_platform_mutex_lock(&logger->file_mutex);

    // Close current file
    if (logger->file) {
        fflush(logger->file);
        fclose(logger->file);
    }

    // Update path
    snprintf(logger->file_path, sizeof(logger->file_path), "%s", path);

    // Create directory
    char dir[PATH_MAX];
    get_directory(path, dir, sizeof(dir));
    mkdir_p(dir);

    // Open new file
    logger->file = fopen(path, "a");
    if (logger->file) {
        fseek(logger->file, 0, SEEK_END);
        logger->current_file_size = (size_t)ftell(logger->file);
    }

    nimcp_platform_mutex_unlock(&logger->file_mutex);

    return logger->file ? 0 : -1;
}


//=============================================================================
// Rate Limiting API Implementation
//=============================================================================

void nimcp_log_set_rate_limit(nimcp_logger_t logger, const nimcp_log_rate_limit_config_t* config) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !config) return;
    rate_limiter_init(&logger->rate_limiter, config);
}


//=============================================================================
// Statistics API Implementation
//=============================================================================

int nimcp_log_get_stats(nimcp_logger_t logger, nimcp_log_stats_t* stats) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_log_get_stats: required parameter is NULL (logger, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(*stats));

    stats->messages_logged = nimcp_atomic_load_u64(&logger->messages_logged, NIMCP_MEMORY_ORDER_RELAXED);
    stats->messages_dropped = nimcp_atomic_load_u64(&logger->messages_dropped, NIMCP_MEMORY_ORDER_RELAXED);
    stats->messages_filtered = nimcp_atomic_load_u64(&logger->messages_filtered, NIMCP_MEMORY_ORDER_RELAXED);

    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        stats->level_counts[i] = nimcp_atomic_load_u64(&logger->level_counts[i], NIMCP_MEMORY_ORDER_RELAXED);
    }

    stats->async_writes = nimcp_atomic_load_u64(&logger->async_writes, NIMCP_MEMORY_ORDER_RELAXED);
    stats->sync_writes = nimcp_atomic_load_u64(&logger->sync_writes, NIMCP_MEMORY_ORDER_RELAXED);
    stats->buffer_overflows = nimcp_atomic_load_u64(&logger->buffer_overflows, NIMCP_MEMORY_ORDER_RELAXED);
    stats->flush_operations = nimcp_atomic_load_u64(&logger->flush_operations, NIMCP_MEMORY_ORDER_RELAXED);

    stats->rotations_performed = nimcp_atomic_load_u64(&logger->rotations_performed, NIMCP_MEMORY_ORDER_RELAXED);
    stats->bytes_written = nimcp_atomic_load_u64(&logger->bytes_written, NIMCP_MEMORY_ORDER_RELAXED);
    stats->current_file_size = logger->current_file_size;

    stats->rate_limit_hits = nimcp_atomic_load_u64(&logger->rate_limit_hits, NIMCP_MEMORY_ORDER_RELAXED);

    stats->total_log_time_ns = nimcp_atomic_load_u64(&logger->total_log_time_ns, NIMCP_MEMORY_ORDER_RELAXED);
    stats->max_log_time_ns = nimcp_atomic_load_u64(&logger->max_log_time_ns, NIMCP_MEMORY_ORDER_RELAXED);
    if (stats->messages_logged > 0) {
        stats->avg_log_time_ns = stats->total_log_time_ns / stats->messages_logged;
    }

    // Buffer utilization
    if (logger->ring_buffer.entries) {
        size_t used = ring_buffer_size(&logger->ring_buffer);
        stats->buffer_utilization = (used * 100) / logger->ring_buffer.capacity;
        stats->memory_used = logger->ring_buffer.capacity * sizeof(nimcp_log_entry_t);
    }

    stats->uptime_ns = get_time_ns() - logger->start_time_ns;

    return 0;
}


uint32_t nimcp_log_get_security_id(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return 0;
    return logger->security_module_id;
}


bool nimcp_log_is_tty(void) {
    return isatty(STDERR_FILENO) != 0;
}
