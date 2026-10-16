// nimcp_health_agent_part_stats.c - stats functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * Manual Trigger API
 * ============================================================================ */

int nimcp_health_agent_report_anomaly(nimcp_health_agent_t* agent,
                                       const health_agent_message_t* msg) {
    if (!validate_agent(agent) || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_heartbeat: required parameter is NULL (validate_agent, msg)");
        return -1;
    }

    /* Copy message and assign anomaly ID */
    health_agent_message_t queued_msg = *msg;
    queued_msg.anomaly_id = atomic_fetch_add(&agent->next_anomaly_id, 1);

    if (queued_msg.timestamp_us == 0) {
        queued_msg.timestamp_us = get_timestamp_us();
    }

    /* Queue message */
    if (!msg_queue_push(&agent->msg_queue, &queued_msg)) {
        nimcp_log(LOG_LEVEL_WARN, "Message queue full, dropping anomaly report");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_heartbeat: msg_queue_push is NULL");
        return -1;
    }

    /* Update statistics */
    nimcp_mutex_lock(agent->stats_mutex);
    agent->stats.anomalies_detected++;
    agent->stats.messages_sent++;
    if (msg->severity > agent->stats.highest_severity_seen) {
        agent->stats.highest_severity_seen = msg->severity;
    }
    nimcp_mutex_unlock(agent->stats_mutex);

    /* Update current severity if higher */
    health_agent_severity_t current = atomic_load(&agent->current_severity);
    if (msg->severity > current) {
        atomic_store(&agent->current_severity, msg->severity);
    }

    /* Call callback if registered */
    if (agent->config.on_anomaly_detected) {
        agent->config.on_anomaly_detected(&queued_msg, agent->config.callback_user_data);
    }

    return 0;
}


int nimcp_health_agent_report_drive(
    nimcp_health_agent_t* agent,
    uint32_t drive_type,
    float drive_level,
    const char* description
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_report_drive: validate_agent is NULL");
        return -1;
    }

    if (!agent->hypothalamus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_report_drive: agent->hypothalamus is NULL");
        return -1;
    }

    /* Map drive level to urgency */
    hypo_urgency_t urgency = HYPO_URGENCY_NONE;
    if (drive_level >= 0.7f) {
        urgency = HYPO_URGENCY_URGENT;
    } else if (drive_level >= 0.5f) {
        urgency = HYPO_URGENCY_ELEVATED;
    } else if (drive_level >= 0.3f) {
        urgency = HYPO_URGENCY_MODERATE;
    } else if (drive_level > 0.0f) {
        urgency = HYPO_URGENCY_LOW;
    }

    /* Report drive to hypothalamus orchestrator */
    int result = hypo_orch_report_drive(
        agent->hypothalamus,
        0,  /* bridge_id: health agent uses bridge 0 */
        drive_type,
        drive_level,
        urgency,
        description
    );
    if (result != 0) {
        nimcp_log(LOG_LEVEL_WARN, "Failed to report drive to hypothalamus");
    }

    atomic_fetch_add(&agent->drive_events_published, 1);

    nimcp_log(LOG_LEVEL_DEBUG, "Reported drive (type=%u, level=%.2f): %s",
              drive_type, drive_level, description ? description : "");
    return 0;
}


/* ============================================================================
 * STATE CONSISTENCY MANAGER - Phase 3
 * ============================================================================
 *
 * Implements comprehensive consistency checking for the health agent:
 * - Reference count validation
 * - Memory canary verification
 * - Magic number checks for registered structs
 * - Mutex state consistency
 * - Circular buffer integrity
 * - Knowledge graph consistency (when available)
 * - Neuron value validation (NaN/Inf detection)
 * ============================================================================ */

/**
 * @brief Check reference counts for consistency
 */
static bool agent_check_reference_counts(nimcp_health_agent_t* agent,
                                          health_agent_consistency_result_t* result) {
    if (!agent || !result) {
        return false;
    }

    bool passed = true;
    uint32_t errors = 0;

    /*
     * Reference count validation checks:
     * 1. Atomic stat counters should not be negative (overflow detection)
     * 2. Cumulative stats should be >= individual component stats
     */

    /* Check prediction stats consistency */
    uint64_t predictions_made = atomic_load(&agent->predictions_made);
    uint64_t predictions_correct = atomic_load(&agent->predictions_correct);
    if (predictions_correct > predictions_made) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: predictions_correct (%lu) > predictions_made (%lu)",
                  (unsigned long)predictions_correct, (unsigned long)predictions_made);
        errors++;
        passed = false;
    }

    /* Check consensus stats consistency */
    uint64_t consensus_achieved = atomic_load(&agent->consensus_achieved);
    uint64_t consensus_requests = atomic_load(&agent->consensus_requests);
    if (consensus_achieved > consensus_requests) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: consensus_achieved (%lu) > consensus_requests (%lu)",
                  (unsigned long)consensus_achieved, (unsigned long)consensus_requests);
        errors++;
        passed = false;
    }

    /* Check engram stats consistency */
    uint64_t engram_recalls = atomic_load(&agent->engram_recalls);
    uint64_t engram_encodings = atomic_load(&agent->engram_encodings);
    /* Note: recalls can exceed encodings if same memory recalled multiple times */
    (void)engram_recalls;
    (void)engram_encodings;

    /* Check dragonfly tracking stats */
    uint64_t interceptions = atomic_load(&agent->dragonfly_interceptions);
    uint64_t pursuits = atomic_load(&agent->dragonfly_pursuits);
    if (interceptions > pursuits && pursuits > 0) {
        nimcp_log(LOG_LEVEL_WARN, "Consistency: dragonfly interceptions (%lu) > pursuits (%lu)",
                  (unsigned long)interceptions, (unsigned long)pursuits);
        errors++;
        passed = false;
    }

    result->refcount_check_passed = passed;
    result->refcount_errors = errors;
    return passed;
}
