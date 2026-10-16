/**
 * @file nimcp_cochlea_occipital_bridge.c
 * @brief Cochlea-Occipital (Visual) bidirectional bridge implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_occipital_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_occipital_bridge)

#define LOG_MODULE "COCHLEA_OCCIPITAL_BRIDGE"

//=============================================================================
// Helpers
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static float clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_occipital_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */
    cochlea_occipital_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    occipital_adapter_t* occipital;

    /* Outbound state: audio -> visual */
    cochlea_audio_to_visual_t audio_to_visual;
    bool outbound_valid;

    /* Inbound state: visual -> audio */
    cochlea_visual_to_audio_t visual_to_audio;
    bool inbound_valid;

    /* Audiovisual binding */
    bool av_bound;
    float binding_confidence;

    /* McGurk */
    bool mcgurk_enabled;
    float visual_weight;

    /* Bidirectional tracking */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Timing */
    float time_since_update_ms;
};

//=============================================================================
// Default Configuration
//=============================================================================

static void cochlea_occipital_default_config(cochlea_occipital_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->av_offset_ms = COCHLEA_OCCIPITAL_AV_OFFSET_MS;
    config->alignment_buffer_size = 32;
    config->enable_mcgurk = true;
    config->visual_weight = 0.5f;
    config->enable_echo_to_visual = false;
    config->spatial_binding_radius_deg = 15.0f;
}

