//=============================================================================
// nimcp_text_generation.h - Creative Text Generation
//=============================================================================
/**
 * @file nimcp_text_generation.h
 * @brief Generates creative text content (poetry, prose, screenplays)
 *
 * WHAT: Produces high-quality creative writing in various forms
 * WHY:  Enable AI to create literature, screenplays, poetry
 * HOW:  Language models + style control + structural templates
 *
 * SUPPORTED FORMS:
 * - Poetry: Various verse forms (sonnet, haiku, free verse, etc.)
 * - Prose: Short stories, novel chapters, essays
 * - Screenplay: Film/TV scripts with proper formatting
 * - Lyrics: Song lyrics with rhyme and meter
 * - Dialogue: Character conversations
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_TEXT_GENERATION_H
#define NIMCP_TEXT_GENERATION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Poetry-Specific Types
//=============================================================================

/**
 * @brief Poetry verse forms
 */
typedef enum {
    VERSE_FREE = 0,                /**< Free verse (no fixed form) */
    VERSE_SONNET_SHAKESPEAREAN,    /**< 14 lines, ABAB CDCD EFEF GG */
    VERSE_SONNET_PETRARCHAN,       /**< 14 lines, ABBAABBA CDECDE */
    VERSE_HAIKU,                   /**< 5-7-5 syllables */
    VERSE_LIMERICK,                /**< AABBA rhyme scheme */
    VERSE_VILLANELLE,              /**< 19 lines, ABA ABA ABA ABA ABA ABAA */
    VERSE_BLANK_VERSE,             /**< Unrhymed iambic pentameter */
    VERSE_COUPLET,                 /**< Rhyming pairs */
    VERSE_QUATRAIN,                /**< Four-line stanzas */
    VERSE_BALLAD,                  /**< Narrative verse, ABCB */
    VERSE_ODE,                     /**< Formal address, varied form */
    VERSE_COUNT
} verse_form_t;

/**
 * @brief Poetry-specific request
 */
typedef struct {
    verse_form_t form;             /**< Verse form */
    uint32_t num_stanzas;          /**< Number of stanzas (0 = form default) */
    bool enforce_meter;            /**< Strictly enforce meter */
    bool enforce_rhyme;            /**< Strictly enforce rhyme scheme */
    float syllable_flexibility;    /**< [0-1] How flexible with syllables */
    const char* subject;           /**< Subject matter */
    const char* imagery_hints;     /**< Imagery guidance */
} poetry_request_t;

//=============================================================================
// Prose-Specific Types
//=============================================================================

/**
 * @brief Prose narrative structure
 */
typedef enum {
    NARRATIVE_LINEAR = 0,          /**< Chronological */
    NARRATIVE_NONLINEAR,           /**< Non-chronological */
    NARRATIVE_FRAMED,              /**< Story within story */
    NARRATIVE_EPISTOLARY,          /**< Letters/documents */
    NARRATIVE_STREAM_OF_CONSCIOUSNESS, /**< Interior monologue */
    NARRATIVE_COUNT
} narrative_structure_t;

/**
 * @brief Point of view
 */
typedef enum {
    POV_FIRST_PERSON = 0,          /**< I/we */
    POV_SECOND_PERSON,             /**< You */
    POV_THIRD_LIMITED,             /**< He/she, limited view */
    POV_THIRD_OMNISCIENT,          /**< He/she, all-knowing */
    POV_THIRD_OBJECTIVE,           /**< External observation only */
    POV_COUNT
} point_of_view_t;

/**
 * @brief Prose-specific request
 */
typedef struct {
    narrative_structure_t structure; /**< Narrative structure */
    point_of_view_t pov;           /**< Point of view */
    uint32_t target_word_count;    /**< Target length in words */
    const char* setting;           /**< Setting description */
    const char* characters;        /**< Character descriptions (comma-sep) */
    const char* plot_outline;      /**< Plot outline (optional) */
    const char* genre;             /**< Genre (fantasy, mystery, etc.) */
    bool include_dialogue;         /**< Include dialogue */
    float dialogue_ratio;          /**< [0-1] Dialogue vs. narration */
} prose_request_t;

//=============================================================================
// Screenplay-Specific Types
//=============================================================================

/**
 * @brief Screenplay element types
 */
