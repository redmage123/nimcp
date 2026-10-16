/**
 * @file nimcp_visual_cortex_gpu.h
 * @brief GPU-Accelerated Visual Cortex API (V1-V4 + MT)
 *
 * WHAT: CUDA kernels for biologically-inspired visual processing
 * WHY:  GPU acceleration for real-time visual perception (30,000+ neurons)
 * HOW:  Custom kernels for Gabor filters, image pyramids, color, motion, saliency
 *
 * BIOLOGICAL BASIS:
 * =================
 * The visual cortex processes visual information through a hierarchical pathway:
 * - V1 (Primary): Orientation-selective simple/complex cells (Gabor filters)
 * - V2 (Secondary): Contour integration and figure-ground segregation
 * - V4 (Ventral): Color processing and shape recognition
 * - MT (Dorsal): Motion processing and optical flow
 *
 * GPU ARCHITECTURE:
 * ================
 * - Each thread = one pixel or one filter response
 * - Shared memory = local convolution patches (optimized tiling)
 * - Multiple streams = parallel V1/V2/V4/MT processing
 *
 * KEY INSIGHT:
 * Visual cortex is inherently parallel - ~140M neurons in human V1 alone.
 * This maps naturally to GPU with massive parallelism per pixel.
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * nimcp_visual_gpu_state_t* state = nimcp_visual_gpu_create(ctx, 8, 4, 6);
 *
 * // Process image through visual cortex
 * nimcp_visual_features_gpu_t* features = nimcp_visual_gpu_process(state, image);
 *
 * // Access individual feature maps
 * nimcp_gpu_tensor_t* edges = features->v1_edges;
 * nimcp_gpu_tensor_t* saliency = features->saliency_map;
 *
 * nimcp_visual_features_gpu_destroy(features);
 * nimcp_visual_gpu_destroy(state);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_VISUAL_CORTEX_GPU_H
#define NIMCP_VISUAL_CORTEX_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

#define VISUAL_GPU_MAX_PYRAMID_LEVELS 8      /**< Maximum image pyramid levels */
#define VISUAL_GPU_MAX_ORIENTATIONS 16       /**< Maximum Gabor orientations */
#define VISUAL_GPU_MAX_SCALES 8              /**< Maximum spatial frequency scales */
#define VISUAL_GPU_DEFAULT_ORIENTATIONS 8    /**< Default: 0, 22.5, 45, ... degrees */
#define VISUAL_GPU_DEFAULT_SCALES 4          /**< Default spatial frequency scales */
#define VISUAL_GPU_DEFAULT_KERNEL_SIZE 9     /**< Default Gabor kernel size */
#define VISUAL_GPU_BLOCK_SIZE 16             /**< CUDA block dimension */
#define VISUAL_GPU_WARP_SIZE 32              /**< CUDA warp size */

//=============================================================================
// Gabor Filter Bank (V1 Simple Cells)
//=============================================================================

/**
 * @brief GPU Gabor filter bank for V1 simple cell responses
 *
 * BIOLOGICAL BASIS:
 * V1 simple cells are orientation and spatial-frequency tuned, well-modeled
 * by Gabor filters. Each filter is: g(x,y) = exp(-(x'^2 + gamma^2*y'^2)/(2*sigma^2)) * cos(2*pi*x'/lambda + psi)
 */
typedef struct nimcp_gabor_bank_gpu {
    nimcp_gpu_tensor_t* filters;          /**< [num_orientations, num_scales, kH, kW] */
    int num_orientations;                 /**< Number of orientations (e.g., 8) */
    int num_scales;                       /**< Number of spatial frequency scales (e.g., 4) */
    int kernel_size;                      /**< Kernel dimension (odd, e.g., 9) */
    float* orientations;                  /**< Host array of orientation angles (radians) */
    float* frequencies;                   /**< Host array of spatial frequencies (cycles/pixel) */
    float* sigmas;                        /**< Host array of Gaussian envelope sigmas */
    float gamma;                          /**< Spatial aspect ratio (typically 0.5) */
    float psi;                            /**< Phase offset (0 or pi/2) */
    nimcp_gpu_context_t* ctx;             /**< GPU context reference */
} nimcp_gabor_bank_gpu_t;

