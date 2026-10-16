// nimcp_omni_world_model_part_core.c - core functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_world_model_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_world_model_training_begin: NULL argument");
        return -1;
    }
    omni_world_model_heartbeat_instance(g_omni_world_model_health_agent, "training_begin", 0.0f);
    return 0;
}


int omni_world_model_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_world_model_training_end: NULL argument");
        return -1;
    }
    omni_world_model_heartbeat_instance(g_omni_world_model_health_agent, "training_end", 1.0f);
    return 0;
}

float omni_wm_symlog(float x) {
    /* symlog(x) = sign(x) * ln(|x| + 1) */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_symlog", 0.0f);


    if (x >= 0.0f) {
        return logf(x + 1.0f);
    } else {
        return -logf(-x + 1.0f);
    }
}


float omni_wm_symexp(float x) {
    /* symexp(x) = sign(x) * (exp(|x|) - 1) */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_symexp", 0.0f);


    if (x >= 0.0f) {
        return expf(x) - 1.0f;
    } else {
        return -(expf(-x) - 1.0f);
    }
}


void omni_wm_symlog_array(const float* input, float* output, uint32_t size) {
    if (!input || !output) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_symlog_array", 0.0f);


    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)size);
        }

        output[i] = omni_wm_symlog(input[i]);
    }
}


void omni_wm_symexp_array(const float* input, float* output, uint32_t size) {
    if (!input || !output) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_symexp_array", 0.0f);


    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)size);
        }

        output[i] = omni_wm_symexp(input[i]);
    }
}


omni_wm_state_t* omni_wm_state_from_values(const float* values, uint32_t dim) {
    if (!values || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_state_from_values: values is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_state_from_v", 0.0f);


    omni_wm_state_t* state = omni_wm_state_create(dim);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_from_values: state is NULL");
        return NULL;
    }

    memcpy(state->values, values, dim * sizeof(float));
    return state;
}


omni_wm_state_t* omni_wm_state_clone(const omni_wm_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_state_clone: state is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_state_clone", 0.0f);


    omni_wm_state_t* clone = omni_wm_state_create(state->dim);
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_clone: clone is NULL");
        return NULL;
    }

    memcpy(clone->values, state->values, state->dim * sizeof(float));
    clone->uncertainty = state->uncertainty;
    clone->timestamp = state->timestamp;
    clone->level = state->level;

    return clone;
}


omni_wm_rssm_state_t* omni_wm_rssm_state_clone(const omni_wm_rssm_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_rssm_state_clone: state is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_c", 0.0f);


    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_create(state->h_dim, state->z_dim);
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_clone: clone is NULL");
        return NULL;
    }

    memcpy(clone->h, state->h, state->h_dim * sizeof(float));
    memcpy(clone->z, state->z, state->z_dim * sizeof(float));
    memcpy(clone->z_mean, state->z_mean, state->z_dim * sizeof(float));
    memcpy(clone->z_std, state->z_std, state->z_dim * sizeof(float));
    clone->timestamp = state->timestamp;

    return clone;
}


nimcp_error_t omni_wm_rssm_imagine(omni_world_model_t* wm,
                                    const omni_wm_rssm_state_t* initial_state,
                                    const float* const* actions,
                                    uint32_t horizon,
                                    omni_wm_rssm_state_t** trajectory) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_imagine", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(initial_state, NIMCP_ERROR_INVALID_PARAM, "initial_state is NULL");
    NIMCP_CHECK_THROW(actions, NIMCP_ERROR_INVALID_PARAM, "actions is NULL");
    NIMCP_CHECK_THROW(trajectory, NIMCP_ERROR_INVALID_PARAM, "trajectory is NULL");
    NIMCP_CHECK_THROW(horizon > 0 && horizon <= OMNI_WM_MAX_HORIZON, NIMCP_ERROR_INVALID_PARAM,
                      "horizon must be between 1 and OMNI_WM_MAX_HORIZON");

    /* First state is initial */
    trajectory[0] = omni_wm_rssm_state_clone(initial_state);
    NIMCP_CHECK_THROW(trajectory[0], NIMCP_ERROR_NO_MEMORY, "failed to clone initial state");

    /* Roll out imagination */
    for (uint32_t t = 1; t < horizon; t++) {
        trajectory[t] = omni_wm_rssm_state_create(
            wm->config.rssm_h_dim,
            wm->config.rssm_z_dim
        );
        if (!trajectory[t]) {
            for (uint32_t i = 0; i < t; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && t > 256) {
                    omni_world_model_heartbeat("omni_world_m_loop",
                                     (float)(i + 1) / (float)t);
                }

                omni_wm_rssm_state_destroy(trajectory[i]);
            }
            return NIMCP_ERROR_NO_MEMORY;
        }

        nimcp_error_t err = omni_wm_rssm_step(
            wm, trajectory[t-1], actions[t-1],
            wm->config.action_dim, trajectory[t]
        );
        if (err != NIMCP_SUCCESS) {
            for (uint32_t i = 0; i <= t; i++) {
                omni_wm_rssm_state_destroy(trajectory[i]);
            }
            return err;
        }
    }

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Dynamics Prediction
 * ============================================================================ */

