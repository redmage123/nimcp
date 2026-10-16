/**
 * @file nimcp_cortical_surround.h
 * @brief Surround suppression and contextual modulation for cortical columns
 *
 * WHAT: Implementation of center-surround interactions in visual cortex that
 *       enable contour integration, figure-ground segregation, and context-dependent
 *       response modulation through iso-orientation suppression and collinear facilitation.
 * WHY:  V1 neurons exhibit strong contextual effects where surround stimuli modulate
 *       center responses. Iso-orientation suppression sharpens tuning, while collinear
 *       facilitation enables contour detection and perceptual grouping.
 * HOW:  Models classical receptive field (CRF) and extra-classical receptive field (eCRF)
 *       interactions using Gaussian spatial profiles and orientation-dependent kernels.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * V1 surround modulation exhibits several key properties:
 *
 * 1. ISO-ORIENTATION SUPPRESSION (Cavanaugh et al., 2002):
 *    - Surround stimuli with similar orientation suppress center response
 *    - Suppression strength: S(θ) = S_max × exp(-|θ_center - θ_surround|² / 2σ²)
 *    - Strongest suppression at matched orientations (θ_diff ≈ 0°)
 *    - Suppression falls off with orientation difference
 *    - Function: Sharpens orientation tuning, reduces redundancy
 *
 * 2. CROSS-ORIENTATION FACILITATION (Sillito et al., 1995):
 *    - Orthogonal/different orientations can facilitate responses
 *    - Weaker effect than iso-orientation suppression
 *    - Function: Enhances edge junctions, texture boundaries
 *
 * 3. COLLINEAR FACILITATION (Series et al., 2003):
 *    - Co-aligned edges along contours facilitate each other
 *    - Facilitation: F = F_max × cos²(θ_alignment)
 *    - Strongest for perfectly aligned edges
 *    - Distance-dependent: Falls off with separation
 *    - Function: Contour integration, perceptual grouping
 *
 * 4. FIGURE-GROUND SEGREGATION (Lamme, 1995):
 *    - Texture discontinuities generate differential responses
 *    - Border ownership signals emerge from surround context
 *    - Function: Segregates objects from backgrounds
 *
 * MATHEMATICAL MODELS:
 * =================================================================================
 * Classical Receptive Field (CRF):
 *   CRF(r) = exp(-r² / (2σ_center²))
 *   Where r is distance from center, σ_center is CRF radius
 *
 * Extra-Classical Receptive Field (eCRF):
 *   eCRF(r) = exp(-r² / (2σ_surround²)) - CRF(r)
 *   Where σ_surround > σ_center (typically 3-5x)
 *
 * Iso-Orientation Suppression:
 *   S(θ_diff) = S_max × exp(-θ_diff² / (2σ_θ²))
 *   Where θ_diff = |θ_center - θ_surround|
 *   σ_θ is orientation tuning width (typically 20-30°)
 *
 * Collinear Facilitation:
 *   F(θ_align) = F_max × cos²(θ_align)
 *   Where θ_align is angle between edge and connecting line
 *
 * Total Modulation:
 *   R_modulated = R_center × (1 - Suppression + Facilitation)
 *   Bounded to [0, R_max]
 *
 * REFERENCES:
 * - Cavanaugh et al. (2002) Nature: "Selectivity and spatial distribution of
 *   signals from the receptive field surround in macaque V1 neurons"
 * - Series et al. (2003) J Neurosci: "Contrast-dependent competition in
 *   recurrent networks of orientation-selective neurons"
 * - Sillito et al. (1995) Nature: "Visual cortical mechanisms detecting focal
 *   orientation discontinuities"
 * - Lamme (1995) J Neurosci: "The neurophysiology of figure-ground segregation
 *   in primary visual cortex"
 *
 * @version 1.0.0
 * @date 2025-12-15
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_SURROUND_H
#define NIMCP_CORTICAL_SURROUND_H

#include <stdint.h>
#include <stdbool.h>
#include "constants/nimcp_math_constants.h"
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct orientation_hypercolumn_t;

/* ============================================================================
 * Constants
 * ========================================================================== */

/** Default classical receptive field radius (in pixels) */
#define SURROUND_DEFAULT_CENTER_RADIUS 8.0f

/** Default surround radius (typically 3-5x center) */
#define SURROUND_DEFAULT_SURROUND_RADIUS 32.0f

/** Default iso-orientation suppression strength */
#define SURROUND_DEFAULT_ISO_SUPPRESSION 0.7f

/** Default cross-orientation facilitation strength */
#define SURROUND_DEFAULT_CROSS_FACILITATION 0.15f