/**
 * @brief Gabor filter bank configuration
 */
typedef struct {
    int num_orientations;                 /**< Number of orientations (default: 8) */
    int num_scales;                       /**< Number of scales (default: 4) */
    int kernel_size;                      /**< Kernel size (default: 9, must be odd) */
    float min_wavelength;                 /**< Minimum wavelength in pixels (default: 3.0) */
    float max_wavelength;                 /**< Maximum wavelength in pixels (default: 15.0) */
    float gamma;                          /**< Aspect ratio (default: 0.5) */
    float sigma_factor;                   /**< Sigma = wavelength * sigma_factor (default: 0.56) */
} nimcp_gabor_config_t;

//=============================================================================
// Image Pyramid (Scale Space)
//=============================================================================

/**
 * @brief GPU image pyramid for multi-scale processing
 */
typedef struct nimcp_image_pyramid_gpu {
    nimcp_gpu_tensor_t* levels[VISUAL_GPU_MAX_PYRAMID_LEVELS]; /**< Pyramid levels */
    nimcp_gpu_tensor_t* dog_levels[VISUAL_GPU_MAX_PYRAMID_LEVELS]; /**< Difference of Gaussians */
    int num_levels;                       /**< Number of pyramid levels */
    int base_width;                       /**< Original image width */
    int base_height;                      /**< Original image height */
    float scale_factor;                   /**< Scale factor between levels (default: 0.5) */
    nimcp_gpu_context_t* ctx;             /**< GPU context reference */
} nimcp_image_pyramid_gpu_t;

//=============================================================================
// Color Processing (V4)
//=============================================================================

/**
 * @brief GPU color opponent channels
 *
 * BIOLOGICAL BASIS:
 * Retinal ganglion cells and LGN encode color as opponent channels:
 * - L+M (luminance): brightness
 * - L-M: red-green opponent
 * - S-(L+M): blue-yellow opponent
 */
typedef struct nimcp_color_opponent_gpu {
    nimcp_gpu_tensor_t* luminance;        /**< L+M channel [H, W] */
    nimcp_gpu_tensor_t* red_green;        /**< L-M opponent [H, W] */
    nimcp_gpu_tensor_t* blue_yellow;      /**< S-(L+M) opponent [H, W] */
    nimcp_gpu_tensor_t* l_cone;           /**< L cone response [H, W] */
    nimcp_gpu_tensor_t* m_cone;           /**< M cone response [H, W] */
    nimcp_gpu_tensor_t* s_cone;           /**< S cone response [H, W] */
    nimcp_gpu_context_t* ctx;             /**< GPU context reference */
} nimcp_color_opponent_gpu_t;

/**
 * @brief GPU double-opponent color processing (V4 color cells)
 *
 * Double-opponent cells have center-surround color opponency,
 * enabling color constancy and edge detection.
 */
typedef struct nimcp_double_opponent_gpu {
    nimcp_gpu_tensor_t* center_surround_rg; /**< R-G center, G-R surround [H, W] */
    nimcp_gpu_tensor_t* center_surround_by; /**< B-Y center, Y-B surround [H, W] */
    nimcp_gpu_tensor_t* color_edges;        /**< Combined color edge map [H, W] */
    int filter_size;                        /**< Center-surround filter size */
    nimcp_gpu_context_t* ctx;               /**< GPU context reference */
} nimcp_double_opponent_gpu_t;

//=============================================================================
// Motion Processing (MT/V5)
//=============================================================================

/**
 * @brief GPU motion energy model (Adelson-Bergen)
 *
 * BIOLOGICAL BASIS:
 * MT neurons are direction-selective, modeled by spatiotemporal energy.
 * Motion energy = squared sum of quadrature pair responses.
 */
