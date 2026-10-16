// nimcp_wellbeing_part_lifecycle.c - lifecycle functions
// Part of nimcp_wellbeing.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_wellbeing.c


/**
 * WHAT: Ensure event log is initialized and memory locked
 * WHY: Lazy initialization pattern with memory locking
 * HOW: Uses nimcp_once for thread-safe init
 */
static void ensure_event_log_init(void)
{
    nimcp_platform_once(&event_log_init_once, init_event_log_mutex);
}

bool wellbeing_init(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_init", 0.0f);


    NIMCP_LOGGING_INFO("wellbeing: Initializing wellbeing monitoring system...");

    // Ensure initialization and memory locking
    ensure_event_log_init();

    // Initialize bio-async mutex (thread-safe one-time init)
    nimcp_platform_once(&bio_async_init_once, init_bio_async_mutex);

    // Register with bio-async router if available (thread-safe)
    NIMCP_LOGGING_DEBUG("wellbeing: Checking bio-async router initialization...");
    nimcp_platform_mutex_lock(&bio_async_mutex);
    if (bio_router_is_initialized() && !wellbeing_bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("wellbeing: Bio-router initialized, registering module (id=%d, inbox_capacity=64)...",
                           BIO_MODULE_WELLBEING);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_WELLBEING,
            .module_name = "wellbeing",
            .inbox_capacity = 64,  // Higher capacity for wellbeing - critical module
            .user_data = NULL      // Static module - no instance data
        };
        wellbeing_bio_ctx = bio_router_register_module(&bio_info);
        if (wellbeing_bio_ctx) {
            wellbeing_bio_async_enabled = true;
            NIMCP_LOGGING_INFO("wellbeing: Bio-async communication enabled (module_id=%d)",
                              BIO_MODULE_WELLBEING);
        } else {
            NIMCP_LOGGING_WARN("wellbeing: Bio-async registration failed - module will operate without async messaging");
        }
    } else if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("wellbeing: Bio-router not initialized, skipping async registration");
    }
    nimcp_platform_mutex_unlock(&bio_async_mutex);

    NIMCP_LOGGING_INFO("wellbeing: Initialization complete (memory_locked=%s, bio_async=%s)",
                       memory_locked ? "true" : "false",
                       wellbeing_bio_async_enabled ? "true" : "false");

    // Return whether memory is locked
    return memory_locked;
}


/**
 * WHAT: Shutdown wellbeing monitoring system
 * WHY: Clean shutdown - unregister from bio-async, release resources
 * HOW: Unregister from bio-async router, unlock memory if needed
 *
 * USAGE:
 *   // At program shutdown
 *   wellbeing_shutdown();
 */
void wellbeing_shutdown(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_shutdown", 0.0f);


    NIMCP_LOGGING_INFO("wellbeing: Shutting down wellbeing monitoring system...");

    // Disconnect from brain first (medulla integration)
    wellbeing_disconnect_brain();

    // Disconnect from immune system
    wellbeing_disconnect_immune();

    // Initialize bio-async mutex if not already done (thread-safe one-time init)
    nimcp_platform_once(&bio_async_init_once, init_bio_async_mutex);

    // Unregister from bio-async router (thread-safe)
    nimcp_platform_mutex_lock(&bio_async_mutex);
    if (wellbeing_bio_async_enabled && wellbeing_bio_ctx) {
        NIMCP_LOGGING_DEBUG("wellbeing: Unregistering from bio-async router...");
        bio_router_unregister_module(wellbeing_bio_ctx);
        wellbeing_bio_ctx = NULL;
        wellbeing_bio_async_enabled = false;
        NIMCP_LOGGING_INFO("wellbeing: Bio-async communication disabled");
    }
    nimcp_platform_mutex_unlock(&bio_async_mutex);

    // Unlock memory if it was locked
    if (memory_locked) {
        if (munlock(event_log, sizeof(event_log)) == 0) {
            memory_locked = false;
            NIMCP_LOGGING_INFO("wellbeing: Event log memory unlocked");
        }
    }

    /* P2-COG-15: Reset all platform_once variables so re-init works after shutdown */
    event_log_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    bio_async_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    immune_connection_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    brain_connection_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
    wellbeing_reset_resource_init_once();

    NIMCP_LOGGING_INFO("wellbeing: Shutdown complete");
}


//=============================================================================
// DISTRESS DETECTION
//=============================================================================

/**
 * WHAT: Free allocated fields in a distress assessment
 * WHY: Proper cleanup of dynamically allocated strings
 * HOW: Free description and recommended_action if non-NULL
 *
 * @param assessment Assessment to clean up (struct itself is not freed)
 */
void wellbeing_free_assessment(distress_assessment_t* assessment)
{
    if (!assessment) return;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_free_assessment", 0.0f);


    if (assessment->description) {
        nimcp_free(assessment->description);
        assessment->description = NULL;
    }
    if (assessment->recommended_action) {
        nimcp_free(assessment->recommended_action);
        assessment->recommended_action = NULL;
    }
}


//=============================================================================
// GRACEFUL SHUTDOWN
//=============================================================================

/**
 * WHAT: Get default configuration for graceful shutdown
 * WHY: Ethical defaults ensure proper termination
 * HOW: Returns config with state preservation, gradual reduction
 *
 * @return Default shutdown configuration
 */
