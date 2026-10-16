/**
 * @file nimcp_occipital_gpu_bridge.h
 * @brief GPU Integration Bridge for Occipital Cortex
 *
 * WHAT: Bridge layer connecting CPU occipital cortex to GPU visual kernels
 * WHY:  Enable GPU acceleration for visual processing (30-60x speedup)
 * HOW:  Routes V1-V5 processing to GPU kernels with automatic CPU fallback
 *
 * ARCHITECTURE:
 * =============
 *   Occipital Adapter (CPU) --> GPU Bridge --> GPU Visual Kernels
 *         occipital_process()                  nimcp_visual_gpu_*
 *                |                                    |
 *                v                                    v
 *   visual_input_t (CPU)  <-- Data Transfer --> nimcp_gpu_tensor_t
 *
 * PROCESSING MAPPING:
 * - V1 (edges/Gabor)    -> nimcp_gabor_bank_gpu_apply, nimcp_edge_nms_gpu
 * - V2 (contours)       -> nimcp_contour_integration_gpu
 * - V4 (color)          -> nimcp_color_opponent_gpu_process, nimcp_color_constancy_gpu
 * - V5/MT (motion)      -> nimcp_optical_flow_gpu_pyramidal, nimcp_motion_energy_gpu_process
 * - Saliency/Attention  -> nimcp_saliency_gpu_*
 *
 * USAGE:
 * ======
 * @code
 * occipital_adapter_t* occipital = occipital_create(NULL);
 * occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
 * occipital_gpu_bridge_t* bridge = occipital_gpu_bridge_create(occipital, &config);
 *
 * // Process through GPU
 * visual_processing_result_t result;
 * if (!occipital_gpu_process(bridge, &result)) {
 *     // Falls back to CPU automatically if auto_fallback is enabled
 * }
 *
 * occipital_gpu_bridge_destroy(bridge);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_OCCIPITAL_GPU_BRIDGE_H
#define NIMCP_OCCIPITAL_GPU_BRIDGE_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/perception/nimcp_visual_cortex_gpu.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "utils/thread/nimcp_thread.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GPU bridge configuration
 */
typedef struct occipital_gpu_bridge_config {
    /* Per-area GPU enable flags */
    bool enable_gpu_v1;           /**< GPU Gabor filtering and edge detection */
    bool enable_gpu_v2;           /**< GPU contour integration */
    bool enable_gpu_v4;           /**< GPU color processing */
    bool enable_gpu_v5;           /**< GPU optical flow / motion */
    bool enable_gpu_saliency;     /**< GPU attention/saliency computation */

    /* Fallback and error handling */
    bool auto_fallback;           /**< Fall back to CPU on GPU error (default: true) */
    bool report_fallbacks;        /**< Log when fallback occurs (default: true) */
    uint32_t max_consecutive_failures; /**< Max GPU failures before disabling (0=never) */

    /* GPU device configuration */
    int device_id;                /**< GPU device to use (default: 0) */
    bool create_streams;          /**< Create separate CUDA streams (default: true) */
    bool enable_async;            /**< Enable async GPU operations (default: false) */

    /* Memory management */
    bool preallocate_tensors;     /**< Preallocate GPU tensors at init (default: true) */
    bool zero_copy_when_possible; /**< Use zero-copy for pinned memory (default: false) */

    /* Processing options */
    int num_orientations;         /**< Gabor orientations (0=use adapter config) */
    int num_scales;               /**< Spatial frequency scales (0=use adapter config) */
    int pyramid_levels;           /**< Image pyramid levels (default: 4) */
} occipital_gpu_bridge_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief GPU bridge statistics
 */
