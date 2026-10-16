//=============================================================================
// nimcp_creative_pattern_extractor.h - Artistic Pattern Extraction
//=============================================================================
/**
 * @file nimcp_creative_pattern_extractor.h
 * @brief Extracts structural and thematic patterns from artistic works
 *
 * WHAT: Identifies recurring patterns, motifs, and structures in art
 * WHY:  Enable learning from and reproducing artistic techniques
 * HOW:  Multi-level pattern analysis (surface, structural, thematic)
 *
 * PATTERN LEVELS:
 * - Surface: Direct features (rhythm, color, melody)
 * - Structural: Organization (form, composition, narrative)
 * - Thematic: Meaning (themes, motifs, symbolism)
 * - Meta: Patterns across works (artist signatures, period styles)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_PATTERN_EXTRACTOR_H
#define NIMCP_CREATIVE_PATTERN_EXTRACTOR_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Pattern Types
//=============================================================================

/**
 * @brief Pattern extraction levels
 */
typedef enum {
    PATTERN_LEVEL_SURFACE = 0,     /**< Direct observable features */
    PATTERN_LEVEL_STRUCTURAL,      /**< Organization and form */
    PATTERN_LEVEL_THEMATIC,        /**< Meaning and symbolism */
    PATTERN_LEVEL_META,            /**< Cross-work patterns */
    PATTERN_LEVEL_ALL              /**< All levels */
} pattern_level_t;

/**
 * @brief Pattern categories by modality
 */
typedef enum {
    /* Text patterns */
    PATTERN_TEXT_RHYTHM = 0,       /**< Prose rhythm patterns */
    PATTERN_TEXT_SYNTAX,           /**< Syntactic structures */
    PATTERN_TEXT_VOCABULARY,       /**< Word choice patterns */
    PATTERN_TEXT_NARRATIVE,        /**< Narrative structures */
    PATTERN_TEXT_IMAGERY,          /**< Imagery and metaphor */
    PATTERN_TEXT_THEME,            /**< Thematic elements */

    /* Music patterns */
    PATTERN_MUSIC_MELODY = 20,     /**< Melodic patterns */
    PATTERN_MUSIC_HARMONY,         /**< Harmonic progressions */
    PATTERN_MUSIC_RHYTHM,          /**< Rhythmic patterns */
    PATTERN_MUSIC_FORM,            /**< Formal structure */
    PATTERN_MUSIC_TEXTURE,         /**< Textural patterns */
    PATTERN_MUSIC_MOTIF,           /**< Motivic development */

    /* Visual patterns */
    PATTERN_VISUAL_COMPOSITION = 40, /**< Compositional patterns */
    PATTERN_VISUAL_COLOR,          /**< Color patterns */
    PATTERN_VISUAL_TEXTURE,        /**< Textural patterns */
    PATTERN_VISUAL_SHAPE,          /**< Shape and form */
    PATTERN_VISUAL_LIGHT,          /**< Light and shadow */
    PATTERN_VISUAL_SYMBOL,         /**< Symbolic elements */

    /* Cinema patterns */
    PATTERN_CINEMA_SHOT = 60,      /**< Shot composition */
    PATTERN_CINEMA_EDITING,        /**< Editing patterns */
    PATTERN_CINEMA_SOUND,          /**< Sound design patterns */
    PATTERN_CINEMA_NARRATIVE,      /**< Narrative structure */
    PATTERN_CINEMA_VISUAL,         /**< Visual motifs */
    PATTERN_CINEMA_PACING          /**< Pacing patterns */
} creative_pattern_category_t;

/**
 * @brief Single extracted pattern
 */
typedef struct {
    creative_pattern_category_t category;   /**< Pattern category */
    pattern_level_t level;         /**< Extraction level */
    char name[64];                 /**< Pattern name */
    char description[256];         /**< Pattern description */
    float* feature_vector;         /**< Numeric representation */
    uint32_t feature_dim;          /**< Feature dimension */
    float prevalence;              /**< [0-1] How prevalent in source */
    float distinctiveness;         /**< [0-1] How distinctive */
    float importance;              /**< [0-1] Importance to overall style */
    uint32_t occurrence_count;     /**< Number of occurrences */
} extracted_pattern_t;

