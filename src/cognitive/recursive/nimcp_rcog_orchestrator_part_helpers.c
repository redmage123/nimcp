// nimcp_rcog_orchestrator_part_helpers.c - helpers functions
// Part of nimcp_rcog_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_orchestrator.c

static void add_trace_entry(
    rcog_orchestrator_t* orch,
    uint64_t task_id,
    const char* event,
    float confidence
) {
    if (!orch->trace_enabled || !orch->trace) {
        return;
    }

    /* Grow trace if needed */
    if (orch->trace->num_entries >= orch->trace->capacity) {
        size_t new_cap = orch->trace->capacity * 2;
        if (new_cap == 0) new_cap = 64;
        rcog_trace_entry_t* new_entries = nimcp_realloc(
            orch->trace->entries,
            new_cap * sizeof(rcog_trace_entry_t)
        );
        if (!new_entries) return;
        orch->trace->entries = new_entries;
        orch->trace->capacity = new_cap;
    }

    rcog_trace_entry_t* entry = &orch->trace->entries[orch->trace->num_entries++];
    entry->timestamp_ms = nimcp_platform_time_monotonic_ms();
    entry->task_id = task_id;
    entry->event = event;
    entry->depth = orch->current_depth;
    entry->confidence = confidence;
}


/**
 * @brief Generate unique task ID
 */
static uint64_t generate_task_id(rcog_orchestrator_t* orch) {
    return orch->next_task_id++;
}


/**
 * @brief Generate unique decomposition ID
 */
static uint64_t generate_decomp_id(rcog_orchestrator_t* orch) {
    return orch->next_decomp_id++;
}


/**
 * @brief Check if task has dependencies satisfied
 */
static bool task_deps_satisfied(
    const rcog_decomposition_t* decomp,
    uint64_t task_id,
    const uint64_t* completed,
    size_t num_completed
) {
    if (!decomp->deps) {
        return true;  /* No dependencies */
    }

    for (size_t i = 0; i < decomp->deps->num_edges; i++) {
        if (decomp->deps->edges[i].to_task_id == task_id) {
            uint64_t dep_id = decomp->deps->edges[i].from_task_id;
            bool found = false;
            for (size_t j = 0; j < num_completed; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_completed > 256) {
                    rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                     (float)(j + 1) / (float)num_completed);
                }

                if (completed[j] == dep_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;  /* Dependency not yet completed - normal */
            }
        }
    }
    return true;
}


/**
 * @brief Detect cycles in dependency graph using DFS
 */
static bool detect_cycle_dfs(
    const rcog_dependency_graph_t* deps,
    const rcog_subtask_t* subtasks,
    size_t num_subtasks,
    uint64_t task_id,
    bool* visited,
    bool* in_stack
) {
    /* Find task index */
    size_t idx = SIZE_MAX;
    for (size_t i = 0; i < num_subtasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_subtasks > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)num_subtasks);
        }

        if (subtasks[i].task_id == task_id) {
            idx = i;
            break;
        }
    }
    if (idx == SIZE_MAX) {
        return false;  /* Task not in subtask array - no cycle through external deps */
    }

    if (in_stack[idx]) return true;  /* Cycle detected */
    if (visited[idx]) {
        return false;  /* Already visited, no cycle through this node */
    }

    visited[idx] = true;
    in_stack[idx] = true;

    /* Visit all tasks that depend on this one */
    for (size_t i = 0; i < deps->num_edges; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && deps->num_edges > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)deps->num_edges);
        }

        if (deps->edges[i].from_task_id == task_id) {
            if (detect_cycle_dfs(deps, subtasks, num_subtasks,
                                 deps->edges[i].to_task_id, visited, in_stack)) {
                return true;
            }
        }
    }

    in_stack[idx] = false;
    return false;  /* No cycle found from this node */
}


/**
 * @brief Apply modulation to limits
 */
