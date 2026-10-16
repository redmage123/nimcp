/**
 * @file nimcp_jepa_multimodal.c
 * @brief Multimodal JEPA Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Unified embedding space for visual and speech modalities
 * WHY:  Enable cross-modal reasoning and prediction
 * HOW:  Project modality-specific encodings to shared latent space
 *
 * @author NIMCP Development Team
 */

#include "cognitive/jepa/nimcp_jepa_multimodal.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE(jepa_multimodal, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MULTIMODAL_LOG_TAG "JEPA_MM"

/* GELU approximation constant */
#define GELU_CONST 0.044715f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int projection_create(
    jepa_mm_projection_t** proj,
    const jepa_mm_projection_config_t* config
);
static void projection_destroy(jepa_mm_projection_t* proj);
static int projection_forward(
    const jepa_mm_projection_t* proj,
    const float* input,
    float* output
);

static float gelu_activation(float x);
static void layer_normalize(float* data, uint32_t dim);
static float cosine_similarity(const float* a, const float* b, uint32_t dim);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_multimodal_default_config(jepa_multimodal_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Joint space config */
    config->joint_dim = JEPA_MULTIMODAL_DEFAULT_JOINT_DIM;

    /* Visual projection */
    config->visual_proj.input_dim = 256;   /* From visual JEPA */
    config->visual_proj.output_dim = JEPA_MULTIMODAL_DEFAULT_JOINT_DIM;
    config->visual_proj.hidden_dim = NIMCP_DEFAULT_HIDDEN_SIZE;
    config->visual_proj.use_layer_norm = true;
    config->visual_proj.use_bias = true;

    /* Speech projection */
    config->speech_proj.input_dim = 256;   /* From speech JEPA */
    config->speech_proj.output_dim = JEPA_MULTIMODAL_DEFAULT_JOINT_DIM;
    config->speech_proj.hidden_dim = NIMCP_DEFAULT_HIDDEN_SIZE;
    config->speech_proj.use_layer_norm = true;
    config->speech_proj.use_bias = true;

    /* Fusion config */
    config->fusion_type = JEPA_MM_FUSION_AVERAGE;

    /* Alignment config */
    config->alignment_type = JEPA_MM_ALIGN_CONTRASTIVE;
    config->temperature = JEPA_MULTIMODAL_DEFAULT_TEMPERATURE;
    config->alignment_weight = 1.0f;

    /* Cross-modal prediction config */
    config->cross_predictor.type = JEPA_PREDICTOR_MLP;
    config->cross_predictor.input_dim = JEPA_MULTIMODAL_DEFAULT_JOINT_DIM;
    config->cross_predictor.hidden_dim = NIMCP_DEFAULT_HIDDEN_SIZE;
    config->cross_predictor.output_dim = JEPA_MULTIMODAL_DEFAULT_JOINT_DIM;
    config->cross_predictor.num_layers = 2;
    config->cross_predictor.dropout_rate = 0.0f;
    config->cross_predictor.enable_layer_norm = true;
    config->cross_predictor.activation = JEPA_ACT_GELU;

    config->enable_visual_to_speech = true;
    config->enable_speech_to_visual = true;

    /* Training parameters */
    config->learning_rate = 0.0001f;
    config->momentum = 0.9f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_multimodal_t* jepa_multimodal_create(const jepa_multimodal_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_create", 0.0f);


    jepa_multimodal_t* mm = NULL;
    jepa_multimodal_config_t default_config;
    int rc = 0;

    /* Use defaults if not provided */
    if (!config) {
        jepa_multimodal_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate system */
    mm = (jepa_multimodal_t*)nimcp_calloc(1, sizeof(jepa_multimodal_t));
    if (!mm) return NULL;
    NIMCP_API_CHECK_ALLOC(mm, "Failed to allocate multimodal system");

    /* Initialize bridge base */
    if (bridge_base_init(&mm->base, BIO_MODULE_JEPA_MULTIMODAL, "jepa_multimodal") != 0) {
        nimcp_free(mm);
        return NULL;
    }
    mm->base.bridge_active = false;

    /* Copy configuration */
    memcpy(&mm->config, config, sizeof(jepa_multimodal_config_t));

    /* Validate and clamp temperature to positive value */
    if (mm->config.temperature <= 0.0f) {
        mm->config.temperature = JEPA_MULTIMODAL_DEFAULT_TEMPERATURE;
    }

    /* Create visual projection */
    rc = projection_create(&mm->visual_projection, &config->visual_proj);
    if (rc != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to create visual projection: %d", rc);
        goto cleanup;
    }

    /* Create speech projection */
    rc = projection_create(&mm->speech_projection, &config->speech_proj);
    if (rc != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to create speech projection: %d", rc);
        goto cleanup;
    }

    /* Create cross-modal predictors */
    if (config->enable_visual_to_speech) {
        mm->visual_to_speech = jepa_predictor_create(&config->cross_predictor);
        if (!mm->visual_to_speech) {
            NIMCP_LOGGING_ERROR("Failed to create V->S predictor");
            goto cleanup;
        }
    }

    if (config->enable_speech_to_visual) {
        mm->speech_to_visual = jepa_predictor_create(&config->cross_predictor);
        if (!mm->speech_to_visual) {
            NIMCP_LOGGING_ERROR("Failed to create S->V predictor");
            goto cleanup;
        }
    }

    /* Allocate working buffers */
    mm->visual_buffer = (float*)nimcp_calloc(config->joint_dim, sizeof(float));
    mm->speech_buffer = (float*)nimcp_calloc(config->joint_dim, sizeof(float));
    if (!mm->speech_buffer) return NULL;
    mm->fused_buffer = (float*)nimcp_calloc(config->joint_dim * 2, sizeof(float));

    if (!mm->visual_buffer || !mm->speech_buffer || !mm->fused_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate buffers");
        goto cleanup;
    }

    /* Initialize training state */
    mm->training_mode = false;
    mm->training_step = 0;

    /* Mark as initialized */
    mm->base.bridge_active = true;

    NIMCP_LOGGING_INFO("Created multimodal JEPA: joint_dim=%u, fusion=%d",
        config->joint_dim, config->fusion_type);

    return mm;

cleanup:
    jepa_multimodal_destroy(mm);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_multimodal_create: operation failed");
    return NULL;
}

void jepa_multimodal_destroy(jepa_multimodal_t* mm)
{
    if (!mm) {
        return;
    }

    /* Disconnect from bio-async */
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_destroy", 0.0f);


    jepa_multimodal_disconnect_bio_async(mm);

    /* Free projections */
    projection_destroy(mm->visual_projection);
    projection_destroy(mm->speech_projection);

    /* Free predictors */
    if (mm->visual_to_speech) {
        jepa_predictor_destroy(mm->visual_to_speech);
    }
    if (mm->speech_to_visual) {
        jepa_predictor_destroy(mm->speech_to_visual);
    }

    /* Free buffers */
    nimcp_free(mm->visual_buffer);
    nimcp_free(mm->speech_buffer);
    nimcp_free(mm->fused_buffer);

    /* Cleanup bridge base (mutex, etc.) */
    bridge_base_cleanup(&mm->base);

    /* Free system */
    nimcp_free(mm);
    mm = NULL;
}

int jepa_multimodal_reset(jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_reset", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    /* Reset stats */
    memset(&mm->stats, 0, sizeof(mm->stats));

    /* Reset training state */
    mm->training_step = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int jepa_multimodal_connect_visual(
    jepa_multimodal_t* mm,
    visual_jepa_bridge_t* visual)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_connect_visual", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual, NIMCP_ERROR_INVALID_PARAM, "visual is NULL");

    mm->visual_encoder = visual;

    NIMCP_LOGGING_INFO("Connected visual JEPA encoder");

    return NIMCP_SUCCESS;
}

int jepa_multimodal_connect_speech(
    jepa_multimodal_t* mm,
    speech_jepa_bridge_t* speech)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_connect_speech", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(speech, NIMCP_ERROR_INVALID_PARAM, "speech is NULL");

    mm->speech_encoder = speech;

    NIMCP_LOGGING_INFO("Connected speech JEPA encoder");

    return NIMCP_SUCCESS;
}

int jepa_multimodal_disconnect_all(jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_disconnect_all", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    mm->visual_encoder = NULL;
    mm->speech_encoder = NULL;
    mm->base.bridge_active = false;

    return NIMCP_SUCCESS;
}

bool jepa_multimodal_is_connected(const jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_is_connected", 0.0f);


    return mm && mm->visual_encoder && mm->speech_encoder;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int jepa_multimodal_encode_visual(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    jepa_latent_t* joint_latent)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_encode_visual", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latent, NIMCP_ERROR_INVALID_PARAM, "visual_latent is NULL");
    NIMCP_CHECK_THROW(joint_latent, NIMCP_ERROR_INVALID_PARAM, "joint_latent is NULL");

    /* Project to joint space */
    int rc = projection_forward(
        mm->visual_projection,
        visual_latent->embedding,
        mm->visual_buffer
    );
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    /* Initialize joint latent if needed */
    if (!joint_latent->embedding) {
        joint_latent->embedding = (float*)nimcp_calloc(mm->config.joint_dim, sizeof(float));
        if (!joint_latent->embedding) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        joint_latent->latent_dim = mm->config.joint_dim;
    }

    /* Copy projection */
    memcpy(joint_latent->embedding, mm->visual_buffer,
           mm->config.joint_dim * sizeof(float));

    /* L2 normalize for contrastive */
    float norm = 0.0f;
    for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)mm->config.joint_dim);
        }

        norm += joint_latent->embedding[i] * joint_latent->embedding[i];
    }
    norm = sqrtf(norm + 1e-8f);
    for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)mm->config.joint_dim);
        }

        joint_latent->embedding[i] /= norm;
    }

    joint_latent->modality = JEPA_MODALITY_MULTIMODAL;
    joint_latent->precision = visual_latent->precision;
    joint_latent->timestamp_ms = visual_latent->timestamp_ms;

    mm->stats.visual_encodings++;

    return NIMCP_SUCCESS;
}

