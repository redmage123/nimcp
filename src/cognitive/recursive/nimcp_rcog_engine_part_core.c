// nimcp_rcog_engine_part_core.c - core functions
// Part of nimcp_rcog_engine.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_engine.c


/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_engine_training_begin(void* ctx) {
    rcog_engine_t* engine = (rcog_engine_t*)ctx;
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_engine_training_begin: NULL argument");
        return -1;
    }
    rcog_engine_heartbeat_instance(
        g_rcog_engine_instance_health_agent,
        "rcog_engine_training_begin", 0.0f);
    return 0;
}


int rcog_engine_training_end(void* ctx) {
    rcog_engine_t* engine = (rcog_engine_t*)ctx;
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_engine_training_end: NULL argument");
        return -1;
    }
    rcog_engine_heartbeat_instance(
        g_rcog_engine_instance_health_agent,
        "rcog_engine_training_end", 1.0f);
    return 0;
}


int rcog_engine_start(rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_start", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    if (engine->state != RCOG_ENGINE_READY && engine->state != RCOG_ENGINE_PAUSED) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Start delegation pool */
    int result = rcog_delegation_pool_start(engine->delegation_pool);
    if (result != 0) {
        nimcp_mutex_unlock(engine->mutex);
        return result;
    }

    engine->state = RCOG_ENGINE_READY;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


int rcog_engine_stop(rcog_engine_t* engine, uint32_t timeout_ms) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_stop", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    if (engine->state == RCOG_ENGINE_STOPPED ||
        engine->state == RCOG_ENGINE_UNINITIALIZED) {
        nimcp_mutex_unlock(engine->mutex);
        return 0;
    }

    engine->state = RCOG_ENGINE_SHUTTING_DOWN;

    /* Cancel all active requests */
    for (uint32_t i = 0; i < RCOG_ENGINE_MAX_CONCURRENT_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ENGINE_MAX_CONCURRENT_GOALS > 256) {
            rcog_engine_heartbeat("rcog_engine_loop",
                             (float)(i + 1) / (float)RCOG_ENGINE_MAX_CONCURRENT_GOALS);
        }

        if (engine->requests[i].active && engine->requests[i].handle) {
            engine->requests[i].handle->cancelled = true;
            engine->stats.goals_cancelled++;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    /* Stop delegation pool */
    if (engine->delegation_pool) {
        rcog_delegation_pool_stop(engine->delegation_pool, timeout_ms);
    }

    nimcp_mutex_lock(engine->mutex);
    engine->state = RCOG_ENGINE_STOPPED;

    /* Signal any waiting threads */
    nimcp_cond_broadcast(engine->request_cond);

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int rcog_engine_connect_immune(
    rcog_engine_t* engine,
    struct rcog_immune_bridge* bridge
) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_connect_immune", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->immune_bridge = bridge;

    /* Connect bridge to subsystems if enabled */
    if (bridge && engine->config.enable_immune_modulation) {
        if (engine->orchestrator) {
            rcog_orchestrator_connect_immune(engine->orchestrator, bridge);
        }
        if (engine->delegation_pool) {
            rcog_delegation_pool_connect_immune(engine->delegation_pool, bridge);
        }
        if (engine->tool_router) {
            rcog_tool_router_connect_immune(engine->tool_router, bridge);
        }
    }

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}


int rcog_engine_connect_imagination(
    rcog_engine_t* engine,
    struct rcog_imagination_bridge* bridge
) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_connect_imagination", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->imagination_bridge = bridge;

    /* Connect bridge to orchestrator for planning */
    if (bridge && engine->config.enable_imagination && engine->orchestrator) {
        rcog_orchestrator_connect_imagination(engine->orchestrator, bridge);
    }

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}


int rcog_engine_connect_collective(
    rcog_engine_t* engine,
    struct rcog_collective_bridge* bridge
) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_connect_collective", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->collective_bridge = bridge;

    /* Connect bridge to delegation pool for swarm distribution */
    if (bridge && engine->config.enable_collective && engine->delegation_pool) {
        rcog_delegation_pool_connect_collective(engine->delegation_pool, bridge);
    }

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}


