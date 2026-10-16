#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_perception_bridge.c - Language-Perception Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_perception_bridge.c
 * @brief Implementation of bidirectional Language-Perception bridge
 *
 * WHAT: Bridge connecting language processing with perceptual input systems
 * WHY:  Enable speech/audio/visual perception to feed language comprehension
 * HOW:  Speech cortex → phonemes → Wernicke; Visual cortex → text → reading
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_perception_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_PERCEPTION_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_perception_bridge)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize speech cortex data
 */
static int init_speech_data(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_speech_data: bridge is NULL");
        return -1;
    }

    language_speech_cortex_data_t* data = &bridge->speech_data;

    /* Allocate phoneme buffer */
    data->phoneme_buffer_size = LANGUAGE_PERCEPTION_DEFAULT_PHONEME_BUFFER_SIZE;

    data->phoneme_buffer = (language_phoneme_t*)nimcp_calloc(
        data->phoneme_buffer_size, sizeof(language_phoneme_t));
    if (!data->phoneme_buffer) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate phoneme buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_speech_data: data->phoneme_buffer is NULL");
        return -1;
    }

    data->phoneme_write_idx = 0;
    data->phoneme_read_idx = 0;
    data->phonemes_available = 0;
    data->detection = SPEECH_DETECTION_NONE;
    data->speech_probability = 0.0f;
    data->signal_quality = 1.0f;
    data->noise_level = 0.0f;
    data->prosody_valid = false;

    return 0;
}

/**
 * @brief Initialize audio cortex data
 */
static int init_audio_data(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_audio_data: bridge is NULL");
        return -1;
    }

    language_audio_cortex_data_t* data = &bridge->audio_data;

    data->spectral_dim = 128;  /* Default FFT bins */
    data->spectral_features = (float*)nimcp_calloc(data->spectral_dim, sizeof(float));
    if (!data->spectral_features) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate spectral features");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_audio_data: data->spectral_features is NULL");
        return -1;
    }

    data->audio_energy = 0.0f;
    data->voice_activity = 0.0f;
    data->attention_level = 0.5f;
    data->is_speech_focused = true;

    return 0;
}

/**
 * @brief Initialize visual cortex data
 */
static int init_visual_data(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_visual_data: bridge is NULL");
        return -1;
    }

    language_visual_cortex_data_t* data = &bridge->visual_data;

    data->text_buffer_size = 1024;
    data->recognized_text = (char*)nimcp_calloc(data->text_buffer_size, sizeof(char));
    if (!data->recognized_text) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate text buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_visual_data: data->recognized_text is NULL");
        return -1;
    }

    data->viseme_dim = 32;  /* Visual phoneme features */
    data->viseme_features = (float*)nimcp_calloc(data->viseme_dim, sizeof(float));
    if (!data->viseme_features) {
        nimcp_free(data->recognized_text);
        data->recognized_text = NULL;
        LOG_ERROR(LOG_MODULE, "Failed to allocate viseme features");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_visual_data: data->viseme_features is NULL");
        return -1;
    }

    data->text_length = 0;
    data->text_confidence = 0.0f;
    data->lip_reading_active = false;
    data->lip_reading_confidence = 0.0f;
    data->gaze_x = 0.5f;
    data->gaze_y = 0.5f;
    data->fixating_on_text = false;

    return 0;
}

/**
 * @brief Initialize multimodal data
 */
static int init_multimodal_data(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_data: bridge is NULL");
        return -1;
    }

    language_multimodal_data_t* data = &bridge->multimodal_data;

    data->fused_dim = 256;  /* Fused representation dimension */
    data->fused_features = (float*)nimcp_calloc(data->fused_dim, sizeof(float));
    if (!data->fused_features) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate fused features");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_data: data->fused_features is NULL");
        return -1;
    }

    data->binding_state = AV_BINDING_NONE;
    data->av_coherence = 0.0f;
    data->mcgurk_active = false;
    data->mcgurk_visual_weight = LANGUAGE_PERCEPTION_MCGURK_DEFAULT_WEIGHT;
    data->mcgurk_perceived_phoneme = 0;

    return 0;
}

/**
 * @brief Initialize attention data
 */
