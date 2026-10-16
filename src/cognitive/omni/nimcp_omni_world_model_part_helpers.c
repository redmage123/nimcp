// nimcp_omni_world_model_part_helpers.c - helpers functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c

static float randn(unsigned int* seed) {
    /* Box-Muller transform for normal distribution */
    float u1 = (float)rand_r(seed) / RAND_MAX;
    float u2 = (float)rand_r(seed) / RAND_MAX;
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
}

static nimcp_error_t handle_omni_wm_predict(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_omni_wm_predict_t), NIMCP_ERROR_INVALID_PARAM,
                      "Predict request too small: expected %zu bytes", sizeof(bio_msg_omni_wm_predict_t));

    const bio_msg_omni_wm_predict_t* req = (const bio_msg_omni_wm_predict_t*)msg;
    omni_world_model_t* wm = (omni_world_model_t*)user_data;

    NIMCP_LOGGING_DEBUG("Received WM predict request: dir=%s, state_dim=%u, action_dim=%u",
                        omni_wm_direction_to_string(req->direction),
                        req->state_dim, req->action_dim);

    /* Validate dimensions */
    NIMCP_CHECK_THROW(req->state_dim > 0 && req->state_dim <= OMNI_WM_MAX_STATE_DIM &&
                      req->action_dim <= OMNI_WM_MAX_ACTION_DIM, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid dimensions in predict request");

    /* Create temporary state from request */
    omni_wm_state_t* state = omni_wm_state_from_values(req->state, req->state_dim);
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NO_MEMORY, "failed to create state from request values");

    /* Prepare response */
    bio_msg_omni_wm_predict_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_OMNI_WM_PREDICT,
                        BIO_MODULE_OMNI_WORLD_MODEL,
                        req->header.source_module,
                        sizeof(response));

    /* Perform prediction based on direction */
    omni_wm_transition_t transition = {0};
    nimcp_error_t result = NIMCP_SUCCESS;

    switch (req->direction) {
        case OMNI_WM_DIR_FORWARD:
            result = omni_wm_predict_forward(wm, req->action, req->action_dim, &transition);
            break;
        case OMNI_WM_DIR_BACKWARD:
            result = omni_wm_infer_backward(wm, state, &transition);
            break;
        case OMNI_WM_DIR_LATERAL: {
            /* For lateral, use action_dim as target modality ID.
             * Allocate next_state so the predict function has somewhere to write. */
            omni_wm_state_t* next = omni_wm_state_create(req->state_dim);
            if (!next) {
                omni_wm_state_destroy(state);
                if (response_promise) {
                    bio_msg_omni_wm_predict_response_t err_resp = {0};
                    err_resp.prediction_error = 1.0f;
                    nimcp_bio_promise_complete_sized(response_promise, &err_resp, sizeof(err_resp));
                }
                return NIMCP_ERROR_NO_MEMORY;
            }
            result = omni_wm_predict_lateral(wm, state, req->action_dim, next);
            transition.next_state = next;
            break;
        }
        case OMNI_WM_DIR_HIERARCHICAL: {
            /* For hierarchical, use first action element as target level.
             * Allocate next_state so the predict function has somewhere to write. */
            omni_wm_state_t* next = omni_wm_state_create(req->state_dim);
            if (!next) {
                omni_wm_state_destroy(state);
                if (response_promise) {
                    bio_msg_omni_wm_predict_response_t err_resp = {0};
                    err_resp.prediction_error = 1.0f;
                    nimcp_bio_promise_complete_sized(response_promise, &err_resp, sizeof(err_resp));
                }
                return NIMCP_ERROR_NO_MEMORY;
            }
            result = omni_wm_predict_hierarchical(wm, state,
                                                   (uint32_t)req->action[0],
                                                   next);
            transition.next_state = next;
            break;
        }
        default:
            result = NIMCP_ERROR_INVALID_PARAM;
            break;
    }

    /* Fill response */
    if (result == NIMCP_SUCCESS && transition.next_state) {
        uint32_t copy_dim = transition.next_state->dim;
        if (copy_dim > OMNI_WM_MAX_STATE_DIM) {
            copy_dim = OMNI_WM_MAX_STATE_DIM;
        }
        memcpy(response.predicted_state, transition.next_state->values,
               copy_dim * sizeof(float));
        response.state_dim = copy_dim;
        response.prediction_error = transition.prediction_error;
        response.confidence = 1.0f - transition.prediction_error;
    } else {
        response.state_dim = 0;
        response.prediction_error = 1.0f;
        response.confidence = 0.0f;
    }

    /* Cleanup */
    omni_wm_state_destroy(state);
    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }
    if (transition.action_taken) {
        nimcp_free(transition.action_taken);
    }

    /* Send response via promise */
    if (response_promise) {
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    return result;
}


