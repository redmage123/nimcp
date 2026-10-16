/**
 * @file nimcp_multimodal_language.h
 * @brief Multimodal language integration for speech
 *
 * WHAT: Integrates gestures, facial expressions, and gaze with speech
 * WHY:  Enable natural, coordinated multimodal communication
 * HOW:  Synchronize non-verbal cues with verbal output
 *
 * ARCHITECTURE:
 * - Gesture Controller: Manages co-speech gestures
 * - Expression Manager: Controls facial expressions
 * - Gaze Director: Coordinates eye movements
 * - Sync Engine: Aligns modalities with speech timing
 *
 * BIOLOGICAL BASIS:
 * - Superior temporal sulcus for multimodal integration
 * - Mirror neuron system for gesture-speech coordination
 * - Frontal eye fields for gaze control
 *
 * MODALITY TYPES:
 * - Iconic Gestures: Represent concrete objects/actions
 * - Metaphoric Gestures: Represent abstract concepts
 * - Beat Gestures: Mark speech rhythm
 * - Deictic Gestures: Point to referents
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#ifndef NIMCP_MULTIMODAL_LANGUAGE_H
#define NIMCP_MULTIMODAL_LANGUAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

typedef struct multimodal_language multimodal_language_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define MULTIMODAL_DEFAULT_MAX_GESTURES      32
#define MULTIMODAL_DEFAULT_MAX_EXPRESSIONS   16
#define MULTIMODAL_DEFAULT_SYNC_TOLERANCE_MS 100

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Gesture types
 */
typedef enum {
    GESTURE_TYPE_NONE = 0,
    GESTURE_TYPE_ICONIC,         /**< Depicts object/action */
    GESTURE_TYPE_METAPHORIC,     /**< Abstract concept */
    GESTURE_TYPE_BEAT,           /**< Rhythmic emphasis */
    GESTURE_TYPE_DEICTIC,        /**< Pointing */
    GESTURE_TYPE_EMBLEMATIC,     /**< Conventional meaning */
    GESTURE_TYPE_COUNT
} gesture_type_t;

/**
 * @brief Facial expression types
 */
typedef enum {
    EXPRESSION_NEUTRAL = 0,
    EXPRESSION_SMILE,
    EXPRESSION_FROWN,
    EXPRESSION_RAISED_EYEBROWS,
    EXPRESSION_SQUINT,
    EXPRESSION_WIDE_EYES,
    EXPRESSION_PURSED_LIPS,
    EXPRESSION_COUNT
} expression_type_t;

/**
 * @brief Gaze targets
 */
typedef enum {
    GAZE_TARGET_FORWARD = 0,
    GAZE_TARGET_ADDRESSEE,
    GAZE_TARGET_OBJECT,
    GAZE_TARGET_AWAY,
    GAZE_TARGET_UP,
    GAZE_TARGET_DOWN,
    GAZE_TARGET_COUNT
} gaze_target_t;

/**
 * @brief Modality types
 */
typedef enum {
    MODALITY_SPEECH = 0,
    MODALITY_GESTURE,
    MODALITY_EXPRESSION,
    MODALITY_GAZE,
    MODALITY_COUNT
} modality_t;

/**
 * @brief Processing status
 */
typedef enum {
    MULTIMODAL_STATUS_IDLE = 0,
    MULTIMODAL_STATUS_PLANNING,
    MULTIMODAL_STATUS_SYNCHRONIZING,
    MULTIMODAL_STATUS_READY,
    MULTIMODAL_STATUS_ERROR
} multimodal_status_t;

typedef enum {
    MULTIMODAL_ERROR_NONE = 0,
    MULTIMODAL_ERROR_INVALID_INPUT,
    MULTIMODAL_ERROR_SYNC_FAILED,
    MULTIMODAL_ERROR_BUFFER_FULL,
    MULTIMODAL_ERROR_INTERNAL
} multimodal_error_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Configuration
 */
typedef struct {
    uint32_t max_gestures;           /**< Max gestures per utterance */
    uint32_t max_expressions;        /**< Max expression changes */
    uint32_t sync_tolerance_ms;      /**< Timing tolerance */
    bool enable_auto_gestures;       /**< Auto-generate gestures */
    bool enable_auto_expressions;    /**< Auto-generate expressions */
    bool enable_gaze_tracking;       /**< Include gaze coordination */
    bool enable_bio_async;           /**< Bio-async messaging */
} multimodal_config_t;

/**
 * @brief Gesture specification
 */
typedef struct {
    uint32_t gesture_id;             /**< Unique identifier */
    gesture_type_t type;             /**< Gesture type */
    float start_time_ms;             /**< Start relative to speech */
    float duration_ms;               /**< Gesture duration */
    float intensity;                 /**< Movement intensity (0-1) */

    /* Spatial parameters */
    float hand_position[3];          /**< x, y, z position */
    float hand_direction[3];         /**< Direction vector */
    uint8_t hand;                    /**< 0=left, 1=right, 2=both */

    /* Association */
    char associated_word[32];        /**< Word this accompanies */
    uint32_t word_position;          /**< Position in utterance */
} gesture_spec_t;

