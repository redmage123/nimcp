/**
 * @file nimcp_omni_wm_state.c
 * @brief State and RSSM State Lifecycle Management
 * @version 1.0.0
 * @date 2026-02-16
 *
 * WHAT: Manages creation, destruction, and cloning of world model states,
 *       RSSM states, and latent representations
 * WHY:  Single Responsibility - State lifecycle management separated from
 *       dynamics, replay, and serialization concerns
 * HOW:  Implements state allocation/deallocation, RSSM state handling with
 *       deterministic (h) and stochastic (z) components, and latent encoding
 */

#include "cognitive/omni/nimcp_omni_wm_internal.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

/* External mesh/heartbeat declarations */
extern nimcp_health_agent_t* g_omni_world_model_health_agent;
extern nimcp_health_agent_t* g_omni_world_model_instance_health_agent;
extern void omni_world_model_heartbeat(const char* op, float progress);

/* ============================================================================
 * State Lifecycle
 * ============================================================================ */

omni_wm_state_t* omni_wm_state_create(uint32_t dim) {
    if (dim == 0 || dim > OMNI_WM_MAX_STATE_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_state_create: invalid dimension");
        return NULL;
    }

    omni_world_model_heartbeat("omni_world_m_omni_wm_state_create", 0.0f);

    omni_wm_state_t* state = nimcp_calloc(1, sizeof(omni_wm_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_create: state allocation failed");
        return NULL;
    }

    state->values = nimcp_calloc(dim, sizeof(float));
    if (!state->values) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_create: values allocation failed");
        return NULL;
    }

    state->dim = dim;
    state->uncertainty = 1.0f;
    state->timestamp = 0.0;
    state->level = 0;

    return state;
}

omni_wm_state_t* omni_wm_state_from_values(const float* values, uint32_t dim) {
    if (!values || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_state_from_values: invalid arguments");
        return NULL;
    }

    omni_world_model_heartbeat("omni_world_m_omni_wm_state_from_v", 0.0f);

    omni_wm_state_t* state = omni_wm_state_create(dim);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_from_values: creation failed");
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

    omni_world_model_heartbeat("omni_world_m_omni_wm_state_clone", 0.0f);

    omni_wm_state_t* clone = omni_wm_state_create(state->dim);
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_state_clone: creation failed");
        return NULL;
    }

    memcpy(clone->values, state->values, state->dim * sizeof(float));
    clone->uncertainty = state->uncertainty;
    clone->timestamp = state->timestamp;
    clone->level = state->level;

    return clone;
}

void omni_wm_state_destroy(omni_wm_state_t* state) {
    if (!state) return;
    omni_world_model_heartbeat("omni_world_m_omni_wm_state_destro", 0.0f);

    nimcp_free(state->values);
    nimcp_free(state);
    state = NULL;
}

nimcp_error_t omni_wm_set_state(omni_world_model_t* wm,
                                 const omni_wm_state_t* state) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_set_state", 0.0f);

    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");

    if (wm->current_state) {
        omni_wm_state_destroy(wm->current_state);
    }
    wm->current_state = omni_wm_state_clone(state);

    return wm->current_state ? NIMCP_SUCCESS : NIMCP_ERROR_NO_MEMORY;
}

const omni_wm_state_t* omni_wm_get_state(const omni_world_model_t* wm) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_state", 0.0f);

    return wm ? wm->current_state : NULL;
}

/* ============================================================================
 * RSSM State Lifecycle
 * ============================================================================ */

omni_wm_rssm_state_t* omni_wm_rssm_state_create(uint32_t h_dim, uint32_t z_dim) {
    if (h_dim == 0 || z_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_rssm_state_create: invalid dimensions");
        return NULL;
    }

    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_c", 0.0f);

    omni_wm_rssm_state_t* state = nimcp_calloc(1, sizeof(omni_wm_rssm_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_create: state allocation failed");
        return NULL;
    }

    state->h = nimcp_calloc(h_dim, sizeof(float));
    if (!state->h) return NULL;
    state->z = nimcp_calloc(z_dim, sizeof(float));
    if (!state->z) return NULL;
    state->z_mean = nimcp_calloc(z_dim, sizeof(float));
    if (!state->z_mean) return NULL;
    state->z_std = nimcp_calloc(z_dim, sizeof(float));

    if (!state->h || !state->z || !state->z_mean || !state->z_std) {
        omni_wm_rssm_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_create: allocation failed");
        return NULL;
    }

    state->h_dim = h_dim;
    state->z_dim = z_dim;

    /* Initialize std to 1 */
    for (uint32_t i = 0; i < z_dim; i++) {
        if ((i & 0xFF) == 0 && z_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop", (float)(i + 1) / (float)z_dim);
        }
        state->z_std[i] = 1.0f;
    }

    return state;
}

omni_wm_rssm_state_t* omni_wm_rssm_state_clone(const omni_wm_rssm_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_wm_rssm_state_clone: state is NULL");
        return NULL;
    }

    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_c", 0.0f);

    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_create(state->h_dim, state->z_dim);
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_rssm_state_clone: creation failed");
        return NULL;
    }

    memcpy(clone->h, state->h, state->h_dim * sizeof(float));
    memcpy(clone->z, state->z, state->z_dim * sizeof(float));
    memcpy(clone->z_mean, state->z_mean, state->z_dim * sizeof(float));
    memcpy(clone->z_std, state->z_std, state->z_dim * sizeof(float));
    clone->timestamp = state->timestamp;

    return clone;
}

