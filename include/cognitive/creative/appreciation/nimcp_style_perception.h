//=============================================================================
// nimcp_style_perception.h - Style Recognition and Analysis
//=============================================================================
/**
 * @file nimcp_style_perception.h
 * @brief Perceives and identifies artistic styles in content
 *
 * WHAT: Recognizes artistic styles across modalities
 * WHY:  Enable understanding of artistic language and conventions
 * HOW:  Pattern matching against known styles, neural style analysis
 *
 * CAPABILITIES:
 * - Identify which archetype(s) content resembles
 * - Detect hybrid/blended styles
 * - Extract unique style features
 * - Track style evolution over content
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_STYLE_PERCEPTION_H
#define NIMCP_STYLE_PERCEPTION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct style_perception style_perception_t;

//=============================================================================
// Style Perception Types
//=============================================================================

/**
 * @brief Style match result
 */
typedef struct {
    int32_t archetype_id;           /**< Archetype ID */
    art_modality_t modality;        /**< Modality */
    float similarity;               /**< [0-1] Similarity to archetype */
    float confidence;               /**< [0-1] Confidence in match */
    char archetype_name[64];        /**< Human-readable name */
} style_match_t;

/**
 * @brief Multi-style analysis result
 */
typedef struct {
    style_match_t* matches;         /**< Sorted by similarity (descending) */
    uint32_t num_matches;           /**< Number of matches */
    bool is_hybrid;                 /**< Is this a hybrid style? */
    float hybrid_coherence;         /**< [0-1] How coherent the hybrid is */
    float originality;              /**< [0-1] Distance from all archetypes */
    style_embedding_t extracted_style; /**< Extracted style embedding */
} style_analysis_result_t;

/**
 * @brief Style evolution tracking
 */
typedef struct {
    style_embedding_t* timeline;    /**< Style embeddings over time */
    uint32_t num_points;            /**< Number of points */
    float* timestamps;              /**< Relative timestamps */
    float total_drift;              /**< Total style change */
    float drift_rate;               /**< Rate of change per unit time */
    bool has_dramatic_shift;        /**< Did style dramatically change? */
    uint32_t shift_point;           /**< Index of dramatic shift (if any) */
} style_evolution_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Style perception configuration
 */
typedef struct {
    /* Matching settings */
    float match_threshold;          /**< Min similarity to report match */
    uint32_t max_matches;           /**< Max matches to return */
    bool detect_hybrids;            /**< Detect hybrid styles */
    float hybrid_threshold;         /**< Min second style weight for hybrid */

    /* Analysis settings */
    bool extract_embedding;         /**< Extract full style embedding */
    uint32_t embedding_dim;         /**< Dimension of style embeddings */
    bool track_evolution;           /**< Track style over time */

    /* Model paths */
    char literary_model_path[256];  /**< Path to literary style model */
    char music_model_path[256];     /**< Path to music style model */
    char visual_model_path[256];    /**< Path to visual style model */
    char cinema_model_path[256];    /**< Path to cinema style model */
} style_perception_config_t;

/**
 * @brief Initialize config with defaults
 */
void style_perception_config_defaults(style_perception_config_t* config);

//=============================================================================
// Perceiver Structure
//=============================================================================

/**
 * @brief Style perception module
 */
struct style_perception {
    style_perception_config_t config;

    /* Style models (one per modality category) */
    void* literary_model;           /**< Literary style model */
    void* music_model;              /**< Music style model */
    void* visual_model;             /**< Visual style model */
    void* cinema_model;             /**< Cinema style model */

    /* Archetype embeddings */
    style_embedding_t* literary_archetypes;
    style_embedding_t* music_archetypes;
    style_embedding_t* visual_archetypes;
    style_embedding_t* cinema_archetypes;

    /* Evolution tracking */
    style_evolution_t* current_evolution;

    /* Cortical integration pointers */
    void* visual_cortex;            /**< Visual cortex for visual style features */
    void* audio_cortex;             /**< Audio cortex for music style features */
    void* speech_cortex;            /**< Speech cortex for linguistic style */

