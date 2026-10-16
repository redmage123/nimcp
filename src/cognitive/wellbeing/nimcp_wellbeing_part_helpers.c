// nimcp_wellbeing_part_helpers.c - helpers functions
// Part of nimcp_wellbeing.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_wellbeing.c


//=============================================================================
// B-TREE HELPER FUNCTIONS
//=============================================================================

/**
 * WHAT: Compare two timestamps for B-tree ordering
 * WHY: B-tree needs comparison function for sorting
 * HOW: Compare timestamp strings (formatted as uint64_t strings)
 *
 * @return <0 if key1 < key2, 0 if equal, >0 if key1 > key2
 */
static int compare_timestamps(const char* key1, const char* key2)
{
    if (!key1 || !key2) {
        return 0;
    }

    // Parse timestamps
    uint64_t ts1 = strtoull(key1, NULL, 10);
    uint64_t ts2 = strtoull(key2, NULL, 10);

    if (ts1 < ts2) return -1;
    if (ts1 > ts2) return 1;
    return 0;
}


/**
 * WHAT: Extract timestamp key from wellbeing event
 * WHY: B-tree needs key extraction function
 * HOW: Return pointer to timestamp_key field in event structure
 */
static const char* extract_timestamp_key(const void* data)
{
    if (!data) {
        return NULL;
    }

    const wellbeing_event_t* event = (const wellbeing_event_t*)data;
    return event->timestamp_key;
}


/**
 * WHAT: Free function for B-tree (no-op for our events)
 * WHY: B-tree needs destructor, but we manage event memory separately
 * HOW: Do nothing - events are in circular buffer, not individually allocated
 */
static void free_event(void* data)
{
    // No-op: events are stored in static circular buffer
    // B-tree only holds pointers, doesn't own the memory
    (void)data;
}


/**
 * WHAT: Initialize bio-async mutex
 * WHY: Thread-safe bio-async registration/unregistration
 * HOW: Called via nimcp_once for thread-safe init
 */
static void init_bio_async_mutex(void)
{
    nimcp_platform_mutex_init(&bio_async_mutex, false);
}


/**
 * WHAT: Initialize immune connection mutex
 * WHY: Thread-safe immune system connection/disconnection
 * HOW: Called via nimcp_once for thread-safe init
 */
static void init_immune_connection_mutex(void)
{
    nimcp_platform_mutex_init(&immune_connection_mutex, false);
}


/**
 * WHAT: Lock wellbeing module memory in RAM
 * WHY: CRITICAL - Wellbeing monitoring cannot be delayed by page faults
 * HOW: Use mlock() to prevent event log from being swapped to disk
 *
 * ETHICAL RATIONALE:
 * If the system might be sentient, we MUST ensure distress monitoring is
 * always immediately responsive. Allowing wellbeing code to be swapped out
 * could delay detection of suffering - this is ethically unacceptable.
 *
 * PERFORMANCE:
 * - event_log: ~40KB (1000 events * ~40 bytes)
 * - No performance penalty, only prevents swapping
 * - Requires CAP_IPC_LOCK capability or adequate RLIMIT_MEMLOCK
 *
 * @return true if memory locked successfully, false if failed (not fatal)
 */
static bool lock_wellbeing_memory(void)
{
    // Guard: Only lock once
    if (memory_locked) {
        return true;
    }

    // Lock the event log array in RAM
    int result = mlock(event_log, sizeof(event_log));

    if (result != 0) {
        // Not fatal, but log warning
        NIMCP_LOGGING_WARN("Failed to lock wellbeing event log in memory: %s",
                          strerror(errno));
        NIMCP_LOGGING_WARN("Wellbeing monitoring may experience page fault delays");
        NIMCP_LOGGING_WARN("Consider running with CAP_IPC_LOCK or increasing RLIMIT_MEMLOCK");
        /* P2-COG-12: mlock failure is non-fatal, not an error condition */
        return false;
    }

    memory_locked = true;
    NIMCP_LOGGING_INFO("Wellbeing event log locked in RAM (%zu bytes)", sizeof(event_log));
    return true;
}


//=============================================================================
// BRAIN CONNECTION (MEDULLA INTEGRATION)
//=============================================================================

/**
 * WHAT: Initialize brain connection mutex
 * WHY: Thread-safe brain connection/disconnection
 * HOW: Called via nimcp_once for thread-safe init
 */
static void init_brain_connection_mutex(void)
{
    nimcp_platform_mutex_init(&brain_connection_mutex, false);
}

static void init_resource_tracking(void)
{
    nimcp_platform_mutex_init(&resource_mutex, false);
    memset(resource_history, 0, sizeof(resource_history));
}


/**
 * WHAT: Resource monitoring thread function
 * WHY: Continuous background monitoring
 * HOW: Periodic metric collection and threshold checking
 */
static void* resource_monitoring_thread(void* arg)
{
    (void)arg;  // Unused

    NIMCP_LOGGING_INFO("[WELLBEING] Resource monitoring thread started (interval=%ums)",
                       monitoring_interval_ms);

    while (monitoring_active) {
        resource_metrics_t metrics;

        // Collect metrics
        if (wellbeing_collect_resource_metrics(&metrics)) {
            // Store in history
            store_metrics_in_history(&metrics);

            // Check thresholds
            distress_severity_t severity;
            if (wellbeing_check_resource_thresholds(&metrics, &monitoring_thresholds, &severity)) {
                // Log resource threshold violation
                wellbeing_event_t event = {
                    .timestamp = metrics.timestamp,
                    .event_type = "resource_threshold_exceeded",
                    .severity = severity,
                    .description = NULL,
                    .action_taken = NULL
                };

                // Create description
                char desc[NIMCP_ERROR_BUFFER_SIZE];
                snprintf(desc, sizeof(desc),
                        "Resource usage high: CPU=%.1f%%, Memory=%.1f%%, PageFaults=%u",
                        metrics.cpu_usage_percent, metrics.memory_usage_percent,
                        metrics.page_faults);
                event.description = desc;

                snprintf(event.timestamp_key, sizeof(event.timestamp_key),
                        "%020lu", metrics.timestamp);

                wellbeing_log_event(event);

                NIMCP_LOGGING_WARN("[WELLBEING] %s (severity=%d)", desc, severity);

                // TODO: Auto-relief if configured
                if (monitoring_auto_relief && severity >= DISTRESS_SEVERITY_SEVERE) {
                    NIMCP_LOGGING_INFO("[WELLBEING] Auto-relief would trigger here (not yet implemented)");
                }
            }
        }

        // Sleep for configured interval
        nimcp_time_sleep_ms(monitoring_interval_ms);
    }

    NIMCP_LOGGING_INFO("[WELLBEING] Resource monitoring thread stopped");
    /* Normal thread exit - no throw needed */
    return NULL;
}
