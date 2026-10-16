/**
 * @file nimcp_gabor.h
 * @brief Unified Gabor filter library for V1 visual processing
 *
 * WHAT: Shared Gabor filter implementation for edge detection and orientation
 *       selectivity across NIMCP perception and cortical modules.
 * WHY:  Consolidates duplicate Gabor code from visual_cortex and orientation_columns,
 *       providing a single, well-tested implementation for all V1-style processing.
 * HOW:  Implements 2D Gabor functions with support for both kernel generation and
 *       point-wise evaluation, energy models (complex cells), and DC balancing.
 *
 * Mathematical Model:
 * G(x,y) = exp(-(x'² + γ²y'²)/(2σ²)) × cos(2π×x'/λ + ψ)
 *
 * Where:
 * - x' = x×cos(θ) + y×sin(θ)     (rotated x)
 * - y' = -x×sin(θ) + y×cos(θ)    (rotated y)
 * - θ = orientation angle
 * - λ = wavelength (1/spatial_frequency)
 * - γ = aspect ratio
 * - σ = Gaussian envelope width
 * - ψ = phase offset
 *
 * Neuroscience Background:
 * - V1 simple cells are optimally modeled by Gabor filters
 * - Complex cells use energy model: E = √(G_even² + G_odd²)
 * - Orientations span 0-180° with typical 15-30° bandwidth
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GABOR_H
#define NIMCP_GABOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

/** Default Gabor wavelength in pixels */
#define GABOR_DEFAULT_WAVELENGTH 4.0f

/** Default Gabor aspect ratio (γ) */
#define GABOR_DEFAULT_ASPECT_RATIO 0.5f

/** Default Gabor bandwidth (σ/λ ratio) */
#define GABOR_DEFAULT_BANDWIDTH 1.0f

/** Default Gabor sigma multiplier for kernel size */
#define GABOR_KERNEL_SIGMA_MULTIPLIER 6

/** Minimum kernel size */
#define GABOR_MIN_KERNEL_SIZE 3

/** Maximum kernel size */
#define GABOR_MAX_KERNEL_SIZE 31

/** Pi constant */

/* ============================================================================
 * Data Structures
 * ========================================================================== */

/**
 * @struct gabor_filter_params_t
 * @brief Unified Gabor filter parameters
 *
 * WHAT: Complete configuration for 2D Gabor filter.
 * WHY:  Single structure replacing cc_gabor_params_t and gabor_params_t.
 * HOW:  Supports both direct sigma specification and wavelength-based calculation.
 */
typedef struct {
    /** Orientation angle in degrees (0-180) */
    float orientation_deg;

    /** Wavelength in pixels (λ = 1/spatial_frequency) */
    float wavelength;

    /** Phase offset in degrees (0=even/cosine, 90=odd/sine) */
    float phase_deg;

    /** Aspect ratio (γ) - controls ellipticity (typical: 0.3-0.7) */
    float aspect_ratio;

    /** Bandwidth - controls envelope width relative to wavelength (typical: 0.5-1.5) */
    float bandwidth;

    /**
     * Sigma x override - if > 0, uses this directly instead of computing from bandwidth.
     * Set to 0 to use automatic calculation: sigma = wavelength × bandwidth
     */
    float sigma_x_override;

    /**
     * Sigma y override - if > 0, uses this directly.
     * Set to 0 to use automatic calculation: sigma_y = sigma_x / aspect_ratio
     */
    float sigma_y_override;
} gabor_filter_params_t;

/**
 * @struct gabor_kernel_t
 * @brief Pre-computed Gabor kernel
 *
 * WHAT: Stores a pre-computed Gabor filter kernel.
 * WHY:  Enables efficient reuse without recomputation.
 * HOW:  Allocates kernel data and stores parameters for reference.
 */
typedef struct {
    /** Kernel data in row-major order [size × size] */
    float* data;

    /** Kernel size (odd, e.g., 7, 9, 11) */
    uint32_t size;

    /** Parameters used to generate this kernel */
    gabor_filter_params_t params;

    /** Sum of kernel values (before DC balance) */
    float sum;

    /** Whether DC balance was applied */
    bool dc_balanced;
} gabor_kernel_t;

/**
 * @struct gabor_filter_bank_t
 * @brief Collection of Gabor kernels at multiple orientations
 *
 * WHAT: Pre-computed bank of Gabor filters covering orientation space.
 * WHY:  Efficient V1-style multi-orientation filtering.
 * HOW:  Stores array of kernels at evenly spaced orientations.
 */