static void apply_modulation_to_limits(rcog_orchestrator_t* orch) {
    if (!orch->modulation_applied) {
        orch->effective_max_depth = orch->config.max_recursion_depth;
        orch->effective_max_parallel = orch->config.max_parallel_subtasks;
        return;
    }

    orch->effective_max_depth = (uint32_t)(
        (float)orch->config.max_recursion_depth *
        orch->current_modulation.max_depth_multiplier
    );
    if (orch->effective_max_depth < 1) {
        orch->effective_max_depth = 1;
    }

    orch->effective_max_parallel = (uint32_t)(
        (float)orch->config.max_parallel_subtasks *
        orch->current_modulation.parallelism_multiplier
    );
    if (orch->effective_max_parallel < 1) {
        orch->effective_max_parallel = 1;
    }
}


/*=============================================================================
 * DECOMPOSITION - SEQUENTIAL STRATEGY
 *===========================================================================*/

/**
 * @brief Create sequential decomposition
 *
 * Splits goal into ordered steps that execute one after another.
 */
static int decompose_sequential(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
) {
    (void)context;  /* May use in full implementation */

    /* Estimate number of subtasks based on goal type */
    size_t num_subtasks = 0;
    switch (goal->type) {
        case RCOG_GOAL_QUESTION_ANSWERING:
            num_subtasks = 2;  /* Parse, Answer */
            break;
        case RCOG_GOAL_REASONING:
            num_subtasks = 3;  /* Analyze, Reason, Conclude */
            break;
        case RCOG_GOAL_PLANNING:
            num_subtasks = 4;  /* Analyze, Plan, Validate, Refine */
            break;
        default:
            num_subtasks = 2;
    }

    if (num_subtasks > RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP) {
        num_subtasks = RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP;
    }

    /* Allocate subtasks */
    result->subtasks = nimcp_calloc(num_subtasks, sizeof(rcog_subtask_t));
    if (!result->subtasks) {
        return RCOG_ERROR_OUT_OF_MEMORY;
    }
    result->num_subtasks = num_subtasks;

    /* Create subtasks */
    for (size_t i = 0; i < num_subtasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_subtasks > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)num_subtasks);
        }

        rcog_subtask_t* subtask = &result->subtasks[i];
        subtask->task_id = generate_task_id(orch);
        subtask->goal = *goal;  /* Copy goal */
        subtask->tier = RCOG_TIER_L1_REASONING;
        subtask->status = RCOG_SUBTASK_PENDING;
        subtask->priority = goal->priority * (1.0f - 0.1f * (float)i);
        subtask->timeout_ms = goal->timeout_ms > 0 ? goal->timeout_ms / (uint32_t)num_subtasks :
                              orch->config.decomposition_timeout_ms;
        subtask->depth = orch->current_depth + 1;
    }

    /* Create sequential dependencies */
    if (num_subtasks > 1) {
        result->deps = nimcp_calloc(1, sizeof(rcog_dependency_graph_t));
        if (!result->deps) {
            nimcp_free(result->subtasks);
            result->subtasks = NULL;
            return RCOG_ERROR_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < num_subtasks - 1; i++) {
            result->deps->edges[i].from_task_id = result->subtasks[i].task_id;
            result->deps->edges[i].to_task_id = result->subtasks[i + 1].task_id;
            result->deps->edges[i].weight = 1.0f;
        }
        result->deps->num_edges = num_subtasks - 1;
        result->deps->has_cycle = false;
    }

    result->ready_for_dispatch = true;
    orch->stats.subtasks_created += num_subtasks;

    return RCOG_OK;
}


/*=============================================================================
 * DECOMPOSITION - PARALLEL STRATEGY
 *===========================================================================*/

/**
 * @brief Create parallel decomposition
 *
 * Splits goal into independent subtasks that can execute simultaneously.
 */
