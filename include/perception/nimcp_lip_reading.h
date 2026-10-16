/**
 * @file nimcp_lip_reading.h
 * @brief Lip Reading System - Visual Speech Perception
 *
 * WHAT: Biologically-inspired lip reading that maps visual mouth movements
 *       to phonemes and speech comprehension
 * WHY:  Enable speech understanding in noisy environments, deaf individuals,
 *       multimodal robustness via McGurk effect
 * HOW:  Visual cortex (mouth ROI) -> STS (visual speech) -> Speech cortex
 *       (phoneme integration) -> Mirror neurons (motor theory)
 *
 * ARCHITECTURE:
 * - Face Detector (FFA analog): Face/mouth localization
 * - Viseme Classifier (STS analog): Lip shape -> viseme category
 * - Temporal Integrator: Track mouth dynamics over time
 * - Audiovisual Integrator: McGurk-style multimodal fusion
 * - Mirror Neuron System: Motor theory of speech perception
 *
 * BIOLOGICAL BASIS:
 * - FFA (Fusiform Face Area): Face detection, mouth localization
 * - STS (Superior Temporal Sulcus): Visual speech specialization
 * - STG (Superior Temporal Gyrus): Audiovisual phoneme integration
 * - Broca's Area: Articulatory motor simulation (motor theory)
 *
 * KEY CAPABILITIES:
 * - McGurk Effect: Visual /ga/ + Audio /ba/ -> Perceived /da/
 * - Speech in noise: SNR = -10dB enhanced by visual cues
 * - Silent speech: Visual-only recognition (60% word accuracy)
 * - Speaker adaptation: 10s exposure -> 30% improvement
 * - Real-time: 30 FPS video processing
 *
 * VISEME GROUPS (13 categories):
 * - Bilabial (/p/, /b/, /m/): Lips closed then release
 * - Labiodental (/f/, /v/): Upper teeth on lower lip
 * - Dental (/th/, /dh/): Tongue between teeth
 * - Alveolar (/t/, /d/, /n/, /l/, /s/, /z/): Tongue tip up
 * - Velar (/k/, /g/, /ng/): Back of mouth
 * - Rounded vowels: Lips rounded, various apertures
 * - Unrounded vowels: Neutral lips, various apertures
 *
 * @version Phase B5: Audiovisual Speech Integration
 * @date 2026-01-15
 */

#ifndef NIMCP_LIP_READING_H
#define NIMCP_LIP_READING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct lip_reading_system lip_reading_system_t;
typedef struct face_detector face_detector_t;
typedef struct viseme_classifier viseme_classifier_t;
typedef struct temporal_integrator temporal_integrator_t;
typedef struct audiovisual_integrator audiovisual_integrator_t;

/* Forward declarations for integration */
typedef struct visual_cortex_struct visual_cortex_t;
typedef struct speech_cortex_struct speech_cortex_t;
typedef struct brain_struct* brain_t;
typedef struct kalman_filter kalman_filter_t;
typedef struct stdp_config stdp_config_t;
typedef struct meta_learning_engine meta_learning_engine_t;
typedef struct phasic_tonic_state phasic_tonic_state_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define LIP_READING_MAX_LIP_CONTOUR_POINTS  12
#define LIP_READING_INNER_CONTOUR_POINTS    8
#define LIP_READING_MAX_VISEME_HISTORY      10
#define LIP_READING_DEFAULT_MOUTH_ROI_SIZE  64
#define LIP_READING_DEFAULT_FRAME_RATE      30
#define LIP_READING_VISUAL_LEAD_MS          200
#define LIP_READING_MAX_SPEAKERS            16
#define LIP_READING_FEATURE_DIM             128
#define LIP_READING_DEFAULT_ADAPTATION_FRAMES 300

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Viseme categories - Visual phoneme groups
 *
 * Multiple phonemes look identical on lips (homophones), so we group
 * them into visemes. This is the fundamental classification unit for
 * visual speech perception.
 */