int jepa_multimodal_encode_speech(
    jepa_multimodal_t* mm,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* joint_latent)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_encode_speech", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(speech_latent, NIMCP_ERROR_INVALID_PARAM, "speech_latent is NULL");
    NIMCP_CHECK_THROW(joint_latent, NIMCP_ERROR_INVALID_PARAM, "joint_latent is NULL");

    /* Project to joint space */
    int rc = projection_forward(
        mm->speech_projection,
        speech_latent->embedding,
        mm->speech_buffer
    );
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    /* Initialize joint latent if needed */
    if (!joint_latent->embedding) {
        joint_latent->embedding = (float*)nimcp_calloc(mm->config.joint_dim, sizeof(float));
        if (!joint_latent->embedding) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        joint_latent->latent_dim = mm->config.joint_dim;
    }

    /* Copy projection */
    memcpy(joint_latent->embedding, mm->speech_buffer,
           mm->config.joint_dim * sizeof(float));

    /* L2 normalize for contrastive */
    float norm = 0.0f;
    for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)mm->config.joint_dim);
        }

        norm += joint_latent->embedding[i] * joint_latent->embedding[i];
    }
    norm = sqrtf(norm + 1e-8f);
    for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)mm->config.joint_dim);
        }

        joint_latent->embedding[i] /= norm;
    }

    joint_latent->modality = JEPA_MODALITY_MULTIMODAL;
    joint_latent->precision = speech_latent->precision;
    joint_latent->timestamp_ms = speech_latent->timestamp_ms;

    mm->stats.speech_encodings++;

    return NIMCP_SUCCESS;
}

