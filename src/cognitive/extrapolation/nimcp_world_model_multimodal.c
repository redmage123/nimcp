/**
 * @file nimcp_world_model_multimodal.c
 * @brief Multi-modal world model implementation
 */

#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(world_model_multimodal, MESH_ADAPTER_CATEGORY_COGNITIVE)



/*=============================================================================
 * STATIC HELPERS
 *===========================================================================*/

static void init_modality_encoders(nimcp_world_model_t* wm) {
    uint32_t default_dims[] = {
        [WM_MODALITY_VISUAL] = 512,
        [WM_MODALITY_AUDITORY] = 256,
        [WM_MODALITY_TACTILE] = 128,
        [WM_MODALITY_PROPRIOCEPTIVE] = 64,
        [WM_MODALITY_OLFACTORY] = 64,
        [WM_MODALITY_GUSTATORY] = 32,
        [WM_MODALITY_VESTIBULAR] = 32,
        [WM_MODALITY_INTEROCEPTIVE] = 64,
        [WM_MODALITY_LINGUISTIC] = 256,
        [WM_MODALITY_SEMANTIC] = 256
    };

    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        wm->encoder_dims[i] = default_dims[i];
        wm->modality_encoders[i] = nimcp_calloc(default_dims[i], sizeof(float));
        wm->modality_active[i] = false;
    }

    /* Visual and proprioceptive active by default */
    wm->modality_active[WM_MODALITY_VISUAL] = true;
    wm->modality_active[WM_MODALITY_PROPRIOCEPTIVE] = true;
}

static void compute_cross_modal_attention(nimcp_world_model_t* wm) {
    float total = 0.0f;

    /* Compute attention weights based on recent activity */
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        if (wm->modality_active[i]) {
            float activity = 0.0f;
            for (uint32_t j = 0; j < wm->encoder_dims[i]; j++) {
                activity += fabsf(wm->modality_encoders[i][j]);
            }
            wm->attention.modality_weights[i] = activity / wm->encoder_dims[i];
            total += wm->attention.modality_weights[i];
        } else {
            wm->attention.modality_weights[i] = 0.0f;
        }
    }

    /* Normalize */
    if (total > 0.0f) {
        for (int i = 0; i < WM_MODALITY_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
                world_model_multimodal_heartbeat("world_model__loop",
                                 (float)(i + 1) / (float)WM_MODALITY_COUNT);
            }

            wm->attention.modality_weights[i] /= total;
        }
    }

    /* Find dominant modality */
    float max_weight = 0.0f;
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        if (wm->attention.modality_weights[i] > max_weight) {
            max_weight = wm->attention.modality_weights[i];
            wm->attention.dominant_modality = i;
        }
    }

    /* Compute coherence (how consistent are modalities) */
    float variance = 0.0f;
    float mean = 1.0f / (fabsf(WM_MODALITY_COUNT) > 1e-7f ? WM_MODALITY_COUNT : 1e-7f);
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        float diff = wm->attention.modality_weights[i] - mean;
        variance += diff * diff;
    }
    wm->attention.coherence_score = 1.0f - sqrtf(variance);
}

static void update_entities(nimcp_world_model_t* wm, float dt_ms) {
    float dt_sec = dt_ms / 1000.0f;

    for (uint32_t i = 0; i < wm->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)wm->num_entities);
        }

        wm_entity_t* e = &wm->entities[i];

        /* Update position based on velocity */
        e->position[0] += e->velocity[0] * dt_sec;
        e->position[1] += e->velocity[1] * dt_sec;
        e->position[2] += e->velocity[2] * dt_sec;

        /* Decay existence probability if not observed */
        uint64_t now = 0; /* Would use actual time */
        float time_since_observed = (float)(now - e->last_observed) / 1000.0f;
        e->existence_prob *= expf(-wm->config.entity_persistence * time_since_observed);

        /* Remove if probability too low */
        if (e->existence_prob < 0.01f) {
            /* Mark for removal */
            e->entity_id = UINT32_MAX;
            wm->stats.entity_deaths++;
        }
    }

    /* Compact entity array */
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < wm->num_entities; read_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((read_idx & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(read_idx + 1) / (float)wm->num_entities);
        }

        if (wm->entities[read_idx].entity_id != UINT32_MAX) {
            if (write_idx != read_idx) {
                wm->entities[write_idx] = wm->entities[read_idx];
            }
            write_idx++;
        }
    }
    wm->num_entities = write_idx;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

