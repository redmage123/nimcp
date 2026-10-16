/**
 * @file nimcp_occipital_audiovisual_bridge.h
 * @brief Bridge between Occipital Cortex and Audio/Speech processing
 *
 * WHAT: Integrates visual processing with audio cortex and Broca's region
 * WHY: Enable audiovisual speech processing (lip reading, gesture-speech binding)
 * HOW: Routes visual features to audio cortex and Broca for multimodal speech
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Sulcus (STS) integrates visual and auditory speech cues
 * - Lip movements precede audio by ~150ms, aiding speech perception
 * - McGurk effect demonstrates visual-audio speech integration
 * - Mirror neurons in premotor cortex link gesture observation to motor plans
 *
 * INTEGRATION PATHS:
 * - Occipital V4/V5 -> STS -> Audio Cortex (visual speech cues to auditory)
 * - Occipital V4 -> Broca's (visual gesture -> motor planning)
 * - Occipital V5/MT -> Broca's (motion -> articulation timing)
 *
 * @version Phase O1: Occipital Audiovisual Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_OCCIPITAL_AUDIOVISUAL_BRIDGE_H
#define NIMCP_OCCIPITAL_AUDIOVISUAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct occipital_adapter occipital_adapter_t;
typedef struct audio_cortex audio_cortex_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct speech_cortex speech_cortex_t;

/* Forward declare bio_router_struct for bio-async (defined in nimcp_bio_router.h) */
struct bio_router_struct;

/* Opaque bridge type */
typedef struct occipital_audiovisual_bridge occipital_audiovisual_bridge_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Audiovisual integration modes
 */
typedef enum {
    OCC_AV_MODE_LIP_READING = 0,   /**< Visual lip movement to phoneme mapping */
    OCC_AV_MODE_GESTURE_SPEECH,     /**< Hand/body gesture to speech binding */
    OCC_AV_MODE_FACE_VOICE,         /**< Face identity to voice binding */
    OCC_AV_MODE_SCENE_SOUND,        /**< Visual scene to sound source binding */
    OCC_AV_MODE_ALL                 /**< Enable all integration modes */
} occipital_av_mode_t;

/**
 * @brief Visual speech feature types
 */
typedef enum {
    VISUAL_SPEECH_LIP_SHAPE = 0,    /**< Lip aperture and rounding */
    VISUAL_SPEECH_LIP_MOTION,       /**< Lip velocity and trajectory */
    VISUAL_SPEECH_JAW_POSITION,     /**< Jaw opening/closing */
    VISUAL_SPEECH_TONGUE_VISIBLE,   /**< Visible tongue position */
    VISUAL_SPEECH_FACE_EXPRESSION,  /**< Facial expression (prosody cue) */
    VISUAL_SPEECH_HEAD_MOTION,      /**< Head nods/shakes (emphasis) */
    VISUAL_SPEECH_HAND_GESTURE,     /**< Co-speech gestures */
    VISUAL_SPEECH_COUNT
} visual_speech_feature_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Integration enables */
    bool enable_lip_reading;          /**< Enable lip-to-phoneme mapping */
    bool enable_gesture_binding;      /**< Enable gesture-speech binding */
    bool enable_face_voice_binding;   /**< Enable face-voice identity binding */
    bool enable_temporal_prediction;  /**< Predict audio from visual timing */
    bool enable_bio_async;            /**< Enable bio-async messaging */

    /* Timing parameters */
    float lip_audio_offset_ms;        /**< Lip-to-audio timing offset (~150ms) */
    float gesture_window_ms;          /**< Gesture-speech binding window */
    float prediction_horizon_ms;      /**< Forward prediction window */

    /* Confidence thresholds */
    float lip_confidence_threshold;   /**< Min confidence for lip features */
    float gesture_confidence_threshold; /**< Min confidence for gestures */
    float binding_strength;           /**< Cross-modal binding strength */

    /* Region of interest */
    float face_roi_x;                 /**< Face ROI center X (normalized) */
    float face_roi_y;                 /**< Face ROI center Y (normalized) */
    float face_roi_radius;            /**< Face ROI radius (normalized) */
} occipital_av_config_t;

/**
 * @brief Visual speech observation
 */
typedef struct {
    visual_speech_feature_t type;     /**< Feature type */
    float features[16];               /**< Feature vector */
    uint32_t feature_dim;             /**< Feature dimensionality */
    float confidence;                 /**< Detection confidence */
    float x, y;                       /**< Feature location (normalized) */
    uint64_t timestamp_us;            /**< Observation timestamp */
} visual_speech_observation_t;

/**
 * @brief Audiovisual binding event
 */
typedef struct {
    uint32_t visual_id;               /**< Visual feature ID */
    uint32_t audio_id;                /**< Audio feature ID (phoneme/word) */
    float binding_strength;           /**< Binding confidence */
    float temporal_offset_ms;         /**< Visual-audio timing offset */
    bool is_prediction;               /**< True if predicted, not observed */
} av_binding_event_t;

/**
 * @brief Bridge effects on processing
 */