static int init_attention_data(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_attention_data: bridge is NULL");
        return -1;
    }

    language_perception_attention_t* data = &bridge->attention;

    data->num_features = 64;
    data->feature_weights = (float*)nimcp_calloc(data->num_features, sizeof(float));
    if (!data->feature_weights) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate feature weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_attention_data: data->feature_weights is NULL");
        return -1;
    }

    /* Initialize uniform attention */
    for (uint32_t i = 0; i < data->num_features; i++) {
        data->feature_weights[i] = 1.0f / (float)data->num_features;
    }

    data->num_predictions = 8;
    data->predicted_phonemes = (uint32_t*)nimcp_calloc(data->num_predictions, sizeof(uint32_t));
    data->prediction_confidence = (float*)nimcp_calloc(data->num_predictions, sizeof(float));
    if (!data->predicted_phonemes || !data->prediction_confidence) {
        nimcp_free(data->feature_weights);
        nimcp_free(data->predicted_phonemes);
        nimcp_free(data->prediction_confidence);
        data->feature_weights = NULL;
        data->predicted_phonemes = NULL;
        data->prediction_confidence = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_attention_data: required parameter is NULL (data->predicted_phonemes, data->prediction_confidence)");
        return -1;
    }

    data->attention_focus_x = 0.5f;
    data->attention_focus_y = 0.5f;
    data->attention_spread = 0.2f;

    return 0;
}

/**
 * @brief Cleanup speech cortex data
 */
static void cleanup_speech_data(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->speech_data.phoneme_buffer) {
        nimcp_free(bridge->speech_data.phoneme_buffer);
        bridge->speech_data.phoneme_buffer = NULL;
    }
}

/**
 * @brief Cleanup audio cortex data
 */
static void cleanup_audio_data(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->audio_data.spectral_features) {
        nimcp_free(bridge->audio_data.spectral_features);
        bridge->audio_data.spectral_features = NULL;
    }
}

/**
 * @brief Cleanup visual cortex data
 */
static void cleanup_visual_data(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->visual_data.recognized_text) {
        nimcp_free(bridge->visual_data.recognized_text);
        bridge->visual_data.recognized_text = NULL;
    }
    if (bridge->visual_data.viseme_features) {
        nimcp_free(bridge->visual_data.viseme_features);
        bridge->visual_data.viseme_features = NULL;
    }
}

/**
 * @brief Cleanup multimodal data
 */
static void cleanup_multimodal_data(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->multimodal_data.fused_features) {
        nimcp_free(bridge->multimodal_data.fused_features);
        bridge->multimodal_data.fused_features = NULL;
    }
}

/**
 * @brief Cleanup attention data
 */
static void cleanup_attention_data(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->attention.feature_weights) {
        nimcp_free(bridge->attention.feature_weights);
        bridge->attention.feature_weights = NULL;
    }
    if (bridge->attention.predicted_phonemes) {
        nimcp_free(bridge->attention.predicted_phonemes);
        bridge->attention.predicted_phonemes = NULL;
    }
    if (bridge->attention.prediction_confidence) {
        nimcp_free(bridge->attention.prediction_confidence);
        bridge->attention.prediction_confidence = NULL;
    }
}

/**
 * @brief Process McGurk effect integration
 */
static void process_mcgurk_integration(language_perception_bridge_t* bridge) {
    if (!bridge) return;

    language_multimodal_data_t* mm = &bridge->multimodal_data;
    language_speech_cortex_data_t* speech = &bridge->speech_data;
    language_visual_cortex_data_t* visual = &bridge->visual_data;

    /* Check for audiovisual condition */
    if (speech->detection != SPEECH_DETECTION_ACTIVE || !visual->lip_reading_active) {
        mm->mcgurk_active = false;
        return;
    }

    /* Check coherence threshold for McGurk detection */
    if (mm->av_coherence < LANGUAGE_PERCEPTION_MCGURK_AV_COHERENCE_THRESHOLD) {
        mm->mcgurk_active = true;
        mm->binding_state = AV_BINDING_INCONGRUENT;

        /* Fuse perception with visual bias */
        /* In McGurk effect, visual input influences perceived phoneme */
        /* e.g., audio /ba/ + visual /ga/ = perceived /da/ */
    } else {
        mm->mcgurk_active = false;
        mm->binding_state = AV_BINDING_CONGRUENT;
    }
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_perception_bridge_t* language_perception_bridge_create(
    const language_perception_config_t* config)
{
    language_perception_bridge_t* bridge = (language_perception_bridge_t*)
        nimcp_calloc(1, sizeof(language_perception_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_perception_bridge_create: allocation failed");
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        return NULL;
    }

    /* Copy configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(language_perception_config_t));
    } else {
        language_perception_default_config(&bridge->config);
    }

    /* Initialize data structures */
    if (init_speech_data(bridge) != 0 ||
        init_audio_data(bridge) != 0 ||
        init_visual_data(bridge) != 0 ||
        init_multimodal_data(bridge) != 0 ||
        init_attention_data(bridge) != 0) {
        language_perception_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_perception_bridge_create: validation failed");
        return NULL;
    }

    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Perception bridge created");
    return bridge;
}

void language_perception_bridge_destroy(language_perception_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_perception");

    /* Unregister from bio-async if registered */
    if (bridge->bio_async_registered) {
        language_perception_bridge_bio_async_unregister(bridge);
    }

    /* Cleanup all data */
    cleanup_speech_data(bridge);
    cleanup_audio_data(bridge);
    cleanup_visual_data(bridge);
    cleanup_multimodal_data(bridge);
    cleanup_attention_data(bridge);

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Perception bridge destroyed");
}

int language_perception_bridge_init(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_init: bridge is NULL");
        return -1;
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(language_perception_stats_t));

    bridge->initialized = true;
    LOG_DEBUG(LOG_MODULE, "Perception bridge initialized");
    return 0;
}

int language_perception_bridge_start(language_perception_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->active = true;
    LOG_INFO(LOG_MODULE, "Perception bridge started");
    return 0;
}

int language_perception_bridge_stop(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_stop: bridge is NULL");
        return -1;
    }

    bridge->active = false;
    LOG_INFO(LOG_MODULE, "Perception bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_perception_bridge_connect_orchestrator(
    language_perception_bridge_t* bridge,
    language_orchestrator_t* orchestrator)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_connect_orchestrator: bridge is NULL");
        return -1;
    }

    bridge->orchestrator = orchestrator;
    LOG_DEBUG(LOG_MODULE, "Connected to orchestrator");
    return 0;
}

