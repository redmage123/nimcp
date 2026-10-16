// nimcp_bio_router_part_lifecycle.c - lifecycle functions
// Part of nimcp_bio_router.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_bio_router.c


/*=============================================================================
 * MESSAGE QUEUE OPERATIONS
 *============================================================================*/

/**
 * WHAT: Initialize message queue
 * WHY:  Setup inbox/outbox for module
 * HOW:  Allocate ring buffer, initialize mutex and condvars
 */
static nimcp_error_t bio_msg_queue_init(bio_msg_queue_t* queue, uint32_t capacity) {
    NIMCP_CHECK_THROW(queue && capacity > 0, NIMCP_ERROR_INVALID_PARAM,
                      "bio_msg_queue_init: queue is NULL or capacity is 0");

    queue->entries = nimcp_calloc(capacity, sizeof(bio_msg_queue_entry_t));
    NIMCP_CHECK_THROW(queue->entries != NULL, NIMCP_ERROR_NO_MEMORY,
                      "bio_msg_queue_init: failed to allocate queue entries");

    queue->capacity = capacity;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->count = 0;

    if (nimcp_platform_mutex_init(&queue->mutex, false) != 0) {
        nimcp_free(queue->entries);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                          "bio_msg_queue_init: failed to init queue mutex");
    }

    if (nimcp_platform_cond_init(&queue->not_empty) != 0) {
        nimcp_platform_mutex_destroy(&queue->mutex);
        nimcp_free(queue->entries);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                          "bio_msg_queue_init: failed to init not_empty cond");
    }

    if (nimcp_platform_cond_init(&queue->not_full) != 0) {
        nimcp_platform_cond_destroy(&queue->not_empty);
        nimcp_platform_mutex_destroy(&queue->mutex);
        nimcp_free(queue->entries);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MUTEX_INIT,
                          "bio_msg_queue_init: failed to init not_full cond");
    }

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Destroy message queue
 * WHY:  Free resources on module unregister
 * HOW:  Free pending messages, destroy synchronization primitives
 * NOTE: Promises are NOT destroyed here - they are owned by the caller of
 *       bio_router_send_async and must be destroyed by that caller even if
 *       the message was never processed.
 */
static void bio_msg_queue_destroy(bio_msg_queue_t* queue) {
    if (!queue || !queue->entries) return;

    // Count pending messages for warning
    uint32_t pending_with_promises = 0;

    // Free all pending messages (but NOT the promises - caller owns those)
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t idx = (queue->read_idx + i) % queue->capacity;
        if (queue->entries[idx].msg_data) {
            nimcp_free(queue->entries[idx].msg_data);
        }
        if (queue->entries[idx].response_promise) {
            // DO NOT destroy the promise - caller of bio_router_send_async owns it
            // Just log a warning that there are unprocessed async messages
            pending_with_promises++;
        }
    }

    if (pending_with_promises > 0) {
        LOG_WARNING("bio_msg_queue_destroy: %u pending messages with response promises "
                    "will not have their handlers invoked. Caller must still destroy "
                    "the promises.", pending_with_promises);
    }

    nimcp_platform_cond_destroy(&queue->not_full);
    nimcp_platform_cond_destroy(&queue->not_empty);
    nimcp_platform_mutex_destroy(&queue->mutex);
    nimcp_free(queue->entries);

    memset(queue, 0, sizeof(*queue));
}


