/**
 * @file nimcp_emotion_recognition.h
 * @brief Multimodal emotion recognition for safe human-AI interaction
 *
 * WHAT: Real-time emotion recognition from facial, vocal, text, and physiological signals
 * WHY:  Enable empathetic, safe responses to user emotions (especially negative emotions)
 * HOW:  Multimodal fusion with attention-weighted integration
 *
 * PURPOSE: General-purpose emotional intelligence for ANY user interaction
 *          NOT education-specific - handles agitated/distressed users in all contexts
 *
 * CORE PRINCIPLE:
 * - Detect user emotions accurately and quickly (<50ms)
 * - Enable empathetic, non-reactive responses
 * - NEVER escalate negative emotions
 * - Support crisis detection (distress, rage, panic)
 *
 * INTEGRATION:
 * - Uses: multimodal_integration_t, empathy_network_t, theory_of_mind_t
 * - Feeds: Empathetic response generator, ethics validation
 * - Extends: Existing emotional_tag_t system with richer recognition
 *
 * COMPLEXITY: O(N) per modality, O(M) fusion where M = active modalities
 * THREAD_SAFETY: Thread-safe read/write operations with proper locking
 *
 * @author NIMCP Team
 * @version 1.0
 * @date 2025-11-11
 */

#ifndef NIMCP_EMOTION_RECOGNITION_H
#define NIMCP_EMOTION_RECOGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_system.h"  // For emotional_tag_t

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// EMOTION CATEGORIES (19 Emotions)
// ============================================================================

/**
 * WHAT: Emotion categories recognized by system
 * WHY:  Cover basic emotions + extended emotions for user interaction
 * HOW:  Hierarchical categorization (basic + extended + dimensional)
 *
 * CATEGORIES:
 * - Basic (Ekman): 6 universal emotions
 * - Extended: 12 additional emotions for user interactions
 * - Dimensional: Valence, arousal, intensity
 *
 * NOTE: This comprehensive definition takes precedence over the simpler
 * Russell-based categories in nimcp_emotional_tagging.h
 */
#ifndef NIMCP_EMOTION_CATEGORY_T_DEFINED
#define NIMCP_EMOTION_CATEGORY_T_DEFINED
typedef enum {
    // === Basic Emotions (Ekman) ===
    EMOTION_HAPPINESS = 0,     /**< Joy, contentment, satisfaction */
    EMOTION_SADNESS,           /**< Sorrow, grief, disappointment */
    EMOTION_ANGER,             /**< Irritation, frustration, annoyance */
    EMOTION_FEAR,              /**< Anxiety, worry, apprehension */
    EMOTION_DISGUST,           /**< Aversion, revulsion, distaste */
    EMOTION_SURPRISE,          /**< Astonishment, shock, wonder */

    // === Extended Emotions (User Interaction) ===
    EMOTION_INTEREST,          /**< Curiosity, engagement, attention */
    EMOTION_CONFUSION,         /**< Uncertainty, bewilderment, perplexity */
    EMOTION_FRUSTRATION,       /**< Exasperation, irritation, impatience */
    EMOTION_BOREDOM,           /**< Disengagement, apathy, restlessness */
    EMOTION_PRIDE,             /**< Accomplishment, self-satisfaction */
    EMOTION_SHAME,             /**< Embarrassment, humiliation, guilt */
    EMOTION_RAGE,              /**< Intense anger, fury, wrath */
    EMOTION_HATE,              /**< Intense dislike, hostility, animosity */
    EMOTION_DESPAIR,           /**< Hopelessness, helplessness, anguish */
    EMOTION_PANIC,             /**< Intense fear, terror, alarm */
    EMOTION_CALM,              /**< Tranquility, peace, relaxation */
    EMOTION_CONTEMPT,          /**< Scorn, disdain, derision */
    EMOTION_NEUTRAL,           /**< No strong emotion, baseline state */

    EMOTION_COUNT,             /**< Total number of emotion categories */
    EMOTION_UNKNOWN = 255      /**< Could not determine emotion */
} emotion_category_t;
#endif /* NIMCP_EMOTION_CATEGORY_T_DEFINED */

/**
 * WHAT: Emotion intensity levels (discrete labels for logging/display)
 * WHY:  Backward-compatible labeling; dispatch uses continuous intensity_value
 * HOW:  5-level scale derived from continuous float [0.0-1.0]
 */
#ifndef NIMCP_EMOTION_INTENSITY_T_DEFINED
#define NIMCP_EMOTION_INTENSITY_T_DEFINED
typedef enum {
    EMOTION_INTENSITY_NONE = 0,      /**< No emotion detected (< 0.05) */
    EMOTION_INTENSITY_LOW,           /**< Mild emotion [0.05-0.3) */
    EMOTION_INTENSITY_MEDIUM,        /**< Moderate emotion [0.3-0.6) */
    EMOTION_INTENSITY_HIGH,          /**< Strong emotion [0.6-0.8) */
    EMOTION_INTENSITY_EXTREME        /**< Extreme emotion [0.8-1.0] */
} emotion_intensity_t;
#endif /* NIMCP_EMOTION_INTENSITY_T_DEFINED */

