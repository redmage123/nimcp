/**
 * @file nimcp_speech_jepa_bridge.c
 * @brief Speech JEPA Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Bridge between Speech Cortex and JEPA latent embedding system
 * WHY:  Enable self-supervised speech learning via JEPA prediction
 * HOW:  Encode phoneme sequences → JEPA latents, predict masked segments
 *
 * @author NIMCP Development Team
 */

#include "perception/nimcp_speech_jepa_bridge.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_async.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_jepa_bridge)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define SPEECH_JEPA_LOG_TAG "SPEECH_JEPA"

/* GELU activation constants */
#define GELU_CONST              0.044715f
#define GELU_SQRT_2_OVER_PI     0.7978845608f   /* sqrt(2/pi) */

/* Default config values */
#define SPEECH_JEPA_DEFAULT_LATENT_DIM          256
#define SPEECH_JEPA_DEFAULT_NUM_LAYERS          2
#define SPEECH_JEPA_DEFAULT_MASK_RATIO          0.3f
#define SPEECH_JEPA_DEFAULT_MIN_MASK_LEN        3
#define SPEECH_JEPA_DEFAULT_MAX_MASK_LEN        10
#define SPEECH_JEPA_DEFAULT_PREDICTOR_LAYERS    2
#define SPEECH_JEPA_DEFAULT_LEARNING_RATE       NIMCP_LEARNING_RATE_MICRO
#define SPEECH_JEPA_DEFAULT_EMA_MOMENTUM        0.996f
#define SPEECH_JEPA_DEFAULT_PRECISION           1.0f

/* Stack buffer and dimension limits */
#define SPEECH_JEPA_MAX_ENCODER_DIM             512

/* Feature normalization constants */
#define SPEECH_JEPA_PHONEME_NORM_FACTOR         64.0f
#define SPEECH_JEPA_FORMANT_MAX_FREQ_HZ         5000.0f
#define SPEECH_JEPA_PITCH_MAX_FREQ_HZ           400.0f
#define SPEECH_JEPA_DURATION_NORM_MS            100.0f

/* Phoneme vocabulary */
#define SPEECH_JEPA_PHONEME_COUNT               64

/* Training parameters */
#define SPEECH_JEPA_MIN_TRAIN_SEQUENCE_LEN      4

/* Masking threshold */
#define SPEECH_JEPA_MASK_THRESHOLD              0.5f

/* Decoding constants */
#define SPEECH_JEPA_CONFIDENCE_SCALE            10.0f
#define SPEECH_JEPA_NEG_INF                     (-1e10f)

/* Layer normalization epsilon */
#define SPEECH_JEPA_LAYER_NORM_EPSILON          1e-5f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int speech_encoder_create(
    speech_jepa_encoder_t** encoder,
    const speech_jepa_encoder_config_t* config
);
static void speech_encoder_destroy(speech_jepa_encoder_t* encoder);
static int speech_encoder_forward(
    speech_jepa_encoder_t* encoder,
    const float* input,
    float* output
);
static int speech_encoder_copy(
    speech_jepa_encoder_t* dst,
    const speech_jepa_encoder_t* src
);
static void speech_encoder_ema_update(
    speech_jepa_encoder_t* target,
    const speech_jepa_encoder_t* online,
    float momentum
);

static float gelu_activation(float x);
static void layer_normalize(float* data, uint32_t dim);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int speech_jepa_bridge_default_config(speech_jepa_bridge_config_t* config)
{
    if (!config) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(*config));

    /* Encoder config */
    config->encoder.type = SPEECH_JEPA_ENCODER_MLP;
    config->encoder.input_dim = SPEECH_JEPA_FRAME_FEATURES;
    config->encoder.hidden_dim = SPEECH_JEPA_DEFAULT_ENCODER_DIM;
    config->encoder.output_dim = SPEECH_JEPA_DEFAULT_LATENT_DIM;
    config->encoder.num_layers = SPEECH_JEPA_DEFAULT_NUM_LAYERS;
    config->encoder.features = SPEECH_JEPA_FEAT_ALL;
    config->encoder.use_positional_encoding = true;
    config->encoder.use_layer_norm = true;

    /* Sequence config */
    config->frame_duration_ms = SPEECH_JEPA_DEFAULT_FRAME_MS;
    config->sequence_length = SPEECH_JEPA_DEFAULT_SEQUENCE_LEN;
    config->sequence_stride = SPEECH_JEPA_DEFAULT_SEQUENCE_LEN / 2;

    /* Masking config */
    config->mask_strategy = SPEECH_JEPA_MASK_TEMPORAL;
    config->mask_ratio = SPEECH_JEPA_DEFAULT_MASK_RATIO;
    config->min_mask_len = SPEECH_JEPA_DEFAULT_MIN_MASK_LEN;
    config->max_mask_len = SPEECH_JEPA_DEFAULT_MAX_MASK_LEN;

    /* Predictor config */
    config->predictor.type = JEPA_PREDICTOR_MLP;
    config->predictor.input_dim = config->encoder.output_dim;
    config->predictor.hidden_dim = config->encoder.hidden_dim;
    config->predictor.output_dim = config->encoder.output_dim;
    config->predictor.num_layers = SPEECH_JEPA_DEFAULT_PREDICTOR_LAYERS;
    config->predictor.dropout_rate = 0.0f;
    config->predictor.enable_layer_norm = true;
    config->predictor.activation = JEPA_ACT_GELU;

    /* Training parameters */
    config->learning_rate = SPEECH_JEPA_DEFAULT_LEARNING_RATE;
    config->momentum = SPEECH_JEPA_DEFAULT_EMA_MOMENTUM;
    config->use_target_encoder = true;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