typedef struct {
    /** Array of Gabor kernels */
    gabor_kernel_t** kernels;

    /** Number of orientations in the bank */
    uint32_t num_orientations;

    /** Kernel size (same for all) */
    uint32_t kernel_size;

    /** Base wavelength */
    float wavelength;

    /** Whether to include quadrature (odd-phase) pairs */
    bool include_quadrature;

    /** Total number of kernels (num_orientations × 2 if quadrature) */
    uint32_t total_kernels;
} gabor_filter_bank_t;

/**
 * @struct gabor_stats_t
 * @brief Statistics for Gabor filter operations
 *
 * WHAT: Diagnostic information about Gabor processing.
 * WHY:  Enable monitoring and debugging.
 * HOW:  Track computation metrics.
 */
typedef struct {
    uint64_t kernels_created;      /**< Total kernels created */
    uint64_t convolutions;         /**< Total convolution operations */
    uint64_t point_evaluations;    /**< Total point evaluations */
    float total_compute_time_ms;   /**< Total computation time */
} gabor_stats_t;

/* ============================================================================
 * Parameter Functions
 * ========================================================================== */

/**
 * @brief Initialize Gabor parameters with defaults
 *
 * WHAT: Sets up default Gabor parameters.
 * WHY:  Provide sensible starting point for filter configuration.
 * HOW:  Uses biologically-plausible V1 values.
 *
 * @param params Parameters structure to initialize
 */
void gabor_default_params(gabor_filter_params_t* params);

/**
 * @brief Initialize Gabor parameters for specific orientation
 *
 * WHAT: Creates parameters for a specific orientation.
 * WHY:  Convenient initialization for orientation column.
 * HOW:  Sets orientation and uses defaults for other parameters.
 *
 * @param params Parameters structure to initialize
 * @param orientation_deg Orientation in degrees (0-180)
 */
void gabor_params_for_orientation(gabor_filter_params_t* params, float orientation_deg);

/**
 * @brief Initialize Gabor parameters from spatial frequency
 *
 * WHAT: Creates parameters from spatial frequency specification.
 * WHY:  Neuroscience typically specifies frequency (cycles/degree).
 * HOW:  Converts frequency to wavelength and sets defaults.
 *
 * @param params Parameters structure to initialize
 * @param spatial_frequency Spatial frequency in cycles per unit
 * @param orientation_deg Orientation in degrees
 */
void gabor_params_from_frequency(gabor_filter_params_t* params,
                                  float spatial_frequency,
                                  float orientation_deg);

/**
 * @brief Validate Gabor parameters
 *
 * WHAT: Checks parameters for validity.
 * WHY:  Prevent computation errors from invalid inputs.
 * HOW:  Verifies ranges and consistency.
 *
 * @param params Parameters to validate
 * @return true if valid, false otherwise
 */
bool gabor_validate_params(const gabor_filter_params_t* params);

/**
 * @brief Compute effective sigma values from parameters
 *
 * WHAT: Calculates actual sigma_x and sigma_y values.
 * WHY:  Handles both direct specification and computed values.
 * HOW:  Uses override if specified, otherwise computes from wavelength.
 *
 * @param params Source parameters
 * @param sigma_x Output sigma_x value
 * @param sigma_y Output sigma_y value
 */
void gabor_compute_sigmas(const gabor_filter_params_t* params,
                          float* sigma_x,
                          float* sigma_y);

/* ============================================================================
 * Point Evaluation Functions
 * ========================================================================== */

/**
 * @brief Evaluate Gabor function at a point
 *
 * WHAT: Computes Gabor filter value at (x, y) coordinates.
 * WHY:  Core building block for convolution and kernel generation.
 * HOW:  Rotates coordinates, applies Gaussian and sinusoid.
 *
 * @param x X coordinate (relative to filter center)
 * @param y Y coordinate (relative to filter center)
 * @param params Gabor filter parameters
 * @return Gabor filter value at (x, y)
 */
float gabor_evaluate(float x, float y, const gabor_filter_params_t* params);

/**
 * @brief Evaluate even (cosine) phase Gabor at a point
 *
 * WHAT: Computes even-symmetric Gabor (phase = 0).
 * WHY:  Part of quadrature pair for energy model.
 * HOW:  Uses cosine carrier with phase = 0.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param params Gabor filter parameters (phase is overridden to 0)
 * @return Even Gabor value
 */
float gabor_evaluate_even(float x, float y, const gabor_filter_params_t* params);

/**
 * @brief Evaluate odd (sine) phase Gabor at a point
 *
 * WHAT: Computes odd-symmetric Gabor (phase = 90).
 * WHY:  Part of quadrature pair for energy model.
 * HOW:  Uses sine carrier with phase = 90.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param params Gabor filter parameters (phase is overridden to 90)
 * @return Odd Gabor value
 */
float gabor_evaluate_odd(float x, float y, const gabor_filter_params_t* params);

