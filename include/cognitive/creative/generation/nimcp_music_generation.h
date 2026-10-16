//=============================================================================
// nimcp_music_generation.h - Creative Music Generation
//=============================================================================
/**
 * @file nimcp_music_generation.h
 * @brief Generates music compositions and audio
 *
 * WHAT: Creates music from symbolic (MIDI) to audio waveforms
 * WHY:  Enable AI to compose and produce music
 * HOW:  Combines compositional rules, neural generation, and audio synthesis
 *
 * OUTPUT FORMATS:
 * - MIDI: Symbolic note representation
 * - Audio: Waveform (WAV, MP3, FLAC)
 * - Sheet music: MusicXML (future)
 *
 * GENERATION APPROACHES:
 * - Rule-based: Music theory constraints
 * - Neural: Learned patterns from training data
 * - Hybrid: Rules + neural for best results
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_MUSIC_GENERATION_H
#define NIMCP_MUSIC_GENERATION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Music Theory Types
//=============================================================================

/**
 * @brief Musical key
 */
typedef struct {
    uint8_t root;                  /**< Root note (0-11, C=0) */
    bool is_minor;                 /**< true for minor, false for major */
    uint8_t mode;                  /**< Mode (0=Ionian/major, 1=Dorian, etc.) */
} music_key_t;

/**
 * @brief Time signature
 */
typedef struct {
    uint8_t numerator;             /**< Beats per measure */
    uint8_t denominator;           /**< Note value of beat */
} time_signature_t;

/**
 * @brief Chord representation
 */
typedef struct {
    uint8_t root;                  /**< Root note */
    uint8_t quality;               /**< 0=maj, 1=min, 2=dim, 3=aug, 4=dom7, etc. */
    uint8_t bass;                  /**< Bass note (for inversions) */
    float duration_beats;          /**< Chord duration */
} music_chord_t;

/**
 * @brief Chord progression
 */
typedef struct {
    music_chord_t* chords;         /**< Array of chords */
    uint32_t num_chords;           /**< Number of chords */
    music_key_t key;               /**< Key of progression */
} chord_progression_t;

//=============================================================================
// Instrument Types
//=============================================================================

/**
 * @brief Instrument categories
 */
typedef enum {
    INSTRUMENT_PIANO = 0,          /**< Piano family */
    INSTRUMENT_STRINGS,            /**< Strings (violin, cello, etc.) */
    INSTRUMENT_BRASS,              /**< Brass (trumpet, horn, etc.) */
    INSTRUMENT_WOODWINDS,          /**< Woodwinds (flute, clarinet, etc.) */
    INSTRUMENT_PERCUSSION,         /**< Drums and percussion */
    INSTRUMENT_GUITAR,             /**< Guitar family */
    INSTRUMENT_BASS,               /**< Bass instruments */
    INSTRUMENT_SYNTH,              /**< Synthesizers */
    INSTRUMENT_VOICE,              /**< Vocal */
    INSTRUMENT_OTHER               /**< Other */
} instrument_category_t;

/**
 * @brief Instrument specification
 */
typedef struct {
    instrument_category_t category;
    uint8_t gm_program;            /**< General MIDI program number */
    char name[32];                 /**< Instrument name */
    uint8_t min_note;              /**< Lowest playable note */
    uint8_t max_note;              /**< Highest playable note */
} instrument_spec_t;

//=============================================================================
// Musical Form Types
//=============================================================================

/**
 * @brief Musical form/structure
 */
typedef enum {
    MUSIC_FORM_FREE = 0,           /**< No fixed structure */
    MUSIC_FORM_VERSE_CHORUS,       /**< Pop song structure */
    MUSIC_FORM_SONATA,             /**< Sonata form (exposition, development, recap) */
    MUSIC_FORM_RONDO,              /**< ABACA form */
    MUSIC_FORM_BINARY,             /**< AB form */
    MUSIC_FORM_TERNARY,            /**< ABA form */
    MUSIC_FORM_THEME_VARIATIONS,   /**< Theme and variations */
    MUSIC_FORM_FUGUE,              /**< Fugal counterpoint */
    MUSIC_FORM_TWELVE_BAR_BLUES,   /**< 12-bar blues */
    MUSIC_FORM_THROUGH_COMPOSED,   /**< No repeating sections */
    MUSIC_FORM_COUNT
} music_form_t;

/**
 * @brief Section specification
 */
typedef struct {
    char label;                    /**< Section label (A, B, C, etc.) */
    uint32_t num_measures;         /**< Number of measures */
    float energy_level;            /**< [0-1] Dynamic intensity */
    char mood[32];                 /**< Section mood */
    chord_progression_t* progression; /**< Chord progression (optional) */
} music_section_t;