typedef struct nimcp_motion_energy_gpu {
    nimcp_gpu_tensor_t* spatiotemporal_filters; /**< [num_directions, T, H_filt, W_filt] */
    nimcp_gpu_tensor_t* temporal_buffer;        /**< Circular buffer [buffer_depth, H, W] */
    nimcp_gpu_tensor_t* motion_energy;          /**< [num_directions, H, W] */
    nimcp_gpu_tensor_t* flow_u;                 /**< Horizontal flow [H, W] */
    nimcp_gpu_tensor_t* flow_v;                 /**< Vertical flow [H, W] */
    int num_directions;                         /**< Number of motion directions */
    int buffer_depth;                           /**< Temporal buffer depth */
    int current_frame;                          /**< Current frame index */
    nimcp_gpu_context_t* ctx;                   /**< GPU context reference */
} nimcp_motion_energy_gpu_t;

/**
 * @brief Optical flow configuration
 */
typedef struct {
    int window_size;                      /**< Lucas-Kanade window size (default: 5) */
    int pyramid_levels;                   /**< Number of pyramid levels (default: 3) */
    int num_iterations;                   /**< Iterations per level (default: 5) */
    float epsilon;                        /**< Convergence threshold (default: 0.01) */
} nimcp_optical_flow_config_t;

//=============================================================================
// Contour Integration (V2)
//=============================================================================

/**
 * @brief GPU association field for contour integration
 *
 * BIOLOGICAL BASIS:
 * V2 neurons integrate contour elements using association fields,
 * where collinear and cocircular edge elements reinforce each other.
 */
typedef struct nimcp_association_field_gpu {
    nimcp_gpu_tensor_t* field;            /**< [field_size, field_size, num_orientations] */
    int field_size;                       /**< Association field radius */
    int num_orientations;                 /**< Number of orientations */
    float sigma_pos;                      /**< Position falloff sigma */
    float sigma_ori;                      /**< Orientation falloff sigma */
    float curvature_weight;               /**< Cocircular vs collinear weight */
    nimcp_gpu_context_t* ctx;             /**< GPU context reference */
} nimcp_association_field_gpu_t;

//=============================================================================
// Saliency Map
//=============================================================================

/**
 * @brief GPU saliency computation state (Itti-Koch model)
 *
 * BIOLOGICAL BASIS:
 * Visual saliency emerges from competition between:
 * - Intensity (contrast)
 * - Color (opponent colors)
 * - Orientation (edge density)
 * - Motion (moving targets)
 */
typedef struct nimcp_saliency_gpu {
    nimcp_gpu_tensor_t* conspicuity_intensity;  /**< Intensity conspicuity [H, W] */
    nimcp_gpu_tensor_t* conspicuity_color;      /**< Color conspicuity [H, W] */
    nimcp_gpu_tensor_t* conspicuity_orientation; /**< Orientation conspicuity [H, W] */
    nimcp_gpu_tensor_t* conspicuity_motion;     /**< Motion conspicuity [H, W] */
    nimcp_gpu_tensor_t* saliency_map;           /**< Combined saliency [H, W] */
    nimcp_gpu_tensor_t* inhibition_of_return;   /**< IOR map for attended locations [H, W] */
    float weight_intensity;                     /**< Weight for intensity (default: 1.0) */
    float weight_color;                         /**< Weight for color (default: 1.0) */
    float weight_orientation;                   /**< Weight for orientation (default: 1.0) */
    float weight_motion;                        /**< Weight for motion (default: 1.0) */
    float ior_decay;                            /**< IOR decay rate per frame */
    nimcp_gpu_context_t* ctx;                   /**< GPU context reference */
} nimcp_saliency_gpu_t;

//=============================================================================
// Visual Cortex GPU State (Unified)
//=============================================================================

/**
 * @brief Complete GPU visual cortex state
 */
