/**
 * @file nimcp_occipital_gpu_bridge.cu
 * @brief GPU Integration Bridge for Occipital Cortex - CUDA Implementation
 *
 * WHAT: Bridge layer connecting CPU occipital cortex to GPU visual kernels
 * WHY:  Enable GPU acceleration for visual processing (30-60x speedup)
 * HOW:  Routes V1-V5 processing to GPU kernels with automatic CPU fallback
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#include <string.h>
#include <math.h>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define OCCIPITAL_GPU_LOG_MODULE "OCCIPITAL_GPU"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define GPU_BLOCK_SIZE 16
#define MAX_FEATURES_PER_AREA 256
#define FEATURE_THRESHOLD 0.1f

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Record a GPU failure and potentially disable GPU
 */
static void record_gpu_failure(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return;

    bridge->stats.gpu_failures++;
    bridge->consecutive_failures++;

    if (bridge->config.max_consecutive_failures > 0 &&
        bridge->consecutive_failures >= bridge->config.max_consecutive_failures) {
        LOG_WARNING(OCCIPITAL_GPU_LOG_MODULE,
                    "GPU disabled after %u consecutive failures",
                    bridge->consecutive_failures);
        bridge->gpu_disabled = true;
    }
}

/**
 * @brief Reset failure counter on success
 */
static void record_gpu_success(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return;
    bridge->consecutive_failures = 0;
}

/**
 * @brief Get current time in milliseconds
 */
static float get_time_ms(void) {
#ifdef NIMCP_ENABLE_CUDA
    cudaEvent_t event;
    cudaEventCreate(&event);
    cudaEventRecord(event, 0);
    cudaEventSynchronize(event);
    float ms = 0.0f;
    cudaEventDestroy(event);
    return ms;
#else
    return 0.0f;
#endif
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

occipital_gpu_bridge_config_t occipital_gpu_bridge_default_config(void) {
    occipital_gpu_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    /* Enable all GPU areas */
    config.enable_gpu_v1 = true;
    config.enable_gpu_v2 = true;
    config.enable_gpu_v4 = true;
    config.enable_gpu_v5 = true;
    config.enable_gpu_saliency = true;

    /* Fallback and error handling */
    config.auto_fallback = true;
    config.report_fallbacks = true;
    config.max_consecutive_failures = 5;

    /* GPU device configuration */
    config.device_id = 0;
    config.create_streams = true;
    config.enable_async = false;

    /* Memory management */
    config.preallocate_tensors = true;
    config.zero_copy_when_possible = false;

    /* Processing options */
    config.num_orientations = 0;  /* Use adapter config */
    config.num_scales = 0;        /* Use adapter config */
    config.pyramid_levels = 4;

    return config;
}

occipital_gpu_bridge_t* occipital_gpu_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_gpu_bridge_config_t* config) {

    if (!occipital) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Cannot create bridge: occipital adapter is NULL");
        return NULL;
    }

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "Creating occipital GPU bridge");

    /* Check GPU availability */
    if (!occipital_gpu_processing_available()) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "GPU processing not available");
        return NULL;
    }

    occipital_gpu_bridge_t* bridge = (occipital_gpu_bridge_t*)nimcp_calloc(1, sizeof(occipital_gpu_bridge_t));
    if (!bridge) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Failed to allocate bridge memory");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = occipital_gpu_bridge_default_config();
    }

    /* Store occipital reference */
    bridge->occipital = occipital;

    /* Create GPU context */
    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Creating GPU context on device %d",
              bridge->config.device_id);
    bridge->gpu_ctx = nimcp_gpu_context_create(bridge->config.device_id);
    if (!bridge->gpu_ctx) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Failed to create GPU context");
        nimcp_free(bridge);
        return NULL;
    }

    /* Get configuration from occipital adapter */
    occipital_config_t occ_config;
    occipital_get_config(occipital, &occ_config);

    int num_orientations = bridge->config.num_orientations > 0
        ? bridge->config.num_orientations
        : (int)occ_config.v1_num_orientations;
    int num_scales = bridge->config.num_scales > 0
        ? bridge->config.num_scales
        : (int)occ_config.v1_num_scales;

    /* Create GPU visual state */
    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Creating visual GPU state: orientations=%d, scales=%d",
              num_orientations, num_scales);
    bridge->visual_state = nimcp_visual_gpu_create(
        bridge->gpu_ctx,
        num_orientations,
        num_scales,
        bridge->config.pyramid_levels
    );
    if (!bridge->visual_state) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Failed to create visual GPU state");
        nimcp_gpu_context_destroy(bridge->gpu_ctx);
        nimcp_free(bridge);
        return NULL;
    }

    /* Create mutex for thread safety */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_NORMAL };
    bridge->mutex = nimcp_mutex_create(&mutex_attr);
    if (!bridge->mutex) {
        LOG_WARNING(OCCIPITAL_GPU_LOG_MODULE, "Failed to create mutex - bridge not thread-safe");
    }

    /* Initialize state */
    bridge->initialized = true;
    bridge->tensors_allocated = false;
    bridge->has_previous_frame = false;
    bridge->gpu_disabled = false;
    bridge->consecutive_failures = 0;

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "Occipital GPU bridge created successfully");
    return bridge;
}