/**
 * @brief Compute energy model response at a point
 *
 * WHAT: Computes phase-invariant response using energy model.
 * WHY:  Models V1 complex cells which are phase-invariant.
 * HOW:  E = sqrt(G_even² + G_odd²)
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param params Gabor filter parameters
 * @return Energy model value (always non-negative)
 */
float gabor_compute_energy(float x, float y, const gabor_filter_params_t* params);

/* ============================================================================
 * Kernel Functions
 * ========================================================================== */

/**
 * @brief Create a Gabor kernel
 *
 * WHAT: Generates complete Gabor filter kernel.
 * WHY:  Pre-compute kernel for efficient convolution.
 * HOW:  Evaluates Gabor at each kernel position.
 *
 * @param size Kernel size (must be odd, typically 7-15)
 * @param params Gabor filter parameters
 * @param dc_balance Whether to apply DC balancing (subtract mean)
 * @return Allocated kernel structure, or NULL on failure
 *
 * @note Caller must free with gabor_kernel_destroy()
 */
gabor_kernel_t* gabor_kernel_create(uint32_t size,
                                     const gabor_filter_params_t* params,
                                     bool dc_balance);

/**
 * @brief Create a Gabor kernel with automatic size
 *
 * WHAT: Creates kernel with size computed from sigma.
 * WHY:  Ensures kernel captures full filter support.
 * HOW:  size = 2 × ceil(sigma × multiplier) + 1
 *
 * @param params Gabor filter parameters
 * @param dc_balance Whether to apply DC balancing
 * @return Allocated kernel structure, or NULL on failure
 */
gabor_kernel_t* gabor_kernel_create_auto_size(const gabor_filter_params_t* params,
                                               bool dc_balance);

/**
 * @brief Destroy a Gabor kernel
 *
 * WHAT: Frees kernel memory.
 * WHY:  Clean up resources.
 * HOW:  Frees data array and structure.
 *
 * @param kernel Kernel to destroy (may be NULL)
 */
void gabor_kernel_destroy(gabor_kernel_t* kernel);

/**
 * @brief Create raw kernel data (legacy API compatibility)
 *
 * WHAT: Creates kernel data array without wrapper structure.
 * WHY:  Backward compatibility with gabor_create_kernel().
 * HOW:  Allocates and fills float array.
 *
 * @param kernel_size Kernel dimension (must be odd)
 * @param params Gabor filter parameters
 * @return Allocated kernel data [size × size], or NULL on failure
 *
 * @note Caller must free with nimcp_free()
 */
float* gabor_create_kernel_data(int kernel_size, const gabor_filter_params_t* params);

/**
 * @brief Get kernel value at position
 *
 * WHAT: Retrieves kernel value at (x, y).
 * WHY:  Safe accessor with bounds checking.
 * HOW:  Computes index and returns value.
 *
 * @param kernel Gabor kernel
 * @param x X position (0 to size-1)
 * @param y Y position (0 to size-1)
 * @return Kernel value, or 0.0 if out of bounds
 */
float gabor_kernel_get(const gabor_kernel_t* kernel, uint32_t x, uint32_t y);

/**
 * @brief Normalize kernel to unit sum
 *
 * WHAT: Scales kernel values so they sum to target.
 * WHY:  Ensures convolution preserves input magnitude.
 * HOW:  Divides all values by current sum.
 *
 * @param kernel Gabor kernel to normalize
 * @param target_sum Target sum (typically 1.0 or 0.0)
 * @return true on success, false on failure
 */
bool gabor_kernel_normalize(gabor_kernel_t* kernel, float target_sum);

/* ============================================================================
 * Filter Bank Functions
 * ========================================================================== */

/**
 * @brief Create a Gabor filter bank
 *
 * WHAT: Creates bank of Gabor filters at multiple orientations.
 * WHY:  Efficient V1-style multi-orientation processing.
 * HOW:  Generates kernels at evenly spaced orientations (0-180°).
 *
 * @param num_orientations Number of orientations (typically 4, 8, or 16)
 * @param kernel_size Kernel size for all filters
 * @param wavelength Wavelength for all filters
 * @param include_quadrature Whether to include odd-phase pairs
 * @return Allocated filter bank, or NULL on failure
 *
 * @note Caller must free with gabor_filter_bank_destroy()
 */
gabor_filter_bank_t* gabor_filter_bank_create(uint32_t num_orientations,
                                               uint32_t kernel_size,
                                               float wavelength,
                                               bool include_quadrature);

/**
 * @brief Destroy a Gabor filter bank
 *
 * WHAT: Frees all kernels and bank structure.
 * WHY:  Clean up resources.
 * HOW:  Iterates through kernels and frees each.
 *
 * @param bank Filter bank to destroy (may be NULL)
 */
