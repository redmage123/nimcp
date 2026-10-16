/**
 * @file nimcp_topographic_maps.h
 * @brief Topographic mapping implementation for cortical organization
 *
 * WHAT: Implements retinotopic, tonotopic, and somatotopic mappings between
 *       sensory input spaces and cortical surface representations.
 *
 * WHY: Biological cortex organizes sensory information spatially with non-uniform
 *      magnification (e.g., fovea in V1, fingertips in S1, low frequencies in A1).
 *      This enables efficient processing with local connectivity and spatial pooling.
 *
 * HOW: Uses mathematical transforms (log-polar for vision, logarithmic for audition,
 *      piecewise for somatosensation) to map input coordinates to cortical positions,
 *      compute receptive fields, and determine cortical magnification factors.
 *
 * @version 1.0.0
 * @date 2025-01-25
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TOPOGRAPHIC_MAPS_H
#define NIMCP_TOPOGRAPHIC_MAPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * WHAT: Enumeration of supported topographic mapping types
 * WHY: Different sensory modalities require different mapping functions
 * HOW: Used to select appropriate transform algorithms at runtime
 */
typedef enum {
    TOPOGRAPHIC_RETINOTOPIC,   /**< Visual field mapping (log-polar) */
    TOPOGRAPHIC_TONOTOPIC,     /**< Frequency mapping (logarithmic) */
    TOPOGRAPHIC_SOMATOTOPIC,   /**< Body surface mapping (piecewise) */
    TOPOGRAPHIC_CUSTOM         /**< User-defined mapping function */
} topographic_type_t;

/**
 * WHAT: Configuration for retinotopic (visual) mapping
 * WHY: V1 uses log-polar mapping with foveal magnification
 * HOW: Parameters control the log-polar transform and magnification function
 */
typedef struct {
    float foveal_radius;           /**< Central high-acuity region (degrees) */
    float cortical_magnification;  /**< M₀ parameter in M = M₀/(1 + E/E₂) */
    float log_polar_a;             /**< Offset in log(r + a) transform */
    float aspect_ratio;            /**< Cortical width/height ratio */
    float eccentricity_half;       /**< E₂ parameter for magnification falloff */
    float angle_coverage;          /**< Angular range (radians, typically 2π) */
} retinotopic_params_t;

/**
 * WHAT: Configuration for tonotopic (auditory) mapping
 * WHY: A1 maps frequency logarithmically (constant Q filters)
 * HOW: Parameters define the frequency range and logarithmic scaling
 */
typedef struct {
    float min_frequency;           /**< Lower bound (Hz, e.g., 20) */
    float max_frequency;           /**< Upper bound (Hz, e.g., 20000) */
    float octave_span;             /**< Octaves per unit cortical distance */
    bool is_logarithmic;           /**< true = log scale, false = linear */
    float q_factor;                /**< Constant Q for bandwidth calculation */
} tonotopic_params_t;

/**
 * WHAT: Configuration for a single body region in somatotopic mapping
 * WHY: Different body parts have different cortical representations
 * HOW: Each region gets a position, size, and magnification factor
 */
typedef struct {
    char name[32];                 /**< Body region identifier */
    float input_start;             /**< Start position in body coordinates */
    float input_end;               /**< End position in body coordinates */
    float cortical_start;          /**< Start position on cortical surface */
    float cortical_end;            /**< End position on cortical surface */
    float magnification;           /**< Cortical mm per input mm */
    float receptor_density;        /**< Receptors per mm² */
} somatotopic_region_t;

/**
 * WHAT: Configuration for somatotopic (body surface) mapping
 * WHY: S1 has non-uniform representation (homunculus)
 * HOW: Piecewise mapping across defined body regions
 */
typedef struct {
    somatotopic_region_t* regions; /**< Array of body regions */
    uint32_t num_regions;          /**< Number of regions */
    float total_cortical_extent;   /**< Total cortical distance (mm) */
    bool is_bilateral;             /**< Symmetric left/right mapping */
} somatotopic_params_t;