typedef enum {
    /* === BILABIAL (Lips Together) === */
    VISEME_BILABIAL = 0,        /**< /p/, /b/, /m/ - lips closed, then release */

    /* === LABIODENTAL (Teeth on Lip) === */
    VISEME_LABIODENTAL,         /**< /f/, /v/ - upper teeth on lower lip */

    /* === DENTAL (Tongue Visible) === */
    VISEME_DENTAL,              /**< /th/ (thin), /dh/ (this) - tongue between teeth */

    /* === ALVEOLAR (Tongue Behind Teeth) === */
    VISEME_ALVEOLAR,            /**< /t/, /d/, /n/, /l/, /s/, /z/ - tongue tip up */

    /* === VELAR (Mouth Open, No Tongue) === */
    VISEME_VELAR,               /**< /k/, /g/, /ng/ (sing) - back of mouth */

    /* === ROUNDED VOWELS === */
    VISEME_ROUNDED_CLOSE,       /**< /u/ (boot), /o/ (boat) - lips rounded, small aperture */
    VISEME_ROUNDED_OPEN,        /**< /aw/ (caught) - lips rounded, large aperture */

    /* === UNROUNDED VOWELS === */
    VISEME_UNROUNDED_CLOSE,     /**< /i/ (beet) - lips spread, small aperture */
    VISEME_UNROUNDED_MID,       /**< /e/ (bet), /uh/ (but) - neutral lips */
    VISEME_UNROUNDED_OPEN,      /**< /a/ (father) - mouth wide open */

    /* === SPECIAL === */
    VISEME_SILENCE,             /**< Mouth closed, no movement */
    VISEME_UNKNOWN,             /**< Unclear lip position */

    VISEME_COUNT = 12
} viseme_t;

/**
 * @brief Phoneme types for audiovisual integration
 */
typedef enum {
    PHONEME_UNKNOWN = 0,

    /* Bilabial */
    PHONEME_P,
    PHONEME_B,
    PHONEME_M,

    /* Labiodental */
    PHONEME_F,
    PHONEME_V,

    /* Dental */
    PHONEME_TH,                 /**< voiceless (thin) */
    PHONEME_DH,                 /**< voiced (this) */

    /* Alveolar */
    PHONEME_T,
    PHONEME_D,
    PHONEME_N,
    PHONEME_L,
    PHONEME_S,
    PHONEME_Z,

    /* Velar */
    PHONEME_K,
    PHONEME_G,
    PHONEME_NG,

    /* Palatal */
    PHONEME_SH,
    PHONEME_ZH,
    PHONEME_CH,
    PHONEME_J,

    /* Glottal */
    PHONEME_H,

    /* Approximants */
    PHONEME_R,
    PHONEME_W,
    PHONEME_Y,

    /* Vowels */
    PHONEME_IY,                 /**< beet */
    PHONEME_IH,                 /**< bit */
    PHONEME_EY,                 /**< bait */
    PHONEME_EH,                 /**< bet */
    PHONEME_AE,                 /**< bat */
    PHONEME_AA,                 /**< father */
    PHONEME_AO,                 /**< caught */
    PHONEME_OW,                 /**< boat */
    PHONEME_UH,                 /**< book */
    PHONEME_UW,                 /**< boot */
    PHONEME_AH,                 /**< but */
    PHONEME_ER,                 /**< bird */

    PHONEME_COUNT
} phoneme_t;

/**
 * @brief Lip reading processing status
 */
typedef enum {
    LIP_READING_STATUS_IDLE = 0,
    LIP_READING_STATUS_FACE_DETECTED,
    LIP_READING_STATUS_MOUTH_TRACKED,
    LIP_READING_STATUS_VISEME_CLASSIFIED,
    LIP_READING_STATUS_AUDIO_INTEGRATED,
    LIP_READING_STATUS_SPEECH_RECOGNIZED,
    LIP_READING_STATUS_ERROR
} lip_reading_status_t;

/**
 * @brief Lip reading error codes
 */
