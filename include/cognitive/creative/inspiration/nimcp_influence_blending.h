//=============================================================================
// nimcp_influence_blending.h - Multi-Influence Blending System
//=============================================================================
/**
 * @file nimcp_influence_blending.h
 * @brief Blends multiple creative influences into coherent new styles
 *
 * WHAT: Combines multiple style influences with weighted contributions
 * WHY:  Enable creative synthesis from diverse inspirations
 * HOW:  Weighted embedding interpolation with coherence optimization
 *
 * DESIGN PATTERN:
 * Follows fin_blend_result_t pattern from financial module.
 * Supports positive influences ("like X") and negative influences ("not like Y").
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_INFLUENCE_BLENDING_H
#define NIMCP_INFLUENCE_BLENDING_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Blending Modes
//=============================================================================

/**
 * @brief Blending algorithm modes
 */
typedef enum {
    BLEND_MODE_LINEAR = 0,         /**< Linear weighted average */
    BLEND_MODE_SPHERICAL,          /**< Spherical interpolation (slerp) */
    BLEND_MODE_ADAPTIVE,           /**< Adaptive based on style compatibility */
    BLEND_MODE_HIERARCHICAL,       /**< Hierarchical blending by category */
    BLEND_MODE_ADVERSARIAL         /**< GAN-style adversarial blending */
} blend_mode_t;

/**
 * @brief Blending constraint types
 */
typedef enum {
    BLEND_CONSTRAINT_NONE = 0,     /**< No constraints */
    BLEND_CONSTRAINT_COHERENCE,    /**< Enforce coherence threshold */
    BLEND_CONSTRAINT_ORIGINALITY,  /**< Enforce minimum originality */
    BLEND_CONSTRAINT_MODALITY,     /**< Keep within modality bounds */
    BLEND_CONSTRAINT_ALL           /**< Apply all constraints */
} blend_constraint_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Influence blender configuration
 */
typedef struct {
    /* Blending settings */
    blend_mode_t default_mode;      /**< Default blending mode */
    blend_constraint_t constraints; /**< Active constraints */
    float coherence_threshold;      /**< Min coherence for result */
    float originality_threshold;    /**< Min originality for result */

    /* Optimization settings */
    bool optimize_weights;          /**< Auto-optimize influence weights */
    uint32_t optimization_iterations; /**< Iterations for optimization */
    float learning_rate;            /**< Weight optimization learning rate */

    /* Limits */
    uint32_t max_influences;        /**< Maximum influences per blend */
    float min_influence_weight;     /**< Minimum meaningful weight */
} influence_blender_config_t;

/**
 * @brief Initialize config with defaults
 */
void influence_blender_config_defaults(influence_blender_config_t* config);

//=============================================================================
// Blend Analysis Types
//=============================================================================

/**
 * @brief Compatibility analysis between two influences
 */
typedef struct {
    int32_t influence_a_idx;        /**< First influence index */
    int32_t influence_b_idx;        /**< Second influence index */
    float compatibility;            /**< [0-1] How compatible they are */
    float tension;                  /**< [0-1] Creative tension between them */
    char compatibility_reason[128]; /**< Explanation */
} influence_compatibility_t;

/**
 * @brief Full blend analysis
 */
typedef struct {
    /* Compatibility matrix */
    influence_compatibility_t* compatibilities;
    uint32_t num_compatibilities;

    /* Overall metrics */
    float avg_compatibility;        /**< Average pairwise compatibility */
    float max_tension;              /**< Maximum tension pair */
    bool is_feasible;               /**< Can these be coherently blended? */

    /* Suggestions */
    float* suggested_weights;       /**< Suggested optimal weights */
    uint32_t num_suggestions;
    char suggestion_rationale[256]; /**< Explanation of suggestions */
} blend_analysis_t;

//=============================================================================
// Blender Structure
//=============================================================================

/**
 * @brief Influence blending module
 */
struct influence_blender {
    influence_blender_config_t config;

    /* Style representer for operations */
    style_representer_t* style_repr;

    /* Compatibility model */
    void* compatibility_model;

    /* Current blend state */
    creative_influence_t* current_influences;
    uint32_t num_current_influences;
    uint32_t influences_capacity;