nimcp_error_t omni_wm_predict_forward(omni_world_model_t* wm,
                                       const float* action,
                                       uint32_t action_dim,
                                       omni_wm_transition_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_forw", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");

    /* Use RSSM if enabled */
    if (wm->config.use_rssm && wm->rssm_state) {
        omni_wm_rssm_state_t* next_rssm = omni_wm_rssm_state_create(
            wm->config.rssm_h_dim,
            wm->config.rssm_z_dim
        );
        NIMCP_CHECK_THROW(next_rssm, NIMCP_ERROR_NO_MEMORY, "failed to create next RSSM state");

        nimcp_error_t err = omni_wm_rssm_step(wm, wm->rssm_state,
                                               action, action_dim, next_rssm);
        if (err != NIMCP_SUCCESS) {
            omni_wm_rssm_state_destroy(next_rssm);
            return err;
        }

        /* Convert RSSM state to simple state */
        result->next_state = omni_wm_state_create(wm->config.state_dim);
        if (!result->next_state) {
            omni_wm_rssm_state_destroy(next_rssm);
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Concatenate h and z into state */
        uint32_t copy_h = next_rssm->h_dim < result->next_state->dim ?
                          next_rssm->h_dim : result->next_state->dim;
        memcpy(result->next_state->values, next_rssm->h, copy_h * sizeof(float));

        if (copy_h < result->next_state->dim) {
            uint32_t copy_z = next_rssm->z_dim;
            if (copy_h + copy_z > result->next_state->dim) {
                copy_z = result->next_state->dim - copy_h;
            }
            memcpy(result->next_state->values + copy_h,
                   next_rssm->z, copy_z * sizeof(float));
        }

        /* Compute uncertainty from z_std */
        float uncertainty = 0.0f;
        for (uint32_t i = 0; i < next_rssm->z_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && next_rssm->z_dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)next_rssm->z_dim);
            }

            uncertainty += logf(next_rssm->z_std[i]);
        }
        result->next_state->uncertainty = uncertainty / next_rssm->z_dim;

        /* Update internal RSSM state */
        omni_wm_rssm_state_destroy(wm->rssm_state);
        wm->rssm_state = next_rssm;

        result->direction = OMNI_WM_DIR_FORWARD;
        result->log_prob = 0.0f; /* TODO: compute properly */
        result->prediction_error = 0.0f;

        return NIMCP_SUCCESS;
    }

    /* Simple linear prediction fallback */
    NIMCP_CHECK_THROW(wm->current_state, NIMCP_ERROR_INVALID_PARAM, "current_state is NULL");

    result->next_state = omni_wm_state_clone(wm->current_state);
    NIMCP_CHECK_THROW(result->next_state, NIMCP_ERROR_NO_MEMORY, "failed to clone current state");

    /* Simple dynamics: next = current + action_effect */
    uint32_t min_dim = action_dim < result->next_state->dim ?
                       action_dim : result->next_state->dim;
    for (uint32_t i = 0; i < min_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)min_dim);
        }

        result->next_state->values[i] += action[i] * 0.1f;
    }

    result->direction = OMNI_WM_DIR_FORWARD;
    wm->stats.forward_predictions++;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_infer_backward(omni_world_model_t* wm,
                                      const omni_wm_state_t* current_state,
                                      omni_wm_transition_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_infer_backwa", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(current_state, NIMCP_ERROR_INVALID_PARAM, "current_state is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");

    /* Create previous state estimate */
    result->next_state = omni_wm_state_clone(current_state);
    NIMCP_CHECK_THROW(result->next_state, NIMCP_ERROR_NO_MEMORY, "failed to clone current state");

    /* Backward dynamics: infer what action led here */
    /* This is an inverse problem - approximate solution */
    result->action_taken = nimcp_calloc(wm->config.action_dim, sizeof(float));
    if (!result->action_taken) {
        omni_wm_state_destroy(result->next_state);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Simple heuristic: action proportional to state */
    for (uint32_t i = 0; i < wm->config.action_dim && i < current_state->dim; i++) {
        result->action_taken[i] = current_state->values[i] * 0.1f;
    }

    result->action_dim = wm->config.action_dim;
    result->direction = OMNI_WM_DIR_BACKWARD;

    wm->stats.backward_inferences++;
    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_predict_lateral(omni_world_model_t* wm,
                                       const omni_wm_state_t* source_state,
                                       uint32_t target_modality,
                                       omni_wm_state_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_late", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(source_state, NIMCP_ERROR_INVALID_PARAM, "source_state is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");
    NIMCP_CHECK_THROW(wm->config.enable_lateral, NIMCP_ERROR_NOT_IMPLEMENTED, "lateral prediction not enabled");

    /* Cross-modal prediction using lateral dynamics */
    memset(result->values, 0, result->dim * sizeof(float));

    /* Simple linear mapping */
    uint32_t min_dim = source_state->dim < result->dim ?
                       source_state->dim : result->dim;
    for (uint32_t i = 0; i < min_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)min_dim);
        }

        result->values[i] = source_state->values[i] * 0.9f;
    }

    result->uncertainty = source_state->uncertainty * 1.1f;
    wm->stats.lateral_predictions++;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_predict_hierarchical(omni_world_model_t* wm,
                                            const omni_wm_state_t* state,
                                            uint32_t target_level,
                                            omni_wm_state_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_hier", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");
    NIMCP_CHECK_THROW(wm->config.enable_hierarchical, NIMCP_ERROR_NOT_IMPLEMENTED, "hierarchical prediction not enabled");
    NIMCP_CHECK_THROW(target_level < wm->config.num_levels, NIMCP_ERROR_INVALID_PARAM, "target_level exceeds num_levels");

    /* Hierarchical abstraction/concretization */
    uint32_t level_diff = target_level > state->level ?
                          target_level - state->level :
                          state->level - target_level;

    memset(result->values, 0, result->dim * sizeof(float));

    if (target_level == state->level) {
        /* Same level: identity copy */
        uint32_t min_dim = state->dim < result->dim ? state->dim : result->dim;
        for (uint32_t i = 0; i < min_dim; i++) {
            result->values[i] = state->values[i];
        }
    } else if (target_level > state->level) {
        /* Abstraction: pool groups of 2^level_diff values, average, apply tanh */
        uint32_t pool_factor = 1u << level_diff;
        uint32_t pooled_count = state->dim / pool_factor;
        if (pooled_count > result->dim) pooled_count = result->dim;

        for (uint32_t i = 0; i < pooled_count; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < pool_factor; j++) {
                uint32_t src_idx = i * pool_factor + j;
                if (src_idx < state->dim) {
                    sum += state->values[src_idx];
                }
            }
            float avg = sum / (float)pool_factor;
            result->values[i] = tanhf(avg);
        }
    } else {
        /* Concretization: linear interpolation to expand */
        uint32_t expand_factor = 1u << level_diff;
        uint32_t expanded_max = state->dim * expand_factor;
        uint32_t fill_dim = expanded_max < result->dim ? expanded_max : result->dim;

        for (uint32_t i = 0; i < fill_dim; i++) {
            /* Map output index to fractional source position */
            float src_pos = (float)i / (float)expand_factor;
            uint32_t src_lo = (uint32_t)src_pos;
            uint32_t src_hi = src_lo + 1;
            float frac = src_pos - (float)src_lo;

            float val_lo = (src_lo < state->dim) ? state->values[src_lo] : 0.0f;
            float val_hi = (src_hi < state->dim) ? state->values[src_hi] : val_lo;

            result->values[i] = val_lo + frac * (val_hi - val_lo);
        }
    }

    result->level = target_level;
    /* Uncertainty grows logarithmically with level distance */
    uint32_t info_factor = 1u << level_diff;
    result->uncertainty = state->uncertainty * (1.0f + 0.15f * logf((float)(info_factor + 1)));

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_encode(omni_world_model_t* wm,
                              const float* observation,
                              uint32_t obs_dim,
                              omni_wm_latent_t* latent) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_encode", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(observation, NIMCP_ERROR_INVALID_PARAM, "observation is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");

    /* Linear encoding: latent = ReLU(W * obs + b) */
    for (uint32_t i = 0; i < latent->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)latent->dim);
        }

        float sum = wm->encoder_b[i];
        for (uint32_t j = 0; j < obs_dim && j < wm->config.obs_dim; j++) {
            sum += wm->encoder_W[i * wm->config.obs_dim + j] * observation[j];
        }
        latent->embedding[i] = sum > 0 ? sum : 0; /* ReLU */
    }

    /* Compute information content (approximate entropy) */
    float entropy = 0.0f;
    for (uint32_t i = 0; i < latent->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && latent->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)latent->dim);
        }

        if (latent->embedding[i] > 0.01f) {
            entropy -= latent->embedding[i] * logf(latent->embedding[i]);
        }
    }
    latent->information_content = entropy;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_decode(omni_world_model_t* wm,
                              const omni_wm_latent_t* latent,
                              float* observation,
                              uint32_t obs_dim) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_decode", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");
    NIMCP_CHECK_THROW(observation, NIMCP_ERROR_INVALID_PARAM, "observation is NULL");

    /* Linear decoding: obs = W * latent + b */
    for (uint32_t i = 0; i < obs_dim && i < wm->config.obs_dim; i++) {
        float sum = wm->decoder_b[i];
        for (uint32_t j = 0; j < latent->dim && j < wm->config.latent_dim; j++) {
            sum += wm->decoder_W[i * wm->config.latent_dim + j] * latent->embedding[j];
        }
        observation[i] = sum;
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_predict_latent(omni_world_model_t* wm,
                                      const omni_wm_latent_t* latent,
                                      const float* action,
                                      uint32_t action_dim,
                                      omni_wm_latent_t* predicted_latent) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_late", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(predicted_latent, NIMCP_ERROR_INVALID_PARAM, "predicted_latent is NULL");

    /* JEPA-style: predict in latent space */
    /* Simple dynamics: next_latent = f(latent, action) */
    for (uint32_t i = 0; i < predicted_latent->dim && i < latent->dim; i++) {
        float action_effect = 0.0f;
        if (i < action_dim) {
            action_effect = action[i] * 0.1f;
        }
        predicted_latent->embedding[i] = latent->embedding[i] + action_effect;
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_predict_mdn(omni_world_model_t* wm,
                                   const omni_wm_state_t* state,
                                   const float* action,
                                   uint32_t action_dim,
                                   omni_wm_mdn_prediction_t* pred) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_mdn", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_INVALID_PARAM, "MDN prediction is NULL");

    /* Generate mixture components based on state and action */
    for (uint32_t k = 0; k < pred->num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && pred->num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)pred->num_components);
        }

        /* Each component gets slightly different prediction */
        float offset = (float)k * 0.1f - 0.05f * pred->num_components;

        for (uint32_t i = 0; i < pred->dim && i < state->dim; i++) {
            float action_effect = (i < action_dim) ? action[i] * 0.1f : 0.0f;
            pred->components[k].mean[i] = state->values[i] + action_effect + offset;
            pred->components[k].std[i] = 0.1f + 0.05f * k;
        }

        pred->components[k].weight = 1.0f / pred->num_components;
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_mdn_sample(const omni_wm_mdn_prediction_t* pred,
                                  float* sample) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_mdn_sample", 0.0f);


    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_INVALID_PARAM, "MDN prediction is NULL");
    NIMCP_CHECK_THROW(sample, NIMCP_ERROR_INVALID_PARAM, "sample buffer is NULL");

    /* Select component based on weights - use thread-local seed */
    static _Thread_local unsigned int tl_mdn_seed = 0;
    if (tl_mdn_seed == 0) {
        tl_mdn_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)&tl_mdn_seed;
    }
    unsigned int seed = tl_mdn_seed;
    float r = (float)rand_r(&seed) / RAND_MAX;
    float cumsum = 0.0f;
    uint32_t selected = 0;

    for (uint32_t k = 0; k < pred->num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && pred->num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)pred->num_components);
        }

        cumsum += pred->components[k].weight;
        if (r <= cumsum) {
            selected = k;
            break;
        }
    }

    /* Sample from selected component */
    omni_wm_mdn_component_t* comp = &pred->components[selected];
    for (uint32_t i = 0; i < pred->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pred->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)pred->dim);
        }

        sample[i] = comp->mean[i] + comp->std[i] * randn(&seed);
    }
    tl_mdn_seed = seed;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_mdn_mode(const omni_wm_mdn_prediction_t* pred,
                                float* mode) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_mdn_mode", 0.0f);


    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_INVALID_PARAM, "MDN prediction is NULL");
    NIMCP_CHECK_THROW(mode, NIMCP_ERROR_INVALID_PARAM, "mode buffer is NULL");

    /* Find component with highest weight */
    uint32_t best = 0;
    float best_weight = pred->components[0].weight;

    for (uint32_t k = 1; k < pred->num_components; k++) {
        if (pred->components[k].weight > best_weight) {
            best_weight = pred->components[k].weight;
            best = k;
        }
    }

    /* Return mean of best component */
    memcpy(mode, pred->components[best].mean, pred->dim * sizeof(float));

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_add_experience(omni_world_model_t* wm,
                                      const omni_wm_experience_t* exp) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_add_experien", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(exp, NIMCP_ERROR_INVALID_PARAM, "experience is NULL");

    omni_wm_replay_buffer_t* buf = wm->replay_buffer;

    /* Clone experience */
    omni_wm_experience_t* clone = omni_wm_experience_create(
        wm->config.state_dim,
        exp->action_dim,
        exp->obs_dim
    );
    NIMCP_CHECK_THROW(clone, NIMCP_ERROR_NO_MEMORY, "failed to create experience clone");

    memcpy(clone->action, exp->action, exp->action_dim * sizeof(float));
    clone->reward = exp->reward;
    clone->symlog_reward = wm->config.use_symlog_rewards ?
                           omni_wm_symlog(exp->reward) : exp->reward;
    clone->terminal = exp->terminal;
    clone->timestamp = exp->timestamp;

    if (exp->observation) {
        memcpy(clone->observation, exp->observation, exp->obs_dim * sizeof(float));
    }

    /* Add to circular buffer */
    if (buf->size < buf->capacity) {
        buf->experiences[buf->size] = clone;
        buf->priorities[buf->size] = 1.0f;
        buf->size++;
    } else {
        /* Overwrite oldest */
        omni_wm_experience_destroy(buf->experiences[buf->head]);
        buf->experiences[buf->head] = clone;
        buf->priorities[buf->head] = 1.0f;
    }
    buf->head = (buf->head + 1) % buf->capacity;

    return NIMCP_SUCCESS;
}


