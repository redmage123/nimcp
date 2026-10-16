// nimcp_logging_part_lifecycle.c - lifecycle functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


/**
 * WHAT: Free memory allocated with log_alloc
 */
static void log_free(void* ptr) {
    if (!ptr) return;
    log_alloc_header_t* header = (log_alloc_header_t*)((char*)ptr - sizeof(log_alloc_header_t));
    if (header->handle) {
        unified_mem_free(header->handle);
    } else {
        nimcp_free(header);
    }
}


//=============================================================================
// Ring Buffer Implementation
//=============================================================================

/**
 * WHAT: Initialize ring buffer
 */
static bool ring_buffer_init(log_ring_buffer_t* rb, size_t capacity, nimcp_logger_t logger) {
    rb->entries = (nimcp_log_entry_t*)log_alloc(logger, capacity * sizeof(nimcp_log_entry_t));
    if (!rb->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ring_buffer_init: rb->entries is NULL");
        return false;
    }

    rb->capacity = capacity;
    nimcp_atomic_init_u64(&rb->write_pos, 0);
    nimcp_atomic_init_u64(&rb->read_pos, 0);
    nimcp_atomic_init_u64(&rb->drop_count, 0);

    memset(rb->entries, 0, capacity * sizeof(nimcp_log_entry_t));
    return true;
}


/**
 * WHAT: Destroy ring buffer
 *
 * Safety: zero capacity BEFORE freeing entries. ring_buffer_pop's
 * teardown guard checks (entries == NULL || capacity == 0) — clearing
 * capacity first means any concurrent pop sees an empty buffer rather
 * than racing with the free. The async writer is joined before this is
 * called under normal lifecycle, but crash-driven shutdown paths can
 * land here while the writer is still draining.
 */
static void ring_buffer_destroy(log_ring_buffer_t* rb) {
    if (!rb) return;
    rb->capacity = 0;  /* fence: pop now treats buffer as empty */
    if (rb->entries) {
        log_free(rb->entries);
        rb->entries = NULL;
    }
}


//=============================================================================
// Rate Limiter Implementation
//=============================================================================

/**
 * WHAT: Initialize rate limiter
 */
static void rate_limiter_init(log_rate_limiter_t* rl, const nimcp_log_rate_limit_config_t* config) {
    rl->enabled = config->enabled;
    rl->max_tokens = config->burst_size > 0 ? config->burst_size : config->max_per_second;
    rl->refill_rate = config->max_per_second;
    nimcp_atomic_init_u64(&rl->tokens, rl->max_tokens);
    nimcp_atomic_init_u64(&rl->last_refill_time_ns, get_time_ns());
}


