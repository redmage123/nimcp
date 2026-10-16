// nimcp_rcog_engine_part_accessors.c - accessors functions
// Part of nimcp_rcog_engine.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_engine.c


void rcog_engine_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_engine_instance_health_agent = agent;
}


static void apply_modulation_to_config(rcog_engine_t* engine) {
    /* Apply immune modulation to effective limits */
    rcog_immune_modulation_t* mod = &engine->current_modulation;

    if (engine->orchestrator) {
        rcog_orchestrator_apply_immune_modulation(engine->orchestrator, mod);
    }
    if (engine->delegation_pool) {
        rcog_delegation_pool_apply_immune_modulation(engine->delegation_pool, mod);
    }
}


/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_engine_config_t rcog_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_default_config", 0.0f);


    rcog_engine_config_t config;
    memset(&config, 0, sizeof(config));

    /* Processing limits */
    config.max_recursion_depth = RCOG_DEFAULT_MAX_DEPTH;
    config.max_parallel_subtasks = RCOG_DEFAULT_MAX_PARALLEL_SUBTASKS;
    config.max_concurrent_goals = RCOG_ENGINE_MAX_CONCURRENT_GOALS;
    config.default_timeout_ms = RCOG_ENGINE_DEFAULT_TIMEOUT_MS;

    /* Answer thresholds */
    config.confidence_threshold = RCOG_ENGINE_DEFAULT_CONFIDENCE_THRESHOLD;
    config.max_refinement_steps = RCOG_DEFAULT_MAX_REFINEMENT_STEPS;
    config.enable_early_termination = true;

    /* Decomposition */
    config.default_strategy = RCOG_DECOMP_ADAPTIVE;
    config.enable_adaptive_strategy = true;

    /* Integration - disabled by default */
    config.enable_bio_async = false;
    config.enable_immune_modulation = false;
    config.enable_imagination = false;
    config.enable_collective = false;
    config.enable_brain_kg = false;

    /* Builtin tools - enabled by default */
    config.register_l1_builtins = true;
    config.register_l2_builtins = true;
    config.register_l3_builtins = true;

    /* Debug */
    config.enable_tracing = false;
    config.verbose_logging = false;

    return config;
}


rcog_engine_state_t rcog_engine_get_state(const rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ENGINE_UNINITIALIZED;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_get_state", 0.0f);


    return engine->state;
}


/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

int rcog_engine_set_context(
    rcog_engine_t* engine,
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype
) {
    if (!engine || !name || !data) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->context_store) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_set_context", 0.0f);


    return rcog_context_store_set(engine->context_store, name, data, size, dtype);
}


int rcog_engine_get_context(
    rcog_engine_t* engine,
    const char* name,
    rcog_query_result_t* result
) {
    if (!engine || !name || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->context_store) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_get_context", 0.0f);


    return rcog_context_store_query(engine->context_store, name,
                                     RCOG_ACCESS_FULL, NULL, result);
}


int rcog_engine_get_immune_modulation(
    const rcog_engine_t* engine,
    rcog_immune_modulation_t* modulation
) {
    if (!engine || !modulation) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_get_immune_modulatio", 0.0f);

    nimcp_mutex_lock(((rcog_engine_t*)engine)->mutex);
    *modulation = engine->current_modulation;
    nimcp_mutex_unlock(((rcog_engine_t*)engine)->mutex);

    return 0;
}


/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int rcog_engine_get_stats(
    const rcog_engine_t* engine,
    rcog_engine_stats_t* stats
) {
    if (!engine || !stats) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_get_stats", 0.0f);

    nimcp_mutex_lock(((rcog_engine_t*)engine)->mutex);

    *stats = engine->stats;
    stats->active_goals = engine->active_count;
    stats->state = engine->state;

    /* Calculate averages */
    if (stats->goals_completed > 0) {
        stats->avg_processing_time_ms =
            (float)stats->total_processing_time_ms / stats->goals_completed;
    }

    nimcp_mutex_unlock(((rcog_engine_t*)engine)->mutex);

    return 0;
}


int rcog_engine_get_progress(
    const rcog_engine_t* engine,
    const rcog_request_handle_t* handle,
    rcog_progress_t* progress
) {
    if (!engine || !progress) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_get_progress", 0.0f);


    memset(progress, 0, sizeof(rcog_progress_t));

    if (handle) {
        /* Get progress for specific request */
        nimcp_mutex_lock(((rcog_engine_t*)engine)->mutex);

        rcog_active_request_t* active_req = find_request_by_id(
            (rcog_engine_t*)engine, handle->request_id);

        if (active_req && active_req->answer) {
            progress->total_subtasks = active_req->decomp.num_subtasks;
            progress->current_confidence = active_req->answer->confidence;
            progress->refinement_step = active_req->answer->refinement_step;
            progress->max_depth_reached = active_req->decomp.metadata.depth;
            progress->elapsed_ms = engine_now_ms() - active_req->started_ms;

            /* Count completed subtasks from batch */
            if (active_req->batch) {
                size_t completed = 0;
                size_t total = 0;
                rcog_delegation_pool_poll_batch(
                    engine->delegation_pool,
                    active_req->batch,
                    &completed,
                    &total
                );
                progress->completed_subtasks = completed;
                progress->active_subtasks = total - completed;
            }
        }

        nimcp_mutex_unlock(((rcog_engine_t*)engine)->mutex);
    } else {
        /* Get overall progress */
        progress->completed_subtasks = engine->stats.subtasks_completed;
        progress->max_depth_reached = engine->stats.max_depth_reached;
        progress->current_confidence = engine->stats.avg_confidence;
    }

    return 0;
}


bool rcog_engine_is_ready(const rcog_engine_t* engine) {
    if (!engine) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_is_ready", 0.0f);


    return engine->state == RCOG_ENGINE_READY ||
           engine->state == RCOG_ENGINE_PROCESSING ||
           engine->state == RCOG_ENGINE_DEGRADED;
}


bool rcog_engine_has_capacity(const rcog_engine_t* engine) {
    if (!engine) {
        return false;
    }

    if (!rcog_engine_is_ready(engine)) {
        return false;
    }

    /* Apply modulation to max concurrent */
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_has_capacity", 0.0f);


    uint32_t effective_max = (uint32_t)(
        engine->config.max_concurrent_goals *
        engine->current_modulation.capacity_multiplier
    );

    if (effective_max == 0) {
        effective_max = 1;  /* Always allow at least one */
    }

    return engine->active_count < effective_max;
}
