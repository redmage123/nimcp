// nimcp_logging_part_stats.c - stats functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


void nimcp_log_flush(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;

    // Signal async writer to flush
    if (nimcp_atomic_load_bool(&logger->async_running, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        nimcp_platform_mutex_lock(&logger->async_mutex);
        nimcp_platform_cond_signal(&logger->async_cond);
        nimcp_platform_mutex_unlock(&logger->async_mutex);

        // Wait for buffer to drain with proper sleep
        int max_wait_cycles = 1000;  // Max 1 second wait
        while (ring_buffer_size(&logger->ring_buffer) > 0 && max_wait_cycles > 0) {
            struct timespec ts = {0, 1000000};  // 1ms
            nanosleep(&ts, NULL);
            max_wait_cycles--;
        }

        // Give async writer time to finish writing current batch
        struct timespec ts = {0, 5000000};  // 5ms
        nanosleep(&ts, NULL);
    }

    // Flush file
    if (logger->file) {
        nimcp_platform_mutex_lock(&logger->file_mutex);
        fflush(logger->file);
        nimcp_platform_mutex_unlock(&logger->file_mutex);
    }

    nimcp_atomic_fetch_add_u64(&logger->flush_operations, 1, NIMCP_MEMORY_ORDER_RELAXED);
}


void nimcp_log_entry(nimcp_logger_t logger, const nimcp_log_entry_t* entry) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !entry) {
        return;
    }

    nimcp_log_entry_t entry_copy = *entry;
    process_entry(logger, &entry_copy);
}


//=============================================================================
// Module Filtering API Implementation
//=============================================================================

void nimcp_log_enable_module(nimcp_logger_t logger, const char* module, log_level_t level) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !module) return;

    nimcp_platform_mutex_lock(&logger->filter_mutex);

    // Check if already exists
    for (size_t i = 0; i < logger->module_filter_count; i++) {
        if (strcmp(logger->module_filters[i].module, module) == 0) {
            logger->module_filters[i].level = level;
            logger->module_filters[i].enabled = true;
            nimcp_platform_mutex_unlock(&logger->filter_mutex);
            return;
        }
    }

    // Add new filter
    if (logger->module_filter_count < MAX_MODULE_FILTERS) {
        snprintf(logger->module_filters[logger->module_filter_count].module,
                 sizeof(logger->module_filters[0].module), "%s", module);
        logger->module_filters[logger->module_filter_count].level = level;
        logger->module_filters[logger->module_filter_count].enabled = true;
        logger->module_filter_count++;
    }

    nimcp_platform_mutex_unlock(&logger->filter_mutex);
}


void nimcp_log_disable_module(nimcp_logger_t logger, const char* module) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC || !module) return;

    nimcp_platform_mutex_lock(&logger->filter_mutex);

    for (size_t i = 0; i < logger->module_filter_count; i++) {
        if (strcmp(logger->module_filters[i].module, module) == 0) {
            logger->module_filters[i].enabled = false;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&logger->filter_mutex);
}


void nimcp_log_clear_module_filters(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;

    nimcp_platform_mutex_lock(&logger->filter_mutex);
    logger->module_filter_count = 0;
    nimcp_platform_mutex_unlock(&logger->filter_mutex);
}


//=============================================================================
// Rotation API Implementation
//=============================================================================

int nimcp_log_rotate(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_log_rotate: logger is NULL");
        return -1;
    }
    if (!logger->file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_log_rotate: logger->file is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(&logger->file_mutex);

    // Close current file
    fflush(logger->file);
    fclose(logger->file);
    logger->file = NULL;

    // Rotate files (rename current to .1, .1 to .2, etc.)
    char rotated_path[PATH_MAX + 16];
    char prev_path[PATH_MAX + 16];

    // Remove oldest
    snprintf(rotated_path, sizeof(rotated_path), "%s.%u",
             logger->file_path, logger->rotation.max_rotated_files);
    unlink(rotated_path);

    // Shift existing rotated files
    for (uint32_t i = logger->rotation.max_rotated_files; i > 1; i--) {
        snprintf(prev_path, sizeof(prev_path), "%s.%u", logger->file_path, i - 1);
        snprintf(rotated_path, sizeof(rotated_path), "%s.%u", logger->file_path, i);
        rename(prev_path, rotated_path);
    }

    // Rename current to .1
    snprintf(rotated_path, sizeof(rotated_path), "%s.1", logger->file_path);
    rename(logger->file_path, rotated_path);

    // Open new file
    logger->file = fopen(logger->file_path, "w");
    logger->current_file_size = 0;
    logger->last_rotation_time = time(NULL);

    nimcp_atomic_fetch_add_u64(&logger->rotations_performed, 1, NIMCP_MEMORY_ORDER_RELAXED);

    nimcp_platform_mutex_unlock(&logger->file_mutex);

    return logger->file ? 0 : -1;
}


//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* nimcp_log_level_name(log_level_t level) {
    if (level >= 0 && level < LOG_LEVEL_COUNT) {
        return LEVEL_NAMES[level];
    }
    return "UNKNOWN";
}


log_level_t nimcp_log_level_from_string(const char* name) {
    if (!name) return LOG_LEVEL_INFO;

    // Case-insensitive comparison
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        if (strcasecmp(name, LEVEL_NAMES[i]) == 0) {
            return (log_level_t)i;
        }
    }

    // Handle common aliases
    if (strcasecmp(name, "WARNING") == 0) {
        return LOG_LEVEL_WARN;
    }

    return LOG_LEVEL_INFO;
}