/**
 * @brief Handle rollout request message
 *
 * WHAT: Execute policy rollout for evaluation
 * WHY:  Enable policy comparison via expected free energy
 * HOW:  Simulate trajectory using provided action sequence
 */
static nimcp_error_t handle_omni_wm_rollout(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_omni_wm_rollout_t), NIMCP_ERROR_INVALID_PARAM,
                      "Rollout request too small: expected %zu bytes", sizeof(bio_msg_omni_wm_rollout_t));

    const bio_msg_omni_wm_rollout_t* req = (const bio_msg_omni_wm_rollout_t*)msg;
    omni_world_model_t* wm = (omni_world_model_t*)user_data;

    NIMCP_LOGGING_DEBUG("Received WM rollout request: horizon=%u, state_dim=%u",
                        req->horizon, req->state_dim);

    /* Validate dimensions */
    NIMCP_CHECK_THROW(req->state_dim > 0 && req->state_dim <= OMNI_WM_MAX_STATE_DIM &&
                      req->action_dim <= OMNI_WM_MAX_ACTION_DIM &&
                      req->horizon <= OMNI_WM_MAX_HORIZON &&
                      req->obs_dim <= OMNI_WM_MAX_OBS_DIM, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid dimensions in rollout request");

    /* Create initial state from request */
    omni_wm_state_t* initial_state = omni_wm_state_from_values(req->initial_state,
                                                                req->state_dim);
    NIMCP_CHECK_THROW(initial_state, NIMCP_ERROR_NO_MEMORY, "failed to create initial state for rollout");

    /* Prepare response */
    bio_msg_omni_wm_rollout_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_OMNI_WM_ROLLOUT,
                        BIO_MODULE_OMNI_WORLD_MODEL,
                        req->header.source_module,
                        sizeof(response));

    /* Execute rollout by manually stepping through actions */
    omni_wm_set_state(wm, initial_state);

    float total_reward = 0.0f;
    uint32_t steps_completed = 0;
    omni_wm_state_t* current_state = omni_wm_state_clone(initial_state);

    for (uint32_t t = 0; t < req->horizon && current_state; t++) {
        /* Get action for this timestep */
        const float* action = &req->policy_actions[t * req->action_dim];

        /* Predict next state */
        omni_wm_transition_t transition = {0};
        nimcp_error_t step_result = omni_wm_predict_forward(wm, action,
                                                             req->action_dim, &transition);

        if (step_result != NIMCP_SUCCESS || !transition.next_state) {
            if (transition.action_taken) nimcp_free(transition.action_taken);
            break;
        }

        /* Accumulate reward (using negative prediction error as proxy) */
        total_reward += -transition.prediction_error;
        steps_completed++;

        /* Move to next state */
        omni_wm_state_destroy(current_state);
        current_state = transition.next_state;
        omni_wm_set_state(wm, current_state);

        if (transition.action_taken) nimcp_free(transition.action_taken);
    }

    /* Compute expected free energy */
    float efe = 0.0f;
    if (req->obs_dim > 0 && current_state) {
        /* Predict observations from final state */
        float predicted_obs[OMNI_WM_MAX_OBS_DIM];
        omni_wm_predict_observations(wm, current_state, predicted_obs, req->obs_dim);

        /* EFE = divergence from preferred + uncertainty */
        for (uint32_t i = 0; i < req->obs_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && req->obs_dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)req->obs_dim);
            }

            float diff = predicted_obs[i] - req->preferred_obs[i];
            efe += diff * diff;
        }
        efe = sqrtf(efe / req->obs_dim);
    }

    /* Fill response */
    response.total_reward = total_reward;
    response.expected_free_energy = efe;
    response.steps_completed = steps_completed;

    if (current_state) {
        uint32_t copy_dim = current_state->dim;
        if (copy_dim > OMNI_WM_MAX_STATE_DIM) {
            copy_dim = OMNI_WM_MAX_STATE_DIM;
        }
        memcpy(response.final_state, current_state->values, copy_dim * sizeof(float));
        response.state_dim = copy_dim;
        omni_wm_state_destroy(current_state);
    }

    /* Cleanup */
    omni_wm_state_destroy(initial_state);

    /* Send response via promise */
    if (response_promise) {
        nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
    }

    NIMCP_LOGGING_DEBUG("WM rollout completed: steps=%u, reward=%.3f, efe=%.3f",
                        steps_completed, total_reward, efe);

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Serialization Helpers
 * ============================================================================ */

