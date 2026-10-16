// nimcp_distributed_cognition_part_accessors.c - accessors functions
// Part of nimcp_distributed_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_distributed_cognition.c


/**
 * @brief Load configuration from config module
 *
 * WHY: Make all hyperparameters configurable at runtime
 * HOW: Query config module for each parameter with fallback to defaults
 */
static void load_configuration(distrib_cognition_config_t* config)
{
    if (!config) return;

    // Start with defaults
    *config = DEFAULT_CONFIG;

    // Override with config module values if available
    bool enable_neuromod = config_get_bool(CONFIG_KEY_ENABLE_NEUROMOD_SYNC, config->enable_neuromod_sync);
    config->enable_neuromod_sync = enable_neuromod;
    LOG_MODULE_DEBUG(MODULE_NAME, "Config: enable_neuromod_sync=%d", enable_neuromod);

    int64_t neuromod_interval = config_get_int(CONFIG_KEY_NEUROMOD_INTERVAL, config->neuromod_broadcast_interval_ms);
    if (neuromod_interval > 0) {
        config->neuromod_broadcast_interval_ms = (uint32_t)neuromod_interval;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: neuromod_interval=%d ms", (int)neuromod_interval);
    }

    double diffusion_rate = config_get_float(CONFIG_KEY_NEUROMOD_DIFFUSION, config->neuromod_diffusion_rate);
    if (diffusion_rate >= 0.0 && diffusion_rate <= 1.0) {
        config->neuromod_diffusion_rate = (float)diffusion_rate;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: neuromod_diffusion_rate=%.3f", diffusion_rate);
    }

    bool enable_glial = config_get_bool(CONFIG_KEY_ENABLE_GLIAL_SYNC, config->enable_glial_sync);
    config->enable_glial_sync = enable_glial;
    LOG_MODULE_DEBUG(MODULE_NAME, "Config: enable_glial_sync=%d", enable_glial);

    int64_t glial_interval = config_get_int(CONFIG_KEY_GLIAL_INTERVAL, config->glial_sync_interval_ms);
    if (glial_interval > 0) {
        config->glial_sync_interval_ms = (uint32_t)glial_interval;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: glial_interval=%d ms", (int)glial_interval);
    }

    bool enable_region = config_get_bool(CONFIG_KEY_ENABLE_REGION_SYNC, config->enable_region_sync);
    config->enable_region_sync = enable_region;
    LOG_MODULE_DEBUG(MODULE_NAME, "Config: enable_region_sync=%d", enable_region);

    int64_t region_interval = config_get_int(CONFIG_KEY_REGION_INTERVAL, config->region_sync_interval_ms);
    if (region_interval > 0) {
        config->region_sync_interval_ms = (uint32_t)region_interval;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: region_interval=%d ms", (int)region_interval);
    }

    int64_t max_queue = config_get_int(CONFIG_KEY_MAX_MESSAGE_QUEUE, config->max_message_queue);
    if (max_queue > 0) {
        config->max_message_queue = (uint32_t)max_queue;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: max_message_queue=%d", (int)max_queue);
    }
}


bool distrib_cognition_get_stats(
    distrib_cognition_t dc,
    distrib_cognition_stats_t* stats)
{
    if (!dc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_get_stats: required parameter is NULL (dc, stats)");
        return false;
    }

    nimcp_rwlock_rdlock(&dc->rwlock);
    *stats = dc->stats;
    nimcp_rwlock_unlock(&dc->rwlock);

    return true;
}


bool distrib_cognition_set_sync_mode(
    distrib_cognition_t dc,
    sync_mode_t mode)
{
    if (!dc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_set_sync_mode: dc is NULL");
        return false;
    }

    if (mode < SYNC_MODE_DISABLED || mode > SYNC_MODE_BIDIRECTIONAL) {
        log_message(LOG_LEVEL_ERROR, "[distributed_cognition] Invalid sync mode: %d", mode);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "distrib_cognition_set_sync_mode: validation failed");
        return false;
    }

    nimcp_rwlock_wrlock(&dc->rwlock);
    dc->config.sync_mode = mode;
    nimcp_rwlock_unlock(&dc->rwlock);

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Sync mode set to %d", mode);

    return true;
}
