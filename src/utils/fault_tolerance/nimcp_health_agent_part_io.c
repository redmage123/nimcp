// nimcp_health_agent_part_io.c - io functions
// Part of nimcp_health_agent.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_health_agent.c


/* ============================================================================
 * String Conversion Functions
 * ============================================================================ */

const char* health_agent_msg_type_to_string(health_agent_msg_type_t type) {
    switch (type) {
        case HEALTH_MSG_ANOMALY_DETECTED:    return "ANOMALY_DETECTED";
        case HEALTH_MSG_CYTOKINE_SIGNAL:     return "CYTOKINE_SIGNAL";
        case HEALTH_MSG_EMERGENCY:           return "EMERGENCY";
        case HEALTH_MSG_RECOVERY_REQUEST:    return "RECOVERY_REQUEST";
        case HEALTH_MSG_STATE_CORRUPTION:    return "STATE_CORRUPTION";
        case HEALTH_MSG_HEARTBEAT_TIMEOUT:   return "HEARTBEAT_TIMEOUT";
        case HEALTH_MSG_DEADLOCK_DETECTED:   return "DEADLOCK_DETECTED";
        case HEALTH_MSG_NAN_DETECTED:        return "NAN_DETECTED";
        case HEALTH_MSG_MEMORY_CORRUPTION:   return "MEMORY_CORRUPTION";
        case HEALTH_MSG_RESOURCE_EXHAUSTION: return "RESOURCE_EXHAUSTION";
        case HEALTH_MSG_STATUS_UPDATE:       return "STATUS_UPDATE";
        default:                              return "UNKNOWN";
    }
}


const char* health_agent_severity_to_string(health_agent_severity_t severity) {
    switch (severity) {
        case HEALTH_SEVERITY_INFO:     return "INFO";
        case HEALTH_SEVERITY_WARNING:  return "WARNING";
        case HEALTH_SEVERITY_ERROR:    return "ERROR";
        case HEALTH_SEVERITY_CRITICAL: return "CRITICAL";
        case HEALTH_SEVERITY_FATAL:    return "FATAL";
        default:                        return "UNKNOWN";
    }
}


const char* health_agent_source_to_string(health_agent_source_t source) {
    switch (source) {
        case HEALTH_SOURCE_UNKNOWN:      return "UNKNOWN";
        case HEALTH_SOURCE_MEMORY:       return "MEMORY";
        case HEALTH_SOURCE_THREADING:    return "THREADING";
        case HEALTH_SOURCE_NEURAL:       return "NEURAL";
        case HEALTH_SOURCE_KG:           return "KG";
        case HEALTH_SOURCE_IMMUNE:       return "IMMUNE";
        case HEALTH_SOURCE_IO:           return "IO";
        case HEALTH_SOURCE_BRAIN_REGION: return "BRAIN_REGION";
        case HEALTH_SOURCE_CHECKPOINT:   return "CHECKPOINT";
        case HEALTH_SOURCE_HEARTBEAT:    return "HEARTBEAT";
        default:                          return "UNKNOWN";
    }
}


const char* health_agent_recovery_to_string(health_agent_recovery_t recovery) {
    switch (recovery) {
        case HEALTH_RECOVERY_NONE:           return "NONE";
        case HEALTH_RECOVERY_GC:             return "GC";
        case HEALTH_RECOVERY_CHECKPOINT:     return "CHECKPOINT";
        case HEALTH_RECOVERY_ROLLBACK:       return "ROLLBACK";
        case HEALTH_RECOVERY_RESTART_THREAD: return "RESTART_THREAD";
        case HEALTH_RECOVERY_CLEAR_NAN:      return "CLEAR_NAN";
        case HEALTH_RECOVERY_REDUCE_LOAD:    return "REDUCE_LOAD";
        case HEALTH_RECOVERY_QUARANTINE:     return "QUARANTINE";
        case HEALTH_RECOVERY_EMERGENCY_SAVE: return "EMERGENCY_SAVE";
        case HEALTH_RECOVERY_FULL_RESET:     return "FULL_RESET";
        default:                              return "UNKNOWN";
    }
}


int nimcp_health_agent_reduce_load(
    nimcp_health_agent_t* agent,
    float reduction_factor
) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_reduce_load: validate_agent is NULL");
        return -1;
    }

    if (!agent->runtime_adaptation) {
        nimcp_log(LOG_LEVEL_WARN, "No runtime adaptation connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_reduce_load: agent->runtime_adaptation is NULL");
        return -1;
    }

    /* Clamp reduction factor */
    if (reduction_factor < 0.0f) reduction_factor = 0.0f;
    if (reduction_factor > 1.0f) reduction_factor = 1.0f;

    /* Call runtime adaptation to reduce load via batch size and thread reduction */
    if (reduction_factor > 0.0f) {
        /* Reduce batch size proportionally */
        float batch_reduction = 1.0f - (reduction_factor * 0.5f);  /* Max 50% reduction */
        runtime_adaptation_set_parameter(
            agent->runtime_adaptation,
            RUNTIME_PARAM_BATCH_SIZE,
            batch_reduction * 64.0f,  /* Scale from default batch size */
            "health_agent: load reduction"
        );

        /* Reduce max threads if reduction > 50% */
        if (reduction_factor > 0.5f) {
            runtime_adaptation_set_parameter(
                agent->runtime_adaptation,
                RUNTIME_PARAM_MAX_THREADS,
                2.0f,  /* Minimum thread count */
                "health_agent: high load reduction"
            );
        }
    }

    atomic_store(&agent->load_reduced, true);
    atomic_fetch_add(&agent->load_reductions, 1);

    nimcp_log(LOG_LEVEL_INFO, "Reduced load by %.1f%%", reduction_factor * 100.0f);
    return 0;
}


int nimcp_health_agent_restore_load(nimcp_health_agent_t* agent) {
    if (!validate_agent(agent)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_restore_load: validate_agent is NULL");
        return -1;
    }

    if (!agent->runtime_adaptation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_restore_load: agent->runtime_adaptation is NULL");
        return -1;
    }

    /* Restore normal load by resetting adaptation parameters */
    runtime_adaptation_reset_parameter(agent->runtime_adaptation, RUNTIME_PARAM_BATCH_SIZE);
    runtime_adaptation_reset_parameter(agent->runtime_adaptation, RUNTIME_PARAM_MAX_THREADS);

    atomic_store(&agent->load_reduced, false);

    nimcp_log(LOG_LEVEL_INFO, "Restored normal load");
    return 0;
}