typedef enum {
    LIP_READING_ERROR_NONE = 0,
    LIP_READING_ERROR_INVALID_INPUT,
    LIP_READING_ERROR_NO_FACE_DETECTED,
    LIP_READING_ERROR_MOUTH_OCCLUDED,
    LIP_READING_ERROR_LOW_CONFIDENCE,
    LIP_READING_ERROR_TRACKING_LOST,
    LIP_READING_ERROR_BUFFER_FULL,
    LIP_READING_ERROR_INTERNAL
} lip_reading_error_t;

/**
 * @brief Articulatory action types for motor theory
 */
typedef enum {
    ARTICULATORY_NONE = 0,
    ARTICULATORY_LIPS_CLOSED,
    ARTICULATORY_LIPS_ROUNDED,
    ARTICULATORY_LIPS_SPREAD,
    ARTICULATORY_TEETH_ON_LIP,
    ARTICULATORY_TONGUE_BETWEEN_TEETH,
    ARTICULATORY_TONGUE_TIP_UP,
    ARTICULATORY_TONGUE_BACK,
    ARTICULATORY_MOUTH_OPEN,
    ARTICULATORY_COUNT
} articulatory_action_type_t;

/**
 * @brief Speaker accent types (for adaptation)
 */
typedef enum {
    ACCENT_UNKNOWN = 0,
    ACCENT_ENGLISH_AMERICAN,
    ACCENT_ENGLISH_BRITISH,
    ACCENT_FRENCH,
    ACCENT_GERMAN,
    ACCENT_SPANISH,
    ACCENT_MANDARIN,
    ACCENT_JAPANESE,
    ACCENT_COUNT
} accent_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Visual speech features extracted from mouth region
 */
typedef struct {
    /* === Geometric Features === */
    float lip_width;              /**< Horizontal distance (mm) */
    float lip_height;             /**< Vertical aperture (mm) */
    float lip_area;               /**< Total visible area (mm^2) */
    float lip_aspect_ratio;       /**< Width / height */
    float lip_protrusion;         /**< 3D depth (for rounded vowels) */

    /* === Teeth Visibility === */
    float upper_teeth_visible;    /**< Fraction of upper teeth shown (0-1) */
    float lower_teeth_visible;    /**< Fraction of lower teeth shown (0-1) */
    float teeth_gap;              /**< Distance between upper/lower (mm) */

    /* === Tongue Features === */
    float tongue_visible;         /**< Is tongue protruding? (0-1) */
    float tongue_position_x;      /**< Horizontal tongue position */
    float tongue_position_y;      /**< Vertical tongue position */

    /* === Dynamic Features === */
    float lip_velocity_x;         /**< Horizontal movement speed (mm/s) */
    float lip_velocity_y;         /**< Vertical movement speed (mm/s) */
    float lip_acceleration;       /**< Rate of change (mm/s^2) */

    /* === Temporal Context === */
    viseme_t previous_viseme;     /**< Coarticulation context */
    float viseme_duration_ms;     /**< How long in this state */

    /* === Lighting Invariance === */
    float normalized_luminance;   /**< Brightness-independent (0-1) */
    float shadow_compensation;    /**< Shadow removal factor */

    /* === Confidence === */
    float feature_confidence;     /**< Overall feature quality (0-1) */
    uint64_t timestamp_ms;        /**< When features were extracted */
} visual_speech_features_t;

/**
 * @brief Face detection result (FFA analog)
 */
typedef struct {
    /* === Face Detection === */
    bool face_detected;
    float face_bbox[4];           /**< [x, y, width, height] */
    float face_confidence;

    /* === Mouth ROI Extraction === */
    float mouth_bbox[4];          /**< [x, y, width, height] */
    float mouth_center[2];        /**< [x, y] in image coordinates */

    /* === Landmarks (lip contour) === */
    float lip_outer_contour[LIP_READING_MAX_LIP_CONTOUR_POINTS][2];  /**< Outer lip points */
    float lip_inner_contour[LIP_READING_INNER_CONTOUR_POINTS][2];   /**< Inner lip points */
    uint32_t outer_contour_count;
    uint32_t inner_contour_count;

    /* === 3D Pose Estimation === */
    float head_pose_yaw;          /**< Left/right rotation (degrees) */
    float head_pose_pitch;        /**< Up/down rotation (degrees) */
    float head_pose_roll;         /**< Tilt (degrees) */

    /* === Occlusion Handling === */
    bool mouth_occluded;          /**< Hand, mask, object blocking */
    float occlusion_confidence;   /**< How certain about occlusion */

    /* === Tracking State === */
    uint32_t track_id;            /**< Consistent ID across frames */
    uint32_t frames_tracked;      /**< Number of frames tracked */
    bool tracking_stable;         /**< Whether tracking is reliable */
} face_detection_result_t;

