// nimcp_omni_world_model_part_stats.c - stats functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


float omni_wm_mdn_log_prob(const omni_wm_mdn_prediction_t* pred,
                            const float* value) {
    if (!pred || !value) return -FLT_MAX;

    /* Log probability under mixture: log(sum_k pi_k * N(x; mu_k, sigma_k)) */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_mdn_log_prob", 0.0f);


    float max_log = -FLT_MAX;
    float* log_probs = nimcp_calloc(pred->num_components, sizeof(float));
    if (!log_probs) return -FLT_MAX;

    for (uint32_t k = 0; k < pred->num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && pred->num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)pred->num_components);
        }

        float log_prob = logf(pred->components[k].weight);

        for (uint32_t i = 0; i < pred->dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pred->dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)pred->dim);
            }

            float diff = value[i] - pred->components[k].mean[i];
            float std = pred->components[k].std[i];
            if (std < 1e-10f) std = 1e-10f;
            log_prob -= 0.5f * (diff * diff) / (std * std);
            log_prob -= logf(std);
            log_prob -= 0.5f * logf(2.0f * M_PI);
        }

        log_probs[k] = log_prob;
        if (log_prob > max_log) max_log = log_prob;
    }

    /* Log-sum-exp for numerical stability */
    float sum = 0.0f;
    for (uint32_t k = 0; k < pred->num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && pred->num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)pred->num_components);
        }

        sum += expf(log_probs[k] - max_log);
    }

    nimcp_free(log_probs);
    log_probs = NULL;
    /* Guard against logf(0) when num_components is 0 */
    return max_log + logf(sum > 0.0f ? sum : 1e-10f);
}


nimcp_error_t omni_wm_counterfactual(omni_world_model_t* wm,
                                      const omni_wm_counterfactual_query_t* query,
                                      omni_wm_counterfactual_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_counterfactu", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_INVALID_PARAM, "query is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");

    memset(result, 0, sizeof(omni_wm_counterfactual_result_t));

    /* Allocate trajectory */
    result->trajectory = nimcp_calloc(query->horizon, sizeof(omni_wm_state_t*));
    NIMCP_CHECK_THROW(result->trajectory, NIMCP_ERROR_NO_MEMORY, "failed to allocate trajectory");

    result->trajectory[0] = omni_wm_state_clone(query->initial_state);
    if (!result->trajectory[0]) {
        nimcp_free(result->trajectory);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Rollout with hypothetical action */
    float cumulative_reward = 0.0f;

    for (uint32_t t = 1; t < query->horizon; t++) {
        omni_wm_transition_t trans;
        memset(&trans, 0, sizeof(trans));

        /* Use query action for first step, then continue with zeros */
        const float* action = (t == 1) ? query->hypothetical_action : NULL;
        uint32_t adim = (t == 1) ? query->action_dim : 0;

        bool action_allocated = false;
        if (!action) {
            action = nimcp_calloc(wm->config.action_dim, sizeof(float));
            if (!action) {
                for (uint32_t i = 0; i < t; i++) {
                    omni_wm_state_destroy(result->trajectory[i]);
                }
                nimcp_free(result->trajectory);
                result->trajectory = NULL;
                return NIMCP_ERROR_NO_MEMORY;
            }
            adim = wm->config.action_dim;
            action_allocated = true;
        }

        /* Temporarily set state for prediction */
        omni_wm_state_t* old_state = wm->current_state;
        wm->current_state = result->trajectory[t-1];

        nimcp_error_t err = omni_wm_predict_forward(wm, action, adim, &trans);

        wm->current_state = old_state;

        if (action_allocated) nimcp_free((void*)action);

        if (err != NIMCP_SUCCESS || !trans.next_state) {
            /* Cleanup on error */
            for (uint32_t i = 0; i < t; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && t > 256) {
                    omni_world_model_heartbeat("omni_world_m_loop",
                                     (float)(i + 1) / (float)t);
                }

                omni_wm_state_destroy(result->trajectory[i]);
            }
            nimcp_free(result->trajectory);
            result->trajectory = NULL;
            return err != NIMCP_SUCCESS ? err : NIMCP_ERROR_NO_MEMORY;
        }

        result->trajectory[t] = trans.next_state;

        /* Estimate reward (simple heuristic) */
        float state_magnitude = 0.0f;
        for (uint32_t i = 0; i < trans.next_state->dim; i++) {
            state_magnitude += trans.next_state->values[i] * trans.next_state->values[i];
        }
        cumulative_reward += sqrtf(state_magnitude) * 0.1f;
    }

    result->trajectory_len = query->horizon;
    result->expected_reward = cumulative_reward;
    result->confidence = 0.8f / (1.0f + 0.1f * query->horizon);
    result->divergence = 0.0f; /* TODO: compute vs actual */

    wm->stats.counterfactual_queries++;
    return NIMCP_SUCCESS;
}