/**
 * @brief Expression specification
 */
typedef struct {
    uint32_t expression_id;          /**< Unique identifier */
    expression_type_t type;          /**< Expression type */
    float start_time_ms;             /**< Start time */
    float duration_ms;               /**< Duration */
    float intensity;                 /**< Expression intensity (0-1) */
    char associated_content[64];     /**< Associated speech content */
} expression_spec_t;

/**
 * @brief Gaze specification
 */
typedef struct {
    gaze_target_t target;            /**< Where to look */
    float start_time_ms;             /**< Start time */
    float duration_ms;               /**< Duration */
    float target_position[3];        /**< Specific position if applicable */
    char associated_word[32];        /**< Associated word */
} gaze_spec_t;

/**
 * @brief Multimodal plan
 */
typedef struct {
    /* Speech timing */
    float speech_duration_ms;        /**< Total speech duration */
    char utterance[512];             /**< The utterance */

    /* Gestures */
    gesture_spec_t* gestures;
    uint32_t gesture_count;

    /* Expressions */
    expression_spec_t* expressions;
    uint32_t expression_count;

    /* Gaze */
    gaze_spec_t* gaze_events;
    uint32_t gaze_count;

    /* Sync quality */
    float sync_score;                /**< How well synchronized */
} multimodal_plan_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t plans_generated;
    uint64_t gestures_generated;
    uint64_t expressions_generated;
    uint64_t gaze_events_generated;
    double avg_sync_score;
    double avg_processing_time_ms;
} multimodal_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

multimodal_config_t multimodal_lang_default_config(void);
multimodal_language_t* multimodal_lang_create(const multimodal_config_t* config);
void multimodal_lang_destroy(multimodal_language_t* processor);
bool multimodal_lang_reset(multimodal_language_t* processor);

/*=============================================================================
 * PLAN GENERATION
 *===========================================================================*/

/**
 * @brief Generate multimodal plan for utterance
 */
bool multimodal_lang_generate_plan(
    multimodal_language_t* processor,
    const char* utterance,
    float speech_duration_ms,
    multimodal_plan_t* plan
);

/**
 * @brief Add gesture to plan
 */
bool multimodal_lang_add_gesture(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const gesture_spec_t* gesture
);

/**
 * @brief Add expression to plan
 */
bool multimodal_lang_add_expression(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const expression_spec_t* expression
);

/**
 * @brief Add gaze event to plan
 */
bool multimodal_lang_add_gaze(
    multimodal_language_t* processor,
    multimodal_plan_t* plan,
    const gaze_spec_t* gaze
);

/**
 * @brief Free plan resources
 */
void multimodal_lang_free_plan(multimodal_plan_t* plan);

/*=============================================================================
 * SYNCHRONIZATION
 *===========================================================================*/

/**
 * @brief Synchronize plan timing
 */
bool multimodal_lang_synchronize_plan(
    multimodal_language_t* processor,
    multimodal_plan_t* plan
);

/**
 * @brief Get events at time point
 */
bool multimodal_lang_get_events_at_time(
    const multimodal_plan_t* plan,
    float time_ms,
    gesture_spec_t* gesture,
    expression_spec_t* expression,
    gaze_spec_t* gaze
);

/*=============================================================================
 * AUTO-GENERATION
 *===========================================================================*/

/**
 * @brief Auto-generate gestures for utterance
 */
uint32_t multimodal_lang_auto_gestures(
    multimodal_language_t* processor,
    const char* utterance,
    gesture_spec_t* gestures,
    uint32_t max_gestures
);

/**
 * @brief Auto-generate expressions for utterance
 */
uint32_t multimodal_lang_auto_expressions(
    multimodal_language_t* processor,
    const char* utterance,
    float emotion_valence,
    expression_spec_t* expressions,
    uint32_t max_expressions
);

/*=============================================================================
 * TYPE NAMES
 *===========================================================================*/

const char* multimodal_lang_gesture_name(gesture_type_t type);
const char* multimodal_lang_expression_name(expression_type_t type);
const char* multimodal_lang_gaze_name(gaze_target_t target);

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

multimodal_status_t multimodal_lang_get_status(const multimodal_language_t* processor);
multimodal_error_t multimodal_lang_get_last_error(const multimodal_language_t* processor);
bool multimodal_lang_get_stats(const multimodal_language_t* processor, multimodal_stats_t* stats);
void multimodal_lang_reset_stats(multimodal_language_t* processor);
bool multimodal_lang_get_config(const multimodal_language_t* processor, multimodal_config_t* config);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool multimodal_lang_register_bio_handler(
    multimodal_language_t* processor,
    bio_router_t* router
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTIMODAL_LANGUAGE_H */
