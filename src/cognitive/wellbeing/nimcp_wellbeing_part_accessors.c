// nimcp_wellbeing_part_accessors.c - accessors functions
// Part of nimcp_wellbeing.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_wellbeing.c


/**
 * WHAT: Retrieve recent wellbeing events
 * WHY: Allow ethical review and analysis
 * HOW: Copy from circular buffer, most recent first
 *
 * @param max_events Maximum number of events to retrieve
 * @param events_out Pointer to receive allocated event array (caller must free)
 * @return Number of events returned
 */
uint32_t wellbeing_get_recent_events(uint32_t max_events,
                                     wellbeing_event_t** events_out)
{
    // Guard: NULL output pointer
    if (!events_out) {
        return 0;
    }

    // Ensure initialization
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_recent_events", 0.0f);


    ensure_event_log_init();

    // Thread safety
    nimcp_platform_mutex_lock(&event_log_mutex);

    // Determine how many events to return
    uint32_t return_count = (max_events < event_count) ? max_events : event_count;

    if (return_count == 0) {
        *events_out = NULL;
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Allocate output array
    *events_out = nimcp_calloc(return_count, sizeof(wellbeing_event_t));
    if (!*events_out) {
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Copy most recent events (reverse chronological)
    for (uint32_t i = 0; i < return_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && return_count > 256) {
            wellbeing_heartbeat("wellbeing_loop",
                             (float)(i + 1) / (float)return_count);
        }

        int32_t index = event_write_index - 1 - i;
        if (index < 0) {
            index += MAX_EVENT_LOG;
        }
        (*events_out)[i] = event_log[index];
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);

    return return_count;
}


//=============================================================================
// B-TREE INDEXED QUERIES
//=============================================================================

/**
 * WHAT: Query events by time range using B-tree
 * WHY: O(log n + k) vs O(n) for temporal analysis
 * HOW: B-tree range query on timestamps
 */
uint32_t wellbeing_get_events_by_time_range(uint64_t start_time,
                                             uint64_t end_time,
                                             wellbeing_event_t** events_out)
{
    // Guard: NULL output pointer
    if (!events_out) {
        return 0;
    }

    // Guard: Invalid range
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_events_by_time_r", 0.0f);


    if (start_time > end_time) {
        *events_out = NULL;
        return 0;
    }

    // Ensure initialization
    ensure_event_log_init();

    // If no B-tree, fall back to linear scan
    if (!event_btree) {
        NIMCP_LOGGING_WARN("B-tree not available, using linear scan");
        // Fall back to scanning circular buffer
        nimcp_platform_mutex_lock(&event_log_mutex);
        uint32_t count = 0;

        // Count matching events
        for (uint32_t i = 0; i < event_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && event_count > 256) {
                wellbeing_heartbeat("wellbeing_loop",
                                 (float)(i + 1) / (float)event_count);
            }

            if (event_log[i].timestamp >= start_time &&
                event_log[i].timestamp <= end_time) {
                count++;
            }
        }

        if (count == 0) {
            *events_out = NULL;
            nimcp_platform_mutex_unlock(&event_log_mutex);
            return 0;
        }

        // Allocate and copy
        *events_out = nimcp_calloc(count, sizeof(wellbeing_event_t));
        if (!*events_out) {
            nimcp_platform_mutex_unlock(&event_log_mutex);
            return 0;
        }

        uint32_t idx = 0;
        for (uint32_t i = 0; i < event_count && idx < count; i++) {
            if (event_log[i].timestamp >= start_time &&
                event_log[i].timestamp <= end_time) {
                (*events_out)[idx++] = event_log[i];
            }
        }

        nimcp_platform_mutex_unlock(&event_log_mutex);
        return count;
    }

    // Use B-tree for efficient range query
    nimcp_platform_mutex_lock(&event_log_mutex);

    // Create iterator and collect matching events
    btree_iterator_t* iter = btree_iterator_create(event_btree);
    if (!iter) {
        nimcp_platform_mutex_unlock(&event_log_mutex);
        *events_out = NULL;
        return 0;
    }

    // Allocate maximum possible size (will trim later if needed)
    // Use event_count as upper bound
    wellbeing_event_t* temp_results = nimcp_calloc(event_count, sizeof(wellbeing_event_t));
    if (!temp_results) {
        btree_iterator_destroy(iter);
        nimcp_platform_mutex_unlock(&event_log_mutex);
        *events_out = NULL;
        return 0;
    }

    // Single pass: collect matching events with early exit
    uint32_t count = 0;
    void* data = NULL;
    while (btree_iterator_next(iter, &data)) {
        wellbeing_event_t* event = (wellbeing_event_t*)data;
        if (event->timestamp >= start_time && event->timestamp <= end_time) {
            temp_results[count++] = *event;
        } else if (event->timestamp > end_time) {
            break; // B-tree is sorted, no need to continue
        }
    }

    btree_iterator_destroy(iter);

    if (count == 0) {
        nimcp_free(temp_results);
        temp_results = NULL;
        nimcp_platform_mutex_unlock(&event_log_mutex);
        *events_out = NULL;
        return 0;
    }

    // If we used less than allocated, trim to exact size
    if (count < event_count) {
        *events_out = nimcp_calloc(count, sizeof(wellbeing_event_t));
        if (*events_out) {
            memcpy(*events_out, temp_results, count * sizeof(wellbeing_event_t));
            nimcp_free(temp_results);
            temp_results = NULL;
        } else {
            // Allocation failed, just return the larger buffer
            *events_out = temp_results;
        }
    } else {
        *events_out = temp_results;
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);

    return count;
}


