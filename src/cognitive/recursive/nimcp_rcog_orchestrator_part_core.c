// nimcp_rcog_orchestrator_part_core.c - core functions
// Part of nimcp_rcog_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_orchestrator.c


/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_orchestrator_training_begin(void* ctx) {
    rcog_orchestrator_t* orch = (rcog_orchestrator_t*)ctx;
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_orchestrator_training_begin: NULL argument");
        return -1;
    }
    rcog_orchestrator_heartbeat_instance(
        g_rcog_orchestrator_instance_health_agent,
        "rcog_orch_training_begin", 0.0f);
    return 0;
}


int rcog_orchestrator_training_step(void* ctx, float progress) {
    rcog_orchestrator_t* orch = (rcog_orchestrator_t*)ctx;
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_orchestrator_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_orchestrator_heartbeat_instance(
        g_rcog_orchestrator_instance_health_agent,
        "rcog_orch_training_step", clamped);
    return 0;
}


int rcog_orchestrator_training_end(void* ctx) {
    rcog_orchestrator_t* orch = (rcog_orchestrator_t*)ctx;
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_orchestrator_training_end: NULL argument");
        return -1;
    }
    rcog_orchestrator_heartbeat_instance(
        g_rcog_orchestrator_instance_health_agent,
        "rcog_orch_training_end", 1.0f);
    return 0;
}