typedef struct nimcp_visual_gpu_state {
    nimcp_gpu_context_t* ctx;             /**< GPU context reference */

    // V1 processing
    nimcp_gabor_bank_gpu_t* v1_filters;   /**< V1 Gabor filter bank */
    nimcp_gpu_tensor_t* v1_simple;        /**< V1 simple cell responses [orientations, scales, H, W] */
    nimcp_gpu_tensor_t* v1_complex;       /**< V1 complex cell responses [orientations, H, W] */

    // V2 contour integration
    nimcp_association_field_gpu_t* v2_association; /**< V2 association field */
    nimcp_gpu_tensor_t* v2_contours;      /**< V2 contour strength [H, W] */
    nimcp_gpu_tensor_t* v2_texture;       /**< V2 texture boundaries [H, W] */

    // V4 color processing
    nimcp_color_opponent_gpu_t* v4_opponent; /**< V4 color opponent channels */
    nimcp_double_opponent_gpu_t* v4_double;  /**< V4 double-opponent cells */

    // MT motion processing
    nimcp_motion_energy_gpu_t* mt_motion; /**< MT motion energy */

    // Image pyramid
    nimcp_image_pyramid_gpu_t* pyramid;   /**< Multi-scale pyramid */

    // Saliency
    nimcp_saliency_gpu_t* saliency;       /**< Saliency computation */

    // Configuration
    int input_width;                      /**< Expected input width */
    int input_height;                     /**< Expected input height */
    int num_orientations;                 /**< Number of Gabor orientations */
    int num_scales;                       /**< Number of spatial frequency scales */
    int num_pyramid_levels;               /**< Number of pyramid levels */

    // Flags
    bool initialized;                     /**< Initialization flag */
    bool has_previous_frame;              /**< Previous frame available for motion */

    // CUDA streams for parallel processing
    #ifdef NIMCP_ENABLE_CUDA
    void* stream_v1;                      /**< Stream for V1 processing */
    void* stream_v4;                      /**< Stream for V4 processing */
    void* stream_mt;                      /**< Stream for MT processing */
    #endif
} nimcp_visual_gpu_state_t;

//=============================================================================
// Visual Feature Output
//=============================================================================

/**
 * @brief Visual feature output from GPU processing
 */
typedef struct nimcp_visual_features_gpu {
    nimcp_gpu_tensor_t* v1_edges;         /**< Edge/orientation map [orientations, H, W] */
    nimcp_gpu_tensor_t* v1_energy;        /**< Gabor energy [scales, H, W] */
    nimcp_gpu_tensor_t* v2_contours;      /**< Contour strength map [H, W] */
    nimcp_gpu_tensor_t* v4_color;         /**< Color features [3, H, W] (RG, BY, L) */
    nimcp_gpu_tensor_t* v4_color_edges;   /**< Color edge map [H, W] */
    nimcp_gpu_tensor_t* mt_motion;        /**< Motion vectors [2, H, W] (u, v) */
    nimcp_gpu_tensor_t* mt_motion_energy; /**< Motion energy [directions, H, W] */
    nimcp_gpu_tensor_t* saliency_map;     /**< Visual saliency [H, W] */
    nimcp_gpu_tensor_t* attended_location; /**< Current attention location [2] (x, y) */
    float attention_strength;             /**< Attention at current location */
    nimcp_visual_gpu_state_t* state;      /**< Reference to processing state */
} nimcp_visual_features_gpu_t;

//=============================================================================
// Gabor Filter Bank API
//=============================================================================

/**
 * @brief Get default Gabor filter configuration
 *
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_gabor_config_t nimcp_gabor_config_default(void);

/**
 * @brief Create GPU Gabor filter bank
 *
 * @param ctx GPU context
 * @param config Gabor configuration (NULL for defaults)
 * @return Gabor bank or NULL on failure
 */
NIMCP_EXPORT nimcp_gabor_bank_gpu_t* nimcp_gabor_bank_gpu_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gabor_config_t* config
);

/**
 * @brief Destroy Gabor filter bank
 *
 * @param bank Gabor bank to destroy
 */
NIMCP_EXPORT void nimcp_gabor_bank_gpu_destroy(nimcp_gabor_bank_gpu_t* bank);

/**
 * @brief Apply Gabor filter bank to image
 *
 * @param bank Gabor filter bank
 * @param input Input image [H, W] or [B, H, W]
 * @param output Output responses [orientations, scales, H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gabor_bank_gpu_apply(
    nimcp_gabor_bank_gpu_t* bank,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Compute Gabor energy (sum of squared even/odd responses)
 *
 * @param ctx GPU context
 * @param gabor_real Even-symmetric Gabor response
 * @param gabor_imag Odd-symmetric Gabor response
 * @param energy Output energy [orientations, H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gabor_energy_gpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* gabor_real,
    const nimcp_gpu_tensor_t* gabor_imag,
    nimcp_gpu_tensor_t* energy
);

//=============================================================================
// Image Pyramid API
//=============================================================================

/**
 * @brief Create image pyramid
 *
 * @param ctx GPU context
 * @param width Base image width
 * @param height Base image height
 * @param num_levels Number of pyramid levels
 * @param scale_factor Scale factor between levels (0.5 for half)
 * @return Pyramid or NULL on failure
 */
