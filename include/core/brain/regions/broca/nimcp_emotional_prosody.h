/**
 * @file nimcp_emotional_prosody.h
 * @brief Emotional prosody processing for speech production
 *
 * WHAT: Maps emotions to prosodic features (pitch, rate, intensity)
 * WHY:  Enable emotionally expressive speech synthesis
 * HOW:  Modulate acoustic parameters based on emotional state
 *
 * ARCHITECTURE:
 * - Emotion Analyzer: Receives emotional state input
 * - Prosodic Mapper: Converts emotions to acoustic parameters
 * - Parameter Modulator: Applies modifications to speech output
 * - Contour Generator: Creates natural prosodic contours
 *
 * BIOLOGICAL BASIS:
 * - Right hemisphere's role in prosody production
 * - Limbic system connections for emotional expression
 * - Basal ganglia involvement in speech timing
 *
 * PROSODIC FEATURES:
 * - Pitch (F0): Fundamental frequency variations
 * - Rate: Speaking tempo modulation
 * - Intensity: Volume/loudness variations
 * - Voice Quality: Breathiness, tenseness
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_EMOTIONAL_PROSODY_H
#define NIMCP_EMOTIONAL_PROSODY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

typedef struct emotional_prosody emotional_prosody_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PROSODY_DEFAULT_MAX_CONTOUR_POINTS   64
#define PROSODY_DEFAULT_SAMPLE_RATE_HZ      100
#define PROSODY_BASE_PITCH_HZ              120.0f
#define PROSODY_BASE_RATE_WPM              150.0f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Basic emotion categories
 */
typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_ANGRY,
    EMOTION_FEARFUL,
    EMOTION_SURPRISED,
    EMOTION_DISGUSTED,
    EMOTION_CONTEMPTUOUS,
    EMOTION_COUNT
} emotion_type_t;

/**
 * @brief Voice quality types
 */
typedef enum {
    VOICE_QUALITY_NORMAL = 0,
    VOICE_QUALITY_BREATHY,
    VOICE_QUALITY_CREAKY,
    VOICE_QUALITY_TENSE,
    VOICE_QUALITY_LAX,
    VOICE_QUALITY_COUNT
} voice_quality_t;

/**
 * @brief Processing status
 */
typedef enum {
    PROSODY_STATUS_IDLE = 0,
    PROSODY_STATUS_PROCESSING,
    PROSODY_STATUS_READY,
    PROSODY_STATUS_ERROR
} prosody_status_t;

typedef enum {
    PROSODY_ERROR_NONE = 0,
    PROSODY_ERROR_INVALID_INPUT,
    PROSODY_ERROR_BUFFER_FULL,
    PROSODY_ERROR_INTERNAL
} prosody_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Emotional prosody configuration
 */
typedef struct {
    uint32_t max_contour_points;     /**< Max points in prosodic contour */
    uint32_t sample_rate_hz;         /**< Sampling rate for contours */
    float base_pitch_hz;             /**< Base pitch frequency */
    float base_rate_wpm;             /**< Base speaking rate */
    float emotion_intensity_scale;   /**< How strongly emotions affect prosody */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_smooth_transitions;  /**< Smooth between emotion changes */
} emotional_prosody_config_t;

/**
 * @brief Emotional state input
 */
typedef struct {
    emotion_type_t primary_emotion;  /**< Primary emotion */
    float primary_intensity;         /**< Intensity (0-1) */
    emotion_type_t secondary_emotion; /**< Secondary/blended emotion */
    float secondary_intensity;       /**< Secondary intensity */
    float arousal;                   /**< Arousal level (0-1) */
    float valence;                   /**< Valence (-1 to 1) */
} emotional_state_t;

/**
 * @brief Prosodic parameters
 */
typedef struct {
    float pitch_mean_hz;             /**< Mean pitch */
    float pitch_range_hz;            /**< Pitch variation range */
    float pitch_slope;               /**< Overall pitch trend */
    float rate_factor;               /**< Rate multiplier (1.0 = normal) */
    float intensity_factor;          /**< Intensity multiplier */
    float pause_factor;              /**< Pause duration multiplier */
    voice_quality_t voice_quality;   /**< Voice quality setting */
} prosodic_params_t;

/**
 * @brief Prosodic contour point
 */
typedef struct {
    float time_ms;                   /**< Time position */
    float pitch_hz;                  /**< Pitch at this point */
    float intensity;                 /**< Intensity at this point */
    float rate_factor;               /**< Local rate factor */
} prosody_contour_point_t;

/**
 * @brief Complete prosodic contour
 */
typedef struct {
    prosody_contour_point_t* points; /**< Contour points */
    uint32_t point_count;            /**< Number of points */
    float duration_ms;               /**< Total duration */
    emotional_state_t emotion;       /**< Associated emotion */
} prosodic_contour_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t contours_generated;
    uint64_t parameters_computed;
    double avg_processing_time_ms;
    float avg_pitch_modulation;
    float avg_rate_modulation;
} prosody_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

emotional_prosody_config_t emotional_prosody_default_config(void);
emotional_prosody_t* emotional_prosody_create(const emotional_prosody_config_t* config);
void emotional_prosody_destroy(emotional_prosody_t* processor);
bool emotional_prosody_reset(emotional_prosody_t* processor);

/*=============================================================================
 * EMOTION TO PROSODY MAPPING
 *===========================================================================*/

/**
 * @brief Get prosodic parameters for an emotion
 */
bool emotional_prosody_get_params(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion,
    prosodic_params_t* params
);

/**
 * @brief Set current emotional state
 */
bool emotional_prosody_set_emotion(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion
);

/**
 * @brief Get current emotional state
 */
bool emotional_prosody_get_emotion(
    const emotional_prosody_t* processor,
    emotional_state_t* emotion
);

/**
 * @brief Blend two emotional states
 */
bool emotional_prosody_blend_emotions(
    emotional_prosody_t* processor,
    const emotional_state_t* emotion1,
    const emotional_state_t* emotion2,
    float blend_factor,
    emotional_state_t* result
);

/*=============================================================================
 * CONTOUR GENERATION
 *===========================================================================*/

/**
 * @brief Generate prosodic contour for utterance
 */
bool emotional_prosody_generate_contour(
    emotional_prosody_t* processor,
    const char* utterance,
    float duration_ms,
    prosodic_contour_t* contour
);

/**
 * @brief Apply prosodic contour to phoneme sequence
 */
bool emotional_prosody_apply_contour(
    emotional_prosody_t* processor,
    const prosodic_contour_t* contour,
    uint8_t* phonemes,
    uint32_t phoneme_count,
    float* pitch_targets,
    float* duration_targets
);

/**
 * @brief Free contour resources
 */
void emotional_prosody_free_contour(prosodic_contour_t* contour);

/*=============================================================================
 * EMOTION NAMES
 *===========================================================================*/

const char* emotional_prosody_emotion_name(emotion_type_t emotion);
const char* emotional_prosody_quality_name(voice_quality_t quality);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

prosody_status_t emotional_prosody_get_status(const emotional_prosody_t* processor);
prosody_error_t emotional_prosody_get_last_error(const emotional_prosody_t* processor);
bool emotional_prosody_get_stats(const emotional_prosody_t* processor, prosody_stats_t* stats);
void emotional_prosody_reset_stats(emotional_prosody_t* processor);
bool emotional_prosody_get_config(const emotional_prosody_t* processor, emotional_prosody_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool emotional_prosody_register_bio_handler(
    emotional_prosody_t* processor,
    bio_router_t* router
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_PROSODY_H */