typedef struct {
    /* Lip reading effects */
    float lip_phoneme_confidence;     /**< Confidence in lip-phoneme mapping */
    float predicted_phoneme_id;       /**< Predicted next phoneme from lips */
    float lip_audio_coherence;        /**< Visual-audio coherence score */

    /* Gesture effects */
    float gesture_speech_binding;     /**< Gesture-speech binding strength */
    float gesture_emphasis_boost;     /**< Emphasis from gesture detection */

    /* Overall effects */
    float visual_speech_saliency;     /**< Visual speech cue saliency */
    float multimodal_integration;     /**< Cross-modal integration quality */
    float prediction_accuracy;        /**< How well visual predicts audio */
} occipital_av_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t lip_observations;        /**< Lip feature observations */
    uint64_t gesture_observations;    /**< Gesture observations */
    uint64_t bindings_created;        /**< Audiovisual bindings created */
    uint64_t predictions_made;        /**< Temporal predictions made */
    uint64_t predictions_correct;     /**< Correct predictions */
    float avg_binding_strength;       /**< Average binding confidence */
    float avg_prediction_error_ms;    /**< Average timing prediction error */
    uint64_t messages_sent;           /**< Bio-async messages sent */
    uint64_t messages_received;       /**< Bio-async messages received */
} occipital_av_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
occipital_av_config_t occipital_av_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create audiovisual bridge
 *
 * @param occipital Occipital adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
occipital_audiovisual_bridge_t* occipital_av_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_av_config_t* config);

/**
 * @brief Destroy audiovisual bridge
 */
void occipital_av_bridge_destroy(occipital_audiovisual_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int occipital_av_bridge_reset(occipital_audiovisual_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to audio cortex for visual-audio integration
 *
 * @param bridge Bridge instance
 * @param audio Audio cortex instance
 * @return 0 on success, -1 on failure
 */
int occipital_av_connect_audio_cortex(
    occipital_audiovisual_bridge_t* bridge,
    audio_cortex_t* audio);

/**
 * @brief Connect to Broca's region for gesture-motor integration
 *
 * @param bridge Bridge instance
 * @param broca Broca adapter instance
 * @return 0 on success, -1 on failure
 */
int occipital_av_connect_broca(
    occipital_audiovisual_bridge_t* bridge,
    broca_adapter_t* broca);

/**
 * @brief Connect to speech cortex for phoneme integration
 *
 * @param bridge Bridge instance
 * @param speech Speech cortex instance
 * @return 0 on success, -1 on failure
 */
int occipital_av_connect_speech_cortex(
    occipital_audiovisual_bridge_t* bridge,
    speech_cortex_t* speech);

/**
 * @brief Register with bio-async router
 */
int occipital_av_bridge_register_bio_async(
    occipital_audiovisual_bridge_t* bridge,
    struct bio_router_struct* router);

/*=============================================================================
 * Processing API
 *===========================================================================*/

/**
 * @brief Process visual speech observation
 *
 * @param bridge Bridge instance
 * @param observation Visual speech observation
 * @return 0 on success, -1 on failure
 */
int occipital_av_process_observation(
    occipital_audiovisual_bridge_t* bridge,
    const visual_speech_observation_t* observation);

/**
 * @brief Update bridge state from occipital processing
 *
 * Called after occipital adapter processes a frame to extract
 * visual speech features and update audiovisual bindings.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int occipital_av_bridge_update(occipital_audiovisual_bridge_t* bridge);

/**
 * @brief Get current effects on processing
 */
int occipital_av_bridge_get_effects(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_effects_t* effects);

/**
 * @brief Apply effects to connected modules
 */
int occipital_av_bridge_apply_effects(occipital_audiovisual_bridge_t* bridge);

/*=============================================================================
 * Prediction API
 *===========================================================================*/

/**
 * @brief Predict audio timing from visual cues
 *
 * Uses lip motion to predict when the next phoneme will occur,
 * allowing audio processing to prepare for incoming speech.
 *
 * @param bridge Bridge instance
 * @param predicted_onset_ms Output: predicted onset time in ms
 * @param predicted_phoneme_id Output: predicted phoneme ID
 * @param confidence Output: prediction confidence
 * @return 0 on success, -1 on failure
 */
int occipital_av_predict_audio_timing(
    occipital_audiovisual_bridge_t* bridge,
    float* predicted_onset_ms,
    uint32_t* predicted_phoneme_id,
    float* confidence);

/**
 * @brief Report actual audio event for prediction learning
 *
 * @param bridge Bridge instance
 * @param actual_onset_ms Actual phoneme onset time
 * @param actual_phoneme_id Actual phoneme ID
 * @return 0 on success, -1 on failure
 */
int occipital_av_report_audio_event(
    occipital_audiovisual_bridge_t* bridge,
    float actual_onset_ms,
    uint32_t actual_phoneme_id);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int occipital_av_bridge_get_stats(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_stats_t* stats);

/**
 * @brief Reset statistics
 */
void occipital_av_bridge_reset_stats(occipital_audiovisual_bridge_t* bridge);

/**
 * @brief Check if audio cortex is connected
 */
bool occipital_av_is_audio_connected(const occipital_audiovisual_bridge_t* bridge);

/**
 * @brief Check if Broca is connected
 */
bool occipital_av_is_broca_connected(const occipital_audiovisual_bridge_t* bridge);

/**
 * @brief Get configuration
 */
int occipital_av_bridge_get_config(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_AUDIOVISUAL_BRIDGE_H */