uint32_t omni_wm_sample_experiences(omni_world_model_t* wm,
                                     omni_wm_experience_t** batch,
                                     uint32_t batch_size) {
    if (!wm || !batch || batch_size == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_sample_exper", 0.0f);


    omni_wm_replay_buffer_t* buf = wm->replay_buffer;
    if (!buf || buf->size == 0) return 0;

    uint32_t actual_size = batch_size < buf->size ? batch_size : buf->size;

    /* Simple uniform sampling - return clones so caller can safely destroy */
    for (uint32_t i = 0; i < actual_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)actual_size);
        }

        uint32_t idx = rand_r(&wm->rand_seed) % buf->size;
        omni_wm_experience_t* src = buf->experiences[idx];
        if (!src) {
            batch[i] = NULL;
            continue;
        }

        /* Clone the experience */
        omni_wm_experience_t* clone = omni_wm_experience_create(
            wm->config.state_dim,
            src->action_dim,
            src->obs_dim
        );
        if (!clone) {
            /* Clean up already allocated clones */
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    omni_world_model_heartbeat("omni_world_m_loop",
                                     (float)(j + 1) / (float)i);
                }

                omni_wm_experience_destroy(batch[j]);
                batch[j] = NULL;
            }
            return 0;
        }

        memcpy(clone->action, src->action, src->action_dim * sizeof(float));
        clone->reward = src->reward;
        clone->symlog_reward = src->symlog_reward;
        clone->terminal = src->terminal;
        clone->timestamp = src->timestamp;

        if (src->observation && clone->observation) {
            memcpy(clone->observation, src->observation, src->obs_dim * sizeof(float));
        }

        batch[i] = clone;
    }

    return actual_size;
}


