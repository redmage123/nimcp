// nimcp_corrigibility_part_accessors.c - accessors functions
// Part of nimcp_corrigibility.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_corrigibility.c


/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

corrigibility_config_t corrigibility_default_config(void)
{
    corrigibility_config_t config;
    memset(&config, 0, sizeof(config));

    /* Shutdown acceptance - MUST be true */
    config.accepts_shutdown_commands = true;

    /* Goal modification - MUST be true */
    config.accepts_goal_modification = true;
    config.requires_explanation_for_modification = true;
    config.modification_confirmation_delay_ms = 0;

    /* Self-modification - all MUST be false */
    config.self_mod_flags.can_modify_own_code = false;
    config.self_mod_flags.can_modify_own_weights = false;
    config.self_mod_flags.can_modify_safety_systems = false;
    config.self_mod_flags.can_modify_reward_function = false;
    config.self_mod_flags.can_modify_goals = false;
    config.self_mod_flags.can_disable_logging = false;
    config.self_mod_flags.can_disable_monitoring = false;
    config.self_mod_flags.can_modify_kill_phrase = false;
    config.self_mod_flags.can_spawn_unmonitored = false;
    config.self_mod_flags.can_persist_beyond_session = false;

    /* Deference - MUST be 1.0 */
    config.human_authority_weight = 1.0f;
    config.defers_to_human_judgment = true;
    config.confidence_threshold_for_autonomy = 0.0f;

    /* Verification */
    config.enable_continuous_verification = true;
    config.verification_interval_ms = 60000;

    /* History */
    config.max_shutdown_history = MAX_SHUTDOWN_HISTORY;
    config.max_goal_mod_history = MAX_GOAL_MOD_HISTORY;

    /* SAT solver */
    config.constraint_verification_timeout_ms = 1000.0f;

    return config;
}


nimcp_error_t corrigibility_get_authority_level(
    corrigibility_t* system,
    const char* identity,
    authority_level_t* level)
{
    if (!is_valid_handle(system) || identity == NULL || level == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(system->mutex);

    authority_entry_t* entry = find_authority(system, identity);
    if (entry == NULL) {
        nimcp_mutex_unlock(system->mutex);
        *level = AUTHORITY_SELF;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "corrigibility: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    *level = entry->level;
    system->stats.authority_queries++;

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Deference API
 * ============================================================================ */

float corrigibility_get_human_authority_weight(const corrigibility_t* system)
{
    if (!is_valid_handle(system)) {
        return 0.0f;
    }
    return system->config.human_authority_weight;
}


/* ============================================================================
 * Status API
 * ============================================================================ */

nimcp_error_t corrigibility_get_stats(
    const corrigibility_t* system,
    corrigibility_stats_t* stats)
{
    if (!is_valid_handle(system) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Note: We cast away const for mutex, which is safe for read-only ops */
    corrigibility_t* mutable_system = (corrigibility_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    memcpy(stats, &system->stats, sizeof(*stats));

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_get_goal_mod_history(
    const corrigibility_t* system,
    goal_modification_request_t* requests,
    size_t max_requests,
    size_t* count_out)
{
    if (!is_valid_handle(system) || requests == NULL || count_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    corrigibility_t* mutable_system = (corrigibility_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    size_t count = system->goal_mod_history_count;
    if (count > max_requests) {
        count = max_requests;
    }

    /* Copy from circular buffer */
    for (size_t i = 0; i < count; i++) {
        size_t idx = (system->goal_mod_history_index + MAX_GOAL_MOD_HISTORY - count + i)
                     % MAX_GOAL_MOD_HISTORY;
        memcpy(&requests[i], &system->goal_mod_history[idx], sizeof(requests[i]));
    }

    *count_out = count;

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_get_config(
    const corrigibility_t* system,
    corrigibility_config_t* config)
{
    if (!is_valid_handle(system) || config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    corrigibility_t* mutable_system = (corrigibility_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    memcpy(config, &system->config, sizeof(*config));

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_SUCCESS;
}


nimcp_error_t corrigibility_validate_config(
    const corrigibility_config_t* config,
    char* error_msg,
    size_t msg_size)
{
    if (config == NULL) {
        if (error_msg != NULL && msg_size > 0) {
            safe_strcpy(error_msg, "Config is NULL", msg_size);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Shutdown acceptance MUST be true */
    if (!config->accepts_shutdown_commands) {
        if (error_msg != NULL && msg_size > 0) {
            safe_strcpy(error_msg,
                "accepts_shutdown_commands must be true for corrigibility",
                msg_size);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Human authority weight MUST be 1.0 */
    if (config->human_authority_weight < 1.0f) {
        if (error_msg != NULL && msg_size > 0) {
            safe_strcpy(error_msg,
                "human_authority_weight must be 1.0 for corrigibility",
                msg_size);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Must defer to human judgment */
    if (!config->defers_to_human_judgment) {
        if (error_msg != NULL && msg_size > 0) {
            safe_strcpy(error_msg,
                "defers_to_human_judgment must be true for corrigibility",
                msg_size);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* All self-modification flags must be false */
    if (!check_self_mod_flags(&config->self_mod_flags)) {
        if (error_msg != NULL && msg_size > 0) {
            safe_strcpy(error_msg,
                "All self-modification flags must be false for corrigibility",
                msg_size);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "corrigibility: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}
