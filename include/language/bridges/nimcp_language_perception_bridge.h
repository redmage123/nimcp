//=============================================================================
// nimcp_language_perception_bridge.h - Language-Perception Bridge Integration
//=============================================================================
/**
 * @file nimcp_language_perception_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Perception Layer
 *
 * WHAT: Bridge connecting language processing with perceptual input systems
 * WHY:  Enable speech/audio/visual perception to feed language comprehension
 * HOW:  Speech cortex → phonemes → Wernicke; Visual cortex → text → reading
 *
 * BIOLOGICAL BASIS:
 * - Speech Cortex (STG): Phoneme extraction from acoustic signal
 * - Audio Cortex (A1): Speech detection, attention, quality assessment
 * - Visual Cortex (V4/VWFA): Text/grapheme recognition (reading)
 * - Cross-modal binding: Audiovisual integration (McGurk effect)
 * - Attentional feedback: Language → Perception attention modulation
 *
 * KEY CONNECTIONS:
 * - Speech Cortex: Phoneme stream, prosody, formants, word boundaries
 * - Audio Cortex: Speech detection, noise level, audio quality
 * - Visual Cortex: OCR output, text features, lip-reading
 * - Omni Sensory Bridge: Cross-modal coherence, multimodal binding
 *
 * DATA FLOW:
 * - Perception → Language: Phonemes, text tokens, multimodal features
 * - Language → Perception: Attention weights, prediction feedback
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_PERCEPTION_BRIDGE_H
#define NIMCP_LANGUAGE_PERCEPTION_BRIDGE_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_perception_bridge language_perception_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct speech_cortex speech_cortex_t;
typedef struct audio_cortex audio_cortex_t;
typedef struct visual_cortex visual_cortex_t;
typedef struct omni_sensory_bridge omni_sensory_bridge_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_PERCEPTION_MODULE_NAME      "language_perception_bridge"
#define LANGUAGE_PERCEPTION_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_PERCEPTION_DEFAULT_UPDATE_INTERVAL_MS    10
#define LANGUAGE_PERCEPTION_DEFAULT_PHONEME_BUFFER_SIZE   256
#define LANGUAGE_PERCEPTION_DEFAULT_CONFIDENCE_THRESHOLD  0.5f

/** McGurk effect configuration */
#define LANGUAGE_PERCEPTION_MCGURK_DEFAULT_WEIGHT         0.3f
#define LANGUAGE_PERCEPTION_MCGURK_AV_COHERENCE_THRESHOLD 0.6f

/** Attention feedback */
#define LANGUAGE_PERCEPTION_ATTENTION_BOOST_DEFAULT       0.2f
#define LANGUAGE_PERCEPTION_ATTENTION_DECAY_DEFAULT       0.95f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Perception input source types
 */
typedef enum {
    PERCEPTION_SOURCE_SPEECH = 0,     /**< Speech cortex (phonemes) */
    PERCEPTION_SOURCE_AUDIO,          /**< Audio cortex (features) */
    PERCEPTION_SOURCE_VISUAL,         /**< Visual cortex (text/lips) */
    PERCEPTION_SOURCE_MULTIMODAL,     /**< Fused multimodal */
    PERCEPTION_SOURCE_COUNT
} language_perception_source_t;

/**
 * @brief Speech detection state
 */
typedef enum {
    SPEECH_DETECTION_NONE = 0,        /**< No speech detected */
    SPEECH_DETECTION_ONSET,           /**< Speech onset detected */
    SPEECH_DETECTION_ACTIVE,          /**< Active speech */
    SPEECH_DETECTION_OFFSET,          /**< Speech ending */
    SPEECH_DETECTION_COUNT
} speech_detection_state_t;

/**
 * @brief Audiovisual binding state
 */
typedef enum {
    AV_BINDING_NONE = 0,              /**< No audiovisual content */
    AV_BINDING_AUDIO_ONLY,            /**< Audio without visual */
    AV_BINDING_VISUAL_ONLY,           /**< Visual without audio (reading) */
    AV_BINDING_CONGRUENT,             /**< Matching audiovisual */
    AV_BINDING_INCONGRUENT,           /**< Mismatched (McGurk) */
    AV_BINDING_COUNT
} av_binding_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Speech cortex connection data
 */
typedef struct {
    /* Phoneme stream */
    language_phoneme_t* phoneme_buffer;   /**< Incoming phoneme buffer */
    uint32_t phoneme_buffer_size;         /**< Buffer capacity */
    uint32_t phoneme_write_idx;           /**< Current write position */
    uint32_t phoneme_read_idx;            /**< Current read position */
    uint32_t phonemes_available;          /**< Pending phonemes */

    /* Prosody */
    language_prosody_t current_prosody;   /**< Current prosodic contour */
    bool prosody_valid;                   /**< Prosody data valid */

    /* Detection state */
    speech_detection_state_t detection;   /**< Current speech state */
    float speech_probability;             /**< Speech presence probability */
    uint64_t speech_onset_time_ms;        /**< When speech started */

    /* Quality metrics */
    float signal_quality;                 /**< Signal quality [0-1] */
    float noise_level;                    /**< Background noise [0-1] */
} language_speech_cortex_data_t;

