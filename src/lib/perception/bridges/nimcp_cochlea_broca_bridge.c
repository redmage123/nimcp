/**
 * @file nimcp_cochlea_broca_bridge.c
 * @brief Cochlea-Broca's Region bidirectional bridge implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_broca_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_broca_bridge)

#define LOG_MODULE "COCHLEA_BROCA_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_broca_bridge {
    bridge_base_t base;                     /**< MUST be first */
    cochlea_broca_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    broca_adapter_t* broca;

    /* Outbound: audio -> speech (cochlea -> Broca) */
    cochlea_audio_to_speech_t outbound;

    /* Inbound: speech -> audio (Broca -> cochlea) */
    cochlea_speech_to_audio_t inbound;

    /* Predictions from Broca */
    broca_prediction_t predictions;

    /* Phonological loop */
    float* loop_buffer;
    uint32_t loop_buffer_size;
    uint32_t loop_write_pos;
    bool loop_active;

    /* Mirror neuron activation */
    float mirror_activation;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Accumulated time */
    float accumulated_time_ms;
};

//=============================================================================
// Helper: Get current time in milliseconds
//=============================================================================

static uint64_t broca_bridge_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_broca_config_t cochlea_broca_config_default(void) {
    cochlea_broca_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.enable_phoneme_detection = true;
    cfg.max_phonemes = COCHLEA_BROCA_MAX_PHONEMES;
    cfg.enable_phonological_loop = true;
    cfg.loop_buffer_size = COCHLEA_BROCA_LOOP_SIZE;
    cfg.loop_decay_rate = 0.95f;
    cfg.enable_mirror_activation = true;
    cfg.mirror_threshold = 0.3f;
    cfg.enable_predictions = true;

    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_broca_bridge_t* cochlea_broca_bridge_create(
    cochlea_t* cochlea,
    broca_adapter_t* broca,
    const cochlea_broca_config_t* config)
{
    cochlea_broca_bridge_heartbeat("create", 0.0f);

    cochlea_broca_bridge_t* bridge =
        (cochlea_broca_bridge_t*)nimcp_calloc(1, sizeof(cochlea_broca_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_broca_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_broca_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_broca_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_broca_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->broca = broca;
    bridge->loop_active = false;
    bridge->mirror_activation = 0.0f;
    bridge->accumulated_time_ms = 0.0f;

    /* Connect systems via base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (broca) {
        bridge_base_connect_b_unlocked(&bridge->base, broca);
    }

    /* Allocate outbound phoneme activations */
    if (bridge->config.max_phonemes > 0) {
        bridge->outbound.phoneme_activations =
            (float*)nimcp_calloc(bridge->config.max_phonemes, sizeof(float));
        if (!bridge->outbound.phoneme_activations) {
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "cochlea_broca_bridge_create: phoneme alloc failed");
            return NULL;
        }
        bridge->outbound.num_phonemes = 0;
    }

    /* Allocate phonological loop buffer */
    if (bridge->config.enable_phonological_loop && bridge->config.loop_buffer_size > 0) {
        bridge->loop_buffer =
            (float*)nimcp_calloc(bridge->config.loop_buffer_size, sizeof(float));
        if (!bridge->loop_buffer) {
            if (bridge->outbound.phoneme_activations) {
                nimcp_free(bridge->outbound.phoneme_activations);
            }
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "cochlea_broca_bridge_create: loop buffer alloc failed");
            return NULL;
        }
        bridge->loop_buffer_size = bridge->config.loop_buffer_size;
        bridge->loop_write_pos = 0;
    }

    cochlea_broca_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_broca_bridge_destroy(cochlea_broca_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_broca");
    cochlea_broca_bridge_heartbeat("destroy", 0.0f);

    /* Free outbound phoneme activations */
    if (bridge->outbound.phoneme_activations) {
        nimcp_free(bridge->outbound.phoneme_activations);
        bridge->outbound.phoneme_activations = NULL;
    }

    /* Free inbound expected phonemes */
    if (bridge->inbound.expected_phonemes) {
        nimcp_free(bridge->inbound.expected_phonemes);
        bridge->inbound.expected_phonemes = NULL;
    }
    if (bridge->inbound.articulatory_template) {
        nimcp_free(bridge->inbound.articulatory_template);
        bridge->inbound.articulatory_template = NULL;
    }

    /* Free prediction buffers */
    if (bridge->predictions.expected_phonemes) {
        nimcp_free(bridge->predictions.expected_phonemes);
        bridge->predictions.expected_phonemes = NULL;
    }
    if (bridge->predictions.articulatory_template) {
        nimcp_free(bridge->predictions.articulatory_template);
        bridge->predictions.articulatory_template = NULL;
    }

    /* Free loop buffer */
    if (bridge->loop_buffer) {
        nimcp_free(bridge->loop_buffer);
        bridge->loop_buffer = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_broca_bridge_update(
    cochlea_broca_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_bridge_update: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_bridge_update: cochlea_output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->accumulated_time_ms += dt_ms;

    /* Apply phonological loop decay */
    if (bridge->loop_active && bridge->loop_buffer && bridge->config.loop_decay_rate < 1.0f) {
        float decay = powf(bridge->config.loop_decay_rate, dt_ms / 1000.0f);
        for (uint32_t i = 0; i < bridge->loop_buffer_size; i++) {
            bridge->loop_buffer[i] *= decay;
        }
    }

    /* Apply mirror neuron decay */
    if (bridge->config.enable_mirror_activation) {
        bridge->mirror_activation *= powf(0.95f, dt_ms / 1000.0f);
        if (bridge->mirror_activation < 0.001f) {
            bridge->mirror_activation = 0.0f;
        }
    }

    /* Record outbound timestamp */
    bridge->last_outbound_ms = broca_bridge_time_ms();

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_broca_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broca_bridge_reset(cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_bridge_reset: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset outbound */
    if (bridge->outbound.phoneme_activations) {
        memset(bridge->outbound.phoneme_activations, 0,
               bridge->config.max_phonemes * sizeof(float));
    }
    bridge->outbound.num_phonemes = 0;
    bridge->outbound.speech_rate = 0.0f;
    bridge->outbound.voice_detected = false;
    bridge->outbound.fundamental_freq_hz = 0.0f;
    memset(bridge->outbound.speech_envelope, 0, sizeof(bridge->outbound.speech_envelope));
    memset(bridge->outbound.pitch_contour, 0, sizeof(bridge->outbound.pitch_contour));

    /* Reset inbound */
    if (bridge->inbound.expected_phonemes) {
        nimcp_free(bridge->inbound.expected_phonemes);
        bridge->inbound.expected_phonemes = NULL;
    }
    if (bridge->inbound.articulatory_template) {
        nimcp_free(bridge->inbound.articulatory_template);
        bridge->inbound.articulatory_template = NULL;
    }
    bridge->inbound.num_expected = 0;
    bridge->inbound.template_size = 0;
    bridge->inbound.subvocal_active = false;

    /* Reset predictions */
    if (bridge->predictions.expected_phonemes) {
        nimcp_free(bridge->predictions.expected_phonemes);
        bridge->predictions.expected_phonemes = NULL;
    }
    if (bridge->predictions.articulatory_template) {
        nimcp_free(bridge->predictions.articulatory_template);
        bridge->predictions.articulatory_template = NULL;
    }
    bridge->predictions.num_expected = 0;
    bridge->predictions.template_size = 0;
    bridge->predictions.confidence = 0.0f;

    /* Reset loop */
    if (bridge->loop_buffer) {
        memset(bridge->loop_buffer, 0, bridge->loop_buffer_size * sizeof(float));
    }
    bridge->loop_write_pos = 0;
    bridge->loop_active = false;

    /* Reset mirror */
    bridge->mirror_activation = 0.0f;

    bridge->accumulated_time_ms = 0.0f;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_broca_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Phoneme Sending (Outbound)
//=============================================================================

nimcp_error_t cochlea_broca_send_phonemes(
    cochlea_broca_bridge_t* bridge,
    const float* phoneme_features,
    uint32_t num_features)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_send_phonemes: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!phoneme_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_send_phonemes: phoneme_features NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("send_phonemes", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t count = (num_features > bridge->config.max_phonemes)
                     ? bridge->config.max_phonemes : num_features;

    if (bridge->outbound.phoneme_activations && count > 0) {
        memcpy(bridge->outbound.phoneme_activations, phoneme_features,
               count * sizeof(float));
        bridge->outbound.num_phonemes = count;
        bridge->outbound.voice_detected = true;
    }

    /* Feed into phonological loop if active */
    if (bridge->loop_active && bridge->loop_buffer && count > 0) {
        for (uint32_t i = 0; i < count && bridge->loop_write_pos < bridge->loop_buffer_size; i++) {
            bridge->loop_buffer[bridge->loop_write_pos++] = phoneme_features[i];
            if (bridge->loop_write_pos >= bridge->loop_buffer_size) {
                bridge->loop_write_pos = 0;  /* Wrap around */
            }
        }
    }

    /* Activate mirror neurons from speech detection */
    if (bridge->config.enable_mirror_activation) {
        /* Increase mirror activation when hearing speech */
        float sum = 0.0f;
        for (uint32_t i = 0; i < count; i++) {
            sum += fabsf(phoneme_features[i]);
        }
        float avg = (count > 0) ? sum / (float)count : 0.0f;
        if (avg > bridge->config.mirror_threshold) {
            bridge->mirror_activation = fminf(1.0f, bridge->mirror_activation + avg * 0.3f);
        }
    }

    bridge->last_outbound_ms = broca_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broca_get_audio_to_speech(
    const cochlea_broca_bridge_t* bridge,
    cochlea_audio_to_speech_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_audio_to_speech: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_audio_to_speech: state NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->outbound;
    /* Note: phoneme_activations pointer is shared; caller must not free */
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Predictions (Inbound)
//=============================================================================

nimcp_error_t cochlea_broca_receive_predictions(
    cochlea_broca_bridge_t* bridge,
    broca_prediction_t* predictions)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_receive_predictions: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_receive_predictions: predictions NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("receive_predictions", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Store predictions locally */
    /* Free old prediction data */
    if (bridge->predictions.expected_phonemes) {
        nimcp_free(bridge->predictions.expected_phonemes);
        bridge->predictions.expected_phonemes = NULL;
    }
    if (bridge->predictions.articulatory_template) {
        nimcp_free(bridge->predictions.articulatory_template);
        bridge->predictions.articulatory_template = NULL;
    }

    bridge->predictions.num_expected = predictions->num_expected;
    bridge->predictions.template_size = predictions->template_size;
    bridge->predictions.confidence = predictions->confidence;

    /* Copy expected phonemes if provided */
    if (predictions->expected_phonemes && predictions->num_expected > 0) {
        bridge->predictions.expected_phonemes =
            (float*)nimcp_calloc(predictions->num_expected, sizeof(float));
        if (bridge->predictions.expected_phonemes) {
            memcpy(bridge->predictions.expected_phonemes,
                   predictions->expected_phonemes,
                   predictions->num_expected * sizeof(float));
        }
    }

    /* Copy articulatory template if provided */
    if (predictions->articulatory_template && predictions->template_size > 0) {
        bridge->predictions.articulatory_template =
            (float*)nimcp_calloc(predictions->template_size, sizeof(float));
        if (bridge->predictions.articulatory_template) {
            memcpy(bridge->predictions.articulatory_template,
                   predictions->articulatory_template,
                   predictions->template_size * sizeof(float));
        }
    }

    /* Update inbound state */
    if (bridge->inbound.expected_phonemes) {
        nimcp_free(bridge->inbound.expected_phonemes);
        bridge->inbound.expected_phonemes = NULL;
    }
    if (bridge->inbound.articulatory_template) {
        nimcp_free(bridge->inbound.articulatory_template);
        bridge->inbound.articulatory_template = NULL;
    }

    bridge->inbound.num_expected = predictions->num_expected;
    bridge->inbound.template_size = predictions->template_size;

    if (predictions->expected_phonemes && predictions->num_expected > 0) {
        bridge->inbound.expected_phonemes =
            (float*)nimcp_calloc(predictions->num_expected, sizeof(float));
        if (bridge->inbound.expected_phonemes) {
            memcpy(bridge->inbound.expected_phonemes,
                   predictions->expected_phonemes,
                   predictions->num_expected * sizeof(float));
        }
    }
    if (predictions->articulatory_template && predictions->template_size > 0) {
        bridge->inbound.articulatory_template =
            (float*)nimcp_calloc(predictions->template_size, sizeof(float));
        if (bridge->inbound.articulatory_template) {
            memcpy(bridge->inbound.articulatory_template,
                   predictions->articulatory_template,
                   predictions->template_size * sizeof(float));
        }
    }

    bridge->last_inbound_ms = broca_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broca_get_speech_to_audio(
    const cochlea_broca_bridge_t* bridge,
    cochlea_speech_to_audio_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_speech_to_audio: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_speech_to_audio: state NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->inbound;
    /* Note: pointers are shared; caller must not free */
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Phonological Loop
//=============================================================================

nimcp_error_t cochlea_broca_activate_loop(cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_activate_loop: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("activate_loop", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->loop_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broca_deactivate_loop(cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_deactivate_loop: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_broca_bridge_heartbeat("deactivate_loop", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->loop_active = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool cochlea_broca_is_loop_active(const cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_is_loop_active: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool active = bridge->loop_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}

nimcp_error_t cochlea_broca_get_loop_content(
    const cochlea_broca_bridge_t* bridge,
    float* buffer,
    uint32_t* buffer_size)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_loop_content: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!buffer || !buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_loop_content: buffer or buffer_size NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->loop_buffer || bridge->loop_buffer_size == 0) {
        *buffer_size = 0;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    uint32_t copy_size = (*buffer_size < bridge->loop_buffer_size)
                         ? *buffer_size : bridge->loop_buffer_size;
    memcpy(buffer, bridge->loop_buffer, copy_size * sizeof(float));
    *buffer_size = copy_size;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Mirror Activation
//=============================================================================

float cochlea_broca_get_mirror_activation(const cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_mirror_activation: bridge NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float activation = bridge->mirror_activation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return activation;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_broca_verify_bidirectional(const cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_verify_bidirectional: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool bidir = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);
    nimcp_mutex_unlock(bridge->base.mutex);

    return bidir;
}

uint64_t cochlea_broca_get_last_outbound(const cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_last_outbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}

uint64_t cochlea_broca_get_last_inbound(const cochlea_broca_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broca_get_last_inbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}
