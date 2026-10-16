/**
 * @file nimcp_emotional_prosody.c
 * @brief Emotional prosody processor implementation
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_emotional_prosody.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(emotional_prosody, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct emotional_prosody {
    emotional_prosody_config_t config;
    prosody_status_t status;
    prosody_error_t last_error;
    prosody_stats_t stats;

    emotional_state_t current_emotion;
    prosodic_params_t current_params;

    bio_router_t* router;
    bool bio_registered;
};

/*=============================================================================
 * EMOTION PROSODY MAPPINGS
 *===========================================================================*/

typedef struct {
    emotion_type_t emotion;
    float pitch_offset;      /* Hz offset from base */
    float pitch_range;       /* Range multiplier */
    float rate_factor;       /* Speaking rate multiplier */
    float intensity_factor;  /* Volume multiplier */
    voice_quality_t quality;
} emotion_prosody_map_t;

static const emotion_prosody_map_t EMOTION_MAPS[] = {
    {EMOTION_NEUTRAL,    0.0f,  1.0f, 1.0f,  1.0f, VOICE_QUALITY_NORMAL},
    {EMOTION_HAPPY,     20.0f,  1.3f, 1.1f,  1.1f, VOICE_QUALITY_NORMAL},
    {EMOTION_SAD,      -15.0f,  0.7f, 0.85f, 0.8f, VOICE_QUALITY_BREATHY},
    {EMOTION_ANGRY,     30.0f,  1.5f, 1.2f,  1.3f, VOICE_QUALITY_TENSE},
    {EMOTION_FEARFUL,   25.0f,  1.4f, 1.3f,  0.9f, VOICE_QUALITY_TENSE},
    {EMOTION_SURPRISED, 40.0f,  1.6f, 1.15f, 1.1f, VOICE_QUALITY_NORMAL},
    {EMOTION_DISGUSTED, -5.0f,  0.9f, 0.9f,  0.95f, VOICE_QUALITY_CREAKY},
    {EMOTION_CONTEMPTUOUS, 5.0f, 0.8f, 0.95f, 1.0f, VOICE_QUALITY_CREAKY},
};

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

emotional_prosody_config_t emotional_prosody_default_config(void) {
    emotional_prosody_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_contour_points = PROSODY_DEFAULT_MAX_CONTOUR_POINTS;
    config.sample_rate_hz = PROSODY_DEFAULT_SAMPLE_RATE_HZ;
    config.base_pitch_hz = PROSODY_BASE_PITCH_HZ;
    config.base_rate_wpm = PROSODY_BASE_RATE_WPM;
    config.emotion_intensity_scale = 1.0f;
    config.enable_bio_async = false;
    config.enable_smooth_transitions = true;

    return config;
}

emotional_prosody_t* emotional_prosody_create(const emotional_prosody_config_t* config) {
    emotional_prosody_t* processor = (emotional_prosody_t*)nimcp_calloc(1, sizeof(emotional_prosody_t));
    if (!processor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_prosody_create: failed to allocate processor");

        return NULL;

    }

    if (config) {
        processor->config = *config;
    } else {
        processor->config = emotional_prosody_default_config();
    }

    processor->status = PROSODY_STATUS_IDLE;
    processor->current_emotion.primary_emotion = EMOTION_NEUTRAL;
    processor->current_emotion.primary_intensity = 1.0f;

    return processor;
}

void emotional_prosody_destroy(emotional_prosody_t* processor) {
    if (!processor) return;
    nimcp_free(processor);
}

bool emotional_prosody_reset(emotional_prosody_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->status = PROSODY_STATUS_IDLE;
    processor->last_error = PROSODY_ERROR_NONE;
    processor->current_emotion.primary_emotion = EMOTION_NEUTRAL;
    processor->current_emotion.primary_intensity = 1.0f;
    memset(&processor->current_params, 0, sizeof(prosodic_params_t));

    return true;
}

/*=============================================================================
 * EMOTION TO PROSODY MAPPING
 *===========================================================================*/