/**
 * @brief Pattern extraction result
 */
typedef struct {
    extracted_pattern_t* patterns; /**< Extracted patterns */
    uint32_t num_patterns;         /**< Number of patterns */
    art_modality_t modality;       /**< Source modality */
    float extraction_confidence;   /**< [0-1] Overall confidence */
    float coverage;                /**< [0-1] How much of work is captured */
    uint64_t extraction_time_ms;   /**< Time to extract */
} pattern_extraction_result_t;

/**
 * @brief Pattern match (pattern found in content)
 */
typedef struct {
    uint32_t pattern_idx;          /**< Index of matched pattern */
    float similarity;              /**< [0-1] How similar */
    float position;                /**< [0-1] Relative position in work */
    float span;                    /**< [0-1] Span of pattern */
} pattern_match_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Pattern extractor configuration
 */
typedef struct {
    /* Extraction settings */
    pattern_level_t extraction_level; /**< Which levels to extract */
    float min_prevalence;          /**< Minimum prevalence to report */
    float min_distinctiveness;     /**< Minimum distinctiveness to report */
    uint32_t max_patterns;         /**< Maximum patterns to return */

    /* Feature settings */
    uint32_t feature_dim;          /**< Feature vector dimension */
    bool normalize_features;       /**< Normalize feature vectors */

    /* Analysis settings */
    bool analyze_structure;        /**< Perform structural analysis */
    bool analyze_semantics;        /**< Perform semantic analysis */
    bool cross_reference;          /**< Cross-reference with known patterns */

    /* Model paths */
    char text_pattern_model[256];
    char music_pattern_model[256];
    char visual_pattern_model[256];
} creative_pattern_extractor_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_pattern_extractor_config_defaults(
    creative_pattern_extractor_config_t* config);

//=============================================================================
// Extractor Structure
//=============================================================================

/**
 * @brief Creative pattern extractor
 */
struct creative_pattern_extractor {
    creative_pattern_extractor_config_t config;

    /* Pattern models */
    void* text_model;
    void* music_model;
    void* visual_model;
    void* cinema_model;

    /* Pattern database (known patterns) */
    extracted_pattern_t* known_patterns;
    uint32_t num_known_patterns;
    uint32_t known_capacity;

    /* Cortical integration */
    void* cortical_columns;        /**< Cortical columns for hierarchical features */

    /* Statistics */
    uint64_t extractions_performed;
    float avg_extraction_time_ms;
    float avg_patterns_per_work;
};

/** @brief Typedef for creative_pattern_extractor */
typedef struct creative_pattern_extractor creative_pattern_extractor_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pattern extractor
 *
 * @param config Configuration (NULL for defaults)
 * @return Extractor or NULL on error
 */
creative_pattern_extractor_t* creative_pattern_extractor_create(
    const creative_pattern_extractor_config_t* config);

/**
 * @brief Destroy pattern extractor
 *
 * @param ext Extractor to destroy
 */
void creative_pattern_extractor_destroy(creative_pattern_extractor_t* ext);

//=============================================================================
// Extraction API
//=============================================================================

/**
 * @brief Extract patterns from text
 *
 * @param ext Extractor
 * @param text Text content
 * @param len Text length
 * @param modality Text modality
 * @param result Output extraction result
 * @return 0 on success, -1 on error
 */
int creative_pattern_extract_text(creative_pattern_extractor_t* ext,
                                   const char* text, size_t len,
                                   art_modality_t modality,
                                   pattern_extraction_result_t* result);

/**
 * @brief Extract patterns from music
 *
 * @param ext Extractor
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param result Output extraction result
 * @return 0 on success, -1 on error
 */
int creative_pattern_extract_music(creative_pattern_extractor_t* ext,
                                    const music_track_t* tracks, uint32_t num_tracks,
                                    pattern_extraction_result_t* result);

/**
 * @brief Extract patterns from image
 *
 * @param ext Extractor
 * @param image Image
 * @param result Output extraction result
 * @return 0 on success, -1 on error
 */
int creative_pattern_extract_visual(creative_pattern_extractor_t* ext,
                                     const visual_image_t* image,
                                     pattern_extraction_result_t* result);

