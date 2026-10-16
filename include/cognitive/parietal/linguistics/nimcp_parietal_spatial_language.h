/**
 * @file nimcp_parietal_spatial_language.h
 * @brief Spatial Language Processing Module (Angular Gyrus, BA39)
 * @version 1.0.0
 * @date 2025-01-31
 *
 * WHAT: Spatial preposition parsing, reference frame selection, and
 *       spatial-semantic mapping for the parietal linguistics system
 *
 * WHY:  The angular gyrus is critical for spatial language processing,
 *       including spatial prepositions, metaphors, and reference frames
 *
 * BIOLOGICAL BASIS:
 * - Angular Gyrus (BA39) lesions cause spatial language deficits
 * - Integrates spatial representations with linguistic symbols
 * - Supports embodied cognition for spatial metaphors
 *
 * FUZZY INTEGRATION:
 * Each spatial preposition maps to fuzzy membership functions:
 * - "near" → Gaussian MF centered at 0, σ=2m
 * - "very near" → μ²(x) (FUZZY_HEDGE_VERY concentration)
 * - "somewhat near" → √μ(x) (FUZZY_HEDGE_SOMEWHAT dilation)
 *
 * MESH ARCHITECTURE:
 * This module participates in the linguistics mesh via:
 * - `spatial_language_mesh_process()` - Produce belief with precision
 * - `spatial_language_mesh_update()` - FEP belief update
 * - `spatial_language_get_precision()` - Current confidence
 *
 * USAGE:
 * ```c
 * spatial_language_t* sl = spatial_language_create();
 *
 * // Parse a spatial preposition
 * spatial_semantics_t sem;
 * spatial_language_parse_preposition(sl, "near", &sem);
 *
 * // Apply hedge modifier
 * spatial_language_apply_hedge(sl, &sem, FUZZY_HEDGE_VERY);
 *
 * // Select reference frame based on context
 * reference_frame_t frame;
 * spatial_language_select_frame(sl, context, &frame);
 *
 * spatial_language_destroy(sl);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_SPATIAL_LANGUAGE_H
#define NIMCP_PARIETAL_SPATIAL_LANGUAGE_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Default distance MF sigma for "near" (meters) */
#define SPATIAL_LANG_DEFAULT_NEAR_SIGMA         2.0f

/** Default distance for "far" threshold (meters) */
#define SPATIAL_LANG_DEFAULT_FAR_THRESHOLD      10.0f

/** Default angle tolerance for directional prepositions (degrees) */
#define SPATIAL_LANG_DEFAULT_ANGLE_TOLERANCE    30.0f

/** Maximum number of custom preposition definitions */
#define SPATIAL_LANG_MAX_CUSTOM_PREPOSITIONS    32

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for spatial language processor */
typedef struct spatial_language spatial_language_t;

/**
 * @brief Spatial language configuration
 */
typedef struct {
    /* Fuzzy parameters */
    float near_sigma;               /**< Sigma for "near" Gaussian MF (default: 2.0m) */
    float far_threshold;            /**< Threshold distance for "far" (default: 10m) */
    float angle_tolerance;          /**< Angle tolerance in degrees (default: 30°) */

    /* Frame selection */
    float egocentric_prior;         /**< Prior for egocentric frame (default: 0.4) */
    float allocentric_prior;        /**< Prior for allocentric frame (default: 0.3) */
    float intrinsic_prior;          /**< Prior for intrinsic frame (default: 0.3) */

    /* Feature flags */
    bool enable_hedge_processing;   /**< Enable linguistic hedges (default: true) */
    bool enable_metaphor_grounding; /**< Enable spatial metaphor mapping (default: true) */
    bool enable_bio_async;          /**< Enable bio-async messaging (default: false) */
    bool enable_world_model;        /**< Enable world model integration (default: false) */

    /* Mesh configuration */
    bool enable_mesh_participation; /**< Participate in linguistics mesh (default: true) */

    /* Modulation */
    float inflammation_sensitivity; /**< Immune modulation factor (0-1) */
    float fatigue_sensitivity;      /**< Fatigue modulation factor (0-1) */
} spatial_language_config_t;

/**
 * @brief Preposition definition with fuzzy semantics
 */