static int decompose_parallel(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
) {
    (void)context;

    /* Number of parallel subtasks based on context refs */
    size_t num_subtasks = goal->num_context_refs > 0 ?
                          goal->num_context_refs :
                          orch->effective_max_parallel;

    if (num_subtasks > orch->effective_max_parallel) {
        num_subtasks = orch->effective_max_parallel;
    }
    if (num_subtasks > RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP) {
        num_subtasks = RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP;
    }
    if (num_subtasks == 0) {
        num_subtasks = 1;
    }

    /* Allocate subtasks */
    result->subtasks = nimcp_calloc(num_subtasks, sizeof(rcog_subtask_t));
    if (!result->subtasks) {
        return RCOG_ERROR_OUT_OF_MEMORY;
    }
    result->num_subtasks = num_subtasks;

    /* Create independent subtasks */
    for (size_t i = 0; i < num_subtasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_subtasks > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)num_subtasks);
        }

        rcog_subtask_t* subtask = &result->subtasks[i];
        subtask->task_id = generate_task_id(orch);
        subtask->goal = *goal;
        subtask->tier = RCOG_TIER_L2_PERCEPTION;
        subtask->status = RCOG_SUBTASK_PENDING;
        subtask->priority = goal->priority;
        subtask->timeout_ms = goal->timeout_ms > 0 ? goal->timeout_ms :
                              orch->config.decomposition_timeout_ms;
        subtask->depth = orch->current_depth + 1;
    }

    /* No dependencies for parallel execution */
    result->deps = NULL;
    result->ready_for_dispatch = true;
    orch->stats.subtasks_created += num_subtasks;

    return RCOG_OK;
}


/*=============================================================================
 * DECOMPOSITION - HIERARCHICAL STRATEGY
 *===========================================================================*/

/**
 * @brief Create hierarchical decomposition
 *
 * Creates a tree structure with coordinator and worker subtasks.
 */
static int decompose_hierarchical(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
) {
    (void)context;

    /* One coordinator + multiple workers */
    size_t num_workers = orch->effective_max_parallel / 2;
    if (num_workers < 1) num_workers = 1;
    if (num_workers > 4) num_workers = 4;

    size_t num_subtasks = 1 + num_workers;  /* Coordinator + workers */

    /* Allocate subtasks */
    result->subtasks = nimcp_calloc(num_subtasks, sizeof(rcog_subtask_t));
    if (!result->subtasks) {
        return RCOG_ERROR_OUT_OF_MEMORY;
    }
    result->num_subtasks = num_subtasks;

    /* Create coordinator (first subtask) */
    rcog_subtask_t* coordinator = &result->subtasks[0];
    coordinator->task_id = generate_task_id(orch);
    coordinator->goal = *goal;
    coordinator->tier = RCOG_TIER_ROOT;  /* Coordinator only */
    coordinator->status = RCOG_SUBTASK_PENDING;
    coordinator->priority = goal->priority;
    coordinator->timeout_ms = orch->config.decomposition_timeout_ms;
    coordinator->depth = orch->current_depth + 1;

    /* Create workers */
    for (size_t i = 0; i < num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_workers > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)num_workers);
        }

        rcog_subtask_t* worker = &result->subtasks[1 + i];
        worker->task_id = generate_task_id(orch);
        worker->goal = *goal;
        worker->tier = RCOG_TIER_L1_REASONING;
        worker->status = RCOG_SUBTASK_PENDING;
        worker->priority = goal->priority * 0.9f;
        worker->timeout_ms = orch->config.decomposition_timeout_ms;
        worker->depth = orch->current_depth + 2;  /* Deeper than coordinator */
    }

    /* Dependencies: coordinator must complete setup before workers */
    /* Then workers run in parallel, then coordinator aggregates */
    result->deps = nimcp_calloc(1, sizeof(rcog_dependency_graph_t));
    if (!result->deps) {
        nimcp_free(result->subtasks);
        result->subtasks = NULL;
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Coordinator spawns workers (implicit dependency through dispatch) */
    result->deps->num_edges = 0;  /* Workers are dispatched by coordinator */
    result->deps->has_cycle = false;

    result->ready_for_dispatch = true;
    orch->stats.subtasks_created += num_subtasks;

    return RCOG_OK;
}