/**
 * WHAT: General topographic map configuration
 * WHY: Unified interface for all mapping types
 * HOW: Contains type-specific parameters and common settings
 */
typedef struct {
    topographic_type_t type;       /**< Mapping type selector */
    uint32_t input_dims;           /**< Dimensionality of input space (1-3) */
    uint32_t cortical_dims;        /**< Cortical surface dims (usually 2) */
    float input_range[6];          /**< [min_x, max_x, min_y, max_y, min_z, max_z] */
    float cortical_range[4];       /**< [min_x, max_x, min_y, max_y] */
    float magnification_factor;    /**< Global scaling factor */

    /* Type-specific parameters (union would be better, but C89 compatible) */
    retinotopic_params_t retinotopic;
    tonotopic_params_t tonotopic;
    somatotopic_params_t somatotopic;
} topographic_map_config_t;

/**
 * WHAT: Statistics about the topographic mapping
 * WHY: Diagnostic information for validation and analysis
 * HOW: Computed from the mapping parameters and structure
 */
typedef struct {
    float mean_magnification;      /**< Average cortical magnification */
    float max_magnification;       /**< Maximum magnification (e.g., fovea) */
    float min_magnification;       /**< Minimum magnification (e.g., periphery) */
    float total_cortical_area;     /**< Total cortical surface area */
    float coverage_ratio;          /**< Fraction of cortical area utilized */
    uint32_t num_columns;          /**< Number of cortical columns mapped */
} topographic_stats_t;

/**
 * WHAT: Opaque handle to a topographic map instance
 * WHY: Encapsulates internal state and ensures thread safety
 * HOW: Internal structure defined in .c file, accessed via API
 */
typedef struct topographic_map topographic_map_t;

/* ============================================================================
 * Core API Functions
 * ========================================================================== */

/**
 * WHAT: Creates a topographic map from configuration
 * WHY: Initialize mapping structure with specified parameters
 * HOW: Allocates memory, initializes type-specific data structures
 *
 * @param config Configuration structure (must not be NULL)
 * @return Pointer to new map, or NULL on failure
 *
 * @note Caller must call topographic_map_destroy() to free resources
 * @note Thread-safe (multiple maps can be created concurrently)
 */
topographic_map_t* topographic_map_create(const topographic_map_config_t* config);

/**
 * WHAT: Destroys a topographic map and frees resources
 * WHY: Prevent memory leaks
 * HOW: Frees all allocated memory, including type-specific structures
 *
 * @param map Map to destroy (NULL is safe, no-op)
 *
 * @note Thread-safe (but map must not be in use by other threads)
 */
void topographic_map_destroy(topographic_map_t* map);

/* ============================================================================
 * Specialized Constructors
 * ========================================================================== */

/**
 * WHAT: Creates a retinotopic map for visual cortex (V1)
 * WHY: Convenience function for common vision case
 * HOW: Initializes log-polar transform with foveal magnification
 *
 * @param params Retinotopic parameters (must not be NULL)
 * @param cortical_width Width of cortical surface (columns)
 * @param cortical_height Height of cortical surface (columns)
 * @return Pointer to new map, or NULL on failure
 */
topographic_map_t* topographic_map_create_retinotopic(
    const retinotopic_params_t* params,
    uint32_t cortical_width,
    uint32_t cortical_height
);

/**
 * WHAT: Creates a tonotopic map for auditory cortex (A1)
 * WHY: Convenience function for frequency mapping
 * HOW: Initializes logarithmic frequency transform
 *
 * @param params Tonotopic parameters (must not be NULL)
 * @param num_frequency_bands Number of frequency bands (cortical columns)
 * @return Pointer to new map, or NULL on failure
 */
topographic_map_t* topographic_map_create_tonotopic(
    const tonotopic_params_t* params,
    uint32_t num_frequency_bands
);

/**
 * WHAT: Creates a somatotopic map for somatosensory cortex (S1)
 * WHY: Convenience function for body surface mapping
 * HOW: Initializes piecewise linear mapping with homunculus
 *
 * @param num_body_regions Number of distinct body regions
 * @return Pointer to new map, or NULL on failure
 *
 * @note Call topographic_map_add_body_region() to define regions
 */