typedef struct {
    spatial_preposition_t type;     /**< Preposition type */
    char word[32];                  /**< Word string */

    /* Fuzzy membership functions */
    fuzzy_mf_t distance_mf;         /**< Distance membership function */
    fuzzy_mf_t angle_mf;            /**< Angle membership function (if applicable) */

    /* Direction vector (for directional prepositions) */
    float direction[3];             /**< Default direction (x,y,z) */

    /* Semantic flags */
    bool requires_reference;        /**< Requires reference object */
    bool supports_path;             /**< Can describe trajectory */
    bool is_projective;             /**< Requires perspective (left/right) */
} preposition_definition_t;

/**
 * @brief Context for reference frame selection
 */
typedef struct {
    float speaker_position[3];      /**< Speaker location */
    float speaker_orientation[3];   /**< Speaker facing direction */
    float listener_position[3];     /**< Listener location (for ToM) */
    float listener_orientation[3];  /**< Listener facing direction */
    float reference_position[3];    /**< Reference object location */
    float reference_orientation[3]; /**< Reference object intrinsic axes */

    bool has_speaker;               /**< Speaker info available */
    bool has_listener;              /**< Listener info available */
    bool has_reference;             /**< Reference object available */
    bool reference_has_intrinsic;   /**< Reference has intrinsic axes */

    float perspective_alignment;    /**< Speaker-listener perspective alignment [0,1] */
} frame_context_t;

/**
 * @brief Spatial metaphor mapping
 *
 * Maps abstract concepts to spatial representations.
 * E.g., "Life is a journey" → path metaphor
 */
typedef struct {
    char source_domain[64];         /**< Abstract domain (e.g., "life") */
    char target_domain[64];         /**< Spatial domain (e.g., "journey") */

    spatial_preposition_t base_prep; /**< Base spatial preposition */
    float mapping_strength;         /**< Strength of metaphorical mapping [0,1] */

    /* Semantic features */
    bool involves_motion;           /**< Motion metaphor */
    bool involves_containment;      /**< Container metaphor */
    bool involves_path;             /**< Path/journey metaphor */
} spatial_metaphor_t;

/**
 * @brief Spatial language statistics
 */
typedef struct {
    uint64_t prepositions_parsed;   /**< Total prepositions parsed */
    uint64_t frames_selected;       /**< Frame selection operations */
    uint64_t hedges_applied;        /**< Hedge applications */
    uint64_t metaphors_grounded;    /**< Metaphor groundings */
    uint64_t unknown_words;         /**< Unknown word encounters */

    float avg_confidence;           /**< Average parse confidence */
    float avg_processing_time_us;   /**< Average processing time */

    /* Frame selection distribution */
    uint64_t egocentric_selections;
    uint64_t allocentric_selections;
    uint64_t intrinsic_selections;
} spatial_language_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create spatial language processor with default configuration
 *
 * @return Handle or NULL on error
 */
spatial_language_t* spatial_language_create(void);

/**
 * @brief Create spatial language processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
spatial_language_t* spatial_language_create_custom(const spatial_language_config_t* config);

/**
 * @brief Destroy spatial language processor
 *
 * @param sl Handle (NULL safe)
 */
void spatial_language_destroy(spatial_language_t* sl);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
spatial_language_config_t spatial_language_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool spatial_language_validate_config(const spatial_language_config_t* config);

/* ============================================================================
 * PARSING API
 * ============================================================================ */

/**
 * @brief Parse a spatial preposition
 *
 * Converts a spatial word to its semantic representation with
 * fuzzy membership functions and default reference frame.
 *
 * @param sl Spatial language handle
 * @param word Preposition word (e.g., "near", "above", "between")
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int spatial_language_parse_preposition(
    spatial_language_t* sl,
    const char* word,
    spatial_semantics_t* out
);

/**
 * @brief Parse preposition with explicit reference frame
 *
 * @param sl Spatial language handle
 * @param word Preposition word
 * @param frame Reference frame to use
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int spatial_language_parse_with_frame(
    spatial_language_t* sl,
    const char* word,
    reference_frame_t frame,
    spatial_semantics_t* out
);

/**
 * @brief Check if word is a known spatial preposition
 *
 * @param sl Spatial language handle
 * @param word Word to check
 * @return true if known preposition
 */
