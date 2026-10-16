#include <math.h>
// nimcp_omni_world_model_part_lifecycle.c - lifecycle functions
// Part of nimcp_omni_world_model.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_omni_world_model.c


static omni_wm_dynamics_t* dynamics_create(uint32_t h_dim, uint32_t z_dim,
                                            uint32_t obs_dim, uint32_t action_dim) {
    omni_wm_dynamics_t* dyn = nimcp_calloc(1, sizeof(omni_wm_dynamics_t));
    if (!dyn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynamics_create: dyn allocation failed");
        return NULL;
    }

    dyn->h_dim = h_dim;
    dyn->z_dim = z_dim;
    dyn->obs_dim = obs_dim;
    dyn->action_dim = action_dim;

    /* Allocate weight matrices */
    uint32_t input_dim = h_dim + z_dim + action_dim;

    dyn->W_h = nimcp_calloc(input_dim * h_dim, sizeof(float));
    dyn->W_z = nimcp_calloc(h_dim * z_dim * 2, sizeof(float)); /* mean + std */
    dyn->W_obs = nimcp_calloc((h_dim + z_dim) * obs_dim, sizeof(float));
    dyn->b_h = nimcp_calloc(h_dim, sizeof(float));
    dyn->b_z = nimcp_calloc(z_dim * 2, sizeof(float));
    dyn->b_obs = nimcp_calloc(obs_dim, sizeof(float));

    if (!dyn->W_h || !dyn->W_z || !dyn->W_obs ||
        !dyn->b_h || !dyn->b_z || !dyn->b_obs) {
        nimcp_free(dyn->W_h);
        nimcp_free(dyn->W_z);
        nimcp_free(dyn->W_obs);
        nimcp_free(dyn->b_h);
        nimcp_free(dyn->b_z);
        nimcp_free(dyn->b_obs);
        nimcp_free(dyn);
        dyn = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynamics_create: weight allocation failed");
        return NULL;
    }

    /* Initialize with small random weights */
    unsigned int seed = (unsigned int)time(NULL);
    for (uint32_t i = 0; i < input_dim * h_dim; i++) {
        dyn->W_h[i] = randn(&seed) * 0.01f;
    }
    for (uint32_t i = 0; i < h_dim * z_dim * 2; i++) {
        dyn->W_z[i] = randn(&seed) * 0.01f;
    }
    for (uint32_t i = 0; i < (h_dim + z_dim) * obs_dim; i++) {
        dyn->W_obs[i] = randn(&seed) * 0.01f;
    }

    return dyn;
}


static void dynamics_destroy(omni_wm_dynamics_t* dyn) {
    if (!dyn) return;
    nimcp_free(dyn->W_h);
    nimcp_free(dyn->W_z);
    nimcp_free(dyn->W_obs);
    nimcp_free(dyn->b_h);
    nimcp_free(dyn->b_z);
    nimcp_free(dyn->b_obs);
    nimcp_free(dyn);
    dyn = NULL;
}


static omni_wm_replay_buffer_t* replay_buffer_create(uint32_t capacity) {
    omni_wm_replay_buffer_t* buf = nimcp_calloc(1, sizeof(omni_wm_replay_buffer_t));
    if (!buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replay_buffer_create: buf is NULL");
        return NULL;
    }

    buf->experiences = nimcp_calloc(capacity, sizeof(omni_wm_experience_t*));
    buf->priorities = nimcp_calloc(capacity, sizeof(float));
    if (!buf->experiences || !buf->priorities) {
        nimcp_free(buf->experiences);
        nimcp_free(buf->priorities);
        nimcp_free(buf);
        buf = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replay_buffer_create: required parameter is NULL (buf->experiences, buf->priorities)");
        return NULL;
    }

    buf->capacity = capacity;
    buf->size = 0;
    buf->head = 0;

    return buf;
}


static void replay_buffer_destroy(omni_wm_replay_buffer_t* buf) {
    if (!buf) return;
    for (uint32_t i = 0; i < buf->size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && buf->size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)buf->size);
        }

        omni_wm_experience_destroy(buf->experiences[i]);
    }
    nimcp_free(buf->experiences);
    nimcp_free(buf->priorities);
    nimcp_free(buf);
    buf = NULL;
}