NIMCP_EXPORT nimcp_image_pyramid_gpu_t* nimcp_pyramid_gpu_create(
    nimcp_gpu_context_t* ctx,
    int width,
    int height,
    int num_levels,
    float scale_factor
);

/**
 * @brief Destroy image pyramid
 */
NIMCP_EXPORT void nimcp_pyramid_gpu_destroy(nimcp_image_pyramid_gpu_t* pyramid);

/**
 * @brief Build Gaussian pyramid from image
 *
 * @param pyramid Pyramid state
 * @param image Input image [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_pyramid_gpu_build(
    nimcp_image_pyramid_gpu_t* pyramid,
    const nimcp_gpu_tensor_t* image
);

/**
 * @brief Compute Difference of Gaussians (DoG) from pyramid
 *
 * @param pyramid Pyramid state (must have been built)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_pyramid_gpu_dog(nimcp_image_pyramid_gpu_t* pyramid);

//=============================================================================
// Color Processing API
//=============================================================================

/**
 * @brief Create color opponent state
 *
 * @param ctx GPU context
 * @param width Image width
 * @param height Image height
 * @return Color opponent state or NULL on failure
 */
NIMCP_EXPORT nimcp_color_opponent_gpu_t* nimcp_color_opponent_gpu_create(
    nimcp_gpu_context_t* ctx,
    int width,
    int height
);

/**
 * @brief Destroy color opponent state
 */
NIMCP_EXPORT void nimcp_color_opponent_gpu_destroy(nimcp_color_opponent_gpu_t* state);

/**
 * @brief Convert RGB to opponent color space
 *
 * @param state Color opponent state
 * @param rgb Input RGB image [3, H, W] or [H, W, 3]
 * @param channel_last true if input is [H, W, 3], false if [3, H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_color_opponent_gpu_process(
    nimcp_color_opponent_gpu_t* state,
    const nimcp_gpu_tensor_t* rgb,
    bool channel_last
);

/**
 * @brief Create double-opponent state
 */
NIMCP_EXPORT nimcp_double_opponent_gpu_t* nimcp_double_opponent_gpu_create(
    nimcp_gpu_context_t* ctx,
    int width,
    int height,
    int filter_size
);

/**
 * @brief Destroy double-opponent state
 */
NIMCP_EXPORT void nimcp_double_opponent_gpu_destroy(nimcp_double_opponent_gpu_t* state);

/**
 * @brief Compute double-opponent responses
 *
 * @param state Double-opponent state
 * @param opponent Color opponent channels
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_double_opponent_gpu_process(
    nimcp_double_opponent_gpu_t* state,
    const nimcp_color_opponent_gpu_t* opponent
);

/**
 * @brief Apply color constancy (von Kries adaptation)
 *
 * @param ctx GPU context
 * @param input Input RGB [3, H, W]
 * @param output Output corrected RGB [3, H, W]
 * @param illuminant Estimated illuminant [3]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_color_constancy_gpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* illuminant
);

//=============================================================================
// Motion Processing API
//=============================================================================

/**
 * @brief Get default optical flow configuration
 */
NIMCP_EXPORT nimcp_optical_flow_config_t nimcp_optical_flow_config_default(void);

/**
 * @brief Create motion energy state
 *
 * @param ctx GPU context
 * @param width Frame width
 * @param height Frame height
 * @param num_directions Number of motion directions (default: 8)
 * @param buffer_depth Temporal buffer depth (default: 5)
 * @return Motion energy state or NULL on failure
 */
NIMCP_EXPORT nimcp_motion_energy_gpu_t* nimcp_motion_energy_gpu_create(
    nimcp_gpu_context_t* ctx,
    int width,
    int height,
    int num_directions,
    int buffer_depth
);

/**
 * @brief Destroy motion energy state
 */