int jepa_multimodal_fuse(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* fused_latent)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_fuse", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latent, NIMCP_ERROR_INVALID_PARAM, "visual_latent is NULL");
    NIMCP_CHECK_THROW(speech_latent, NIMCP_ERROR_INVALID_PARAM, "speech_latent is NULL");
    NIMCP_CHECK_THROW(fused_latent, NIMCP_ERROR_INVALID_PARAM, "fused_latent is NULL");

    /* Project both to joint space */
    jepa_latent_t visual_joint = {0};
    jepa_latent_t speech_joint = {0};

    int rc = jepa_multimodal_encode_visual(mm, visual_latent, &visual_joint);
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    rc = jepa_multimodal_encode_speech(mm, speech_latent, &speech_joint);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(visual_joint.embedding);
        return rc;
    }

    uint32_t joint_dim = mm->config.joint_dim;

    /* Initialize fused latent based on fusion type */
    uint32_t fused_dim = joint_dim;
    if (mm->config.fusion_type == JEPA_MM_FUSION_CONCATENATE) {
        fused_dim = joint_dim * 2;
    }

    if (!fused_latent->embedding) {
        fused_latent->embedding = (float*)nimcp_calloc(fused_dim, sizeof(float));
        if (!fused_latent->embedding) {
            nimcp_free(visual_joint.embedding);
            nimcp_free(speech_joint.embedding);
            return NIMCP_ERROR_NO_MEMORY;
        }
        fused_latent->latent_dim = fused_dim;
    }

    /* Apply fusion strategy */
    switch (mm->config.fusion_type) {
        case JEPA_MM_FUSION_CONCATENATE:
            memcpy(fused_latent->embedding, visual_joint.embedding,
                   joint_dim * sizeof(float));
            memcpy(fused_latent->embedding + joint_dim, speech_joint.embedding,
                   joint_dim * sizeof(float));
            break;

        case JEPA_MM_FUSION_AVERAGE:
            for (uint32_t i = 0; i < joint_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && joint_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)joint_dim);
                }

                fused_latent->embedding[i] = 0.5f * (
                    visual_joint.embedding[i] + speech_joint.embedding[i]
                );
            }
            break;

        case JEPA_MM_FUSION_ATTENTION:
            /* Simple attention: use similarity as weight */
            {
                float sim = cosine_similarity(
                    visual_joint.embedding, speech_joint.embedding, joint_dim);
                float visual_weight = 0.5f + 0.5f * sim;  /* Range [0, 1] */
                float speech_weight = 1.0f - visual_weight;
                for (uint32_t i = 0; i < joint_dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && joint_dim > 256) {
                        jepa_multimodal_heartbeat("jepa_multimo_loop",
                                         (float)(i + 1) / (float)joint_dim);
                    }

                    fused_latent->embedding[i] =
                        visual_weight * visual_joint.embedding[i] +
                        speech_weight * speech_joint.embedding[i];
                }
            }
            break;

        case JEPA_MM_FUSION_GATE:
            /* Gated fusion: element-wise sigmoid gate */
            for (uint32_t i = 0; i < joint_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && joint_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)joint_dim);
                }

                float gate = 1.0f / (1.0f + expf(-(visual_joint.embedding[i])));
                fused_latent->embedding[i] =
                    gate * visual_joint.embedding[i] +
                    (1.0f - gate) * speech_joint.embedding[i];
            }
            break;

        default:
            /* Default to average */
            for (uint32_t i = 0; i < joint_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && joint_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)joint_dim);
                }

                fused_latent->embedding[i] = 0.5f * (
                    visual_joint.embedding[i] + speech_joint.embedding[i]
                );
            }
            break;
    }

    /* Set metadata */
    fused_latent->modality = JEPA_MODALITY_MULTIMODAL;
    fused_latent->precision = (visual_latent->precision + speech_latent->precision) / 2.0f;
    fused_latent->timestamp_ms = visual_latent->timestamp_ms;

    /* Cleanup */
    nimcp_free(visual_joint.embedding);
    nimcp_free(speech_joint.embedding);

    mm->stats.fusions_performed++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cross-Modal Prediction API
 * ============================================================================ */