nimcp_error_t omni_wm_clear_replay(omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_clear_replay", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(wm->replay_buffer, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    for (uint32_t i = 0; i < wm->replay_buffer->size; i++) {
        omni_wm_experience_destroy(wm->replay_buffer->experiences[i]);
        wm->replay_buffer->experiences[i] = NULL;
    }
    wm->replay_buffer->size = 0;
    wm->replay_buffer->head = 0;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_dream(omni_world_model_t* wm,
                             uint32_t num_episodes,
                             uint32_t episode_length) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_dream", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(wm->config.enable_dreaming, NIMCP_ERROR_NOT_IMPLEMENTED, "dreaming not enabled");

    /* W8: KG emit — announce the dream session. Read-path queries whether
     * the FEP root node is present so we only cross-link when it is. */
    {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            world_model_kg_emit_wm_step(_kg_brain,
                (uint64_t)num_episodes, 0.0f, 0.0f);
            (void)world_model_kg_has_partner(_kg_brain, "cog_fep_free_energy");
        }
    }

    for (uint32_t ep = 0; ep < num_episodes; ep++) {
        /* Phase 8: Loop progress heartbeat */
        if ((ep & 0xFF) == 0 && num_episodes > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(ep + 1) / (float)num_episodes);
        }

        /* Start from random experience or current state */
        omni_wm_rssm_state_t* dream_state = NULL;

        if (wm->replay_buffer->size > 0) {
            uint32_t idx = rand_r(&wm->rand_seed) % wm->replay_buffer->size;
            dream_state = omni_wm_rssm_state_clone(
                wm->replay_buffer->experiences[idx]->state
            );
        } else if (wm->rssm_state) {
            dream_state = omni_wm_rssm_state_clone(wm->rssm_state);
        } else {
            continue;
        }

        if (!dream_state) continue;

        /* Dream rollout */
        for (uint32_t t = 0; t < episode_length; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && episode_length > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(t + 1) / (float)episode_length);
            }

            /* Generate random action with noise */
            float* action = nimcp_calloc(wm->config.action_dim, sizeof(float));
            if (!action) break;

            for (uint32_t i = 0; i < wm->config.action_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && wm->config.action_dim > 256) {
                    omni_world_model_heartbeat("omni_world_m_loop",
                                     (float)(i + 1) / (float)wm->config.action_dim);
                }

                action[i] = randn(&wm->rand_seed) * wm->config.imagination_noise;
            }

            /* Step in dream */
            omni_wm_rssm_state_t* next_dream = omni_wm_rssm_state_create(
                wm->config.rssm_h_dim,
                wm->config.rssm_z_dim
            );
            if (!next_dream) {
                nimcp_free(action);
                action = NULL;
                break;
            }

            omni_wm_rssm_step(wm, dream_state, action, wm->config.action_dim, next_dream);

            omni_wm_rssm_state_destroy(dream_state);
            dream_state = next_dream;
            nimcp_free(action);
            action = NULL;
        }

        omni_wm_rssm_state_destroy(dream_state);
    }

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_generate_dream(omni_world_model_t* wm,
                                      uint32_t dream_length,
                                      float noise_scale,
                                      omni_wm_rollout_t* rollout) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_generate_dr", 0.0f);

    if (!wm) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!rollout) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Fall back to config dream_horizon when caller passes 0 */
    if (dream_length == 0) {
        dream_length = wm->config.dream_horizon;
    }
    if (dream_length > OMNI_WM_MAX_HORIZON) {
        dream_length = OMNI_WM_MAX_HORIZON;
    }

    rollout->length = dream_length;
    rollout->total_reward = 0.0f;
    rollout->expected_free_energy = 0.0f;

    /* Seed state: use current state if available, otherwise create one */
    for (uint32_t i = 0; i < dream_length; i++) {
        omni_wm_state_t* st = omni_wm_state_create(wm->config.state_dim);
        if (!st) {
            rollout->length = i;
            return NIMCP_ERROR_NO_MEMORY;
        }

        if (i == 0) {
            /* Seed from current state or initialise with small random values */
            if (wm->current_state) {
                uint32_t copy_dim = wm->current_state->dim < st->dim
                                        ? wm->current_state->dim : st->dim;
                memcpy(st->values, wm->current_state->values,
                       copy_dim * sizeof(float));
                st->uncertainty = wm->current_state->uncertainty;
            } else {
                for (uint32_t j = 0; j < st->dim; j++) {
                    st->values[j] = randn(&wm->rand_seed) * 0.1f;
                }
                st->uncertainty = 0.5f;
            }
        } else {
            /* Simple forward dynamics: tanh(prev + noise) */
            omni_wm_state_t* prev = rollout->states[i - 1];
            for (uint32_t j = 0; j < st->dim; j++) {
                float noise = randn(&wm->rand_seed) * noise_scale;
                st->values[j] = tanhf(prev->values[j] + noise);
            }
            st->uncertainty = prev->uncertainty * 1.05f;
        }
        rollout->states[i] = st;
    }

    /* Allocate actions for transitions 0..length-2 */
    for (uint32_t i = 0; i + 1 < dream_length; i++) {
        float* act = nimcp_calloc(wm->config.action_dim, sizeof(float));
        if (!act) {
            rollout->length = dream_length;
            return NIMCP_ERROR_NO_MEMORY;
        }
        for (uint32_t j = 0; j < wm->config.action_dim; j++) {
            act[j] = randn(&wm->rand_seed) * noise_scale;
        }
        rollout->actions[i] = act;

        /* Simple reward: negative squared uncertainty growth */
        float r = -0.01f * rollout->states[i + 1]->uncertainty;
        rollout->rewards[i] = r;
        rollout->total_reward += r;
    }

    rollout->expected_free_energy = rollout->total_reward * 0.5f;

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_what_if(omni_world_model_t* wm,
                               const float* action,
                               uint32_t action_dim,
                               uint32_t horizon,
                               omni_wm_counterfactual_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_what_if", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");
    NIMCP_CHECK_THROW(wm->current_state, NIMCP_ERROR_INVALID_PARAM, "current_state is NULL");

    omni_wm_counterfactual_query_t* query = omni_wm_cf_query_create(
        OMNI_WM_CF_ACTION,
        wm->current_state,
        action,
        action_dim,
        horizon
    );
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NO_MEMORY, "failed to create counterfactual query");

    nimcp_error_t err = omni_wm_counterfactual(wm, query, result);
    omni_wm_cf_query_destroy(query);

    return err;
}