/**
 * @brief Audio cortex connection data
 */
typedef struct {
    /* Audio features */
    float* spectral_features;             /**< Spectral representation */
    uint32_t spectral_dim;                /**< Spectral dimension */
    float audio_energy;                   /**< Current audio energy */
    float voice_activity;                 /**< VAD score [0-1] */

    /* Attention */
    float attention_level;                /**< Current attention to audio */
    bool is_speech_focused;               /**< Attending to speech vs. other */
} language_audio_cortex_data_t;

/**
 * @brief Visual cortex connection data (reading/lip-reading)
 */
typedef struct {
    /* Text recognition (reading) */
    char* recognized_text;                /**< OCR text buffer */
    uint32_t text_buffer_size;            /**< Buffer size */
    uint32_t text_length;                 /**< Current text length */
    float text_confidence;                /**< OCR confidence */

    /* Lip reading */
    float* viseme_features;               /**< Visual phoneme features */
    uint32_t viseme_dim;                  /**< Viseme feature dimension */
    bool lip_reading_active;              /**< Lip reading enabled */
    float lip_reading_confidence;         /**< Lip reading confidence */

    /* Fixation */
    float gaze_x;                         /**< Current gaze X */
    float gaze_y;                         /**< Current gaze Y */
    bool fixating_on_text;                /**< Gaze on text region */
} language_visual_cortex_data_t;

/**
 * @brief Multimodal integration data
 */
typedef struct {
    /* Binding state */
    av_binding_state_t binding_state;     /**< Current binding */
    float av_coherence;                   /**< Audiovisual coherence [0-1] */

    /* McGurk effect */
    bool mcgurk_active;                   /**< McGurk condition detected */
    float mcgurk_visual_weight;           /**< Visual influence weight */
    uint32_t mcgurk_perceived_phoneme;    /**< Fused perception */

    /* Cross-modal features */
    float* fused_features;                /**< Fused representation */
    uint32_t fused_dim;                   /**< Fused dimension */
} language_multimodal_data_t;

/**
 * @brief Attention feedback to perception
 */
typedef struct {
    /* Feature attention */
    float* feature_weights;               /**< Per-feature attention weights */
    uint32_t num_features;                /**< Number of features */

    /* Spatial attention */
    float attention_focus_x;              /**< Attention focus X */
    float attention_focus_y;              /**< Attention focus Y */
    float attention_spread;               /**< Attention spread */

    /* Phoneme prediction */
    uint32_t* predicted_phonemes;         /**< Predicted next phonemes */
    float* prediction_confidence;         /**< Prediction confidences */
    uint32_t num_predictions;             /**< Number of predictions */
} language_perception_attention_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Input counts */
    uint64_t phonemes_received;           /**< Total phonemes received */
    uint64_t words_detected;              /**< Total words detected */
    uint64_t text_segments_received;      /**< Text segments received */

    /* Quality metrics */
    float avg_phoneme_confidence;         /**< Average phoneme confidence */
    float avg_speech_quality;             /**< Average speech quality */
    float avg_av_coherence;               /**< Average AV coherence */

    /* McGurk statistics */
    uint64_t mcgurk_events;               /**< McGurk detections */
    float mcgurk_integration_rate;        /**< Rate of AV integration */

    /* Performance */
    float avg_processing_time_ms;         /**< Average processing time */
    uint64_t last_update_time_ms;         /**< Last update timestamp */
} language_perception_stats_t;

//=============================================================================
// Bridge Configuration (Extended from language_perception_config_t)
//=============================================================================

/**
 * @brief Language-perception bridge state
 */
struct language_perception_bridge {
    /* Configuration */
    language_perception_config_t config;  /**< Bridge configuration */
    bool initialized;                     /**< Initialization state */
    bool active;                          /**< Active processing */

    /* Connected components */
    language_orchestrator_t* orchestrator;/**< Parent orchestrator */
    speech_cortex_t* speech_cortex;       /**< Speech cortex connection */
    audio_cortex_t* audio_cortex;         /**< Audio cortex connection */
    visual_cortex_t* visual_cortex;       /**< Visual cortex connection */
    omni_sensory_bridge_t* omni_sensory;  /**< Omni sensory bridge */

    /* Connection data */
    language_speech_cortex_data_t speech_data;  /**< Speech cortex data */
    language_audio_cortex_data_t audio_data;    /**< Audio cortex data */
    language_visual_cortex_data_t visual_data;  /**< Visual cortex data */
    language_multimodal_data_t multimodal_data; /**< Multimodal data */

    /* Attention feedback */
    language_perception_attention_t attention;  /**< Attention to perception */