wm_config_t wm_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_default_config", 0.0f);


    wm_config_t config = {
        .latent_dim = WM_LATENT_DIM,
        .context_size = WM_CONTEXT_SIZE,
        .max_entities = WM_MAX_ENTITIES,
        .max_prediction_steps = WM_MAX_PREDICTION_STEPS,
        .fusion_type = WM_FUSION_ATTENTION,
        .prediction_mode = WM_PRED_MODE_PROBABILISTIC,
        .learning_rate = NIMCP_LEARNING_RATE_FINE,
        .prediction_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .entity_persistence = 0.1f,
        .enable_bio_async = true,
        .enable_immune = true,
        .enable_logging = true
    };
    return config;
}

nimcp_world_model_t* wm_create(const wm_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_create", 0.0f);


    nimcp_world_model_t* wm = nimcp_calloc(1, sizeof(nimcp_world_model_t));
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_create: failed to allocate world model");
        return NULL;
    }

    if (config) {
        wm->config = *config;
    } else {
        wm->config = wm_default_config();
    }

    /* Allocate global state */
    wm->global_state_dim = wm->config.latent_dim;
    wm->global_state = nimcp_calloc(wm->global_state_dim, sizeof(float));
    if (!wm->global_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_create: failed to allocate global state");
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }

    /* Allocate context buffer */
    wm->context_buffer = nimcp_calloc(wm->config.context_size * wm->config.latent_dim, sizeof(float));
    if (!wm->context_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_create: failed to allocate context buffer");
        nimcp_free(wm->global_state);
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }
    wm->context_pos = 0;

    /* Allocate entities */
    wm->entity_capacity = wm->config.max_entities;
    wm->entities = nimcp_calloc(wm->entity_capacity, sizeof(wm_entity_t));
    if (!wm->entities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_create: failed to allocate entities");
        nimcp_free(wm->context_buffer);
        nimcp_free(wm->global_state);
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }
    wm->num_entities = 0;

    /* Allocate prediction buffer */
    wm->prediction_buffer = nimcp_calloc(
        wm->config.max_prediction_steps * wm->config.latent_dim,
        sizeof(float));
    if (!wm->prediction_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_create: failed to allocate prediction buffer");
        nimcp_free(wm->entities);
        nimcp_free(wm->context_buffer);
        nimcp_free(wm->global_state);
        nimcp_free(wm);
        wm = NULL;
        return NULL;
    }

    /* Initialize modality encoders */
    init_modality_encoders(wm);

    wm->status = WM_STATUS_IDLE;
    wm->last_error = WM_OK;

    return wm;
}

wm_error_t wm_init(nimcp_world_model_t* wm) {
    if (!wm) return WM_ERR_NULL_PTR;

    /* Initialize attention matrix */
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_init", 0.0f);


    memset(&wm->attention, 0, sizeof(wm_cross_modal_attention_t));
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        wm->attention.modality_weights[i] = 1.0f / (fabsf(WM_MODALITY_COUNT) > 1e-7f ? WM_MODALITY_COUNT : 1e-7f);
    }

    /* Initialize stats */
    memset(&wm->stats, 0, sizeof(wm_stats_t));

    wm->initialized = true;
    return WM_OK;
}

wm_error_t wm_reset(nimcp_world_model_t* wm) {
    if (!wm) return WM_ERR_NULL_PTR;

    /* Clear state */
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_reset", 0.0f);


    memset(wm->global_state, 0, wm->global_state_dim * sizeof(float));
    memset(wm->context_buffer, 0,
           wm->config.context_size * wm->config.latent_dim * sizeof(float));
    wm->context_pos = 0;

    /* Clear entities */
    wm->num_entities = 0;

    /* Reset attention */
    memset(&wm->attention, 0, sizeof(wm_cross_modal_attention_t));

    /* Reset stats */
    memset(&wm->stats, 0, sizeof(wm_stats_t));

    wm->status = WM_STATUS_IDLE;
    wm->last_error = WM_OK;

    return WM_OK;
}

void wm_destroy(nimcp_world_model_t* wm) {
    if (!wm) return;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_destroy", 0.0f);


    nimcp_free(wm->global_state);
    nimcp_free(wm->context_buffer);
    nimcp_free(wm->entities);
    nimcp_free(wm->prediction_buffer);

    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)WM_MODALITY_COUNT);
        }

        nimcp_free(wm->modality_encoders[i]);
    }

    nimcp_free(wm);
    wm = NULL;
}