typedef struct occipital_gpu_bridge_stats {
    /* Frame counts */
    uint64_t frames_processed_gpu;    /**< Frames processed on GPU */
    uint64_t frames_processed_cpu;    /**< Frames processed on CPU (fallback) */
    uint64_t gpu_failures;            /**< GPU processing failures */

    /* Per-area GPU processing counts */
    uint64_t v1_gpu_calls;            /**< V1 GPU processing calls */
    uint64_t v2_gpu_calls;            /**< V2 GPU processing calls */
    uint64_t v4_gpu_calls;            /**< V4 GPU processing calls */
    uint64_t v5_gpu_calls;            /**< V5 GPU processing calls */
    uint64_t saliency_gpu_calls;      /**< Saliency GPU calls */

    /* Timing (milliseconds) */
    float avg_gpu_time_ms;            /**< Average total GPU time */
    float avg_cpu_fallback_time_ms;   /**< Average CPU fallback time */
    float avg_upload_time_ms;         /**< Average data upload time */
    float avg_download_time_ms;       /**< Average data download time */

    /* Per-area GPU timing */
    float avg_v1_gpu_time_ms;         /**< Average V1 GPU time */
    float avg_v2_gpu_time_ms;         /**< Average V2 GPU time */
    float avg_v4_gpu_time_ms;         /**< Average V4 GPU time */
    float avg_v5_gpu_time_ms;         /**< Average V5 GPU time */
    float max_gpu_time_ms;            /**< Maximum GPU processing time */

    /* Memory */
    size_t gpu_memory_allocated;      /**< GPU memory allocated */
    size_t gpu_memory_peak;           /**< Peak GPU memory usage */

    /* Speedup tracking */
    float estimated_speedup;          /**< Estimated GPU vs CPU speedup */
} occipital_gpu_bridge_stats_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief GPU bridge internal state
 */
typedef struct occipital_gpu_bridge {
    /* Core references */
    occipital_adapter_t* occipital;       /**< CPU occipital adapter */
    nimcp_gpu_context_t* gpu_ctx;         /**< GPU context */
    nimcp_visual_gpu_state_t* visual_state; /**< GPU visual processing state */

    /* Configuration */
    occipital_gpu_bridge_config_t config; /**< Bridge configuration */

    /* Cached GPU tensors for input/output */
    nimcp_gpu_tensor_t* d_input_gray;     /**< GPU grayscale input [H, W] */
    nimcp_gpu_tensor_t* d_input_rgb;      /**< GPU RGB input [3, H, W] */
    nimcp_gpu_tensor_t* d_prev_frame;     /**< Previous frame for motion [H, W] */

    /* V1 output tensors */
    nimcp_gpu_tensor_t* d_v1_edges;       /**< V1 edge map [H, W] */
    nimcp_gpu_tensor_t* d_v1_orientations; /**< V1 orientation map [orientations, H, W] */
    nimcp_gpu_tensor_t* d_v1_energy;      /**< V1 energy [scales, H, W] */

    /* V2 output tensors */
    nimcp_gpu_tensor_t* d_v2_contours;    /**< V2 contour strength [H, W] */

    /* V4 output tensors */
    nimcp_gpu_tensor_t* d_v4_color;       /**< V4 opponent channels [3, H, W] */
    nimcp_gpu_tensor_t* d_v4_color_edges; /**< V4 color edges [H, W] */

    /* V5 output tensors */
    nimcp_gpu_tensor_t* d_v5_flow_u;      /**< V5 horizontal flow [H, W] */
    nimcp_gpu_tensor_t* d_v5_flow_v;      /**< V5 vertical flow [H, W] */
    nimcp_gpu_tensor_t* d_v5_motion_energy; /**< V5 motion energy [directions, H, W] */

    /* Saliency tensor */
    nimcp_gpu_tensor_t* d_saliency;       /**< Saliency map [H, W] */

    /* Image dimensions */
    uint32_t image_width;                 /**< Current image width */
    uint32_t image_height;                /**< Current image height */
    uint32_t image_channels;              /**< Current image channels */

    /* State flags */
    bool initialized;                     /**< Bridge initialized */
    bool tensors_allocated;               /**< GPU tensors allocated */
    bool has_previous_frame;              /**< Previous frame available for motion */
    bool gpu_disabled;                    /**< GPU disabled due to failures */

    /* Failure tracking */
    uint32_t consecutive_failures;        /**< Consecutive GPU failures */

    /* Statistics */
    occipital_gpu_bridge_stats_t stats;   /**< Bridge statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;                 /**< Mutex for thread safety */
} occipital_gpu_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default GPU bridge configuration
 *
 * WHAT: Returns sensible defaults for GPU bridge configuration
 * WHY:  Simplify bridge creation with common settings
 * HOW:  Enable all GPU areas, auto-fallback, device 0
 *
 * @return Default configuration
 */
