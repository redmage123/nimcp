// nimcp_rcog_orchestrator_part_accessors.c - accessors functions
// Part of nimcp_rcog_orchestrator.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_orchestrator.c


void rcog_orchestrator_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_orchestrator_instance_health_agent = agent;
}


/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_orchestrator_config_t rcog_orchestrator_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_default_config", 0.0f);


    rcog_orchestrator_config_t config = {0};

    config.max_recursion_depth = RCOG_DEFAULT_MAX_DEPTH;
    config.max_parallel_subtasks = RCOG_DEFAULT_MAX_PARALLEL_SUBTASKS;

    config.ready_threshold = RCOG_DEFAULT_READY_THRESHOLD;
    config.max_refinement_steps = RCOG_DEFAULT_MAX_REFINEMENT_STEPS;
    config.enable_early_termination = true;

    config.scheduling_policy = RCOG_SCHED_ADAPTIVE;
    config.decomposition_timeout_ms = RCOG_ORCH_DEFAULT_DECOMP_TIMEOUT_MS;

    config.default_strategy = RCOG_DECOMP_ADAPTIVE;
    config.enable_adaptive_strategy = true;

    config.enable_imagination_planning = true;
    config.enable_immune_modulation = true;

    config.enable_trace = false;
    config.verbose_logging = false;

    return config;
}

static uint32_t rcog_mcts_get_action_count(const void* state, void* user_data) {
    (void)state;
    (void)user_data;
    return 4;  /* 4 strategies: SEQUENTIAL, PARALLEL, HIERARCHICAL, ADAPTIVE */
}


static uint32_t rcog_mcts_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)state;
    (void)user_data;
    return idx;  /* Action is strategy index */
}


static bool rcog_mcts_is_terminal(const void* state, void* user_data) {
    const rcog_mcts_state_t* s = (const rcog_mcts_state_t*)state;
    (void)user_data;

    /* Terminal when high confidence or low complexity */
    return s->estimated_confidence >= 0.9f || s->complexity < 0.1f;
}


bool rcog_orchestrator_is_answer_ready(
    const rcog_orchestrator_t* orch,
    const rcog_answer_state_t* answer
) {
    if (!orch || !answer) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_is_answer_ready", 0.0f);


    return answer->ready || answer->confidence >= orch->config.ready_threshold;
}


/*=============================================================================
 * DEPTH CONTROL
 *===========================================================================*/

uint32_t rcog_orchestrator_get_current_depth(const rcog_orchestrator_t* orch) {
    if (!orch) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_current_depth", 0.0f);


    return orch->current_depth;
}


uint32_t rcog_orchestrator_get_max_depth(const rcog_orchestrator_t* orch) {
    if (!orch) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_max_depth", 0.0f);


    return orch->effective_max_depth;
}


int rcog_orchestrator_get_effective_limits(
    const rcog_orchestrator_t* orch,
    uint32_t* max_depth,
    uint32_t* max_parallel
) {
    if (!orch || !max_depth || !max_parallel) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_effective_limits", 0.0f);


    nimcp_mutex_lock(((rcog_orchestrator_t*)orch)->mutex);
    *max_depth = orch->effective_max_depth;
    *max_parallel = orch->effective_max_parallel;
    nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);

    return RCOG_OK;
}


/*=============================================================================
 * STATISTICS AND TRACE
 *===========================================================================*/

int rcog_orchestrator_get_stats(
    const rcog_orchestrator_t* orch,
    rcog_orchestrator_stats_t* stats
) {
    if (!orch || !stats) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_orchestrator_t*)orch)->mutex);
    *stats = orch->stats;

    /* Compute averages */
    if (stats->goals_decomposed > 0) {
        stats->avg_subtasks_per_goal =
            (float)stats->subtasks_created / (float)stats->goals_decomposed;
    }

    nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);

    return RCOG_OK;
}


int rcog_orchestrator_get_trace(
    const rcog_orchestrator_t* orch,
    rcog_trace_t** trace
) {
    if (!orch || !trace) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_trace", 0.0f);


    nimcp_mutex_lock(((rcog_orchestrator_t*)orch)->mutex);

    if (!orch->trace) {
        nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);
        *trace = NULL;
        return RCOG_OK;
    }

    /* Copy trace */
    rcog_trace_t* copy = nimcp_calloc(1, sizeof(rcog_trace_t));
    if (!copy) {
        nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    copy->num_entries = orch->trace->num_entries;
    copy->capacity = orch->trace->num_entries;
    if (copy->num_entries > 0) {
        copy->entries = nimcp_calloc(copy->num_entries, sizeof(rcog_trace_entry_t));
        if (!copy->entries) {
            nimcp_free(copy);
            copy = NULL;
            nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);
            return RCOG_ERROR_OUT_OF_MEMORY;
        }
        memcpy(copy->entries, orch->trace->entries,
               copy->num_entries * sizeof(rcog_trace_entry_t));
    }

    *trace = copy;

    nimcp_mutex_unlock(((rcog_orchestrator_t*)orch)->mutex);

    return RCOG_OK;
}

static uint32_t rcog_get_dep_count(uint32_t node_index, void* user_data) {
    rcog_topo_context_t* ctx = (rcog_topo_context_t*)user_data;
    if (node_index >= ctx->decomp->num_subtasks) {
        return 0;
    }
    return ctx->in_degrees[node_index];
}


/**
 * @brief Get a specific dependency of a subtask (callback for nimcp_topological_sort)
 *
 * This returns the index of the dependency (not the task_id).
 * Since edges are stored as (from, to), we need to find edges where
 * the current task is the 'to' and return the index of the 'from' task.
 */