bool emotional_prosody_get_params(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion,
    prosodic_params_t* params) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion is NULL");
        return false;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "params is NULL");
        return false;
    }

    processor->status = PROSODY_STATUS_PROCESSING;

    /* Find emotion mapping */
    const emotion_prosody_map_t* map = &EMOTION_MAPS[0]; /* Default to neutral */
    for (size_t i = 0; i < sizeof(EMOTION_MAPS) / sizeof(EMOTION_MAPS[0]); i++) {
        if (EMOTION_MAPS[i].emotion == emotion->primary_emotion) {
            map = &EMOTION_MAPS[i];
            break;
        }
    }

    /* Apply base parameters with emotion intensity scaling */
    float intensity = emotion->primary_intensity * processor->config.emotion_intensity_scale;

    params->pitch_mean_hz = processor->config.base_pitch_hz + (map->pitch_offset * intensity);
    params->pitch_range_hz = 30.0f * map->pitch_range * intensity;
    params->pitch_slope = 0.0f;
    params->rate_factor = 1.0f + (map->rate_factor - 1.0f) * intensity;
    params->intensity_factor = 1.0f + (map->intensity_factor - 1.0f) * intensity;
    params->pause_factor = 1.0f;
    params->voice_quality = map->quality;

    /* Blend with secondary emotion if present */
    if (emotion->secondary_emotion != EMOTION_NEUTRAL &&
        emotion->secondary_intensity > 0.1f) {

        const emotion_prosody_map_t* secondary_map = &EMOTION_MAPS[0];
        for (size_t i = 0; i < sizeof(EMOTION_MAPS) / sizeof(EMOTION_MAPS[0]); i++) {
            if (EMOTION_MAPS[i].emotion == emotion->secondary_emotion) {
                secondary_map = &EMOTION_MAPS[i];
                break;
            }
        }

        float blend = emotion->secondary_intensity;
        params->pitch_mean_hz += secondary_map->pitch_offset * blend * 0.5f;
        params->rate_factor += (secondary_map->rate_factor - 1.0f) * blend * 0.5f;
    }

    processor->stats.parameters_computed++;
    processor->status = PROSODY_STATUS_READY;
    return true;
}

bool emotional_prosody_set_emotion(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion is NULL");
        return false;
    }

    processor->current_emotion = *emotion;
    return emotional_prosody_get_params(processor, emotion, &processor->current_params);
}

bool emotional_prosody_get_emotion(
    const emotional_prosody_t* processor,
    emotional_state_t* emotion) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion is NULL");
        return false;
    }
    *emotion = processor->current_emotion;
    return true;
}

bool emotional_prosody_blend_emotions(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion1,
    const emotional_state_t* emotion2,
    float blend_factor,
    emotional_state_t* result) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!emotion1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion1 is NULL");
        return false;
    }
    if (!emotion2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion2 is NULL");
        return false;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
        return false;
    }

    /* Clamp blend factor */
    if (blend_factor < 0.0f) blend_factor = 0.0f;
    if (blend_factor > 1.0f) blend_factor = 1.0f;

    /* Use dominant emotion as primary */
    if (blend_factor < 0.5f) {
        result->primary_emotion = emotion1->primary_emotion;
        result->primary_intensity = emotion1->primary_intensity * (1.0f - blend_factor);
        result->secondary_emotion = emotion2->primary_emotion;
        result->secondary_intensity = emotion2->primary_intensity * blend_factor;
    } else {
        result->primary_emotion = emotion2->primary_emotion;
        result->primary_intensity = emotion2->primary_intensity * blend_factor;
        result->secondary_emotion = emotion1->primary_emotion;
        result->secondary_intensity = emotion1->primary_intensity * (1.0f - blend_factor);
    }

    /* Blend arousal and valence */
    result->arousal = emotion1->arousal * (1.0f - blend_factor) + emotion2->arousal * blend_factor;
    result->valence = emotion1->valence * (1.0f - blend_factor) + emotion2->valence * blend_factor;

    return true;
}

/*=============================================================================
 * CONTOUR GENERATION
 *===========================================================================*/

