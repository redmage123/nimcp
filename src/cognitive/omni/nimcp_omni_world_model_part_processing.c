// nimcp_omni_world_model_part_processing.c - processing functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


int omni_world_model_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_world_model_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_world_model_heartbeat_instance(g_omni_world_model_health_agent, "training_step", progress);
    return 0;
}


/* ============================================================================
 * RSSM Dynamics
 * ============================================================================ */

nimcp_error_t omni_wm_rssm_step(omni_world_model_t* wm,
                                 const omni_wm_rssm_state_t* state,
                                 const float* action,
                                 uint32_t action_dim,
                                 omni_wm_rssm_state_t* next_state) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_step", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(next_state, NIMCP_ERROR_INVALID_PARAM, "next_state is NULL");

    omni_wm_dynamics_t* dyn = wm->forward_dynamics;

    /* Concatenate input: [h, z, a] */
    uint32_t input_dim = state->h_dim + state->z_dim + action_dim;
    float* input = nimcp_calloc(input_dim, sizeof(float));
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate RSSM input buffer");
        return NIMCP_ERROR_NO_MEMORY;
    }

    memcpy(input, state->h, state->h_dim * sizeof(float));
    memcpy(input + state->h_dim, state->z, state->z_dim * sizeof(float));
    uint32_t copy_dim = action_dim < dyn->action_dim ? action_dim : dyn->action_dim;
    memcpy(input + state->h_dim + state->z_dim, action, copy_dim * sizeof(float));

    /* Compute next h: h' = tanh(W_h * [h, z, a] + b_h) */
    for (uint32_t i = 0; i < dyn->h_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dyn->h_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)dyn->h_dim);
        }

        float sum = dyn->b_h[i];
        for (uint32_t j = 0; j < input_dim && j < dyn->h_dim + dyn->z_dim + dyn->action_dim; j++) {
            sum += dyn->W_h[i * (dyn->h_dim + dyn->z_dim + dyn->action_dim) + j] * input[j];
        }
        next_state->h[i] = tanhf(sum);
    }

    /* Compute z prior: [mean, log_std] = W_z * h' + b_z */
    for (uint32_t i = 0; i < dyn->z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dyn->z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)dyn->z_dim);
        }

        float sum_mean = dyn->b_z[i];
        float sum_std = dyn->b_z[dyn->z_dim + i];
        for (uint32_t j = 0; j < dyn->h_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && dyn->h_dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(j + 1) / (float)dyn->h_dim);
            }

            sum_mean += dyn->W_z[i * dyn->h_dim + j] * next_state->h[j];
            sum_std += dyn->W_z[(dyn->z_dim + i) * dyn->h_dim + j] * next_state->h[j];
        }
        next_state->z_mean[i] = sum_mean;
        next_state->z_std[i] = expf(sum_std) + 0.01f; /* Softplus-like */

        /* Sample z from N(mean, std) */
        next_state->z[i] = next_state->z_mean[i] +
                           next_state->z_std[i] * randn(&wm->rand_seed);
    }

    next_state->h_dim = dyn->h_dim;
    next_state->z_dim = dyn->z_dim;

    nimcp_free(input);
    input = NULL;

    wm->stats.forward_predictions++;
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Learning
 * ============================================================================ */