/*=============================================================================
 * DECOMPOSITION - ADAPTIVE STRATEGY
 *===========================================================================*/

/**
 * @brief Create adaptive decomposition
 *
 * Selects strategy based on goal characteristics.
 */
static int decompose_adaptive(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
) {
    rcog_decomposition_strategy_t selected = RCOG_DECOMP_SEQUENTIAL;

    /* Heuristic selection */
    switch (goal->type) {
        case RCOG_GOAL_QUESTION_ANSWERING:
        case RCOG_GOAL_EXTRACTION:
            /* Simple goals: sequential */
            selected = RCOG_DECOMP_SEQUENTIAL;
            break;

        case RCOG_GOAL_ANALYSIS:
        case RCOG_GOAL_SUMMARIZATION:
            /* Multi-source goals: parallel if multiple context refs */
            if (goal->num_context_refs > 1) {
                selected = RCOG_DECOMP_PARALLEL;
            } else {
                selected = RCOG_DECOMP_SEQUENTIAL;
            }
            break;

        case RCOG_GOAL_REASONING:
        case RCOG_GOAL_PLANNING:
            /* Complex goals: hierarchical */
            if (orch->current_depth < orch->effective_max_depth - 2) {
                selected = RCOG_DECOMP_HIERARCHICAL;
            } else {
                selected = RCOG_DECOMP_SEQUENTIAL;
            }
            break;

        case RCOG_GOAL_GENERATION:
        case RCOG_GOAL_TRANSLATION:
            /* Creative goals: parallel for variety */
            selected = RCOG_DECOMP_PARALLEL;
            break;

        default:
            selected = RCOG_DECOMP_SEQUENTIAL;
    }

    /* Respect depth limits */
    if (orch->current_depth >= orch->effective_max_depth - 1) {
        selected = RCOG_DECOMP_SEQUENTIAL;
    }

    /* Apply degraded mode if immune modulation active */
    if (orch->current_modulation.enable_degraded_mode) {
        selected = RCOG_DECOMP_SEQUENTIAL;
    }

    orch->stats.strategy_adaptations++;

    /* Delegate to selected strategy */
    switch (selected) {
        case RCOG_DECOMP_PARALLEL:
            return decompose_parallel(orch, goal, context, result);
        case RCOG_DECOMP_HIERARCHICAL:
            return decompose_hierarchical(orch, goal, context, result);
        default:
            return decompose_sequential(orch, goal, context, result);
    }
}