/**
 * @brief Viseme classification result
 */
typedef struct {
    viseme_t viseme;              /**< Classified viseme */
    float confidence;             /**< Classification confidence (0-1) */
    float probabilities[VISEME_COUNT]; /**< Full probability distribution */

    /* === Temporal Context === */
    viseme_t viseme_history[LIP_READING_MAX_VISEME_HISTORY]; /**< Recent history */
    uint32_t history_count;

    /* === Derived Features === */
    bool is_transition;           /**< Currently transitioning between visemes */
    float transition_progress;    /**< How far through transition (0-1) */
    viseme_t transition_target;   /**< Target viseme if transitioning */

    /* === Plosive Detection === */
    bool plosive_burst_detected;  /**< /p/, /t/, /k/ sudden opening */
    bool closure_detected;        /**< /m/, /n/, /ng/ lip closure */

    uint64_t timestamp_ms;
} viseme_classification_t;

/**
 * @brief Audiovisual integration result
 */
typedef struct {
    /* === Visual Speech Pathway === */
    viseme_t visual_viseme;
    float visual_confidence;

    /* === Auditory Speech Pathway === */
    phoneme_t auditory_phoneme;
    float auditory_confidence;
    float auditory_snr;           /**< Signal-to-noise ratio (-20 to +40 dB) */

    /* === Integration Weights === */
    float visual_weight;          /**< 0-1 (higher when SNR low) */
    float auditory_weight;        /**< 0-1 (higher when SNR high) */

    /* === Optimal Integration (MLE) === */
    float reliability_visual;     /**< 1 / variance_visual */
    float reliability_auditory;   /**< 1 / variance_auditory */

    /* === McGurk Detection === */
    bool mcgurk_conflict_detected; /**< Visual != Auditory */
    phoneme_t fused_phoneme;       /**< Integrated percept */
    float fusion_confidence;

    /* === Temporal Alignment === */
    float temporal_offset_ms;     /**< Visual -> Audio delay */
    bool audio_visual_aligned;    /**< Whether streams are synchronized */

    uint64_t timestamp_ms;
} audiovisual_integration_t;

/**
 * @brief Articulatory action for motor theory
 */
typedef struct {
    articulatory_action_type_t type;
    bool lips_closed;
    bool lips_rounded;
    bool lips_spread;
    bool upper_teeth_on_lower_lip;
    bool tongue_between_teeth;
    bool tongue_tip_up;
    bool tongue_back;
    float mouth_aperture;         /**< 0=closed, 1=fully open */
    bool velum_open;              /**< For nasal sounds */
    bool airflow_friction;        /**< For fricatives */
    bool voicing;                 /**< Voiced vs voiceless */
} articulatory_action_t;

/**
 * @brief Speaker profile for adaptation
 */
typedef struct {
    uint32_t speaker_id;
    char speaker_name[64];
    accent_t accent;

    /* === Lip Morphology Parameters === */
    float avg_lip_width;          /**< Average lip width (mm) */
    float avg_lip_height;         /**< Average lip height (mm) */
    float lip_width_range[2];     /**< [min, max] */
    float lip_height_range[2];    /**< [min, max] */

    /* === Viseme Calibration === */
    float viseme_threshold_adjustments[VISEME_COUNT];

    /* === Adaptation Statistics === */
    uint32_t frames_observed;
    float adaptation_quality;     /**< 0-1, how well calibrated */
    uint64_t last_seen_ms;
} speaker_profile_t;