/** Default collinear facilitation strength */
#define SURROUND_DEFAULT_COLLINEAR_FACILITATION 0.3f

/** Default orientation bandwidth (tuning width in degrees) */
#define SURROUND_DEFAULT_ORIENTATION_BANDWIDTH 25.0f

/** Maximum field size (pixels) */
#define SURROUND_MAX_FIELD_SIZE 256

/** Pi constant */

/* ============================================================================
 * Data Structures
 * ========================================================================== */

/**
 * @enum surround_type_t
 * @brief Types of surround modulation
 *
 * WHAT: Categories of contextual modulation effects.
 * WHY:  Different surround configurations produce different effects.
 * HOW:  Enum distinguishes between suppression and facilitation mechanisms.
 */
typedef enum {
    SURROUND_ISO_ORIENTATION,      /**< Same orientation suppresses center */
    SURROUND_CROSS_ORIENTATION,    /**< Different orientation facilitates */
    SURROUND_COLLINEAR,            /**< Co-aligned edges facilitate (contours) */
    SURROUND_ORTHOGONAL            /**< Perpendicular edges (junctions) */
} surround_type_t;

/**
 * @struct surround_config_t
 * @brief Configuration for surround suppression module
 *
 * WHAT: Parameters controlling center-surround interactions.
 * WHY:  Allows customization of contextual modulation strength and spatial extent.
 * HOW:  Stores radii, strengths, and feature flags for surround effects.
 */
typedef struct {
    float center_radius;              /**< Classical RF radius (pixels) */
    float surround_radius;            /**< Surround extent (pixels) */
    float iso_suppression_strength;   /**< Max iso-orientation suppression (0-1) */
    float cross_facilitation_strength;/**< Max cross-orientation facilitation (0-1) */
    float collinear_facilitation;     /**< Max collinear facilitation (0-1) */
    float orientation_bandwidth;      /**< Tuning width for suppression (degrees) */
    bool enable_figure_ground;        /**< Enable figure-ground detection */
    uint32_t field_size;              /**< Spatial field size (pixels, must be odd) */
} surround_config_t;

/**
 * @struct surround_field_t
 * @brief Spatial profile for surround modulation
 *
 * WHAT: Pre-computed spatial weights for center and surround regions.
 * WHY:  Efficient application of surround effects without recomputing Gaussians.
 * HOW:  Stores suppression and facilitation weights as 2D arrays (flattened).
 */
typedef struct {
    float* suppression_weights;    /**< Spatial suppression profile (field_size²) */
    float* facilitation_weights;   /**< Spatial facilitation profile (field_size²) */
    uint32_t field_size;           /**< Size of weight arrays (width = height) */
} surround_field_t;

/**
 * @struct cortical_surround_t
 * @brief Main surround suppression module
 *
 * WHAT: Complete surround modulation system for cortical columns.
 * WHY:  Integrates iso-suppression, cross-facilitation, and collinear facilitation.
 * HOW:  Maintains configuration, spatial fields, and integration with orientation columns.
 */
typedef struct {
    surround_config_t config;         /**< Module configuration */
    surround_field_t field;           /**< Spatial weight profiles */

    /* Orientation column integration */
    struct orientation_hypercolumn_t** orientation_columns; /**< Connected hypercolumns */
    uint32_t num_hypercolumns;        /**< Number of connected hypercolumns */

    /* Modulation state */
    float* center_responses;          /**< Current center responses */
    float* surround_responses;        /**< Current surround responses */
    float* modulated_responses;       /**< Output responses after modulation */
    uint32_t response_size;           /**< Size of response arrays */

    /* Figure-ground signals */
    float* border_ownership;          /**< Border ownership signals (if enabled) */
    float* texture_contrast;          /**< Texture contrast signals (if enabled) */

    /* Statistics */
    uint64_t total_updates;           /**< Total modulation operations */
    float mean_suppression;           /**< Average suppression applied */
    float mean_facilitation;          /**< Average facilitation applied */

    /* Bio-async integration */
    void* bio_ctx;                    /**< Bio-async module context */
    bool bio_async_enabled;           /**< Bio-async active flag */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;    /**< Thread safety mutex */
} cortical_surround_t;

/**
 * @struct surround_stats_t
 * @brief Statistics for surround modulation module
 *
 * WHAT: Diagnostic metrics for surround effects.
 * WHY:  Monitor modulation strength and effectiveness.
 * HOW:  Aggregates suppression/facilitation statistics over time.
 */