/**
 * @brief Simple CRC32 implementation for checksum
 */
static uint32_t crc32_compute(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && length > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)length);
        }

        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && 8 > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(j + 1) / (float)8);
            }

            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}


/**
 * @brief Write uint8 to buffer (big-endian)
 */
static size_t write_u8(uint8_t* buf, size_t pos, uint8_t val) {
    if (buf) buf[pos] = val;
    return pos + 1;
}


/**
 * @brief Write uint32 to buffer (big-endian)
 */
static size_t write_u32(uint8_t* buf, size_t pos, uint32_t val) {
    if (buf) {
        buf[pos] = (val >> 24) & 0xFF;
        buf[pos + 1] = (val >> 16) & 0xFF;
        buf[pos + 2] = (val >> 8) & 0xFF;
        buf[pos + 3] = val & 0xFF;
    }
    return pos + 4;
}


/**
 * @brief Write uint64 to buffer (big-endian)
 */
static size_t write_u64(uint8_t* buf, size_t pos, uint64_t val) {
    if (buf) {
        buf[pos] = (val >> 56) & 0xFF;
        buf[pos + 1] = (val >> 48) & 0xFF;
        buf[pos + 2] = (val >> 40) & 0xFF;
        buf[pos + 3] = (val >> 32) & 0xFF;
        buf[pos + 4] = (val >> 24) & 0xFF;
        buf[pos + 5] = (val >> 16) & 0xFF;
        buf[pos + 6] = (val >> 8) & 0xFF;
        buf[pos + 7] = val & 0xFF;
    }
    return pos + 8;
}


/**
 * @brief Write float to buffer (as uint32, big-endian)
 */
static size_t write_float_be(uint8_t* buf, size_t pos, float val) {
    union { float f; uint32_t i; } conv;
    conv.f = val;
    return write_u32(buf, pos, conv.i);
}


/**
 * @brief Write double to buffer (as uint64, big-endian)
 */
static size_t write_double_be(uint8_t* buf, size_t pos, double val) {
    union { double d; uint64_t i; } conv;
    conv.d = val;
    return write_u64(buf, pos, conv.i);
}


/**
 * @brief Read uint8 from buffer
 */
static uint8_t read_u8(const uint8_t* buf, size_t* pos) {
    uint8_t val = buf[*pos];
    (*pos)++;
    return val;
}


/**
 * @brief Read uint32 from buffer (big-endian)
 */
static uint32_t read_u32(const uint8_t* buf, size_t* pos) {
    uint32_t val = ((uint32_t)buf[*pos] << 24) |
                   ((uint32_t)buf[*pos + 1] << 16) |
                   ((uint32_t)buf[*pos + 2] << 8) |
                   buf[*pos + 3];
    (*pos) += 4;
    return val;
}


/**
 * @brief Read uint64 from buffer (big-endian)
 */
