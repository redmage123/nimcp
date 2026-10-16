/**
 * @file nimcp_audio_logic_bridge.h
 * @brief Bridge between Audio cortex and Logic module
 *
 * WHAT: Connects auditory perception to symbolic reasoning
 * WHY: Enables speech understanding, sound-based inference, auditory grounding
 * HOW: Converts audio events to logical predicates; routes expectations back
 *
 * BIOLOGICAL BASIS:
 * - Superior temporal gyrus processes speech and complex sounds
 * - Wernicke's area links sound patterns to semantic concepts
 * - Prefrontal cortex integrates auditory evidence with reasoning
 * - Language areas provide bidirectional speech-logic coupling
 *
 * INTEGRATION PATHWAYS:
 * - Audio → Logic: Sound events, speech tokens → predicate grounding
 * - Logic → Audio: Attention guidance, expected sound predictions
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_AUDIO_LOGIC_BRIDGE_H
#define NIMCP_AUDIO_LOGIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Signal Types
//=============================================================================

/** Audio-to-logic signal types */
#define AUDIO_LOGIC_SOUND_DETECTED      0x3401  /**< Sound event detected */
#define AUDIO_LOGIC_SPEECH_TOKEN        0x3402  /**< Speech token recognized */
#define AUDIO_LOGIC_WORD_RECOGNIZED     0x3403  /**< Word recognized */
#define AUDIO_LOGIC_PHRASE_COMPLETE     0x3404  /**< Phrase/sentence complete */
#define AUDIO_LOGIC_SPEAKER_IDENTIFIED  0x3405  /**< Speaker identified */
#define AUDIO_LOGIC_MUSIC_PATTERN       0x3406  /**< Musical pattern detected */

/** Logic-to-audio signal types */
#define LOGIC_AUDIO_ATTEND_SOUND        0x3501  /**< Attend to sound type */
#define LOGIC_AUDIO_EXPECT_WORD         0x3502  /**< Expect specific word */
#define LOGIC_AUDIO_VERIFY_SPEAKER      0x3503  /**< Verify speaker identity */
#define LOGIC_AUDIO_FOCUS_FREQUENCY     0x3504  /**< Focus on frequency band */

//=============================================================================
// Sound Categories
//=============================================================================

typedef enum {
    SOUND_CAT_SPEECH = 0,           /**< Human speech */
    SOUND_CAT_MUSIC,                /**< Musical sounds */
    SOUND_CAT_ENVIRONMENTAL,        /**< Environmental/ambient */
    SOUND_CAT_ALERT,                /**< Alert/alarm sounds */
    SOUND_CAT_ANIMAL,               /**< Animal sounds */
    SOUND_CAT_MECHANICAL,           /**< Mechanical/vehicle */
    SOUND_CAT_UNKNOWN,              /**< Unknown/unclassified */
    SOUND_CAT_COUNT
} sound_category_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Audio observation for logical grounding
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    sound_category_t category;      /**< Sound category */
    char concept_name[64];          /**< Concept/word/sound name */
    float confidence;               /**< Recognition confidence [0,1] */
    float salience;                 /**< Audio salience [0,1] */

    /* Spatial audio info */
    float azimuth;                  /**< Sound source azimuth (radians) */
    float elevation;                /**< Sound source elevation (radians) */
    float distance;                 /**< Estimated distance */
    bool spatial_valid;             /**< True if spatial info available */

    /* Speech specifics */
    char speaker_id[32];            /**< Speaker identifier */
    float speech_clarity;           /**< Speech clarity [0,1] */
    bool is_question;               /**< True if question intonation */
    bool is_command;                /**< True if command intonation */

    /* Acoustic features */
    float fundamental_freq;         /**< Fundamental frequency (Hz) */
    float intensity_db;             /**< Sound intensity (dB) */
    float duration_ms;              /**< Sound duration (ms) */

    uint64_t timestamp_us;          /**< Observation timestamp */
} audio_logic_observation_t;

/**
 * @brief Semantic audio predicate
 */
typedef struct {
    char predicate_name[64];        /**< Predicate (said, heard, playing) */
    char subject[64];               /**< Subject (speaker, source) */
    char object[64];                /**< Object (word, sound) */
    float confidence;               /**< Predicate confidence [0,1] */
    uint64_t timestamp_us;          /**< When observed */
} audio_logic_predicate_t;

/**
 * @brief Logic-to-audio attention command
 */
typedef struct {
    uint32_t signal_type;           /**< Command type */
    sound_category_t target_category;/**< Target sound category */
    char expected_content[64];      /**< Expected word/sound */
    char target_speaker[32];        /**< Target speaker (for focus) */
    float priority;                 /**< Attention priority [0,1] */
    float freq_low;                 /**< Low frequency bound (Hz) */
    float freq_high;                /**< High frequency bound (Hz) */
} logic_audio_command_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_speech_grounding;   /**< Ground speech as predicates */
    bool enable_sound_grounding;    /**< Ground non-speech sounds */
    bool enable_speaker_tracking;   /**< Track speaker identities */
    bool enable_top_down_attention; /**< Allow logic to guide attention */
    bool enable_verification;       /**< Allow predicate verification */
    bool enable_spatial_grounding;  /**< Ground spatial sound info */
    float min_confidence_threshold; /**< Minimum confidence for grounding */
    float min_salience_threshold;   /**< Minimum salience for processing */
    float speech_priority_boost;    /**< Priority boost for speech [1-2] */
    uint32_t max_active_speakers;   /**< Maximum concurrent speakers */
} audio_logic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t sounds_grounded;       /**< Sound events converted to predicates */
    uint64_t words_recognized;      /**< Words recognized and grounded */
    uint64_t phrases_completed;     /**< Complete phrases processed */
    uint64_t speakers_tracked;      /**< Unique speakers tracked */
    uint64_t attention_commands;    /**< Top-down attention requests */
    uint64_t verifications_requested;/**< Predicate verifications */
    uint64_t verifications_confirmed;/**< Successful verifications */
    float avg_speech_confidence;    /**< Average speech recognition confidence */
    float avg_sound_confidence;     /**< Average sound detection confidence */
} audio_logic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct audio_logic_bridge audio_logic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with reasonable values
 */