typedef enum {
    SCREENPLAY_SCENE_HEADING = 0,  /**< INT./EXT. location - time */
    SCREENPLAY_ACTION,             /**< Action/description */
    SCREENPLAY_CHARACTER,          /**< Character name */
    SCREENPLAY_DIALOGUE,           /**< Character dialogue */
    SCREENPLAY_PARENTHETICAL,      /**< Actor direction */
    SCREENPLAY_TRANSITION,         /**< CUT TO:, FADE OUT:, etc. */
    SCREENPLAY_SHOT,               /**< Camera direction */
    SCREENPLAY_COUNT
} screenplay_element_type_t;

/**
 * @brief Screenplay element
 */
typedef struct {
    screenplay_element_type_t type;
    char* content;                 /**< Element content */
    char character[64];            /**< Character name (if dialogue) */
} screenplay_element_t;

/**
 * @brief Screenplay-specific request
 */
typedef struct {
    float target_page_count;       /**< Target pages (1 page ~ 1 min) */
    const char* logline;           /**< One-sentence premise */
    const char* characters;        /**< Main characters (comma-sep) */
    const char* setting;           /**< Primary setting */
    const char* genre;             /**< Genre */
    bool include_camera_directions; /**< Include shot directions */
    const char* act_structure;     /**< "3-act", "5-act", "7-point", etc. */
} screenplay_request_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Text generator configuration
 */
typedef struct {
    /* Model settings */
    char model_path[256];          /**< Path to text generation model */
    uint32_t max_context_length;   /**< Maximum context window */
    bool use_gpu;                  /**< Use GPU acceleration */
    int32_t gpu_device_id;         /**< GPU device ID */

    /* Generation settings */
    float default_temperature;     /**< Default sampling temperature */
    float default_top_p;           /**< Default nucleus sampling */
    uint32_t default_max_tokens;   /**< Default max tokens */

    /* Quality settings */
    bool enable_self_evaluation;   /**< Self-evaluate output */
    float min_quality_threshold;   /**< Reject below this quality */
    uint32_t max_regeneration_attempts; /**< Max retry attempts */

    /* Style settings */
    bool enable_style_control;     /**< Enable style embeddings */
    uint32_t style_embedding_dim;  /**< Style embedding dimension */
} text_generator_config_t;

/**
 * @brief Initialize config with defaults
 */
void text_generator_config_defaults(text_generator_config_t* config);

//=============================================================================
// Generator Structure
//=============================================================================

/**
 * @brief Text generator
 */
struct text_generator {
    text_generator_config_t config;

    /* Models */
    void* language_model;          /**< Core language model */
    void* poetry_adapter;          /**< Poetry-specific adapter */
    void* screenplay_formatter;    /**< Screenplay formatting model */

    /* Style control */
    void* style_encoder;           /**< Style encoding model */
    style_embedding_t* current_style;

    /* Integration */
    void* aesthetic_evaluator;     /**< For self-evaluation */
    void* creative_bridge;         /**< For validation */

    /* Cortical integration */
    void* speech_cortex;           /**< Speech cortex for prosody/phonology */