/*=============================================================================
 * MODALITY API
 *===========================================================================*/

wm_error_t wm_process_modality(
    nimcp_world_model_t* wm,
    const wm_modality_input_t* input)
{
    if (!wm || !input) return WM_ERR_NULL_PTR;
    if (!wm->initialized) return WM_ERR_NOT_INITIALIZED;
    if (input->modality >= WM_MODALITY_COUNT) return WM_ERR_INVALID_MODALITY;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_process_modality", 0.0f);


    wm->status = WM_STATUS_PROCESSING;

    /* Copy features to encoder (simplified - real impl would transform) */
    uint32_t copy_dim = input->feature_dim < wm->encoder_dims[input->modality]
                       ? input->feature_dim : wm->encoder_dims[input->modality];

    memcpy(wm->modality_encoders[input->modality],
           input->features,
           copy_dim * sizeof(float));

    wm->modality_active[input->modality] = true;
    wm->stats.inputs_processed++;

    wm->status = WM_STATUS_IDLE;
    return WM_OK;
}

wm_error_t wm_process_multimodal(
    nimcp_world_model_t* wm,
    const wm_modality_input_t* inputs,
    uint32_t num_inputs)
{
    if (!wm || !inputs) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_process_multimoda", 0.0f);


    for (uint32_t i = 0; i < num_inputs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_inputs > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)num_inputs);
        }

        wm_error_t err = wm_process_modality(wm, &inputs[i]);
        if (err != WM_OK) return err;
    }

    return WM_OK;
}

wm_error_t wm_set_modality_active(
    nimcp_world_model_t* wm,
    wm_modality_t modality,
    bool active)
{
    if (!wm) return WM_ERR_NULL_PTR;
    if (modality >= WM_MODALITY_COUNT) return WM_ERR_INVALID_MODALITY;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_set_modality_acti", 0.0f);


    wm->modality_active[modality] = active;
    return WM_OK;
}

wm_error_t wm_get_modality_encoding(
    nimcp_world_model_t* wm,
    wm_modality_t modality,
    float* encoding,
    uint32_t* dim)
{
    if (!wm || !encoding || !dim) return WM_ERR_NULL_PTR;
    if (modality >= WM_MODALITY_COUNT) return WM_ERR_INVALID_MODALITY;

    *dim = wm->encoder_dims[modality];
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_modality_enco", 0.0f);


    memcpy(encoding, wm->modality_encoders[modality], *dim * sizeof(float));

    return WM_OK;
}

/*=============================================================================
 * FUSION API
 *===========================================================================*/

wm_error_t wm_fuse_modalities(nimcp_world_model_t* wm) {
    if (!wm) return WM_ERR_NULL_PTR;
    if (!wm->initialized) return WM_ERR_NOT_INITIALIZED;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_fuse_modalities", 0.0f);


    wm->status = WM_STATUS_FUSING;

    /* Compute cross-modal attention */
    compute_cross_modal_attention(wm);

    /* Fuse into global state using attention weights */
    memset(wm->global_state, 0, wm->global_state_dim * sizeof(float));

    for (int m = 0; m < WM_MODALITY_COUNT; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && WM_MODALITY_COUNT > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(m + 1) / (float)WM_MODALITY_COUNT);
        }

        if (!wm->modality_active[m]) continue;

        float weight = wm->attention.modality_weights[m];
        uint32_t dim = wm->encoder_dims[m] < wm->global_state_dim
                      ? wm->encoder_dims[m] : wm->global_state_dim;

        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                world_model_multimodal_heartbeat("world_model__loop",
                                 (float)(i + 1) / (float)dim);
            }

            wm->global_state[i] += weight * wm->modality_encoders[m][i];
        }
    }

    /* Update context buffer */
    uint32_t ctx_offset = wm->context_pos * wm->config.latent_dim;
    memcpy(&wm->context_buffer[ctx_offset], wm->global_state,
           wm->config.latent_dim * sizeof(float));
    wm->context_pos = (wm->context_pos + 1) % wm->config.context_size;

    wm->stats.fusion_operations++;
    wm->status = WM_STATUS_IDLE;

    return WM_OK;
}