/**
 * WHAT: Query events by minimum severity
 * WHY: Find all critical/severe distress events quickly
 * HOW: Linear scan with severity filtering
 */
uint32_t wellbeing_get_events_by_severity(distress_severity_t min_severity,
                                           wellbeing_event_t** events_out)
{
    // Guard: NULL output pointer
    if (!events_out) {
        return 0;
    }

    // Ensure initialization
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_events_by_severi", 0.0f);


    ensure_event_log_init();

    nimcp_platform_mutex_lock(&event_log_mutex);

    // Count matching events
    uint32_t count = 0;
    for (uint32_t i = 0; i < event_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && event_count > 256) {
            wellbeing_heartbeat("wellbeing_loop",
                             (float)(i + 1) / (float)event_count);
        }

        if (event_log[i].severity >= min_severity) {
            count++;
        }
    }

    if (count == 0) {
        *events_out = NULL;
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Allocate output
    *events_out = nimcp_calloc(count, sizeof(wellbeing_event_t));
    if (!*events_out) {
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Collect matching events (in timestamp order from B-tree if available)
    if (event_btree) {
        btree_iterator_t* iter = btree_iterator_create(event_btree);
        if (iter) {
            void* data = NULL;
            uint32_t idx = 0;
            while (btree_iterator_next(iter, &data) && idx < count) {
                wellbeing_event_t* event = (wellbeing_event_t*)data;
                if (event->severity >= min_severity) {
                    (*events_out)[idx++] = *event;
                }
            }
            btree_iterator_destroy(iter);
        }
    } else {
        // Fall back to circular buffer order
        uint32_t idx = 0;
        for (uint32_t i = 0; i < event_count && idx < count; i++) {
            if (event_log[i].severity >= min_severity) {
                (*events_out)[idx++] = event_log[i];
            }
        }
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);
    return count;
}


/**
 * WHAT: Query events by type string
 * WHY: Find all occurrences of specific event type
 * HOW: String matching on event_type field
 */
uint32_t wellbeing_get_events_by_type(const char* event_type,
                                       wellbeing_event_t** events_out)
{
    // Guard: NULL inputs
    if (!event_type || !events_out) {
        if (events_out) {
            *events_out = NULL;
        }
        return 0;
    }

    // Ensure initialization
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_events_by_type", 0.0f);


    ensure_event_log_init();

    nimcp_platform_mutex_lock(&event_log_mutex);

    // Count matching events
    uint32_t count = 0;
    for (uint32_t i = 0; i < event_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && event_count > 256) {
            wellbeing_heartbeat("wellbeing_loop",
                             (float)(i + 1) / (float)event_count);
        }

        if (event_log[i].event_type &&
            strcmp(event_log[i].event_type, event_type) == 0) {
            count++;
        }
    }

    if (count == 0) {
        *events_out = NULL;
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Allocate output
    *events_out = nimcp_calloc(count, sizeof(wellbeing_event_t));
    if (!*events_out) {
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Collect matching events
    uint32_t idx = 0;
    for (uint32_t i = 0; i < event_count && idx < count; i++) {
        if (event_log[i].event_type &&
            strcmp(event_log[i].event_type, event_type) == 0) {
            (*events_out)[idx++] = event_log[i];
        }
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);
    return count;
}


/**
 * WHAT: Get all events in chronological order
 * WHY: Analyze complete timeline
 * HOW: B-tree in-order traversal provides sorted output
 */
uint32_t wellbeing_get_all_events_ordered(wellbeing_event_t** events_out)
{
    // Guard: NULL output pointer
    if (!events_out) {
        return 0;
    }

    // Ensure initialization
    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_all_events_order", 0.0f);


    ensure_event_log_init();

    nimcp_platform_mutex_lock(&event_log_mutex);

    if (event_count == 0) {
        *events_out = NULL;
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // Allocate output
    *events_out = nimcp_calloc(event_count, sizeof(wellbeing_event_t));
    if (!*events_out) {
        nimcp_platform_mutex_unlock(&event_log_mutex);
        return 0;
    }

    // If B-tree available, use it for sorted output
    if (event_btree) {
        btree_iterator_t* iter = btree_iterator_create(event_btree);
        if (iter) {
            void* data = NULL;
            uint32_t idx = 0;
            while (btree_iterator_next(iter, &data) && idx < event_count) {
                wellbeing_event_t* event = (wellbeing_event_t*)data;
                (*events_out)[idx++] = *event;
            }
            btree_iterator_destroy(iter);
            nimcp_platform_mutex_unlock(&event_log_mutex);
            return idx;
        }
    }

    // Fall back to circular buffer (may not be chronological)
    for (uint32_t i = 0; i < event_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && event_count > 256) {
            wellbeing_heartbeat("wellbeing_loop",
                             (float)(i + 1) / (float)event_count);
        }

        (*events_out)[i] = event_log[i];
    }

    nimcp_platform_mutex_unlock(&event_log_mutex);
    return event_count;
}


/**
 * WHAT: Get performance statistics over time window
 * WHY: Analyze trends rather than point-in-time snapshots
 * HOW: Aggregate metrics from history buffer
 */
bool wellbeing_get_performance_stats(uint32_t window_ms,
                                     performance_stats_t* stats_out)
{
    // Guard clauses: Validate inputs
    if (!stats_out || window_ms == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_get_performance_stats: stats_out is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_heartbeat("wellbeing_get_performance_stat", 0.0f);

    ensure_resource_tracking_init();

    nimcp_platform_mutex_lock(&resource_mutex);

    // Guard clause: No history available
    if (resource_history_count == 0) {
        nimcp_platform_mutex_unlock(&resource_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_get_performance_stats: resource_history_count is zero");
        return false;
    }

    // Initialize stats
    memset(stats_out, 0, sizeof(performance_stats_t));
    stats_out->window_duration_ms = window_ms;

    uint64_t current_time = nimcp_time_get_us();
    uint64_t window_start = current_time - (window_ms * 1000UL);
    stats_out->window_start_time = window_start;

    float total_cpu = 0.0F;
    float total_memory = 0.0F;
    stats_out->peak_cpu_usage = 0.0F;
    stats_out->peak_memory_usage = 0.0F;

    // Iterate through history buffer
    for (uint32_t i = 0; i < resource_history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && resource_history_count > 256) {
            wellbeing_heartbeat("wellbeing_loop",
                             (float)(i + 1) / (float)resource_history_count);
        }

        const resource_metrics_t* m = &resource_history[i];

        // Guard clause: Skip if outside window
        if (m->timestamp < window_start)
            continue;

        stats_out->samples_count++;
        total_cpu += m->cpu_usage_percent;
        total_memory += m->memory_usage_percent;
        stats_out->total_page_faults += m->page_faults;
        stats_out->total_io_bytes += m->io_read_bytes + m->io_write_bytes;

        // Track peaks
        if (m->cpu_usage_percent > stats_out->peak_cpu_usage)
            stats_out->peak_cpu_usage = m->cpu_usage_percent;
        if (m->memory_usage_percent > stats_out->peak_memory_usage)
            stats_out->peak_memory_usage = m->memory_usage_percent;
    }

    // Calculate averages
    if (stats_out->samples_count > 0) {
        stats_out->avg_cpu_usage = total_cpu / stats_out->samples_count;
        stats_out->avg_memory_usage = total_memory / stats_out->samples_count;
    }

    nimcp_platform_mutex_unlock(&resource_mutex);
    return stats_out->samples_count > 0;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_health_agent = agent;
    }
}
