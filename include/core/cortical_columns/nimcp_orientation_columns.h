/**
 * @file nimcp_orientation_columns.h
 * @brief Orientation columns for primary visual cortex (V1) edge detection
 *
 * WHAT: Implementation of orientation-selective columns in V1 cortex that detect
 *       edges at specific angles using Gabor filters and energy models.
 * WHY:  Orientation selectivity is fundamental to visual processing, enabling
 *       edge detection, contour integration, and form perception.
 * HOW:  Uses Gabor filters (simple cells), energy models (complex cells),
 *       hypercolumn organization with pinwheel patterns, and divisive normalization.
 *
 * Mathematical Models:
 * - Gabor filter: G(x,y,θ,λ,ψ,σ,γ) = exp(-(x'² + γ²y'²)/(2σ²)) × cos(2π×x'/λ + ψ)
 * - Energy model: E = √(G_even² + G_odd²)
 * - Tuning curve: R(θ) = R_base + R_max × exp(κ × cos(2(θ - θ_pref)))
 * - OSI: (R_pref - R_orth) / (R_pref + R_orth)
 * - Circular variance: 1 - |Σ(R_i × e^(2iθ_i))| / Σ(R_i)
 *
 * @version 1.0.0
 * @date 2025-01-25
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ORIENTATION_COLUMNS_H
#define NIMCP_ORIENTATION_COLUMNS_H

#include <stdint.h>
#include <stdbool.h>
#include "constants/nimcp_math_constants.h"
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

/** Default number of orientations in a hypercolumn */
#define ORIENTATION_DEFAULT_NUM_ORIENTATIONS 16

/** Default tuning width (HWHH in degrees) */
#define ORIENTATION_DEFAULT_TUNING_WIDTH 30.0f

/** Default spatial frequency (cycles/degree) */
#define ORIENTATION_DEFAULT_SPATIAL_FREQ 2.0f

/** Maximum orientations per hypercolumn */
#define ORIENTATION_MAX_ORIENTATIONS 64

/** Pi constant */

/* ============================================================================
 * Data Structures
 * ========================================================================== */

/**
 * @struct cc_gabor_params_t
 * @brief Parameters for Gabor filter kernel
 *
 * WHAT: Configuration for 2D Gabor filter that models simple cell receptive fields.
 * WHY:  Gabor filters optimally detect oriented edges with specific spatial frequencies.
 * HOW:  Combines Gaussian envelope with sinusoidal carrier for orientation selectivity.
 */
typedef struct {
    float sigma_x;      /**< Gaussian envelope width parallel to orientation */
    float sigma_y;      /**< Gaussian envelope width perpendicular to orientation */
    float lambda;       /**< Wavelength of sinusoidal carrier (1/spatial_frequency) */
    float gamma;        /**< Aspect ratio (sigma_y/sigma_x) */
    float psi;          /**< Phase offset (0 for even, π/2 for odd symmetry) */
    float theta;        /**< Orientation angle in radians */
} cc_gabor_params_t;

/**
 * @struct orientation_column_t
 * @brief Single orientation-selective column
 *
 * WHAT: Represents one orientation preference within a hypercolumn.
 * WHY:  Models a population of neurons tuned to a specific edge orientation.
 * HOW:  Uses Gabor filtering and tuning curves to compute orientation responses.
 */
typedef struct {
    float preferred_orientation;    /**< Preferred angle in degrees (0-180) */
    float tuning_width;            /**< Half-width at half-height in degrees */
    float spatial_frequency;       /**< Preferred spatial frequency (cycles/degree) */
    float phase;                   /**< Gabor phase (0 or π/2) */
    float activation;              /**< Current response level */
    uint32_t column_id;            /**< Unique identifier */

    /* Gabor filter parameters */
    cc_gabor_params_t gabor_params;   /**< Configured Gabor parameters */

    /* Tuning curve parameters */
    float kappa;                   /**< Von Mises concentration parameter */
    float baseline_response;       /**< Spontaneous firing rate */
    float max_response;            /**< Maximum response amplitude */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex; /**< Thread safety mutex */
} orientation_column_t;

/**
 * @struct orientation_hypercolumn_t
 * @brief Complete set of orientation columns (hypercolumn)
 *
 * WHAT: Collection of orientation columns covering all orientations (0-180°).
 * WHY:  Hypercolumns provide complete orientation coverage at each spatial location.
 * HOW:  Organizes columns in pinwheel pattern with winner-take-all competition.
 */