NIMCP_EXPORT void nimcp_motion_energy_gpu_destroy(nimcp_motion_energy_gpu_t* state);

/**
 * @brief Process frame for motion energy
 *
 * @param state Motion energy state
 * @param frame Current frame [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_motion_energy_gpu_process(
    nimcp_motion_energy_gpu_t* state,
    const nimcp_gpu_tensor_t* frame
);

/**
 * @brief Compute Lucas-Kanade optical flow
 *
 * @param ctx GPU context
 * @param frame_t Current frame [H, W]
 * @param frame_t1 Previous frame [H, W]
 * @param flow_u Output horizontal flow [H, W]
 * @param flow_v Output vertical flow [H, W]
 * @param config Optical flow configuration (NULL for defaults)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_optical_flow_gpu_lk(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* frame_t,
    const nimcp_gpu_tensor_t* frame_t1,
    nimcp_gpu_tensor_t* flow_u,
    nimcp_gpu_tensor_t* flow_v,
    const nimcp_optical_flow_config_t* config
);

/**
 * @brief Compute pyramidal Lucas-Kanade optical flow
 *
 * Multi-scale for large displacements.
 *
 * @param ctx GPU context
 * @param frame_t Current frame [H, W]
 * @param frame_t1 Previous frame [H, W]
 * @param flow_u Output horizontal flow [H, W]
 * @param flow_v Output vertical flow [H, W]
 * @param config Optical flow configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_optical_flow_gpu_pyramidal(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* frame_t,
    const nimcp_gpu_tensor_t* frame_t1,
    nimcp_gpu_tensor_t* flow_u,
    nimcp_gpu_tensor_t* flow_v,
    const nimcp_optical_flow_config_t* config
);

//=============================================================================
// Contour Integration API
//=============================================================================

/**
 * @brief Create association field
 *
 * @param ctx GPU context
 * @param field_size Field radius in pixels
 * @param num_orientations Number of orientations
 * @param sigma_pos Position falloff sigma
 * @param sigma_ori Orientation falloff sigma
 * @return Association field or NULL on failure
 */
NIMCP_EXPORT nimcp_association_field_gpu_t* nimcp_association_field_gpu_create(
    nimcp_gpu_context_t* ctx,
    int field_size,
    int num_orientations,
    float sigma_pos,
    float sigma_ori
);

/**
 * @brief Destroy association field
 */
NIMCP_EXPORT void nimcp_association_field_gpu_destroy(nimcp_association_field_gpu_t* field);

/**
 * @brief Apply contour integration
 *
 * @param field Association field
 * @param edge_map Edge magnitude map [H, W]
 * @param orientation_map Edge orientation map [H, W]
 * @param contour_strength Output contour strength [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_contour_integration_gpu(
    nimcp_association_field_gpu_t* field,
    const nimcp_gpu_tensor_t* edge_map,
    const nimcp_gpu_tensor_t* orientation_map,
    nimcp_gpu_tensor_t* contour_strength
);

//=============================================================================
// Saliency API
//=============================================================================

/**
 * @brief Create saliency state
 *
 * @param ctx GPU context
 * @param width Image width
 * @param height Image height
 * @return Saliency state or NULL on failure
 */
NIMCP_EXPORT nimcp_saliency_gpu_t* nimcp_saliency_gpu_create(
    nimcp_gpu_context_t* ctx,
    int width,
    int height
);

/**
 * @brief Destroy saliency state
 */
NIMCP_EXPORT void nimcp_saliency_gpu_destroy(nimcp_saliency_gpu_t* state);

/**
 * @brief Set saliency weights
 *
 * @param state Saliency state
 * @param w_intensity Intensity weight
 * @param w_color Color weight
 * @param w_orientation Orientation weight
 * @param w_motion Motion weight
 */
NIMCP_EXPORT void nimcp_saliency_gpu_set_weights(
    nimcp_saliency_gpu_t* state,
    float w_intensity,
    float w_color,
    float w_orientation,
    float w_motion
);