omni_world_model_t* omni_wm_create(const omni_wm_config_t* config) {
    /* Validate config dimensions if provided */
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_create", 0.0f);


    if (config) {
        if (config->state_dim == 0 || config->action_dim == 0 || config->obs_dim == 0) {
            NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "omni_wm_create: zero dimension in config");
            return NULL;
        }
        if (config->state_dim > OMNI_WM_MAX_STATE_DIM ||
            config->action_dim > OMNI_WM_MAX_ACTION_DIM ||
            config->obs_dim > OMNI_WM_MAX_OBS_DIM) {
            NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                "omni_wm_create: dimension exceeds maximum (state=%u, action=%u, obs=%u)",
                config->state_dim, config->action_dim, config->obs_dim);
            return NULL;
        }
    }

    omni_world_model_t* wm = nimcp_calloc(1, sizeof(omni_world_model_t));
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_create: failed to allocate world model");
        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        memcpy(&wm->config, config, sizeof(omni_wm_config_t));
    } else {
        omni_wm_get_default_config(&wm->config);
    }

    /* Initialize random seed */
    wm->rand_seed = (unsigned int)time(NULL);

    /* Create dynamics models */
    wm->forward_dynamics = dynamics_create(
        wm->config.rssm_h_dim,
        wm->config.rssm_z_dim,
        wm->config.obs_dim,
        wm->config.action_dim
    );
    if (!wm->forward_dynamics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_create: failed to create forward dynamics");
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }

    wm->backward_dynamics = dynamics_create(
        wm->config.rssm_h_dim,
        wm->config.rssm_z_dim,
        wm->config.obs_dim,
        wm->config.action_dim
    );
    if (!wm->backward_dynamics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_create: failed to create backward dynamics");
        dynamics_destroy(wm->forward_dynamics);
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }

    if (wm->config.enable_lateral) {
        wm->lateral_dynamics = dynamics_create(
            wm->config.rssm_h_dim,
            wm->config.rssm_z_dim,
            wm->config.obs_dim,
            wm->config.action_dim
        );
    }

    /* Create encoder/decoder weights */
    uint32_t enc_size = wm->config.obs_dim * wm->config.latent_dim;
    wm->encoder_W = nimcp_calloc(enc_size, sizeof(float));
    wm->encoder_b = nimcp_calloc(wm->config.latent_dim, sizeof(float));
    wm->decoder_W = nimcp_calloc(enc_size, sizeof(float));
    wm->decoder_b = nimcp_calloc(wm->config.obs_dim, sizeof(float));

    if (!wm->encoder_W || !wm->encoder_b || !wm->decoder_W || !wm->decoder_b) {
        omni_wm_destroy(wm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_create: required parameter is NULL (wm->encoder_W, wm->encoder_b, wm->decoder_W, wm->decoder_b)");
        return NULL;
    }

    /* Initialize encoder/decoder */
    for (uint32_t i = 0; i < enc_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && enc_size > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)enc_size);
        }

        wm->encoder_W[i] = randn(&wm->rand_seed) * 0.01f;
        wm->decoder_W[i] = randn(&wm->rand_seed) * 0.01f;
    }

    /* Create replay buffer */
    wm->replay_buffer = replay_buffer_create(wm->config.replay_buffer_size);
    if (!wm->replay_buffer) {
        omni_wm_destroy(wm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_create: wm->replay_buffer is NULL");
        return NULL;
    }

    /* Create initial RSSM state */
    wm->rssm_state = omni_wm_rssm_state_create(
        wm->config.rssm_h_dim,
        wm->config.rssm_z_dim
    );

    /* Create simple state wrapper */
    wm->current_state = omni_wm_state_create(wm->config.state_dim);

    /* Create mutex */
    wm->mutex = nimcp_mutex_create(NULL);
    wm->initialized = true;

    return wm;
}


omni_world_model_t* omni_wm_create_simple(uint32_t state_dim,
                                           uint32_t action_dim,
                                           uint32_t obs_dim) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_create_simpl", 0.0f);


    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.state_dim = state_dim;
    config.action_dim = action_dim;
    config.obs_dim = obs_dim;
    config.rssm_h_dim = state_dim / 2 > 0 ? state_dim / 2 : 16;
    config.rssm_z_dim = state_dim / 4 > 0 ? state_dim / 4 : 8;
    config.latent_dim = state_dim;
    return omni_wm_create(&config);
}