nimcp_logger_t nimcp_log_create(const nimcp_log_config_t* config) {
    nimcp_log_config_t cfg = config ? *config : nimcp_log_default_config();

    // Allocate logger (can't use unified memory yet as it's not initialized)
    nimcp_logger_t logger = (nimcp_logger_t)nimcp_malloc(sizeof(struct nimcp_logger_struct));
    if (!logger) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logger is NULL");

        return NULL;
    }

    memset(logger, 0, sizeof(struct nimcp_logger_struct));
    logger->magic = NIMCP_LOG_MAGIC;

    // Store configuration
    logger->level = cfg.level;

    // Environment variable override: NIMCP_LOG_LEVEL=debug|trace|info|warn|error
    const char* env_level = getenv("NIMCP_LOG_LEVEL");
    if (env_level) {
        if (strcasecmp(env_level, "trace") == 0)      logger->level = LOG_LEVEL_TRACE;
        else if (strcasecmp(env_level, "debug") == 0)  logger->level = LOG_LEVEL_DEBUG;
        else if (strcasecmp(env_level, "info") == 0)   logger->level = LOG_LEVEL_INFO;
        else if (strcasecmp(env_level, "warn") == 0)   logger->level = LOG_LEVEL_WARN;
        else if (strcasecmp(env_level, "error") == 0)  logger->level = LOG_LEVEL_ERROR;
        else if (strcasecmp(env_level, "off") == 0)    logger->level = LOG_LEVEL_OFF;
    }

    logger->destinations = cfg.destinations;
    logger->format = cfg.format;
    logger->async_mode = cfg.async_mode;
    logger->color_mode = cfg.color_mode;
    logger->include_source_location = cfg.include_source_location;
    logger->flush_interval_ms = cfg.flush_interval_ms > 0 ? cfg.flush_interval_ms : NIMCP_LOG_DEFAULT_FLUSH_INTERVAL_MS;

    // Copy rotation config
    logger->rotation = cfg.rotation;
    logger->last_rotation_time = time(NULL);

    // Initialize rate limiter
    rate_limiter_init(&logger->rate_limiter, &cfg.rate_limit);

    // Initialize mutexes
    if (nimcp_platform_mutex_init(&logger->file_mutex, false) != 0 ||
        nimcp_platform_mutex_init(&logger->filter_mutex, false) != 0 ||
        nimcp_platform_mutex_init(&logger->async_mutex, false) != 0 ||
        nimcp_platform_mutex_init(&logger->context_mutex, false) != 0) {
        nimcp_free(logger);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_log_create: validation failed");
        return NULL;
    }

    if (nimcp_platform_cond_init(&logger->async_cond) != 0) {
        nimcp_platform_mutex_destroy(&logger->file_mutex);
        nimcp_platform_mutex_destroy(&logger->filter_mutex);
        nimcp_platform_mutex_destroy(&logger->async_mutex);
        nimcp_platform_mutex_destroy(&logger->context_mutex);
        nimcp_free(logger);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_log_create: validation failed");
        return NULL;
    }

    // Store integrations
    logger->memory_mgr = (unified_mem_manager_t)cfg.memory_manager;
    logger->security_ctx = (nimcp_sec_integration_t*)cfg.security_context;

    // Callbacks
    logger->callback = cfg.callback;
    logger->callback_user_data = cfg.callback_user_data;
    logger->filter = cfg.filter;
    logger->filter_user_data = cfg.filter_user_data;

    // Initialize atomic counters
    nimcp_atomic_init_u64(&logger->messages_logged, 0);
    nimcp_atomic_init_u64(&logger->messages_dropped, 0);
    nimcp_atomic_init_u64(&logger->messages_filtered, 0);
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        nimcp_atomic_init_u64(&logger->level_counts[i], 0);
    }
    nimcp_atomic_init_u64(&logger->async_writes, 0);
    nimcp_atomic_init_u64(&logger->sync_writes, 0);
    nimcp_atomic_init_u64(&logger->buffer_overflows, 0);
    nimcp_atomic_init_u64(&logger->flush_operations, 0);
    nimcp_atomic_init_u64(&logger->rotations_performed, 0);
    nimcp_atomic_init_u64(&logger->bytes_written, 0);
    nimcp_atomic_init_u64(&logger->rate_limit_hits, 0);
    nimcp_atomic_init_u64(&logger->total_log_time_ns, 0);
    nimcp_atomic_init_u64(&logger->max_log_time_ns, 0);
    nimcp_atomic_init_u32(&logger->sequence, 0);
    nimcp_atomic_init_bool(&logger->async_running, false);

    logger->start_time_ns = get_time_ns();

    // Open log file if file destination enabled
    if (logger->destinations & NIMCP_LOG_DEST_FILE) {
        const char* path = cfg.file_path ? cfg.file_path : DEFAULT_LOG_PATH;
        snprintf(logger->file_path, sizeof(logger->file_path), "%s", path);

        // Create directory
        char dir[PATH_MAX];
        get_directory(path, dir, sizeof(dir));
        mkdir_p(dir);

        logger->file = fopen(path, cfg.append_mode ? "a" : "w");
        if (!logger->file) {
            // Fallback to /tmp
            snprintf(logger->file_path, sizeof(logger->file_path), "/tmp/nimcp.log");
            logger->file = fopen(logger->file_path, cfg.append_mode ? "a" : "w");
        }

        if (logger->file) {
            // Get current file size
            fseek(logger->file, 0, SEEK_END);
            logger->current_file_size = (size_t)ftell(logger->file);
        }
    }

    // Initialize async components
    if (logger->async_mode != NIMCP_LOG_ASYNC_OFF) {
        size_t buffer_size = cfg.buffer_size > 0 ? cfg.buffer_size : NIMCP_LOG_DEFAULT_BUFFER_SIZE;
        if (!ring_buffer_init(&logger->ring_buffer, buffer_size, logger)) {
            // Fall back to sync mode
            logger->async_mode = NIMCP_LOG_ASYNC_OFF;
        } else {
            // Start writer thread
            nimcp_atomic_store_bool(&logger->async_running, true, NIMCP_MEMORY_ORDER_RELEASE);
            if (nimcp_platform_thread_create(&logger->writer_thread, async_writer_thread, logger) != 0) {
                ring_buffer_destroy(&logger->ring_buffer);
                logger->async_mode = NIMCP_LOG_ASYNC_OFF;
                nimcp_atomic_store_bool(&logger->async_running, false, NIMCP_MEMORY_ORDER_RELEASE);
            }
        }
    }

    // Register with security module
    if (logger->security_ctx) {
        nimcp_sec_register_module(logger->security_ctx, LOG_MODULE_NAME,
                                  NIMCP_SEC_CAT_UTILITY, &logger->security_module_id);
    }

    return logger;
}