static uint32_t rcog_get_dep(uint32_t node_index, uint32_t dep_index, void* user_data) {
    rcog_topo_context_t* ctx = (rcog_topo_context_t*)user_data;
    const rcog_decomposition_t* decomp = ctx->decomp;

    if (node_index >= decomp->num_subtasks || !decomp->deps) {
        return UINT32_MAX;
    }

    uint64_t task_id = decomp->subtasks[node_index].task_id;
    uint32_t found = 0;

    /* Find edges where this task is the destination */
    for (size_t i = 0; i < decomp->deps->num_edges; i++) {
        if (decomp->deps->edges[i].to_task_id == task_id) {
            if (found == dep_index) {
                /* Found the edge, now find the index of the source task */
                uint64_t from_id = decomp->deps->edges[i].from_task_id;
                for (size_t j = 0; j < decomp->num_subtasks; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && decomp->num_subtasks > 256) {
                        rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                         (float)(j + 1) / (float)decomp->num_subtasks);
                    }

                    if (decomp->subtasks[j].task_id == from_id) {
                        return (uint32_t)j;
                    }
                }
                return UINT32_MAX;  /* Source not found */
            }
            found++;
        }
    }

    return UINT32_MAX;
}


int rcog_orchestrator_get_topological_order(
    const rcog_decomposition_t* decomp,
    uint64_t* order,
    size_t max_order,
    size_t* num_order
) {
    if (!decomp || !order || !num_order) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_topological_orde", 0.0f);


    if (decomp->num_subtasks > max_order) {
        return RCOG_ERROR_CONTEXT_FULL;
    }

    /* Simple case: no dependencies */
    if (!decomp->deps || decomp->deps->num_edges == 0) {
        for (size_t i = 0; i < decomp->num_subtasks; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && decomp->num_subtasks > 256) {
                rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                 (float)(i + 1) / (float)decomp->num_subtasks);
            }

            order[i] = decomp->subtasks[i].task_id;
        }
        *num_order = decomp->num_subtasks;
        return RCOG_OK;
    }

    /* Precompute in-degrees for the callback */
    uint32_t* in_degrees = nimcp_calloc(decomp->num_subtasks, sizeof(uint32_t));
    if (!in_degrees) {
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < decomp->deps->num_edges; i++) {
        uint64_t to_id = decomp->deps->edges[i].to_task_id;
        for (size_t j = 0; j < decomp->num_subtasks; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && decomp->num_subtasks > 256) {
                rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                 (float)(j + 1) / (float)decomp->num_subtasks);
            }

            if (decomp->subtasks[j].task_id == to_id) {
                in_degrees[j]++;
                break;
            }
        }
    }

    /* Set up context for callbacks */
    rcog_topo_context_t ctx = {
        .decomp = decomp,
        .in_degrees = in_degrees
    };

    nimcp_topo_config_t config = {
        .node_count = (uint32_t)decomp->num_subtasks,
        .user_data = &ctx,
        .get_dep_count = rcog_get_dep_count,
        .get_dep = rcog_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    /* Allocate temporary index array */
    uint32_t* sorted_indices = nimcp_calloc(decomp->num_subtasks, sizeof(uint32_t));
    if (!sorted_indices) {
        nimcp_free(in_degrees);
        in_degrees = NULL;
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    uint32_t sorted_count = 0;
    nimcp_sort_result_t result = nimcp_topological_sort(
        &config, sorted_indices, (uint32_t)decomp->num_subtasks, &sorted_count);

    /* Map indices back to task IDs */
    for (uint32_t i = 0; i < sorted_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sorted_count > 256) {
            rcog_orchestrator_heartbeat("rcog_orchest_loop",
                             (float)(i + 1) / (float)sorted_count);
        }

        uint32_t idx = sorted_indices[i];
        if (idx < decomp->num_subtasks) {
            order[i] = decomp->subtasks[idx].task_id;
        }
    }

    nimcp_free(sorted_indices);
    sorted_indices = NULL;
    nimcp_free(in_degrees);
    in_degrees = NULL;

    *num_order = sorted_count;

    if (result == NIMCP_SORT_ERROR_CYCLE) {
        return RCOG_ERROR_INVALID_CONFIG;
    }
    if (result != NIMCP_SORT_OK) {
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    return RCOG_OK;
}


int rcog_orchestrator_get_ready_subtasks(
    const rcog_decomposition_t* decomp,
    const uint64_t* completed,
    size_t num_completed,
    uint64_t* ready,
    size_t max_ready,
    size_t* num_ready
) {
    if (!decomp || !ready || !num_ready) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_orchestrator_heartbeat("rcog_orchest_get_ready_subtasks", 0.0f);


    size_t n = 0;
    for (size_t i = 0; i < decomp->num_subtasks && n < max_ready; i++) {
        /* Skip already completed */
        bool is_completed = false;
        for (size_t j = 0; j < num_completed; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_completed > 256) {
                rcog_orchestrator_heartbeat("rcog_orchest_loop",
                                 (float)(j + 1) / (float)num_completed);
            }

            if (completed[j] == decomp->subtasks[i].task_id) {
                is_completed = true;
                break;
            }
        }
        if (is_completed) continue;

        /* Check if dependencies satisfied */
        if (task_deps_satisfied(decomp, decomp->subtasks[i].task_id,
                                completed, num_completed)) {
            ready[n++] = decomp->subtasks[i].task_id;
        }
    }

    *num_ready = n;
    return RCOG_OK;
}