//=============================================================================
// Generation Types
//=============================================================================

/**
 * @brief Generation approach
 */
typedef enum {
    GEN_APPROACH_NEURAL = 0,       /**< Pure neural generation */
    GEN_APPROACH_RULE_BASED,       /**< Pure rule-based */
    GEN_APPROACH_HYBRID,           /**< Neural + rules (recommended) */
    GEN_APPROACH_TEMPLATE          /**< Template-based with variation */
} generation_approach_t;

/**
 * @brief Extended music generation request
 */
typedef struct {
    /* Basic parameters */
    style_embedding_t* style;      /**< Target style */
    float duration_seconds;        /**< Target duration */
    uint16_t tempo_bpm;            /**< Tempo */
    music_key_t key;               /**< Musical key */
    time_signature_t time_sig;     /**< Time signature */

    /* Structure */
    music_form_t form;             /**< Musical form */
    music_section_t* sections;     /**< Section specs (optional) */
    uint32_t num_sections;         /**< Number of sections */

    /* Instrumentation */
    instrument_spec_t* instruments; /**< Instruments to use */
    uint32_t num_instruments;      /**< Number of instruments */

    /* Generation settings */
    generation_approach_t approach; /**< Generation approach */
    float creativity;              /**< [0-1] Creativity/randomness level */
    float coherence;               /**< [0-1] Structural coherence */

    /* Mood/character */
    const char* mood;              /**< Overall mood */
    const char* imagery;           /**< Visual/emotional imagery */
    float energy;                  /**< [0-1] Overall energy level */

    /* Output settings */
    bool generate_midi;            /**< Generate MIDI data */
    bool generate_audio;           /**< Generate audio waveform */
    uint32_t audio_sample_rate;    /**< Audio sample rate */
} music_generation_request_ext_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Music generator configuration
 */
typedef struct {
    /* Model settings */
    char composition_model_path[256]; /**< Composition model path */
    char audio_model_path[256];    /**< Audio synthesis model path */
    char soundfont_path[256];      /**< SoundFont for MIDI rendering */
    bool use_gpu;                  /**< Use GPU */
    int32_t gpu_device_id;         /**< GPU device ID */

    /* Generation settings */
    generation_approach_t default_approach;
    float default_creativity;
    float default_coherence;

    /* Quality settings */
    bool enable_self_evaluation;
    float min_quality_threshold;
    uint32_t max_regeneration_attempts;

    /* Audio settings */
    uint32_t default_sample_rate;
    uint8_t default_bit_depth;
} music_generator_config_t;

/**
 * @brief Initialize config with defaults
 */
void music_generator_config_defaults(music_generator_config_t* config);

//=============================================================================
// Generator Structure
//=============================================================================

/**
 * @brief Music generator
 */
struct music_generator {
    music_generator_config_t config;

    /* Models */
    void* composition_model;       /**< Composition model */
    void* audio_model;             /**< Audio synthesis model */
    void* soundfont;               /**< SoundFont for rendering */

    /* Style control */
    void* style_encoder;
    style_embedding_t* current_style;

    /* Integration */
    void* aesthetic_evaluator;
    void* creative_bridge;

    /* Cortical integration */
    void* audio_cortex;            /**< Audio cortex for harmonic analysis */