speech_jepa_bridge_t* speech_jepa_bridge_create(
    const speech_jepa_bridge_config_t* config)
{
    speech_jepa_bridge_t* bridge = NULL;
    speech_jepa_bridge_config_t default_config;
    int rc;

    /* Use defaults if not provided */
    if (!config) {
        speech_jepa_bridge_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    bridge = (speech_jepa_bridge_t*)nimcp_calloc(1, sizeof(speech_jepa_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate speech JEPA bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate speech JEPA bridge");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SPEECH_JEPA, "speech_jepa") != 0) {
        nimcp_free(bridge);
        return NULL;
    }
    bridge->base.bridge_active = false;

    /* P1 fix: Validate output_dim fits in stack encoding buffers */
    if (config->encoder.output_dim > SPEECH_JEPA_MAX_ENCODER_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "speech_jepa_bridge_create: encoder.output_dim %u exceeds max %u",
            config->encoder.output_dim, SPEECH_JEPA_MAX_ENCODER_DIM);
        nimcp_free(bridge);
        return NULL;
    }

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(speech_jepa_bridge_config_t));

    /* Create encoder */
    rc = speech_encoder_create(&bridge->encoder, &config->encoder);
    if (rc != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create encoder: %d", rc);
        NIMCP_LOGGING_ERROR("Failed to create encoder: %d", rc);
        goto cleanup;
    }

    /* Create target encoder if enabled */
    if (config->use_target_encoder) {
        rc = speech_encoder_create(&bridge->target_encoder, &config->encoder);
        if (rc != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create target encoder: %d", rc);
            NIMCP_LOGGING_ERROR("Failed to create target encoder: %d", rc);
            goto cleanup;
        }
        /* Copy initial weights from online encoder */
        rc = speech_encoder_copy(bridge->target_encoder, bridge->encoder);
        if (rc != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to copy encoder weights");
            goto cleanup;
        }
    }

    /* Create predictor */
    bridge->predictor = jepa_predictor_create(&config->predictor);
    if (!bridge->predictor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create predictor");
        NIMCP_LOGGING_ERROR("Failed to create predictor");
        goto cleanup;
    }

    /* Allocate working buffers */
    bridge->frame_buffer = (float*)nimcp_calloc(
        SPEECH_JEPA_FRAME_FEATURES, sizeof(float));
    if (!bridge->frame_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate frame buffer");
        goto cleanup;
    }

    bridge->encoding_buffer = (float*)nimcp_calloc(
        config->encoder.output_dim, sizeof(float));
    if (!bridge->encoding_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate encoding buffer");
        goto cleanup;
    }

    /* Create current sequence buffer */
    bridge->current_sequence = speech_jepa_sequence_create(config->sequence_length);
    if (!bridge->current_sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create current sequence");
        goto cleanup;
    }

    /* Initialize training state */
    bridge->training_mode = false;
    bridge->training_step = 0;
    bridge->ema_decay = config->momentum;

    /* Mark as initialized */
    bridge->base.bridge_active = true;

    NIMCP_LOGGING_INFO("Created speech JEPA bridge: latent_dim=%u, sequence_len=%u",
        config->encoder.output_dim, config->sequence_length);

    return bridge;

cleanup:
    speech_jepa_bridge_destroy(bridge);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_jepa_bridge_create: operation failed");
    return NULL;
}