nimcp_error_t bio_router_init(const bio_router_config_t* config) {
    // Initialize global init mutex once (thread-safe)
    // WHAT: Use pthread_once to guarantee mutex initialization happens exactly once
    // WHY:  Fixes TOCTOU race where multiple threads could both check
    //       g_router_init_mutex_initialized == false and both try to initialize
    // HOW:  pthread_once guarantees init_router_mutex_once() executes exactly once
    nimcp_platform_once(&g_router_init_once, init_router_mutex_once);

    nimcp_platform_mutex_lock(&g_router_init_mutex);

    // Check if already initialized
    if (g_router != NULL) {
        nimcp_platform_mutex_unlock(&g_router_init_mutex);
        LOG_WARN("Bio-router already initialized");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS,
                          "bio_router_init: already initialized");
    }

    // Use defaults if no config provided
    bio_router_config_t cfg = config ? *config : bio_router_default_config();

    // WHAT: Ensure bio-async is initialized before router
    // WHY:  Router uses bio-async promises; predictive models require bio-async
    // HOW:  Initialize bio-async with defaults if not already done
    if (!nimcp_bio_async_is_initialized()) {
        nimcp_error_t bio_err = nimcp_bio_async_init(NULL);  // Use defaults
        if (bio_err != NIMCP_SUCCESS) {
            nimcp_platform_mutex_unlock(&g_router_init_mutex);
            LOG_WARN("Bio-async initialization failed (code %d), router continues without predictive coding", bio_err);
            // Non-fatal: router can work without full bio-async
        } else {
            LOG_INFO("Bio-async auto-initialized by bio-router");
        }
    }

    // Allocate router - nimcp_calloc zeroes all pointers for safe cleanup
    bool modules_mutex_init = false;
    bool stats_mutex_init = false;

    g_router = nimcp_calloc(1, sizeof(struct bio_router_struct));
    if (!g_router) {
        nimcp_platform_mutex_unlock(&g_router_init_mutex);
        LOG_ERROR("Failed to allocate bio-router");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY,
                          "bio_router_init: failed to allocate router");
    }

    g_router->magic = BIO_ROUTER_MAGIC;
    g_router->config = cfg;
    g_router->module_capacity = cfg.max_modules;

    // Allocate module registry
    g_router->modules = nimcp_calloc(cfg.max_modules, sizeof(bio_module_entry_t));
    if (!g_router->modules) goto router_cleanup;

    // Initialize mutexes
    if (nimcp_platform_mutex_init(&g_router->modules_mutex, false) != 0) goto router_cleanup;
    modules_mutex_init = true;

    if (nimcp_platform_mutex_init(&g_router->stats_mutex, false) != 0) goto router_cleanup;
    stats_mutex_init = true;

    // Initialize unified memory if requested
    if (cfg.max_message_size > 0) {
        unified_mem_config_t mem_cfg = unified_mem_default_config();
        mem_cfg.enable_cow = false;  // Direct allocation for messages
        mem_cfg.object_pool_num_blocks = cfg.max_modules * cfg.inbox_capacity;
        g_router->mem_mgr = unified_mem_create(&mem_cfg);
    }

    // Initialize predictive protocol if enabled
    if (cfg.enable_predictive_protocol) {
        predictive_config_t pred_cfg = predictive_protocol_default_config();
        g_router->predictive_proto = predictive_protocol_create(&pred_cfg);
        if (!g_router->predictive_proto) {
            LOG_WARN("Failed to initialize predictive protocol, continuing without it");
        } else {
            LOG_INFO("Predictive protocol enabled (cache_size=%u, min_confidence=%.2f)",
                     pred_cfg.cache_size, pred_cfg.min_confidence);
        }
    }

    // Initialize immune integration fields
    g_router->brain_immune_system = NULL;
    g_router->immune_ctx = NULL;

    g_router->initialized = true;
    g_router->shutdown_requested = false;

    nimcp_platform_mutex_unlock(&g_router_init_mutex);

    LOG_INFO("Bio-router initialized (max_modules=%u, inbox_capacity=%u, predictive=%s)",
             cfg.max_modules, cfg.inbox_capacity,
             cfg.enable_predictive_protocol ? "enabled" : "disabled");

    return NIMCP_SUCCESS;

router_cleanup:
    LOG_ERROR("Failed to initialize bio-router resources");
    if (stats_mutex_init) {
        nimcp_platform_mutex_destroy(&g_router->stats_mutex);
    }
    if (modules_mutex_init) {
        nimcp_platform_mutex_destroy(&g_router->modules_mutex);
    }
    nimcp_free(g_router->modules);
    nimcp_free(g_router);
    g_router = NULL;
    nimcp_platform_mutex_unlock(&g_router_init_mutex);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY,
                      "bio_router_init: resource allocation failed");
}