    /* Statistics */
    uint64_t pieces_generated;
    float avg_quality_score;
    float avg_generation_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create music generator
 *
 * @param config Configuration (NULL for defaults)
 * @return Generator or NULL on error
 */
music_generator_t* music_generator_create(const music_generator_config_t* config);

/**
 * @brief Destroy music generator
 *
 * @param gen Generator to destroy
 */
void music_generator_destroy(music_generator_t* gen);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate music from request
 *
 * @param gen Generator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int music_generate(music_generator_t* gen,
                   const music_generation_request_t* request,
                   music_generation_result_t* result);

/**
 * @brief Generate music with extended options
 *
 * @param gen Generator
 * @param request Extended request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int music_generate_extended(music_generator_t* gen,
                            const music_generation_request_ext_t* request,
                            music_generation_result_t* result);

/**
 * @brief Generate melody over chord progression
 *
 * @param gen Generator
 * @param progression Chord progression
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int music_generate_melody(music_generator_t* gen,
                          const chord_progression_t* progression,
                          const style_embedding_t* style,
                          music_generation_result_t* result);

/**
 * @brief Generate accompaniment for melody
 *
 * @param gen Generator
 * @param melody Melody track
 * @param style Target style (optional)
 * @param result Output result (includes original melody)
 * @return 0 on success, -1 on error
 */
int music_generate_accompaniment(music_generator_t* gen,
                                 const music_track_t* melody,
                                 const style_embedding_t* style,
                                 music_generation_result_t* result);

/**
 * @brief Generate chord progression
 *
 * @param gen Generator
 * @param key Musical key
 * @param num_measures Number of measures
 * @param style Target style (optional)
 * @param progression Output progression
 * @return 0 on success, -1 on error
 */
int music_generate_progression(music_generator_t* gen,
                               const music_key_t* key,
                               uint32_t num_measures,
                               const style_embedding_t* style,
                               chord_progression_t* progression);

//=============================================================================
// Film Score API
//=============================================================================

/**
 * @brief Generate film score for scene
 *
 * @param gen Generator
 * @param scene_description Scene description
 * @param duration_seconds Scene duration
 * @param mood Scene mood
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int music_generate_film_score(music_generator_t* gen,
                              const char* scene_description,
                              float duration_seconds,
                              const char* mood,
                              const style_embedding_t* style,
                              music_generation_result_t* result);

/**
 * @brief Generate leitmotif (recurring theme)
 *
 * @param gen Generator
 * @param character_description Character/concept description
 * @param style Target style (optional)
 * @param result Output result (short motif)
 * @return 0 on success, -1 on error
 */
int music_generate_leitmotif(music_generator_t* gen,
                             const char* character_description,
                             const style_embedding_t* style,
                             music_generation_result_t* result);

//=============================================================================
// Variation API
//=============================================================================

/**
 * @brief Generate variation of existing music
 *
 * @param gen Generator
 * @param original Original music
 * @param variation_strength [0-1] How different from original
 * @param result Output variation
 * @return 0 on success, -1 on error
 */
int music_generate_variation(music_generator_t* gen,
                             const music_generation_result_t* original,
                             float variation_strength,
                             music_generation_result_t* result);

/**
 * @brief Extend existing music
 *
 * @param gen Generator
 * @param existing Existing music
 * @param additional_seconds Seconds to add
 * @param result Output extended music
 * @return 0 on success, -1 on error
 */
int music_extend(music_generator_t* gen,
                 const music_generation_result_t* existing,
                 float additional_seconds,
                 music_generation_result_t* result);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export to MIDI file
 *
 * @param result Music result
 * @param path Output file path
 * @return 0 on success, -1 on error
 */
int music_export_midi(const music_generation_result_t* result, const char* path);

/**
 * @brief Export to audio file
 *
 * @param result Music result
 * @param path Output file path
 * @param format Format ("wav", "mp3", "flac")
 * @return 0 on success, -1 on error
 */
int music_export_audio(const music_generation_result_t* result,
                       const char* path, const char* format);

/**
 * @brief Render MIDI to audio
 *
 * @param gen Generator
 * @param result Music result (modifies to add audio)
 * @param sample_rate Output sample rate
 * @return 0 on success, -1 on error
 */
int music_render_audio(music_generator_t* gen,
                       music_generation_result_t* result,
                       uint32_t sample_rate);

//=============================================================================
// Style Control API
//=============================================================================

/**
 * @brief Set generation style
 *
 * @param gen Generator
 * @param style Style embedding
 */
void music_generator_set_style(music_generator_t* gen,
                               const style_embedding_t* style);

/**
 * @brief Clear generation style
 *
 * @param gen Generator
 */
void music_generator_clear_style(music_generator_t* gen);

/**
 * @brief Get style from archetype
 *
 * @param gen Generator
 * @param archetype_id Musical archetype ID
 * @param out Output style
 * @return 0 on success, -1 on error
 */
int music_generator_archetype_style(music_generator_t* gen,
                                    musical_style_archetype_t archetype_id,
                                    style_embedding_t* out);

//=============================================================================
// Cortical Integration API
//=============================================================================

/**
 * @brief Set audio cortex for harmonic analysis feedback
 *
 * @param gen Generator
 * @param audio_cortex Audio cortex pointer (A1 features)
 */
void music_generator_set_audio_cortex(music_generator_t* gen, void* audio_cortex);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Parse key string to key struct
 *
 * @param key_string Key string (e.g., "C major", "F# minor")
 * @param out Output key
 * @return 0 on success, -1 on error
 */
int music_parse_key(const char* key_string, music_key_t* out);

/**
 * @brief Get key name string
 *
 * @param key Key struct
 * @return Key name string
 */
const char* music_key_name(const music_key_t* key);

/**
 * @brief Free chord progression
 *
 * @param progression Progression to free
 */
void chord_progression_free(chord_progression_t* progression);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MUSIC_GENERATION_H */