bool emotional_prosody_generate_contour(
    emotional_prosody_t* processor,
    const char* utterance,
    float duration_ms,
    prosodic_contour_t* contour) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!utterance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "utterance is NULL");
        return false;
    }
    if (!contour) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contour is NULL");
        return false;
    }
    if (duration_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "duration_ms <= 0");
        return false;
    }

    processor->status = PROSODY_STATUS_PROCESSING;

    /* Allocate contour points */
    uint32_t num_points = (uint32_t)(duration_ms / (1000.0f / processor->config.sample_rate_hz));
    if (num_points > processor->config.max_contour_points) {
        num_points = processor->config.max_contour_points;
    }
    if (num_points < 2) num_points = 2;

    contour->points = (prosody_contour_point_t*)nimcp_calloc(num_points, sizeof(prosody_contour_point_t));
    if (!contour->points) {
        processor->last_error = PROSODY_ERROR_INTERNAL;
        processor->status = PROSODY_STATUS_ERROR;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: contour->points is NULL");
        return false;
    }

    contour->point_count = num_points;
    contour->duration_ms = duration_ms;
    contour->emotion = processor->current_emotion;

    /* Generate contour based on current parameters */
    float time_step = duration_ms / (num_points - 1);
    for (uint32_t i = 0; i < num_points; i++) {
        prosody_contour_point_t* pt = &contour->points[i];
        pt->time_ms = i * time_step;

        /* Basic declination contour with emotion modulation */
        float progress = (float)i / (num_points - 1);
        float declination = -processor->current_params.pitch_range_hz * 0.3f * progress;

        /* Add some natural variation */
        float variation = sinf(progress * NIMCP_PI_F * 4) * processor->current_params.pitch_range_hz * 0.1f;

        pt->pitch_hz = processor->current_params.pitch_mean_hz + declination + variation;
        pt->intensity = processor->current_params.intensity_factor;
        pt->rate_factor = processor->current_params.rate_factor;
    }

    processor->stats.contours_generated++;
    processor->status = PROSODY_STATUS_READY;
    (void)utterance;  /* Would be used for more sophisticated analysis */
    return true;
}

bool emotional_prosody_apply_contour(
    emotional_prosody_t* processor,
    const prosodic_contour_t* contour,
    uint8_t* phonemes,
    uint32_t phoneme_count,
    float* pitch_targets,
    float* duration_targets) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!contour) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contour is NULL");
        return false;
    }
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonemes is NULL");
        return false;
    }
    if (!pitch_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pitch_targets is NULL");
        return false;
    }
    if (!duration_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "duration_targets is NULL");
        return false;
    }
    if (phoneme_count == 0 || contour->point_count == 0) return false;

    /* Distribute contour points across phonemes */
    float phonemes_per_point = (float)phoneme_count / contour->point_count;

    for (uint32_t i = 0; i < phoneme_count; i++) {
        uint32_t contour_idx = (uint32_t)(i / phonemes_per_point);
        if (contour_idx >= contour->point_count) {
            contour_idx = contour->point_count - 1;
        }

        pitch_targets[i] = contour->points[contour_idx].pitch_hz;
        duration_targets[i] = 1.0f / contour->points[contour_idx].rate_factor;
    }

    return true;
}

void emotional_prosody_free_contour(prosodic_contour_t* contour) {
    if (!contour) return;
    if (contour->points) {
        nimcp_free(contour->points);
        contour->points = NULL;
    }
    contour->point_count = 0;
}

/*=============================================================================
 * EMOTION NAMES
 *===========================================================================*/

const char* emotional_prosody_emotion_name(emotion_type_t emotion) {
    static const char* names[] = {
        "NEUTRAL", "HAPPY", "SAD", "ANGRY", "FEARFUL",
        "SURPRISED", "DISGUSTED", "CONTEMPTUOUS"
    };
    if (emotion >= EMOTION_COUNT) return "INVALID";
    return names[emotion];
}

const char* emotional_prosody_quality_name(voice_quality_t quality) {
    static const char* names[] = {
        "NORMAL", "BREATHY", "CREAKY", "TENSE", "LAX"
    };
    if (quality >= VOICE_QUALITY_COUNT) return "INVALID";
    return names[quality];
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

prosody_status_t emotional_prosody_get_status(const emotional_prosody_t* processor) {
    if (!processor) return PROSODY_STATUS_ERROR;
    return processor->status;
}

prosody_error_t emotional_prosody_get_last_error(const emotional_prosody_t* processor) {
    if (!processor) return PROSODY_ERROR_INTERNAL;
    return processor->last_error;
}

bool emotional_prosody_get_stats(const emotional_prosody_t* processor, prosody_stats_t* stats) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return false;
    }
    *stats = processor->stats;
    return true;
}

void emotional_prosody_reset_stats(emotional_prosody_t* processor) {
    if (!processor) return;
    memset(&processor->stats, 0, sizeof(prosody_stats_t));
}

bool emotional_prosody_get_config(const emotional_prosody_t* processor, emotional_prosody_config_t* config) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return false;
    }
    *config = processor->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool emotional_prosody_register_bio_handler(
    emotional_prosody_t* processor,
    bio_router_t* router) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");
        return false;
    }

    processor->router = router;
    processor->bio_registered = true;
    return true;
}
