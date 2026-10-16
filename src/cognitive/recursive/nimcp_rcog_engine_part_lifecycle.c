// nimcp_rcog_engine_part_lifecycle.c - lifecycle functions
// Part of nimcp_rcog_engine.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_engine.c


rcog_engine_t* rcog_engine_create(const rcog_engine_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_create", 0.0f);


    rcog_engine_t* engine = nimcp_calloc(1, sizeof(rcog_engine_t));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate engine");

        return NULL;
    }

    /* Copy configuration */
    if (config) {
        engine->config = *config;
    } else {
        engine->config = rcog_engine_default_config();
    }

    /* Initialize mutex */
    engine->mutex = nimcp_mutex_create(NULL);
    if (!engine->mutex) {
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_engine_create: engine->mutex is NULL");
        return NULL;
    }

    /* Initialize condition variable for request completion */
    engine->request_cond = nimcp_calloc(1, sizeof(nimcp_cond_t));
    if (!engine->request_cond) {
        nimcp_mutex_destroy(engine->mutex);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_engine_create: engine->request_cond is NULL");
        return NULL;
    }
    nimcp_cond_init(engine->request_cond);

    /* Set initial state */
    engine->state = RCOG_ENGINE_UNINITIALIZED;
    engine->next_request_id = 1;
    engine->stats_start_ms = engine_now_ms();

    /* Initialize default modulation (no effect) */
    engine->current_modulation.capacity_multiplier = 1.0f;
    engine->current_modulation.max_depth_multiplier = 1.0f;
    engine->current_modulation.parallelism_multiplier = 1.0f;
    engine->current_modulation.timeout_multiplier = 1.0f;
    engine->current_modulation.enable_degraded_mode = false;

    return engine;
}


rcog_engine_t* rcog_engine_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_create_default", 0.0f);


    return rcog_engine_create(NULL);
}