NIMCP_EXPORT occipital_gpu_bridge_config_t occipital_gpu_bridge_default_config(void);

/**
 * @brief Create GPU bridge for occipital cortex
 *
 * WHAT: Create bridge connecting CPU occipital to GPU visual kernels
 * WHY:  Enable GPU-accelerated visual processing
 * HOW:  Initialize GPU context, create visual state, allocate tensors
 *
 * @param occipital CPU occipital adapter (required, must be valid)
 * @param config Bridge configuration (NULL for defaults)
 * @return GPU bridge or NULL on failure
 *
 * @note The bridge does NOT take ownership of the occipital adapter
 */
NIMCP_EXPORT occipital_gpu_bridge_t* occipital_gpu_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_gpu_bridge_config_t* config
);

/**
 * @brief Destroy GPU bridge and free resources
 *
 * WHAT: Release all GPU resources and bridge state
 * WHY:  Prevent memory leaks
 * HOW:  Destroy tensors, visual state, GPU context
 *
 * @param bridge Bridge to destroy (NULL-safe)
 *
 * @note Does NOT destroy the occipital adapter
 */
NIMCP_EXPORT void occipital_gpu_bridge_destroy(occipital_gpu_bridge_t* bridge);

/**
 * @brief Initialize bridge for specific image size
 *
 * WHAT: Allocate/reallocate GPU tensors for new image dimensions
 * WHY:  Handle dynamic image sizes efficiently
 * HOW:  Create appropriately sized GPU tensors
 *
 * @param bridge GPU bridge
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1 or 3)
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_init_size(
    occipital_gpu_bridge_t* bridge,
    uint32_t width,
    uint32_t height,
    uint32_t channels
);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear processing state without deallocating
 * WHY:  Prepare for new processing sequence
 * HOW:  Reset motion history, clear statistics, reset failure count
 *
 * @param bridge GPU bridge
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_reset(occipital_gpu_bridge_t* bridge);

//=============================================================================
// Data Transfer Functions
//=============================================================================

/**
 * @brief Upload visual input to GPU
 *
 * WHAT: Transfer CPU visual input to GPU memory
 * WHY:  Prepare data for GPU processing
 * HOW:  cudaMemcpy from visual_input_t to GPU tensor
 *
 * @param bridge GPU bridge
 * @param input Visual input from occipital adapter
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_upload_input(
    occipital_gpu_bridge_t* bridge,
    const visual_input_t* input
);

/**
 * @brief Download GPU results to visual features
 *
 * WHAT: Transfer GPU processing results to CPU memory
 * WHY:  Make results available to downstream CPU processing
 * HOW:  Extract features from GPU tensors, populate CPU arrays
 *
 * @param bridge GPU bridge
 * @param features Output array of visual features
 * @param max_features Maximum features to extract
 * @param num_features Output: actual number of features extracted
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_download_features(
    occipital_gpu_bridge_t* bridge,
    visual_feature_t* features,
    uint32_t max_features,
    uint32_t* num_features
);

/**
 * @brief Download motion vectors from GPU
 *
 * @param bridge GPU bridge
 * @param vectors Output motion vector array
 * @param max_vectors Maximum vectors to extract
 * @param num_vectors Output: actual number extracted
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_download_motion(
    occipital_gpu_bridge_t* bridge,
    motion_vector_t* vectors,
    uint32_t max_vectors,
    uint32_t* num_vectors
);

//=============================================================================
// GPU Processing Functions
//=============================================================================

/**
 * @brief Process V1 (primary visual cortex) on GPU
 *
 * WHAT: GPU-accelerated Gabor filtering and edge detection
 * WHY:  V1 Gabor is computationally expensive (50-100ms CPU -> 1-2ms GPU)
 * HOW:  Uses nimcp_gabor_bank_gpu_apply + edge kernels
 *
 * GPU Kernels Used:
 * - nimcp_gabor_bank_gpu_apply
 * - nimcp_gabor_energy_gpu
 * - nimcp_edge_nms_gpu
 *
 * @param bridge GPU bridge
 * @return true on success, false triggers CPU fallback if enabled
 */