    /* Statistics */
    uint64_t analyses_performed;
    float avg_analysis_time_ms;
    uint64_t last_analysis_us;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create style perception module
 *
 * @param config Configuration (NULL for defaults)
 * @return Perceiver or NULL on error
 */
style_perception_t* style_perception_create(
    const style_perception_config_t* config);

/**
 * @brief Destroy style perception module
 *
 * @param perc Perceiver to destroy
 */
void style_perception_destroy(style_perception_t* perc);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Analyze content for style
 *
 * @param perc Perceiver
 * @param content Content (type depends on modality)
 * @param modality Content modality
 * @param result Output analysis result
 * @return 0 on success, -1 on error
 */
int style_perception_analyze(style_perception_t* perc,
                              const void* content,
                              art_modality_t modality,
                              style_analysis_result_t* result);

/**
 * @brief Analyze text for literary style
 *
 * @param perc Perceiver
 * @param text Text content
 * @param len Text length
 * @param result Output analysis result
 * @return 0 on success, -1 on error
 */
int style_perception_analyze_text(style_perception_t* perc,
                                   const char* text, size_t len,
                                   style_analysis_result_t* result);

/**
 * @brief Analyze music for style
 *
 * @param perc Perceiver
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param result Output analysis result
 * @return 0 on success, -1 on error
 */
int style_perception_analyze_music(style_perception_t* perc,
                                    const music_track_t* tracks, uint32_t num_tracks,
                                    style_analysis_result_t* result);

/**
 * @brief Analyze image for visual style
 *
 * @param perc Perceiver
 * @param image Image to analyze
 * @param result Output analysis result
 * @return 0 on success, -1 on error
 */
int style_perception_analyze_visual(style_perception_t* perc,
                                     const visual_image_t* image,
                                     style_analysis_result_t* result);

//=============================================================================
// Comparison API
//=============================================================================

/**
 * @brief Compare two styles
 *
 * @param perc Perceiver
 * @param style_a First style
 * @param style_b Second style
 * @return Similarity [0-1]
 */
float style_perception_compare(const style_perception_t* perc,
                                const style_embedding_t* style_a,
                                const style_embedding_t* style_b);

/**
 * @brief Find closest archetype to style
 *
 * @param perc Perceiver
 * @param style Input style
 * @param out Output match
 * @return 0 on success, -1 on error
 */
int style_perception_closest_archetype(const style_perception_t* perc,
                                        const style_embedding_t* style,
                                        style_match_t* out);

/**
 * @brief Decompose style into archetype mixture
 *
 * @param perc Perceiver
 * @param style Input style
 * @param weights Output weights for each archetype (caller allocated)
 * @param max_archetypes Maximum number of archetypes in modality
 * @return Number of archetypes with non-zero weight
 */
uint32_t style_perception_decompose(const style_perception_t* perc,
                                     const style_embedding_t* style,
                                     float* weights,
                                     uint32_t max_archetypes);

//=============================================================================
// Evolution Tracking API
//=============================================================================

/**
 * @brief Start tracking style evolution
 *
 * @param perc Perceiver
 * @return 0 on success, -1 on error
 */
int style_perception_start_evolution_tracking(style_perception_t* perc);

/**
 * @brief Add point to evolution timeline
 *
 * @param perc Perceiver
 * @param style Style at this point
 * @param timestamp Relative timestamp
 * @return 0 on success, -1 on error
 */
int style_perception_add_evolution_point(style_perception_t* perc,
                                          const style_embedding_t* style,
                                          float timestamp);

/**
 * @brief Get style evolution analysis
 *
 * @param perc Perceiver
 * @param out Output evolution analysis
 * @return 0 on success, -1 on error
 */
int style_perception_get_evolution(const style_perception_t* perc,
                                    style_evolution_t* out);

/**
 * @brief Stop and clear evolution tracking
 *
 * @param perc Perceiver
 */
void style_perception_stop_evolution_tracking(style_perception_t* perc);

//=============================================================================
// Archetype Info API
//=============================================================================

/**
 * @brief Get archetype name
 *
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @return Archetype name or "Unknown"
 */
const char* style_perception_archetype_name(art_modality_t modality,
                                             int32_t archetype_id);

/**
 * @brief Get archetype description
 *
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @return Description or "No description available"
 */
const char* style_perception_archetype_description(art_modality_t modality,
                                                    int32_t archetype_id);

/**
 * @brief Get archetype embedding
 *
 * @param perc Perceiver
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_perception_get_archetype(const style_perception_t* perc,
                                    art_modality_t modality,
                                    int32_t archetype_id,
                                    style_embedding_t* out);

/**
 * @brief Get number of archetypes for modality
 *
 * @param modality Art modality
 * @return Number of archetypes
 */
uint32_t style_perception_archetype_count(art_modality_t modality);

//=============================================================================
// Cortical Integration API
//=============================================================================

/**
 * @brief Set visual cortex for visual style features
 *
 * @param perc Perceiver
 * @param visual_cortex Visual cortex pointer (V1-IT features)
 */
void style_perception_set_visual_cortex(style_perception_t* perc, void* visual_cortex);

/**
 * @brief Set audio cortex for music style features
 *
 * @param perc Perceiver
 * @param audio_cortex Audio cortex pointer (A1 features)
 */
void style_perception_set_audio_cortex(style_perception_t* perc, void* audio_cortex);

/**
 * @brief Set speech cortex for linguistic style
 *
 * @param perc Perceiver
 * @param speech_cortex Speech cortex pointer
 */
void style_perception_set_speech_cortex(style_perception_t* perc, void* speech_cortex);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free style analysis result
 *
 * @param result Result to free
 */
void style_analysis_result_free(style_analysis_result_t* result);

/**
 * @brief Free style evolution
 *
 * @param evolution Evolution to free
 */
void style_evolution_free(style_evolution_t* evolution);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STYLE_PERCEPTION_H */