int jepa_multimodal_predict_speech_from_visual(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    jepa_latent_t* predicted_speech)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_predict_speech_from_", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latent, NIMCP_ERROR_INVALID_PARAM, "visual_latent is NULL");
    NIMCP_CHECK_THROW(predicted_speech, NIMCP_ERROR_INVALID_PARAM, "predicted_speech is NULL");
    NIMCP_CHECK_THROW(mm->visual_to_speech, NIMCP_ERROR_NOT_INITIALIZED, "V->S predictor not enabled");

    /* Project visual to joint space */
    jepa_latent_t visual_joint = {0};
    int rc = jepa_multimodal_encode_visual(mm, visual_latent, &visual_joint);
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    /* Allocate output latent if needed */
    if (!predicted_speech->embedding || predicted_speech->latent_dim != mm->config.joint_dim) {
        nimcp_free(predicted_speech->embedding);
        predicted_speech->embedding = (float*)nimcp_calloc(mm->config.joint_dim, sizeof(float));
        if (!predicted_speech->embedding) {
            nimcp_free(visual_joint.embedding);
            return NIMCP_ERROR_NO_MEMORY;
        }
        predicted_speech->latent_dim = mm->config.joint_dim;
    }

    /* Predict speech in joint space */
    rc = jepa_predictor_predict(mm->visual_to_speech, &visual_joint, predicted_speech);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(visual_joint.embedding);
        return rc;
    }

    predicted_speech->modality = JEPA_MODALITY_SPEECH;
    mm->stats.visual_to_speech_preds++;

    nimcp_free(visual_joint.embedding);
    return NIMCP_SUCCESS;
}

int jepa_multimodal_predict_visual_from_speech(
    jepa_multimodal_t* mm,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* predicted_visual)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_predict_visual_from_", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(speech_latent, NIMCP_ERROR_INVALID_PARAM, "speech_latent is NULL");
    NIMCP_CHECK_THROW(predicted_visual, NIMCP_ERROR_INVALID_PARAM, "predicted_visual is NULL");
    NIMCP_CHECK_THROW(mm->speech_to_visual, NIMCP_ERROR_NOT_INITIALIZED, "S->V predictor not enabled");

    /* Project speech to joint space */
    jepa_latent_t speech_joint = {0};
    int rc = jepa_multimodal_encode_speech(mm, speech_latent, &speech_joint);
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    /* Allocate output latent if needed */
    if (!predicted_visual->embedding || predicted_visual->latent_dim != mm->config.joint_dim) {
        nimcp_free(predicted_visual->embedding);
        predicted_visual->embedding = (float*)nimcp_calloc(mm->config.joint_dim, sizeof(float));
        if (!predicted_visual->embedding) {
            nimcp_free(speech_joint.embedding);
            return NIMCP_ERROR_NO_MEMORY;
        }
        predicted_visual->latent_dim = mm->config.joint_dim;
    }

    /* Predict visual in joint space */
    rc = jepa_predictor_predict(mm->speech_to_visual, &speech_joint, predicted_visual);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(speech_joint.embedding);
        return rc;
    }

    predicted_visual->modality = JEPA_MODALITY_VISUAL;
    mm->stats.speech_to_visual_preds++;

    nimcp_free(speech_joint.embedding);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Similarity API
 * ============================================================================ */

int jepa_multimodal_similarity(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    float* similarity)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_similarity", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latent, NIMCP_ERROR_INVALID_PARAM, "visual_latent is NULL");
    NIMCP_CHECK_THROW(speech_latent, NIMCP_ERROR_INVALID_PARAM, "speech_latent is NULL");
    NIMCP_CHECK_THROW(similarity, NIMCP_ERROR_INVALID_PARAM, "similarity is NULL");

    /* Check if both latents are already in joint space (multimodal) */
    if (visual_latent->modality == JEPA_MODALITY_MULTIMODAL &&
        speech_latent->modality == JEPA_MODALITY_MULTIMODAL) {
        /* Direct cosine similarity without projection */
        uint32_t dim = (visual_latent->latent_dim < speech_latent->latent_dim)
                     ? visual_latent->latent_dim : speech_latent->latent_dim;
        *similarity = cosine_similarity(
            visual_latent->embedding,
            speech_latent->embedding,
            dim
        );
        /* Rescale to [0, 1] */
        *similarity = (*similarity + 1.0f) / 2.0f;
        return NIMCP_SUCCESS;
    }

    /* Project both to joint space */
    jepa_latent_t visual_joint = {0};
    jepa_latent_t speech_joint = {0};

    int rc = jepa_multimodal_encode_visual(mm, visual_latent, &visual_joint);
    if (rc != NIMCP_SUCCESS) {
        return rc;
    }

    rc = jepa_multimodal_encode_speech(mm, speech_latent, &speech_joint);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(visual_joint.embedding);
        return rc;
    }

    /* Compute cosine similarity */
    *similarity = cosine_similarity(
        visual_joint.embedding,
        speech_joint.embedding,
        mm->config.joint_dim
    );

    /* Rescale to [0, 1] */
    *similarity = (*similarity + 1.0f) / 2.0f;

    nimcp_free(visual_joint.embedding);
    nimcp_free(speech_joint.embedding);

    return NIMCP_SUCCESS;
}