nimcp_error_t omni_wm_rollout(omni_world_model_t* wm,
                               omni_wm_policy_fn policy,
                               uint32_t horizon,
                               omni_wm_rollout_t* rollout,
                               void* user_data) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rollout", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(policy, NIMCP_ERROR_INVALID_PARAM, "policy function is NULL");
    NIMCP_CHECK_THROW(rollout, NIMCP_ERROR_INVALID_PARAM, "rollout is NULL");
    NIMCP_CHECK_THROW(wm->current_state, NIMCP_ERROR_INVALID_PARAM, "current_state is NULL");

    rollout->states[0] = omni_wm_state_clone(wm->current_state);
    NIMCP_CHECK_THROW(rollout->states[0], NIMCP_ERROR_NO_MEMORY, "failed to clone current state for rollout");

    float total_reward = 0.0f;

    for (uint32_t t = 0; t < horizon - 1; t++) {
        /* Get action from policy */
        float* action = nimcp_calloc(wm->config.action_dim, sizeof(float));
        if (!action) break;

        policy(rollout->states[t], action, user_data);
        rollout->actions[t] = action;

        /* Predict next state */
        omni_wm_transition_t trans;
        memset(&trans, 0, sizeof(trans));

        omni_wm_state_t* old = wm->current_state;
        wm->current_state = rollout->states[t];

        nimcp_error_t err = omni_wm_predict_forward(wm, action,
                                                     wm->config.action_dim, &trans);
        wm->current_state = old;

        if (err != NIMCP_SUCCESS || !trans.next_state) break;

        rollout->states[t + 1] = trans.next_state;

        /* Estimate reward */
        float reward = 0.0f;
        for (uint32_t i = 0; i < trans.next_state->dim; i++) {
            reward += trans.next_state->values[i] * 0.01f;
        }
        rollout->rewards[t] = reward;
        total_reward += reward;

        rollout->length = t + 2;
    }

    rollout->total_reward = total_reward;
    wm->stats.rollouts_completed++;

    return NIMCP_SUCCESS;
}


