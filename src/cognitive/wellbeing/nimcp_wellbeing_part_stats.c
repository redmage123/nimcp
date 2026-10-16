// nimcp_wellbeing_part_stats.c - stats functions
// Part of nimcp_wellbeing.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_wellbeing.c


/**
 * WHAT: Initialize event log mutex, B-tree, and lock memory
 * WHY: Must initialize all structures before use
 * HOW: Called via nimcp_once for thread-safe init
 */
static void init_event_log_mutex(void)
{
    nimcp_platform_mutex_init(&event_log_mutex, false);

    // Create B-tree for timestamp-indexed queries
    event_btree = btree_create(compare_timestamps, extract_timestamp_key, free_event);
    if (!event_btree) {
        NIMCP_LOGGING_WARN("Failed to create wellbeing event B-tree - queries will be slower");
    }

    // Lock wellbeing memory in RAM (critical for ethical monitoring)
    lock_wellbeing_memory();
}


//=============================================================================
// EVENT LOGGING
//=============================================================================

/**
 * WHAT: Log a wellbeing event for audit trail
 * WHY: Ethical accountability requires complete record
 * HOW: Thread-safe circular buffer with B-tree index for efficient queries
 *
 * @param event Event to log
 * @return true if logged successfully
 */
bool wellbeing_log_event(wellbeing_event_t event)
{
    // Ensure initialization
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_log_event", 0.0f);


    ensure_event_log_init();

    // Populate timestamp key for B-tree indexing
    snprintf(event.timestamp_key, sizeof(event.timestamp_key), "%020llu",
             (unsigned long long)event.timestamp);

    // Thread safety
    nimcp_platform_mutex_lock(&event_log_mutex);

    // If buffer is full, remove the event we're about to overwrite from B-tree
    if (event_count >= MAX_EVENT_LOG && event_btree) {
        // We're about to overwrite event_log[event_write_index]
        // Remove it from B-tree first to avoid stale pointers
        const char* old_key = event_log[event_write_index].timestamp_key;
        if (old_key && old_key[0] != '\0') {
            btree_remove(event_btree, old_key);
        }
    }

    // Free old deep-copied strings when overwriting in circular buffer
    if (event_count >= MAX_EVENT_LOG) {
        wellbeing_event_t* old = &event_log[event_write_index];
        if (old->event_type) {
            nimcp_free(old->event_type);
            old->event_type = NULL;
        }
        if (old->description) {
            nimcp_free(old->description);
            old->description = NULL;
        }
        if (old->action_taken) {
            nimcp_free(old->action_taken);
            old->action_taken = NULL;
        }
    }

    // Store in circular buffer with deep-copied strings to prevent dangling pointers
    event_log[event_write_index] = event;
    event_log[event_write_index].event_type = event.event_type ? nimcp_strdup(event.event_type) : NULL;
    event_log[event_write_index].description = event.description ? nimcp_strdup(event.description) : NULL;
    event_log[event_write_index].action_taken = event.action_taken ? nimcp_strdup(event.action_taken) : NULL;

    // Insert into B-tree for efficient querying
    if (event_btree) {
        // Insert pointer to event in circular buffer
        int result = btree_insert(event_btree, &event_log[event_write_index]);
        if (result != BTREE_SUCCESS && result != BTREE_DUPLICATE) {
            NIMCP_LOGGING_WARN("Failed to insert event into B-tree: %d", result);
        }
    }

    event_write_index = (event_write_index + 1) % MAX_EVENT_LOG;

    if (event_count < MAX_EVENT_LOG) {
        event_count++;
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);

    // NOTE: Logging disabled during testing to avoid performance bottleneck
    // In production, you may want to enable this for critical events only
    // NIMCP_LOGGING_INFO("WELLBEING EVENT: %s - %s (severity: %d)",
    //          event.event_type ? event.event_type : "unknown",
    //          event.description ? event.description : "no description",
    //          event.severity);

    return true;
}


/**
 * WHAT: Collect CPU and memory metrics from /proc (Linux)
 * WHY: Direct OS-level metrics are most accurate
 * HOW: Parse /proc/self/stat and /proc/self/status
 *
 * @param metrics Output structure to fill
 * @return true if successful
 *
 * COMPLEXITY: O(1) - fixed file size
 */