/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int rcog_orchestrator_connect_context_store(
    rcog_orchestrator_t* orch,
    struct rcog_context_store* store
) {
    if (!orch || !store) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_context_stor", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->context_store = store;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_connect_answer_refiner(
    rcog_orchestrator_t* orch,
    struct rcog_answer_refiner* refiner
) {
    if (!orch || !refiner) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_answer_refin", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->answer_refiner = refiner;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_connect_delegation_pool(
    rcog_orchestrator_t* orch,
    struct rcog_delegation_pool* pool
) {
    if (!orch || !pool) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_delegation_p", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->delegation_pool = pool;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_connect_imagination(
    rcog_orchestrator_t* orch,
    struct rcog_imagination_bridge* imagination
) {
    if (!orch || !imagination) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_imagination", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->imagination = imagination;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_connect_immune(
    rcog_orchestrator_t* orch,
    struct rcog_immune_bridge* immune
) {
    if (!orch || !immune) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_immune", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->immune = immune;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_connect_engine(
    rcog_orchestrator_t* orch,
    struct rcog_engine* engine
) {
    if (!orch || !engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_connect_engine", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    orch->engine = engine;
    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


/*=============================================================================
 * DECOMPOSITION - MAIN FUNCTIONS
 *===========================================================================*/

int rcog_orchestrator_decompose(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
) {
    if (!orch || !goal || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_decompose", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    /* Check depth limit */
    if (orch->current_depth >= orch->effective_max_depth) {
        orch->stats.depth_limit_hits++;
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_MAX_DEPTH_EXCEEDED;
    }

    /* Initialize result */
    memset(result, 0, sizeof(rcog_decomposition_t));
    result->metadata.decomp_id = generate_decomp_id(orch);
    result->metadata.depth = orch->current_depth;
    result->metadata.created_ms = nimcp_platform_time_monotonic_ms();

    rcog_decomposition_strategy_t strategy = orch->config.default_strategy;
    if (orch->config.enable_adaptive_strategy) {
        /* Use MCTS for strategy selection when enabled */
        if (orch->enable_mcts_strategy && !orch->current_modulation.enable_degraded_mode) {
            strategy = select_strategy_mcts(orch, goal);
        } else {
            strategy = RCOG_DECOMP_ADAPTIVE;
        }
    }
    result->metadata.strategy = strategy;

    add_trace_entry(orch, 0, "decompose_start", 0.0f);

    int err = 0;
    switch (strategy) {
        case RCOG_DECOMP_SEQUENTIAL:
            err = decompose_sequential(orch, goal, context, result);
            break;
        case RCOG_DECOMP_PARALLEL:
            err = decompose_parallel(orch, goal, context, result);
            break;
        case RCOG_DECOMP_HIERARCHICAL:
            err = decompose_hierarchical(orch, goal, context, result);
            break;
        case RCOG_DECOMP_ADAPTIVE:
        default:
            err = decompose_adaptive(orch, goal, context, result);
            break;
    }

    if (err == RCOG_OK) {
        orch->stats.goals_decomposed++;
        result->metadata.estimated_complexity =
            (float)result->num_subtasks / (float)RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP;
        add_trace_entry(orch, result->metadata.decomp_id, "decompose_complete",
                       result->metadata.estimated_complexity);
    }

    nimcp_mutex_unlock(orch->mutex);

    return err;
}


int rcog_orchestrator_decompose_with_strategy(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_strategy_t strategy,
    rcog_decomposition_t* result
) {
    if (!orch || !goal || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_decompose_with_strat", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    if (orch->current_depth >= orch->effective_max_depth) {
        orch->stats.depth_limit_hits++;
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_MAX_DEPTH_EXCEEDED;
    }

    memset(result, 0, sizeof(rcog_decomposition_t));
    result->metadata.decomp_id = generate_decomp_id(orch);
    result->metadata.depth = orch->current_depth;
    result->metadata.strategy = strategy;
    result->metadata.created_ms = nimcp_platform_time_monotonic_ms();

    int err = 0;
    switch (strategy) {
        case RCOG_DECOMP_SEQUENTIAL:
            err = decompose_sequential(orch, goal, context, result);
            break;
        case RCOG_DECOMP_PARALLEL:
            err = decompose_parallel(orch, goal, context, result);
            break;
        case RCOG_DECOMP_HIERARCHICAL:
            err = decompose_hierarchical(orch, goal, context, result);
            break;
        case RCOG_DECOMP_ADAPTIVE:
        default:
            err = decompose_adaptive(orch, goal, context, result);
            break;
    }

    if (err == RCOG_OK) {
        orch->stats.goals_decomposed++;
    }

    nimcp_mutex_unlock(orch->mutex);

    return err;
}


int rcog_orchestrator_decompose_with_hints(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    const rcog_strategy_hints_t* hints,
    rcog_decomposition_t* result
) {
    if (!orch || !goal || !hints || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Select strategy based on hints */
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_decompose_with_hints", 0.0f);


    rcog_decomposition_strategy_t strategy;
    if (hints->prefer_parallel) {
        strategy = RCOG_DECOMP_PARALLEL;
    } else if (hints->prefer_shallow) {
        strategy = RCOG_DECOMP_SEQUENTIAL;
    } else {
        strategy = RCOG_DECOMP_ADAPTIVE;
    }

    return rcog_orchestrator_decompose_with_strategy(orch, goal, context, strategy, result);
}


/*=============================================================================
 * DISPATCH
 *===========================================================================*/

int rcog_orchestrator_dispatch(
    rcog_orchestrator_t* orch,
    const rcog_decomposition_t* decomp,
    rcog_batch_handle_t** handle
) {
    if (!orch || !decomp || !handle) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_dispatch", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    if (!decomp->ready_for_dispatch) {
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* In full implementation, would submit to delegation pool */
    /* For now, mark as dispatched */
    orch->stats.subtasks_dispatched += decomp->num_subtasks;

    add_trace_entry(orch, decomp->metadata.decomp_id, "dispatch", 0.0f);

    /* Create placeholder handle — must have valid mutex/cond because
     * rcog_delegation_pool_await_batch() and free_batch_handle() expect them.
     * Mark all_done=true since this placeholder doesn't dispatch real work. */
    *handle = nimcp_calloc(1, sizeof(rcog_batch_handle_t));
    if (*handle) {
        mutex_attr_t mattr;
        memset(&mattr, 0, sizeof(mattr));
        mattr.type = MUTEX_TYPE_NORMAL;
        (*handle)->mutex = nimcp_mutex_create(&mattr);
        (*handle)->cond = nimcp_calloc(1, sizeof(nimcp_cond_t));
        if ((*handle)->cond) {
            nimcp_cond_init((*handle)->cond);
        }
        (*handle)->all_done = true;  /* No real subtasks dispatched */
    }

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_dispatch_subtask(
    rcog_orchestrator_t* orch,
    rcog_subtask_t* subtask
) {
    if (!orch || !subtask) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_dispatch_subtask", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    subtask->status = RCOG_SUBTASK_QUEUED;
    subtask->started_ms = nimcp_platform_time_monotonic_ms();
    orch->stats.subtasks_dispatched++;

    add_trace_entry(orch, subtask->task_id, "dispatch_subtask", subtask->priority);

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_await_batch(
    rcog_orchestrator_t* orch,
    rcog_batch_handle_t* handle,
    uint32_t timeout_ms
) {
    if (!orch || !handle) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* In full implementation, would wait for delegation pool */
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_await_batch", 0.0f);


    (void)timeout_ms;

    return RCOG_OK;
}


/*=============================================================================
 * AGGREGATION
 *===========================================================================*/

int rcog_orchestrator_aggregate(
    rcog_orchestrator_t* orch,
    rcog_batch_handle_t* handle,
    const rcog_subtask_result_t* results,
    size_t num_results,
    rcog_answer_state_t* answer
) {
    if (!orch || !handle || !results || !answer) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_aggregate", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    /* Aggregate confidence from subtask results */
    float total_confidence = 0.0f;
    size_t successful = 0;

    for (size_t i = 0; i < num_results; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_results > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)num_results);
        }

        if (results[i].success) {
            total_confidence += results[i].confidence;
            successful++;
            orch->stats.subtasks_completed++;
        } else {
            orch->stats.subtasks_failed++;
        }
    }

    if (successful > 0) {
        answer->confidence = total_confidence / (float)successful;
    }
    answer->refinement_step++;
    answer->last_updated_ms = nimcp_platform_time_monotonic_ms();

    /* Check if ready */
    if (answer->confidence >= orch->config.ready_threshold) {
        answer->ready = true;
        answer->status = RCOG_ANSWER_READY;
        if (orch->config.enable_early_termination) {
            orch->stats.early_terminations++;
        }
    } else {
        answer->status = RCOG_ANSWER_REFINING;
    }

    add_trace_entry(orch, 0, "aggregate", answer->confidence);

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_refine(
    rcog_orchestrator_t* orch,
    rcog_answer_state_t* answer,
    const struct rcog_context_store* context
) {
    if (!orch || !answer) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_refine", 0.0f);


    (void)context;  /* Would use for additional queries in full implementation */

    nimcp_mutex_lock(orch->mutex);

    if (answer->ready) {
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_OK;
    }

    if (answer->refinement_step >= orch->config.max_refinement_steps) {
        answer->status = RCOG_ANSWER_STALLED;
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_TIMEOUT;
    }

    /* Simulate refinement step */
    float old_confidence = answer->confidence;
    answer->confidence += 0.05f * (1.0f - answer->confidence);  /* Diminishing returns */
    answer->delta = answer->confidence - old_confidence;
    answer->refinement_step++;
    answer->last_updated_ms = nimcp_platform_time_monotonic_ms();

    if (answer->confidence >= orch->config.ready_threshold) {
        answer->ready = true;
        answer->status = RCOG_ANSWER_READY;
    } else if (answer->delta < RCOG_DEFAULT_CONVERGENCE_EPSILON) {
        answer->status = RCOG_ANSWER_CONVERGING;
    }

    add_trace_entry(orch, 0, "refine", answer->confidence);

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


bool rcog_orchestrator_would_exceed_depth(
    const rcog_orchestrator_t* orch,
    uint32_t additional_depth
) {
    if (!orch) return true;
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_would_exceed_depth", 0.0f);


    return (orch->current_depth + additional_depth) > orch->effective_max_depth;
}


int rcog_orchestrator_push_depth(rcog_orchestrator_t* orch) {
    if (!orch) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_push_depth", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    if (orch->current_depth >= orch->effective_max_depth) {
        orch->stats.depth_limit_hits++;
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_MAX_DEPTH_EXCEEDED;
    }

    orch->current_depth++;
    if (orch->current_depth > orch->stats.max_depth_reached) {
        orch->stats.max_depth_reached = orch->current_depth;
    }

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


void rcog_orchestrator_pop_depth(rcog_orchestrator_t* orch) {
    if (!orch) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_pop_depth", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    if (orch->current_depth > 0) {
        orch->current_depth--;
    }
    nimcp_mutex_unlock(orch->mutex);
}


/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

int rcog_orchestrator_apply_immune_modulation(
    rcog_orchestrator_t* orch,
    const rcog_immune_modulation_t* modulation
) {
    if (!orch || !modulation) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_apply_immune_modulat", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    orch->current_modulation = *modulation;
    orch->modulation_applied = true;
    apply_modulation_to_limits(orch);

    add_trace_entry(orch, 0, "immune_modulation", modulation->capacity_multiplier);

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


/*=============================================================================
 * STRATEGY SELECTION
 *===========================================================================*/

int rcog_orchestrator_select_strategy(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_strategy_t* strategy
) {
    if (!orch || !goal || !strategy) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_select_strategy", 0.0f);


    (void)context;

    nimcp_mutex_lock(orch->mutex);

    /* Use adaptive selection logic */
    switch (goal->type) {
        case RCOG_GOAL_REASONING:
        case RCOG_GOAL_PLANNING:
            *strategy = RCOG_DECOMP_HIERARCHICAL;
            break;
        case RCOG_GOAL_ANALYSIS:
        case RCOG_GOAL_SUMMARIZATION:
            *strategy = goal->num_context_refs > 1 ?
                        RCOG_DECOMP_PARALLEL : RCOG_DECOMP_SEQUENTIAL;
            break;
        default:
            *strategy = RCOG_DECOMP_SEQUENTIAL;
    }

    /* Respect modulation */
    if (orch->current_modulation.enable_degraded_mode) {
        *strategy = RCOG_DECOMP_SEQUENTIAL;
    }

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_recommend_strategies(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    rcog_decomposition_strategy_t* strategies,
    float* confidences,
    size_t max_strategies,
    size_t* num_strategies
) {
    if (!orch || !goal || !strategies || !confidences || !num_strategies) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_recommend_strategies", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    size_t n = 0;

    /* Always recommend adaptive first */
    if (n < max_strategies) {
        strategies[n] = RCOG_DECOMP_ADAPTIVE;
        confidences[n] = 0.9f;
        n++;
    }

    /* Recommend based on goal type */
    if (goal->type == RCOG_GOAL_REASONING || goal->type == RCOG_GOAL_PLANNING) {
        if (n < max_strategies) {
            strategies[n] = RCOG_DECOMP_HIERARCHICAL;
            confidences[n] = 0.8f;
            n++;
        }
    }

    if (goal->num_context_refs > 1 && n < max_strategies) {
        strategies[n] = RCOG_DECOMP_PARALLEL;
        confidences[n] = 0.7f;
        n++;
    }

    if (n < max_strategies) {
        strategies[n] = RCOG_DECOMP_SEQUENTIAL;
        confidences[n] = 0.6f;
        n++;
    }

    *num_strategies = n;

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


/*=============================================================================
 * SUB-ORCHESTRATORS
 *===========================================================================*/

int rcog_orchestrator_spawn_sub(
    rcog_orchestrator_t* orch,
    const rcog_subtask_t* parent_task,
    rcog_orchestrator_t** sub_orch
) {
    if (!orch || !parent_task || !sub_orch) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_spawn_sub", 0.0f);


    nimcp_mutex_lock(orch->mutex);

    /* Check depth limit */
    if (orch->current_depth >= orch->effective_max_depth) {
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_MAX_DEPTH_EXCEEDED;
    }

    /* Create sub-orchestrator with reduced limits */
    rcog_orchestrator_config_t sub_config = orch->config;
    sub_config.max_recursion_depth = orch->effective_max_depth - orch->current_depth - 1;
    sub_config.max_parallel_subtasks = orch->effective_max_parallel / 2;
    if (sub_config.max_parallel_subtasks < 1) {
        sub_config.max_parallel_subtasks = 1;
    }

    *sub_orch = rcog_orchestrator_create(&sub_config);
    if (!*sub_orch) {
        nimcp_mutex_unlock(orch->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Inherit connections */
    (*sub_orch)->context_store = orch->context_store;
    (*sub_orch)->answer_refiner = orch->answer_refiner;
    (*sub_orch)->delegation_pool = orch->delegation_pool;
    (*sub_orch)->imagination = orch->imagination;
    (*sub_orch)->immune = orch->immune;
    (*sub_orch)->engine = orch->engine;

    /* Set initial depth */
    (*sub_orch)->current_depth = parent_task->depth;

    nimcp_mutex_unlock(orch->mutex);

    return RCOG_OK;
}


void rcog_orchestrator_clear_trace(rcog_orchestrator_t* orch) {
    if (!orch) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_clear_trace", 0.0f);


    nimcp_mutex_lock(orch->mutex);
    if (orch->trace) {
        orch->trace->num_entries = 0;
    }
    nimcp_mutex_unlock(orch->mutex);
}


/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

int rcog_orchestrator_validate_decomposition(const rcog_decomposition_t* decomp) {
    if (!decomp) {
        return RCOG_ERROR_NULL_POINTER;
    }

    if (!decomp->subtasks || decomp->num_subtasks == 0) {
        return RCOG_ERROR_INVALID_CONFIG;
    }

    /* Check for cycles if dependencies exist */
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_validate_decompositi", 0.0f);


    if (decomp->deps && decomp->deps->num_edges > 0) {
        bool* visited = nimcp_calloc(decomp->num_subtasks, sizeof(bool));
        bool* in_stack = nimcp_calloc(decomp->num_subtasks, sizeof(bool));

        if (!visited || !in_stack) {
            if (visited) nimcp_free(visited);
            if (in_stack) nimcp_free(in_stack);
            return RCOG_ERROR_OUT_OF_MEMORY;
        }

        bool has_cycle = false;
        for (size_t i = 0; i < decomp->num_subtasks && !has_cycle; i++) {
            if (!visited[i]) {
                has_cycle = detect_cycle_dfs(decomp->deps, decomp->subtasks,
                                             decomp->num_subtasks,
                                             decomp->subtasks[i].task_id,
                                             visited, in_stack);
            }
        }

        nimcp_free(visited);
        visited = NULL;
        nimcp_free(in_stack);
        in_stack = NULL;

        if (has_cycle) {
            return RCOG_ERROR_INVALID_CONFIG;
        }
    }

    return RCOG_OK;
}


int rcog_orchestrator_estimate_complexity(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    float* complexity
) {
    if (!orch || !goal || !complexity) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_estimate_complexity", 0.0f);


    (void)context;

    /* Simple heuristic based on goal type and context */
    float base = 0.3f;  /* Base complexity */

    switch (goal->type) {
        case RCOG_GOAL_QUESTION_ANSWERING:
        case RCOG_GOAL_EXTRACTION:
            base = 0.2f;
            break;
        case RCOG_GOAL_SUMMARIZATION:
        case RCOG_GOAL_TRANSLATION:
            base = 0.4f;
            break;
        case RCOG_GOAL_REASONING:
        case RCOG_GOAL_ANALYSIS:
            base = 0.6f;
            break;
        case RCOG_GOAL_PLANNING:
        case RCOG_GOAL_GENERATION:
            base = 0.8f;
            break;
        default:
            base = 0.5f;
    }

    /* Adjust for context refs */
    if (goal->num_context_refs > 1) {
        base += 0.1f * (float)(goal->num_context_refs - 1);
    }

    if (base > 1.0f) base = 1.0f;

    *complexity = base;
    return RCOG_OK;
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_orchestrator_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Orchestrator_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Orchestrator_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Orchestrator_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