void omni_wm_rssm_state_destroy(omni_wm_rssm_state_t* state) {
    if (!state) return;
    omni_world_model_heartbeat("omni_world_m_omni_wm_rssm_state_d", 0.0f);

    nimcp_free(state->h);
    nimcp_free(state->z);
    nimcp_free(state->z_mean);
    nimcp_free(state->z_std);
    nimcp_free(state);
    state = NULL;
}

const omni_wm_rssm_state_t* omni_wm_get_rssm_state(const omni_world_model_t* wm) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_get_rssm_sta", 0.0f);

    return wm ? wm->rssm_state : NULL;
}

nimcp_error_t omni_wm_set_rssm_state(omni_world_model_t* wm,
                                      const omni_wm_rssm_state_t* state) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_set_rssm_sta", 0.0f);

    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "RSSM state is NULL");

    if (wm->rssm_state) {
        omni_wm_rssm_state_destroy(wm->rssm_state);
    }
    wm->rssm_state = omni_wm_rssm_state_clone(state);

    return wm->rssm_state ? NIMCP_SUCCESS : NIMCP_ERROR_NO_MEMORY;
}

/* ============================================================================
 * Latent Representation Lifecycle
 * ============================================================================ */

omni_wm_latent_t* omni_wm_latent_create(uint32_t dim) {
    if (dim == 0 || dim > OMNI_WM_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wm_latent_create: invalid dimension");
        return NULL;
    }

    omni_world_model_heartbeat("omni_world_m_omni_wm_latent_creat", 0.0f);

    omni_wm_latent_t* latent = nimcp_calloc(1, sizeof(omni_wm_latent_t));
    if (!latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_latent_create: latent allocation failed");
        return NULL;
    }

    latent->embedding = nimcp_calloc(dim, sizeof(float));
    if (!latent->embedding) {
        nimcp_free(latent);
        latent = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_latent_create: embedding allocation failed");
        return NULL;
    }

    latent->dim = dim;
    return latent;
}

void omni_wm_latent_destroy(omni_wm_latent_t* latent) {
    if (!latent) return;
    omni_world_model_heartbeat("omni_world_m_omni_wm_latent_destr", 0.0f);

    nimcp_free(latent->embedding);
    nimcp_free(latent);
    latent = NULL;
}

nimcp_error_t omni_wm_encode(omni_world_model_t* wm,
                              const float* observation,
                              uint32_t obs_dim,
                              omni_wm_latent_t* latent) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_encode", 0.0f);

    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(observation, NIMCP_ERROR_INVALID_PARAM, "observation is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");

    /* Linear encoding: latent = ReLU(W * obs + b) */
    for (uint32_t i = 0; i < latent->dim; i++) {
        if ((i & 0xFF) == 0 && latent->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop", (float)(i + 1) / (float)latent->dim);
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
        if ((i & 0xFF) == 0 && latent->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop", (float)(i + 1) / (float)latent->dim);
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
    omni_world_model_heartbeat("omni_world_m_omni_wm_decode", 0.0f);

    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");
    NIMCP_CHECK_THROW(observation, NIMCP_ERROR_INVALID_PARAM, "observation is NULL");

    /* Linear decoding: obs = W * latent + b */
    for (uint32_t i = 0; i < obs_dim && i < wm->config.obs_dim; i++) {
        if ((i & 0xFF) == 0 && obs_dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop", (float)(i + 1) / (float)obs_dim);
        }

        float sum = wm->decoder_b[i];
        for (uint32_t j = 0; j < latent->dim && j < wm->config.latent_dim; j++) {
            sum += wm->decoder_W[i * wm->config.latent_dim + j] * latent->embedding[j];
        }
        observation[i] = sum;
    }

    /* Compute reconstruction error */
    /* Note: Would need original observation to compute actual error */
    /* For now, just return success */

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_predict_latent(omni_world_model_t* wm,
                                      const omni_wm_latent_t* latent,
                                      const float* action,
                                      uint32_t action_dim,
                                      omni_wm_latent_t* predicted_latent) {
    omni_world_model_heartbeat("omni_world_m_omni_wm_predict_lat", 0.0f);

    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID_PARAM, "world model is NULL");
    NIMCP_CHECK_THROW(latent, NIMCP_ERROR_INVALID_PARAM, "latent is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(predicted_latent, NIMCP_ERROR_INVALID_PARAM, "predicted_latent is NULL");

    /* Simple latent prediction: add action influence */
    for (uint32_t i = 0; i < predicted_latent->dim && i < latent->dim; i++) {
        if ((i & 0xFF) == 0 && predicted_latent->dim > 256) {
            omni_world_model_heartbeat("omni_world_m_loop",
                             (float)(i + 1) / (float)predicted_latent->dim);
        }

        predicted_latent->embedding[i] = latent->embedding[i];
        /* Add action influence (simplified) */
        if (i < action_dim) {
            predicted_latent->embedding[i] += 0.1f * action[i];
        }
    }

    predicted_latent->information_content = latent->information_content;
    return NIMCP_SUCCESS;
}