int language_perception_bridge_connect_speech_cortex(
    language_perception_bridge_t* bridge,
    speech_cortex_t* speech_cortex)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_connect_speech_cortex: bridge is NULL");
        return -1;
    }

    bridge->speech_cortex = speech_cortex;
    LOG_DEBUG(LOG_MODULE, "Connected to speech cortex");
    return 0;
}

int language_perception_bridge_connect_audio_cortex(
    language_perception_bridge_t* bridge,
    audio_cortex_t* audio_cortex)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_connect_audio_cortex: bridge is NULL");
        return -1;
    }

    bridge->audio_cortex = audio_cortex;
    LOG_DEBUG(LOG_MODULE, "Connected to audio cortex");
    return 0;
}

int language_perception_bridge_connect_visual_cortex(
    language_perception_bridge_t* bridge,
    visual_cortex_t* visual_cortex)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_connect_visual_cortex: bridge is NULL");
        return -1;
    }

    bridge->visual_cortex = visual_cortex;
    LOG_DEBUG(LOG_MODULE, "Connected to visual cortex");
    return 0;
}

int language_perception_bridge_connect_omni_sensory(
    language_perception_bridge_t* bridge,
    omni_sensory_bridge_t* omni_sensory)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_connect_omni_sensory: bridge is NULL");
        return -1;
    }

    bridge->omni_sensory = omni_sensory;
    LOG_DEBUG(LOG_MODULE, "Connected to omni sensory bridge");
    return 0;
}

//=============================================================================
// Input Processing API Implementation
//=============================================================================

int language_perception_bridge_receive_phonemes(
    language_perception_bridge_t* bridge,
    const language_phoneme_t* phonemes,
    uint32_t count)
{
    if (!bridge || !phonemes || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_phonemes: required parameter is NULL (bridge, phonemes)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_phonemes: bridge->active is NULL");
        return -1;
    }

    language_speech_cortex_data_t* data = &bridge->speech_data;
    uint32_t accepted = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (data->phonemes_available >= data->phoneme_buffer_size) {
            /* Buffer full */
            break;
        }

        /* Copy phoneme to circular buffer */
        memcpy(&data->phoneme_buffer[data->phoneme_write_idx],
               &phonemes[i], sizeof(language_phoneme_t));

        data->phoneme_write_idx = (data->phoneme_write_idx + 1) % data->phoneme_buffer_size;
        data->phonemes_available++;
        accepted++;
    }

    bridge->stats.phonemes_received += accepted;

    return (int)accepted;
}

int language_perception_bridge_receive_prosody(
    language_perception_bridge_t* bridge,
    const language_prosody_t* prosody)
{
    if (!bridge || !prosody) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_prosody: required parameter is NULL (bridge, prosody)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_prosody: bridge->active is NULL");
        return -1;
    }

    memcpy(&bridge->speech_data.current_prosody, prosody, sizeof(language_prosody_t));
    bridge->speech_data.prosody_valid = true;

    return 0;
}

