// nimcp_rcog_engine_part_processing.c - processing functions
// Part of nimcp_rcog_engine.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_engine.c


int rcog_engine_training_step(void* ctx, float progress) {
    rcog_engine_t* engine = (rcog_engine_t*)ctx;
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_engine_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_engine_heartbeat_instance(
        g_rcog_engine_instance_health_agent,
        "rcog_engine_training_step", clamped);
    return 0;
}


/*=============================================================================
 * GOAL PROCESSING
 *===========================================================================*/

int rcog_engine_process(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_process_result_t* result
) {
    if (!engine || !goal || !result) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Create sync request */
    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_process", 0.0f);


    rcog_process_request_t request = rcog_engine_default_request(goal);
    request.mode = RCOG_MODE_SYNC;

    return rcog_engine_process_ex(engine, &request, result, NULL);
}


int rcog_engine_process_ex(
    rcog_engine_t* engine,
    const rcog_process_request_t* request,
    rcog_process_result_t* result,
    rcog_request_handle_t** handle
) {
    if (!engine || !request) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_process_ex", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Check engine state */
    if (engine->state != RCOG_ENGINE_READY &&
        engine->state != RCOG_ENGINE_PROCESSING &&
        engine->state != RCOG_ENGINE_DEGRADED) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Check capacity */
    if (engine->active_count >= engine->config.max_concurrent_goals) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_WORKER_POOL_EXHAUSTED;
    }

    /* Allocate request slot */
    rcog_active_request_t* active_req = allocate_request_slot(engine);
    if (!active_req) {
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_WORKER_POOL_EXHAUSTED;
    }

    /* Create request handle */
    active_req->handle = nimcp_calloc(1, sizeof(rcog_request_handle_t));
    if (!active_req->handle) {
        release_request_slot(engine, active_req);
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    active_req->handle->request_id = engine->next_request_id++;
    active_req->handle->goal = request->goal;
    active_req->handle->mode = request->mode;
    active_req->handle->state = RCOG_ENGINE_PROCESSING;
    active_req->handle->completed = false;
    active_req->handle->cancelled = false;
    active_req->request = *request;
    active_req->started_ms = engine_now_ms();

    engine->state = RCOG_ENGINE_PROCESSING;
    engine->stats.goals_submitted++;

    nimcp_mutex_unlock(engine->mutex);

    /* Create answer state for this request */
    active_req->answer = rcog_answer_state_create(engine->answer_refiner, &request->goal);
    if (!active_req->answer) {
        nimcp_mutex_lock(engine->mutex);
        nimcp_free(active_req->handle);
        active_req->handle = NULL;
        release_request_slot(engine, active_req);
        nimcp_mutex_unlock(engine->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    int proc_result = 0;

    /* Decompose goal */
    if (!request->skip_decomposition) {
        rcog_decomposition_strategy_t strategy = request->strategy_override;
        if (strategy == 0) {
            strategy = engine->config.default_strategy;
        }

        proc_result = rcog_orchestrator_decompose_with_strategy(
            engine->orchestrator,
            &request->goal,
            engine->context_store,
            strategy,
            &active_req->decomp
        );

        if (proc_result != 0) {
            goto cleanup;
        }

        /* Dispatch decomposition to pool */
        if (active_req->decomp.num_subtasks > 0) {
            proc_result = rcog_orchestrator_dispatch(
                engine->orchestrator,
                &active_req->decomp,
                &active_req->batch
            );

            if (proc_result != 0) {
                goto cleanup;
            }

            /* Update stats */
            nimcp_mutex_lock(engine->mutex);
            engine->stats.subtasks_created += active_req->decomp.num_subtasks;
            if (active_req->decomp.metadata.depth > engine->stats.max_depth_reached) {
                engine->stats.max_depth_reached = active_req->decomp.metadata.depth;
            }
            nimcp_mutex_unlock(engine->mutex);
        }
    }

    /* Handle by mode */
    if (request->mode == RCOG_MODE_SYNC) {
        /* Synchronous: wait for completion */
        uint32_t timeout = request->timeout_ms;
        if (timeout == 0) {
            timeout = engine->config.default_timeout_ms;
        }

        if (active_req->batch) {
            proc_result = rcog_delegation_pool_await_batch(
                engine->delegation_pool,
                active_req->batch,
                timeout
            );

            if (proc_result == RCOG_ERROR_TIMEOUT) {
                nimcp_mutex_lock(engine->mutex);
                engine->stats.goals_timeout++;
                nimcp_mutex_unlock(engine->mutex);
                goto cleanup;
            }

            /* Get batch results and aggregate */
            rcog_subtask_result_t* batch_results = NULL;
            size_t num_results = 0;

            if (active_req->decomp.num_subtasks > 0) {
                batch_results = nimcp_calloc(active_req->decomp.num_subtasks,
                                             sizeof(rcog_subtask_result_t));
                if (batch_results) {
                    rcog_delegation_pool_get_batch_results(
                        engine->delegation_pool,
                        active_req->batch,
                        batch_results,
                        active_req->decomp.num_subtasks,
                        &num_results
                    );

                    /* Aggregate results into answer */
                    rcog_orchestrator_aggregate(
                        engine->orchestrator,
                        active_req->batch,
                        batch_results,
                        num_results,
                        active_req->answer
                    );

                    /* Update subtask stats */
                    nimcp_mutex_lock(engine->mutex);
                    for (size_t i = 0; i < num_results; i++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((i & 0xFF) == 0 && num_results > 256) {
                            rcog_engine_heartbeat("rcog_engine_loop",
                                             (float)(i + 1) / (float)num_results);
                        }

                        if (batch_results[i].success) {
                            engine->stats.subtasks_completed++;
                        } else {
                            engine->stats.subtasks_failed++;
                        }
                    }
                    nimcp_mutex_unlock(engine->mutex);

                    nimcp_free(batch_results);
                    batch_results = NULL;
                }
            }
        }

        /* Refine until ready */
        while (!rcog_answer_is_ready(engine->answer_refiner, active_req->answer)) {
            proc_result = rcog_orchestrator_refine(
                engine->orchestrator,
                active_req->answer,
                engine->context_store
            );

            if (proc_result != 0) {
                break;
            }

            /* Check for stall */
            if (rcog_answer_is_stalled(engine->answer_refiner, active_req->answer, 3)) {
                break;
            }

            /* Check max steps */
            if (active_req->answer->refinement_step >= engine->config.max_refinement_steps) {
                break;
            }
        }

        /* Fill result */
        if (result) {
            memset(result, 0, sizeof(rcog_process_result_t));
            result->request_id = active_req->handle->request_id;
            result->goal = request->goal;
            result->answer = *active_req->answer;
            result->success = (active_req->answer->status == RCOG_ANSWER_READY);
            result->subtasks_created = active_req->decomp.num_subtasks;
            result->max_depth_used = active_req->decomp.metadata.depth;
            result->refinement_steps = active_req->answer->refinement_step;
            result->processing_time_ms = engine_now_ms() - active_req->started_ms;

            if (result->success) {
                nimcp_mutex_lock(engine->mutex);
                engine->stats.goals_completed++;
                engine->stats.total_processing_time_ms += result->processing_time_ms;
                engine->stats.avg_confidence =
                    (engine->stats.avg_confidence * (engine->stats.goals_completed - 1) +
                     active_req->answer->confidence) / engine->stats.goals_completed;
                nimcp_mutex_unlock(engine->mutex);
            } else {
                result->error = RCOG_ERROR_ANSWER_NOT_READY;
                result->error_message = "Answer did not reach confidence threshold";
                nimcp_mutex_lock(engine->mutex);
                engine->stats.goals_failed++;
                nimcp_mutex_unlock(engine->mutex);
            }
        }

        active_req->handle->completed = true;

    } else if (request->mode == RCOG_MODE_ASYNC) {
        /* Asynchronous: return handle immediately */
        if (handle) {
            *handle = active_req->handle;
        }
        /* Don't cleanup - caller will use rcog_engine_await */
        return 0;

    } else if (request->mode == RCOG_MODE_STREAMING) {
        /* Streaming: call progress callback */
        if (request->progress_callback) {
            rcog_progress_t progress;
            memset(&progress, 0, sizeof(progress));
            progress.total_subtasks = active_req->decomp.num_subtasks;
            progress.current_confidence = active_req->answer->confidence;
            progress.elapsed_ms = engine_now_ms() - active_req->started_ms;
            request->progress_callback(&progress, request->progress_user_data);
        }

        if (handle) {
            *handle = active_req->handle;
        }
        return 0;
    }

cleanup:
    nimcp_mutex_lock(engine->mutex);

    /* Update engine state if no more active requests */
    if (engine->active_count <= 1) {
        engine->state = engine->in_degraded_mode ? RCOG_ENGINE_DEGRADED : RCOG_ENGINE_READY;
    }

    /* Free request resources for sync mode */
    if (request->mode == RCOG_MODE_SYNC) {
        if (active_req->batch) {
            rcog_delegation_pool_free_batch_handle(active_req->batch);
            active_req->batch = NULL;
        }
        rcog_orchestrator_free_decomposition(&active_req->decomp);
        /* Answer state transferred to result, don't free */
        active_req->answer = NULL;
        nimcp_free(active_req->handle);
        active_req->handle = NULL;
        release_request_slot(engine, active_req);
    }

    nimcp_mutex_unlock(engine->mutex);

    return proc_result;
}


int rcog_engine_process_async(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_progress_callback_t callback,
    void* user_data,
    rcog_request_handle_t** handle
) {
    if (!engine || !goal || !handle) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_process_async", 0.0f);


    rcog_process_request_t request = rcog_engine_default_request(goal);
    request.mode = RCOG_MODE_ASYNC;
    request.progress_callback = callback;
    request.progress_user_data = user_data;

    return rcog_engine_process_ex(engine, &request, NULL, handle);
}