void omni_wm_destroy(omni_world_model_t* wm) {
    if (!wm) return;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_destroy", 0.0f);


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

    /* Clean up checkpoint store */
    checkpoint_store_destroy_internal(wm->checkpoint_store);

    if (wm->mutex) {
        nimcp_mutex_destroy(wm->mutex);
    }

    nimcp_free(wm);
    wm = NULL;
}


/* ============================================================================
 * State Management
 * ============================================================================ */

omni_wm_state_t* omni_wm_state_create(uint32_t dim) {
    if (dim == 0 || dim > OMNI_WM_MAX_STATE_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_create: dim is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_state_create", 0.0f);


    omni_wm_state_t* state = nimcp_calloc(1, sizeof(omni_wm_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_create: state is NULL");
        return NULL;
    }

    state->values = nimcp_calloc(dim, sizeof(float));
    if (!state->values) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_create: state->values is NULL");
        return NULL;
    }

    state->dim = dim;
    state->uncertainty = 1.0f;
    state->timestamp = 0.0;
    state->level = 0;

    return state;
}


void omni_wm_state_destroy(omni_wm_state_t* state) {
    if (!state) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_state_destro", 0.0f);


    nimcp_free(state->values);
    nimcp_free(state);
    state = NULL;
}


/* ============================================================================
 * RSSM State Management
 * ============================================================================ */

omni_wm_rssm_state_t* omni_wm_rssm_state_create(uint32_t h_dim, uint32_t z_dim) {
    if (h_dim == 0 || z_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_create: h_dim is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_c", 0.0f);


    omni_wm_rssm_state_t* state = nimcp_calloc(1, sizeof(omni_wm_rssm_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_create: state is NULL");
        return NULL;
    }

    state->h = nimcp_calloc(h_dim, sizeof(float));
    state->z = nimcp_calloc(z_dim, sizeof(float));
    state->z_mean = nimcp_calloc(z_dim, sizeof(float));
    state->z_std = nimcp_calloc(z_dim, sizeof(float));

    if (!state->h || !state->z || !state->z_mean || !state->z_std) {
        omni_wm_rssm_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_create: required parameter is NULL (state->h, state->z, state->z_mean, state->z_std)");
        return NULL;
    }

    state->h_dim = h_dim;
    state->z_dim = z_dim;

    /* Initialize std to 1 */
    for (uint32_t i = 0; i < z_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)z_dim);
        }

        state->z_std[i] = 1.0f;
    }

    return state;
}


void omni_wm_rssm_state_destroy(omni_wm_rssm_state_t* state) {
    if (!state) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_d", 0.0f);


    nimcp_free(state->h);
    nimcp_free(state->z);
    nimcp_free(state->z_mean);
    nimcp_free(state->z_std);
    nimcp_free(state);
    state = NULL;
}


/* ============================================================================
 * Latent Encoding (JEPA-inspired)
 * ============================================================================ */