typedef struct {
    float mean_iso_suppression;       /**< Average iso-orientation suppression */
    float mean_cross_facilitation;    /**< Average cross-orientation facilitation */
    float mean_collinear_facilitation;/**< Average collinear facilitation */
    float max_suppression_observed;   /**< Peak suppression value */
    float max_facilitation_observed;  /**< Peak facilitation value */
    uint64_t total_updates;           /**< Number of update cycles */
    float figure_ground_strength;     /**< Figure-ground segregation strength */
} surround_stats_t;

/* ============================================================================
 * Lifecycle Functions
 * ========================================================================== */

/**
 * @brief Create surround suppression module
 *
 * WHAT: Allocates and initializes surround modulation system.
 * WHY:  Establish center-surround interactions for contextual modulation.
 * HOW:  Allocates memory, computes spatial fields, initializes configuration.
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Pointer to created module, NULL on failure
 *
 * @note Caller must free with cortical_surround_destroy()
 * @note Thread-safe after creation
 */
cortical_surround_t* cortical_surround_create(const surround_config_t* config);

/**
 * @brief Destroy surround suppression module
 *
 * WHAT: Frees all resources associated with surround module.
 * WHY:  Prevents memory leaks and releases locks.
 * HOW:  Destroys mutex, frees arrays, releases bio-async context.
 *
 * @param surround Module to destroy (may be NULL)
 */
void cortical_surround_destroy(cortical_surround_t* surround);

/**
 * @brief Get default configuration
 *
 * WHAT: Populates config structure with biologically-plausible defaults.
 * WHY:  Simplifies module creation with standard parameters.
 * HOW:  Sets defaults based on neuroscience literature values.
 *
 * @param config Output configuration structure
 * @return true on success, false on failure
 */
bool cortical_surround_default_config(surround_config_t* config);

/* ============================================================================
 * Integration Functions
 * ========================================================================== */

/**
 * @brief Connect to orientation hypercolumns
 *
 * WHAT: Links surround module to orientation column system.
 * WHY:  Surround effects depend on orientation similarity.
 * HOW:  Stores pointer to hypercolumn for orientation queries.
 *
 * @param surround Surround module
 * @param hypercolumns Array of orientation hypercolumns
 * @param num_hypercolumns Number of hypercolumns
 * @return true on success, false on failure
 */
bool cortical_surround_connect_orientation_columns(
    cortical_surround_t* surround,
    struct orientation_hypercolumn_t** hypercolumns,
    uint32_t num_hypercolumns
);

/* ============================================================================
 * Surround Computation Functions
 * ========================================================================== */

/**
 * @brief Compute iso-orientation suppression
 *
 * WHAT: Calculates suppression from surround with similar orientation.
 * WHY:  Models iso-orientation surround suppression in V1.
 * HOW:  S(θ) = S_max × exp(-|θ_center - θ_surround|² / 2σ²)
 *
 * @param surround Surround module
 * @param center_orientation Center orientation in degrees
 * @param surround_orientation Surround orientation in degrees
 * @param center_response Center response amplitude
 * @param surround_response Surround response amplitude
 * @return Suppression amount (0-1)
 */
float cortical_surround_compute_iso_suppression(
    const cortical_surround_t* surround,
    float center_orientation,
    float surround_orientation,
    float center_response,
    float surround_response
);

/**
 * @brief Compute cross-orientation facilitation
 *
 * WHAT: Calculates facilitation from different orientations.
 * WHY:  Models cross-orientation facilitation at edge junctions.
 * HOW:  F = F_max × (1 - exp(-|θ_diff - 90|² / 2σ²))
 *
 * @param surround Surround module
 * @param center_orientation Center orientation in degrees
 * @param surround_orientation Surround orientation in degrees
 * @param center_response Center response amplitude
 * @param surround_response Surround response amplitude
 * @return Facilitation amount (0-1)
 */
float cortical_surround_compute_cross_facilitation(
    const cortical_surround_t* surround,
    float center_orientation,
    float surround_orientation,
    float center_response,
    float surround_response
);

/**
 * @brief Apply collinear facilitation
 *
 * WHAT: Computes facilitation for co-aligned edges (contour integration).
 * WHY:  Models association field facilitating contour detection.
 * HOW:  F = F_max × cos²(θ_alignment) where θ_alignment is angle between
 *       edge orientation and connecting line.
 *
 * @param surround Surround module
 * @param center_x Center position X
 * @param center_y Center position Y
 * @param center_orientation Center orientation in degrees
 * @param neighbor_x Neighbor position X
 * @param neighbor_y Neighbor position Y
 * @param neighbor_orientation Neighbor orientation in degrees
 * @param neighbor_response Neighbor response amplitude
 * @return Facilitation amount (0-1)
 */