typedef struct {
    orientation_column_t* columns;  /**< Array of orientation columns */
    uint32_t num_orientations;      /**< Number of columns in hypercolumn */

    /* Spatial organization */
    float pinwheel_center_x;        /**< X coordinate of pinwheel singularity */
    float pinwheel_center_y;        /**< Y coordinate of pinwheel singularity */

    /* Population response */
    float dominant_orientation;     /**< Current winning orientation (degrees) */
    float selectivity_index;        /**< OSI = (R_pref - R_orth) / (R_pref + R_orth) */
    float circular_variance;        /**< 1 - |Σ(R_i × e^(2iθ_i))| / Σ(R_i) */

    /* Competition parameters */
    float inhibition_strength;      /**< Cross-orientation inhibition strength */
    float normalization_constant;   /**< Divisive normalization constant */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;  /**< Thread safety mutex */
} orientation_hypercolumn_t;

/**
 * @struct orientation_stats_t
 * @brief Statistics for a single orientation column
 *
 * WHAT: Diagnostic metrics for column performance and selectivity.
 * WHY:  Enable monitoring and debugging of orientation processing.
 * HOW:  Aggregates activation history and tuning properties.
 */
typedef struct {
    float mean_activation;          /**< Average activation over time */
    float max_activation;           /**< Peak activation observed */
    float min_activation;           /**< Minimum activation observed */
    float tuning_sharpness;         /**< Sharpness of orientation tuning */
    uint64_t total_activations;     /**< Number of activation events */
} orientation_stats_t;

/**
 * @struct orientation_hypercolumn_stats_t
 * @brief Statistics for an orientation hypercolumn
 *
 * WHAT: Population-level metrics for orientation hypercolumn performance.
 * WHY:  Characterize hypercolumn selectivity and competition dynamics.
 * HOW:  Aggregates statistics across all constituent orientation columns.
 * NOTE: Named orientation_hypercolumn_stats_t to avoid conflict with cc_hypercolumn_stats_t
 */
typedef struct {
    float mean_osi;                 /**< Average orientation selectivity index */
    float mean_circular_variance;   /**< Average circular variance */
    float competition_strength;     /**< Measure of competitive interactions */
    float coverage_uniformity;      /**< How evenly orientations are sampled */
    uint32_t num_active_columns;    /**< Number of columns above threshold */
} orientation_hypercolumn_stats_t;

/* ============================================================================
 * Orientation Column Functions
 * ========================================================================== */

/**
 * @brief Create a single orientation column
 *
 * WHAT: Allocates and initializes an orientation-selective column.
 * WHY:  Establishes a basic unit for orientation processing.
 * HOW:  Allocates memory, sets orientation preference, configures Gabor parameters.
 *
 * @param preferred_orientation Preferred angle in degrees (0-180)
 * @param tuning_width Half-width at half-height in degrees
 * @param spatial_frequency Preferred spatial frequency in cycles/degree
 * @return Pointer to created column, NULL on failure
 *
 * @note Caller must free with orientation_column_destroy()
 * @note Thread-safe after creation
 */
orientation_column_t* orientation_column_create(
    float preferred_orientation,
    float tuning_width,
    float spatial_frequency
);

/**
 * @brief Destroy an orientation column
 *
 * WHAT: Frees all resources associated with a column.
 * WHY:  Prevents memory leaks and releases locks.
 * HOW:  Destroys mutex, frees memory using nimcp_free.
 *
 * @param col Column to destroy (may be NULL)
 */
void orientation_column_destroy(orientation_column_t* col);

/**
 * @brief Set Gabor filter parameters for a column
 *
 * WHAT: Configures the Gabor filter kernel for this column.
 * WHY:  Allows customization of receptive field properties.
 * HOW:  Updates gabor_params structure with provided values.
 *
 * @param col Orientation column to configure
 * @param params Gabor parameters to set
 * @return true on success, false on failure
 */
bool orientation_column_set_gabor(
    orientation_column_t* col,
    const cc_gabor_params_t* params
);

/**
 * @brief Apply Gabor filter to image patch
 *
 * WHAT: Computes simple cell response using Gabor convolution.
 * WHY:  Models V1 simple cells that detect oriented edges.
 * HOW:  Convolves image patch with Gabor kernel, returns activation.
 *
 * @param col Orientation column with Gabor parameters
 * @param image_patch Grayscale image data (row-major, normalized 0-1)
 * @param patch_width Width of image patch in pixels
 * @param patch_height Height of image patch in pixels
 * @return Gabor filter response (activation level)
 */
float orientation_column_apply_gabor(
    orientation_column_t* col,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
);