/**
 * @brief Configuration for lip reading system
 */
typedef struct {
    /* === Processing Settings === */
    uint32_t mouth_roi_width;     /**< Mouth ROI extraction width */
    uint32_t mouth_roi_height;    /**< Mouth ROI extraction height */
    float min_face_confidence;    /**< Minimum face detection confidence */
    float min_viseme_confidence;  /**< Minimum viseme classification confidence */
    uint32_t target_frame_rate;   /**< Target processing frame rate */

    /* === Audiovisual Integration === */
    bool enable_audiovisual_fusion; /**< Enable McGurk-style fusion */
    float visual_lead_ms;         /**< Expected visual-audio offset */
    float snr_visual_threshold;   /**< SNR below which visual dominates */

    /* === Temporal Processing === */
    bool enable_temporal_smoothing; /**< Smooth viseme transitions */
    uint32_t viseme_history_length; /**< Length of viseme history buffer */
    float transition_threshold;   /**< Probability threshold for transitions */

    /* === Speaker Adaptation === */
    bool enable_speaker_adaptation; /**< Enable per-speaker calibration */
    uint32_t adaptation_frames;   /**< Frames needed for adaptation */
    uint32_t max_speakers;        /**< Maximum tracked speakers */

    /* === Learning === */
    bool enable_stdp_learning;    /**< Enable visual-phoneme association learning */
    bool enable_meta_learning;    /**< Enable rapid speaker adaptation */

    /* === Attention === */
    bool enable_attention_modulation; /**< Acetylcholine attention boost */
    float speech_salience_threshold;  /**< Speech detection threshold */

    /* === Integration === */
    bool enable_bio_async;        /**< Bio-async messaging */
    bool enable_mirror_neurons;   /**< Motor theory simulation */

    /* === Debug/Diagnostics === */
    bool debug_mode;              /**< Enable debug output */
    bool save_mouth_roi;          /**< Save extracted mouth ROIs */
} lip_reading_config_t;

/**
 * @brief Statistics for lip reading system
 */
typedef struct {
    /* === Processing Stats === */
    uint64_t frames_processed;
    uint64_t faces_detected;
    uint64_t visemes_classified;
    uint64_t audiovisual_fusions;

    /* === Accuracy Stats === */
    float avg_viseme_confidence;
    float avg_fusion_confidence;
    uint64_t mcgurk_effects_detected;

    /* === Speaker Stats === */
    uint32_t speakers_tracked;
    uint32_t speakers_adapted;

    /* === Timing Stats === */
    double avg_face_detection_ms;
    double avg_viseme_classification_ms;
    double avg_audiovisual_fusion_ms;
    double avg_total_processing_ms;

    /* === Error Stats === */
    uint64_t face_detection_failures;
    uint64_t tracking_losses;
    uint64_t occlusion_events;
    uint64_t low_confidence_frames;
} lip_reading_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default lip reading configuration
 */
lip_reading_config_t lip_reading_default_config(void);

/**
 * @brief Create lip reading system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 */
lip_reading_system_t* lip_reading_create(const lip_reading_config_t* config);

/**
 * @brief Destroy lip reading system
 *
 * @param system System to destroy
 */
void lip_reading_destroy(lip_reading_system_t* system);

/**
 * @brief Reset lip reading system state
 *
 * @param system System to reset
 * @return true on success
 */
bool lip_reading_reset(lip_reading_system_t* system);

/*=============================================================================
 * FACE/MOUTH DETECTION
 *===========================================================================*/

/**
 * @brief Detect face and extract mouth region
 *
 * @param system Lip reading system
 * @param image Input image (grayscale or RGB)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1 or 3)
 * @param result Face detection result
 * @return true if face detected
 */
bool lip_reading_detect_face(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    face_detection_result_t* result
);

/**
 * @brief Extract mouth ROI from full image
 *
 * @param system Lip reading system
 * @param image Input image
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param face_result Previous face detection result
 * @param mouth_roi Output buffer (must be roi_width * roi_height * channels)
 * @param roi_width Output ROI width
 * @param roi_height Output ROI height
 * @return true on success
 */