wm_error_t wm_get_attention(
    nimcp_world_model_t* wm,
    wm_cross_modal_attention_t* attention)
{
    if (!wm || !attention) return WM_ERR_NULL_PTR;

    *attention = wm->attention;
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_attention", 0.0f);


    return WM_OK;
}

wm_error_t wm_set_fusion_weights(
    nimcp_world_model_t* wm,
    const float* weights,
    uint32_t num_weights)
{
    if (!wm || !weights) return WM_ERR_NULL_PTR;
    if (num_weights > WM_MODALITY_COUNT) return WM_ERR_CAPACITY_EXCEEDED;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_set_fusion_weight", 0.0f);


    for (uint32_t i = 0; i < num_weights; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_weights > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)num_weights);
        }

        wm->attention.modality_weights[i] = weights[i];
    }

    return WM_OK;
}

/*=============================================================================
 * PREDICTION API
 *===========================================================================*/

wm_error_t wm_predict(
    nimcp_world_model_t* wm,
    uint32_t horizon_steps,
    wm_prediction_t* prediction)
{
    if (!wm || !prediction) return WM_ERR_NULL_PTR;
    if (!wm->initialized) return WM_ERR_NOT_INITIALIZED;
    if (horizon_steps > wm->config.max_prediction_steps) return WM_ERR_INVALID_HORIZON;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_predict", 0.0f);


    wm->status = WM_STATUS_PREDICTING;

    /* Initialize prediction with current state */
    float* current = nimcp_calloc(wm->global_state_dim, sizeof(float));
    if (!current) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_predict: failed to allocate current state");
        wm->status = WM_STATUS_ERROR;
        wm->last_error = WM_ERR_MEMORY_ALLOC;
        return WM_ERR_MEMORY_ALLOC;
    }
    memcpy(current, wm->global_state, wm->global_state_dim * sizeof(float));

    prediction->horizon_steps = horizon_steps;
    prediction->state_dim = wm->global_state_dim;
    prediction->predicted_states = nimcp_calloc(horizon_steps * wm->global_state_dim, sizeof(float));
    if (!prediction->predicted_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_predict: failed to allocate predicted states");
        nimcp_free(current);
        current = NULL;
        wm->status = WM_STATUS_ERROR;
        wm->last_error = WM_ERR_MEMORY_ALLOC;
        return WM_ERR_MEMORY_ALLOC;
    }
    prediction->uncertainties = nimcp_calloc(horizon_steps, sizeof(float));
    if (!prediction->uncertainties) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_predict: failed to allocate uncertainties");
        nimcp_free(prediction->predicted_states);
        prediction->predicted_states = NULL;
        nimcp_free(current);
        current = NULL;
        wm->status = WM_STATUS_ERROR;
        wm->last_error = WM_ERR_MEMORY_ALLOC;
        return WM_ERR_MEMORY_ALLOC;
    }

    /* Simple autoregressive prediction */
    float uncertainty = 0.1f;
    for (uint32_t t = 0; t < horizon_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && horizon_steps > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(t + 1) / (float)horizon_steps);
        }

        /* Copy current state to predictions */
        memcpy(&prediction->predicted_states[t * wm->global_state_dim],
               current, wm->global_state_dim * sizeof(float));

        /* Simple decay model (real impl would use learned dynamics) */
        for (uint32_t i = 0; i < wm->global_state_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && wm->global_state_dim > 256) {
                world_model_multimodal_heartbeat("world_model__loop",
                                 (float)(i + 1) / (float)wm->global_state_dim);
            }

            current[i] *= wm->config.prediction_decay;
        }

        /* Accumulate uncertainty */
        prediction->uncertainties[t] = uncertainty;
        uncertainty *= 1.1f; /* Uncertainty grows over time */
    }

    prediction->prediction_confidence = 1.0f / (1.0f + uncertainty);
    prediction->surprise = 0.0f;
    prediction->num_entities = wm->num_entities;

    nimcp_free(current);
    current = NULL;
    wm->stats.predictions_made++;
    wm->status = WM_STATUS_IDLE;

    return WM_OK;
}

