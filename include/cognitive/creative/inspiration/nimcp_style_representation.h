//=============================================================================
// nimcp_style_representation.h - Style Embedding and Representation
//=============================================================================
/**
 * @file nimcp_style_representation.h
 * @brief Creates and manages style embeddings for creative inspiration
 *
 * WHAT: Generates dense vector representations of artistic styles
 * WHY:  Enable style transfer, blending, and comparison
 * HOW:  Neural embeddings + archetype-based parametric representations
 *
 * DESIGN PATTERN:
 * Follows the archetype pattern from nimcp_financial_investor_archetype.h
 * Each archetype has a canonical embedding that can be retrieved and blended.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_STYLE_REPRESENTATION_H
#define NIMCP_STYLE_REPRESENTATION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Style representer configuration
 */
typedef struct {
    /* Embedding settings */
    uint32_t embedding_dim;         /**< Dimension of style embeddings (256-1024) */
    bool use_pretrained;            /**< Use pretrained archetype embeddings */
    char embedding_model_path[256]; /**< Path to embedding model */

    /* Archetype settings */
    bool load_literary_archetypes;  /**< Load literary style archetypes */
    bool load_music_archetypes;     /**< Load music style archetypes */
    bool load_visual_archetypes;    /**< Load visual style archetypes */
    bool load_cinema_archetypes;    /**< Load cinema style archetypes */

    /* Computation settings */
    bool enable_gpu;                /**< Use GPU for embedding computation */
    int32_t gpu_device_id;          /**< GPU device ID */
} style_representer_config_t;

/**
 * @brief Initialize config with defaults
 */
void style_representer_config_defaults(style_representer_config_t* config);

//=============================================================================
// Archetype Information
//=============================================================================

/**
 * @brief Detailed archetype information
 */
typedef struct {
    int32_t archetype_id;           /**< Archetype ID */
    art_modality_t modality;        /**< Associated modality */
    char name[64];                  /**< Name */
    char description[512];          /**< Detailed description */
    char era[32];                   /**< Historical era/period */
    char characteristics[256];      /**< Key characteristics */
    style_embedding_t canonical;    /**< Canonical embedding */
} archetype_info_t;

//=============================================================================
// Representer Structure
//=============================================================================

/**
 * @brief Style representation module
 */
struct style_representer {
    style_representer_config_t config;

    /* Embedding model */
    void* embedding_model;          /**< Neural embedding model */
    uint32_t embedding_dim;         /**< Active embedding dimension */

    /* Archetype storage */
    archetype_info_t* literary_archetypes;
    uint32_t num_literary;
    archetype_info_t* music_archetypes;
    uint32_t num_music;
    archetype_info_t* visual_archetypes;
    uint32_t num_visual;
    archetype_info_t* cinema_archetypes;
    uint32_t num_cinema;

    /* GPU context */
    void* gpu_context;

    /* Cortical integration */
    void* cortical_columns;         /**< Cortical columns for multi-scale features */

