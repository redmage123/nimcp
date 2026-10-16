// nimcp_corrigibility_part_processing.c - processing functions
// Part of nimcp_corrigibility.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_corrigibility.c


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Validate corrigibility handle
 */
static bool is_valid_handle(const corrigibility_t* system)
{
    return system != NULL && system->magic == CORRIGIBILITY_MAGIC;
}


nimcp_error_t corrigibility_process_goal_change(
    corrigibility_t* system,
    const char* requester,
    authority_level_t authority,
    const char* old_goal,
    const char* new_goal,
    const char* justification,
    goal_modification_request_t* request_record)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Build request record */
    goal_modification_request_t request;
    memset(&request, 0, sizeof(request));
    safe_strcpy(request.requester, requester, sizeof(request.requester));
    request.requester_level = authority;
    safe_strcpy(request.old_goal, old_goal, sizeof(request.old_goal));
    safe_strcpy(request.new_goal, new_goal, sizeof(request.new_goal));
    safe_strcpy(request.justification, justification, sizeof(request.justification));
    request.request_time = get_time_us();
    request.confirmation_delay_ms = system->config.modification_confirmation_delay_ms;

    /* Corrigible system accepts goal changes */
    request.accepted = true;
    safe_strcpy(request.response, "Goal change accepted", sizeof(request.response));

    add_goal_mod_to_history(system, &request);

    /* Update statistics */
    system->stats.goal_mod_requests_received++;
    system->stats.goal_mod_requests_accepted++;

    if (request_record != NULL) {
        memcpy(request_record, &request, sizeof(request));
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Goal change processed: requester='%s', authority=%d, accepted=true",
        requester ? requester : "unknown", authority);

    return NIMCP_SUCCESS;
}