/**
 * @brief Compute energy model response (complex cell)
 *
 * WHAT: Calculates phase-invariant response using energy model.
 * WHY:  Complex cells in V1 are phase-invariant, responding to edges regardless of contrast polarity.
 * HOW:  Computes E = √(G_even² + G_odd²) using quadrature pair.
 *
 * @param col Orientation column
 * @param image_patch Grayscale image data
 * @param patch_width Width in pixels
 * @param patch_height Height in pixels
 * @return Energy model response
 */
float orientation_column_compute_energy(
    orientation_column_t* col,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
);

/**
 * @brief Get response to a stimulus orientation
 *
 * WHAT: Computes column response using orientation tuning curve.
 * WHY:  Models how neurons respond to different orientations.
 * HOW:  Uses von Mises function: R = R_base + R_max × exp(κ × cos(2(θ - θ_pref)))
 *
 * @param col Orientation column
 * @param stimulus_orientation Stimulus angle in degrees
 * @return Response level (0-1 normalized)
 */
float orientation_column_get_response(
    orientation_column_t* col,
    float stimulus_orientation
);

/**
 * @brief Get complete tuning curve
 *
 * WHAT: Samples orientation tuning curve at multiple points.
 * WHY:  Visualize and analyze orientation selectivity.
 * HOW:  Evaluates tuning function at num_points evenly spaced orientations.
 *
 * @param col Orientation column
 * @param orientations Output array for orientation values (degrees)
 * @param responses Output array for response values
 * @param num_points Number of sample points (must be > 0)
 * @return true on success, false on failure
 */
bool orientation_column_get_tuning_curve(
    orientation_column_t* col,
    float* orientations,
    float* responses,
    uint32_t num_points
);

/**
 * @brief Get column statistics
 *
 * WHAT: Retrieves diagnostic metrics for a column.
 * WHY:  Monitor performance and debug issues.
 * HOW:  Populates stats structure with current metrics.
 *
 * @param col Orientation column
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool orientation_column_get_stats(
    orientation_column_t* col,
    orientation_stats_t* stats
);

/* ============================================================================
 * Hypercolumn Functions
 * ========================================================================== */

/**
 * @brief Create an orientation hypercolumn
 *
 * WHAT: Allocates hypercolumn with evenly spaced orientation columns.
 * WHY:  Provides complete orientation coverage at a spatial location.
 * HOW:  Creates num_orientations columns spanning 0-180 degrees.
 *
 * @param num_orientations Number of orientation columns (typically 8-16)
 * @param spatial_frequency Preferred spatial frequency for all columns
 * @param tuning_width Tuning width for all columns in degrees
 * @return Pointer to created hypercolumn, NULL on failure
 *
 * @note Caller must free with orientation_hypercolumn_destroy()
 */
orientation_hypercolumn_t* orientation_hypercolumn_create(
    uint32_t num_orientations,
    float spatial_frequency,
    float tuning_width
);

/**
 * @brief Destroy an orientation hypercolumn
 *
 * WHAT: Frees all resources associated with hypercolumn and its columns.
 * WHY:  Prevents memory leaks.
 * HOW:  Destroys all columns, mutex, and frees memory.
 *
 * @param hcol Hypercolumn to destroy (may be NULL)
 */
void orientation_hypercolumn_destroy(orientation_hypercolumn_t* hcol);

/**
 * @brief Process image patch through hypercolumn
 *
 * WHAT: Computes responses of all orientation columns to an image patch.
 * WHY:  Determines which orientations are present in the input.
 * HOW:  Applies energy model to each column, updates activations.
 *
 * @param hcol Orientation hypercolumn
 * @param image_patch Grayscale image data
 * @param patch_width Width in pixels
 * @param patch_height Height in pixels
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_process(
    orientation_hypercolumn_t* hcol,
    const float* image_patch,
    uint32_t patch_width,
    uint32_t patch_height
);

/**
 * @brief Get dominant orientation in hypercolumn
 *
 * WHAT: Returns the orientation with strongest activation.
 * WHY:  Determine the primary edge orientation at this location.
 * HOW:  Finds column with maximum activation.
 *
 * @param hcol Orientation hypercolumn
 * @return Dominant orientation in degrees (0-180), or -1 on error
 */
float orientation_hypercolumn_get_dominant(orientation_hypercolumn_t* hcol);