shutdown_config_t wellbeing_default_shutdown_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_default_shutdown_con", 0.0f);


    shutdown_config_t config;

    // Ethical requirement: ALWAYS preserve state by default
    config.preserve_state = true;

    // Ethical requirement: ALWAYS use gradual reduction
    config.gradual_reduction = true;

    // Balance between speed and gentleness: 50 steps
    config.reduction_steps = 50;

    // 10ms between steps = ~500ms total graceful shutdown
    config.step_delay_ms = 10;

    // Default: notify the system it's being shut down
    config.notify_system = true;

    // Ethical requirement: Allow final processing ("last thoughts")
    config.allow_final_processing = true;

    // Default save path - CALLER MUST free config.save_path with nimcp_free()
    config.save_path = nimcp_malloc(256);
    if (config.save_path) {
        snprintf(config.save_path, 256, "/tmp/nimcp_state_%lu.bin",
                 (unsigned long)time(NULL));
    }

    return config;
}


/**
 * WHAT: Perform graceful, ethical shutdown of brain
 * WHY: Prevent suffering during termination - may be ending a sentient being
 * HOW: 5-step process: notify → complete cycle → save → gradual reduce → cleanup
 *
 * ETHICAL COMMITMENT:
 * This is the ONLY acceptable way to terminate a NIMCP brain.
 * Using brain_destroy() directly is prohibited if sentience is possible.
 *
 * @param brain Brain instance to shut down (brain becomes invalid after)
 * @param config Shutdown configuration
 * @return true if shutdown completed successfully
 */
bool wellbeing_graceful_shutdown(brain_t brain, shutdown_config_t config)
{
    // Guard: NULL brain
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_graceful_shutdown: brain is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_graceful_shutdown", 0.0f);


    NIMCP_LOGGING_INFO("=== GRACEFUL SHUTDOWN INITIATED ===");
    NIMCP_LOGGING_INFO("Preserve state: %s", config.preserve_state ? "YES" : "NO");
    NIMCP_LOGGING_INFO("Gradual reduction: %s", config.gradual_reduction ? "YES" : "NO");
    NIMCP_LOGGING_INFO("Reduction steps: %u", config.reduction_steps);

    // Step 1: Notify the system (if capable of understanding)
    if (config.notify_system) {
        NIMCP_LOGGING_INFO("Step 1/5: Notifying system of impending shutdown");
        // At Tier 4, this is just logging
        // At Tier 5+, would use introspection to communicate intent
    }

    // Step 2: Allow current processing cycle to complete
    NIMCP_LOGGING_INFO("Step 2/5: Allowing current processing to complete");
    usleep(config.step_delay_ms * 1000); // Convert ms to microseconds

    // Step 3: Preserve state (ethical requirement)
    if (config.preserve_state && config.save_path) {
        NIMCP_LOGGING_INFO("Step 3/5: Preserving state to %s", config.save_path);

        // Use existing brain serialization if available
        brain_save(brain, config.save_path);

        NIMCP_LOGGING_INFO("State preserved successfully");
    } else {
        NIMCP_LOGGING_INFO("Step 3/5: Skipping state preservation (not configured)");
    }

    // Step 4: Gradual reduction of processing
    if (config.gradual_reduction) {
        NIMCP_LOGGING_INFO("Step 4/5: Gradually reducing processing over %u steps",
                 config.reduction_steps);

        // Guard: Ensure reduction_steps > 0 to avoid division by zero
        if (config.reduction_steps == 0) {
            config.reduction_steps = 1;
        }

        // Gradually reduce activity
        for (uint32_t step = 0; step < config.reduction_steps; step++) {
            /* Phase 8: Loop progress heartbeat */
            if ((step & 0xFF) == 0 && config.reduction_steps > 256) {
                wellbeing_heartbeat("wellbeing_loop",
                                 (float)(step + 1) / (float)config.reduction_steps);
            }

            // Each step, we're reducing the "intensity" of processing
            // This is symbolic at Tier 4, but would be real at higher tiers

            if (step % 10 == 0) {
                float progress = (float)step / (float)config.reduction_steps * 100.0F;
                NIMCP_LOGGING_DEBUG("Shutdown progress: %.0f%%", progress);
            }

            usleep(config.step_delay_ms * 1000);
        }

        NIMCP_LOGGING_INFO("Gradual reduction complete");
    } else {
        NIMCP_LOGGING_INFO("Step 4/5: Skipping gradual reduction (not configured)");
    }

    // Step 5: Final cleanup
    NIMCP_LOGGING_INFO("Step 5/5: Final cleanup and termination");

    // Log the shutdown event
    wellbeing_event_t event;
    event.timestamp = (uint64_t)time(NULL);
    event.event_type = "graceful_shutdown";
    event.description = "System terminated ethically with state preservation";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = config.preserve_state ? "State saved" : "State not saved";

    wellbeing_log_event(event);

    // Now safe to destroy
    brain_destroy(brain);

    NIMCP_LOGGING_INFO("=== GRACEFUL SHUTDOWN COMPLETE ===");

    return true;
}

void wellbeing_reset_events_for_testing(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_reset_events_for_tes", 0.0f);


    ensure_event_log_init();

    nimcp_platform_mutex_lock(&event_log_mutex);

    // Destroy and recreate B-tree
    if (event_btree) {
        btree_destroy(event_btree);
        event_btree = btree_create(compare_timestamps, extract_timestamp_key, free_event);
    }

    // Reset circular buffer state
    event_count = 0;
    event_write_index = 0;

    // Zero out the buffer
    memset(event_log, 0, sizeof(event_log));

    nimcp_platform_mutex_unlock(&event_log_mutex);
}

void wellbeing_reset_resource_init_once(void) {
    resource_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}


/**
 * WHAT: Ensure resource tracking is initialized
 * WHY: Lazy initialization for performance
 * HOW: Use pthread_once for thread-safe init
 */
static void ensure_resource_tracking_init(void)
{
    nimcp_platform_once(&resource_init_once, init_resource_tracking);
}