static uint64_t read_u64(const uint8_t* buf, size_t* pos) {
    uint64_t val = ((uint64_t)buf[*pos] << 56) |
                   ((uint64_t)buf[*pos + 1] << 48) |
                   ((uint64_t)buf[*pos + 2] << 40) |
                   ((uint64_t)buf[*pos + 3] << 32) |
                   ((uint64_t)buf[*pos + 4] << 24) |
                   ((uint64_t)buf[*pos + 5] << 16) |
                   ((uint64_t)buf[*pos + 6] << 8) |
                   buf[*pos + 7];
    (*pos) += 8;
    return val;
}


/**
 * @brief Read float from buffer
 */
static float read_float_be(const uint8_t* buf, size_t* pos) {
    union { uint32_t i; float f; } conv;
    conv.i = read_u32(buf, pos);
    return conv.f;
}


/**
 * @brief Read double from buffer
 */
static double read_double_be(const uint8_t* buf, size_t* pos) {
    union { uint64_t i; double d; } conv;
    conv.i = read_u64(buf, pos);
    return conv.d;
}


/**
 * @brief Serialize state
 */
static size_t serialize_state(uint8_t* buf, size_t pos, const omni_wm_state_t* state) {
    if (!state) {
        pos = write_u8(buf, pos, 0); /* null marker */
        return pos;
    }

    pos = write_u8(buf, pos, 1); /* present marker */
    pos = write_u32(buf, pos, state->dim);
    pos = write_float_be(buf, pos, state->uncertainty);
    pos = write_double_be(buf, pos, state->timestamp);
    pos = write_u32(buf, pos, state->level);

    /* Write values array */
    for (uint32_t i = 0; i < state->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)state->dim);
        }

        pos = write_float_be(buf, pos, state->values[i]);
    }

    return pos;
}


/**
 * @brief Deserialize state
 */
static omni_wm_state_t* deserialize_state_from_buf(const uint8_t* buf, size_t* pos) {
    uint8_t present = read_u8(buf, pos);
    if (!present) {
        return NULL;  /* Not-present marker is normal for optional fields */
    }

    uint32_t dim = read_u32(buf, pos);
    omni_wm_state_t* state = omni_wm_state_create(dim);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_state_from_buf: state is NULL");
        return NULL;
    }

    state->uncertainty = read_float_be(buf, pos);
    state->timestamp = read_double_be(buf, pos);
    state->level = read_u32(buf, pos);

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)dim);
        }

        state->values[i] = read_float_be(buf, pos);
    }

    return state;
}


/**
 * @brief Serialize RSSM state
 */
static size_t serialize_rssm_state(uint8_t* buf, size_t pos, const omni_wm_rssm_state_t* state) {
    if (!state) {
        pos = write_u8(buf, pos, 0);
        return pos;
    }

    pos = write_u8(buf, pos, 1);
    pos = write_u32(buf, pos, state->h_dim);
    pos = write_u32(buf, pos, state->z_dim);
    pos = write_double_be(buf, pos, state->timestamp);

    /* Write h array */
    for (uint32_t i = 0; i < state->h_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->h_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)state->h_dim);
        }

        pos = write_float_be(buf, pos, state->h[i]);
    }

    /* Write z array */
    for (uint32_t i = 0; i < state->z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)state->z_dim);
        }

        pos = write_float_be(buf, pos, state->z[i]);
    }

    /* Write z_mean array */
    for (uint32_t i = 0; i < state->z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)state->z_dim);
        }

        pos = write_float_be(buf, pos, state->z_mean[i]);
    }

    /* Write z_std array */
    for (uint32_t i = 0; i < state->z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)state->z_dim);
        }

        pos = write_float_be(buf, pos, state->z_std[i]);
    }

    return pos;
}


/**
 * @brief Deserialize RSSM state
 */