// ============================================================================
// CONTINUOUS INTENSITY API
// ============================================================================

#ifndef NIMCP_EMOTION_INTENSITY_EFFECTS_T_DEFINED
#define NIMCP_EMOTION_INTENSITY_EFFECTS_T_DEFINED
typedef struct {
    float response_urgency;
    float empathy_weight;
    float intervention_probability;
    float de_escalation_strength;
    float grounding_need;
    emotion_intensity_t label;
} emotion_intensity_effects_t;
#endif

static inline emotion_intensity_t emotion_intensity_from_float(float intensity) {
    if (intensity < 0.05f) return EMOTION_INTENSITY_NONE;
    if (intensity < 0.30f) return EMOTION_INTENSITY_LOW;
    if (intensity < 0.60f) return EMOTION_INTENSITY_MEDIUM;
    if (intensity < 0.80f) return EMOTION_INTENSITY_HIGH;
    return EMOTION_INTENSITY_EXTREME;
}

static inline float emotion_intensity_to_float(emotion_intensity_t label) {
    switch (label) {
        case EMOTION_INTENSITY_NONE:    return 0.0f;
        case EMOTION_INTENSITY_LOW:     return 0.175f;
        case EMOTION_INTENSITY_MEDIUM:  return 0.45f;
        case EMOTION_INTENSITY_HIGH:    return 0.70f;
        case EMOTION_INTENSITY_EXTREME: return 0.90f;
        default:                        return 0.0f;
    }
}

int emotion_compute_intensity_effects(float intensity, emotion_intensity_effects_t *out);

/**
 * WHAT: Valence classification (positive/negative/neutral)
 * WHY:  Quick classification for response selection
 * HOW:  Threshold-based on valence score
 */
typedef enum {
    EMOTION_VALENCE_NEGATIVE = -1,   /**< Negative emotions (distress) */
    EMOTION_VALENCE_NEUTRAL = 0,     /**< Neutral emotions */
    EMOTION_VALENCE_POSITIVE = 1     /**< Positive emotions (wellbeing) */
} emotion_valence_class_t;

// ============================================================================
// MODALITY-SPECIFIC RESULTS
// ============================================================================

/**
 * WHAT: Facial emotion recognition result
 * WHY:  Facial expressions are primary emotion indicator
 * HOW:  Action Unit detection + expression classification
 */
typedef struct {
    emotion_category_t category;     /**< Detected emotion */
    float confidence;                /**< Confidence [0.0-1.0] */
    float action_units[17];          /**< Facial Action Coding System units */
    uint64_t timestamp_ms;           /**< When detected */
    bool valid;                      /**< Whether detection succeeded */
} facial_emotion_result_t;

/**
 * WHAT: Vocal emotion recognition result
 * WHY:  Voice prosody reveals emotional state
 * HOW:  Pitch, intensity, voice quality analysis
 */
typedef struct {
    emotion_category_t category;     /**< Detected emotion */
    float confidence;                /**< Confidence [0.0-1.0] */
    float pitch_mean;                /**< Average pitch (Hz) */
    float pitch_variance;            /**< Pitch variability */
    float intensity_mean;            /**< Average intensity (dB) */
    float voice_quality;             /**< Jitter, shimmer, HNR */
    uint64_t timestamp_ms;           /**< When detected */
    bool valid;                      /**< Whether detection succeeded */
} vocal_emotion_result_t;

/**
 * WHAT: Text emotion recognition result
 * WHY:  Text sentiment reveals emotional content
 * HOW:  NLP-based emotion detection from language
 */
typedef struct {
    emotion_category_t category;     /**< Detected emotion */
    float confidence;                /**< Confidence [0.0-1.0] */
    float valence;                   /**< Sentiment valence [-1.0, +1.0] */
    float arousal;                   /**< Arousal level [0.0-1.0] */
    const char **emotion_keywords;   /**< Detected emotion words */
    uint32_t num_keywords;           /**< Number of keywords */
    uint64_t timestamp_ms;           /**< When detected */
    bool valid;                      /**< Whether detection succeeded */
} text_emotion_result_t;

/**
 * WHAT: Physiological emotion indicators
 * WHY:  Bodily signals reveal arousal and stress
 * HOW:  Heart rate, skin conductance, respiration
 */