void nimcp_log_destroy(nimcp_logger_t logger) {
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        return;
    }

    // Stop async writer
    if (nimcp_atomic_load_bool(&logger->async_running, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        nimcp_atomic_store_bool(&logger->async_running, false, NIMCP_MEMORY_ORDER_RELEASE);

        // Wake up writer thread
        nimcp_platform_mutex_lock(&logger->async_mutex);
        nimcp_platform_cond_signal(&logger->async_cond);
        nimcp_platform_mutex_unlock(&logger->async_mutex);

        // Wait for thread to exit
        nimcp_platform_thread_join(logger->writer_thread, NULL);
    }

    // Destroy ring buffer
    ring_buffer_destroy(&logger->ring_buffer);

    // Unregister from security
    if (logger->security_ctx && logger->security_module_id != 0) {
        nimcp_sec_unregister_module(logger->security_ctx, logger->security_module_id);
    }

    // Close syslog
    if (logger->syslog_opened) {
#ifdef __linux__
        closelog();
#endif
    }

    // Close file
    if (logger->file) {
        fflush(logger->file);
        fclose(logger->file);
    }

    // Destroy mutexes and condition
    nimcp_platform_cond_destroy(&logger->async_cond);
    nimcp_platform_mutex_destroy(&logger->async_mutex);
    nimcp_platform_mutex_destroy(&logger->filter_mutex);
    nimcp_platform_mutex_destroy(&logger->file_mutex);
    nimcp_platform_mutex_destroy(&logger->context_mutex);

    // Invalidate and free
    logger->magic = 0;
    nimcp_free(logger);
}


bool nimcp_log_is_initialized(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    return logger && logger->magic == NIMCP_LOG_MAGIC;
}


//=============================================================================
// Global Logger Functions
//=============================================================================

int nimcp_log_init(const nimcp_log_config_t* config) {
    nimcp_platform_once(&g_global_once, init_global_mutex);
    nimcp_platform_mutex_lock(&g_global_mutex);

    if (g_global_initialized && g_global_logger) {
        nimcp_platform_mutex_unlock(&g_global_mutex);
        return 0;  // Already initialized
    }

    g_global_logger = nimcp_log_create(config);
    g_global_initialized = (g_global_logger != NULL);

    nimcp_platform_mutex_unlock(&g_global_mutex);
    return g_global_initialized ? 0 : -1;
}