int rcog_engine_init(rcog_engine_t* engine) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_init", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    if (engine->state != RCOG_ENGINE_UNINITIALIZED) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_ALREADY_INITIALIZED;
    }

    engine->state = RCOG_ENGINE_INITIALIZING;

    /* Create context store */
    engine->context_store = rcog_context_store_create_default();
    if (!engine->context_store) {
        engine->state = RCOG_ENGINE_UNINITIALIZED;
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Create orchestrator */
    rcog_orchestrator_config_t orch_config = rcog_orchestrator_default_config();
    orch_config.max_recursion_depth = engine->config.max_recursion_depth;
    orch_config.max_parallel_subtasks = engine->config.max_parallel_subtasks;
    orch_config.ready_threshold = engine->config.confidence_threshold;
    orch_config.max_refinement_steps = engine->config.max_refinement_steps;
    orch_config.enable_early_termination = engine->config.enable_early_termination;
    orch_config.default_strategy = engine->config.default_strategy;
    orch_config.enable_adaptive_strategy = engine->config.enable_adaptive_strategy;
    orch_config.enable_trace = engine->config.enable_tracing;
    orch_config.verbose_logging = engine->config.verbose_logging;

    engine->orchestrator = rcog_orchestrator_create(&orch_config);
    if (!engine->orchestrator) {
        rcog_context_store_destroy(engine->context_store);
        engine->context_store = NULL;
        engine->state = RCOG_ENGINE_UNINITIALIZED;
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Create delegation pool */
    engine->delegation_pool = rcog_delegation_pool_create_default();
    if (!engine->delegation_pool) {
        rcog_orchestrator_destroy(engine->orchestrator);
        engine->orchestrator = NULL;
        rcog_context_store_destroy(engine->context_store);
        engine->context_store = NULL;
        engine->state = RCOG_ENGINE_UNINITIALIZED;
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Create tool router */
    engine->tool_router = rcog_tool_router_create_default();
    if (!engine->tool_router) {
        rcog_delegation_pool_destroy(engine->delegation_pool);
        engine->delegation_pool = NULL;
        rcog_orchestrator_destroy(engine->orchestrator);
        engine->orchestrator = NULL;
        rcog_context_store_destroy(engine->context_store);
        engine->context_store = NULL;
        engine->state = RCOG_ENGINE_UNINITIALIZED;
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Create answer refiner */
    rcog_answer_config_t answer_config = rcog_answer_default_config();
    answer_config.ready_threshold = engine->config.confidence_threshold;
    answer_config.max_steps = engine->config.max_refinement_steps;
    answer_config.enable_early_stopping = engine->config.enable_early_termination;

    engine->answer_refiner = rcog_answer_refiner_create(&answer_config);
    if (!engine->answer_refiner) {
        rcog_tool_router_destroy(engine->tool_router);
        engine->tool_router = NULL;
        rcog_delegation_pool_destroy(engine->delegation_pool);
        engine->delegation_pool = NULL;
        rcog_orchestrator_destroy(engine->orchestrator);
        engine->orchestrator = NULL;
        rcog_context_store_destroy(engine->context_store);
        engine->context_store = NULL;
        engine->state = RCOG_ENGINE_UNINITIALIZED;
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    engine->subsystems_created = true;

    /* Connect subsystems to each other */
    rcog_orchestrator_connect_context_store(engine->orchestrator, engine->context_store);
    rcog_orchestrator_connect_answer_refiner(engine->orchestrator, engine->answer_refiner);
    rcog_orchestrator_connect_delegation_pool(engine->orchestrator, engine->delegation_pool);
    rcog_orchestrator_connect_engine(engine->orchestrator, engine);

    rcog_delegation_pool_connect_tool_router(engine->delegation_pool, engine->tool_router);
    rcog_delegation_pool_connect_context_store(engine->delegation_pool, engine->context_store);

    rcog_tool_router_connect_context_store(engine->tool_router, engine->context_store);

    engine->subsystems_connected = true;

    /* Create SNN and plasticity bridges with default configs */
    rcog_snn_config_t snn_config = rcog_snn_config_default();
    engine->snn_bridge = rcog_snn_create(&snn_config);

    rcog_plasticity_config_t plasticity_config = rcog_plasticity_config_default();
    engine->plasticity_bridge = rcog_plasticity_create(&plasticity_config);

    /* Bridges are optional - don't fail init if they can't be created */
    engine->bridges_enabled = (engine->snn_bridge != NULL && engine->plasticity_bridge != NULL);

    /* Register builtin tools if configured */
    if (engine->config.register_l1_builtins) {
        rcog_tool_router_register_l1_builtins(engine->tool_router);
    }
    if (engine->config.register_l2_builtins) {
        rcog_tool_router_register_l2_builtins(engine->tool_router);
    }
    if (engine->config.register_l3_builtins) {
        rcog_tool_router_register_l3_builtins(engine->tool_router);
    }

    engine->state = RCOG_ENGINE_READY;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}


void rcog_engine_destroy(rcog_engine_t* engine) {
    if (!engine) {
        return;
    }

    /* Stop if not already stopped */
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_destroy", 0.0f);


    if (engine->state != RCOG_ENGINE_STOPPED &&
        engine->state != RCOG_ENGINE_UNINITIALIZED) {
        rcog_engine_stop(engine, 5000);
    }

    /* Destroy subsystems in reverse order */
    if (engine->answer_refiner) {
        rcog_answer_refiner_destroy(engine->answer_refiner);
        engine->answer_refiner = NULL;
    }

    if (engine->tool_router) {
        rcog_tool_router_destroy(engine->tool_router);
        engine->tool_router = NULL;
    }

    if (engine->delegation_pool) {
        rcog_delegation_pool_destroy(engine->delegation_pool);
        engine->delegation_pool = NULL;
    }

    if (engine->orchestrator) {
        rcog_orchestrator_destroy(engine->orchestrator);
        engine->orchestrator = NULL;
    }

    if (engine->context_store) {
        rcog_context_store_destroy(engine->context_store);
        engine->context_store = NULL;
    }

    /* Destroy SNN and plasticity bridges */
    if (engine->snn_bridge) {
        rcog_snn_destroy(engine->snn_bridge);
        engine->snn_bridge = NULL;
    }

    if (engine->plasticity_bridge) {
        rcog_plasticity_destroy(engine->plasticity_bridge);
        engine->plasticity_bridge = NULL;
    }

    engine->bridges_enabled = false;

    /* Clean up active request handles */
    for (uint32_t i = 0; i < RCOG_ENGINE_MAX_CONCURRENT_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ENGINE_MAX_CONCURRENT_GOALS > 256) {
            rcog_engine_heartbeat("rcog_engine_loop",
                             (float)(i + 1) / (float)RCOG_ENGINE_MAX_CONCURRENT_GOALS);
        }

        if (engine->requests[i].handle) {
            nimcp_free(engine->requests[i].handle);
            engine->requests[i].handle = NULL;
        }
        if (engine->requests[i].answer) {
            rcog_answer_state_destroy(engine->requests[i].answer);
            engine->requests[i].answer = NULL;
        }
    }

    /* Destroy synchronization primitives */
    if (engine->request_cond) {
        nimcp_cond_destroy(engine->request_cond);
        nimcp_free(engine->request_cond);
        engine->request_cond = NULL;
    }

    if (engine->mutex) {
        nimcp_mutex_destroy(engine->mutex);
        engine->mutex = NULL;
    }

    nimcp_free(engine);
    engine = NULL;
}


void rcog_engine_free_handle(rcog_request_handle_t* handle) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_free_handle", 0.0f);


    if (handle) {
        nimcp_free(handle);
        handle = NULL;
    }
}


void rcog_engine_free_result(rcog_process_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_free_result", 0.0f);


    if (result) {
        if (result->answer.content) {
            nimcp_free(result->answer.content);
            result->answer.content = NULL;
        }
        if (result->answer.latent) {
            nimcp_free(result->answer.latent);
            result->answer.latent = NULL;
        }
    }
}


void rcog_engine_reset_stats(rcog_engine_t* engine) {
    if (!engine) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_reset_stats", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    memset(&engine->stats, 0, sizeof(engine->stats));
    engine->stats_start_ms = engine_now_ms();

    nimcp_mutex_unlock(engine->mutex);
}


rcog_goal_t rcog_engine_create_goal(
    const char* query,
    rcog_goal_type_t type
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_create_goal", 0.0f);


    rcog_goal_t goal;
    memset(&goal, 0, sizeof(goal));

    goal.type = type;
    goal.query = query;
    goal.priority = 0.5f;
    goal.timeout_ms = 0;  /* Use default */
    goal.max_depth = 0;   /* Use default */

    return goal;
}