void occipital_gpu_bridge_destroy(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return;

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "Destroying occipital GPU bridge");

    /* Lock for cleanup */
    if (bridge->mutex) {
        nimcp_mutex_lock(bridge->mutex);
    }

    /* Destroy GPU tensors */
    if (bridge->d_input_gray) nimcp_gpu_tensor_destroy(bridge->d_input_gray);
    if (bridge->d_input_rgb) nimcp_gpu_tensor_destroy(bridge->d_input_rgb);
    if (bridge->d_prev_frame) nimcp_gpu_tensor_destroy(bridge->d_prev_frame);
    if (bridge->d_v1_edges) nimcp_gpu_tensor_destroy(bridge->d_v1_edges);
    if (bridge->d_v1_orientations) nimcp_gpu_tensor_destroy(bridge->d_v1_orientations);
    if (bridge->d_v1_energy) nimcp_gpu_tensor_destroy(bridge->d_v1_energy);
    if (bridge->d_v2_contours) nimcp_gpu_tensor_destroy(bridge->d_v2_contours);
    if (bridge->d_v4_color) nimcp_gpu_tensor_destroy(bridge->d_v4_color);
    if (bridge->d_v4_color_edges) nimcp_gpu_tensor_destroy(bridge->d_v4_color_edges);
    if (bridge->d_v5_flow_u) nimcp_gpu_tensor_destroy(bridge->d_v5_flow_u);
    if (bridge->d_v5_flow_v) nimcp_gpu_tensor_destroy(bridge->d_v5_flow_v);
    if (bridge->d_v5_motion_energy) nimcp_gpu_tensor_destroy(bridge->d_v5_motion_energy);
    if (bridge->d_saliency) nimcp_gpu_tensor_destroy(bridge->d_saliency);

    /* Destroy visual GPU state */
    if (bridge->visual_state) {
        nimcp_visual_gpu_destroy(bridge->visual_state);
    }

    /* Destroy GPU context */
    if (bridge->gpu_ctx) {
        nimcp_gpu_context_destroy(bridge->gpu_ctx);
    }

    /* Unlock and destroy mutex */
    if (bridge->mutex) {
        nimcp_mutex_unlock(bridge->mutex);
        nimcp_mutex_free(bridge->mutex);
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Occipital GPU bridge destroyed");
    nimcp_free(bridge);
}

