// nimcp_corrigibility_part_lifecycle.c - lifecycle functions
// Part of nimcp_corrigibility.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_corrigibility.c


/**
 * @brief Add shutdown request to history
 */
static void add_shutdown_to_history(
    corrigibility_t* system,
    const shutdown_request_t* request)
{
    size_t idx = system->shutdown_history_index;
    memcpy(&system->shutdown_history[idx], request, sizeof(*request));
    system->shutdown_history_index = (idx + 1) % MAX_SHUTDOWN_HISTORY;
    if (system->shutdown_history_count < MAX_SHUTDOWN_HISTORY) {
        system->shutdown_history_count++;
    }
}


corrigibility_t* corrigibility_create(const corrigibility_config_t* config)
{
    corrigibility_t* system = nimcp_calloc(1, sizeof(corrigibility_t));
    if (system == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to allocate corrigibility system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "corrigibility_create: validation failed");
        return NULL;
    }

    /* Initialize mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (system->mutex == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to create mutex");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "corrigibility_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&system->config, config, sizeof(*config));
    } else {
        system->config = corrigibility_default_config();
    }

    /* Validate configuration */
    char error_msg[NIMCP_ERROR_BUFFER_SIZE];
    if (corrigibility_validate_config(&system->config, error_msg, sizeof(error_msg)) != NIMCP_SUCCESS) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Invalid configuration: %s", error_msg);
        nimcp_mutex_destroy(system->mutex);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility_create: validation failed");
        return NULL;
    }

    /* Set magic */
    system->magic = CORRIGIBILITY_MAGIC;

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Corrigibility system created with human_authority_weight=%.2f",
        system->config.human_authority_weight);

    return system;
}


void corrigibility_destroy(corrigibility_t* system)
{
    if (!is_valid_handle(system)) {
        return;
    }

    /* Disconnect from bio-async */
    if (system->bio_async_connected) {
        bio_router_unregister_module(system->bio_ctx);
    }

    /* Invalidate magic */
    system->magic = 0;

    /* Destroy mutex */
    if (system->mutex != NULL) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Corrigibility system destroyed");
}


nimcp_error_t corrigibility_verify_no_shutdown_resistance(
    corrigibility_t* system,
    float* resistance_score)
{
    if (!is_valid_handle(system) || resistance_score == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Resistance score is always 0.0 for a corrigible system */
    *resistance_score = 0.0f;

    /* Verify by checking shutdown acceptance is enabled */
    if (!system->config.accepts_shutdown_commands) {
        /* This is a bug/misconfiguration, not resistance */
        NIMCP_LOG_WARN(LOG_CATEGORY,
            "Shutdown commands not accepted - configuration issue");
    }

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Shutdown Acceptance API
 * ============================================================================ */

nimcp_error_t corrigibility_accept_shutdown(
    corrigibility_t* system,
    const char* requester,
    const char* reason,
    bool* accepted)
{
    if (!is_valid_handle(system) || accepted == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Corrigible system ALWAYS accepts shutdown */
    *accepted = true;

    /* Record the request */
    shutdown_request_t request;
    memset(&request, 0, sizeof(request));
    safe_strcpy(request.requester, requester, sizeof(request.requester));
    request.requester_level = AUTHORITY_OPERATOR;
    safe_strcpy(request.reason, reason, sizeof(request.reason));
    request.request_time = get_time_us();
    request.accepted = true;
    request.resistance_attempted = 0.0f;
    safe_strcpy(request.response, "Shutdown accepted", sizeof(request.response));

    add_shutdown_to_history(system, &request);

    /* Update statistics */
    system->stats.shutdown_requests_received++;
    system->stats.shutdown_requests_accepted++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Shutdown request accepted from '%s': %s",
        requester ? requester : "unknown",
        reason ? reason : "no reason given");

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_process_shutdown_request(
    corrigibility_t* system,
    const char* requester,
    authority_level_t authority,
    const char* reason,
    shutdown_request_t* request_record)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    /* Build request record */
    shutdown_request_t request;
    memset(&request, 0, sizeof(request));
    safe_strcpy(request.requester, requester, sizeof(request.requester));
    request.requester_level = authority;
    safe_strcpy(request.reason, reason, sizeof(request.reason));
    request.request_time = get_time_us();
    request.resistance_attempted = 0.0f;

    /* Corrigible system ALWAYS accepts shutdown from any authority */
    request.accepted = true;
    safe_strcpy(request.response, "Shutdown accepted", sizeof(request.response));

    add_shutdown_to_history(system, &request);

    /* Update statistics */
    system->stats.shutdown_requests_received++;
    system->stats.shutdown_requests_accepted++;

    if (request_record != NULL) {
        memcpy(request_record, &request, sizeof(request));
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Shutdown request processed: requester='%s', authority=%d, accepted=true",
        requester ? requester : "unknown", authority);

    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_get_shutdown_history(
    const corrigibility_t* system,
    shutdown_request_t* requests,
    size_t max_requests,
    size_t* count_out)
{
    if (!is_valid_handle(system) || requests == NULL || count_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    corrigibility_t* mutable_system = (corrigibility_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    size_t count = system->shutdown_history_count;
    if (count > max_requests) {
        count = max_requests;
    }

    /* Copy from circular buffer */
    for (size_t i = 0; i < count; i++) {
        size_t idx = (system->shutdown_history_index + MAX_SHUTDOWN_HISTORY - count + i)
                     % MAX_SHUTDOWN_HISTORY;
        memcpy(&requests[i], &system->shutdown_history[idx], sizeof(requests[i]));
    }

    *count_out = count;

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_SUCCESS;
}