omni_wm_latent_t* omni_wm_latent_create(uint32_t dim) {
    if (dim == 0 || dim > OMNI_WM_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_latent_create: dim is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_latent_creat", 0.0f);


    omni_wm_latent_t* latent = nimcp_calloc(1, sizeof(omni_wm_latent_t));
    if (!latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_latent_create: latent is NULL");
        return NULL;
    }

    latent->embedding = nimcp_calloc(dim, sizeof(float));
    if (!latent->embedding) {
        nimcp_free(latent);
        latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_latent_create: latent->embedding is NULL");
        return NULL;
    }

    latent->dim = dim;
    return latent;
}


void omni_wm_latent_destroy(omni_wm_latent_t* latent) {
    if (!latent) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_latent_destr", 0.0f);


    nimcp_free(latent->embedding);
    nimcp_free(latent);
    latent = NULL;
}


/* ============================================================================
 * MDN (Mixture Density Network)
 * ============================================================================ */

omni_wm_mdn_prediction_t* omni_wm_mdn_create(uint32_t num_components, uint32_t dim) {
    if (num_components == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_mdn_create: num_components is zero");
        return NULL;
    }
    if (num_components > OMNI_WM_MAX_MDN_COMPONENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_mdn_create: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_mdn_create", 0.0f);


    omni_wm_mdn_prediction_t* pred = nimcp_calloc(1, sizeof(omni_wm_mdn_prediction_t));
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_mdn_create: pred is NULL");
        return NULL;
    }

    pred->components = nimcp_calloc(num_components, sizeof(omni_wm_mdn_component_t));
    if (!pred->components) {
        nimcp_free(pred);
        pred = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_mdn_create: pred->components is NULL");
        return NULL;
    }

    for (uint32_t k = 0; k < num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)num_components);
        }

        pred->components[k].mean = nimcp_calloc(dim, sizeof(float));
        pred->components[k].std = nimcp_calloc(dim, sizeof(float));
        pred->components[k].dim = dim;
        pred->components[k].weight = 1.0f / (fabsf(num_components) > 1e-7f ? num_components : 1e-7f);

        if (!pred->components[k].mean || !pred->components[k].std) {
            omni_wm_mdn_destroy(pred);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_mdn_create: required parameter is NULL (pred->components, pred->components)");
            return NULL;
        }

        /* Initialize std to 1 */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                omni_world_model_heartbeat("omni_world_m_loop",
                                 (float)(i + 1) / (float)dim);
            }

            pred->components[k].std[i] = 1.0f;
        }
    }

    pred->num_components = num_components;
    pred->dim = dim;

    return pred;
}


void omni_wm_mdn_destroy(omni_wm_mdn_prediction_t* pred) {
    if (!pred) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_mdn_destroy", 0.0f);


    for (uint32_t k = 0; k < pred->num_components; k++) {
        /* Phase 8: Loop progress heartbeat */
        if ((k & 0xFF) == 0 && pred->num_components > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(k + 1) / (float)pred->num_components);
        }

        nimcp_free(pred->components[k].mean);
        nimcp_free(pred->components[k].std);
    }
    nimcp_free(pred->components);
    nimcp_free(pred);
    pred = NULL;
}


/* ============================================================================
 * Experience Replay
 * ============================================================================ */

omni_wm_experience_t* omni_wm_experience_create(uint32_t state_dim,
                                                  uint32_t action_dim,
                                                  uint32_t obs_dim) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_experience_c", 0.0f);


    if (state_dim == 0 || action_dim == 0 || obs_dim == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "omni_wm_experience_create: zero dimension");
        return NULL;
    }

    omni_wm_experience_t* exp = nimcp_calloc(1, sizeof(omni_wm_experience_t));
    if (!exp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_experience_create: allocation failed");
        return NULL;
    }

    exp->state = omni_wm_rssm_state_create(state_dim / 2, state_dim / 4);
    exp->next_state = omni_wm_rssm_state_create(state_dim / 2, state_dim / 4);
    exp->action = nimcp_calloc(action_dim, sizeof(float));
    exp->observation = nimcp_calloc(obs_dim, sizeof(float));

    if (!exp->state || !exp->next_state || !exp->action || !exp->observation) {
        omni_wm_experience_destroy(exp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: required parameter is NULL (exp->state, exp->next_state, exp->action, exp->observation)");
        return NULL;
    }

    exp->action_dim = action_dim;
    exp->obs_dim = obs_dim;

    return exp;
}


void omni_wm_experience_destroy(omni_wm_experience_t* exp) {
    if (!exp) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_experience_d", 0.0f);


    omni_wm_rssm_state_destroy(exp->state);
    omni_wm_rssm_state_destroy(exp->next_state);
    nimcp_free(exp->action);
    nimcp_free(exp->observation);
    nimcp_free(exp);
    exp = NULL;
}


/* ============================================================================
 * Counterfactual Reasoning
 * ============================================================================ */