//=============================================================================
// Pattern Matching API
//=============================================================================

/**
 * @brief Find patterns in content
 *
 * @param ext Extractor
 * @param content Content
 * @param modality Content modality
 * @param patterns Patterns to search for
 * @param num_patterns Number of patterns
 * @param matches Output matches (caller allocated)
 * @param max_matches Maximum matches
 * @return Number of matches found
 */
uint32_t creative_pattern_find(const creative_pattern_extractor_t* ext,
                                const void* content,
                                art_modality_t modality,
                                const extracted_pattern_t* patterns,
                                uint32_t num_patterns,
                                pattern_match_t* matches,
                                uint32_t max_matches);

/**
 * @brief Compare two sets of patterns
 *
 * @param ext Extractor
 * @param patterns_a First pattern set
 * @param num_a Number in first set
 * @param patterns_b Second pattern set
 * @param num_b Number in second set
 * @return Similarity [0-1]
 */
float creative_pattern_compare(const creative_pattern_extractor_t* ext,
                                const extracted_pattern_t* patterns_a, uint32_t num_a,
                                const extracted_pattern_t* patterns_b, uint32_t num_b);

//=============================================================================
// Pattern Database API
//=============================================================================

/**
 * @brief Add pattern to database
 *
 * @param ext Extractor
 * @param pattern Pattern to add
 * @return Pattern index on success, -1 on error
 */
int32_t creative_pattern_db_add(creative_pattern_extractor_t* ext,
                                 const extracted_pattern_t* pattern);

/**
 * @brief Find similar patterns in database
 *
 * @param ext Extractor
 * @param query Query pattern
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_pattern_db_find_similar(const creative_pattern_extractor_t* ext,
                                           const extracted_pattern_t* query,
                                           uint32_t max_results,
                                           extracted_pattern_t* results);

/**
 * @brief Get patterns by category
 *
 * @param ext Extractor
 * @param category Pattern category
 * @param max_results Maximum results
 * @param results Output array (caller allocated)
 * @return Number of results
 */
uint32_t creative_pattern_db_by_category(const creative_pattern_extractor_t* ext,
                                          creative_pattern_category_t category,
                                          uint32_t max_results,
                                          extracted_pattern_t* results);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Analyze pattern distribution in work
 *
 * @param ext Extractor
 * @param result Extraction result to analyze
 * @param category_counts Output: count per category (caller allocated, size = 80)
 * @param level_counts Output: count per level (caller allocated, size = 5)
 */
void creative_pattern_analyze_distribution(const creative_pattern_extractor_t* ext,
                                            const pattern_extraction_result_t* result,
                                            uint32_t* category_counts,
                                            uint32_t* level_counts);

/**
 * @brief Find dominant patterns
 *
 * @param result Extraction result
 * @param n Number of top patterns
 * @param indices Output: indices of top patterns (caller allocated)
 * @return Actual number returned
 */
uint32_t creative_pattern_top_n(const pattern_extraction_result_t* result,
                                 uint32_t n,
                                 uint32_t* indices);

/**
 * @brief Calculate pattern uniqueness
 *
 * How unique is this pattern compared to database?
 *
 * @param ext Extractor
 * @param pattern Pattern to check
 * @return Uniqueness [0-1] (1 = completely unique)
 */
float creative_pattern_uniqueness(const creative_pattern_extractor_t* ext,
                                   const extracted_pattern_t* pattern);

//=============================================================================
// Cortical Integration API
//=============================================================================

/**
 * @brief Set cortical columns for hierarchical feature extraction
 *
 * @param ext Extractor
 * @param cortical_columns Cortical columns pointer
 */
void creative_pattern_extractor_set_cortical_columns(
    creative_pattern_extractor_t* ext, void* cortical_columns);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free extraction result
 *
 * @param result Result to free
 */
void pattern_extraction_result_free(pattern_extraction_result_t* result);

/**
 * @brief Free single pattern
 *
 * @param pattern Pattern to free
 */
void extracted_pattern_free(extracted_pattern_t* pattern);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_PATTERN_EXTRACTOR_H */