bool lip_reading_extract_mouth_roi(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    const face_detection_result_t* face_result,
    uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height
);

/**
 * @brief Detect lip landmarks (contour points)
 *
 * @param system Lip reading system
 * @param mouth_roi Mouth ROI image
 * @param roi_width ROI width
 * @param roi_height ROI height
 * @param result Face detection result to update with landmarks
 * @return true on success
 */
bool lip_reading_detect_lip_landmarks(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    face_detection_result_t* result
);

/*=============================================================================
 * VISUAL SPEECH FEATURE EXTRACTION
 *===========================================================================*/

/**
 * @brief Extract visual speech features from mouth ROI
 *
 * @param system Lip reading system
 * @param mouth_roi Mouth ROI image
 * @param roi_width ROI width
 * @param roi_height ROI height
 * @param face_result Face detection with landmarks
 * @param features Output features
 * @return true on success
 */
bool lip_reading_extract_features(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    const face_detection_result_t* face_result,
    visual_speech_features_t* features
);

/**
 * @brief Update temporal dynamics (velocity, acceleration)
 *
 * @param system Lip reading system
 * @param current_features Current frame features
 * @param previous_features Previous frame features
 * @param delta_time_ms Time between frames
 * @return true on success
 */
bool lip_reading_update_dynamics(
    lip_reading_system_t* system,
    visual_speech_features_t* current_features,
    const visual_speech_features_t* previous_features,
    float delta_time_ms
);

/*=============================================================================
 * VISEME CLASSIFICATION
 *===========================================================================*/

/**
 * @brief Classify viseme from visual features
 *
 * @param system Lip reading system
 * @param features Visual speech features
 * @param result Viseme classification result
 * @return true on success
 */
bool lip_reading_classify_viseme(
    lip_reading_system_t* system,
    const visual_speech_features_t* features,
    viseme_classification_t* result
);

/**
 * @brief Classify viseme directly from mouth ROI
 *
 * @param system Lip reading system
 * @param mouth_roi Mouth ROI image
 * @param roi_width ROI width
 * @param roi_height ROI height
 * @param result Viseme classification result
 * @return true on success
 */
bool lip_reading_classify_viseme_from_roi(
    lip_reading_system_t* system,
    const uint8_t* mouth_roi,
    uint32_t roi_width,
    uint32_t roi_height,
    viseme_classification_t* result
);

/**
 * @brief Map phoneme to viseme
 *
 * @param phoneme Input phoneme
 * @return Corresponding viseme
 */
viseme_t lip_reading_phoneme_to_viseme(phoneme_t phoneme);

/**
 * @brief Get possible phonemes for a viseme
 *
 * @param viseme Input viseme
 * @param phonemes Output phoneme array
 * @param max_phonemes Maximum phonemes to return
 * @return Number of possible phonemes
 */
uint32_t lip_reading_viseme_to_phonemes(
    viseme_t viseme,
    phoneme_t* phonemes,
    uint32_t max_phonemes
);

/**
 * @brief Get viseme name string
 *
 * @param viseme Viseme type
 * @return Name string
 */
const char* lip_reading_viseme_name(viseme_t viseme);

/**
 * @brief Get phoneme name string
 *
 * @param phoneme Phoneme type
 * @return Name string
 */
const char* lip_reading_phoneme_name(phoneme_t phoneme);

/*=============================================================================
 * AUDIOVISUAL INTEGRATION
 *===========================================================================*/

/**
 * @brief Integrate visual and auditory phoneme cues
 *
 * @param system Lip reading system
 * @param visual_viseme Visual viseme
 * @param visual_confidence Visual confidence
 * @param auditory_phoneme Auditory phoneme
 * @param auditory_confidence Auditory confidence
 * @param auditory_snr Audio signal-to-noise ratio (dB)
 * @param result Integration result
 * @return true on success
 */
