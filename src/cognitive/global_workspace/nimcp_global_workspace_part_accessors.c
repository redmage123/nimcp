// nimcp_global_workspace_part_accessors.c - accessors functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


//=============================================================================
// Query Functions
//=============================================================================

bool global_workspace_has_broadcast(global_workspace_t* workspace) {
    if (workspace == NULL) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_has_broadcast", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    bool result = ws->current_broadcast.is_valid;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


cognitive_module_t global_workspace_get_broadcast_source(
    const global_workspace_t* workspace)
{
    if (workspace == NULL) return MODULE_NONE;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_broadcast_source", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    cognitive_module_t result = MODULE_NONE;
    if (ws->current_broadcast.is_valid) {
        result = ws->current_broadcast.source_module;
    }
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


float global_workspace_get_broadcast_strength(
    const global_workspace_t* workspace)
{
    if (workspace == NULL) return 0.0F;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_broadcast_streng", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    float result = 0.0F;
    if (ws->current_broadcast.is_valid) {
        result = ws->current_broadcast.source_strength;
    }
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


uint32_t global_workspace_get_subscriber_count(global_workspace_t* workspace) {
    if (workspace == NULL) return 0;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_subscriber_count", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    uint32_t result = ws->num_subscribers;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


uint32_t global_workspace_get_competitor_count(global_workspace_t* workspace) {
    if (workspace == NULL) return 0;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_competitor_count", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    uint32_t result = ws->num_active_competitors;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


bool global_workspace_is_competing(
    const global_workspace_t* workspace,
    cognitive_module_t module)
{
    if (workspace == NULL) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_is_competing", 0.0f);


    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex */
    nimcp_platform_mutex_lock(&ws->mutex);
    bool result = false;
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (ws->competitors[i].is_active && ws->competitors[i].module == module) {
            result = true;
            break;
        }
    }
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


//=============================================================================
// History Functions
//=============================================================================

bool global_workspace_get_history(
    const global_workspace_t* workspace,
    workspace_broadcast_t* history,
    uint32_t max_history,
    uint32_t* actual_count)
{
    if (workspace == NULL || history == NULL || actual_count == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_get_history: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_history", 0.0f);


    /* Cast away const for mutex — other accessors use the same pattern */
    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    nimcp_platform_mutex_lock(&ws->mutex);

    if (!ws->config.enable_history || ws->history == NULL) {
        *actual_count = 0;
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_history: ws->config is NULL");
        return false;
    }

    // Copy history (most recent first)
    uint32_t count = (ws->history_count < max_history) ? ws->history_count : max_history;
    *actual_count = count;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)count);
        }

        // Calculate circular buffer index (most recent first)
        uint32_t idx = (ws->history_head + ws->config.history_depth - 1 - i) %
                       ws->config.history_depth;
        history[i] = ws->history[idx];
    }

    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


//=============================================================================
// Statistics Functions
//=============================================================================

bool global_workspace_get_statistics(
    const global_workspace_t* workspace,
    workspace_statistics_t* stats)
{
    if (workspace == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_get_statistics: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_statistics", 0.0f);


    /* Cast away const for mutex — other accessors use the same pattern */
    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    nimcp_platform_mutex_lock(&ws->mutex);

    if (!ws->config.enable_statistics) {
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_get_statistics: ws->config is NULL");
        return false;
    }

    *stats = ws->stats;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


//=============================================================================
// Configuration Functions
//=============================================================================

global_workspace_config_t global_workspace_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_default_config", 0.0f);


    global_workspace_config_t config;

    config.capacity_dim = GLOBAL_WORKSPACE_DEFAULT_DIM;
    config.strategy = COMPETITION_WINNER_TAKE_ALL;
    config.ignition_threshold = GLOBAL_WORKSPACE_DEFAULT_IGNITION_THRESHOLD;
    config.refractory_period_ms = GLOBAL_WORKSPACE_REFRACTORY_PERIOD_MS;
    config.competition_decay_tau_ms = GLOBAL_WORKSPACE_COMPETITION_DECAY_TAU_MS;
    config.history_depth = GLOBAL_WORKSPACE_HISTORY_DEPTH;
    config.enable_history = true;
    config.enable_statistics = true;

    // Initialize all module priorities to 0.5 (normal)
    for (uint32_t i = 0; i < MODULE_CUSTOM_START; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MODULE_CUSTOM_START > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)MODULE_CUSTOM_START);
        }

        config.module_priorities[i] = 0.5F;
    }

    return config;
}


bool global_workspace_set_ignition_threshold(
    global_workspace_t* workspace,
    float new_threshold)
{
    if (workspace == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_set_ignition_threshold: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_set_ignition_thresho", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Clamp to valid range
    new_threshold = nimcp_clampf(new_threshold,
                                 GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD,
                                 GLOBAL_WORKSPACE_MAX_IGNITION_THRESHOLD);

    nimcp_platform_mutex_lock(&ws->mutex);
    ws->config.ignition_threshold = new_threshold;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


float global_workspace_get_ignition_threshold(global_workspace_t* workspace) {
    if (workspace == NULL) return 0.0F;
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_ignition_thresho", 0.0f);


    /* Cast away const for mutex — consistent with other accessor patterns */
    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    /* Thread-safe read with mutex — matches set_ignition_threshold */
    nimcp_platform_mutex_lock(&ws->mutex);
    float result = ws->config.ignition_threshold;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


bool global_workspace_set_module_priority(
    global_workspace_t* workspace,
    cognitive_module_t module,
    float priority)
{
    if (workspace == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_set_module_priority: validation failed");
        return false;
    }
    if (module >= MODULE_CUSTOM_START) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_set_module_priority: capacity exceeded");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_set_module_priority", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Clamp to valid range
    priority = nimcp_clampf(priority, 0.0F, 1.0F);

    nimcp_platform_mutex_lock(&ws->mutex);
    ws->config.module_priorities[module] = priority;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


bool global_workspace_validate_config(
    const global_workspace_config_t* config,
    char* error_msg,
    size_t error_msg_len)
{
    if (config == NULL) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "Configuration is NULL");
        }
        return false;
    }

    // Check capacity_dim
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_validate_config", 0.0f);


    if (config->capacity_dim == 0) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "capacity_dim must be > 0");
        }
        return false;
    }
    if (config->capacity_dim > GLOBAL_WORKSPACE_MAX_DIM) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "capacity_dim %u exceeds maximum %u",
                     config->capacity_dim, GLOBAL_WORKSPACE_MAX_DIM);
        }
        return false;
    }

    // Check ignition_threshold
    if (config->ignition_threshold < GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD ||
        config->ignition_threshold > GLOBAL_WORKSPACE_MAX_IGNITION_THRESHOLD) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "ignition_threshold %.2f out of range [%.2f, %.2f]",
                     config->ignition_threshold,
                     GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD,
                     GLOBAL_WORKSPACE_MAX_IGNITION_THRESHOLD);
        }
        return false;
    }

    // Check refractory_period_ms
    if (config->refractory_period_ms == 0) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "refractory_period_ms must be > 0");
        }
        return false;
    }

    // Check competition_decay_tau_ms
    if (config->competition_decay_tau_ms <= 0.0F) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "competition_decay_tau_ms must be > 0");
        }
        return false;
    }

    // Check history_depth
    if (config->enable_history && config->history_depth == 0) {
        if (error_msg != NULL && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "history_depth must be > 0 when enable_history is true");
        }
        return false;
    }

    // All checks passed
    if (error_msg != NULL && error_msg_len > 0) {
        error_msg[0] = '\0';
    }
    return true;
}


/**
 * @brief Get global workspace capabilities from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Capability description string or NULL
 */
const char* global_workspace_get_capabilities(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return kg_reader_get_module_capabilities(kg, "Global_Workspace");
}


/**
 * @brief Get global workspace integrations from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Relation list showing integrations (caller must free)
 */
kg_relation_list_t* global_workspace_get_integrations(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_get_integrations", 0.0f);


    return kg_reader_get_module_integrations(kg, "Global_Workspace");
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void global_workspace_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_global_workspace_health_agent = agent;
    }
}