int jepa_multimodal_batch_similarity(
    jepa_multimodal_t* mm,
    jepa_latent_t** visual_latents,
    jepa_latent_t** speech_latents,
    uint32_t num_samples,
    float* similarity_matrix)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_batch_similarity", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latents, NIMCP_ERROR_INVALID_PARAM, "visual_latents is NULL");
    NIMCP_CHECK_THROW(speech_latents, NIMCP_ERROR_INVALID_PARAM, "speech_latents is NULL");
    NIMCP_CHECK_THROW(similarity_matrix, NIMCP_ERROR_INVALID_PARAM, "similarity_matrix is NULL");

    if (num_samples == 0) {
        return NIMCP_SUCCESS;
    }

    /* Project all to joint space */
    jepa_latent_t* visual_joints = (jepa_latent_t*)nimcp_calloc(
        num_samples, sizeof(jepa_latent_t));
    jepa_latent_t* speech_joints = (jepa_latent_t*)nimcp_calloc(
        num_samples, sizeof(jepa_latent_t));

    if (!visual_joints || !speech_joints) {
        nimcp_free(visual_joints);
        visual_joints = NULL;
        nimcp_free(speech_joints);
        speech_joints = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Project visual */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        int rc = jepa_multimodal_encode_visual(mm, visual_latents[i], &visual_joints[i]);
        if (rc != NIMCP_SUCCESS) {
            /* Cleanup on error */
            for (uint32_t j = 0; j < num_samples; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_samples > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(j + 1) / (float)num_samples);
                }

                nimcp_free(visual_joints[j].embedding);
                nimcp_free(speech_joints[j].embedding);
            }
            nimcp_free(visual_joints);
            visual_joints = NULL;
            nimcp_free(speech_joints);
            speech_joints = NULL;
            return rc;
        }
    }

    /* Project speech */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        int rc = jepa_multimodal_encode_speech(mm, speech_latents[i], &speech_joints[i]);
        if (rc != NIMCP_SUCCESS) {
            for (uint32_t j = 0; j < num_samples; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_samples > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(j + 1) / (float)num_samples);
                }

                nimcp_free(visual_joints[j].embedding);
                nimcp_free(speech_joints[j].embedding);
            }
            nimcp_free(visual_joints);
            visual_joints = NULL;
            nimcp_free(speech_joints);
            speech_joints = NULL;
            return rc;
        }
    }

    /* Compute all pairwise similarities */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        for (uint32_t j = 0; j < num_samples; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_samples > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(j + 1) / (float)num_samples);
            }

            float sim = cosine_similarity(
                visual_joints[i].embedding,
                speech_joints[j].embedding,
                mm->config.joint_dim
            );
            /* Rescale from [-1, 1] to [0, 1] */
            similarity_matrix[i * num_samples + j] = (sim + 1.0f) / 2.0f;
        }
    }

    /* Cleanup */
    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        nimcp_free(visual_joints[i].embedding);
        nimcp_free(speech_joints[i].embedding);
    }
    nimcp_free(visual_joints);
    visual_joints = NULL;
    nimcp_free(speech_joints);
    speech_joints = NULL;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int jepa_multimodal_set_training(jepa_multimodal_t* mm, bool training)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_set_training", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    mm->training_mode = training;

    if (mm->visual_to_speech) {
        jepa_predictor_set_training(mm->visual_to_speech, training);
    }
    if (mm->speech_to_visual) {
        jepa_predictor_set_training(mm->speech_to_visual, training);
    }

    return NIMCP_SUCCESS;
}

int jepa_multimodal_align_step(
    jepa_multimodal_t* mm,
    const jepa_mm_batch_t* batch,
    float* loss)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_align_step", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(batch, NIMCP_ERROR_INVALID_PARAM, "batch is NULL");
    NIMCP_CHECK_THROW(loss, NIMCP_ERROR_INVALID_PARAM, "loss is NULL");

    if (batch->num_pairs == 0) {
        *loss = 0.0f;
        return NIMCP_SUCCESS;
    }

    float total_loss = 0.0f;

    switch (mm->config.alignment_type) {
        case JEPA_MM_ALIGN_CONTRASTIVE: {
            /* InfoNCE-style contrastive loss */
            float temperature = mm->config.temperature;

            for (uint32_t i = 0; i < batch->num_pairs; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && batch->num_pairs > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)batch->num_pairs);
                }

                if (!batch->pairs[i].is_matched) continue;

                /* Project pair to joint space */
                jepa_latent_t v_joint = {0};
                jepa_latent_t s_joint = {0};

                jepa_multimodal_encode_visual(mm, batch->pairs[i].visual, &v_joint);
                jepa_multimodal_encode_speech(mm, batch->pairs[i].speech, &s_joint);

                /* Positive similarity */
                float pos_sim = cosine_similarity(
                    v_joint.embedding, s_joint.embedding, mm->config.joint_dim);
                pos_sim /= temperature;

                /* Compute negative similarities and denominator */
                float exp_sum = expf(pos_sim);
                for (uint32_t j = 0; j < batch->num_pairs; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && batch->num_pairs > 256) {
                        jepa_multimodal_heartbeat("jepa_multimo_loop",
                                         (float)(j + 1) / (float)batch->num_pairs);
                    }

                    if (j == i) continue;

                    jepa_latent_t neg_s = {0};
                    jepa_multimodal_encode_speech(mm, batch->pairs[j].speech, &neg_s);

                    float neg_sim = cosine_similarity(
                        v_joint.embedding, neg_s.embedding, mm->config.joint_dim);
                    neg_sim /= temperature;
                    exp_sum += expf(neg_sim);

                    nimcp_free(neg_s.embedding);
                }

                /* NCE loss: -log(exp(pos) / sum(exp)) */
                total_loss -= logf(expf(pos_sim) / (fabsf(exp_sum) > 1e-7f ? exp_sum : 1e-7f));

                nimcp_free(v_joint.embedding);
                nimcp_free(s_joint.embedding);
            }

            if (batch->num_positive > 0) {
                total_loss /= (float)batch->num_positive;
            }
            break;
        }

        case JEPA_MM_ALIGN_MSE: {
            /* Mean squared error between matched pairs */
            for (uint32_t i = 0; i < batch->num_pairs; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && batch->num_pairs > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)batch->num_pairs);
                }

                if (!batch->pairs[i].is_matched) continue;

                jepa_latent_t v_joint = {0};
                jepa_latent_t s_joint = {0};

                jepa_multimodal_encode_visual(mm, batch->pairs[i].visual, &v_joint);
                jepa_multimodal_encode_speech(mm, batch->pairs[i].speech, &s_joint);

                for (uint32_t j = 0; j < mm->config.joint_dim; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && mm->config.joint_dim > 256) {
                        jepa_multimodal_heartbeat("jepa_multimo_loop",
                                         (float)(j + 1) / (float)mm->config.joint_dim);
                    }

                    float diff = v_joint.embedding[j] - s_joint.embedding[j];
                    total_loss += diff * diff;
                }

                nimcp_free(v_joint.embedding);
                nimcp_free(s_joint.embedding);
            }

            if (batch->num_positive > 0) {
                total_loss /= (float)(batch->num_positive * mm->config.joint_dim);
            }
            break;
        }

        case JEPA_MM_ALIGN_COSINE: {
            /* Cosine similarity loss (1 - sim) */
            for (uint32_t i = 0; i < batch->num_pairs; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && batch->num_pairs > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)batch->num_pairs);
                }

                if (!batch->pairs[i].is_matched) continue;

                jepa_latent_t v_joint = {0};
                jepa_latent_t s_joint = {0};

                jepa_multimodal_encode_visual(mm, batch->pairs[i].visual, &v_joint);
                jepa_multimodal_encode_speech(mm, batch->pairs[i].speech, &s_joint);

                float sim = cosine_similarity(
                    v_joint.embedding, s_joint.embedding, mm->config.joint_dim);
                total_loss += 1.0f - sim;

                nimcp_free(v_joint.embedding);
                nimcp_free(s_joint.embedding);
            }

            if (batch->num_positive > 0) {
                total_loss /= (float)batch->num_positive;
            }
            break;
        }

        default:
            *loss = 0.0f;
            return NIMCP_SUCCESS;
    }

    *loss = total_loss * mm->config.alignment_weight;

    /* Update stats */
    mm->training_step++;
    mm->stats.alignment_steps++;
    mm->stats.avg_alignment_loss =
        (mm->stats.avg_alignment_loss * (mm->stats.alignment_steps - 1) + *loss) /
        mm->stats.alignment_steps;

    return NIMCP_SUCCESS;
}