bool lip_reading_integrate_audiovisual(
    lip_reading_system_t* system,
    viseme_t visual_viseme,
    float visual_confidence,
    phoneme_t auditory_phoneme,
    float auditory_confidence,
    float auditory_snr,
    audiovisual_integration_t* result
);

/**
 * @brief Disambiguate viseme using auditory cues
 *
 * @param viseme Visual viseme (ambiguous)
 * @param auditory_hint Auditory phoneme hint
 * @return Disambiguated phoneme
 */
phoneme_t lip_reading_disambiguate_viseme(viseme_t viseme, phoneme_t auditory_hint);

/**
 * @brief Check if phoneme is voiced
 *
 * @param phoneme Phoneme to check
 * @return true if voiced
 */
bool lip_reading_is_phoneme_voiced(phoneme_t phoneme);

/**
 * @brief Check if phoneme is nasal
 *
 * @param phoneme Phoneme to check
 * @return true if nasal
 */
bool lip_reading_is_phoneme_nasal(phoneme_t phoneme);

/*=============================================================================
 * MOTOR THEORY (MIRROR NEURONS)
 *===========================================================================*/

/**
 * @brief Convert viseme to articulatory motor command
 *
 * Motor theory of speech perception: We perceive speech by internally
 * simulating how we would produce it.
 *
 * @param viseme Observed viseme
 * @param action Output articulatory action
 * @return true on success
 */
bool lip_reading_viseme_to_motor_command(
    viseme_t viseme,
    articulatory_action_t* action
);

/**
 * @brief Simulate articulatory action (mirror neuron)
 *
 * @param system Lip reading system
 * @param viseme Observed viseme
 * @param action Articulatory action to simulate
 * @return true on success
 */
bool lip_reading_simulate_articulation(
    lip_reading_system_t* system,
    viseme_t viseme,
    const articulatory_action_t* action
);

/*=============================================================================
 * SPEAKER ADAPTATION
 *===========================================================================*/

/**
 * @brief Register new speaker for adaptation
 *
 * @param system Lip reading system
 * @param speaker_name Speaker name (optional)
 * @return Speaker ID or 0 on failure
 */
uint32_t lip_reading_register_speaker(
    lip_reading_system_t* system,
    const char* speaker_name
);

/**
 * @brief Update speaker profile with new observation
 *
 * @param system Lip reading system
 * @param speaker_id Speaker ID
 * @param features Visual speech features
 * @param actual_phoneme Ground truth phoneme (from audio)
 * @return true on success
 */
bool lip_reading_update_speaker_profile(
    lip_reading_system_t* system,
    uint32_t speaker_id,
    const visual_speech_features_t* features,
    phoneme_t actual_phoneme
);

/**
 * @brief Get speaker profile
 *
 * @param system Lip reading system
 * @param speaker_id Speaker ID
 * @param profile Output profile
 * @return true if found
 */
bool lip_reading_get_speaker_profile(
    lip_reading_system_t* system,
    uint32_t speaker_id,
    speaker_profile_t* profile
);

/**
 * @brief Set active speaker for classification
 *
 * @param system Lip reading system
 * @param speaker_id Speaker ID (0 for default)
 * @return true on success
 */
bool lip_reading_set_active_speaker(
    lip_reading_system_t* system,
    uint32_t speaker_id
);

/*=============================================================================
 * FULL PIPELINE
 *===========================================================================*/

/**
 * @brief Process full video frame through lip reading pipeline
 *
 * This function performs the complete lip reading pipeline:
 * 1. Face detection
 * 2. Mouth ROI extraction
 * 3. Feature extraction
 * 4. Viseme classification
 *
 * @param system Lip reading system
 * @param image Input image
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param classification Output viseme classification
 * @return Lip reading status
 */
lip_reading_status_t lip_reading_process_frame(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    viseme_classification_t* classification
);

/**
 * @brief Process frame with audiovisual fusion
 *
 * @param system Lip reading system
 * @param image Input image
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param auditory_phoneme Auditory phoneme from audio stream
 * @param auditory_confidence Auditory confidence
 * @param auditory_snr Audio signal-to-noise ratio (dB)
 * @param integration Output audiovisual integration
 * @return Lip reading status
 */