/**
 * @brief Compute intensity conspicuity map
 *
 * @param state Saliency state
 * @param pyramid Intensity pyramid
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_intensity(
    nimcp_saliency_gpu_t* state,
    const nimcp_image_pyramid_gpu_t* pyramid
);

/**
 * @brief Compute color conspicuity map
 *
 * @param state Saliency state
 * @param color_opponent Color opponent channels
 * @param pyramid Image pyramid (for scale processing)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_color(
    nimcp_saliency_gpu_t* state,
    const nimcp_color_opponent_gpu_t* color_opponent,
    const nimcp_image_pyramid_gpu_t* pyramid
);

/**
 * @brief Compute orientation conspicuity map
 *
 * @param state Saliency state
 * @param gabor_responses Gabor filter responses [orientations, H, W]
 * @param pyramid Image pyramid
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_orientation(
    nimcp_saliency_gpu_t* state,
    const nimcp_gpu_tensor_t* gabor_responses,
    const nimcp_image_pyramid_gpu_t* pyramid
);

/**
 * @brief Compute motion conspicuity map
 *
 * @param state Saliency state
 * @param motion Motion energy or optical flow
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_motion(
    nimcp_saliency_gpu_t* state,
    const nimcp_motion_energy_gpu_t* motion
);

/**
 * @brief Combine conspicuity maps into saliency
 *
 * @param state Saliency state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_combine(nimcp_saliency_gpu_t* state);

/**
 * @brief Find most salient location (winner-take-all)
 *
 * @param state Saliency state
 * @param x Output: x coordinate of attention
 * @param y Output: y coordinate of attention
 * @param value Output: saliency value at attention point
 * @param apply_ior Apply inhibition of return to attended location
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_winner_take_all(
    nimcp_saliency_gpu_t* state,
    int* x,
    int* y,
    float* value,
    bool apply_ior
);

/**
 * @brief Apply inhibition of return
 *
 * @param state Saliency state
 * @param x X coordinate to inhibit
 * @param y Y coordinate to inhibit
 * @param radius Inhibition radius
 * @param strength Inhibition strength
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_saliency_gpu_apply_ior(
    nimcp_saliency_gpu_t* state,
    int x,
    int y,
    float radius,
    float strength
);

//=============================================================================
// Visual Cortex Unified API
//=============================================================================

/**
 * @brief Create visual cortex GPU state
 *
 * @param ctx GPU context
 * @param num_orientations Number of Gabor orientations (0 for default 8)
 * @param num_scales Number of spatial scales (0 for default 4)
 * @param num_pyramid_levels Number of pyramid levels (0 for default 6)
 * @return Visual cortex state or NULL on failure
 */
NIMCP_EXPORT nimcp_visual_gpu_state_t* nimcp_visual_gpu_create(
    nimcp_gpu_context_t* ctx,
    int num_orientations,
    int num_scales,
    int num_pyramid_levels
);

/**
 * @brief Destroy visual cortex GPU state
 *
 * @param state State to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_visual_gpu_destroy(nimcp_visual_gpu_state_t* state);

/**
 * @brief Initialize visual cortex for specific image size
 *
 * Call this before processing if image size changes.
 *
 * @param state Visual cortex state
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_visual_gpu_init(
    nimcp_visual_gpu_state_t* state,
    int width,
    int height
);

/**
 * @brief Full visual processing pipeline
 *
 * Processes image through V1 -> V2 -> V4 -> MT -> Saliency.
 *
 * @param state Visual cortex state
 * @param image Input image [H, W, C] or [C, H, W] or [H, W]
 * @return Visual features or NULL on failure (caller must destroy)
 */
NIMCP_EXPORT nimcp_visual_features_gpu_t* nimcp_visual_gpu_process(
    nimcp_visual_gpu_state_t* state,
    const nimcp_gpu_tensor_t* image
);

/**
 * @brief Destroy visual features
 */
NIMCP_EXPORT void nimcp_visual_features_gpu_destroy(nimcp_visual_features_gpu_t* features);