int language_perception_bridge_receive_text(
    language_perception_bridge_t* bridge,
    const char* text,
    float confidence)
{
    if (!bridge || !text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_text: required parameter is NULL (bridge, text)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_text: bridge->active is NULL");
        return -1;
    }

    language_visual_cortex_data_t* data = &bridge->visual_data;

    size_t len = strlen(text);
    if (len >= data->text_buffer_size) {
        len = data->text_buffer_size - 1;
    }

    strncpy(data->recognized_text, text, len);
    data->recognized_text[len] = '\0';
    data->text_length = (uint32_t)len;
    data->text_confidence = confidence;

    bridge->stats.text_segments_received++;

    return 0;
}

int language_perception_bridge_receive_visemes(
    language_perception_bridge_t* bridge,
    const float* visemes,
    uint32_t dim)
{
    if (!bridge || !visemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_visemes: required parameter is NULL (bridge, visemes)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_visemes: bridge->active is NULL");
        return -1;
    }

    language_visual_cortex_data_t* data = &bridge->visual_data;

    uint32_t copy_dim = dim < data->viseme_dim ? dim : data->viseme_dim;
    memcpy(data->viseme_features, visemes, copy_dim * sizeof(float));
    data->lip_reading_active = true;

    /* Compute confidence from viseme energy */
    float energy = 0.0f;
    for (uint32_t i = 0; i < copy_dim; i++) {
        energy += visemes[i] * visemes[i];
    }
    data->lip_reading_confidence = sqrtf(energy / (float)copy_dim);

    return 0;
}

int language_perception_bridge_receive_speech_detection(
    language_perception_bridge_t* bridge,
    speech_detection_state_t state,
    float probability)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_speech_detection: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_receive_speech_detection: bridge->active is NULL");
        return -1;
    }

    language_speech_cortex_data_t* data = &bridge->speech_data;

    speech_detection_state_t prev_state = data->detection;
    data->detection = state;
    data->speech_probability = probability;

    /* Track onset time */
    if (state == SPEECH_DETECTION_ONSET && prev_state == SPEECH_DETECTION_NONE) {
        data->speech_onset_time_ms = bridge->stats.last_update_time_ms;
    }

    /* Update multimodal binding state */
    if (state == SPEECH_DETECTION_ACTIVE) {
        if (bridge->visual_data.lip_reading_active) {
            bridge->multimodal_data.binding_state = AV_BINDING_CONGRUENT;
        } else {
            bridge->multimodal_data.binding_state = AV_BINDING_AUDIO_ONLY;
        }
    } else if (state == SPEECH_DETECTION_NONE) {
        if (bridge->visual_data.fixating_on_text) {
            bridge->multimodal_data.binding_state = AV_BINDING_VISUAL_ONLY;
        } else {
            bridge->multimodal_data.binding_state = AV_BINDING_NONE;
        }
    }

    return 0;
}

//=============================================================================
// Output Processing API Implementation
//=============================================================================

int language_perception_bridge_send_attention(
    language_perception_bridge_t* bridge,
    const float* weights,
    uint32_t count)
{
    if (!bridge || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_attention: required parameter is NULL (bridge, weights)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_attention: bridge->active is NULL");
        return -1;
    }

    language_perception_attention_t* attn = &bridge->attention;

    uint32_t copy_count = count < attn->num_features ? count : attn->num_features;
    memcpy(attn->feature_weights, weights, copy_count * sizeof(float));

    return 0;
}

int language_perception_bridge_send_predictions(
    language_perception_bridge_t* bridge,
    const uint32_t* predicted_phonemes,
    const float* confidences,
    uint32_t count)
{
    if (!bridge || !predicted_phonemes || !confidences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_predictions: required parameter is NULL (bridge, predicted_phonemes, confidences)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_predictions: bridge->active is NULL");
        return -1;
    }

    language_perception_attention_t* attn = &bridge->attention;

    uint32_t copy_count = count < attn->num_predictions ? count : attn->num_predictions;
    memcpy(attn->predicted_phonemes, predicted_phonemes, copy_count * sizeof(uint32_t));
    memcpy(attn->prediction_confidence, confidences, copy_count * sizeof(float));

    return 0;
}

int language_perception_bridge_send_spatial_attention(
    language_perception_bridge_t* bridge,
    float x,
    float y,
    float spread)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_spatial_attention: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_send_spatial_attention: bridge->active is NULL");
        return -1;
    }

    bridge->attention.attention_focus_x = x;
    bridge->attention.attention_focus_y = y;
    bridge->attention.attention_spread = spread;

    return 0;
}

//=============================================================================
// Multimodal Integration API Implementation
//=============================================================================