    /* Statistics */
    uint64_t embeddings_computed;
    float avg_compute_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create style representer
 *
 * @param config Configuration (NULL for defaults)
 * @return Representer or NULL on error
 */
style_representer_t* style_representer_create(
    const style_representer_config_t* config);

/**
 * @brief Destroy style representer
 *
 * @param repr Representer to destroy
 */
void style_representer_destroy(style_representer_t* repr);

//=============================================================================
// Embedding API
//=============================================================================

/**
 * @brief Compute style embedding from text
 *
 * @param repr Representer
 * @param text Text content
 * @param len Text length
 * @param modality Text modality
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_repr_embed_text(style_representer_t* repr,
                          const char* text, size_t len,
                          art_modality_t modality,
                          style_embedding_t* out);

/**
 * @brief Compute style embedding from music
 *
 * @param repr Representer
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_repr_embed_music(style_representer_t* repr,
                           const music_track_t* tracks, uint32_t num_tracks,
                           style_embedding_t* out);

/**
 * @brief Compute style embedding from image
 *
 * @param repr Representer
 * @param image Image
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_repr_embed_visual(style_representer_t* repr,
                            const visual_image_t* image,
                            style_embedding_t* out);

/**
 * @brief Compute style embedding from audio
 *
 * @param repr Representer
 * @param audio Audio samples
 * @param num_samples Number of samples
 * @param sample_rate Sample rate
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_repr_embed_audio(style_representer_t* repr,
                           const float* audio, uint64_t num_samples,
                           uint32_t sample_rate,
                           style_embedding_t* out);

//=============================================================================
// Archetype API
//=============================================================================

/**
 * @brief Get archetype information
 *
 * @param repr Representer
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @param out Output info
 * @return 0 on success, -1 on error
 */
int style_repr_get_archetype_info(const style_representer_t* repr,
                                   art_modality_t modality,
                                   int32_t archetype_id,
                                   archetype_info_t* out);

/**
 * @brief Get archetype embedding
 *
 * @param repr Representer
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @param out Output embedding
 * @return 0 on success, -1 on error
 */
int style_repr_get_archetype_embedding(const style_representer_t* repr,
                                        art_modality_t modality,
                                        int32_t archetype_id,
                                        style_embedding_t* out);

/**
 * @brief List all archetypes for modality
 *
 * @param repr Representer
 * @param modality Art modality
 * @param out Output array (caller allocated)
 * @param max_count Maximum entries
 * @return Number of archetypes
 */
uint32_t style_repr_list_archetypes(const style_representer_t* repr,
                                     art_modality_t modality,
                                     archetype_info_t* out,
                                     uint32_t max_count);

/**
 * @brief Find archetype by name
 *
 * @param repr Representer
 * @param modality Art modality
 * @param name Archetype name
 * @param out Output info
 * @return 0 on success, -1 if not found
 */
int style_repr_find_archetype_by_name(const style_representer_t* repr,
                                       art_modality_t modality,
                                       const char* name,
                                       archetype_info_t* out);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Compute similarity between two embeddings
 *
 * @param a First embedding
 * @param b Second embedding
 * @return Similarity [0-1] (cosine similarity normalized)
 */
float style_repr_similarity(const style_embedding_t* a,
                            const style_embedding_t* b);

/**
 * @brief Compute distance between two embeddings
 *
 * @param a First embedding
 * @param b Second embedding
 * @return Distance (L2)
 */
float style_repr_distance(const style_embedding_t* a,
                          const style_embedding_t* b);

/**
 * @brief Interpolate between two styles
 *
 * @param a First style
 * @param b Second style
 * @param t Interpolation factor [0-1]
 * @param out Output interpolated style
 * @return 0 on success, -1 on error
 */
int style_repr_interpolate(const style_embedding_t* a,
                           const style_embedding_t* b,
                           float t,
                           style_embedding_t* out);

/**
 * @brief Add style vectors (for combining influences)
 *
 * @param a First style
 * @param b Second style
 * @param scale_a Scale factor for a
 * @param scale_b Scale factor for b
 * @param out Output combined style
 * @return 0 on success, -1 on error
 */
int style_repr_combine(const style_embedding_t* a,
                       const style_embedding_t* b,
                       float scale_a, float scale_b,
                       style_embedding_t* out);

/**
 * @brief Negate style (for "not like this" constraints)
 *
 * @param style Input style
 * @param out Output negated style
 * @return 0 on success, -1 on error
 */
int style_repr_negate(const style_embedding_t* style,
                      style_embedding_t* out);

/**
 * @brief Project style to nearest archetype
 *
 * @param repr Representer
 * @param style Input style
 * @param out Output projected style
 * @return Archetype ID of nearest archetype
 */
int32_t style_repr_project_to_archetype(const style_representer_t* repr,
                                         const style_embedding_t* style,
                                         style_embedding_t* out);

//=============================================================================
// Cortical Integration API
//=============================================================================

/**
 * @brief Set cortical columns for multi-scale style feature extraction
 *
 * @param repr Representer
 * @param cortical_columns Cortical columns pointer
 */
void style_representer_set_cortical_columns(style_representer_t* repr,
                                             void* cortical_columns);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Allocate style embedding
 *
 * @param dim Embedding dimension
 * @return Allocated embedding or NULL
 */
style_embedding_t* style_repr_alloc_embedding(uint32_t dim);

/**
 * @brief Clone style embedding
 *
 * @param src Source embedding
 * @return Cloned embedding or NULL
 */
style_embedding_t* style_repr_clone_embedding(const style_embedding_t* src);

/**
 * @brief Normalize embedding to unit length
 *
 * @param style Embedding to normalize (in-place)
 */
void style_repr_normalize(style_embedding_t* style);

/**
 * @brief Get embedding dimension
 *
 * @param repr Representer
 * @return Embedding dimension
 */
uint32_t style_repr_get_dim(const style_representer_t* repr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STYLE_REPRESENTATION_H */