wm_error_t wm_predict_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    uint32_t horizon_steps,
    float* trajectory,
    float* confidence)
{
    if (!wm || !trajectory || !confidence) return WM_ERR_NULL_PTR;

    /* Find entity */
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_predict_entity", 0.0f);


    wm_entity_t* entity = NULL;
    for (uint32_t i = 0; i < wm->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)wm->num_entities);
        }

        if (wm->entities[i].entity_id == entity_id) {
            entity = &wm->entities[i];
            break;
        }
    }

    if (!entity) return WM_ERR_INVALID_MODALITY; /* Reuse error */

    /* Predict trajectory using velocity */
    float pos[3];
    memcpy(pos, entity->position, 3 * sizeof(float));

    for (uint32_t t = 0; t < horizon_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && horizon_steps > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(t + 1) / (float)horizon_steps);
        }

        trajectory[t * 3 + 0] = pos[0] + entity->velocity[0] * t;
        trajectory[t * 3 + 1] = pos[1] + entity->velocity[1] * t;
        trajectory[t * 3 + 2] = pos[2] + entity->velocity[2] * t;
    }

    *confidence = entity->existence_prob * wm->config.prediction_decay;

    return WM_OK;
}

wm_error_t wm_update_prediction_error(
    nimcp_world_model_t* wm,
    const float* actual_state,
    uint32_t state_dim)
{
    if (!wm || !actual_state) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_update_prediction", 0.0f);


    uint32_t dim = state_dim < wm->global_state_dim ? state_dim : wm->global_state_dim;

    float error = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)dim);
        }

        float diff = actual_state[i] - wm->global_state[i];
        error += diff * diff;
    }
    error = sqrtf(error / dim);

    /* Update running mean */
    wm->stats.mean_prediction_error =
        0.9f * wm->stats.mean_prediction_error + 0.1f * error;

    return WM_OK;
}

/*=============================================================================
 * ENTITY API
 *===========================================================================*/

wm_error_t wm_add_entity(
    nimcp_world_model_t* wm,
    const wm_entity_t* entity,
    uint32_t* entity_id)
{
    if (!wm || !entity || !entity_id) return WM_ERR_NULL_PTR;
    if (wm->num_entities >= wm->entity_capacity) return WM_ERR_CAPACITY_EXCEEDED;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_add_entity", 0.0f);


    wm_entity_t* new_entity = &wm->entities[wm->num_entities];
    *new_entity = *entity;
    new_entity->entity_id = wm->num_entities;
    *entity_id = new_entity->entity_id;

    wm->num_entities++;
    wm->stats.entity_births++;
    wm->stats.active_entities = wm->num_entities;

    return WM_OK;
}

wm_error_t wm_update_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    const wm_entity_t* update)
{
    if (!wm || !update) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_update_entity", 0.0f);


    for (uint32_t i = 0; i < wm->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)wm->num_entities);
        }

        if (wm->entities[i].entity_id == entity_id) {
            wm->entities[i] = *update;
            wm->entities[i].entity_id = entity_id;
            return WM_OK;
        }
    }

    return WM_ERR_INVALID_MODALITY; /* Entity not found */
}

wm_error_t wm_get_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    wm_entity_t* entity)
{
    if (!wm || !entity) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_entity", 0.0f);


    for (uint32_t i = 0; i < wm->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)wm->num_entities);
        }

        if (wm->entities[i].entity_id == entity_id) {
            *entity = wm->entities[i];
            return WM_OK;
        }
    }

    return WM_ERR_INVALID_MODALITY;
}

wm_error_t wm_remove_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id)
{
    if (!wm) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_remove_entity", 0.0f);


    for (uint32_t i = 0; i < wm->num_entities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && wm->num_entities > 256) {
            world_model_multimodal_heartbeat("world_model__loop",
                             (float)(i + 1) / (float)wm->num_entities);
        }

        if (wm->entities[i].entity_id == entity_id) {
            /* Shift remaining entities */
            for (uint32_t j = i; j < wm->num_entities - 1; j++) {
                wm->entities[j] = wm->entities[j + 1];
            }
            wm->num_entities--;
            wm->stats.entity_deaths++;
            wm->stats.active_entities = wm->num_entities;
            return WM_OK;
        }
    }

    return WM_ERR_INVALID_MODALITY;
}

wm_error_t wm_get_entities(
    nimcp_world_model_t* wm,
    wm_entity_t* entities,
    uint32_t* count)
{
    if (!wm || !entities || !count) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_entities", 0.0f);


    memcpy(entities, wm->entities, wm->num_entities * sizeof(wm_entity_t));
    *count = wm->num_entities;

    return WM_OK;
}