topographic_map_t* topographic_map_create_somatotopic(uint32_t num_body_regions);

/* ============================================================================
 * Coordinate Mapping Functions
 * ========================================================================== */

/**
 * WHAT: Maps input coordinates to cortical coordinates (forward mapping)
 * WHY: Determine cortical location for sensory input
 * HOW: Applies type-specific transform (log-polar, log, or piecewise)
 *
 * @param map Topographic map (must not be NULL)
 * @param input_coords Input coordinates array [x, y, z, ...] per point
 * @param cortical_coords Output cortical coordinates [x, y] per point
 * @param num_points Number of points to transform
 *
 * @note Thread-safe (read-only operation)
 * @note input_coords size must be num_points * input_dims
 * @note cortical_coords size must be num_points * cortical_dims
 */
void topographic_map_input_to_cortex(
    topographic_map_t* map,
    const float* input_coords,
    float* cortical_coords,
    uint32_t num_points
);

/**
 * WHAT: Maps cortical coordinates back to input coordinates (inverse mapping)
 * WHY: Determine receptive field centers from cortical position
 * HOW: Applies inverse of type-specific transform
 *
 * @param map Topographic map (must not be NULL)
 * @param cortical_coords Cortical coordinates [x, y] per point
 * @param input_coords Output input coordinates array
 * @param num_points Number of points to transform
 *
 * @note Thread-safe (read-only operation)
 * @note Not all mappings have unique inverses (may return average)
 */
void topographic_map_cortex_to_input(
    topographic_map_t* map,
    const float* cortical_coords,
    float* input_coords,
    uint32_t num_points
);

/**
 * WHAT: Gets the cortical column ID for a given input coordinate
 * WHY: Map sensory input to specific column for processing
 * HOW: Transforms coordinate and quantizes to column grid
 *
 * @param map Topographic map (must not be NULL)
 * @param input_coords Input coordinates [x, y, z, ...]
 * @return Column ID (0-based index), or UINT32_MAX if out of range
 *
 * @note Thread-safe (read-only operation)
 */
uint32_t topographic_map_get_column_for_input(
    topographic_map_t* map,
    const float* input_coords
);

/* ============================================================================
 * Receptive Field Functions
 * ========================================================================== */

/**
 * WHAT: Computes receptive field center and size for a cortical column
 * WHY: Determine what input region each column responds to
 * HOW: Inverse mapping + magnification factor calculation
 *
 * @param map Topographic map (must not be NULL)
 * @param column_id Column index (must be valid)
 * @param rf_center Output receptive field center coordinates
 * @param rf_size Output receptive field size (radius or half-width)
 *
 * @note Thread-safe (read-only operation)
 * @note rf_center size must match input_dims
 * @note rf_size is scalar for isotropic RFs, array for anisotropic
 */
void topographic_map_get_receptive_field(
    topographic_map_t* map,
    uint32_t column_id,
    float* rf_center,
    float* rf_size
);

/**
 * WHAT: Computes cortical magnification factor at input location
 * WHY: Determine cortical area per input area (spatial resolution)
 * HOW: Evaluates magnification function M(x) for the mapping type
 *
 * @param map Topographic map (must not be NULL)
 * @param input_coords Input coordinates where to compute magnification
 * @return Magnification factor (cortical mm / input mm), or 0.0 on error
 *
 * @note Thread-safe (read-only operation)
 * @note Higher values = more cortical area dedicated to input region
 */
float topographic_map_get_magnification(
    topographic_map_t* map,
    const float* input_coords
);

/* ============================================================================
 * Activity Projection Functions
 * ========================================================================== */

/**
 * WHAT: Projects input activity pattern onto cortical surface
 * WHY: Transform sensory input into cortical activation pattern
 * HOW: Resamples input using forward mapping and magnification weighting
 *
 * @param map Topographic map (must not be NULL)
 * @param input_activity Input activation values (2D array)
 * @param input_width Width of input array
 * @param input_height Height of input array
 * @param cortical_activity Output cortical activation (2D array)
 * @param cortical_width Width of cortical array
 * @param cortical_height Height of cortical array
 *
 * @note Thread-safe (read-only on map)
 * @note Uses bilinear interpolation for smooth projection
 */