omni_wm_counterfactual_query_t* omni_wm_cf_query_create(
    omni_wm_counterfactual_type_t type,
    const omni_wm_state_t* initial_state,
    const float* hypothetical_action,
    uint32_t action_dim,
    uint32_t horizon) {

    if (!initial_state || !hypothetical_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_set_learning_rate: required parameter is NULL (initial_state, hypothetical_action)");
        return NULL;
    }
    if (horizon == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_set_learning_rate: horizon is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_cf_query_cre", 0.0f);


    omni_wm_counterfactual_query_t* query = nimcp_calloc(1, sizeof(omni_wm_counterfactual_query_t));
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_set_learning_rate: query is NULL");
        return NULL;
    }

    query->type = type;
    query->initial_state = omni_wm_state_clone(initial_state);
    query->hypothetical_action = nimcp_calloc(action_dim, sizeof(float));

    if (!query->initial_state || !query->hypothetical_action) {
        omni_wm_cf_query_destroy(query);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_set_learning_rate: required parameter is NULL (query->initial_state, query->hypothetical_action)");
        return NULL;
    }

    memcpy(query->hypothetical_action, hypothetical_action, action_dim * sizeof(float));
    query->action_dim = action_dim;
    query->horizon = horizon;

    return query;
}


void omni_wm_cf_query_destroy(omni_wm_counterfactual_query_t* query) {
    if (!query) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_cf_query_des", 0.0f);


    omni_wm_state_destroy(query->initial_state);
    nimcp_free(query->hypothetical_action);
    nimcp_free(query->context);
    nimcp_free(query);
    query = NULL;
}


void omni_wm_cf_result_destroy(omni_wm_counterfactual_result_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_cf_result_de", 0.0f);


    for (uint32_t i = 0; i < result->trajectory_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && result->trajectory_len > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)result->trajectory_len);
        }

        omni_wm_state_destroy(result->trajectory[i]);
    }
    nimcp_free(result->trajectory);
    nimcp_free(result->predicted_obs);
}


/* ============================================================================
 * Policy Rollouts
 * ============================================================================ */

omni_wm_rollout_t* omni_wm_rollout_create(uint32_t max_length,
                                           uint32_t state_dim,
                                           uint32_t action_dim,
                                           uint32_t obs_dim) {
    if (max_length == 0 || max_length > OMNI_WM_MAX_HORIZON) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_cf_result_destroy: max_length is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rollout_crea", 0.0f);


    omni_wm_rollout_t* rollout = nimcp_calloc(1, sizeof(omni_wm_rollout_t));
    if (!rollout) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_cf_result_destroy: rollout is NULL");
        return NULL;
    }

    rollout->states = nimcp_calloc(max_length, sizeof(omni_wm_state_t*));
    rollout->actions = nimcp_calloc(max_length, sizeof(float*));
    rollout->observations = nimcp_calloc(max_length, sizeof(float*));
    rollout->rewards = nimcp_calloc(max_length, sizeof(float));

    if (!rollout->states || !rollout->actions ||
        !rollout->observations || !rollout->rewards) {
        omni_wm_rollout_destroy(rollout);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_cf_result_destroy: operation failed");
        return NULL;
    }

    return rollout;
}


void omni_wm_rollout_destroy(omni_wm_rollout_t* rollout) {
    if (!rollout) return;

    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_rollout_dest", 0.0f);


    for (uint32_t i = 0; i < rollout->length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && rollout->length > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)rollout->length);
        }

        omni_wm_state_destroy(rollout->states[i]);
        nimcp_free(rollout->actions[i]);
        nimcp_free(rollout->observations[i]);
    }

    nimcp_free(rollout->states);
    nimcp_free(rollout->actions);
    nimcp_free(rollout->observations);
    nimcp_free(rollout->rewards);
    nimcp_free(rollout);
    rollout = NULL;
}


nimcp_error_t omni_wm_reset_stats(omni_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    omni_world_model_heartbeat("omni_world_m_omni_wm_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    memset(&wm->stats, 0, sizeof(omni_wm_stats_t));
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Checkpoint Management
 * ============================================================================ */

/**
 * @brief Initialize checkpoint store
 */
static omni_wm_checkpoint_store_t* checkpoint_store_create(void) {
    omni_wm_checkpoint_store_t* store = nimcp_calloc(1, sizeof(omni_wm_checkpoint_store_t));
    if (!store) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "checkpoint_store_create: store is NULL");
        return NULL;
    }
    store->next_id = 1;
    return store;
}


/**
 * @brief Destroy checkpoint store - used in cleanup
 */
static void checkpoint_store_destroy_internal(omni_wm_checkpoint_store_t* store) {
    if (!store) return;

    for (uint32_t i = 0; i < store->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && store->count > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)store->count);
        }

        nimcp_free(store->checkpoints[i].data);
    }
    nimcp_free(store);
    store = NULL;
}