int rcog_engine_connect_brain_kg(
    rcog_engine_t* engine,
    struct rcog_brain_kg_bridge* bridge
) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_connect_brain_kg", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->brain_kg_bridge = bridge;
    nimcp_mutex_unlock(engine->mutex);
    return 0;
}


int rcog_engine_await(
    rcog_engine_t* engine,
    rcog_request_handle_t* handle,
    uint32_t timeout_ms,
    rcog_process_result_t* result
) {
    if (!engine || !handle) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_await", 0.0f);


    uint64_t start_ms = engine_now_ms();
    uint64_t deadline = timeout_ms > 0 ? start_ms + timeout_ms : 0;

    nimcp_mutex_lock(engine->mutex);

    rcog_active_request_t* active_req = find_request_by_id(engine, handle->request_id);
    if (!active_req) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    /* Wait for completion */
    while (!handle->completed && !handle->cancelled) {
        if (deadline > 0) {
            uint64_t now = engine_now_ms();
            if (now >= deadline) {
                nimcp_mutex_unlock(engine->mutex);
                return RCOG_ERROR_TIMEOUT;
            }
            nimcp_cond_timedwait(engine->request_cond, engine->mutex,
                                 (uint32_t)(deadline - now));
        } else {
            nimcp_cond_wait(engine->request_cond, engine->mutex);
        }
    }

    /* Fill result */
    if (result && active_req->answer) {
        memset(result, 0, sizeof(rcog_process_result_t));
        result->request_id = handle->request_id;
        result->goal = active_req->request.goal;
        result->answer = *active_req->answer;
        result->success = (active_req->answer->status == RCOG_ANSWER_READY);
        result->subtasks_created = active_req->decomp.num_subtasks;
        result->max_depth_used = active_req->decomp.metadata.depth;
        result->refinement_steps = active_req->answer->refinement_step;
        result->processing_time_ms = engine_now_ms() - active_req->started_ms;

        if (handle->cancelled) {
            result->error = RCOG_ERROR_TIMEOUT;
            result->error_message = "Request cancelled";
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


int rcog_engine_cancel(
    rcog_engine_t* engine,
    rcog_request_handle_t* handle
) {
    if (!engine || !handle) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_cancel", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    rcog_active_request_t* active_req = find_request_by_id(engine, handle->request_id);
    if (!active_req) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_CONTEXT_NOT_FOUND;
    }

    handle->cancelled = true;
    engine->stats.goals_cancelled++;

    /* Cancel batch if exists */
    if (active_req->batch) {
        rcog_delegation_pool_cancel_batch(engine->delegation_pool, active_req->batch);
    }

    /* Signal waiters */
    nimcp_cond_broadcast(engine->request_cond);

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


int rcog_engine_query_context(
    rcog_engine_t* engine,
    const char* name,
    rcog_access_pattern_t pattern,
    const rcog_query_params_t* params,
    rcog_query_result_t* result
) {
    if (!engine || !name || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->context_store) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_query_context", 0.0f);


    return rcog_context_store_query(engine->context_store, name, pattern, params, result);
}


int rcog_engine_clear_context(
    rcog_engine_t* engine,
    const char* name
) {
    if (!engine || !name) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->context_store) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_clear_context", 0.0f);


    return rcog_context_store_remove(engine->context_store, name);
}


int rcog_engine_clear_all_context(rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->context_store) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_clear_all_context", 0.0f);


    return rcog_context_store_clear(engine->context_store);
}


/*=============================================================================
 * TOOL MANAGEMENT
 *===========================================================================*/

int rcog_engine_register_tool(
    rcog_engine_t* engine,
    const char* name,
    rcog_tool_fn handler,
    rcog_capability_tier_t tier,
    void* context
) {
    if (!engine || !name || !handler) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->tool_router) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_register_tool", 0.0f);


    rcog_tool_def_t def = rcog_tool_def_create(name, handler, tier);
    def.context = context;

    return rcog_tool_router_register(engine->tool_router, &def);
}