/**
 * @brief Process V1 only (edge detection)
 *
 * @param state Visual cortex state
 * @param grayscale Input grayscale image [H, W]
 * @return V1 edge responses [orientations, H, W] (reference, do not destroy)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_visual_gpu_v1_process(
    nimcp_visual_gpu_state_t* state,
    const nimcp_gpu_tensor_t* grayscale
);

/**
 * @brief Compute saliency map only
 *
 * @param state Visual cortex state
 * @param image Input image [H, W, C] or [H, W]
 * @return Saliency map [H, W] (reference, do not destroy)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_visual_gpu_compute_saliency(
    nimcp_visual_gpu_state_t* state,
    const nimcp_gpu_tensor_t* image
);

/**
 * @brief Compute optical flow between frames
 *
 * @param state Visual cortex state
 * @param frame_t Current frame [H, W]
 * @param frame_t1 Previous frame [H, W]
 * @return Flow tensor [2, H, W] (u, v) (caller must destroy)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_visual_gpu_compute_optical_flow(
    nimcp_visual_gpu_state_t* state,
    const nimcp_gpu_tensor_t* frame_t,
    const nimcp_gpu_tensor_t* frame_t1
);

//=============================================================================
// Edge Detection API (V1)
//=============================================================================

/**
 * @brief Non-maximum suppression for edge thinning
 *
 * @param ctx GPU context
 * @param magnitude Edge magnitude [H, W]
 * @param orientation Edge orientation [H, W]
 * @param output Thinned edges [H, W]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_edge_nms_gpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* magnitude,
    const nimcp_gpu_tensor_t* orientation,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Orientation pooling (hypercolumn model)
 *
 * @param ctx GPU context
 * @param gabor_responses Gabor responses [orientations, H, W]
 * @param pooled Output pooled [H, W] (max orientation response)
 * @param dominant_orientation Output [H, W] (index of dominant orientation)
 * @param pool_size Spatial pooling size
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_orientation_pooling_gpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* gabor_responses,
    nimcp_gpu_tensor_t* pooled,
    nimcp_gpu_tensor_t* dominant_orientation,
    int pool_size
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Visual cortex GPU statistics
 */
typedef struct {
    uint64_t frames_processed;           /**< Total frames processed */
    uint64_t v1_operations;              /**< V1 convolution operations */
    uint64_t v4_operations;              /**< V4 color operations */
    uint64_t mt_operations;              /**< MT motion operations */
    float avg_v1_time_ms;                /**< Average V1 processing time */
    float avg_v4_time_ms;                /**< Average V4 processing time */
    float avg_mt_time_ms;                /**< Average MT processing time */
    float avg_total_time_ms;             /**< Average total processing time */
    size_t gpu_memory_used;              /**< GPU memory in use */
} nimcp_visual_gpu_stats_t;

/**
 * @brief Get visual cortex GPU statistics
 *
 * @param state Visual cortex state
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_visual_gpu_get_stats(
    const nimcp_visual_gpu_state_t* state,
    nimcp_visual_gpu_stats_t* stats
);

/**
 * @brief Reset visual cortex GPU statistics
 *
 * @param state Visual cortex state
 */
NIMCP_EXPORT void nimcp_visual_gpu_reset_stats(nimcp_visual_gpu_state_t* state);

//=============================================================================
// CPU Fallback API (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of Gabor filter
 *
 * @param kernel_size Kernel size (odd)
 * @param theta Orientation (radians)
 * @param lambda Wavelength (pixels)
 * @param sigma Envelope sigma
 * @param gamma Aspect ratio
 * @param psi Phase
 * @param output Output kernel [kernel_size, kernel_size]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gabor_cpu(
    int kernel_size,
    float theta,
    float lambda,
    float sigma,
    float gamma,
    float psi,
    float* output
);

/**
 * @brief CPU reference for 2D convolution
 *
 * @param input Input image [H, W]
 * @param kernel Convolution kernel [kH, kW]
 * @param output Output [H, W]
 * @param H Image height
 * @param W Image width
 * @param kH Kernel height
 * @param kW Kernel width
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_conv2d_cpu(
    const float* input,
    const float* kernel,
    float* output,
    int H,
    int W,
    int kH,
    int kW
);

/**
 * @brief CPU reference for Lucas-Kanade optical flow
 */
NIMCP_EXPORT bool nimcp_optical_flow_cpu_lk(
    const float* frame_t,
    const float* frame_t1,
    float* flow_u,
    float* flow_v,
    int H,
    int W,
    int window_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_CORTEX_GPU_H */