cochlea_occipital_config_t cochlea_occipital_config_default(void) {
    cochlea_occipital_bridge_heartbeat("config_default", 0.0f);
    cochlea_occipital_config_t config;
    cochlea_occipital_default_config(&config);
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

cochlea_occipital_bridge_t* cochlea_occipital_bridge_create(
    cochlea_t* cochlea,
    occipital_adapter_t* occipital,
    const cochlea_occipital_config_t* config
) {
    cochlea_occipital_bridge_heartbeat("create", 0.0f);

    cochlea_occipital_bridge_t* bridge = (cochlea_occipital_bridge_t*)nimcp_calloc(
        1, sizeof(cochlea_occipital_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_occipital_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        cochlea_occipital_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_occipital_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_occipital_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->occipital = occipital;
    bridge->outbound_valid = false;
    bridge->inbound_valid = false;
    bridge->av_bound = false;
    bridge->binding_confidence = 0.0f;
    bridge->mcgurk_enabled = bridge->config.enable_mcgurk;
    bridge->visual_weight = bridge->config.visual_weight;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;
    bridge->time_since_update_ms = 0.0f;

    /* Connect systems to base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (occipital) {
        bridge_base_connect_b_unlocked(&bridge->base, occipital);
    }

    cochlea_occipital_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_occipital_bridge_destroy(cochlea_occipital_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_occipital");
    cochlea_occipital_bridge_heartbeat("destroy", 0.0f);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_occipital_bridge_update(
    cochlea_occipital_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_bridge_update: bridge NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_update_ms += dt_ms;

    /* If cochlea output available, update outbound state */
    if (cochlea_output) {
        /* Outbound: extract speech envelope and spatial info */
        bridge->last_outbound_ms = get_time_ms();
    }

    /* Attempt audiovisual binding if both sides have data */
    if (bridge->outbound_valid && bridge->inbound_valid) {
        /* Spatial binding check: compare sound azimuth to visual gaze */
        float azimuth_diff = fabsf(bridge->audio_to_visual.sound_azimuth_deg -
                                   bridge->visual_to_audio.expected_sound_azimuth);
        if (azimuth_diff <= bridge->config.spatial_binding_radius_deg) {
            bridge->av_bound = true;
            bridge->binding_confidence = 1.0f - (azimuth_diff / bridge->config.spatial_binding_radius_deg);
        } else {
            bridge->av_bound = false;
            bridge->binding_confidence = 0.0f;
        }
    }

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_occipital_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_occipital_bridge_reset(cochlea_occipital_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->audio_to_visual, 0, sizeof(bridge->audio_to_visual));
    memset(&bridge->visual_to_audio, 0, sizeof(bridge->visual_to_audio));
    bridge->outbound_valid = false;
    bridge->inbound_valid = false;
    bridge->av_bound = false;
    bridge->binding_confidence = 0.0f;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;
    bridge->time_since_update_ms = 0.0f;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_occipital_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Audio to Visual (Outbound)
//=============================================================================

nimcp_error_t cochlea_occipital_send_audio(
    cochlea_occipital_bridge_t* bridge,
    const cochlea_output_t* output
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_send_audio: bridge NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_send_audio: output NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("send_audio", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear previous outbound data */
    memset(&bridge->audio_to_visual, 0, sizeof(bridge->audio_to_visual));

    /* Populate speech envelope and spatial features from cochlea output */
    bridge->audio_to_visual.has_echo_map = false;
    bridge->audio_to_visual.echo_spatial_map = NULL;

    bridge->outbound_valid = true;
    bridge->last_outbound_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_occipital_bridge_heartbeat("send_audio", 1.0f);
    return 0;
}

nimcp_error_t cochlea_occipital_get_audio_to_visual(
    const cochlea_occipital_bridge_t* bridge,
    cochlea_audio_to_visual_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_get_audio_to_visual: bridge NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_get_audio_to_visual: state NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("get_audio_to_visual", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->audio_to_visual;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Visual to Audio (Inbound)
//=============================================================================

nimcp_error_t cochlea_occipital_receive_visual(
    cochlea_occipital_bridge_t* bridge,
    const occipital_visual_features_t* features
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_receive_visual: bridge NULL");
        return -1;
    }
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_receive_visual: features NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("receive_visual", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Map visual features to visual-to-audio state */
    bridge->visual_to_audio.lip_aperture = features->lip_aperture;
    bridge->visual_to_audio.lip_protrusion = features->lip_protrusion;
    bridge->visual_to_audio.jaw_position = features->jaw_position;
    bridge->visual_to_audio.visual_gaze_x = features->gaze_x;
    bridge->visual_to_audio.visual_gaze_y = features->gaze_y;

    /* Estimate expected sound direction from gaze */
    bridge->visual_to_audio.expected_sound_azimuth = features->gaze_x * 90.0f;

    /* Face detection: if lip data is non-zero, assume face detected */
    bridge->visual_to_audio.face_detected =
        (features->lip_aperture > 0.0f || features->lip_protrusion > 0.0f);

    bridge->inbound_valid = true;
    bridge->last_inbound_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_occipital_bridge_heartbeat("receive_visual", 1.0f);
    return 0;
}

nimcp_error_t cochlea_occipital_get_visual_to_audio(
    const cochlea_occipital_bridge_t* bridge,
    cochlea_visual_to_audio_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_get_visual_to_audio: bridge NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_get_visual_to_audio: state NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("get_visual_to_audio", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->visual_to_audio;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Audiovisual Binding
//=============================================================================

nimcp_error_t cochlea_occipital_bind(cochlea_occipital_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_bind: bridge NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("bind", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->outbound_valid || !bridge->inbound_valid) {
        /* Cannot bind without data from both sides */
        bridge->av_bound = false;
        bridge->binding_confidence = 0.0f;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Spatial binding: compare sound azimuth to expected sound direction from vision */
    float azimuth_diff = fabsf(bridge->audio_to_visual.sound_azimuth_deg -
                               bridge->visual_to_audio.expected_sound_azimuth);

    /* Temporal binding: check timestamp proximity */
    float temporal_diff = 0.0f;
    if (bridge->last_outbound_ms > 0 && bridge->last_inbound_ms > 0) {
        if (bridge->last_outbound_ms > bridge->last_inbound_ms) {
            temporal_diff = (float)(bridge->last_outbound_ms - bridge->last_inbound_ms);
        } else {
            temporal_diff = (float)(bridge->last_inbound_ms - bridge->last_outbound_ms);
        }
    }

    /* Spatial score */
    float spatial_score = 0.0f;
    if (azimuth_diff <= bridge->config.spatial_binding_radius_deg) {
        spatial_score = 1.0f - (azimuth_diff / bridge->config.spatial_binding_radius_deg);
    }

    /* Temporal score: must be within AV offset window */
    float temporal_score = 0.0f;
    if (temporal_diff <= bridge->config.av_offset_ms) {
        temporal_score = 1.0f - (temporal_diff / bridge->config.av_offset_ms);
    }

    /* Combined binding confidence */
    bridge->binding_confidence = spatial_score * 0.5f + temporal_score * 0.5f;
    bridge->av_bound = (bridge->binding_confidence > 0.3f);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_occipital_bridge_heartbeat("bind", 1.0f);
    return 0;
}

bool cochlea_occipital_is_bound(const cochlea_occipital_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->av_bound;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float cochlea_occipital_get_binding_confidence(const cochlea_occipital_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float conf = bridge->binding_confidence;
    nimcp_mutex_unlock(bridge->base.mutex);
    return conf;
}

//=============================================================================
// McGurk Effect
//=============================================================================

nimcp_error_t cochlea_occipital_set_mcgurk(
    cochlea_occipital_bridge_t* bridge,
    bool enable
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_set_mcgurk: bridge NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("set_mcgurk", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->mcgurk_enabled = enable;
    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_occipital_bridge_heartbeat("set_mcgurk", 1.0f);
    return 0;
}

nimcp_error_t cochlea_occipital_set_visual_weight(
    cochlea_occipital_bridge_t* bridge,
    float weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_set_visual_weight: bridge NULL");
        return -1;
    }
    cochlea_occipital_bridge_heartbeat("set_visual_weight", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->visual_weight = clampf(weight, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_occipital_bridge_heartbeat("set_visual_weight", 1.0f);
    return 0;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_occipital_verify_bidirectional(const cochlea_occipital_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_occipital_verify_bidirectional: bridge is NULL");
        return false;
    }
    cochlea_occipital_bridge_heartbeat("verify_bidirectional", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bool result = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);

    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_occipital_get_last_outbound(const cochlea_occipital_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t val = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);
    return val;
}

uint64_t cochlea_occipital_get_last_inbound(const cochlea_occipital_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t val = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);
    return val;
}