float omni_wm_evaluate_efe(omni_world_model_t* wm,
                            const omni_wm_rollout_t* rollout,
                            const float* preferred_obs,
                            uint32_t obs_dim) {
    if (!wm || !rollout || !preferred_obs) return FLT_MAX;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_evaluate_efe", 0.0f);


    float efe = 0.0f;
    float gamma = wm->config.discount_factor;
    float discount = 1.0f;

    for (uint32_t t = 0; t < rollout->length; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && rollout->length > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(t + 1) / (float)rollout->length);
        }

        if (!rollout->states[t]) continue;

        /* Risk: KL[q(o|pi) || p(o)] - distance from preferred */
        float risk = 0.0f;
        uint32_t min_dim = rollout->states[t]->dim < obs_dim ?
                           rollout->states[t]->dim : obs_dim;

        for (uint32_t i = 0; i < min_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && min_dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)min_dim);
            }

            float diff = rollout->states[t]->values[i] - preferred_obs[i];
            risk += diff * diff;
        }
        risk = sqrtf(risk / min_dim);

        /* Ambiguity: uncertainty in state */
        float ambiguity = rollout->states[t]->uncertainty;

        efe += discount * (risk + 0.5f * ambiguity);
        discount *= gamma;
    }

    return efe;
}


/* ============================================================================
 * Observation Prediction
 * ============================================================================ */

nimcp_error_t omni_wm_predict_observations(omni_world_model_t* wm,
                                            const omni_wm_state_t* state,
                                            float* predicted_obs,
                                            uint32_t obs_dim) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_obse", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");
    NIMCP_CHECK_THROW(predicted_obs, NIMCP_ERROR_INVALID_PARAM, "predicted_obs buffer is NULL");

    /* Use decoder to predict observations from state */
    omni_wm_latent_t latent;
    latent.embedding = state->values;
    latent.dim = state->dim;

    return omni_wm_decode(wm, &latent, predicted_obs, obs_dim);
}


nimcp_error_t omni_wm_infer_state(omni_world_model_t* wm,
                                   const float* observations,
                                   uint32_t obs_dim,
                                   omni_wm_state_t* inferred_state) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_infer_state", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(observations, NIMCP_ERROR_INVALID_PARAM, "observations is NULL");
    NIMCP_CHECK_THROW(inferred_state, NIMCP_ERROR_INVALID_PARAM, "inferred_state is NULL");

    /* Use encoder to infer state from observations */
    omni_wm_latent_t* latent = omni_wm_latent_create(inferred_state->dim);
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_NO_MEMORY, "failed to create latent for state inference");

    nimcp_error_t err = omni_wm_encode(wm, observations, obs_dim, latent);
    if (err == NIMCP_SUCCESS) {
        memcpy(inferred_state->values, latent->embedding,
               inferred_state->dim * sizeof(float));
        inferred_state->uncertainty = 1.0f / (1.0f + latent->information_content);
    }

    omni_wm_latent_destroy(latent);
    return err;
}