void nimcp_log_shutdown(void) {
    nimcp_platform_once(&g_global_once, init_global_mutex);
    nimcp_platform_mutex_lock(&g_global_mutex);

    if (g_global_logger) {
        nimcp_log_destroy(g_global_logger);
        g_global_logger = NULL;
    }
    g_global_initialized = false;

    nimcp_platform_mutex_unlock(&g_global_mutex);
}


//=============================================================================
// Legacy API Implementation
//=============================================================================

void log_init(const char* log_file) {
    nimcp_log_config_t config = nimcp_log_default_config();
    config.file_path = log_file;
    config.async_mode = NIMCP_LOG_ASYNC_OFF;  // Legacy was sync
    config.destinations = NIMCP_LOG_DEST_FILE;
    nimcp_log_init(&config);
}


void log_close(void) {
    nimcp_log_shutdown();
}


//=============================================================================
// Context API Implementation
//=============================================================================

nimcp_log_context_t nimcp_log_context_create(nimcp_logger_t logger, const char* context_id) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_log_context_create: logger is NULL");
        return NULL;
    }

    nimcp_log_context_t ctx = (nimcp_log_context_t)log_alloc(logger,
                                                              sizeof(struct nimcp_log_context_struct));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    ctx->logger = logger;

    // Save previous context
    nimcp_platform_mutex_lock(&logger->context_mutex);
    snprintf(ctx->previous_context, sizeof(ctx->previous_context), "%s", logger->current_context);

    // Set new context
    if (context_id) {
        snprintf(ctx->context_id, sizeof(ctx->context_id), "%s", context_id);
    } else {
        // Auto-generate context ID
        snprintf(ctx->context_id, sizeof(ctx->context_id), "ctx-%u-%lu",
                 get_thread_id(), (unsigned long)get_time_ns() % 1000000);
    }
    snprintf(logger->current_context, sizeof(logger->current_context), "%s", ctx->context_id);
    nimcp_platform_mutex_unlock(&logger->context_mutex);

    return ctx;
}


void nimcp_log_context_destroy(nimcp_log_context_t context) {
    if (!context) return;

    nimcp_logger_t logger = context->logger;
    if (logger && logger->magic == NIMCP_LOG_MAGIC) {
        // Restore previous context
        nimcp_platform_mutex_lock(&logger->context_mutex);
        snprintf(logger->current_context, sizeof(logger->current_context),
                 "%s", context->previous_context);
        nimcp_platform_mutex_unlock(&logger->context_mutex);
    }

    log_free(context);
}


void nimcp_log_reset_rate_limit(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;
    nimcp_atomic_store_u64(&logger->rate_limiter.tokens, logger->rate_limiter.max_tokens,
                           NIMCP_MEMORY_ORDER_RELEASE);
}


void nimcp_log_reset_stats(nimcp_logger_t logger) {
    if (!logger) logger = g_global_logger;
    if (!logger || logger->magic != NIMCP_LOG_MAGIC) return;

    nimcp_atomic_store_u64(&logger->messages_logged, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->messages_dropped, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->messages_filtered, 0, NIMCP_MEMORY_ORDER_RELAXED);

    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        nimcp_atomic_store_u64(&logger->level_counts[i], 0, NIMCP_MEMORY_ORDER_RELAXED);
    }

    nimcp_atomic_store_u64(&logger->async_writes, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->sync_writes, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->buffer_overflows, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->flush_operations, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->rate_limit_hits, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->total_log_time_ns, 0, NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_store_u64(&logger->max_log_time_ns, 0, NIMCP_MEMORY_ORDER_RELAXED);

    logger->start_time_ns = get_time_ns();
}