bool spatial_language_is_preposition(
    const spatial_language_t* sl,
    const char* word
);

/**
 * @brief Get preposition type from word
 *
 * @param sl Spatial language handle
 * @param word Preposition word
 * @param out Output preposition type
 * @return 0 on success, LING_ERR_UNKNOWN_PREPOSITION if not found
 */
int spatial_language_get_preposition_type(
    const spatial_language_t* sl,
    const char* word,
    spatial_preposition_t* out
);

/**
 * @brief Get word for preposition type
 *
 * @param sl Spatial language handle
 * @param type Preposition type
 * @param word Output word buffer
 * @param max_len Maximum buffer length
 * @return 0 on success
 */
int spatial_language_get_preposition_word(
    const spatial_language_t* sl,
    spatial_preposition_t type,
    char* word,
    uint32_t max_len
);

/* ============================================================================
 * REFERENCE FRAME API
 * ============================================================================ */

/**
 * @brief Select optimal reference frame based on context
 *
 * Uses Bayesian inference to select the most appropriate frame
 * based on speaker/listener positions and reference object properties.
 *
 * @param sl Spatial language handle
 * @param context Frame selection context
 * @param out Output selected frame
 * @return 0 on success
 */
int spatial_language_select_frame(
    spatial_language_t* sl,
    const frame_context_t* context,
    reference_frame_t* out
);

/**
 * @brief Get frame selection probabilities
 *
 * Returns Bayesian posteriors for all frame types given context.
 *
 * @param sl Spatial language handle
 * @param context Frame selection context
 * @param probabilities Output array of size REF_FRAME_COUNT
 * @return 0 on success
 */
int spatial_language_get_frame_probabilities(
    spatial_language_t* sl,
    const frame_context_t* context,
    float* probabilities
);

/**
 * @brief Transform semantics between reference frames
 *
 * Converts spatial semantics from one frame to another.
 *
 * @param sl Spatial language handle
 * @param semantics Input semantics (modified in place)
 * @param from_frame Source frame
 * @param to_frame Target frame
 * @param context Transform context
 * @return 0 on success
 */
int spatial_language_transform_frame(
    spatial_language_t* sl,
    spatial_semantics_t* semantics,
    reference_frame_t from_frame,
    reference_frame_t to_frame,
    const frame_context_t* context
);

/* ============================================================================
 * FUZZY SEMANTICS API
 * ============================================================================ */

/**
 * @brief Evaluate spatial semantics at given distance/angle
 *
 * Computes membership degree for the spatial relation at a specific
 * distance and angle from reference.
 *
 * @param sl Spatial language handle
 * @param semantics Spatial semantics to evaluate
 * @param distance Distance from reference (meters)
 * @param angle Angle from reference axis (radians)
 * @return Combined membership degree [0,1]
 */
float spatial_language_evaluate_membership(
    const spatial_language_t* sl,
    const spatial_semantics_t* semantics,
    float distance,
    float angle
);

/**
 * @brief Apply linguistic hedge to spatial semantics
 *
 * Modifies the membership function according to the hedge:
 * - VERY: μ²(x) (concentration)
 * - SOMEWHAT: √μ(x) (dilation)
 * - EXTREMELY: μ³(x)
 * - etc.
 *
 * @param sl Spatial language handle
 * @param semantics Semantics to modify (in place)
 * @param hedge Hedge to apply (from fuzzy_hedge_t)
 * @return 0 on success
 */
int spatial_language_apply_hedge(
    spatial_language_t* sl,
    spatial_semantics_t* semantics,
    uint8_t hedge
);

/**
 * @brief Get distance membership function for preposition
 *
 * @param sl Spatial language handle
 * @param prep Preposition type
 * @param out Output membership function
 * @return 0 on success
 */
int spatial_language_get_distance_mf(
    const spatial_language_t* sl,
    spatial_preposition_t prep,
    fuzzy_mf_t* out
);

/**
 * @brief Get angle membership function for preposition
 *
 * @param sl Spatial language handle
 * @param prep Preposition type
 * @param out Output membership function
 * @return 0 on success
 */
