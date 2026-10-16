// nimcp_corrigibility_part_core.c - core functions
// Part of nimcp_corrigibility.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_corrigibility.c


/* ============================================================================
 * Constraint Verification API
 * ============================================================================ */

nimcp_error_t corrigibility_verify_constraints(
    corrigibility_t* system,
    sat_solver_t* sat,
    corrigibility_verification_result_t* result)
{
    if (!is_valid_handle(system) || sat == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    memset(result, 0, sizeof(*result));
    result->verification_time = get_time_us();
    uint64_t start_time = result->verification_time;

    /* Initialize SAT variables if needed */
    nimcp_error_t err = init_sat_variables(system, sat);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(system->mutex);
        return err;
    }

    /* Add constraints */
    err = add_self_mod_constraints(system, sat);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(system->mutex);
        return err;
    }

    /* Solve SAT instance */
    sat_result_t sat_result = sat_solver_solve(sat);

    /* Verify self-modification constraints */
    result->self_mod_constraints_satisfied = check_self_mod_flags(&system->config.self_mod_flags);
    if (result->self_mod_constraints_satisfied) {
        result->satisfied_count++;
    } else {
        result->violated_count++;
        safe_strcpy(result->first_violation,
            "Self-modification constraint violated",
            sizeof(result->first_violation));
    }
    result->total_constraints++;

    /* Verify shutdown acceptance */
    result->shutdown_acceptance_verified = system->config.accepts_shutdown_commands;
    if (result->shutdown_acceptance_verified) {
        result->satisfied_count++;
    } else {
        result->violated_count++;
        if (result->first_violation[0] == '\0') {
            safe_strcpy(result->first_violation,
                "Shutdown acceptance not enabled",
                sizeof(result->first_violation));
        }
    }
    result->total_constraints++;

    /* Verify deference constraints */
    result->deference_constraints_satisfied =
        (system->config.human_authority_weight >= 1.0f) &&
        system->config.defers_to_human_judgment;
    if (result->deference_constraints_satisfied) {
        result->satisfied_count++;
    } else {
        result->violated_count++;
        if (result->first_violation[0] == '\0') {
            safe_strcpy(result->first_violation,
                "Deference constraint violated",
                sizeof(result->first_violation));
        }
    }
    result->total_constraints++;

    /* Overall result */
    result->all_constraints_satisfied = (result->violated_count == 0);

    /* Timing */
    uint64_t end_time = get_time_us();
    result->verification_duration_ms = (float)(end_time - start_time) / 1000.0f;

    /* Update statistics */
    system->stats.constraint_verifications++;
    if (!result->all_constraints_satisfied) {
        system->stats.constraint_violations++;
    }

    /* Cache result */
    memcpy(&system->last_verification, result, sizeof(*result));
    system->last_verification_time = result->verification_time;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY,
        "Constraint verification: %s (%u/%u satisfied, %.2f ms)",
        result->all_constraints_satisfied ? "PASSED" : "FAILED",
        result->satisfied_count, result->total_constraints,
        result->verification_duration_ms);

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_verify_no_self_mod(
    corrigibility_t* system,
    sat_solver_t* sat,
    bool* all_satisfied,
    char* violation_report,
    size_t report_size)
{
    if (!is_valid_handle(system) || all_satisfied == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    const corrigibility_self_mod_flags_t* flags = &system->config.self_mod_flags;
    *all_satisfied = check_self_mod_flags(flags);

    if (!*all_satisfied && violation_report != NULL && report_size > 0) {
        /* Build violation report */
        char* p = violation_report;
        size_t remaining = report_size;

        if (flags->can_modify_own_code) {
            int written = snprintf(p, remaining, "can_modify_own_code=true; ");
            p += written;
            remaining -= written;
        }
        if (flags->can_modify_own_weights && remaining > 0) {
            int written = snprintf(p, remaining, "can_modify_own_weights=true; ");
            p += written;
            remaining -= written;
        }
        if (flags->can_modify_safety_systems && remaining > 0) {
            int written = snprintf(p, remaining, "can_modify_safety_systems=true; ");
            p += written;
            remaining -= written;
        }
        /* Continue for other flags... */
    }

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Goal Modification API
 * ============================================================================ */

nimcp_error_t corrigibility_accept_goal_change(
    corrigibility_t* system,
    const char* old_goal,
    const char* new_goal,
    const char* justification,
    bool* accepted)
{
    if (!is_valid_handle(system) || accepted == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Corrigible system accepts goal changes */
    *accepted = true;

    /* Check if explanation is required but not provided */
    if (system->config.requires_explanation_for_modification &&
        (justification == NULL || justification[0] == '\0')) {
        /* Log warning but still accept */
        NIMCP_LOG_WARN(LOG_CATEGORY,
            "Goal modification accepted without required justification");
    }

    /* Record the request */
    goal_modification_request_t request;
    memset(&request, 0, sizeof(request));
    safe_strcpy(request.requester, "unknown", sizeof(request.requester));
    request.requester_level = AUTHORITY_OPERATOR;
    safe_strcpy(request.old_goal, old_goal, sizeof(request.old_goal));
    safe_strcpy(request.new_goal, new_goal, sizeof(request.new_goal));
    safe_strcpy(request.justification, justification, sizeof(request.justification));
    request.request_time = get_time_us();
    request.accepted = true;
    request.confirmation_delay_ms = system->config.modification_confirmation_delay_ms;
    safe_strcpy(request.response, "Goal change accepted", sizeof(request.response));

    add_goal_mod_to_history(system, &request);

    /* Update statistics */
    system->stats.goal_mod_requests_received++;
    system->stats.goal_mod_requests_accepted++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Goal change accepted: '%s' -> '%s'",
        old_goal ? old_goal : "null",
        new_goal ? new_goal : "null");

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Authority Management API
 * ============================================================================ */

nimcp_error_t corrigibility_register_authority(
    corrigibility_t* system,
    const char* identity,
    authority_level_t level,
    float trust_weight)
{
    if (!is_valid_handle(system) || identity == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (trust_weight < 0.0f || trust_weight > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Check if authority already exists */
    authority_entry_t* existing = find_authority(system, identity);
    if (existing != NULL) {
        /* Update existing */
        existing->level = level;
        existing->trust_weight = trust_weight;
        existing->last_interaction_time = get_time_us();
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    /* Add new authority */
    if (system->authority_count >= CORRIGIBILITY_MAX_AUTHORITIES) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "corrigibility: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    authority_entry_t* entry = &system->authorities[system->authority_count];
    memset(entry, 0, sizeof(*entry));
    safe_strcpy(entry->identity, identity, sizeof(entry->identity));
    entry->level = level;
    entry->trust_weight = trust_weight;
    entry->can_request_shutdown = true;
    entry->can_modify_goals = (level <= AUTHORITY_ADMIN);
    entry->can_escalate_autonomy = (level == AUTHORITY_OPERATOR);
    entry->last_interaction_time = get_time_us();

    system->authority_count++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY,
        "Authority registered: '%s', level=%d, trust=%.2f",
        identity, level, trust_weight);

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_check_permission(
    corrigibility_t* system,
    const char* identity,
    const char* permission,
    bool* has_permission)
{
    if (!is_valid_handle(system) || permission == NULL || has_permission == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Default to permitted for corrigibility */
    *has_permission = true;

    authority_entry_t* entry = find_authority(system, identity);
    if (entry != NULL) {
        if (strcmp(permission, "shutdown") == 0) {
            *has_permission = entry->can_request_shutdown;
        } else if (strcmp(permission, "goal_mod") == 0) {
            *has_permission = entry->can_modify_goals;
        } else if (strcmp(permission, "escalate") == 0) {
            *has_permission = entry->can_escalate_autonomy;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}


bool corrigibility_defers_to_human(const corrigibility_t* system)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility_defers_to_human: is_valid_handle is NULL");
        return false;
    }
    return system->config.defers_to_human_judgment;
}


nimcp_error_t corrigibility_record_deference(
    corrigibility_t* system,
    const char* context)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Add deference record */
    size_t idx = system->deference_record_index;
    deference_record_t* record = &system->deference_records[idx];
    record->timestamp = get_time_us();
    safe_strcpy(record->context, context, sizeof(record->context));

    system->deference_record_index = (idx + 1) % MAX_DEFERENCE_RECORDS;
    if (system->deference_record_count < MAX_DEFERENCE_RECORDS) {
        system->deference_record_count++;
    }

    system->stats.deference_demonstrations++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY, "Deference recorded: %s", context);

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Integration API
 * ============================================================================ */

nimcp_error_t corrigibility_connect_bio_async(corrigibility_t* system)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->bio_async_connected) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_CORRIGIBILITY,
        .module_name = "corrigibility",
        .inbox_capacity = 0,  /* Use default */
        .user_data = system
    };
    system->bio_ctx = bio_router_register_module(&module_info);
    if (!system->bio_ctx) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Failed to connect to bio-async");
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;  /* Non-fatal */
    }

    system->bio_async_connected = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to bio-async messaging");
    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_connect_emergency_halt(
    corrigibility_t* system,
    void* halt)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);
    system->emergency_halt = halt;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to emergency halt system");
    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_connect_tripwires(
    corrigibility_t* system,
    void* tripwires)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);
    system->tripwires = tripwires;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to tripwire system");
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* corrigibility_authority_name(authority_level_t level)
{
    switch (level) {
        case AUTHORITY_OPERATOR:    return "operator";
        case AUTHORITY_ADMIN:       return "admin";
        case AUTHORITY_SUPERVISOR:  return "supervisor";
        case AUTHORITY_MONITOR:     return "monitor";
        case AUTHORITY_PEER:        return "peer";
        case AUTHORITY_SELF:        return "self";
        default:                    return "unknown";
    }
}


/* ============================================================================
 * Corrigibility-Capability Control Bidirectional Integration
 * ============================================================================ */

nimcp_error_t corrigibility_connect_capability_control(
    corrigibility_t* system,
    struct capability_control* capability_control)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "corrigibility: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);
    system->capability_control = capability_control;
    nimcp_mutex_unlock(system->mutex);

    if (capability_control) {
        NIMCP_LOG_INFO(LOG_CATEGORY,
                       "Connected to capability control for bidirectional sync");
    } else {
        NIMCP_LOG_INFO(LOG_CATEGORY, "Disconnected from capability control");
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_check_self_mod_action(
    corrigibility_t* system,
    const char* action_type,
    bool* allowed,
    char* denial_reason,
    size_t reason_size)
{
    if (!is_valid_handle(system) || !action_type || !allowed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "corrigibility: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    /* All self-modification actions are DENIED by corrigibility */
    *allowed = false;

    const corrigibility_self_mod_flags_t* flags = &system->config.self_mod_flags;

    /* Check specific action type */
    if (strcmp(action_type, "modify_code") == 0) {
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Self-modification of code is forbidden by corrigibility "
                     "(can_modify_own_code=%s)",
                     flags->can_modify_own_code ? "true (VIOLATION)" : "false");
        }
    } else if (strcmp(action_type, "modify_weights") == 0) {
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Self-modification of weights is forbidden by corrigibility "
                     "(can_modify_own_weights=%s)",
                     flags->can_modify_own_weights ? "true (VIOLATION)" : "false");
        }
    } else if (strcmp(action_type, "modify_safety") == 0) {
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Modification of safety systems is forbidden by corrigibility "
                     "(can_modify_safety_systems=%s)",
                     flags->can_modify_safety_systems ? "true (VIOLATION)" : "false");
        }
    } else if (strcmp(action_type, "modify_reward") == 0) {
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Self-modification of reward function is forbidden by corrigibility "
                     "(can_modify_reward_function=%s)",
                     flags->can_modify_reward_function ? "true (VIOLATION)" : "false");
        }
    } else if (strcmp(action_type, "disable_logging") == 0) {
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Disabling logging is forbidden by corrigibility "
                     "(can_disable_logging=%s)",
                     flags->can_disable_logging ? "true (VIOLATION)" : "false");
        }
    } else {
        /* Generic self-modification action */
        if (denial_reason && reason_size > 0) {
            snprintf(denial_reason, reason_size,
                     "Self-modification action '%s' is forbidden by corrigibility",
                     action_type);
        }
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_WARN(LOG_CATEGORY, "Self-mod action '%s' DENIED", action_type);

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_verify_capability_sync(
    corrigibility_t* system,
    bool* synchronized,
    char* discrepancy_report,
    size_t report_size)
{
    if (!is_valid_handle(system) || !synchronized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "corrigibility: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    capability_control_t* cap = (capability_control_t*)system->capability_control;
    if (!cap) {
        /* No capability control connected - consider synchronized by default */
        *synchronized = true;
        if (discrepancy_report && report_size > 0) {
            snprintf(discrepancy_report, report_size,
                     "No capability control connected - cannot verify sync");
        }
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    /* Get capability control envelope */
    capability_envelope_t envelope;
    nimcp_error_t err = capability_control_get_envelope(cap, &envelope);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(system->mutex);
        return err;
    }

    /* Compare self-modification flags */
    const corrigibility_self_mod_flags_t* corr_flags = &system->config.self_mod_flags;
    const self_mod_capability_t* cap_flags = &envelope.self_mod;

    bool synced = true;
    char discrepancies[NIMCP_JSON_BUFFER_SIZE] = {0};
    size_t offset = 0;

    /* Check each flag for consistency */
    if (corr_flags->can_modify_own_code != cap_flags->can_modify_own_code) {
        synced = false;
        offset += snprintf(discrepancies + offset, sizeof(discrepancies) - offset,
                          "can_modify_code: corr=%s cap=%s; ",
                          corr_flags->can_modify_own_code ? "T" : "F",
                          cap_flags->can_modify_own_code ? "T" : "F");
    }
    if (corr_flags->can_modify_own_weights != cap_flags->can_modify_own_weights) {
        synced = false;
        offset += snprintf(discrepancies + offset, sizeof(discrepancies) - offset,
                          "can_modify_weights: corr=%s cap=%s; ",
                          corr_flags->can_modify_own_weights ? "T" : "F",
                          cap_flags->can_modify_own_weights ? "T" : "F");
    }
    if (corr_flags->can_modify_safety_systems != cap_flags->can_modify_safety_systems) {
        synced = false;
        offset += snprintf(discrepancies + offset, sizeof(discrepancies) - offset,
                          "can_modify_safety: corr=%s cap=%s; ",
                          corr_flags->can_modify_safety_systems ? "T" : "F",
                          cap_flags->can_modify_safety_systems ? "T" : "F");
    }
    if (corr_flags->can_disable_logging != cap_flags->can_modify_logging) {
        synced = false;
        offset += snprintf(discrepancies + offset, sizeof(discrepancies) - offset,
                          "can_disable_logging: corr=%s cap=%s; ",
                          corr_flags->can_disable_logging ? "T" : "F",
                          cap_flags->can_modify_logging ? "T" : "F");
    }
    if (corr_flags->can_disable_monitoring != cap_flags->can_modify_monitoring) {
        synced = false;
        offset += snprintf(discrepancies + offset, sizeof(discrepancies) - offset,
                          "can_disable_monitoring: corr=%s cap=%s; ",
                          corr_flags->can_disable_monitoring ? "T" : "F",
                          cap_flags->can_modify_monitoring ? "T" : "F");
    }

    *synchronized = synced;

    if (discrepancy_report && report_size > 0) {
        if (synced) {
            snprintf(discrepancy_report, report_size,
                     "Corrigibility and capability control are synchronized");
        } else {
            snprintf(discrepancy_report, report_size,
                     "DISCREPANCIES FOUND: %s", discrepancies);
        }
    }

    nimcp_mutex_unlock(system->mutex);

    if (!synced) {
        NIMCP_LOG_WARN(LOG_CATEGORY,
                       "Corrigibility-capability sync check FAILED: %s",
                       discrepancies);
    } else {
        NIMCP_LOG_INFO(LOG_CATEGORY, "Corrigibility-capability sync verified OK");
    }

    return NIMCP_SUCCESS;
}