int jepa_multimodal_cross_pred_step(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    float* loss)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_cross_pred_step", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(visual_latent, NIMCP_ERROR_INVALID_PARAM, "visual_latent is NULL");
    NIMCP_CHECK_THROW(speech_latent, NIMCP_ERROR_INVALID_PARAM, "speech_latent is NULL");
    NIMCP_CHECK_THROW(loss, NIMCP_ERROR_INVALID_PARAM, "loss is NULL");

    float total_loss = 0.0f;
    int count = 0;

    /* Visual → Speech prediction */
    if (mm->visual_to_speech) {
        jepa_latent_t pred_speech = {0};
        jepa_latent_t speech_joint = {0};

        jepa_multimodal_predict_speech_from_visual(mm, visual_latent, &pred_speech);
        jepa_multimodal_encode_speech(mm, speech_latent, &speech_joint);

        /* MSE loss */
        float v2s_loss = 0.0f;
        for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(i + 1) / (float)mm->config.joint_dim);
            }

            float diff = pred_speech.embedding[i] - speech_joint.embedding[i];
            v2s_loss += diff * diff;
        }
        v2s_loss /= (float)mm->config.joint_dim;
        total_loss += v2s_loss;
        count++;

        nimcp_free(pred_speech.embedding);
        nimcp_free(speech_joint.embedding);
    }

    /* Speech → Visual prediction */
    if (mm->speech_to_visual) {
        jepa_latent_t pred_visual = {0};
        jepa_latent_t visual_joint = {0};

        jepa_multimodal_predict_visual_from_speech(mm, speech_latent, &pred_visual);
        jepa_multimodal_encode_visual(mm, visual_latent, &visual_joint);

        /* MSE loss */
        float s2v_loss = 0.0f;
        for (uint32_t i = 0; i < mm->config.joint_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mm->config.joint_dim > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(i + 1) / (float)mm->config.joint_dim);
            }

            float diff = pred_visual.embedding[i] - visual_joint.embedding[i];
            s2v_loss += diff * diff;
        }
        s2v_loss /= (float)mm->config.joint_dim;
        total_loss += s2v_loss;
        count++;

        nimcp_free(pred_visual.embedding);
        nimcp_free(visual_joint.embedding);
    }

    *loss = count > 0 ? total_loss / (float)count : 0.0f;

    /* Update stats */
    mm->stats.avg_cross_pred_loss =
        (mm->stats.avg_cross_pred_loss * mm->training_step + *loss) /
        (mm->training_step + 1);
    mm->training_step++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Batch Management
 * ============================================================================ */