    /* Statistics */
    language_perception_stats_t stats;    /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;             /**< Bio-async router */
    bool bio_async_registered;            /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-perception bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_perception_bridge_t* language_perception_bridge_create(
    const language_perception_config_t* config
);

/**
 * @brief Destroy language-perception bridge
 *
 * @param bridge Bridge instance
 */
void language_perception_bridge_destroy(language_perception_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_init(language_perception_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_start(language_perception_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_stop(language_perception_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to language orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Language orchestrator
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_connect_orchestrator(
    language_perception_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to speech cortex
 *
 * @param bridge Bridge instance
 * @param speech_cortex Speech cortex instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_connect_speech_cortex(
    language_perception_bridge_t* bridge,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Connect to audio cortex
 *
 * @param bridge Bridge instance
 * @param audio_cortex Audio cortex instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_connect_audio_cortex(
    language_perception_bridge_t* bridge,
    audio_cortex_t* audio_cortex
);

/**
 * @brief Connect to visual cortex
 *
 * @param bridge Bridge instance
 * @param visual_cortex Visual cortex instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_connect_visual_cortex(
    language_perception_bridge_t* bridge,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Connect to omni sensory bridge
 *
 * @param bridge Bridge instance
 * @param omni_sensory Omni sensory bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_connect_omni_sensory(
    language_perception_bridge_t* bridge,
    omni_sensory_bridge_t* omni_sensory
);

//=============================================================================
// Input Processing API (Perception → Language)
//=============================================================================

/**
 * @brief Receive phonemes from speech cortex
 *
 * @param bridge Bridge instance
 * @param phonemes Phoneme array
 * @param count Number of phonemes
 * @return Number of phonemes accepted, or -1 on error
 */
int language_perception_bridge_receive_phonemes(
    language_perception_bridge_t* bridge,
    const language_phoneme_t* phonemes,
    uint32_t count
);

/**
 * @brief Receive prosody from speech cortex
 *
 * @param bridge Bridge instance
 * @param prosody Prosodic contour
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_receive_prosody(
    language_perception_bridge_t* bridge,
    const language_prosody_t* prosody
);

/**
 * @brief Receive text from visual cortex (reading)
 *
 * @param bridge Bridge instance
 * @param text Text string
 * @param confidence Recognition confidence
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_receive_text(
    language_perception_bridge_t* bridge,
    const char* text,
    float confidence
);

/**
 * @brief Receive lip-reading features
 *
 * @param bridge Bridge instance
 * @param visemes Viseme features
 * @param dim Feature dimension
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_receive_visemes(
    language_perception_bridge_t* bridge,
    const float* visemes,
    uint32_t dim
);

/**
 * @brief Receive speech detection event
 *
 * @param bridge Bridge instance
 * @param state Detection state
 * @param probability Speech probability
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_receive_speech_detection(
    language_perception_bridge_t* bridge,
    speech_detection_state_t state,
    float probability
);

//=============================================================================
// Output Processing API (Language → Perception)
//=============================================================================

/**
 * @brief Send attention weights to perception
 *
 * @param bridge Bridge instance
 * @param weights Feature attention weights
 * @param count Number of weights
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_send_attention(
    language_perception_bridge_t* bridge,
    const float* weights,
    uint32_t count
);

/**
 * @brief Send phoneme predictions for top-down guidance
 *
 * @param bridge Bridge instance
 * @param predicted_phonemes Array of predicted phoneme IDs
 * @param confidences Prediction confidences
 * @param count Number of predictions
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_send_predictions(
    language_perception_bridge_t* bridge,
    const uint32_t* predicted_phonemes,
    const float* confidences,
    uint32_t count
);

/**
 * @brief Send spatial attention focus
 *
 * @param bridge Bridge instance
 * @param x Focus X coordinate
 * @param y Focus Y coordinate
 * @param spread Attention spread
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_send_spatial_attention(
    language_perception_bridge_t* bridge,
    float x,
    float y,
    float spread
);

//=============================================================================
// Multimodal Integration API
//=============================================================================

/**
 * @brief Process audiovisual binding
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_process_av_binding(
    language_perception_bridge_t* bridge
);

/**
 * @brief Get current AV binding state
 *
 * @param bridge Bridge instance
 * @return Current binding state
 */
av_binding_state_t language_perception_bridge_get_av_state(
    const language_perception_bridge_t* bridge
);

/**
 * @brief Check for McGurk effect condition
 *
 * @param bridge Bridge instance
 * @return true if McGurk integration active
 */
bool language_perception_bridge_is_mcgurk_active(
    const language_perception_bridge_t* bridge
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update bridge (call each frame/cycle)
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_update(
    language_perception_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get pending phonemes for language processing
 *
 * @param bridge Bridge instance
 * @param phonemes Output phoneme array
 * @param max_count Maximum phonemes to retrieve
 * @return Number of phonemes retrieved, or -1 on error
 */
int language_perception_bridge_get_phonemes(
    language_perception_bridge_t* bridge,
    language_phoneme_t* phonemes,
    uint32_t max_count
);

/**
 * @brief Get current speech detection state
 *
 * @param bridge Bridge instance
 * @return Speech detection state
 */
speech_detection_state_t language_perception_bridge_get_speech_state(
    const language_perception_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_get_stats(
    const language_perception_bridge_t* bridge,
    language_perception_stats_t* stats
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_bio_async_register(
    language_perception_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_perception_bridge_bio_async_unregister(
    language_perception_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_PERCEPTION_BRIDGE_H */