static void* rcog_mcts_apply_action(const void* state, uint32_t action, void* user_data) {
    const rcog_mcts_state_t* s = (const rcog_mcts_state_t*)state;
    (void)user_data;

    rcog_mcts_state_t* new_state = nimcp_malloc(sizeof(rcog_mcts_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_state");

        return NULL;

    }

    *new_state = *s;
    new_state->selected_strategy = (rcog_decomposition_strategy_t)action;

    /* Simulate strategy effects */
    switch (action) {
        case RCOG_DECOMP_SEQUENTIAL:
            /* Sequential: slower but more reliable */
            new_state->estimated_confidence = s->estimated_confidence + 0.15f;
            new_state->complexity = s->complexity * 0.9f;
            break;
        case RCOG_DECOMP_PARALLEL:
            /* Parallel: faster for multi-context, may reduce reliability */
            if (s->num_context_refs > 1) {
                new_state->estimated_confidence = s->estimated_confidence + 0.2f;
                new_state->complexity = s->complexity * 0.7f;
            } else {
                new_state->estimated_confidence = s->estimated_confidence + 0.1f;
                new_state->complexity = s->complexity * 0.8f;
            }
            break;
        case RCOG_DECOMP_HIERARCHICAL:
            /* Hierarchical: good for complex goals */
            if (s->goal_type == RCOG_GOAL_REASONING || s->goal_type == RCOG_GOAL_PLANNING) {
                new_state->estimated_confidence = s->estimated_confidence + 0.25f;
            } else {
                new_state->estimated_confidence = s->estimated_confidence + 0.1f;
            }
            new_state->complexity = s->complexity * 0.6f;
            break;
        case RCOG_DECOMP_ADAPTIVE:
        default:
            /* Adaptive: balanced */
            new_state->estimated_confidence = s->estimated_confidence + 0.18f;
            new_state->complexity = s->complexity * 0.75f;
            break;
    }

    /* Clamp confidence */
    if (new_state->estimated_confidence > 1.0f) {
        new_state->estimated_confidence = 1.0f;
    }

    return new_state;
}


static float rcog_mcts_evaluate(const void* state, void* user_data) {
    const rcog_mcts_state_t* s = (const rcog_mcts_state_t*)state;
    (void)user_data;

    /* Value based on confidence and efficiency (inverse complexity) */
    float efficiency = 1.0f - s->complexity;
    return s->estimated_confidence * 0.7f + efficiency * 0.3f;
}


static void* rcog_mcts_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return NULL;

    }

    rcog_mcts_state_t* clone = nimcp_malloc(sizeof(rcog_mcts_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;

    }

    *clone = *(const rcog_mcts_state_t*)state;
    return clone;
}


/**
 * @brief Select decomposition strategy using MCTS
 */
static rcog_decomposition_strategy_t select_strategy_mcts(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal
) {
    /* Create initial state */
    rcog_mcts_state_t initial_state = {
        .goal_type = goal->type,
        .num_context_refs = (uint32_t)goal->num_context_refs,
        .current_depth = orch->current_depth,
        .max_depth = orch->effective_max_depth,
        .selected_strategy = RCOG_DECOMP_SEQUENTIAL,
        .estimated_confidence = 0.3f,
        .complexity = 0.7f
    };

    /* Adjust initial complexity based on goal type */
    switch (goal->type) {
        case RCOG_GOAL_REASONING:
        case RCOG_GOAL_PLANNING:
            initial_state.complexity = 0.9f;
            break;
        case RCOG_GOAL_QUESTION_ANSWERING:
        case RCOG_GOAL_EXTRACTION:
            initial_state.complexity = 0.4f;
            break;
        default:
            initial_state.complexity = 0.6f;
    }

    rcog_mcts_user_data_t user_data = {
        .orch = orch,
        .goal = goal
    };

    /* Configure MCTS */
    mcts_config_t config;
    mcts_config_init(&config);
    config.max_iterations = orch->mcts_iterations;
    config.max_depth = 3;
    config.exploration_constant = 1.2f;
    config.discount_factor = 0.9f;
    config.max_nodes = 64;

    config.get_action_count = rcog_mcts_get_action_count;
    config.get_action = rcog_mcts_get_action;
    config.apply_action = rcog_mcts_apply_action;
    config.evaluate = rcog_mcts_evaluate;
    config.is_terminal = rcog_mcts_is_terminal;
    config.free_state = rcog_mcts_free_state;
    config.clone_state = rcog_mcts_clone_state;
    config.user_data = &user_data;
    config.seed = orch->rand_seed;

    mcts_result_t result;
    nimcp_mc_result_t err = mcts_search(&config, &initial_state, &result);

    orch->rand_seed = config.seed;

    if (err != NIMCP_MC_OK) {
        return RCOG_DECOMP_ADAPTIVE;  /* Fallback */
    }

    rcog_decomposition_strategy_t selected =
        (rcog_decomposition_strategy_t)result.best_action;

    mcts_result_free(&result);

    return selected;
}