void bio_router_shutdown(void) {
    if (!g_router) return;

    LOG_INFO("Shutting down bio-router");

    // DEADLOCK FIX: Set shutdown flag first, then acquire lock to ensure
    // all operations see the flag before we start cleanup.
    // Use volatile-style memory barrier semantics via the mutex.
    nimcp_platform_mutex_lock(&g_router->modules_mutex);
    g_router->shutdown_requested = true;

    // Wake up any threads blocked on queue condition variables.
    // This prevents deadlock where a thread is waiting on a full/empty queue
    // while we're trying to destroy those queues.
    for (uint32_t i = 0; i < g_router->module_count; i++) {
        bio_module_entry_t* entry = &g_router->modules[i];
        if (entry->magic == BIO_MODULE_MAGIC) {
            // Signal all waiters to wake up and check shutdown flag
            nimcp_platform_mutex_lock(&entry->inbox.mutex);
            nimcp_platform_cond_broadcast(&entry->inbox.not_empty);
            nimcp_platform_cond_broadcast(&entry->inbox.not_full);
            nimcp_platform_mutex_unlock(&entry->inbox.mutex);
        }
    }
    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    // P2 note: Brief yield to allow blocked threads to wake and exit.
    // This is a best-effort drain approach. Proper shutdown would use a
    // thread barrier or condition variable, but the shutdown_requested flag
    // + cond_broadcast above ensures all blocked threads will eventually wake
    // and check the flag. The 1ms sleep is a pragmatic tradeoff.
    nimcp_platform_sleep_ms(1);  // 1ms

    // DEADLOCK FIX: Clear immune context reference BEFORE unregistering.
    // This prevents the unregister call from racing with other code
    // that might check immune_ctx.
    bio_module_context_t immune_ctx_to_cleanup = NULL;
    nimcp_platform_mutex_lock(&g_router->modules_mutex);
    if (g_router->immune_ctx) {
        immune_ctx_to_cleanup = g_router->immune_ctx;
        g_router->immune_ctx = NULL;
        g_router->brain_immune_system = NULL;
    }
    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    // Cleanup immune integration if connected (outside lock to avoid deadlock)
    if (immune_ctx_to_cleanup) {
        bio_router_unregister_module(immune_ctx_to_cleanup);
    }

    // Unregister all remaining modules
    // DEADLOCK FIX: Hold lock for entire cleanup to prevent races
    nimcp_platform_mutex_lock(&g_router->modules_mutex);
    for (uint32_t i = 0; i < g_router->module_count; i++) {
        bio_module_entry_t* entry = &g_router->modules[i];
        if (entry->magic == BIO_MODULE_MAGIC) {
            bio_msg_queue_destroy(&entry->inbox);
            nimcp_platform_mutex_destroy(&entry->handler_mutex);
            entry->magic = 0;
        }
    }
    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    // Destroy predictive protocol
    if (g_router->predictive_proto) {
        prefetch_result_t stats;
        if (predictive_protocol_get_stats(g_router->predictive_proto, &stats) == 0) {
            LOG_INFO("Predictive protocol stats: predictions=%lu, hits=%lu, misses=%lu, hit_rate=%.1f%%, wasted=%lu",
                     stats.predictions_made, stats.cache_hits, stats.cache_misses,
                     stats.hit_rate * 100.0F, stats.wasted_prefetches);
        }
        predictive_protocol_destroy(g_router->predictive_proto);
        g_router->predictive_proto = NULL;
    }

    // Destroy memory manager
    if (g_router->mem_mgr) {
        unified_mem_destroy(g_router->mem_mgr);
    }

    // Destroy mutexes
    nimcp_platform_mutex_destroy(&g_router->stats_mutex);
    nimcp_platform_mutex_destroy(&g_router->modules_mutex);

    // Free module registry
    nimcp_free(g_router->modules);

    // Free router
    nimcp_free(g_router);
    g_router = NULL;

    // Clear orchestrator reference to prevent use-after-free
    // (orchestrator may have been destroyed separately)
    g_router_orchestrator = NULL;

    // Clear brain KG reference
    g_router_brain_kg = NULL;

    /* RACE FIX: Clear the brain KG mutex "open for business" flag so no new
     * readers enter get_router_brain_kg_safe(). We do NOT destroy the mutex
     * because other threads may still be blocked on nimcp_platform_mutex_lock()
     * inside get_router_brain_kg_safe(). Destroying a mutex while threads are
     * blocked on it is undefined behavior (hangs on Linux/glibc). The mutex
     * persists for the process lifetime (~40 bytes); init_router_mutex_once()
     * guards against double-initialization via g_router_brain_kg_mutex_created.
     *
     * Sequence: lock -> set flag false -> unlock. All blocked readers will
     * then wake one by one, see flag=false in the re-check, and return NULL. */
    if (atomic_load_explicit(&g_router_brain_kg_mutex_initialized, memory_order_acquire)) {
        nimcp_platform_mutex_lock(&g_router_brain_kg_mutex);
        atomic_store_explicit(&g_router_brain_kg_mutex_initialized, false, memory_order_release);
        nimcp_platform_mutex_unlock(&g_router_brain_kg_mutex);
        /* Do NOT destroy - blocked readers still need it to wake and drain */
    }

    /* P1-1 fix: Reset subsystem once-flags so they re-initialize after shutdown */
    bio_router_reset_subsystem_statics();

    /* P2-54 fix: Reset platform_once so bio-router can be re-initialized after shutdown */
    g_router_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;

    LOG_INFO("Bio-router shutdown complete");
}