static bool collect_linux_metrics(resource_metrics_t* metrics)
{
    // Guard clause: Validate input
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collect_linux_metrics: metrics is NULL");
        return false;
    }

    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // CPU time (user + system)
        metrics->cpu_time_us = (usage.ru_utime.tv_sec * 1000000UL + usage.ru_utime.tv_usec) +
                               (usage.ru_stime.tv_sec * 1000000UL + usage.ru_stime.tv_usec);

        // Memory usage (RSS in KB -> bytes)
        metrics->memory_used_bytes = usage.ru_maxrss * 1024;
        metrics->memory_peak_bytes = usage.ru_maxrss * 1024;

        // Page faults
        metrics->page_faults = usage.ru_majflt;  // Major page faults only

        // I/O operations
        metrics->io_read_ops = usage.ru_inblock;
        metrics->io_write_ops = usage.ru_oublock;

        // Context switches
        metrics->context_switches = usage.ru_nivcsw + usage.ru_nvcsw;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collect_linux_metrics: operation failed");
        return false;
    }

    // Try to get more detailed memory info from /proc/self/status
    FILE* status = fopen("/proc/self/status", "r");
    if (status) {
        char line[NIMCP_ERROR_BUFFER_SIZE];
        while (fgets(line, sizeof(line), status)) {
            unsigned long val;
            if (sscanf(line, "VmRSS: %lu kB", &val) == 1) {
                metrics->memory_used_bytes = val * 1024;
            } else if (sscanf(line, "VmPeak: %lu kB", &val) == 1) {
                metrics->memory_peak_bytes = val * 1024;
            } else if (sscanf(line, "Threads: %u", &metrics->thread_count) == 1) {
                // Thread count found
            }
        }
        fclose(status);
    }

    // Try to get I/O bytes from /proc/self/io
    FILE* io = fopen("/proc/self/io", "r");
    if (io) {
        char line[NIMCP_ERROR_BUFFER_SIZE];
        while (fgets(line, sizeof(line), io)) {
            unsigned long long val;
            if (sscanf(line, "read_bytes: %llu", &val) == 1) {
                metrics->io_read_bytes = val;
            } else if (sscanf(line, "write_bytes: %llu", &val) == 1) {
                metrics->io_write_bytes = val;
            }
        }
        fclose(io);
    }

    // Calculate CPU usage percentage (requires two samples, so estimate for now)
    // TODO: Improve with delta calculation over time
    metrics->cpu_usage_percent = 0.0F;  // Placeholder
    metrics->cpu_steal_percent = 0.0F;  // Would need /proc/stat parsing

    // Memory percentage (estimate - would need system total memory)
    if (metrics->memory_limit_bytes > 0) {
        metrics->memory_usage_percent =
            (float)metrics->memory_used_bytes / metrics->memory_limit_bytes * 100.0F;
    } else {
        metrics->memory_usage_percent = 0.0F;
    }

    metrics->timestamp = nimcp_time_get_us();
    return true;
}


/**
 * WHAT: Collect current resource usage metrics
 * WHY: Monitor for resource starvation
 * HOW: Platform-specific implementation (Linux, macOS, Windows)
 */
bool wellbeing_collect_resource_metrics(resource_metrics_t* metrics)
{
    // Guard clause: Validate input
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_collect_resource_metrics: metrics is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_collect_resource_met", 0.0f);

    ensure_resource_tracking_init();

    // Zero out the structure
    memset(metrics, 0, sizeof(resource_metrics_t));

#ifdef __linux__
    return collect_linux_metrics(metrics);
#elif defined(__APPLE__)
    // macOS implementation would go here (using sysctl)
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_collect_resource_metrics: operation failed");
    return false;
#elif defined(_WIN32)
    // Windows implementation would go here (using Performance Counters)
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_collect_resource_metrics: operation failed");
    return false;
#else
    // Unsupported platform
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_collect_resource_metrics: operation failed");
    return false;
#endif
}


/**
 * WHAT: Store metrics in history buffer
 * WHY: Enable trend analysis and statistics
 * HOW: Circular buffer with mutex protection
 */
static void store_metrics_in_history(const resource_metrics_t* metrics)
{
    if (!metrics)
        return;

    ensure_resource_tracking_init();

    nimcp_platform_mutex_lock(&resource_mutex);

    // Store in circular buffer
    resource_history[resource_history_index] = *metrics;
    resource_history_index = (resource_history_index + 1) % MAX_RESOURCE_HISTORY;

    if (resource_history_count < MAX_RESOURCE_HISTORY) {
        resource_history_count++;
    }

    nimcp_platform_mutex_unlock(&resource_mutex);
}