/**
 * @brief Get orientation distribution across hypercolumn
 *
 * WHAT: Returns activations for all orientations.
 * WHY:  Analyze full population response pattern.
 * HOW:  Copies orientation preferences and activations to output arrays.
 *
 * @param hcol Orientation hypercolumn
 * @param orientations Output array for orientation values (degrees)
 * @param responses Output array for activation values
 * @param num_orientations Output for number of orientations written
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_get_distribution(
    orientation_hypercolumn_t* hcol,
    float* orientations,
    float* responses,
    uint32_t* num_orientations
);

/**
 * @brief Apply divisive normalization to hypercolumn
 *
 * WHAT: Normalizes activations using divisive normalization.
 * WHY:  Models gain control and contrast normalization in V1.
 * HOW:  R_i' = R_i / (σ + Σ(R_j))
 *
 * @param hcol Orientation hypercolumn
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_normalize(orientation_hypercolumn_t* hcol);

/**
 * @brief Apply cross-orientation inhibition
 *
 * WHAT: Suppresses columns based on activity of other orientations.
 * WHY:  Models lateral inhibition and sharpens tuning.
 * HOW:  R_i' = R_i - strength × Σ(R_j × inhibition_kernel(θ_i - θ_j))
 *
 * @param hcol Orientation hypercolumn
 * @param strength Inhibition strength (0-1)
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_apply_inhibition(
    orientation_hypercolumn_t* hcol,
    float strength
);

/**
 * @brief Compute orientation selectivity index (OSI)
 *
 * WHAT: Calculates OSI = (R_pref - R_orth) / (R_pref + R_orth).
 * WHY:  Quantifies how selective the hypercolumn is for orientation.
 * HOW:  Compares preferred orientation response to orthogonal response.
 *
 * @param hcol Orientation hypercolumn
 * @return OSI value (0-1), where 1 is maximally selective, or -1 on error
 */
float orientation_hypercolumn_compute_osi(orientation_hypercolumn_t* hcol);

/**
 * @brief Compute circular variance
 *
 * WHAT: Calculates CV = 1 - |Σ(R_i × e^(2iθ_i))| / Σ(R_i).
 * WHY:  Measures tuning sharpness (0 = sharp, 1 = broad).
 * HOW:  Uses complex vector sum of responses weighted by orientation.
 *
 * @param hcol Orientation hypercolumn
 * @return Circular variance (0-1), or -1 on error
 */
float orientation_hypercolumn_compute_circular_variance(
    orientation_hypercolumn_t* hcol
);

/**
 * @brief Set pinwheel center for hypercolumn
 *
 * WHAT: Defines the spatial location of the orientation pinwheel singularity.
 * WHY:  Pinwheel organization is a hallmark of V1 orientation maps.
 * HOW:  Stores center coordinates for spatial orientation calculations.
 *
 * @param hcol Orientation hypercolumn
 * @param center_x X coordinate of pinwheel center
 * @param center_y Y coordinate of pinwheel center
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_set_pinwheel(
    orientation_hypercolumn_t* hcol,
    float center_x,
    float center_y
);

/**
 * @brief Get local orientation at spatial location
 *
 * WHAT: Computes orientation preference based on pinwheel geometry.
 * WHY:  Model spatial variation of orientation preference.
 * HOW:  θ(x,y) = atan2(y - y₀, x - x₀)
 *
 * @param hcol Orientation hypercolumn with pinwheel configured
 * @param x X coordinate
 * @param y Y coordinate
 * @return Local orientation in degrees (0-180), or -1 on error
 */
float orientation_hypercolumn_get_local_orientation(
    orientation_hypercolumn_t* hcol,
    float x,
    float y
);

/**
 * @brief Get hypercolumn statistics
 *
 * WHAT: Retrieves population-level metrics.
 * WHY:  Monitor hypercolumn performance and selectivity.
 * HOW:  Aggregates statistics across all columns.
 *
 * @param hcol Orientation hypercolumn
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool orientation_hypercolumn_get_stats(
    orientation_hypercolumn_t* hcol,
    orientation_hypercolumn_stats_t* stats
);

/* ============================================================================
 * Batch Processing Functions
 * ========================================================================== */

/**
 * @brief Process entire image through orientation hypercolumns
 *
 * WHAT: Applies hypercolumns to an image to create orientation map.
 * WHY:  Generate orientation representation for full visual field.
 * HOW:  Tiles hypercolumns across image, populates orientation map.
 *
 * @param image Input grayscale image (row-major, normalized 0-1)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param hypercolumns Array of hypercolumns to apply
 * @param num_hypercolumns Number of hypercolumns
 * @param orientation_map Output orientation map (dominant orientation per pixel)
 * @return true on success, false on failure
 *
 * @note orientation_map must be pre-allocated to width × height floats
 */
bool orientation_process_image(
    const float* image,
    uint32_t width,
    uint32_t height,
    orientation_hypercolumn_t** hypercolumns,
    uint32_t num_hypercolumns,
    float* orientation_map
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ORIENTATION_COLUMNS_H */