typedef struct {
    float heart_rate;                /**< BPM (40-200) */
    float heart_rate_variability;    /**< HRV (ms) */
    float skin_conductance;          /**< GSR (microsiemens) */
    float respiration_rate;          /**< Breaths per minute */
    float arousal_estimate;          /**< Estimated arousal [0.0-1.0] */
    uint64_t timestamp_ms;           /**< When measured */
    bool valid;                      /**< Whether data available */
} physiological_emotion_result_t;

// ============================================================================
// MULTIMODAL FUSION RESULT
// ============================================================================

/**
 * WHAT: Integrated emotion recognition result from all modalities
 * WHY:  Multimodal fusion is more accurate than single modality
 * HOW:  Attention-weighted integration with confidence estimation
 *
 * FUSION STRATEGY:
 * - Early fusion: Combine features before classification
 * - Late fusion: Combine decisions with attention weights
 * - Missing modality handling: Graceful degradation
 */
typedef struct {
    // === Primary Emotion ===
    emotion_category_t category;     /**< Final emotion decision */
    float confidence;                /**< Overall confidence [0.0-1.0] */
    emotion_intensity_t intensity;   /**< Discrete label (for logging/display) */
    float intensity_value;           /**< Continuous intensity [0.0-1.0] (for dispatch) */

    // === Dimensional Representation ===
    float valence;                   /**< Pleasure/displeasure [-1.0, +1.0] */
    float arousal;                   /**< Activation [0.0-1.0] */
    float dominance;                 /**< Control/power [-1.0, +1.0] */

    // === Per-Modality Results ===
    facial_emotion_result_t facial;
    vocal_emotion_result_t vocal;
    text_emotion_result_t text;
    physiological_emotion_result_t physiological;

    // === Fusion Metadata ===
    float modality_weights[4];       /**< Attention weights per modality */
    uint32_t active_modalities;      /**< Bitmask of available modalities */
    uint64_t timestamp_ms;           /**< When computed */

    // === Safety Flags ===
    bool requires_intervention;      /**< Crisis/distress detected */
    float distress_level;            /**< Overall distress [0.0-1.0] */
    bool is_negative_emotion;        /**< Requires empathetic response */
} emotion_recognition_result_t;

// ============================================================================
// EMOTION TRANSITION TRACKING
// ============================================================================

/**
 * WHAT: Emotion transition between states
 * WHY:  Rapid transitions indicate distress or crisis
 * HOW:  Track emotion history, detect sudden changes
 */
typedef struct {
    emotion_category_t from_emotion; /**< Previous emotion */
    emotion_category_t to_emotion;   /**< Current emotion */
    float transition_speed;          /**< How fast transition occurred */
    uint64_t timestamp_ms;           /**< When transition detected */
    bool is_distress_transition;     /**< Calm→Panic, Happy→Rage, etc */
} emotion_transition_t;

/**
 * WHAT: Emotion history tracker
 * WHY:  Detect patterns, transitions, emotional trajectories
 * HOW:  Circular buffer of recent emotions
 */
typedef struct emotion_history emotion_history_t;

// ============================================================================
// EMOTION RECOGNITION SYSTEM
// ============================================================================

/**
 * WHAT: Multimodal emotion recognition system
 * WHY:  Detect user emotions from multiple signals
 * HOW:  Parallel processing + attention-weighted fusion
 */
typedef struct emotion_recognition_system emotion_recognition_system_t;

/**
 * WHAT: Configuration for emotion recognition
 * WHY:  Allow tuning sensitivity, modalities, performance
 * HOW:  Feature flags and thresholds
 */
typedef struct {
    // === Enabled Modalities ===
    bool enable_facial;              /**< Process facial expressions */
    bool enable_vocal;               /**< Process voice prosody */
    bool enable_text;                /**< Process text sentiment */
    bool enable_physiological;       /**< Process biosignals */

    // === Detection Thresholds ===
    float confidence_threshold;      /**< Min confidence to accept [0.5-0.9] */
    float distress_threshold;        /**< Distress level for intervention [0.7] */
    float transition_speed_alert;    /**< Speed threshold for rapid transitions */

    // === Performance ===
    uint32_t history_size;           /**< Emotion history buffer size [10-50] */
    uint32_t fusion_window_ms;       /**< Time window for fusion [500-2000ms] */
    bool enable_transition_tracking; /**< Track emotion changes */

    // === Safety ===
    bool enable_crisis_detection;    /**< Detect crisis indicators */
    bool enable_intervention_flags;  /**< Flag situations needing help */
} emotion_recognition_config_t;

// ============================================================================
// API FUNCTIONS
// ============================================================================

/**
 * WHAT: Create emotion recognition system
 * WHY:  Initialize multimodal emotion recognition
 * HOW:  Allocate structures, load models, configure fusion
 *
 * @param config Configuration parameters
 * @return Emotion recognition system, or NULL if allocation fails
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe creation
 */