NIMCP_EXPORT bool occipital_gpu_process_v1(occipital_gpu_bridge_t* bridge);

/**
 * @brief Process V2 (contour integration) on GPU
 *
 * WHAT: GPU-accelerated contour integration
 * WHY:  Association field computation benefits from parallelism
 * HOW:  Uses nimcp_contour_integration_gpu
 *
 * @param bridge GPU bridge (must have run V1 first)
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_process_v2(occipital_gpu_bridge_t* bridge);

/**
 * @brief Process V4 (color and form) on GPU
 *
 * WHAT: GPU-accelerated color opponent processing
 * WHY:  Color channel separation is embarrassingly parallel
 * HOW:  Uses nimcp_color_opponent_gpu_process
 *
 * GPU Kernels Used:
 * - nimcp_color_opponent_gpu_process
 * - nimcp_double_opponent_gpu_process
 * - nimcp_color_constancy_gpu
 *
 * @param bridge GPU bridge
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_process_v4(occipital_gpu_bridge_t* bridge);

/**
 * @brief Process V5/MT (motion) on GPU
 *
 * WHAT: GPU-accelerated optical flow and motion energy
 * WHY:  Optical flow is computationally intensive
 * HOW:  Uses pyramidal Lucas-Kanade on GPU
 *
 * GPU Kernels Used:
 * - nimcp_optical_flow_gpu_pyramidal
 * - nimcp_motion_energy_gpu_process
 *
 * @param bridge GPU bridge (requires previous frame)
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_process_v5(occipital_gpu_bridge_t* bridge);

/**
 * @brief Compute saliency map on GPU
 *
 * WHAT: GPU-accelerated visual saliency computation
 * WHY:  Attention guidance from multi-feature combination
 * HOW:  Uses Itti-Koch model on GPU
 *
 * @param bridge GPU bridge (must have run V1, V4, V5)
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_compute_saliency(occipital_gpu_bridge_t* bridge);

/**
 * @brief Full GPU visual processing pipeline
 *
 * WHAT: Complete visual processing through all areas
 * WHY:  Single call for full GPU-accelerated processing
 * HOW:  Runs V1->V2->V4->V5->Saliency with per-area fallback
 *
 * Processing Flow:
 * 1. Upload input to GPU
 * 2. Process V1 (edges) on GPU
 * 3. Process V2 (contours) on GPU
 * 4. Process V4 (color) on GPU
 * 5. Process V5 (motion) on GPU (if previous frame available)
 * 6. Compute saliency on GPU
 * 7. Download results to CPU result structure
 *
 * @param bridge GPU bridge
 * @param result Output processing result (populated on success)
 * @return true if any processing succeeded (may be partial GPU + CPU)
 */
NIMCP_EXPORT bool occipital_gpu_process(
    occipital_gpu_bridge_t* bridge,
    visual_processing_result_t* result
);

/**
 * @brief Process with explicit input (bypass occipital adapter input)
 *
 * WHAT: GPU processing with direct input specification
 * WHY:  Useful for testing or when input comes from elsewhere
 * HOW:  Same as occipital_gpu_process but with explicit input
 *
 * @param bridge GPU bridge
 * @param input Visual input
 * @param result Output processing result
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_process_input(
    occipital_gpu_bridge_t* bridge,
    const visual_input_t* input,
    visual_processing_result_t* result
);

//=============================================================================
// Status and Diagnostics
//=============================================================================

/**
 * @brief Check if GPU bridge is available and functional
 *
 * @param bridge GPU bridge
 * @return true if GPU processing is available
 */