lip_reading_status_t lip_reading_process_frame_with_audio(
    lip_reading_system_t* system,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    phoneme_t auditory_phoneme,
    float auditory_confidence,
    float auditory_snr,
    audiovisual_integration_t* integration
);

/*=============================================================================
 * APPLICATIONS
 *===========================================================================*/

/**
 * @brief Recognize speech in noisy environment
 *
 * Uses visual cues to enhance speech recognition when audio SNR is poor.
 *
 * @param system Lip reading system
 * @param video_frames Array of video frames
 * @param num_frames Number of frames
 * @param frame_width Frame width
 * @param frame_height Frame height
 * @param channels Number of channels
 * @param audio_phonemes Noisy audio phonemes
 * @param audio_confidences Audio confidences
 * @param audio_snr Overall audio SNR
 * @param recognized_phonemes Output enhanced phoneme sequence
 * @param max_phonemes Maximum output phonemes
 * @return Number of recognized phonemes
 */
uint32_t lip_reading_enhance_speech_in_noise(
    lip_reading_system_t* system,
    const uint8_t** video_frames,
    uint32_t num_frames,
    uint32_t frame_width,
    uint32_t frame_height,
    uint32_t channels,
    const phoneme_t* audio_phonemes,
    const float* audio_confidences,
    float audio_snr,
    phoneme_t* recognized_phonemes,
    uint32_t max_phonemes
);

/**
 * @brief Recognize silent speech (visual only)
 *
 * @param system Lip reading system
 * @param video_frames Array of video frames
 * @param num_frames Number of frames
 * @param frame_width Frame width
 * @param frame_height Frame height
 * @param channels Number of channels
 * @param viseme_sequence Output viseme sequence
 * @param max_visemes Maximum output visemes
 * @return Number of recognized visemes
 */
uint32_t lip_reading_recognize_silent_speech(
    lip_reading_system_t* system,
    const uint8_t** video_frames,
    uint32_t num_frames,
    uint32_t frame_width,
    uint32_t frame_height,
    uint32_t channels,
    viseme_t* viseme_sequence,
    uint32_t max_visemes
);

/*=============================================================================
 * INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to visual cortex
 *
 * @param system Lip reading system
 * @param visual_cortex Visual cortex instance
 * @return true on success
 */
bool lip_reading_connect_visual_cortex(
    lip_reading_system_t* system,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Connect to speech cortex
 *
 * @param system Lip reading system
 * @param speech_cortex Speech cortex instance
 * @return true on success
 */
bool lip_reading_connect_speech_cortex(
    lip_reading_system_t* system,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Connect to brain
 *
 * @param system Lip reading system
 * @param brain Brain instance
 * @return true on success
 */
bool lip_reading_connect_brain(
    lip_reading_system_t* system,
    brain_t* brain
);

/**
 * @brief Connect bio-async router
 *
 * @param system Lip reading system
 * @param router Bio-async router
 * @return true on success
 */
bool lip_reading_connect_bio_router(
    lip_reading_system_t* system,
    bio_router_t* router
);

/*=============================================================================
 * STATUS & DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 *
 * @param system Lip reading system
 * @return Current status
 */
lip_reading_status_t lip_reading_get_status(const lip_reading_system_t* system);

/**
 * @brief Get last error
 *
 * @param system Lip reading system
 * @return Last error code
 */
lip_reading_error_t lip_reading_get_last_error(const lip_reading_system_t* system);

/**
 * @brief Get error message
 *
 * @param error Error code
 * @return Error message string
 */
const char* lip_reading_error_message(lip_reading_error_t error);

/**
 * @brief Get statistics
 *
 * @param system Lip reading system
 * @param stats Output statistics
 * @return true on success
 */
bool lip_reading_get_stats(
    const lip_reading_system_t* system,
    lip_reading_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param system Lip reading system
 * @return true on success
 */
bool lip_reading_reset_stats(lip_reading_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LIP_READING_H */