int language_perception_bridge_process_av_binding(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_process_av_binding: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_process_av_binding: bridge->active is NULL");
        return -1;
    }

    /* Compute audiovisual coherence */
    language_multimodal_data_t* mm = &bridge->multimodal_data;
    language_speech_cortex_data_t* speech = &bridge->speech_data;
    language_visual_cortex_data_t* visual = &bridge->visual_data;

    /* Simple coherence based on confidence correlation */
    if (speech->detection == SPEECH_DETECTION_ACTIVE && visual->lip_reading_active) {
        mm->av_coherence = (speech->speech_probability + visual->lip_reading_confidence) / 2.0f;
    } else {
        mm->av_coherence = 0.0f;
    }

    /* Process McGurk integration */
    process_mcgurk_integration(bridge);

    /* Update statistics */
    bridge->stats.avg_av_coherence =
        (bridge->stats.avg_av_coherence * 0.99f) + (mm->av_coherence * 0.01f);

    return 0;
}

av_binding_state_t language_perception_bridge_get_av_state(
    const language_perception_bridge_t* bridge)
{
    if (!bridge) return AV_BINDING_NONE;
    return bridge->multimodal_data.binding_state;
}

bool language_perception_bridge_is_mcgurk_active(
    const language_perception_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->multimodal_data.mcgurk_active;
}

//=============================================================================
// Update and Query API Implementation
//=============================================================================

int language_perception_bridge_update(
    language_perception_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->active) return 0;

    /* Update timestamp */
    bridge->stats.last_update_time_ms = current_time_ms;

    /* Process audiovisual binding */
    language_perception_bridge_process_av_binding(bridge);

    /* Apply attention decay */
    for (uint32_t i = 0; i < bridge->attention.num_features; i++) {
        bridge->attention.feature_weights[i] *= LANGUAGE_PERCEPTION_ATTENTION_DECAY_DEFAULT;
    }

    /* Update running averages */
    if (bridge->speech_data.phonemes_available > 0) {
        /* Compute average phoneme confidence from buffer */
        float sum_conf = 0.0f;
        for (uint32_t i = 0; i < bridge->speech_data.phonemes_available; i++) {
            uint32_t idx = (bridge->speech_data.phoneme_read_idx + i) %
                           bridge->speech_data.phoneme_buffer_size;
            sum_conf += bridge->speech_data.phoneme_buffer[idx].confidence;
        }
        float avg_conf = sum_conf / (float)bridge->speech_data.phonemes_available;
        bridge->stats.avg_phoneme_confidence =
            (bridge->stats.avg_phoneme_confidence * 0.9f) + (avg_conf * 0.1f);
    }

    bridge->stats.avg_speech_quality =
        (bridge->stats.avg_speech_quality * 0.95f) +
        (bridge->speech_data.signal_quality * 0.05f);

    return 0;
}

int language_perception_bridge_get_phonemes(
    language_perception_bridge_t* bridge,
    language_phoneme_t* phonemes,
    uint32_t max_count)
{
    if (!bridge || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_get_phonemes: required parameter is NULL (bridge, phonemes)");
        return -1;
    }

    language_speech_cortex_data_t* data = &bridge->speech_data;
    uint32_t retrieved = 0;

    while (retrieved < max_count && data->phonemes_available > 0) {
        memcpy(&phonemes[retrieved],
               &data->phoneme_buffer[data->phoneme_read_idx],
               sizeof(language_phoneme_t));

        data->phoneme_read_idx = (data->phoneme_read_idx + 1) % data->phoneme_buffer_size;
        data->phonemes_available--;
        retrieved++;
    }

    return (int)retrieved;
}

speech_detection_state_t language_perception_bridge_get_speech_state(
    const language_perception_bridge_t* bridge)
{
    if (!bridge) return SPEECH_DETECTION_NONE;
    return bridge->speech_data.detection;
}

int language_perception_bridge_get_stats(
    const language_perception_bridge_t* bridge,
    language_perception_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(language_perception_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_perception_bridge_bio_async_register(
    language_perception_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_bio_async_register: required parameter is NULL (bridge, router)");
        return -1;
    }

    bridge->bio_router = router;
    bridge->bio_async_registered = true;

    LOG_DEBUG(LOG_MODULE, "Registered with bio-async router");
    return 0;
}

int language_perception_bridge_bio_async_unregister(language_perception_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_perception_bridge_bio_async_unregister: bridge is NULL");
        return -1;
    }

    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    LOG_DEBUG(LOG_MODULE, "Unregistered from bio-async router");
    return 0;
}