float cortical_surround_apply_collinear_facilitation(
    const cortical_surround_t* surround,
    float center_x,
    float center_y,
    float center_orientation,
    float neighbor_x,
    float neighbor_y,
    float neighbor_orientation,
    float neighbor_response
);

/**
 * @brief Detect figure-ground segregation
 *
 * WHAT: Computes border ownership and texture contrast signals.
 * WHY:  Surround context indicates figure-ground relationships.
 * HOW:  Analyzes orientation/texture discontinuities in surround.
 *
 * @param surround Surround module
 * @param center_x Center position X
 * @param center_y Center position Y
 * @param orientation_map Orientation map (width × height floats)
 * @param response_map Response amplitude map (width × height floats)
 * @param width Map width
 * @param height Map height
 * @param border_ownership Output border ownership (-1 to 1)
 * @param texture_contrast Output texture contrast (0 to 1)
 * @return true on success, false on failure
 */
bool cortical_surround_detect_figure_ground(
    cortical_surround_t* surround,
    uint32_t center_x,
    uint32_t center_y,
    const float* orientation_map,
    const float* response_map,
    uint32_t width,
    uint32_t height,
    float* border_ownership,
    float* texture_contrast
);

/**
 * @brief Modulate response with all surround effects
 *
 * WHAT: Applies complete surround modulation to center response.
 * WHY:  Integrates iso-suppression, cross-facilitation, and collinear facilitation.
 * HOW:  R_out = R_center × (1 - suppression + facilitation), clamped to [0, max].
 *
 * @param surround Surround module
 * @param center_x Center position X
 * @param center_y Center position Y
 * @param orientation_map Orientation map (width × height floats)
 * @param response_map Response amplitude map (width × height floats)
 * @param width Map width
 * @param height Map height
 * @return Modulated response value
 */
float cortical_surround_modulate_response(
    cortical_surround_t* surround,
    uint32_t center_x,
    uint32_t center_y,
    const float* orientation_map,
    const float* response_map,
    uint32_t width,
    uint32_t height
);

/**
 * @brief Batch modulate responses across map
 *
 * WHAT: Applies surround modulation to entire orientation/response map.
 * WHY:  Efficiently processes full visual field.
 * HOW:  Iterates over all locations, applies cortical_surround_modulate_response.
 *
 * @param surround Surround module
 * @param orientation_map Input orientation map (width × height)
 * @param response_map Input response map (width × height)
 * @param output_map Output modulated responses (width × height, pre-allocated)
 * @param width Map width
 * @param height Map height
 * @return true on success, false on failure
 */
bool cortical_surround_batch_modulate(
    cortical_surround_t* surround,
    const float* orientation_map,
    const float* response_map,
    float* output_map,
    uint32_t width,
    uint32_t height
);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Get module statistics
 *
 * WHAT: Retrieves diagnostic metrics for surround module.
 * WHY:  Monitor modulation effects and debug issues.
 * HOW:  Populates stats structure with accumulated metrics.
 *
 * @param surround Surround module
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool cortical_surround_get_stats(
    const cortical_surround_t* surround,
    surround_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clears accumulated statistics.
 * WHY:  Start fresh monitoring period.
 * HOW:  Zeros all stat counters.
 *
 * @param surround Surround module
 * @return true on success, false on failure
 */
bool cortical_surround_reset_stats(cortical_surround_t* surround);

/* ============================================================================
 * Bio-Async Integration Functions
 * ========================================================================== */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers module with bio-async messaging system.
 * WHY:  Enable inter-module communication for surround signals.
 * HOW:  Registers with BIO_MODULE_CORTICAL_SURROUND ID.
 *
 * @param surround Surround module
 * @return true on success, false on failure
 */
bool cortical_surround_connect_bio_async(cortical_surround_t* surround);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters module from bio-async system.
 * WHY:  Clean shutdown and resource release.
 * HOW:  Calls bio_router_unregister_module.
 *
 * @param surround Surround module
 * @return true on success, false on failure
 */
bool cortical_surround_disconnect_bio_async(cortical_surround_t* surround);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Queries bio-async connection status.
 * WHY:  Determine if messaging is available.
 * HOW:  Returns bio_async_enabled flag.
 *
 * @param surround Surround module
 * @return true if connected, false otherwise
 */
bool cortical_surround_is_bio_async_connected(const cortical_surround_t* surround);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_SURROUND_H */