audio_logic_config_t audio_logic_default_config(void);

/**
 * @brief Get sound category name
 * @param category Sound category enum
 * @return String name of category
 */
const char* audio_logic_category_name(sound_category_t category);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create audio-logic bridge
 * @param audio Audio cortex handle
 * @param logic Logic module handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
audio_logic_bridge_t* audio_logic_bridge_create(
    void* audio,
    void* logic,
    const audio_logic_config_t* config
);

/**
 * @brief Destroy audio-logic bridge
 * @param bridge Bridge to destroy
 */
void audio_logic_bridge_destroy(audio_logic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int audio_logic_bridge_reset(audio_logic_bridge_t* bridge);

//=============================================================================
// Audio → Logic API
//=============================================================================

/**
 * @brief Ground audio observation as logical predicate
 * @param bridge Bridge handle
 * @param obs Audio observation
 * @return 0 on success, -1 on error
 *
 * Converts audio event into logical predicate:
 * e.g., heard(bell), said(john, "hello"), playing(music)
 */
int audio_logic_ground_observation(
    audio_logic_bridge_t* bridge,
    const audio_logic_observation_t* obs
);

/**
 * @brief Report speech content as predicate
 * @param bridge Bridge handle
 * @param speaker Speaker identifier (can be NULL)
 * @param words Recognized words/phrase
 * @param confidence Recognition confidence
 * @return 0 on success, -1 on error
 */
int audio_logic_report_speech(
    audio_logic_bridge_t* bridge,
    const char* speaker,
    const char* words,
    float confidence
);

/**
 * @brief Report sound event as predicate
 * @param bridge Bridge handle
 * @param sound_name Sound type/name
 * @param category Sound category
 * @param confidence Detection confidence
 * @return 0 on success, -1 on error
 */
int audio_logic_report_sound(
    audio_logic_bridge_t* bridge,
    const char* sound_name,
    sound_category_t category,
    float confidence
);

/**
 * @brief Process batch of audio observations
 * @param bridge Bridge handle
 * @param observations Array of observations
 * @param count Number of observations
 * @return Number processed, -1 on error
 */
int audio_logic_process_batch(
    audio_logic_bridge_t* bridge,
    const audio_logic_observation_t* observations,
    uint32_t count
);

//=============================================================================
// Logic → Audio API
//=============================================================================

/**
 * @brief Request attention to sound category
 * @param bridge Bridge handle
 * @param category Target sound category
 * @param priority Attention priority [0,1]
 * @return 0 on success, -1 on error
 */
int audio_logic_request_attention(
    audio_logic_bridge_t* bridge,
    sound_category_t category,
    float priority
);

/**
 * @brief Request focus on specific speaker
 * @param bridge Bridge handle
 * @param speaker_id Speaker to focus on
 * @param priority Attention priority [0,1]
 * @return 0 on success, -1 on error
 */
int audio_logic_focus_speaker(
    audio_logic_bridge_t* bridge,
    const char* speaker_id,
    float priority
);

/**
 * @brief Expect specific word/phrase
 * @param bridge Bridge handle
 * @param expected_word Expected word or phrase
 * @return 0 on success, -1 on error
 */
int audio_logic_expect_word(
    audio_logic_bridge_t* bridge,
    const char* expected_word
);

/**
 * @brief Verify audio predicate
 * @param bridge Bridge handle
 * @param predicate Predicate to verify
 * @param verified Output: verification result
 * @param confidence Output: verification confidence
 * @return 0 on success, -1 on error
 */
int audio_logic_verify_predicate(
    audio_logic_bridge_t* bridge,
    const audio_logic_predicate_t* predicate,
    bool* verified,
    float* confidence
);

/**
 * @brief Send top-down command
 * @param bridge Bridge handle
 * @param command Command structure
 * @return 0 on success, -1 on error
 */
int audio_logic_send_command(
    audio_logic_bridge_t* bridge,
    const logic_audio_command_t* command
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if specific speaker is currently active
 * @param bridge Bridge handle
 * @param speaker_id Speaker identifier
 * @param active Output: true if speaker active
 * @return 0 on success, -1 on error
 */
int audio_logic_is_speaker_active(
    const audio_logic_bridge_t* bridge,
    const char* speaker_id,
    bool* active
);

/**
 * @brief Get count of active speakers
 * @param bridge Bridge handle
 * @return Number of active speakers, -1 on error
 */
int audio_logic_get_active_speaker_count(const audio_logic_bridge_t* bridge);

/**
 * @brief Check if specific sound category recently heard
 * @param bridge Bridge handle
 * @param category Sound category
 * @param recent_ms Time window in milliseconds
 * @param heard Output: true if heard within window
 * @return 0 on success, -1 on error
 */
int audio_logic_category_heard_recently(
    const audio_logic_bridge_t* bridge,
    sound_category_t category,
    uint32_t recent_ms,
    bool* heard
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int audio_logic_bridge_get_stats(
    const audio_logic_bridge_t* bridge,
    audio_logic_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void audio_logic_bridge_reset_stats(audio_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_LOGIC_BRIDGE_H */