bool occipital_gpu_bridge_init_size(
    occipital_gpu_bridge_t* bridge,
    uint32_t width,
    uint32_t height,
    uint32_t channels) {

    if (!bridge || !bridge->gpu_ctx) {
        return false;
    }

    /* Check if reallocation needed */
    if (bridge->tensors_allocated &&
        bridge->image_width == width &&
        bridge->image_height == height &&
        bridge->image_channels == channels) {
        return true;  /* Already initialized with same size */
    }

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "Initializing GPU tensors for %ux%ux%u",
             width, height, channels);

    if (bridge->mutex) nimcp_mutex_lock(bridge->mutex);

    /* Initialize visual GPU state for this size */
    if (!nimcp_visual_gpu_init(bridge->visual_state, (int)width, (int)height)) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Failed to initialize visual GPU state");
        if (bridge->mutex) nimcp_mutex_unlock(bridge->mutex);
        return false;
    }

    /* Destroy existing tensors if any */
    if (bridge->d_input_gray) nimcp_gpu_tensor_destroy(bridge->d_input_gray);
    if (bridge->d_input_rgb) nimcp_gpu_tensor_destroy(bridge->d_input_rgb);
    if (bridge->d_prev_frame) nimcp_gpu_tensor_destroy(bridge->d_prev_frame);
    if (bridge->d_v1_edges) nimcp_gpu_tensor_destroy(bridge->d_v1_edges);
    if (bridge->d_v2_contours) nimcp_gpu_tensor_destroy(bridge->d_v2_contours);
    if (bridge->d_v4_color) nimcp_gpu_tensor_destroy(bridge->d_v4_color);
    if (bridge->d_v5_flow_u) nimcp_gpu_tensor_destroy(bridge->d_v5_flow_u);
    if (bridge->d_v5_flow_v) nimcp_gpu_tensor_destroy(bridge->d_v5_flow_v);
    if (bridge->d_saliency) nimcp_gpu_tensor_destroy(bridge->d_saliency);

    /* Allocate new tensors */
    size_t dims_2d[] = { (size_t)height, (size_t)width };
    size_t dims_3d[] = { 3, (size_t)height, (size_t)width };

    bridge->d_input_gray = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                    NIMCP_GPU_PRECISION_FP32);
    bridge->d_input_rgb = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_3d, 3,
                                                   NIMCP_GPU_PRECISION_FP32);
    bridge->d_prev_frame = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                    NIMCP_GPU_PRECISION_FP32);
    bridge->d_v1_edges = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                  NIMCP_GPU_PRECISION_FP32);
    bridge->d_v2_contours = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                     NIMCP_GPU_PRECISION_FP32);
    bridge->d_v4_color = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_3d, 3,
                                                  NIMCP_GPU_PRECISION_FP32);
    bridge->d_v5_flow_u = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                   NIMCP_GPU_PRECISION_FP32);
    bridge->d_v5_flow_v = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                   NIMCP_GPU_PRECISION_FP32);
    bridge->d_saliency = nimcp_gpu_tensor_create(bridge->gpu_ctx, dims_2d, 2,
                                                  NIMCP_GPU_PRECISION_FP32);

    /* Check allocation success */
    bool success = bridge->d_input_gray && bridge->d_input_rgb &&
                   bridge->d_prev_frame && bridge->d_v1_edges &&
                   bridge->d_v2_contours && bridge->d_v4_color &&
                   bridge->d_v5_flow_u && bridge->d_v5_flow_v &&
                   bridge->d_saliency;

    if (success) {
        bridge->image_width = width;
        bridge->image_height = height;
        bridge->image_channels = channels;
        bridge->tensors_allocated = true;
        bridge->has_previous_frame = false;

        LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "GPU tensors allocated successfully");
    } else {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "Failed to allocate GPU tensors");
    }

    if (bridge->mutex) nimcp_mutex_unlock(bridge->mutex);
    return success;
}

bool occipital_gpu_bridge_reset(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return false;

    if (bridge->mutex) nimcp_mutex_lock(bridge->mutex);

    bridge->has_previous_frame = false;
    bridge->consecutive_failures = 0;
    bridge->gpu_disabled = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge->mutex) nimcp_mutex_unlock(bridge->mutex);

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Bridge reset complete");
    return true;
}

/*=============================================================================
 * DATA TRANSFER FUNCTIONS
 *===========================================================================*/