void gabor_filter_bank_destroy(gabor_filter_bank_t* bank);

/**
 * @brief Get kernel from filter bank
 *
 * WHAT: Retrieves kernel at specific orientation index.
 * WHY:  Access individual filters from bank.
 * HOW:  Returns pointer to indexed kernel.
 *
 * @param bank Filter bank
 * @param orientation_idx Orientation index (0 to num_orientations-1)
 * @param is_odd If true and quadrature enabled, returns odd kernel
 * @return Kernel pointer, or NULL if invalid index
 */
const gabor_kernel_t* gabor_filter_bank_get_kernel(const gabor_filter_bank_t* bank,
                                                    uint32_t orientation_idx,
                                                    bool is_odd);

/* ============================================================================
 * Convolution Functions
 * ========================================================================== */

/**
 * @brief Apply Gabor kernel to image patch
 *
 * WHAT: Convolves Gabor kernel with image patch.
 * WHY:  Core operation for edge/orientation detection.
 * HOW:  Standard 2D convolution at center.
 *
 * @param kernel Gabor kernel
 * @param image Image patch data (row-major, normalized 0-1)
 * @param width Image patch width
 * @param height Image patch height
 * @return Convolution response at patch center
 */
float gabor_convolve(const gabor_kernel_t* kernel,
                     const float* image,
                     uint32_t width,
                     uint32_t height);

/**
 * @brief Compute energy model response on image patch
 *
 * WHAT: Phase-invariant response using quadrature pair.
 * WHY:  Models complex cell response.
 * HOW:  E = sqrt(even_response² + odd_response²)
 *
 * @param params Gabor parameters (or use kernel internally)
 * @param image Image patch data
 * @param width Image patch width
 * @param height Image patch height
 * @param kernel_size Kernel size for convolution
 * @return Energy model response
 */
float gabor_energy_response(const gabor_filter_params_t* params,
                            const float* image,
                            uint32_t width,
                            uint32_t height,
                            uint32_t kernel_size);

/**
 * @brief Apply filter bank to image patch
 *
 * WHAT: Computes responses for all orientations.
 * WHY:  Full orientation analysis in one call.
 * HOW:  Convolves each kernel and stores responses.
 *
 * @param bank Gabor filter bank
 * @param image Image patch data
 * @param width Image patch width
 * @param height Image patch height
 * @param responses Output array [num_orientations] or [num_orientations × 2] if quadrature
 * @return true on success, false on failure
 */
bool gabor_filter_bank_apply(const gabor_filter_bank_t* bank,
                             const float* image,
                             uint32_t width,
                             uint32_t height,
                             float* responses);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Convert degrees to radians
 *
 * @param degrees Angle in degrees
 * @return Angle in radians
 */
float gabor_deg_to_rad(float degrees);

/**
 * @brief Convert radians to degrees
 *
 * @param radians Angle in radians
 * @return Angle in degrees
 */
float gabor_rad_to_deg(float radians);

/**
 * @brief Normalize orientation to [0, 180) range
 *
 * WHAT: Wraps orientation angle to valid range.
 * WHY:  Orientations have 180° periodicity.
 * HOW:  Modulo arithmetic with wrapping.
 *
 * @param orientation Orientation in degrees
 * @return Normalized orientation [0, 180)
 */
float gabor_normalize_orientation(float orientation);

/**
 * @brief Compute angular difference with 180° periodicity
 *
 * WHAT: Shortest angular distance between orientations.
 * WHY:  Needed for tuning curve calculations.
 * HOW:  Considers wrap-around at 180°.
 *
 * @param angle1 First orientation in degrees
 * @param angle2 Second orientation in degrees
 * @return Angular difference [0, 90]
 */
float gabor_angular_difference(float angle1, float angle2);

/**
 * @brief Compute optimal kernel size for parameters
 *
 * WHAT: Calculates appropriate kernel size.
 * WHY:  Ensure kernel captures filter support.
 * HOW:  Based on sigma and multiplier constant.
 *
 * @param params Gabor filter parameters
 * @return Recommended kernel size (odd)
 */
uint32_t gabor_optimal_kernel_size(const gabor_filter_params_t* params);

/**
 * @brief Get Gabor library statistics
 *
 * WHAT: Retrieves library usage statistics.
 * WHY:  Monitoring and debugging.
 * HOW:  Returns global stats structure.
 *
 * @param stats Output statistics structure
 */
void gabor_get_stats(gabor_stats_t* stats);

/**
 * @brief Reset Gabor library statistics
 *
 * WHAT: Clears all statistics counters.
 * WHY:  Fresh start for benchmarking.
 * HOW:  Zeros global stats structure.
 */
void gabor_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GABOR_H */