/**
 * @brief Handle counterfactual query message
 *
 * WHAT: Process "what if" queries for alternative scenarios
 * WHY:  Enable other modules to evaluate hypothetical actions
 * HOW:  Execute counterfactual simulation and return expected outcome
 */
static nimcp_error_t handle_omni_wm_counterfactual(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_omni_wm_counterfactual_t), NIMCP_ERROR_INVALID_PARAM,
                      "Counterfactual request too small: expected %zu bytes", sizeof(bio_msg_omni_wm_counterfactual_t));

    const bio_msg_omni_wm_counterfactual_t* req = (const bio_msg_omni_wm_counterfactual_t*)msg;
    omni_world_model_t* wm = (omni_world_model_t*)user_data;

    NIMCP_LOGGING_DEBUG("Received WM counterfactual request: type=%s, horizon=%u",
                        omni_wm_cf_type_to_string(req->cf_type), req->horizon);

    /* Validate dimensions */
    NIMCP_CHECK_THROW(req->state_dim > 0 && req->state_dim <= OMNI_WM_MAX_STATE_DIM &&
                      req->action_dim <= OMNI_WM_MAX_ACTION_DIM &&
                      req->horizon <= OMNI_WM_MAX_HORIZON, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid dimensions in counterfactual request");

    /* Create temporary state from request */
    omni_wm_state_t* state = omni_wm_state_from_values(req->initial_state, req->state_dim);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NO_MEMORY, "failed to create state from counterfactual request");

    /* Prepare response */
    bio_msg_omni_wm_counterfactual_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_OMNI_WM_COUNTERFACTUAL,
                        BIO_MODULE_OMNI_WORLD_MODEL,
                        req->header.source_module,
                        sizeof(response));

    /* Execute counterfactual query */
    omni_wm_counterfactual_result_t cf_result = {0};
    nimcp_error_t result = omni_wm_what_if(wm,
                                            req->hypothetical_action,
                                            req->action_dim,
                                            req->horizon,
                                            &cf_result);

    /* Fill response */
    if (result == NIMCP_SUCCESS) {
        response.expected_reward = cf_result.expected_reward;
        response.divergence = cf_result.divergence;
        response.confidence = cf_result.confidence;
        response.trajectory_length = cf_result.trajectory_len;

        /* Cleanup counterfactual result */
        omni_wm_cf_result_destroy(&cf_result);
    } else {
        response.expected_reward = 0.0f;
        response.divergence = 1.0f;
        response.confidence = 0.0f;
        response.trajectory_length = 0;
    }

    /* Cleanup */
    omni_wm_state_destroy(state);

    /* Send response via promise */
    if (response_promise) {
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    return result;
}


/**
 * @brief Serialize statistics
 */
static size_t serialize_wm_stats(uint8_t* buf, size_t pos, const omni_wm_stats_t* stats) {
    pos = write_u64(buf, pos, stats->forward_predictions);
    pos = write_u64(buf, pos, stats->backward_inferences);
    pos = write_u64(buf, pos, stats->lateral_predictions);
    pos = write_u64(buf, pos, stats->counterfactual_queries);
    pos = write_u64(buf, pos, stats->rollouts_completed);
    pos = write_u64(buf, pos, stats->model_updates);
    pos = write_float_be(buf, pos, stats->mean_prediction_error);
    pos = write_float_be(buf, pos, stats->mean_counterfactual_divergence);
    return pos;
}


/**
 * @brief Deserialize statistics
 */
static size_t deserialize_wm_stats(const uint8_t* buf, size_t pos, omni_wm_stats_t* stats) {
    stats->forward_predictions = read_u64(buf, &pos);
    stats->backward_inferences = read_u64(buf, &pos);
    stats->lateral_predictions = read_u64(buf, &pos);
    stats->counterfactual_queries = read_u64(buf, &pos);
    stats->rollouts_completed = read_u64(buf, &pos);
    stats->model_updates = read_u64(buf, &pos);
    stats->mean_prediction_error = read_float_be(buf, &pos);
    stats->mean_counterfactual_divergence = read_float_be(buf, &pos);
    return pos;
}