/* ============================================================================
 * Active Inference Integration
 * ============================================================================ */

nimcp_error_t omni_wm_connect_active_inference(omni_world_model_t* wm,
                                                struct omni_active_inference* ai) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_connect_acti", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    wm->ai_ctx = ai;
    return NIMCP_SUCCESS;
}


float omni_wm_evaluate_policy(omni_world_model_t* wm,
                               const float* policy_actions,
                               uint32_t horizon,
                               const float* preferred_obs,
                               uint32_t obs_dim) {
    if (!wm || !policy_actions || !preferred_obs) return FLT_MAX;
    if (!wm->current_state) return FLT_MAX;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_evaluate_pol", 0.0f);


    float efe = 0.0f;
    float gamma = wm->config.discount_factor;
    float discount = 1.0f;

    omni_wm_state_t* state = omni_wm_state_clone(wm->current_state);
    if (!state) return FLT_MAX;

    for (uint32_t t = 0; t < horizon; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && horizon > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(t + 1) / (float)horizon);
        }

        /* Get action for this timestep */
        const float* action = policy_actions + t * wm->config.action_dim;

        /* Predict next state */
        omni_wm_transition_t trans;
        memset(&trans, 0, sizeof(trans));

        omni_wm_state_t* old = wm->current_state;
        wm->current_state = state;

        nimcp_error_t err = omni_wm_predict_forward(wm, action,
                                                     wm->config.action_dim, &trans);
        wm->current_state = old;

        if (err != NIMCP_SUCCESS || !trans.next_state) {
            omni_wm_state_destroy(state);
            return FLT_MAX;
        }

        /* Compute EFE components */
        float risk = 0.0f;
        uint32_t min_dim = trans.next_state->dim < obs_dim ?
                           trans.next_state->dim : obs_dim;

        for (uint32_t i = 0; i < min_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && min_dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)min_dim);
            }

            float diff = trans.next_state->values[i] - preferred_obs[i];
            risk += diff * diff;
        }
        risk = sqrtf(risk / min_dim);

        float ambiguity = trans.next_state->uncertainty;

        efe += discount * (risk + 0.5f * ambiguity);
        discount *= gamma;

        omni_wm_state_destroy(state);
        state = trans.next_state;
    }

    omni_wm_state_destroy(state);
    return efe;
}


/* ============================================================================
 * Bio-Async Integration - Connection Management
 * ============================================================================ */

nimcp_error_t omni_wm_connect_bio_async(omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_connect_bio_", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");

    /* Already connected? */
    if (wm->bio_async_connected) {
        NIMCP_LOGGING_DEBUG("Omni world model already connected to bio-async");
        return NIMCP_SUCCESS;
    }

    /* Check if router is available */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_WORLD_MODEL,
        .module_name = "omni_world_model",
        .inbox_capacity = 64,
        .user_data = wm
    };

    wm->bio_ctx = bio_router_register_module(&info);
    if (!wm->bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register omni world model with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal: module can operate without bio-async */
    }

    /* Register message handlers */
    nimcp_error_t result;

    result = bio_router_register_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_PREDICT,
                                          handle_omni_wm_predict);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Failed to register PREDICT handler: %d", result);
    }

    result = bio_router_register_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_COUNTERFACTUAL,
                                          handle_omni_wm_counterfactual);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Failed to register COUNTERFACTUAL handler: %d", result);
    }

    result = bio_router_register_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_UPDATE,
                                          handle_omni_wm_update);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Failed to register UPDATE handler: %d", result);
    }

    result = bio_router_register_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_ROLLOUT,
                                          handle_omni_wm_rollout);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Failed to register ROLLOUT handler: %d", result);
    }

    wm->bio_async_connected = true;
    NIMCP_LOGGING_INFO("Omni world model connected to bio-async with 4 handlers");

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_disconnect_bio_async(omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_disconnect_b", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");

    /* Not connected? */
    if (!wm->bio_async_connected) {
        return NIMCP_SUCCESS;
    }

    /* Unregister handlers */
    if (wm->bio_ctx) {
        bio_router_unregister_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_PREDICT);
        bio_router_unregister_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_COUNTERFACTUAL);
        bio_router_unregister_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_UPDATE);
        bio_router_unregister_handler(wm->bio_ctx, BIO_MSG_OMNI_WM_ROLLOUT);

        /* Unregister module */
        bio_router_unregister_module(wm->bio_ctx);
        wm->bio_ctx = NULL;
    }

    wm->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Omni world model disconnected from bio-async");

    return NIMCP_SUCCESS;
}


uint64_t omni_wm_checkpoint(omni_world_model_t* wm) {
    if (!wm) return 0;

    /* Create checkpoint store if needed */
    if (!wm->checkpoint_store) {
        wm->checkpoint_store = checkpoint_store_create();
        if (!wm->checkpoint_store) return 0;
    }

    /* Check capacity */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_checkpoint", 0.0f);


    if (wm->checkpoint_store->count >= OMNI_WM_MAX_CHECKPOINTS) {
        return 0; /* No room for more checkpoints */
    }

    /* Serialize current state */
    size_t required_size = omni_wm_serialize(wm, NULL, 0);
    if (required_size == 0) return 0;

    uint8_t* data = nimcp_malloc(required_size);
    if (!data) return 0;

    size_t written = omni_wm_serialize(wm, data, required_size);
    if (written == 0) {
        nimcp_free(data);
        data = NULL;
        return 0;
    }

    /* Create checkpoint */
    uint32_t idx = wm->checkpoint_store->count;
    uint64_t id = wm->checkpoint_store->next_id++;

    wm->checkpoint_store->checkpoints[idx].id = id;
    wm->checkpoint_store->checkpoints[idx].data = data;
    wm->checkpoint_store->checkpoints[idx].data_size = written;
    wm->checkpoint_store->checkpoints[idx].timestamp = (double)time(NULL);
    memset(wm->checkpoint_store->checkpoints[idx].description, 0, 64);

    wm->checkpoint_store->count++;

    return id;
}


