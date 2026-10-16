// nimcp_rcog_orchestrator_part_lifecycle.c - lifecycle functions
// Part of nimcp_rcog_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_orchestrator.c


rcog_orchestrator_t* rcog_orchestrator_create(
    const rcog_orchestrator_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_create", 0.0f);


    rcog_orchestrator_t* orch = nimcp_calloc(1, sizeof(rcog_orchestrator_t));
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate orch");

        return NULL;
    }

    if (config) {
        orch->config = *config;
    } else {
        orch->config = rcog_orchestrator_default_config();
    }

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    orch->mutex = nimcp_mutex_create(&attr);
    if (!orch->mutex) {
        nimcp_free(orch);
        orch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_orchestrator_create: orch->mutex is NULL");
        return NULL;
    }

    /* Initialize effective limits */
    orch->effective_max_depth = orch->config.max_recursion_depth;
    orch->effective_max_parallel = orch->config.max_parallel_subtasks;

    /* Initialize modulation to neutral */
    orch->current_modulation.capacity_multiplier = 1.0f;
    orch->current_modulation.max_depth_multiplier = 1.0f;
    orch->current_modulation.parallelism_multiplier = 1.0f;
    orch->current_modulation.timeout_multiplier = 1.0f;
    orch->current_modulation.enable_degraded_mode = false;

    /* Initialize trace if enabled */
    if (orch->config.enable_trace) {
        orch->trace = nimcp_calloc(1, sizeof(rcog_trace_t));
        if (!orch->trace) {
            nimcp_mutex_destroy(orch->mutex);
            nimcp_free(orch);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_orchestrator_create: trace allocation failed");
            return NULL;
        }
        orch->trace_enabled = true;
    }

    /* Initialize ID generators */
    orch->next_decomp_id = 1;
    orch->next_task_id = 1;

    /* Initialize MCTS for strategy selection */
    orch->rand_seed = mc_seed_from_time();
    orch->enable_mcts_strategy = true;
    orch->mcts_iterations = 30;

    return orch;
}


rcog_orchestrator_t* rcog_orchestrator_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_create_default", 0.0f);


    return rcog_orchestrator_create(NULL);
}


void rcog_orchestrator_destroy(rcog_orchestrator_t* orch) {
    if (!orch) {
        return;
    }

    /* Free active decompositions */
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_destroy", 0.0f);


    for (size_t i = 0; i < RCOG_ORCH_MAX_ACTIVE_DECOMPOSITIONS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ORCH_MAX_ACTIVE_DECOMPOSITIONS > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)RCOG_ORCH_MAX_ACTIVE_DECOMPOSITIONS);
        }

        if (orch->active_decomps[i].in_use && orch->active_decomps[i].decomp) {
            rcog_orchestrator_free_decomposition(orch->active_decomps[i].decomp);
        }
    }

    /* Free trace */
    if (orch->trace) {
        rcog_orchestrator_free_trace(orch->trace);
    }

    if (orch->mutex) {
        nimcp_mutex_destroy(orch->mutex);
    }

    nimcp_free(orch);
    orch = NULL;
}


static void rcog_mcts_free_state(void* state, void* user_data) {
    (void)user_data;
    nimcp_free(state);
    state = NULL;
}


void rcog_orchestrator_free_decomposition(rcog_decomposition_t* decomp) {
    if (!decomp) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_free_decomposition", 0.0f);


    if (decomp->subtasks) {
        nimcp_free(decomp->subtasks);
        decomp->subtasks = NULL;
    }

    if (decomp->deps) {
        nimcp_free(decomp->deps);
        decomp->deps = NULL;
    }

    decomp->num_subtasks = 0;
}


void rcog_orchestrator_destroy_sub(
    rcog_orchestrator_t* orch,
    rcog_orchestrator_t* sub_orch
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_destroy_sub", 0.0f);


    (void)orch;  /* Parent reference not needed for cleanup */
    rcog_orchestrator_destroy(sub_orch);
}


void rcog_orchestrator_reset_stats(rcog_orchestrator_t* orch) {
    if (!orch) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_reset_stats", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    memset(&orch->stats, 0, sizeof(rcog_orchestrator_stats_t));
    nimcp_mutex_unlock(orch->mutex);
}


void rcog_orchestrator_free_trace(rcog_trace_t* trace) {
    if (!trace) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_free_trace", 0.0f);


    if (trace->entries) {
        nimcp_free(trace->entries);
    }
    nimcp_free(trace);
    trace = NULL;
}