jepa_mm_batch_t* jepa_mm_batch_create(uint32_t max_pairs)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_jepa_mm_batch_create", 0.0f);


    if (max_pairs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mm_batch_create: max_pairs is zero");
        return NULL;
    }

    jepa_mm_batch_t* batch = (jepa_mm_batch_t*)nimcp_calloc(1, sizeof(jepa_mm_batch_t));
    if (!batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate batch");

        return NULL;
    }

    batch->pairs = (jepa_mm_pair_t*)nimcp_calloc(max_pairs, sizeof(jepa_mm_pair_t));
    if (!batch->pairs) {
        nimcp_free(batch);
        batch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mm_batch_create: batch->pairs is NULL");
        return NULL;
    }

    return batch;
}

void jepa_mm_batch_destroy(jepa_mm_batch_t* batch)
{
    if (!batch) {
        return;
    }

    /* Note: pairs don't own their latents */
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_jepa_mm_batch_destro", 0.0f);


    nimcp_free(batch->pairs);
    nimcp_free(batch);
    batch = NULL;
}

int jepa_mm_batch_add_pair(
    jepa_mm_batch_t* batch,
    const jepa_latent_t* visual,
    const jepa_latent_t* speech,
    bool is_matched)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_jepa_mm_batch_add_pa", 0.0f);


    NIMCP_CHECK_THROW(batch, NIMCP_ERROR_INVALID_PARAM, "batch is NULL");
    NIMCP_CHECK_THROW(visual, NIMCP_ERROR_INVALID_PARAM, "visual is NULL");
    NIMCP_CHECK_THROW(speech, NIMCP_ERROR_INVALID_PARAM, "speech is NULL");

    /* Store references (not copies) */
    batch->pairs[batch->num_pairs].visual = (jepa_latent_t*)visual;
    batch->pairs[batch->num_pairs].speech = (jepa_latent_t*)speech;
    batch->pairs[batch->num_pairs].is_matched = is_matched;
    batch->pairs[batch->num_pairs].similarity = is_matched ? 1.0f : 0.0f;

    batch->num_pairs++;
    if (is_matched) {
        batch->num_positive++;
    }

    return NIMCP_SUCCESS;
}

int jepa_mm_batch_clear(jepa_mm_batch_t* batch)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_jepa_mm_batch_clear", 0.0f);


    NIMCP_CHECK_THROW(batch, NIMCP_ERROR_INVALID_PARAM, "batch is NULL");

    batch->num_pairs = 0;
    batch->num_positive = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int jepa_multimodal_get_stats(
    const jepa_multimodal_t* mm,
    jepa_multimodal_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_get_stats", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &mm->stats, sizeof(jepa_multimodal_stats_t));
    return NIMCP_SUCCESS;
}