NIMCP_EXPORT bool occipital_gpu_bridge_is_available(const occipital_gpu_bridge_t* bridge);

/**
 * @brief Check if specific visual area GPU processing is enabled
 *
 * @param bridge GPU bridge
 * @param area Visual area (0=V1, 1=V2, 2=V4, 3=V5)
 * @return true if that area's GPU processing is enabled
 */
NIMCP_EXPORT bool occipital_gpu_bridge_area_enabled(
    const occipital_gpu_bridge_t* bridge,
    int area
);

/**
 * @brief Get GPU bridge statistics
 *
 * @param bridge GPU bridge
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_get_stats(
    const occipital_gpu_bridge_t* bridge,
    occipital_gpu_bridge_stats_t* stats
);

/**
 * @brief Reset GPU bridge statistics
 *
 * @param bridge GPU bridge
 */
NIMCP_EXPORT void occipital_gpu_bridge_reset_stats(occipital_gpu_bridge_t* bridge);

/**
 * @brief Get GPU memory usage information
 *
 * @param bridge GPU bridge
 * @param used_bytes Output: memory currently allocated
 * @param peak_bytes Output: peak memory usage
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_memory_info(
    const occipital_gpu_bridge_t* bridge,
    size_t* used_bytes,
    size_t* peak_bytes
);

/**
 * @brief Re-enable GPU after it was disabled due to failures
 *
 * WHAT: Reset failure state and attempt to re-enable GPU
 * WHY:  Allow recovery from transient GPU issues
 * HOW:  Reset failure counter, clear disabled flag
 *
 * @param bridge GPU bridge
 * @return true if GPU was re-enabled successfully
 */
NIMCP_EXPORT bool occipital_gpu_bridge_reenable(occipital_gpu_bridge_t* bridge);

//=============================================================================
// Advanced Configuration
//=============================================================================

/**
 * @brief Update bridge configuration at runtime
 *
 * WHAT: Change processing options without recreating bridge
 * WHY:  Allow dynamic tuning of GPU usage
 * HOW:  Update config, may trigger tensor reallocation
 *
 * @param bridge GPU bridge
 * @param config New configuration
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_configure(
    occipital_gpu_bridge_t* bridge,
    const occipital_gpu_bridge_config_t* config
);

/**
 * @brief Enable/disable specific visual area GPU processing
 *
 * @param bridge GPU bridge
 * @param area Visual area (0=V1, 1=V2, 2=V4, 3=V5, 4=saliency)
 * @param enable Enable or disable
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_bridge_set_area_enabled(
    occipital_gpu_bridge_t* bridge,
    int area,
    bool enable
);

/**
 * @brief Get direct access to GPU tensor for advanced use
 *
 * WHAT: Get pointer to internal GPU tensor
 * WHY:  Allow direct GPU operations without download
 * HOW:  Return reference to internal tensor (do not destroy)
 *
 * @param bridge GPU bridge
 * @param tensor_name Name of tensor ("v1_edges", "v4_color", "saliency", etc.)
 * @return GPU tensor or NULL if not found
 *
 * @warning Returned tensor is owned by bridge - do not destroy
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* occipital_gpu_bridge_get_tensor(
    occipital_gpu_bridge_t* bridge,
    const char* tensor_name
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if GPU visual processing is available on this system
 *
 * WHAT: System-level GPU availability check
 * WHY:  Determine if GPU bridge can be created
 * HOW:  Check for CUDA device and driver
 *
 * @return true if GPU visual processing is available
 */
NIMCP_EXPORT bool occipital_gpu_processing_available(void);

/**
 * @brief Get GPU device information
 *
 * @param device_id GPU device ID
 * @param name Output: device name (must be at least 256 chars)
 * @param compute_capability Output: compute capability (major * 10 + minor)
 * @param memory_mb Output: device memory in MB
 * @return true on success
 */
NIMCP_EXPORT bool occipital_gpu_device_info(
    int device_id,
    char* name,
    int* compute_capability,
    size_t* memory_mb
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_GPU_BRIDGE_H */