    /* Statistics */
    uint64_t blends_performed;
    float avg_blend_coherence;
    float avg_blend_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create influence blender
 *
 * @param config Configuration (NULL for defaults)
 * @param style_repr Style representer for embedding operations
 * @return Blender or NULL on error
 */
influence_blender_t* influence_blender_create(
    const influence_blender_config_t* config,
    style_representer_t* style_repr);

/**
 * @brief Destroy influence blender
 *
 * @param blender Blender to destroy
 */
void influence_blender_destroy(influence_blender_t* blender);

//=============================================================================
// Influence Management API
//=============================================================================

/**
 * @brief Clear all current influences
 *
 * @param blender Blender
 */
void influence_blender_clear(influence_blender_t* blender);

/**
 * @brief Add positive influence (style to include)
 *
 * @param blender Blender
 * @param style Style embedding
 * @param weight Influence weight [0-1]
 * @param source_work Source work name (optional)
 * @param source_artist Artist name (optional)
 * @return 0 on success, -1 on error
 */
int influence_blender_add(influence_blender_t* blender,
                          const style_embedding_t* style,
                          float weight,
                          const char* source_work,
                          const char* source_artist);

/**
 * @brief Add negative influence (style to avoid)
 *
 * @param blender Blender
 * @param style Style embedding to avoid
 * @param weight Avoidance weight [0-1]
 * @param source_work Source work name (optional)
 * @return 0 on success, -1 on error
 */
int influence_blender_add_negative(influence_blender_t* blender,
                                    const style_embedding_t* style,
                                    float weight,
                                    const char* source_work);

/**
 * @brief Add influence from archetype
 *
 * @param blender Blender
 * @param modality Art modality
 * @param archetype_id Archetype ID
 * @param weight Influence weight
 * @param is_positive true for positive, false for negative
 * @return 0 on success, -1 on error
 */
int influence_blender_add_archetype(influence_blender_t* blender,
                                     art_modality_t modality,
                                     int32_t archetype_id,
                                     float weight,
                                     bool is_positive);

/**
 * @brief Remove influence by index
 *
 * @param blender Blender
 * @param index Influence index
 * @return 0 on success, -1 on error
 */
int influence_blender_remove(influence_blender_t* blender, uint32_t index);

/**
 * @brief Update influence weight
 *
 * @param blender Blender
 * @param index Influence index
 * @param new_weight New weight
 * @return 0 on success, -1 on error
 */
int influence_blender_set_weight(influence_blender_t* blender,
                                  uint32_t index, float new_weight);

//=============================================================================
// Blending API
//=============================================================================

/**
 * @brief Blend current influences
 *
 * @param blender Blender
 * @param mode Blending mode (or -1 for default)
 * @param result Output blend result
 * @return 0 on success, -1 on error
 */
int influence_blender_blend(influence_blender_t* blender,
                            blend_mode_t mode,
                            influence_blend_result_t* result);

/**
 * @brief Blend with explicit influences (one-shot)
 *
 * @param blender Blender
 * @param influences Array of influences
 * @param num_influences Number of influences
 * @param mode Blending mode
 * @param result Output blend result
 * @return 0 on success, -1 on error
 */
int influence_blender_blend_explicit(influence_blender_t* blender,
                                      const creative_influence_t* influences,
                                      uint32_t num_influences,
                                      blend_mode_t mode,
                                      influence_blend_result_t* result);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Analyze influence compatibility before blending
 *
 * @param blender Blender
 * @param analysis Output analysis
 * @return 0 on success, -1 on error
 */
int influence_blender_analyze(influence_blender_t* blender,
                               blend_analysis_t* analysis);

/**
 * @brief Get compatibility between two specific styles
 *
 * @param blender Blender
 * @param style_a First style
 * @param style_b Second style
 * @return Compatibility [0-1]
 */
float influence_blender_compatibility(const influence_blender_t* blender,
                                       const style_embedding_t* style_a,
                                       const style_embedding_t* style_b);

/**
 * @brief Optimize weights for current influences
 *
 * Adjusts weights to maximize coherence while respecting originality.
 *
 * @param blender Blender
 * @return 0 on success, -1 on error
 */
int influence_blender_optimize_weights(influence_blender_t* blender);

//=============================================================================
// Preset Blends API
//=============================================================================

/**
 * @brief Create "homage" blend (heavily weighted to one influence)
 *
 * @param blender Blender
 * @param primary Primary style (80% weight)
 * @param accent Accent style (20% weight)
 * @param result Output blend result
 * @return 0 on success, -1 on error
 */
int influence_blender_homage(influence_blender_t* blender,
                              const style_embedding_t* primary,
                              const style_embedding_t* accent,
                              influence_blend_result_t* result);

/**
 * @brief Create "fusion" blend (equal weights)
 *
 * @param blender Blender
 * @param styles Array of styles
 * @param num_styles Number of styles
 * @param result Output blend result
 * @return 0 on success, -1 on error
 */
int influence_blender_fusion(influence_blender_t* blender,
                              const style_embedding_t* styles,
                              uint32_t num_styles,
                              influence_blend_result_t* result);

/**
 * @brief Create "contrast" blend (styles with positive/negative pairing)
 *
 * @param blender Blender
 * @param toward Style to approach
 * @param away Style to avoid
 * @param result Output blend result
 * @return 0 on success, -1 on error
 */
int influence_blender_contrast(influence_blender_t* blender,
                                const style_embedding_t* toward,
                                const style_embedding_t* away,
                                influence_blend_result_t* result);

//=============================================================================
// Iteration API
//=============================================================================

/**
 * @brief Refine blend iteratively
 *
 * Takes a blend result and refines it based on feedback.
 *
 * @param blender Blender
 * @param current Current blend result
 * @param coherence_target Target coherence
 * @param iterations Max iterations
 * @param result Output refined result
 * @return 0 on success, -1 on error
 */
int influence_blender_refine(influence_blender_t* blender,
                              const influence_blend_result_t* current,
                              float coherence_target,
                              uint32_t iterations,
                              influence_blend_result_t* result);

/**
 * @brief Generate variations of a blend
 *
 * @param blender Blender
 * @param base Base blend result
 * @param num_variations Number of variations to generate
 * @param variation_strength How different from base [0-1]
 * @param results Output array of results (caller allocated)
 * @return Number of variations generated
 */
uint32_t influence_blender_variations(influence_blender_t* blender,
                                       const influence_blend_result_t* base,
                                       uint32_t num_variations,
                                       float variation_strength,
                                       influence_blend_result_t* results);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free blend analysis
 *
 * @param analysis Analysis to free
 */
void blend_analysis_free(blend_analysis_t* analysis);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFLUENCE_BLENDING_H */