bool bio_router_is_initialized(void) {
    return g_router != NULL && g_router->initialized;
}


void bio_router_reset_stats(void) {
    if (!g_router) return;

    // Count active modules before reset (need to preserve this)
    nimcp_platform_mutex_lock(&g_router->modules_mutex);
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < g_router->module_count; i++) {
        if (g_router->modules[i].magic == BIO_MODULE_MAGIC) {
            active_count++;
        }
    }
    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    // Reset stats but preserve active module count
    nimcp_platform_mutex_lock(&g_router->stats_mutex);
    memset(&g_router->stats, 0, sizeof(g_router->stats));
    g_router->stats.active_modules = active_count;
    nimcp_platform_mutex_unlock(&g_router->stats_mutex);

    LOG_DEBUG("Bio-router statistics reset (active_modules=%u preserved)", active_count);
}


nimcp_glial_wave_t bio_router_initiate_wave(bio_module_context_t ctx,
                                             float intensity,
                                             const void* metadata) {
    if (!ctx || intensity <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_wave_mutex_once: ctx is NULL");
        return NULL;
    }

    // Initialize mutex on first use (thread-safe via pthread_once)
    nimcp_platform_once(&g_wave_mutex_once, init_wave_mutex_once);

    nimcp_platform_mutex_lock(&g_wave_mutex);

    if (g_wave_count >= 64) {
        nimcp_platform_mutex_unlock(&g_wave_mutex);
        LOG_ERROR("Glial wave registry full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "init_wave_mutex_once: capacity exceeded");
        return NULL;
    }

    // Create wave context
    glial_wave_context_t* wave = &g_waves[g_wave_count];
    wave->wave_id = g_next_wave_id++;
    wave->source_module = bio_module_context_get_id(ctx);
    wave->intensity = intensity;
    wave->current_intensity = intensity;
    wave->start_time_us = nimcp_platform_time_monotonic_us();
    wave->active = true;

    // Copy metadata if provided
    if (metadata) {
        wave->metadata_size = 256;  // Simplified: fixed size
        memcpy(wave->metadata, metadata, 256);
    } else {
        wave->metadata_size = 0;
    }

    g_wave_count++;

    nimcp_platform_mutex_unlock(&g_wave_mutex);

    LOG_DEBUG("Initiated glial wave %u from module %u with intensity %.3f",
              wave->wave_id, wave->source_module, intensity);

    // Notify all registered callbacks
    nimcp_platform_mutex_lock(&g_wave_mutex);

    for (uint32_t i = 0; i < g_wave_callback_count; i++) {
        wave_callback_entry_t* entry = &g_wave_callbacks[i];
        if (entry->active && entry->module_id != wave->source_module) {
            // Call the callback with current intensity
            entry->callback((nimcp_glial_wave_t)wave,
                          entry->module_id,
                          wave->current_intensity,
                          entry->user_data);
        }
    }

    nimcp_platform_mutex_unlock(&g_wave_mutex);

    return (nimcp_glial_wave_t)wave;
}

static void subscription_init(void) {
    nimcp_platform_mutex_init(&g_subscription_mutex, false);
    memset(g_subscriptions, 0, sizeof(g_subscriptions));
    g_subscription_count = 0;
}

static void emotion_registration_init(void) {
    nimcp_platform_mutex_init(&g_emotion_reg_mutex, false);
    memset(g_emotion_registrations, 0, sizeof(g_emotion_registrations));
    g_emotion_registration_count = 0;
}


/*=============================================================================
 * Subsystem statics reset (called from bio_router_shutdown)
 *============================================================================*/
static void bio_router_reset_subsystem_statics(void) {
    g_signal_mutex_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    g_signal_observer_count = 0;
    memset(g_signal_observers, 0, sizeof(g_signal_observers));

    g_wave_mutex_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    g_wave_callback_count = 0;
    memset(g_wave_callbacks, 0, sizeof(g_wave_callbacks));

    g_subscription_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    g_subscription_count = 0;
    memset(g_subscriptions, 0, sizeof(g_subscriptions));

    g_emotion_reg_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    g_emotion_registration_count = 0;
    memset(g_emotion_registrations, 0, sizeof(g_emotion_registrations));
}