nimcp_error_t omni_wm_restore_checkpoint(omni_world_model_t* wm,
                                          uint64_t checkpoint_id) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_restore_chec", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(wm->checkpoint_store, NIMCP_ERROR_INVALID_PARAM, "checkpoint store is NULL");
    NIMCP_CHECK_THROW(checkpoint_id != 0, NIMCP_ERROR_INVALID_PARAM, "checkpoint_id is zero");

    /* Find checkpoint */
    omni_wm_checkpoint_t* cp = NULL;
    for (uint32_t i = 0; i < wm->checkpoint_store->count; i++) {
        if (wm->checkpoint_store->checkpoints[i].id == checkpoint_id) {
            cp = &wm->checkpoint_store->checkpoints[i];
            break;
        }
    }

    NIMCP_CHECK_THROW(cp, NIMCP_ERROR_NOT_FOUND, "checkpoint not found");

    /* Deserialize into temporary */
    omni_world_model_t* restored = omni_wm_deserialize(cp->data, cp->data_size);
    NIMCP_CHECK_THROW(restored, NIMCP_ERROR_INVALID_STATE, "failed to deserialize checkpoint");

    /* Preserve checkpoint store and mutex from current wm */
    omni_wm_checkpoint_store_t* store = wm->checkpoint_store;
    nimcp_mutex_t* mutex = wm->mutex;

    /* Swap internals (avoiding checkpoint store and mutex) */
    /* Free current state */
    dynamics_destroy(wm->forward_dynamics);
    dynamics_destroy(wm->backward_dynamics);
    dynamics_destroy(wm->lateral_dynamics);
    nimcp_free(wm->encoder_W);
    nimcp_free(wm->encoder_b);
    nimcp_free(wm->decoder_W);
    nimcp_free(wm->decoder_b);
    replay_buffer_destroy(wm->replay_buffer);
    omni_wm_state_destroy(wm->current_state);
    omni_wm_rssm_state_destroy(wm->rssm_state);

    /* Copy restored state */
    wm->config = restored->config;
    wm->current_state = restored->current_state;
    wm->rssm_state = restored->rssm_state;
    wm->forward_dynamics = restored->forward_dynamics;
    wm->backward_dynamics = restored->backward_dynamics;
    wm->lateral_dynamics = restored->lateral_dynamics;
    wm->encoder_W = restored->encoder_W;
    wm->encoder_b = restored->encoder_b;
    wm->decoder_W = restored->decoder_W;
    wm->decoder_b = restored->decoder_b;
    wm->replay_buffer = restored->replay_buffer;
    wm->stats = restored->stats;
    wm->rand_seed = restored->rand_seed;

    /* Restore preserved items */
    wm->checkpoint_store = store;
    wm->mutex = mutex;

    /* Free the temporary wrapper (but not its contents - now owned by wm) */
    restored->current_state = NULL;
    restored->rssm_state = NULL;
    restored->forward_dynamics = NULL;
    restored->backward_dynamics = NULL;
    restored->lateral_dynamics = NULL;
    restored->encoder_W = NULL;
    restored->encoder_b = NULL;
    restored->decoder_W = NULL;
    restored->decoder_b = NULL;
    restored->replay_buffer = NULL;
    restored->checkpoint_store = NULL;
    restored->mutex = NULL;
    omni_wm_destroy(restored);

    return NIMCP_SUCCESS;
}


nimcp_error_t omni_wm_delete_checkpoint(omni_world_model_t* wm,
                                         uint64_t checkpoint_id) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_delete_check", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(wm->checkpoint_store, NIMCP_ERROR_INVALID_PARAM, "checkpoint store is NULL");
    NIMCP_CHECK_THROW(checkpoint_id != 0, NIMCP_ERROR_INVALID_PARAM, "checkpoint_id is zero");

    /* Find and remove checkpoint */
    for (uint32_t i = 0; i < wm->checkpoint_store->count; i++) {
        if (wm->checkpoint_store->checkpoints[i].id == checkpoint_id) {
            nimcp_free(wm->checkpoint_store->checkpoints[i].data);

            /* Shift remaining checkpoints */
            for (uint32_t j = i; j < wm->checkpoint_store->count - 1; j++) {
                wm->checkpoint_store->checkpoints[j] = wm->checkpoint_store->checkpoints[j + 1];
            }
            wm->checkpoint_store->count--;

            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;
}


nimcp_error_t omni_wm_clear_checkpoints(omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_clear_checkp", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");

    if (wm->checkpoint_store) {
        for (uint32_t i = 0; i < wm->checkpoint_store->count; i++) {
            nimcp_free(wm->checkpoint_store->checkpoints[i].data);
            wm->checkpoint_store->checkpoints[i].data = NULL;
        }
        wm->checkpoint_store->count = 0;
    }

    return NIMCP_SUCCESS;
}