int rcog_engine_unregister_tool(
    rcog_engine_t* engine,
    const char* name
) {
    if (!engine || !name) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->tool_router) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_unregister_tool", 0.0f);


    return rcog_tool_router_unregister(engine->tool_router, name);
}


int rcog_engine_list_tools(
    rcog_engine_t* engine,
    rcog_capability_tier_t tier,
    char (*tools)[64],
    size_t max_tools,
    size_t* num_tools
) {
    if (!engine || !tools || !num_tools) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!engine->tool_router) {
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_list_tools", 0.0f);


    return rcog_tool_router_get_accessible_tools(
        engine->tool_router, tier, tools, max_tools, num_tools);
}


/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

int rcog_engine_apply_immune_modulation(
    rcog_engine_t* engine,
    const rcog_immune_modulation_t* modulation
) {
    if (!engine || !modulation) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_apply_immune_modulat", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    engine->current_modulation = *modulation;
    engine->stats.immune_modulations++;

    /* Apply to subsystems */
    apply_modulation_to_config(engine);

    /* Check for degraded mode */
    if (modulation->enable_degraded_mode && !engine->in_degraded_mode) {
        engine->in_degraded_mode = true;
        if (engine->state == RCOG_ENGINE_READY || engine->state == RCOG_ENGINE_PROCESSING) {
            engine->state = RCOG_ENGINE_DEGRADED;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


int rcog_engine_enter_degraded_mode(rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_enter_degraded_mode", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    engine->in_degraded_mode = true;
    engine->current_modulation.enable_degraded_mode = true;

    if (engine->state == RCOG_ENGINE_READY || engine->state == RCOG_ENGINE_PROCESSING) {
        engine->state = RCOG_ENGINE_DEGRADED;
    }

    /* Apply reduced limits */
    engine->current_modulation.capacity_multiplier = 0.5f;
    engine->current_modulation.max_depth_multiplier = 0.5f;
    engine->current_modulation.parallelism_multiplier = 0.5f;
    apply_modulation_to_config(engine);

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


int rcog_engine_exit_degraded_mode(rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_exit_degraded_mode", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    engine->in_degraded_mode = false;
    engine->current_modulation.enable_degraded_mode = false;

    if (engine->state == RCOG_ENGINE_DEGRADED) {
        engine->state = engine->active_count > 0 ? RCOG_ENGINE_PROCESSING : RCOG_ENGINE_READY;
    }

    /* Restore full limits */
    engine->current_modulation.capacity_multiplier = 1.0f;
    engine->current_modulation.max_depth_multiplier = 1.0f;
    engine->current_modulation.parallelism_multiplier = 1.0f;
    apply_modulation_to_config(engine);

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


/*=============================================================================
 * UTILITY
 *===========================================================================*/

rcog_process_request_t rcog_engine_default_request(const rcog_goal_t* goal) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_default_request", 0.0f);


    rcog_process_request_t request;
    memset(&request, 0, sizeof(request));

    if (goal) {
        request.goal = *goal;
    }

    request.mode = RCOG_MODE_SYNC;
    request.timeout_ms = 0;  /* Use engine default */
    request.skip_decomposition = false;
    request.force_local = false;
    request.strategy_override = 0;  /* Use engine default */

    return request;
}


const char* rcog_engine_state_name(rcog_engine_state_t state) {
    switch (state) {
        case RCOG_ENGINE_UNINITIALIZED: return "uninitialized";
        case RCOG_ENGINE_INITIALIZING:  return "initializing";
        case RCOG_ENGINE_READY:         return "ready";
        case RCOG_ENGINE_PROCESSING:    return "processing";
        case RCOG_ENGINE_PAUSED:        return "paused";
        case RCOG_ENGINE_DEGRADED:      return "degraded";
        case RCOG_ENGINE_SHUTTING_DOWN: return "shutting_down";
        case RCOG_ENGINE_STOPPED:       return "stopped";
        default:                        return "unknown";
    }
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_engine_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Engine_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_engine_heartbeat("rcog_engine_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Engine_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Engine_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