void topographic_map_project_activity(
    topographic_map_t* map,
    const float* input_activity,
    uint32_t input_width,
    uint32_t input_height,
    float* cortical_activity,
    uint32_t cortical_width,
    uint32_t cortical_height
);

/* ============================================================================
 * Column Assignment Functions
 * ========================================================================== */

/**
 * WHAT: Assigns input coordinates to all cortical columns
 * WHY: Pre-compute receptive field centers for all columns
 * HOW: Iterates cortical grid and applies inverse mapping
 *
 * @param map Topographic map (must not be NULL)
 * @param column_ids Output column IDs (can be NULL if not needed)
 * @param column_positions Output column RF centers (must not be NULL)
 * @param num_columns Number of columns (must match map size)
 *
 * @note Thread-safe (read-only on map)
 * @note column_positions size must be num_columns * input_dims
 */
void topographic_map_assign_columns(
    topographic_map_t* map,
    uint32_t* column_ids,
    float* column_positions,
    uint32_t num_columns
);

/* ============================================================================
 * Neighborhood Functions
 * ========================================================================== */

/**
 * WHAT: Finds neighboring columns within cortical distance
 * WHY: Determine lateral connectivity patterns
 * HOW: Searches cortical grid using Euclidean distance
 *
 * @param map Topographic map (must not be NULL)
 * @param column_id Center column ID
 * @param radius Search radius in cortical space (mm or normalized units)
 * @param neighbor_ids Output array for neighbor column IDs
 * @param max_neighbors Maximum number of neighbors to return
 * @return Actual number of neighbors found
 *
 * @note Thread-safe (read-only operation)
 * @note Returns neighbors sorted by distance (nearest first)
 */
uint32_t topographic_map_get_neighbors(
    topographic_map_t* map,
    uint32_t column_id,
    float radius,
    uint32_t* neighbor_ids,
    uint32_t max_neighbors
);

/* ============================================================================
 * Statistics and Diagnostics
 * ========================================================================== */

/**
 * WHAT: Computes statistics about the topographic mapping
 * WHY: Validate mapping and analyze cortical organization
 * HOW: Samples mapping across input space and aggregates metrics
 *
 * @param map Topographic map (must not be NULL)
 * @param stats Output statistics structure (must not be NULL)
 *
 * @note Thread-safe (read-only operation)
 * @note May be computationally expensive for large maps
 */
void topographic_map_get_stats(
    topographic_map_t* map,
    topographic_stats_t* stats
);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * WHAT: Adds a body region to a somatotopic map
 * WHY: Define homunculus structure region by region
 * HOW: Appends region to internal array
 *
 * @param map Somatotopic map (must not be NULL)
 * @param region Region definition (must not be NULL)
 * @return true on success, false on failure
 *
 * @note Thread-safe (uses internal mutex)
 * @note Must be called before using the map
 */
bool topographic_map_add_body_region(
    topographic_map_t* map,
    const somatotopic_region_t* region
);

/**
 * WHAT: Validates a topographic map configuration
 * WHY: Catch configuration errors before creating map
 * HOW: Checks ranges, parameters, and type-specific constraints
 *
 * @param config Configuration to validate (must not be NULL)
 * @return true if valid, false otherwise
 *
 * @note Thread-safe (pure function)
 */
bool topographic_map_validate_config(const topographic_map_config_t* config);

/**
 * WHAT: Gets the cortical dimensions of the map
 * WHY: Query map structure for array allocation
 * HOW: Returns stored width/height values
 *
 * @param map Topographic map (must not be NULL)
 * @param width Output width (can be NULL)
 * @param height Output height (can be NULL)
 *
 * @note Thread-safe (read-only operation)
 */
void topographic_map_get_dimensions(
    topographic_map_t* map,
    uint32_t* width,
    uint32_t* height
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOPOGRAPHIC_MAPS_H */