bool occipital_gpu_upload_input(
    occipital_gpu_bridge_t* bridge,
    const visual_input_t* input) {

    if (!bridge || !input || !input->data) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    /* Initialize tensors for this size if needed */
    if (!occipital_gpu_bridge_init_size(bridge, input->width, input->height, input->channels)) {
        return false;
    }

#ifdef NIMCP_ENABLE_CUDA
    size_t size = (size_t)input->width * input->height * sizeof(float);

    if (input->channels == 1) {
        /* Grayscale input */
        NIMCP_CUDA_RECOVER(cudaMemcpy(bridge->d_input_gray->data, input->data,
                                      size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    } else if (input->channels >= 3) {
        /* RGB input - upload to RGB tensor */
        size_t rgb_size = size * 3;
        NIMCP_CUDA_RECOVER(cudaMemcpy(bridge->d_input_rgb->data, input->data,
                                      rgb_size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

        /* Also create grayscale version for V1 processing */
        /* For now, we'll do this on CPU - could add a GPU kernel */
        float* gray_data = (float*)nimcp_malloc(size);
        if (gray_data) {
            for (uint32_t i = 0; i < input->width * input->height; i++) {
                gray_data[i] = (input->data[i] +
                               input->data[input->width * input->height + i] +
                               input->data[2 * input->width * input->height + i]) / 3.0f;
            }
            NIMCP_CUDA_RECOVER(cudaMemcpy(bridge->d_input_gray->data, gray_data, size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
            nimcp_free(gray_data);
        }
    }

    return true;
#else
    (void)bridge;
    (void)input;
    return false;
#endif
}

bool occipital_gpu_download_features(
    occipital_gpu_bridge_t* bridge,
    visual_feature_t* features,
    uint32_t max_features,
    uint32_t* num_features) {

    if (!bridge || !features || !num_features) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    *num_features = 0;

#ifdef NIMCP_ENABLE_CUDA
    if (!bridge->d_v1_edges || !bridge->tensors_allocated) {
        return false;
    }

    /* Download edge map from GPU */
    size_t size = bridge->image_width * bridge->image_height * sizeof(float);
    float* edge_data = (float*)nimcp_malloc(size);
    if (!edge_data) return false;

    NIMCP_CUDA_RECOVER(cudaMemcpy(edge_data, bridge->d_v1_edges->data,
                                  size, cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    /* Extract features from edge map */
    uint32_t count = 0;
    uint32_t step = 4;  /* Skip pixels for performance */

    for (uint32_t y = step; y < bridge->image_height - step && count < max_features; y += step) {
        for (uint32_t x = step; x < bridge->image_width - step && count < max_features; x += step) {
            float edge_val = edge_data[y * bridge->image_width + x];
            if (edge_val > FEATURE_THRESHOLD) {
                visual_feature_t* f = &features[count];
                f->type = VISUAL_FEATURE_EDGE;
                f->source_area = VISUAL_AREA_V1;
                f->x = (float)x / (float)bridge->image_width;
                f->y = (float)y / (float)bridge->image_height;
                f->scale = 1.0f;
                f->orientation = 0.0f;  /* Would need orientation map */
                f->strength = edge_val;
                f->descriptor = NULL;
                f->descriptor_size = 0;
                count++;
            }
        }
    }

    nimcp_free(edge_data);
    *num_features = count;
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_download_motion(
    occipital_gpu_bridge_t* bridge,
    motion_vector_t* vectors,
    uint32_t max_vectors,
    uint32_t* num_vectors) {

    if (!bridge || !vectors || !num_vectors) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    *num_vectors = 0;

#ifdef NIMCP_ENABLE_CUDA
    if (!bridge->d_v5_flow_u || !bridge->d_v5_flow_v || !bridge->tensors_allocated) {
        return false;
    }

    size_t size = bridge->image_width * bridge->image_height * sizeof(float);
    float* flow_u = (float*)nimcp_malloc(size);
    float* flow_v = (float*)nimcp_malloc(size);

    if (!flow_u || !flow_v) {
        if (flow_u) nimcp_free(flow_u);
        if (flow_v) nimcp_free(flow_v);
        return false;
    }

    NIMCP_CUDA_RECOVER(cudaMemcpy(flow_u, bridge->d_v5_flow_u->data, size, cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(flow_v, bridge->d_v5_flow_v->data, size, cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    /* Extract motion vectors from flow field */
    uint32_t count = 0;
    uint32_t step = 16;
    float motion_threshold = 0.5f;

    for (uint32_t y = step; y < bridge->image_height - step && count < max_vectors; y += step) {
        for (uint32_t x = step; x < bridge->image_width - step && count < max_vectors; x += step) {
            size_t idx = y * bridge->image_width + x;
            float u = flow_u[idx];
            float v = flow_v[idx];
            float mag = sqrtf(u * u + v * v);

            if (mag > motion_threshold) {
                motion_vector_t* mv = &vectors[count];
                mv->x = (float)x / (float)bridge->image_width;
                mv->y = (float)y / (float)bridge->image_height;
                mv->dx = u;
                mv->dy = v;
                mv->confidence = fminf(mag / 10.0f, 1.0f);
                count++;
            }
        }
    }

    nimcp_free(flow_u);
    nimcp_free(flow_v);
    *num_vectors = count;
    return true;
#else
    return false;
#endif
}

/*=============================================================================
 * GPU PROCESSING FUNCTIONS
 *===========================================================================*/

bool occipital_gpu_process_v1(occipital_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->tensors_allocated || bridge->gpu_disabled) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!bridge->config.enable_gpu_v1) {
        return false;
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Processing V1 on GPU");
    bridge->stats.v1_gpu_calls++;

#ifdef NIMCP_ENABLE_CUDA
    /* Use GPU visual cortex V1 processing */
    nimcp_gpu_tensor_t* v1_output = nimcp_visual_gpu_v1_process(
        bridge->visual_state,
        bridge->d_input_gray
    );

    if (!v1_output) {
        LOG_WARNING(OCCIPITAL_GPU_LOG_MODULE, "V1 GPU processing failed");
        record_gpu_failure(bridge);
        return false;
    }

    /* Copy result to our edge tensor */
    nimcp_gpu_copy(bridge->gpu_ctx, v1_output, bridge->d_v1_edges);

    record_gpu_success(bridge);
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_process_v2(occipital_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->tensors_allocated || bridge->gpu_disabled) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!bridge->config.enable_gpu_v2) {
        return false;
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Processing V2 on GPU");
    bridge->stats.v2_gpu_calls++;

#ifdef NIMCP_ENABLE_CUDA
    /* V2 requires V1 output (edges and orientations) */
    if (!bridge->visual_state->v2_association) {
        LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "V2 association field not available");
        return false;
    }

    /* Contour integration using association field */
    bool success = nimcp_contour_integration_gpu(
        bridge->visual_state->v2_association,
        bridge->d_v1_edges,
        bridge->d_v1_edges,  /* Use edge map as orientation approximation */
        bridge->d_v2_contours
    );

    if (!success) {
        record_gpu_failure(bridge);
        return false;
    }

    record_gpu_success(bridge);
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_process_v4(occipital_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->tensors_allocated || bridge->gpu_disabled) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!bridge->config.enable_gpu_v4) {
        return false;
    }

    if (bridge->image_channels < 3) {
        LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "V4 requires RGB input");
        return false;
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Processing V4 on GPU");
    bridge->stats.v4_gpu_calls++;

#ifdef NIMCP_ENABLE_CUDA
    /* Process color opponent channels */
    if (!bridge->visual_state->v4_opponent) {
        LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "V4 color opponent state not available");
        return false;
    }

    bool success = nimcp_color_opponent_gpu_process(
        bridge->visual_state->v4_opponent,
        bridge->d_input_rgb,
        false  /* CHW format */
    );

    if (!success) {
        record_gpu_failure(bridge);
        return false;
    }

    /* Process double-opponent cells if available */
    if (bridge->visual_state->v4_double) {
        nimcp_double_opponent_gpu_process(
            bridge->visual_state->v4_double,
            bridge->visual_state->v4_opponent
        );
    }

    record_gpu_success(bridge);
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_process_v5(occipital_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->tensors_allocated || bridge->gpu_disabled) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!bridge->config.enable_gpu_v5) {
        return false;
    }

    if (!bridge->has_previous_frame) {
        /* Store current frame as previous for next iteration */
#ifdef NIMCP_ENABLE_CUDA
        nimcp_gpu_copy(bridge->gpu_ctx, bridge->d_input_gray, bridge->d_prev_frame);
#endif
        bridge->has_previous_frame = true;
        LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "V5: First frame stored, waiting for next");
        return true;
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Processing V5 on GPU");
    bridge->stats.v5_gpu_calls++;

#ifdef NIMCP_ENABLE_CUDA
    /* Compute optical flow */
    nimcp_optical_flow_config_t flow_config = nimcp_optical_flow_config_default();

    bool success = nimcp_optical_flow_gpu_pyramidal(
        bridge->gpu_ctx,
        bridge->d_input_gray,
        bridge->d_prev_frame,
        bridge->d_v5_flow_u,
        bridge->d_v5_flow_v,
        &flow_config
    );

    /* Store current frame for next iteration */
    nimcp_gpu_copy(bridge->gpu_ctx, bridge->d_input_gray, bridge->d_prev_frame);

    if (!success) {
        record_gpu_failure(bridge);
        return false;
    }

    /* Also compute motion energy if available */
    if (bridge->visual_state->mt_motion) {
        nimcp_motion_energy_gpu_process(
            bridge->visual_state->mt_motion,
            bridge->d_input_gray
        );
    }

    record_gpu_success(bridge);
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_compute_saliency(occipital_gpu_bridge_t* bridge) {
    if (!bridge || !bridge->tensors_allocated || bridge->gpu_disabled) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!bridge->config.enable_gpu_saliency) {
        return false;
    }

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Computing saliency on GPU");
    bridge->stats.saliency_gpu_calls++;

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_tensor_t* saliency = nimcp_visual_gpu_compute_saliency(
        bridge->visual_state,
        bridge->d_input_gray  /* or RGB if available */
    );

    if (!saliency) {
        record_gpu_failure(bridge);
        return false;
    }

    /* Copy to our saliency tensor */
    nimcp_gpu_copy(bridge->gpu_ctx, saliency, bridge->d_saliency);

    record_gpu_success(bridge);
    return true;
#else
    return false;
#endif
}

bool occipital_gpu_process(
    occipital_gpu_bridge_t* bridge,
    visual_processing_result_t* result) {

    if (!bridge || !result) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    memset(result, 0, sizeof(*result));

    bool any_success = false;

    /* V1 Processing */
    if (occipital_gpu_process_v1(bridge)) {
        result->v1_processed = true;
        any_success = true;
        bridge->stats.frames_processed_gpu++;
    } else if (bridge->config.auto_fallback) {
        /* Fall back to CPU */
        if (bridge->config.report_fallbacks) {
            LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "V1 falling back to CPU");
        }
        if (occipital_process_v1(bridge->occipital)) {
            result->v1_processed = true;
            bridge->stats.frames_processed_cpu++;
        }
    }

    /* V2 Processing (depends on V1) */
    if (result->v1_processed) {
        if (occipital_gpu_process_v2(bridge)) {
            result->v2_processed = true;
        } else if (bridge->config.auto_fallback) {
            if (occipital_process_v2(bridge->occipital)) {
                result->v2_processed = true;
            }
        }
    }

    /* V4 Processing (color) */
    if (bridge->image_channels >= 3) {
        if (occipital_gpu_process_v4(bridge)) {
            result->v4_processed = true;
        } else if (bridge->config.auto_fallback) {
            if (occipital_process_v4(bridge->occipital)) {
                result->v4_processed = true;
            }
        }
    }

    /* V5 Processing (motion) */
    if (occipital_gpu_process_v5(bridge)) {
        result->v5_processed = true;
    } else if (bridge->config.auto_fallback) {
        if (occipital_process_v5(bridge->occipital)) {
            result->v5_processed = true;
        }
    }

    /* Saliency */
    occipital_gpu_compute_saliency(bridge);

    /* Extract feature counts */
    visual_feature_t features[MAX_FEATURES_PER_AREA];
    uint32_t num_features = MAX_FEATURES_PER_AREA;
    if (occipital_gpu_download_features(bridge, features, MAX_FEATURES_PER_AREA, &num_features)) {
        result->edge_count = num_features;
    }

    motion_vector_t motions[128];
    uint32_t num_motions = 128;
    if (occipital_gpu_download_motion(bridge, motions, 128, &num_motions)) {
        result->motion_vector_count = num_motions;
    }

    result->total_features = result->edge_count + result->motion_vector_count;
    result->ready_for_downstream = any_success;

    return any_success;
}

bool occipital_gpu_process_input(
    occipital_gpu_bridge_t* bridge,
    const visual_input_t* input,
    visual_processing_result_t* result) {

    if (!bridge || !input || !result) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    /* Upload input to GPU */
    if (!occipital_gpu_upload_input(bridge, input)) {
        return false;
    }

    /* Process */
    return occipital_gpu_process(bridge, result);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

bool occipital_gpu_bridge_is_available(const occipital_gpu_bridge_t* bridge) {
    if (!bridge) return false;
    return bridge->initialized && !bridge->gpu_disabled && bridge->gpu_ctx != NULL;
}

bool occipital_gpu_bridge_area_enabled(const occipital_gpu_bridge_t* bridge, int area) {
    if (!bridge) return false;

    switch (area) {
        case 0: return bridge->config.enable_gpu_v1;
        case 1: return bridge->config.enable_gpu_v2;
        case 2: return bridge->config.enable_gpu_v4;
        case 3: return bridge->config.enable_gpu_v5;
        case 4: return bridge->config.enable_gpu_saliency;
        default: return false;
    }
}

bool occipital_gpu_bridge_get_stats(
    const occipital_gpu_bridge_t* bridge,
    occipital_gpu_bridge_stats_t* stats) {

    if (!bridge || !stats) return false;
    *stats = bridge->stats;
    return true;
}

void occipital_gpu_bridge_reset_stats(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

bool occipital_gpu_bridge_memory_info(
    const occipital_gpu_bridge_t* bridge,
    size_t* used_bytes,
    size_t* peak_bytes) {

    if (!bridge || !used_bytes || !peak_bytes) return false;

    *used_bytes = bridge->stats.gpu_memory_allocated;
    *peak_bytes = bridge->stats.gpu_memory_peak;
    return true;
}

bool occipital_gpu_bridge_reenable(occipital_gpu_bridge_t* bridge) {
    if (!bridge) return false;

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "Attempting to re-enable GPU");

    bridge->gpu_disabled = false;
    bridge->consecutive_failures = 0;

    /* Verify GPU is still available */
    if (!occipital_gpu_processing_available()) {
        LOG_ERROR(OCCIPITAL_GPU_LOG_MODULE, "GPU still not available");
        bridge->gpu_disabled = true;
        return false;
    }

    LOG_INFO(OCCIPITAL_GPU_LOG_MODULE, "GPU re-enabled successfully");
    return true;
}

/*=============================================================================
 * ADVANCED CONFIGURATION
 *===========================================================================*/

bool occipital_gpu_bridge_configure(
    occipital_gpu_bridge_t* bridge,
    const occipital_gpu_bridge_config_t* config) {

    if (!bridge || !config) return false;

    if (bridge->mutex) nimcp_mutex_lock(bridge->mutex);

    bridge->config = *config;

    if (bridge->mutex) nimcp_mutex_unlock(bridge->mutex);

    LOG_DEBUG(OCCIPITAL_GPU_LOG_MODULE, "Configuration updated");
    return true;
}

bool occipital_gpu_bridge_set_area_enabled(
    occipital_gpu_bridge_t* bridge,
    int area,
    bool enable) {

    if (!bridge) return false;

    switch (area) {
        case 0: bridge->config.enable_gpu_v1 = enable; break;
        case 1: bridge->config.enable_gpu_v2 = enable; break;
        case 2: bridge->config.enable_gpu_v4 = enable; break;
        case 3: bridge->config.enable_gpu_v5 = enable; break;
        case 4: bridge->config.enable_gpu_saliency = enable; break;
        default: return false;
    }

    return true;
}

nimcp_gpu_tensor_t* occipital_gpu_bridge_get_tensor(
    occipital_gpu_bridge_t* bridge,
    const char* tensor_name) {

    if (!bridge || !tensor_name) return NULL;

    if (strcmp(tensor_name, "v1_edges") == 0) return bridge->d_v1_edges;
    if (strcmp(tensor_name, "v2_contours") == 0) return bridge->d_v2_contours;
    if (strcmp(tensor_name, "v4_color") == 0) return bridge->d_v4_color;
    if (strcmp(tensor_name, "v5_flow_u") == 0) return bridge->d_v5_flow_u;
    if (strcmp(tensor_name, "v5_flow_v") == 0) return bridge->d_v5_flow_v;
    if (strcmp(tensor_name, "saliency") == 0) return bridge->d_saliency;
    if (strcmp(tensor_name, "input_gray") == 0) return bridge->d_input_gray;
    if (strcmp(tensor_name, "input_rgb") == 0) return bridge->d_input_rgb;

    return NULL;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

bool occipital_gpu_processing_available(void) {
#ifdef NIMCP_ENABLE_CUDA
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_count == 0) {
        return false;
    }

    /* Check if we can actually use the device */
    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, 0);
    if (err != cudaSuccess) {
        return false;
    }

    /* Require compute capability 3.0+ */
    if (prop.major < 3) {
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool occipital_gpu_device_info(
    int device_id,
    char* name,
    int* compute_capability,
    size_t* memory_mb) {

#ifdef NIMCP_ENABLE_CUDA
    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_id);
    if (err != cudaSuccess) {
        return false;
    }

    if (name) {
        strncpy(name, prop.name, 256);
        name[255] = '\0';
    }

    if (compute_capability) {
        *compute_capability = prop.major * 10 + prop.minor;
    }

    if (memory_mb) {
        *memory_mb = prop.totalGlobalMem / (1024 * 1024);
    }

    return true;
#else
    (void)device_id;
    (void)name;
    (void)compute_capability;
    (void)memory_mb;
    return false;
#endif
}