nimcp_error_t omni_wm_update(omni_world_model_t* wm,
                              const omni_wm_state_t* state,
                              const float* action,
                              uint32_t action_dim,
                              const omni_wm_state_t* next_state,
                              float reward) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_update", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(next_state, NIMCP_ERROR_INVALID_PARAM, "next_state is NULL");

    /* Compute prediction error */
    omni_wm_transition_t pred;
    memset(&pred, 0, sizeof(pred));

    nimcp_error_t err = omni_wm_predict_forward(wm, action, action_dim, &pred);
    if (err != NIMCP_SUCCESS) return err;

    float error = 0.0f;
    uint32_t min_dim = pred.next_state->dim < next_state->dim ?
                       pred.next_state->dim : next_state->dim;

    for (uint32_t i = 0; i < min_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)min_dim);
        }

        float diff = pred.next_state->values[i] - next_state->values[i];
        error += diff * diff;
    }
    error = min_dim > 0 ? sqrtf(error / min_dim) : 0.0f;

    /* Update running average of prediction error */
    float alpha = 0.01f;
    float new_mean = (1.0f - alpha) * wm->stats.mean_prediction_error + alpha * error;
    if (isfinite(new_mean)) {
        wm->stats.mean_prediction_error = new_mean;
    }

    /* Simple gradient update on dynamics weights */
    float lr = wm->config.learning_rate;
    omni_wm_dynamics_t* dyn = wm->forward_dynamics;

    /* Update bias towards correct prediction */
    for (uint32_t i = 0; i < dyn->h_dim && i < min_dim; i++) {
        float grad = next_state->values[i] - pred.next_state->values[i];
        dyn->b_h[i] += lr * grad;
    }

    omni_wm_state_destroy(pred.next_state);
    if (pred.action_taken) {
        nimcp_free(pred.action_taken);
    }
    wm->stats.model_updates++;

    return NIMCP_SUCCESS;
}


/**
 * @brief Handle model update message
 *
 * WHAT: Process experience tuple for model learning
 * WHY:  Allow other modules to contribute to world model training
 * HOW:  Call omni_wm_update with provided state transition
 */
static nimcp_error_t handle_omni_wm_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_omni_wm_update_t), NIMCP_ERROR_INVALID_PARAM,
                      "Update request too small: expected %zu bytes", sizeof(bio_msg_omni_wm_update_t));

    const bio_msg_omni_wm_update_t* req = (const bio_msg_omni_wm_update_t*)msg;
    omni_world_model_t* wm = (omni_world_model_t*)user_data;

    NIMCP_LOGGING_DEBUG("Received WM update request: state_dim=%u, action_dim=%u, reward=%.3f",
                        req->state_dim, req->action_dim, req->reward);

    /* Validate dimensions */
    NIMCP_CHECK_THROW(req->state_dim > 0 && req->state_dim <= OMNI_WM_MAX_STATE_DIM &&
                      req->action_dim <= OMNI_WM_MAX_ACTION_DIM, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid dimensions in update request");

    /* Create temporary states from request */
    omni_wm_state_t* state = omni_wm_state_from_values(req->state, req->state_dim);
    omni_wm_state_t* next_state = omni_wm_state_from_values(req->next_state, req->state_dim);

    if (!state || !next_state) {
        if (state) omni_wm_state_destroy(state);
        if (next_state) omni_wm_state_destroy(next_state);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Prepare response */
    bio_msg_omni_wm_update_ack_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_OMNI_WM_UPDATE,
                        BIO_MODULE_OMNI_WORLD_MODEL,
                        req->header.source_module,
                        sizeof(response));

    /* Get prediction error before update for reporting */
    omni_wm_transition_t pred_transition = {0};
    omni_wm_set_state(wm, state);
    omni_wm_predict_forward(wm, req->action, req->action_dim, &pred_transition);

    float prediction_error = 0.0f;
    if (pred_transition.next_state) {
        /* Compute MSE between predicted and actual next state */
        for (uint32_t i = 0; i < req->state_dim && i < pred_transition.next_state->dim; i++) {
            float diff = pred_transition.next_state->values[i] - req->next_state[i];
            prediction_error += diff * diff;
        }
        prediction_error = sqrtf(prediction_error / req->state_dim);
        omni_wm_state_destroy(pred_transition.next_state);
    }
    if (pred_transition.action_taken) {
        nimcp_free(pred_transition.action_taken);
    }

    /* Perform the update */
    nimcp_error_t result = omni_wm_update(wm, state, req->action, req->action_dim,
                                           next_state, req->reward);

    /* Fill response */
    response.status = result;
    response.prediction_error = prediction_error;

    /* Cleanup */
    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);

    /* Send response via promise */
    if (response_promise) {
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    NIMCP_LOGGING_DEBUG("WM update completed: status=%d, pred_error=%.4f",
                        result, prediction_error);

    return result;
}