emotion_recognition_system_t* emotion_recognition_create(
    const emotion_recognition_config_t *config
);

/**
 * WHAT: Destroy emotion recognition system
 * WHY:  Free resources, cleanup models
 * HOW:  Deallocate all structures
 *
 * @param system Emotion recognition system to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Not thread-safe, ensure exclusive access
 */
void emotion_recognition_destroy(emotion_recognition_system_t *system);

/**
 * WHAT: Recognize emotion from multimodal input
 * WHY:  Main API for emotion detection
 * HOW:  Process each modality, fuse results with attention weighting
 *
 * @param system Emotion recognition system
 * @param facial_frame Facial image (RGB, width x height x 3), can be NULL
 * @param facial_width Width of facial image
 * @param facial_height Height of facial image
 * @param audio_signal Audio samples (mono, 16kHz), can be NULL
 * @param audio_samples Number of audio samples
 * @param text_input Text string (UTF-8), can be NULL
 * @param physiological Physiological data, can be NULL
 * @param result Output: Emotion recognition result
 *
 * @return true if emotion detected, false otherwise
 *
 * COMPLEXITY: O(N) where N = input size per modality
 * THREAD_SAFETY: Thread-safe if different result buffers used
 *
 * USAGE:
 * ```c
 * emotion_recognition_result_t result;
 * bool detected = emotion_recognition_recognize(
 *     system,
 *     video_frame, width, height,
 *     audio, num_samples,
 *     "I'm so frustrated with this!",
 *     NULL,  // No physiological data
 *     &result
 * );
 * if (detected && result.is_negative_emotion) {
 *     // Generate empathetic response
 * }
 * ```
 */
bool emotion_recognition_recognize(
    emotion_recognition_system_t *system,
    const uint8_t *facial_frame,
    uint32_t facial_width,
    uint32_t facial_height,
    const float *audio_signal,
    uint32_t audio_samples,
    const char *text_input,
    const physiological_emotion_result_t *physiological,
    emotion_recognition_result_t *result
);

/**
 * WHAT: Convert emotion to existing emotional_tag_t format
 * WHY:  Integration with existing emotional tagging system
 * HOW:  Map category to valence/arousal coordinates
 *
 * @param emotion Emotion recognition result
 * @return Emotional tag (valence/arousal format)
 *
 * COMPLEXITY: O(1)
 *
 * MAPPING:
 * - Happiness: valence=+0.8, arousal=+0.6
 * - Sadness: valence=-0.6, arousal=-0.4
 * - Anger: valence=-0.7, arousal=+0.8
 * - Fear: valence=-0.8, arousal=+0.9
 * - Rage: valence=-0.95, arousal=+0.95 (extreme)
 */
emotional_tag_t emotion_to_emotional_tag(
    const emotion_recognition_result_t *emotion
);

/**
 * WHAT: Get emotion category name as string
 * WHY:  Debugging, logging, user display
 * HOW:  Lookup table
 *
 * @param category Emotion category
 * @return String name (e.g., "ANGER", "HAPPINESS")
 *
 * COMPLEXITY: O(1)
 */
const char* emotion_category_to_string(emotion_category_t category);

/**
 * WHAT: Detect if emotion requires intervention
 * WHY:  Crisis detection, safety
 * HOW:  Check intensity, category, distress level
 *
 * @param emotion Emotion recognition result
 * @return true if intervention needed (crisis, extreme distress)
 *
 * CRITERIA:
 * - Extreme intensity (>0.8) + negative valence
 * - Crisis emotions: RAGE, PANIC, DESPAIR at high intensity
 * - Rapid distress transitions (tracked separately)
 *
 * COMPLEXITY: O(1)
 */
bool emotion_requires_intervention(
    const emotion_recognition_result_t *emotion
);

/**
 * WHAT: Add emotion to history tracker
 * WHY:  Enable transition detection and pattern analysis
 * HOW:  Append to circular buffer
 *
 * @param system Emotion recognition system
 * @param emotion Current emotion
 * @param transition Output: Detected transition (if any), can be NULL
 *
 * @return true if significant transition detected
 *
 * COMPLEXITY: O(1)
 */
bool emotion_recognition_add_to_history(
    emotion_recognition_system_t *system,
    const emotion_recognition_result_t *emotion,
    emotion_transition_t *transition
);

/**
 * WHAT: Get default configuration
 * WHY:  Sensible defaults for most use cases
 * HOW:  Preset values optimized for general interaction
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - All modalities enabled
 * - Confidence threshold: 0.6
 * - Distress threshold: 0.7
 * - History size: 20 emotions
 * - Fusion window: 1000ms
 */
emotion_recognition_config_t emotion_recognition_config_default(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EMOTION_RECOGNITION_H