int spatial_language_get_angle_mf(
    const spatial_language_t* sl,
    spatial_preposition_t prep,
    fuzzy_mf_t* out
);

/* ============================================================================
 * METAPHOR API
 * ============================================================================ */

/**
 * @brief Ground an abstract metaphor to spatial representation
 *
 * Maps abstract language to spatial semantics.
 * E.g., "up" in "prices are going up" → vertical increase
 *
 * @param sl Spatial language handle
 * @param abstract_phrase Abstract phrase
 * @param out Output spatial semantics
 * @return 0 on success, error code if no mapping found
 */
int spatial_language_ground_metaphor(
    spatial_language_t* sl,
    const char* abstract_phrase,
    spatial_semantics_t* out
);

/**
 * @brief Register custom spatial metaphor mapping
 *
 * @param sl Spatial language handle
 * @param metaphor Metaphor definition
 * @return 0 on success
 */
int spatial_language_register_metaphor(
    spatial_language_t* sl,
    const spatial_metaphor_t* metaphor
);

/* ============================================================================
 * MESH INTEGRATION API
 * ============================================================================ */

/**
 * @brief Process mesh request and produce belief
 *
 * Implements linguistics_mesh_handler_t::process for mesh participation.
 *
 * @param sl Spatial language handle
 * @param request Mesh request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int spatial_language_mesh_process(
    spatial_language_t* sl,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Update belief based on neighbor beliefs
 *
 * Implements FEP update: μ' = μ - lr * Π * ε
 *
 * @param sl Spatial language handle
 * @param neighbor_beliefs Beliefs from mesh neighbors
 * @param neighbor_count Number of neighbor beliefs
 * @param updated_belief Output updated belief
 * @return 0 on success
 */
int spatial_language_mesh_update(
    spatial_language_t* sl,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
);

/**
 * @brief Get current precision (inverse prediction error variance)
 *
 * @param sl Spatial language handle
 * @return Precision Π ∈ [PRECISION_FLOOR, PRECISION_CEILING]
 */
float spatial_language_get_precision(const spatial_language_t* sl);

/**
 * @brief Get mesh handler interface
 *
 * Returns handler struct for registering with mesh coordinator.
 *
 * @param sl Spatial language handle
 * @param handler Output handler struct
 * @return 0 on success
 */
int spatial_language_get_mesh_handler(
    spatial_language_t* sl,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * WORLD MODEL INTEGRATION API
 * ============================================================================ */

/**
 * @brief Predict spatial outcome from instruction
 *
 * Uses world model to predict new spatial state after instruction.
 * E.g., "go left" → predict new position
 *
 * @param sl Spatial language handle
 * @param instruction Spatial instruction
 * @param current_position Current position [x,y,z]
 * @param predicted_position Output predicted position [x,y,z]
 * @return 0 on success
 */
int spatial_language_predict_outcome(
    spatial_language_t* sl,
    const char* instruction,
    const float current_position[3],
    float predicted_position[3]
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level for immune modulation
 *
 * High inflammation can degrade spatial language precision.
 *
 * @param sl Spatial language handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int spatial_language_set_inflammation(
    spatial_language_t* sl,
    float level
);

/**
 * @brief Set fatigue level
 *
 * Fatigue can affect reference frame selection accuracy.
 *
 * @param sl Spatial language handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int spatial_language_set_fatigue(
    spatial_language_t* sl,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param sl Spatial language handle
 * @param stats Output statistics
 * @return 0 on success
 */
int spatial_language_get_stats(
    const spatial_language_t* sl,
    spatial_language_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param sl Spatial language handle
 */
void spatial_language_reset_stats(spatial_language_t* sl);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* spatial_language_get_last_error(void);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Get human-readable name for preposition type
 *
 * @param prep Preposition type
 * @return Static string name
 */
const char* spatial_language_preposition_name(spatial_preposition_t prep);

/**
 * @brief Get human-readable name for reference frame
 *
 * @param frame Reference frame type
 * @return Static string name
 */
const char* spatial_language_frame_name(reference_frame_t frame);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_SPATIAL_LANGUAGE_H */