    /* Statistics */
    uint64_t texts_generated;
    float avg_quality_score;
    float avg_generation_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create text generator
 *
 * @param config Configuration (NULL for defaults)
 * @return Generator or NULL on error
 */
text_generator_t* text_generator_create(const text_generator_config_t* config);

/**
 * @brief Destroy text generator
 *
 * @param gen Generator to destroy
 */
void text_generator_destroy(text_generator_t* gen);

//=============================================================================
// General Generation API
//=============================================================================

/**
 * @brief Generate text from request
 *
 * @param gen Generator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate(text_generator_t* gen,
                  const text_generation_request_t* request,
                  text_generation_result_t* result);

/**
 * @brief Continue existing text
 *
 * @param gen Generator
 * @param existing Existing text to continue
 * @param existing_len Existing text length
 * @param style Target style (optional)
 * @param max_new_tokens Max tokens to generate
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_continue(text_generator_t* gen,
                           const char* existing, size_t existing_len,
                           const style_embedding_t* style,
                           uint32_t max_new_tokens,
                           text_generation_result_t* result);

//=============================================================================
// Poetry Generation API
//=============================================================================

/**
 * @brief Generate poetry
 *
 * @param gen Generator
 * @param request Poetry-specific request
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_poetry(text_generator_t* gen,
                         const poetry_request_t* request,
                         const style_embedding_t* style,
                         text_generation_result_t* result);

/**
 * @brief Generate haiku
 *
 * @param gen Generator
 * @param subject Subject/theme
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_haiku(text_generator_t* gen,
                        const char* subject,
                        const style_embedding_t* style,
                        text_generation_result_t* result);

/**
 * @brief Generate sonnet
 *
 * @param gen Generator
 * @param subject Subject/theme
 * @param shakespearean true for Shakespearean, false for Petrarchan
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_sonnet(text_generator_t* gen,
                         const char* subject,
                         bool shakespearean,
                         const style_embedding_t* style,
                         text_generation_result_t* result);

//=============================================================================
// Prose Generation API
//=============================================================================

/**
 * @brief Generate prose
 *
 * @param gen Generator
 * @param request Prose-specific request
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_prose(text_generator_t* gen,
                        const prose_request_t* request,
                        const style_embedding_t* style,
                        text_generation_result_t* result);

/**
 * @brief Generate short story
 *
 * @param gen Generator
 * @param premise Story premise
 * @param word_count Target word count
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_short_story(text_generator_t* gen,
                              const char* premise,
                              uint32_t word_count,
                              const style_embedding_t* style,
                              text_generation_result_t* result);

//=============================================================================
// Screenplay Generation API
//=============================================================================

/**
 * @brief Generate screenplay
 *
 * @param gen Generator
 * @param request Screenplay-specific request
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_screenplay(text_generator_t* gen,
                             const screenplay_request_t* request,
                             const style_embedding_t* style,
                             text_generation_result_t* result);

/**
 * @brief Generate single scene
 *
 * @param gen Generator
 * @param scene_description Scene description
 * @param characters Characters in scene (comma-sep)
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_scene(text_generator_t* gen,
                        const char* scene_description,
                        const char* characters,
                        const style_embedding_t* style,
                        text_generation_result_t* result);

//=============================================================================
// Dialogue Generation API
//=============================================================================

/**
 * @brief Generate dialogue between characters
 *
 * @param gen Generator
 * @param character_a First character description
 * @param character_b Second character description
 * @param situation Situation/context
 * @param num_exchanges Number of back-and-forth exchanges
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_dialogue(text_generator_t* gen,
                           const char* character_a,
                           const char* character_b,
                           const char* situation,
                           uint32_t num_exchanges,
                           const style_embedding_t* style,
                           text_generation_result_t* result);

//=============================================================================
// Lyrics Generation API
//=============================================================================

/**
 * @brief Generate song lyrics
 *
 * @param gen Generator
 * @param theme Song theme
 * @param structure Song structure (e.g., "VCVCBC")
 * @param style Target style (optional)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int text_generate_lyrics(text_generator_t* gen,
                         const char* theme,
                         const char* structure,
                         const style_embedding_t* style,
                         text_generation_result_t* result);

//=============================================================================
// Style Control API
//=============================================================================

/**
 * @brief Set generation style
 *
 * @param gen Generator
 * @param style Style embedding
 */
void text_generator_set_style(text_generator_t* gen,
                              const style_embedding_t* style);

/**
 * @brief Clear generation style
 *
 * @param gen Generator
 */
void text_generator_clear_style(text_generator_t* gen);

/**
 * @brief Get style from archetype
 *
 * @param gen Generator
 * @param archetype_id Literary archetype ID
 * @param out Output style
 * @return 0 on success, -1 on error
 */
int text_generator_archetype_style(text_generator_t* gen,
                                   literary_style_archetype_t archetype_id,
                                   style_embedding_t* out);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set aesthetic evaluator for self-evaluation
 *
 * @param gen Generator
 * @param evaluator Aesthetic evaluator
 */
void text_generator_set_evaluator(text_generator_t* gen, void* evaluator);

/**
 * @brief Set creative bridge for validation
 *
 * @param gen Generator
 * @param bridge Creative bridge
 */
void text_generator_set_bridge(text_generator_t* gen, void* bridge);

/**
 * @brief Set speech cortex for prosody and phonological analysis
 *
 * @param gen Generator
 * @param speech_cortex Speech cortex pointer
 */
void text_generator_set_speech_cortex(text_generator_t* gen, void* speech_cortex);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEXT_GENERATION_H */