void speech_jepa_bridge_destroy(speech_jepa_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async */
    speech_jepa_bridge_disconnect_bio_async(bridge);

    /* Free components */
    speech_encoder_destroy(bridge->encoder);
    speech_encoder_destroy(bridge->target_encoder);

    if (bridge->predictor) {
        jepa_predictor_destroy(bridge->predictor);
    }

    /* Free buffers */
    nimcp_free(bridge->frame_buffer);
    nimcp_free(bridge->encoding_buffer);

    /* Free sequence */
    speech_jepa_sequence_destroy(bridge->current_sequence);

    /* Cleanup bridge base (mutex, bio-async context) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
}

int speech_jepa_bridge_reset(speech_jepa_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_jepa_bridge_reset");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clear sequence */
    speech_jepa_sequence_clear(bridge->current_sequence);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset training state */
    bridge->training_step = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int speech_jepa_bridge_connect_speech_cortex(
    speech_jepa_bridge_t* bridge,
    speech_cortex_t* speech)
{
    if (!bridge || !speech) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_jepa_bridge_connect_speech_cortex");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (bridge->speech_cortex) {
        NIMCP_LOGGING_WARN("Already connected to speech cortex");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    bridge->speech_cortex = speech;
    bridge->base.system_a_connected = true;

    NIMCP_LOGGING_INFO("Connected to speech cortex");

    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_disconnect_speech_cortex(speech_jepa_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_jepa_bridge_disconnect_speech_cortex");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    bridge->speech_cortex = NULL;
    bridge->base.system_a_connected = false;

    return NIMCP_SUCCESS;
}

bool speech_jepa_bridge_is_connected(const speech_jepa_bridge_t* bridge)
{
    return bridge && bridge->speech_cortex != NULL;
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int speech_jepa_bridge_extract_features(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_frame_t* frame,
    float* features,
    uint32_t feature_dim)
{
    if (!bridge || !frame || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_jepa_bridge_extract_features");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (feature_dim < SPEECH_JEPA_FRAME_FEATURES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Buffer too small in speech_jepa_bridge_extract_features");
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    uint32_t idx = 0;
    speech_jepa_feature_flags_t flags = bridge->config.encoder.features;

    /* Clear feature buffer */
    memset(features, 0, feature_dim * sizeof(float));

    /* Phoneme features (one-hot encoding, simplified) */
    if (flags & SPEECH_JEPA_FEAT_PHONEME) {
        /* Use phoneme index directly, normalized */
        features[idx++] = (float)frame->phoneme / SPEECH_JEPA_PHONEME_NORM_FACTOR;
        features[idx++] = frame->phoneme_confidence;
        features[idx++] = frame->is_voiced ? 1.0f : 0.0f;
    } else {
        idx += 3;
    }

    /* Formant features */
    if (flags & SPEECH_JEPA_FEAT_FORMANT) {
        for (uint32_t i = 0; i < SPEECH_NUM_FORMANTS && idx < feature_dim; i++) {
            /* Normalize formant frequencies (typical range ~200-5000 Hz) */
            features[idx++] = frame->formants[i] / SPEECH_JEPA_FORMANT_MAX_FREQ_HZ;
        }
    } else {
        idx += SPEECH_NUM_FORMANTS;
    }

    /* Prosody features */
    if (flags & SPEECH_JEPA_FEAT_PROSODY) {
        /* Pitch (F0): typical range 80-400 Hz */
        features[idx++] = frame->pitch / SPEECH_JEPA_PITCH_MAX_FREQ_HZ;
        /* Energy: already normalized */
        features[idx++] = frame->energy;
        /* Duration: normalize to typical frame duration */
        features[idx++] = frame->duration_ms / SPEECH_JEPA_DURATION_NORM_MS;
    } else {
        idx += 3;
    }

    /* Pad remaining features with zeros */
    while (idx < feature_dim) {
        features[idx++] = 0.0f;
    }

    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_encode(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    jepa_latent_t* latent)
{
    if (!bridge || !sequence || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_jepa_bridge_encode");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (sequence->num_frames == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Empty sequence in speech_jepa_bridge_encode");
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint32_t latent_dim = bridge->config.encoder.output_dim;
    float* pooled = bridge->encoding_buffer;
    memset(pooled, 0, latent_dim * sizeof(float));

    /* Encode each frame and pool */
    for (uint32_t i = 0; i < sequence->num_frames; i++) {
        /* Extract features */
        int rc = speech_jepa_bridge_extract_features(
            bridge,
            &sequence->frames[i],
            bridge->frame_buffer,
            SPEECH_JEPA_FRAME_FEATURES
        );
        if (rc != NIMCP_SUCCESS) {
            return rc;
        }

        /* Encode frame */
        float frame_encoding[512];  /* Max latent dim */
        rc = speech_encoder_forward(
            bridge->encoder,
            bridge->frame_buffer,
            frame_encoding
        );
        if (rc != NIMCP_SUCCESS) {
            return rc;
        }

        /* Accumulate for mean pooling */
        for (uint32_t j = 0; j < latent_dim; j++) {
            pooled[j] += frame_encoding[j];
        }
    }

    /* Mean pooling */
    float scale = 1.0f / (float)sequence->num_frames;
    for (uint32_t j = 0; j < latent_dim; j++) {
        pooled[j] *= scale;
    }

    /* Initialize latent embedding if needed */
    if (!latent->embedding) {
        latent->embedding = (float*)nimcp_calloc(latent_dim, sizeof(float));
        if (!latent->embedding) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate latent embedding in speech_jepa_bridge_encode");
            return NIMCP_ERROR_NO_MEMORY;
        }
        latent->latent_dim = latent_dim;
    }

    /* Copy to latent */
    memcpy(latent->embedding, pooled, latent_dim * sizeof(float));
    latent->precision = SPEECH_JEPA_DEFAULT_PRECISION;
    latent->modality = JEPA_MODALITY_SPEECH;
    latent->timestamp_ms = sequence->end_time_ms;

    /* Update stats */
    bridge->stats.sequences_processed++;
    bridge->stats.frames_processed += sequence->num_frames;

    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_encode_phonemes(
    speech_jepa_bridge_t* bridge,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    jepa_latent_t* latent)
{
    if (!bridge || !phonemes || !latent || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL or invalid parameter in speech_jepa_bridge_encode_phonemes");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Create temporary sequence */
    speech_jepa_sequence_t* seq = speech_jepa_sequence_create(num_phonemes);
    if (!seq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create sequence in speech_jepa_bridge_encode_phonemes");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Create frames from phonemes */
    for (uint32_t i = 0; i < num_phonemes; i++) {
        speech_jepa_frame_t frame = {0};
        frame.phoneme = phonemes[i];
        frame.phoneme_confidence = 1.0f;
        frame.is_voiced = true;
        frame.timestamp_ms = i * bridge->config.frame_duration_ms;
        frame.duration_ms = (float)bridge->config.frame_duration_ms;

        speech_jepa_sequence_add_frame(seq, &frame);
    }

    /* Encode sequence */
    int rc = speech_jepa_bridge_encode(bridge, seq, latent);

    /* Cleanup */
    speech_jepa_sequence_destroy(seq);

    return rc;
}

int speech_jepa_bridge_encode_frames(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    jepa_latent_t** frame_latents,
    uint32_t* num_latents)
{
    if (!bridge || !sequence || !frame_latents || !num_latents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_jepa_bridge_encode_frames");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (sequence->num_frames == 0) {
        *num_latents = 0;
        return NIMCP_SUCCESS;
    }

    uint32_t latent_dim = bridge->config.encoder.output_dim;
    *num_latents = sequence->num_frames;

    /* Encode each frame */
    for (uint32_t i = 0; i < sequence->num_frames; i++) {
        /* Extract features */
        int rc = speech_jepa_bridge_extract_features(
            bridge,
            &sequence->frames[i],
            bridge->frame_buffer,
            SPEECH_JEPA_FRAME_FEATURES
        );
        if (rc != NIMCP_SUCCESS) {
            return rc;
        }

        /* Encode frame */
        float encoding[SPEECH_JEPA_MAX_ENCODER_DIM];
        rc = speech_encoder_forward(
            bridge->encoder,
            bridge->frame_buffer,
            encoding
        );
        if (rc != NIMCP_SUCCESS) {
            return rc;
        }

        /* Initialize latent if needed */
        if (!frame_latents[i]) {
            frame_latents[i] = jepa_latent_create_dim(latent_dim);
            if (!frame_latents[i]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create frame latent in speech_jepa_bridge_encode_frames");
                return NIMCP_ERROR_NO_MEMORY;
            }
        }

        /* Copy encoding */
        memcpy(frame_latents[i]->embedding, encoding, latent_dim * sizeof(float));
        frame_latents[i]->modality = JEPA_MODALITY_SPEECH;
        frame_latents[i]->timestamp_ms = sequence->frames[i].timestamp_ms;
    }

    bridge->stats.frames_processed += sequence->num_frames;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int speech_jepa_bridge_set_training(speech_jepa_bridge_t* bridge, bool training)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge in speech_jepa_bridge_set_training");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    bridge->training_mode = training;

    if (bridge->predictor) {
        jepa_predictor_set_training(bridge->predictor, training);
    }

    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_train_step(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    float* loss)
{
    if (!bridge || !sequence || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL parameter in speech_jepa_bridge_train_step");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!bridge->training_mode) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    if (sequence->num_frames < SPEECH_JEPA_MIN_TRAIN_SEQUENCE_LEN) {
        *loss = 0.0f;
        return NIMCP_SUCCESS;  /* Sequence too short */
    }

    uint32_t latent_dim = bridge->config.encoder.output_dim;
    int rc;

    /* 1. Generate mask based on strategy */
    uint32_t num_masked = (uint32_t)(sequence->num_frames * bridge->config.mask_ratio);
    if (num_masked < 1) num_masked = 1;
    if (num_masked >= sequence->num_frames) num_masked = sequence->num_frames - 1;

    bool* mask = (bool*)nimcp_calloc(sequence->num_frames, sizeof(bool));
    if (!mask) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mask in speech_jepa_bridge_train_step");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Apply masking strategy */
    switch (bridge->config.mask_strategy) {
        case SPEECH_JEPA_MASK_TEMPORAL: {
            /* Mask contiguous segment */
            uint32_t mask_start = nimcp_rand_uint(sequence->num_frames - num_masked);
            for (uint32_t i = 0; i < num_masked; i++) {
                mask[mask_start + i] = true;
            }
            break;
        }
        case SPEECH_JEPA_MASK_RANDOM: {
            /* Random frame dropout */
            uint32_t masked = 0;
            while (masked < num_masked) {
                uint32_t idx = nimcp_rand_uint(sequence->num_frames);
                if (!mask[idx]) {
                    mask[idx] = true;
                    masked++;
                }
            }
            break;
        }
        case SPEECH_JEPA_MASK_CAUSAL: {
            /* Mask future frames */
            uint32_t mask_start = sequence->num_frames - num_masked;
            for (uint32_t i = mask_start; i < sequence->num_frames; i++) {
                mask[i] = true;
            }
            break;
        }
        default:
            /* Default to temporal masking */
            uint32_t mask_start = nimcp_rand_uint(sequence->num_frames - num_masked);
            for (uint32_t i = 0; i < num_masked; i++) {
                mask[mask_start + i] = true;
            }
            break;
    }

    /* 2. Encode visible frames with online encoder */
    float* context_sum = (float*)nimcp_calloc(latent_dim, sizeof(float));
    uint32_t num_visible = 0;

    for (uint32_t i = 0; i < sequence->num_frames; i++) {
        if (!mask[i]) {
            /* Extract features */
            rc = speech_jepa_bridge_extract_features(
                bridge,
                &sequence->frames[i],
                bridge->frame_buffer,
                SPEECH_JEPA_FRAME_FEATURES
            );
            if (rc != NIMCP_SUCCESS) {
                goto cleanup;
            }

            /* Encode frame */
            float encoding[SPEECH_JEPA_MAX_ENCODER_DIM];
            rc = speech_encoder_forward(bridge->encoder, bridge->frame_buffer, encoding);
            if (rc != NIMCP_SUCCESS) {
                goto cleanup;
            }

            /* Accumulate */
            for (uint32_t j = 0; j < latent_dim; j++) {
                context_sum[j] += encoding[j];
            }
            num_visible++;
        }
    }

    if (num_visible == 0) {
        *loss = 0.0f;
        rc = NIMCP_SUCCESS;
        goto cleanup;
    }

    /* Mean pool context */
    for (uint32_t j = 0; j < latent_dim; j++) {
        context_sum[j] /= (float)num_visible;
    }

    /* 3. Encode masked frames with target encoder (stop gradient) */
    float* target_sum = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!target_sum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate target_sum in speech_jepa_bridge_train_step");
        rc = NIMCP_ERROR_NO_MEMORY;
        goto cleanup;
    }
    speech_jepa_encoder_t* target_enc = bridge->target_encoder ?
                                         bridge->target_encoder : bridge->encoder;

    for (uint32_t i = 0; i < sequence->num_frames; i++) {
        if (mask[i]) {
            rc = speech_jepa_bridge_extract_features(
                bridge,
                &sequence->frames[i],
                bridge->frame_buffer,
                SPEECH_JEPA_FRAME_FEATURES
            );
            if (rc != NIMCP_SUCCESS) {
                nimcp_free(target_sum);
                goto cleanup;
            }

            float encoding[SPEECH_JEPA_MAX_ENCODER_DIM];
            rc = speech_encoder_forward(target_enc, bridge->frame_buffer, encoding);
            if (rc != NIMCP_SUCCESS) {
                nimcp_free(target_sum);
                goto cleanup;
            }

            for (uint32_t j = 0; j < latent_dim; j++) {
                target_sum[j] += encoding[j];
            }
        }
    }

    /* Mean pool target */
    for (uint32_t j = 0; j < latent_dim; j++) {
        target_sum[j] /= (float)num_masked;
    }

    /* 4. Predict target from context */
    jepa_latent_t context_latent = {0};
    context_latent.embedding = (float*)nimcp_calloc(latent_dim, sizeof(float));
    context_latent.latent_dim = latent_dim;
    if (!context_latent.embedding) {
        nimcp_free(target_sum);
        rc = NIMCP_ERROR_NO_MEMORY;
        goto cleanup;
    }
    memcpy(context_latent.embedding, context_sum, latent_dim * sizeof(float));

    jepa_latent_t predicted_latent = {0};
    predicted_latent.embedding = (float*)nimcp_calloc(latent_dim, sizeof(float));
    predicted_latent.latent_dim = latent_dim;
    if (!predicted_latent.embedding) {
        nimcp_free(context_latent.embedding);
        nimcp_free(target_sum);
        rc = NIMCP_ERROR_NO_MEMORY;
        goto cleanup;
    }

    rc = jepa_predictor_predict(bridge->predictor, &context_latent, &predicted_latent);
    if (rc != NIMCP_SUCCESS) {
        nimcp_free(context_latent.embedding);
        nimcp_free(predicted_latent.embedding);
        nimcp_free(target_sum);
        goto cleanup;
    }

    /* 5. Compute L2 loss in latent space */
    float total_loss = 0.0f;
    for (uint32_t j = 0; j < latent_dim; j++) {
        float diff = predicted_latent.embedding[j] - target_sum[j];
        total_loss += diff * diff;
    }
    *loss = total_loss / (float)latent_dim;

    /* 6. Update target encoder with EMA */
    if (bridge->target_encoder) {
        speech_encoder_ema_update(
            bridge->target_encoder,
            bridge->encoder,
            bridge->ema_decay
        );
    }

    /* Update stats */
    bridge->training_step++;
    bridge->stats.predictions_made++;
    float new_avg_pred_loss =
        (bridge->stats.avg_prediction_loss * (bridge->training_step - 1) + *loss) /
        bridge->training_step;
    if (isfinite(new_avg_pred_loss)) bridge->stats.avg_prediction_loss = new_avg_pred_loss;
    if (*loss < bridge->stats.min_loss || bridge->training_step == 1) {
        bridge->stats.min_loss = *loss;
    }

    /* Cleanup */
    nimcp_free(context_latent.embedding);
    nimcp_free(predicted_latent.embedding);
    nimcp_free(target_sum);
    rc = NIMCP_SUCCESS;

cleanup:
    nimcp_free(context_sum);
    nimcp_free(mask);
    return rc;
}

int speech_jepa_bridge_update_target_encoder(speech_jepa_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in speech_jepa_bridge_update_target_encoder");

    if (!bridge->target_encoder) {
        return NIMCP_SUCCESS;  /* No target encoder */
    }

    speech_encoder_ema_update(
        bridge->target_encoder,
        bridge->encoder,
        bridge->ema_decay
    );

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int speech_jepa_bridge_predict_next(
    speech_jepa_bridge_t* bridge,
    const jepa_latent_t* context,
    jepa_latent_t* predicted_latent)
{
    NIMCP_CHECK_THROW(bridge && context && predicted_latent, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_jepa_bridge_predict_next");

    int rc = jepa_predictor_predict(bridge->predictor, context, predicted_latent);
    if (rc == NIMCP_SUCCESS) {
        predicted_latent->modality = JEPA_MODALITY_SPEECH;
        bridge->stats.predictions_made++;
    }

    return rc;
}

int speech_jepa_bridge_predict_masked(
    speech_jepa_bridge_t* bridge,
    jepa_latent_t** context_latents,
    uint32_t num_context,
    const jepa_mask_t* mask,
    jepa_latent_t** predictions,
    uint32_t* num_predictions)
{
    NIMCP_CHECK_THROW(bridge && context_latents && mask && predictions && num_predictions, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_jepa_bridge_predict_masked");

    if (num_context == 0) {
        *num_predictions = 0;
        return NIMCP_SUCCESS;
    }

    uint32_t latent_dim = bridge->config.encoder.output_dim;

    /* Pool context latents */
    jepa_latent_t pooled_context = {0};
    pooled_context.embedding = (float*)nimcp_calloc(latent_dim, sizeof(float));
    pooled_context.latent_dim = latent_dim;
    NIMCP_CHECK_THROW(pooled_context.embedding, NIMCP_ERROR_NO_MEMORY, "Failed to allocate pooled_context.embedding in speech_jepa_bridge_predict_masked");

    for (uint32_t i = 0; i < num_context; i++) {
        if (context_latents[i]) {
            for (uint32_t j = 0; j < latent_dim; j++) {
                pooled_context.embedding[j] += context_latents[i]->embedding[j];
            }
        }
    }
    for (uint32_t j = 0; j < latent_dim; j++) {
        pooled_context.embedding[j] /= (float)num_context;
    }

    /* Count masked positions */
    uint32_t masked_count = 0;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        if (mask->data[i] > SPEECH_JEPA_MASK_THRESHOLD) {
            masked_count++;
        }
    }

    *num_predictions = masked_count;

    /* Predict each masked position */
    uint32_t pred_idx = 0;
    for (uint32_t i = 0; i < mask->total_size && pred_idx < masked_count; i++) {
        if (mask->data[i] > SPEECH_JEPA_MASK_THRESHOLD) {
            if (!predictions[pred_idx]) {
                predictions[pred_idx] = jepa_latent_create_dim(latent_dim);
            }
            if (predictions[pred_idx]) {
                jepa_predictor_predict(bridge->predictor, &pooled_context, predictions[pred_idx]);
                predictions[pred_idx]->modality = JEPA_MODALITY_SPEECH;
            }
            pred_idx++;
        }
    }

    nimcp_free(pooled_context.embedding);
    bridge->stats.predictions_made += masked_count;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Sequence Management
 * ============================================================================ */

speech_jepa_sequence_t* speech_jepa_sequence_create(uint32_t max_frames)
{
    if (max_frames == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_jepa_sequence_create: max_frames is zero");
        return NULL;
    }

    speech_jepa_sequence_t* seq = (speech_jepa_sequence_t*)nimcp_calloc(
        1, sizeof(speech_jepa_sequence_t));
    if (!seq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_jepa_sequence_create: seq is NULL");
        return NULL;
    }

    seq->frames = (speech_jepa_frame_t*)nimcp_calloc(
        max_frames, sizeof(speech_jepa_frame_t));
    if (!seq->frames) {
        nimcp_free(seq);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_jepa_sequence_create: seq->frames is NULL");
        return NULL;
    }

    seq->max_frames = max_frames;
    seq->num_frames = 0;

    return seq;
}

void speech_jepa_sequence_destroy(speech_jepa_sequence_t* sequence)
{
    if (!sequence) {
        return;
    }
    nimcp_free(sequence->frames);
    nimcp_free(sequence);
}

int speech_jepa_sequence_add_frame(
    speech_jepa_sequence_t* sequence,
    const speech_jepa_frame_t* frame)
{
    NIMCP_CHECK_THROW(sequence && frame, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_jepa_sequence_add_frame");

    NIMCP_CHECK_THROW(sequence->num_frames < sequence->max_frames, NIMCP_ERROR_BUFFER_TOO_SMALL, "Sequence buffer full in speech_jepa_sequence_add_frame");

    memcpy(&sequence->frames[sequence->num_frames], frame, sizeof(speech_jepa_frame_t));

    /* Update timestamps */
    if (sequence->num_frames == 0) {
        sequence->start_time_ms = frame->timestamp_ms;
    }
    sequence->end_time_ms = frame->timestamp_ms;

    sequence->num_frames++;

    return NIMCP_SUCCESS;
}

int speech_jepa_sequence_clear(speech_jepa_sequence_t* sequence)
{
    NIMCP_CHECK_THROW(sequence, NIMCP_ERROR_INVALID_PARAM, "NULL sequence in speech_jepa_sequence_clear");

    sequence->num_frames = 0;
    sequence->start_time_ms = 0;
    sequence->end_time_ms = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int speech_jepa_bridge_get_stats(
    const speech_jepa_bridge_t* bridge,
    speech_jepa_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_jepa_bridge_get_stats");

    memcpy(stats, &bridge->stats, sizeof(speech_jepa_stats_t));
    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_reset_stats(speech_jepa_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in speech_jepa_bridge_reset_stats");

    memset(&bridge->stats, 0, sizeof(speech_jepa_stats_t));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int speech_jepa_bridge_connect_bio_async(speech_jepa_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in speech_jepa_bridge_connect_bio_async");

    /* Register with bio-async system */
    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_DEBUG("%s: Connected to bio-async router", SPEECH_JEPA_LOG_TAG);

    return NIMCP_SUCCESS;
}

int speech_jepa_bridge_disconnect_bio_async(speech_jepa_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in speech_jepa_bridge_disconnect_bio_async");

    bridge->base.bio_async_enabled = false;

    return NIMCP_SUCCESS;
}

bool speech_jepa_bridge_is_bio_async_connected(const speech_jepa_bridge_t* bridge)
{
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

void speech_jepa_phoneme_to_onehot(phoneme_t phoneme, float* encoding)
{
    if (!encoding) {
        return;
    }

    const uint32_t num_phonemes = SPEECH_JEPA_PHONEME_COUNT;
    memset(encoding, 0, num_phonemes * sizeof(float));

    if (phoneme < num_phonemes) {
        encoding[phoneme] = 1.0f;
    }
}

int speech_jepa_decode_phoneme(
    speech_jepa_bridge_t* bridge,
    const jepa_latent_t* latent,
    phoneme_t* phoneme,
    float* confidence)
{
    NIMCP_CHECK_THROW(bridge && latent && phoneme && confidence, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_jepa_decode_phoneme");

    /* Simple approach: find nearest phoneme embedding */
    /* This is a placeholder - real implementation would use a learned decoder */

    /* For now, return most confident based on embedding magnitude */
    float max_val = SPEECH_JEPA_NEG_INF;
    uint32_t max_idx = 0;

    uint32_t check_dim = latent->latent_dim < SPEECH_JEPA_PHONEME_COUNT ? latent->latent_dim : SPEECH_JEPA_PHONEME_COUNT;
    for (uint32_t i = 0; i < check_dim; i++) {
        if (latent->embedding[i] > max_val) {
            max_val = latent->embedding[i];
            max_idx = i;
        }
    }

    *phoneme = (phoneme_t)max_idx;
    *confidence = max_val > 0 ? max_val / SPEECH_JEPA_CONFIDENCE_SCALE : 0.0f;
    if (*confidence > 1.0f) *confidence = 1.0f;
    if (*confidence < 0.0f) *confidence = 0.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Functions - Encoder
 * ============================================================================ */

/**
 * @brief GELU activation function
 */
static float gelu_activation(float x)
{
    /* GELU(x) = x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))) */
    float x3 = x * x * x;
    float inner = GELU_SQRT_2_OVER_PI * (x + GELU_CONST * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

/**
 * @brief Layer normalization (in-place)
 */
static void layer_normalize(float* data, uint32_t dim)
{
    if (!data || dim == 0) return;

    /* Compute mean */
    float mean = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        mean += data[i];
    }
    mean /= (float)dim;

    /* Compute variance */
    float variance = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)dim;

    /* Normalize */
    float std_inv = 1.0f / sqrtf(variance + SPEECH_JEPA_LAYER_NORM_EPSILON);
    for (uint32_t i = 0; i < dim; i++) {
        data[i] = (data[i] - mean) * std_inv;
    }
}

/**
 * @brief Create speech encoder
 */
static int speech_encoder_create(
    speech_jepa_encoder_t** encoder,
    const speech_jepa_encoder_config_t* config)
{
    NIMCP_CHECK_THROW(encoder && config, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_encoder_create");

    speech_jepa_encoder_t* enc = (speech_jepa_encoder_t*)nimcp_calloc(
        1, sizeof(speech_jepa_encoder_t));
    NIMCP_CHECK_THROW(enc, NIMCP_ERROR_NO_MEMORY, "Failed to allocate speech_jepa_encoder_t");

    enc->input_dim = config->input_dim;
    enc->hidden_dim = config->hidden_dim;
    enc->output_dim = config->output_dim;

    /* Allocate layer 1 */
    enc->weights_1 = (float*)nimcp_calloc(
        config->input_dim * config->hidden_dim, sizeof(float));
    enc->bias_1 = (float*)nimcp_calloc(config->hidden_dim, sizeof(float));

    /* Allocate layer 2 */
    enc->weights_2 = (float*)nimcp_calloc(
        config->hidden_dim * config->output_dim, sizeof(float));
    enc->bias_2 = (float*)nimcp_calloc(config->output_dim, sizeof(float));

    if (!enc->weights_1 || !enc->bias_1 || !enc->weights_2 || !enc->bias_2) {
        speech_encoder_destroy(enc);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Xavier initialization */
    float scale1 = sqrtf(2.0f / (float)(config->input_dim + config->hidden_dim));
    float scale2 = sqrtf(2.0f / (float)(config->hidden_dim + config->output_dim));

    for (uint32_t i = 0; i < config->input_dim * config->hidden_dim; i++) {
        enc->weights_1[i] = (nimcp_rand_uniform() * 2.0f - 1.0f) * scale1;
    }
    for (uint32_t i = 0; i < config->hidden_dim * config->output_dim; i++) {
        enc->weights_2[i] = (nimcp_rand_uniform() * 2.0f - 1.0f) * scale2;
    }

    *encoder = enc;
    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy speech encoder
 */
static void speech_encoder_destroy(speech_jepa_encoder_t* encoder)
{
    if (!encoder) return;

    nimcp_free(encoder->weights_1);
    nimcp_free(encoder->bias_1);
    nimcp_free(encoder->weights_2);
    nimcp_free(encoder->bias_2);
    nimcp_free(encoder);
}

/**
 * @brief Forward pass through encoder
 */
static int speech_encoder_forward(
    speech_jepa_encoder_t* encoder,
    const float* input,
    float* output)
{
    NIMCP_CHECK_THROW(encoder && input && output, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_encoder_forward");

    /* Allocate hidden */
    float* hidden = (float*)nimcp_calloc(encoder->hidden_dim, sizeof(float));
    NIMCP_CHECK_THROW(hidden, NIMCP_ERROR_NO_MEMORY, "Failed to allocate hidden in speech_encoder_forward");

    /* Layer 1: hidden = GELU(input @ W1 + b1) */
    for (uint32_t j = 0; j < encoder->hidden_dim; j++) {
        float sum = encoder->bias_1[j];
        for (uint32_t i = 0; i < encoder->input_dim; i++) {
            sum += input[i] * encoder->weights_1[i * encoder->hidden_dim + j];
        }
        hidden[j] = gelu_activation(sum);
    }

    /* Layer norm on hidden */
    layer_normalize(hidden, encoder->hidden_dim);

    /* Layer 2: output = hidden @ W2 + b2 */
    for (uint32_t j = 0; j < encoder->output_dim; j++) {
        float sum = encoder->bias_2[j];
        for (uint32_t i = 0; i < encoder->hidden_dim; i++) {
            sum += hidden[i] * encoder->weights_2[i * encoder->output_dim + j];
        }
        output[j] = sum;
    }

    /* Layer norm on output */
    layer_normalize(output, encoder->output_dim);

    nimcp_free(hidden);
    return NIMCP_SUCCESS;
}

/**
 * @brief Copy encoder weights
 */
static int speech_encoder_copy(
    speech_jepa_encoder_t* dst,
    const speech_jepa_encoder_t* src)
{
    NIMCP_CHECK_THROW(dst && src, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in speech_encoder_copy");

    NIMCP_CHECK_THROW(dst->input_dim == src->input_dim &&
        dst->hidden_dim == src->hidden_dim &&
        dst->output_dim == src->output_dim, NIMCP_ERROR_DIMENSION_MISMATCH, "Encoder dimension mismatch in speech_encoder_copy");

    memcpy(dst->weights_1, src->weights_1,
           src->input_dim * src->hidden_dim * sizeof(float));
    memcpy(dst->bias_1, src->bias_1, src->hidden_dim * sizeof(float));
    memcpy(dst->weights_2, src->weights_2,
           src->hidden_dim * src->output_dim * sizeof(float));
    memcpy(dst->bias_2, src->bias_2, src->output_dim * sizeof(float));

    return NIMCP_SUCCESS;
}

/**
 * @brief EMA update for target encoder
 */
static void speech_encoder_ema_update(
    speech_jepa_encoder_t* target,
    const speech_jepa_encoder_t* online,
    float momentum)
{
    if (!target || !online) return;

    float one_minus_m = 1.0f - momentum;

    /* Update weights_1 */
    for (uint32_t i = 0; i < target->input_dim * target->hidden_dim; i++) {
        target->weights_1[i] = momentum * target->weights_1[i] +
                               one_minus_m * online->weights_1[i];
    }

    /* Update bias_1 */
    for (uint32_t i = 0; i < target->hidden_dim; i++) {
        target->bias_1[i] = momentum * target->bias_1[i] +
                            one_minus_m * online->bias_1[i];
    }

    /* Update weights_2 */
    for (uint32_t i = 0; i < target->hidden_dim * target->output_dim; i++) {
        target->weights_2[i] = momentum * target->weights_2[i] +
                               one_minus_m * online->weights_2[i];
    }

    /* Update bias_2 */
    for (uint32_t i = 0; i < target->output_dim; i++) {
        target->bias_2[i] = momentum * target->bias_2[i] +
                            one_minus_m * online->bias_2[i];
    }
}