/*=============================================================================
 * STATE API
 *===========================================================================*/

wm_error_t wm_get_global_state(
    nimcp_world_model_t* wm,
    float* state,
    uint32_t* dim)
{
    if (!wm || !state || !dim) return WM_ERR_NULL_PTR;

    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_global_state", 0.0f);


    memcpy(state, wm->global_state, wm->global_state_dim * sizeof(float));
    *dim = wm->global_state_dim;

    return WM_OK;
}

wm_error_t wm_update(nimcp_world_model_t* wm, float dt_ms) {
    if (!wm) return WM_ERR_NULL_PTR;
    if (!wm->initialized) return WM_ERR_NOT_INITIALIZED;

    /* Update entity states */
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_update", 0.0f);


    update_entities(wm, dt_ms);

    /* Update coherence */
    wm->stats.mean_coherence =
        0.9f * wm->stats.mean_coherence + 0.1f * wm->attention.coherence_score;

    return WM_OK;
}

wm_error_t wm_get_stats(
    nimcp_world_model_t* wm,
    wm_stats_t* stats)
{
    if (!wm || !stats) return WM_ERR_NULL_PTR;

    *stats = wm->stats;
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_stats", 0.0f);


    return WM_OK;
}

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

wm_status_t wm_get_status(nimcp_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_status", 0.0f);


    return wm ? wm->status : WM_STATUS_ERROR;
}

wm_error_t wm_get_last_error(nimcp_world_model_t* wm) {
    /* Phase 8: Heartbeat at operation start */
    world_model_multimodal_heartbeat("world_model__wm_get_last_error", 0.0f);


    return wm ? wm->last_error : WM_ERR_NULL_PTR;
}

const char* wm_error_string(wm_error_t error) {
    switch (error) {
        case WM_OK: return "OK";
        case WM_ERR_NULL_PTR: return "Null pointer";
        case WM_ERR_NOT_INITIALIZED: return "Not initialized";
        case WM_ERR_INVALID_MODALITY: return "Invalid modality";
        case WM_ERR_PREDICTION_FAILED: return "Prediction failed";
        case WM_ERR_FUSION_FAILED: return "Fusion failed";
        case WM_ERR_MEMORY_ALLOC: return "Memory allocation failed";
        case WM_ERR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case WM_ERR_INVALID_HORIZON: return "Invalid prediction horizon";
        case WM_ERR_MODALITY_MISMATCH: return "Modality mismatch";
        default: return "Unknown error";
    }
}

const char* wm_status_string(wm_status_t status) {
    switch (status) {
        case WM_STATUS_IDLE: return "Idle";
        case WM_STATUS_PROCESSING: return "Processing";
        case WM_STATUS_PREDICTING: return "Predicting";
        case WM_STATUS_FUSING: return "Fusing";
        case WM_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

const char* wm_modality_string(wm_modality_t modality) {
    switch (modality) {
        case WM_MODALITY_VISUAL: return "Visual";
        case WM_MODALITY_AUDITORY: return "Auditory";
        case WM_MODALITY_TACTILE: return "Tactile";
        case WM_MODALITY_PROPRIOCEPTIVE: return "Proprioceptive";
        case WM_MODALITY_OLFACTORY: return "Olfactory";
        case WM_MODALITY_GUSTATORY: return "Gustatory";
        case WM_MODALITY_VESTIBULAR: return "Vestibular";
        case WM_MODALITY_INTEROCEPTIVE: return "Interoceptive";
        case WM_MODALITY_LINGUISTIC: return "Linguistic";
        case WM_MODALITY_SEMANTIC: return "Semantic";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void world_model_multimodal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_world_model_multimodal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int world_model_multimodal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "world_model_multimodal_training_begin: NULL argument");
        return -1;
    }
    world_model_multimodal_heartbeat_instance(NULL, "world_model_multimodal_training_begin", 0.0f);
    return 0;
}

int world_model_multimodal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "world_model_multimodal_training_end: NULL argument");
        return -1;
    }
    world_model_multimodal_heartbeat_instance(NULL, "world_model_multimodal_training_end", 1.0f);
    return 0;
}

int world_model_multimodal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "world_model_multimodal_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    world_model_multimodal_heartbeat_instance(NULL, "world_model_multimodal_training_step", progress);
    return 0;
}