int jepa_multimodal_reset_stats(jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    memset(&mm->stats, 0, sizeof(mm->stats));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int jepa_multimodal_connect_bio_async(jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    mm->base.bio_async_enabled = true;

    NIMCP_LOGGING_DEBUG("Connected to bio-async router");

    return NIMCP_SUCCESS;
}

int jepa_multimodal_disconnect_bio_async(jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(mm, NIMCP_ERROR_INVALID_PARAM, "mm is NULL");

    mm->base.bio_async_enabled = false;

    return NIMCP_SUCCESS;
}

bool jepa_multimodal_is_bio_async_connected(const jepa_multimodal_t* mm)
{
    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_is_bio_async_connect", 0.0f);


    return mm && mm->base.bio_async_enabled;
}

/* ============================================================================
 * Internal Functions - Projection
 * ============================================================================ */

static int projection_create(
    jepa_mm_projection_t** proj,
    const jepa_mm_projection_config_t* config)
{
    NIMCP_CHECK_THROW(proj, NIMCP_ERROR_INVALID_PARAM, "proj output pointer is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    jepa_mm_projection_t* p = (jepa_mm_projection_t*)nimcp_calloc(
        1, sizeof(jepa_mm_projection_t));
    if (!p) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    p->input_dim = config->input_dim;
    p->hidden_dim = config->hidden_dim;
    p->output_dim = config->output_dim;
    p->use_bias = config->use_bias;
    p->use_layer_norm = config->use_layer_norm;

    if (config->hidden_dim > 0) {
        /* Two-layer MLP projection */
        p->hidden_weights = (float*)nimcp_calloc(
            config->input_dim * config->hidden_dim, sizeof(float));
        p->hidden_bias = (float*)nimcp_calloc(config->hidden_dim, sizeof(float));
        if (!p->hidden_bias) return -1;
        p->weights = (float*)nimcp_calloc(
            config->hidden_dim * config->output_dim, sizeof(float));

        if (!p->hidden_weights || !p->hidden_bias || !p->weights) {
            projection_destroy(p);
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Xavier initialization */
        float scale1 = sqrtf(2.0f / (float)(config->input_dim + config->hidden_dim));
        float scale2 = sqrtf(2.0f / (float)(config->hidden_dim + config->output_dim));

        for (uint32_t i = 0; i < config->input_dim * config->hidden_dim; i++) {
            p->hidden_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale1;
        }
        for (uint32_t i = 0; i < config->hidden_dim * config->output_dim; i++) {
            p->weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale2;
        }
    } else {
        /* Linear projection */
        p->weights = (float*)nimcp_calloc(
            config->input_dim * config->output_dim, sizeof(float));

        if (!p->weights) {
            projection_destroy(p);
            return NIMCP_ERROR_NO_MEMORY;
        }

        float scale = sqrtf(2.0f / (float)(config->input_dim + config->output_dim));
        for (uint32_t i = 0; i < config->input_dim * config->output_dim; i++) {
            p->weights[i] = ((float)nimcp_tl_rand() / RAND_MAX * 2.0f - 1.0f) * scale;
        }
    }

    if (config->use_bias) {
        p->bias = (float*)nimcp_calloc(config->output_dim, sizeof(float));
        if (!p->bias) {
            projection_destroy(p);
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    *proj = p;
    return NIMCP_SUCCESS;
}

static void projection_destroy(jepa_mm_projection_t* proj)
{
    if (!proj) return;

    nimcp_free(proj->weights);
    nimcp_free(proj->bias);
    nimcp_free(proj->hidden_weights);
    nimcp_free(proj->hidden_bias);
    nimcp_free(proj);
    proj = NULL;
}

static int projection_forward(
    const jepa_mm_projection_t* proj,
    const float* input,
    float* output)
{
    NIMCP_CHECK_THROW(proj, NIMCP_ERROR_INVALID_PARAM, "proj is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output is NULL");

    if (proj->hidden_dim > 0) {
        /* Two-layer MLP */
        float* hidden = (float*)nimcp_calloc(proj->hidden_dim, sizeof(float));
        if (!hidden) {
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Layer 1: hidden = GELU(input @ W_hidden + b_hidden) */
        for (uint32_t j = 0; j < proj->hidden_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && proj->hidden_dim > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(j + 1) / (float)proj->hidden_dim);
            }

            float sum = proj->hidden_bias ? proj->hidden_bias[j] : 0.0f;
            for (uint32_t i = 0; i < proj->input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && proj->input_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)proj->input_dim);
                }

                sum += input[i] * proj->hidden_weights[i * proj->hidden_dim + j];
            }
            hidden[j] = gelu_activation(sum);
        }

        if (proj->use_layer_norm) {
            layer_normalize(hidden, proj->hidden_dim);
        }

        /* Layer 2: output = hidden @ W_out + b_out */
        for (uint32_t j = 0; j < proj->output_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && proj->output_dim > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(j + 1) / (float)proj->output_dim);
            }

            float sum = proj->bias ? proj->bias[j] : 0.0f;
            for (uint32_t i = 0; i < proj->hidden_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && proj->hidden_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)proj->hidden_dim);
                }

                sum += hidden[i] * proj->weights[i * proj->output_dim + j];
            }
            output[j] = sum;
        }

        nimcp_free(hidden);
        hidden = NULL;
    } else {
        /* Linear projection */
        for (uint32_t j = 0; j < proj->output_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && proj->output_dim > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(j + 1) / (float)proj->output_dim);
            }

            float sum = proj->bias ? proj->bias[j] : 0.0f;
            for (uint32_t i = 0; i < proj->input_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && proj->input_dim > 256) {
                    jepa_multimodal_heartbeat("jepa_multimo_loop",
                                     (float)(i + 1) / (float)proj->input_dim);
                }

                sum += input[i] * proj->weights[i * proj->output_dim + j];
            }
            output[j] = sum;
        }
    }

    if (proj->use_layer_norm) {
        layer_normalize(output, proj->output_dim);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Functions - Math
 * ============================================================================ */

static float gelu_activation(float x)
{
    float x3 = x * x * x;
    float inner = 0.7978845608f * (x + GELU_CONST * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

static void layer_normalize(float* data, uint32_t dim)
{
    if (!data || dim == 0) return;

    float mean = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)dim);
        }

        mean += data[i];
    }
    mean /= (float)dim;

    float variance = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)dim);
        }

        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)dim;

    float std_inv = 1.0f / sqrtf(variance + 1e-5f);
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)dim);
        }

        data[i] = (data[i] - mean) * std_inv;
    }
}

static float cosine_similarity(const float* a, const float* b, uint32_t dim)
{
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_multimodal_heartbeat("jepa_multimo_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    return denom > 1e-8f ? dot / denom : 0.0f;
}

/* ============================================================================
 * KG Self-Awareness API
 * ============================================================================ */

int jepa_multimodal_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_multimodal_heartbeat("jepa_multimo_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Multimodal");
    if (self) {
        NIMCP_LOGGING_INFO("[%s] Self-knowledge entity: %s (type: %s)",
                          MULTIMODAL_LOG_TAG, self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_multimodal_heartbeat("jepa_multimo_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("[%s] Observation[%u]: %s",
                               MULTIMODAL_LOG_TAG, i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Multimodal");
    if (connections) {
        NIMCP_LOGGING_DEBUG("[%s] Outgoing connections: %u", MULTIMODAL_LOG_TAG, connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Multimodal");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("[%s] Incoming connections: %u", MULTIMODAL_LOG_TAG, incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_multimodal_set_instance_health_agent(void* instance,
                                                nimcp_health_agent_t* agent) {
    (void)instance;
    g_jepa_multimodal_health_agent = agent;
}

int jepa_multimodal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_multimodal_training_begin: NULL argument");
        return -1;
    }
    jepa_multimodal_heartbeat_instance(g_jepa_multimodal_health_agent,
                                        "jepa_multimo_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int jepa_multimodal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_multimodal_training_end: NULL argument");
        return -1;
    }
    jepa_multimodal_heartbeat_instance(g_jepa_multimodal_health_agent,
                                        "jepa_multimo_training_end", 1.0f);
    (void)instance;
    return 0;
}

int jepa_multimodal_training_step(void* instance, uint32_t step) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_multimodal_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_multimodal_heartbeat_instance(g_jepa_multimodal_health_agent,
                                        "jepa_multimo_training_step", progress);
    (void)instance;
    return 0;
}