static omni_wm_rssm_state_t* deserialize_rssm_state_from_buf(const uint8_t* buf, size_t* pos) {
    uint8_t present = read_u8(buf, pos);
    if (!present) {
        return NULL;  /* Not-present marker is normal for optional fields */
    }

    uint32_t h_dim = read_u32(buf, pos);
    uint32_t z_dim = read_u32(buf, pos);

    omni_wm_rssm_state_t* state = omni_wm_rssm_state_create(h_dim, z_dim);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_rssm_state_from_buf: state is NULL");
        return NULL;
    }

    state->timestamp = read_double_be(buf, pos);

    for (uint32_t i = 0; i < h_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && h_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)h_dim);
        }

        state->h[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)z_dim);
        }

        state->z[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)z_dim);
        }

        state->z_mean[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)z_dim);
        }

        state->z_std[i] = read_float_be(buf, pos);
    }

    return state;
}


/**
 * @brief Serialize dynamics weights
 */
static size_t serialize_dynamics_weights(uint8_t* buf, size_t pos, const omni_wm_dynamics_t* dyn) {
    if (!dyn) {
        pos = write_u8(buf, pos, 0);
        return pos;
    }

    pos = write_u8(buf, pos, 1);
    pos = write_u32(buf, pos, dyn->h_dim);
    pos = write_u32(buf, pos, dyn->z_dim);
    pos = write_u32(buf, pos, dyn->obs_dim);
    pos = write_u32(buf, pos, dyn->action_dim);

    uint32_t input_dim = dyn->h_dim + dyn->z_dim + dyn->action_dim;

    /* W_h: input_dim * h_dim */
    for (uint32_t i = 0; i < input_dim * dyn->h_dim; i++) {
        pos = write_float_be(buf, pos, dyn->W_h[i]);
    }

    /* W_z: h_dim * z_dim * 2 */
    for (uint32_t i = 0; i < dyn->h_dim * dyn->z_dim * 2; i++) {
        pos = write_float_be(buf, pos, dyn->W_z[i]);
    }

    /* W_obs: (h_dim + z_dim) * obs_dim */
    for (uint32_t i = 0; i < (dyn->h_dim + dyn->z_dim) * dyn->obs_dim; i++) {
        pos = write_float_be(buf, pos, dyn->W_obs[i]);
    }

    /* Biases */
    for (uint32_t i = 0; i < dyn->h_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dyn->h_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)dyn->h_dim);
        }

        pos = write_float_be(buf, pos, dyn->b_h[i]);
    }
    for (uint32_t i = 0; i < dyn->z_dim * 2; i++) {
        pos = write_float_be(buf, pos, dyn->b_z[i]);
    }
    for (uint32_t i = 0; i < dyn->obs_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dyn->obs_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)dyn->obs_dim);
        }

        pos = write_float_be(buf, pos, dyn->b_obs[i]);
    }

    return pos;
}


/**
 * @brief Deserialize dynamics weights
 */
static omni_wm_dynamics_t* deserialize_dynamics_from_buf(const uint8_t* buf, size_t* pos) {
    uint8_t present = read_u8(buf, pos);
    if (!present) {
        return NULL;  /* Not-present marker is normal for optional fields */
    }

    uint32_t h_dim = read_u32(buf, pos);
    uint32_t z_dim = read_u32(buf, pos);
    uint32_t obs_dim = read_u32(buf, pos);
    uint32_t action_dim = read_u32(buf, pos);

    omni_wm_dynamics_t* dyn = dynamics_create(h_dim, z_dim, obs_dim, action_dim);
    if (!dyn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_dynamics_from_buf: dyn is NULL");
        return NULL;
    }

    uint32_t input_dim = h_dim + z_dim + action_dim;

    for (uint32_t i = 0; i < input_dim * h_dim; i++) {
        dyn->W_h[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < h_dim * z_dim * 2; i++) {
        dyn->W_z[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < (h_dim + z_dim) * obs_dim; i++) {
        dyn->W_obs[i] = read_float_be(buf, pos);
    }

    for (uint32_t i = 0; i < h_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && h_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)h_dim);
        }

        dyn->b_h[i] = read_float_be(buf, pos);
    }
    for (uint32_t i = 0; i < z_dim * 2; i++) {
        dyn->b_z[i] = read_float_be(buf, pos);
    }
    for (uint32_t i = 0; i < obs_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && obs_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)obs_dim);
        }

        dyn->b_obs[i] = read_float_be(buf, pos);
    }

    return dyn;
}
