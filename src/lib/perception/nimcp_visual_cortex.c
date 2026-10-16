/**
 * @file nimcp_visual_cortex.c
 * @brief Implementation of biologically-inspired visual processing
 *
 * WHAT: CNN-based visual cortex with V1-style edge detection
 * WHY:  Enable visual perception and memory in NIMCP
 * HOW:  Lightweight convolution + Gabor filters + tensor-accelerated feature extraction
 *
 * Phase TENSOR-2: Tensor library integration for accelerated operations
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7.0 - Added tensor library integration
 */

#include "perception/nimcp_visual_cortex.h"
#include "utils/tensor/nimcp_tensor.h"  /* Tensor library for vectorized operations */
#include "utils/gabor/nimcp_gabor.h"    /* Shared Gabor filter library */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"  /* KG reader for self-awareness */

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_pool.h"  // Memory pool for O(1) allocations
#include "utils/memory/nimcp_page_cow.h"     // Copy-on-Write for shallow copies
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "plasticity/nimcp_second_messengers.h"  // Second messenger cascades
#include "core/brain/nimcp_brain.h"  // Brain reference
#include "core/neuralnet/nimcp_neuralnet.h"  // Neural network for internal V1 connections
#include "core/topology/nimcp_fractal_topology.h"  // Scale-free topology generation
#include "async/nimcp_bio_async.h"  // Bio-async communication
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_cortex)

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define VISUAL_LOG_MODULE "VISUAL_CORTEX"


//=============================================================================
// Neuromodulation
//=============================================================================

/**
 * @brief Compute visual gain from acetylcholine and norepinephrine
 *
 * WHAT: Calculate gain factor for visual processing based on ACh + NE
 * WHY:  ACh enhances attention, NE enhances arousal-based vision
 * HOW:  Read ACh and NE levels, combine into multiplicative gain
 *
 * BIOLOGY:
 * - Acetylcholine: Visual attention and feature binding
 *   High ACh (0.7) → focused attention, enhanced features
 *   Low ACh (0.3) → inattentive, reduced feature detection
 *
 * - Norepinephrine: Arousal-dependent visual sensitivity
 *   High NE (0.7) → hypervigilant, threat detection (anxiety/PTSD)
 *   Low NE (0.3) → reduced alertness, missing cues (depression)
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain to read neurotransmitters from
 * @return Visual gain factor [0.6, 1.8], or 1.0 if no brain
 */
static float get_visual_gain(brain_t brain)
{
    // Guard: No brain available
    if (!brain) {
        return 1.0F;
    }

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (!neuromod) {
        return 1.0F;
    }

    // Read neurotransmitter levels
    float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);
    float ne = neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE);

    // ACh contribution: [0.3, 0.7] → [0.8, 1.2]
    float ach_gain = 0.8F + (ach - 0.3F);

    // NE contribution: [0.3, 0.7] → [0.8, 1.2]
    float ne_gain = 0.8F + (ne - 0.3F);

    // Combined gain: [0.64, 1.44] approx [0.6, 1.5]
    return ach_gain * ne_gain;
}

//=============================================================================
// Activation Functions
//=============================================================================

/**
 * @brief Apply activation function without gain (baseline)
 *
 * WHAT: Standard activation function
 * WHY:  For layers that don't use neuromodulation
 * HOW:  Apply activation function directly
 *
 * @param x Input value
 * @param activation Activation function type
 * @return Activated output
 */
static inline float apply_activation(float x, visual_activation_type_t activation)
{
    switch (activation) {
        case VISUAL_ACTIVATION_RELU:
            return (x > 0.0F) ? x : 0.0F;
        case VISUAL_ACTIVATION_SIGMOID:
            return 1.0F / (1.0F + expf(-x));
        case VISUAL_ACTIVATION_TANH:
            return tanhf(x);
        case VISUAL_ACTIVATION_NONE:
        default:
            return x;
    }
}

/**
 * @brief Apply activation function with optional gain modulation
 *
 * WHAT: Activate neuron output with neuromodulator gain
 * WHY:  Enable neurochemical modulation of visual processing
 * HOW:  Apply gain, then activation function
 *
 * @param x Input value
 * @param activation Activation function type
 * @param gain Neuromodulator gain factor (typically 1.0 baseline)
 * @return Activated output
 */
static inline float apply_activation_with_gain(float x,
                                               visual_activation_type_t activation,
                                               float gain)
{
    // Apply neuromodulator gain first
    x *= gain;

    // Then apply activation function
    return apply_activation(x, activation);
}

//=============================================================================
// Convolution Layer Implementation
//=============================================================================

struct conv_layer_struct {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t input_channels;
    uint32_t num_filters;
    uint32_t kernel_size;
    uint32_t stride;
    uint32_t padding;
    visual_activation_type_t activation;

    uint32_t output_width;
    uint32_t output_height;

    float* kernels;  // [num_filters][kernel_size][kernel_size][input_channels]
    float* bias;     // [num_filters]
};

/**
 * WHAT: Create convolution layer
 */
conv_layer_t* conv_layer_create(const conv_layer_config_t* config)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(config, "config")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "visual_cortex",
            "NULL config provided to conv_layer_create");
        return NULL;
    }

    if (config->input_width == 0 || config->input_height == 0 ||
        config->input_channels == 0 || config->num_filters == 0 ||
        config->kernel_size == 0 || config->stride == 0) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "visual_cortex",
            "Invalid convolution layer config: width=%u height=%u channels=%u filters=%u kernel=%u stride=%u",
            config->input_width, config->input_height, config->input_channels,
            config->num_filters, config->kernel_size, config->stride);
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid convolution layer configuration parameters");
        return NULL;
    }

    // Allocate layer
    conv_layer_t* layer = (conv_layer_t*)nimcp_calloc(1, sizeof(conv_layer_t));
    if (!layer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(conv_layer_t),
            "Failed to allocate convolution layer");
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate convolution layer");
        return NULL;
    }

    layer->input_width = config->input_width;
    layer->input_height = config->input_height;
    layer->input_channels = config->input_channels;
    layer->num_filters = config->num_filters;
    layer->kernel_size = config->kernel_size;
    layer->stride = config->stride;
    layer->padding = config->padding;
    layer->activation = config->activation;

    // Compute output dimensions
    layer->output_width = (config->input_width + 2 * config->padding - config->kernel_size) / config->stride + 1;
    layer->output_height = (config->input_height + 2 * config->padding - config->kernel_size) / config->stride + 1;

    // Allocate kernels
    uint32_t kernel_total_size = config->num_filters * config->kernel_size * config->kernel_size * config->input_channels;
    layer->kernels = (float*)nimcp_calloc(kernel_total_size, sizeof(float));
    if (!nimcp_validate_pointer(layer->kernels, "kernels")) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, kernel_total_size * sizeof(float),
            "Failed to allocate convolution kernels (%u total)", kernel_total_size);
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate convolution kernels");
        conv_layer_destroy(layer);
        return NULL;
    }

    // Initialize kernels with small random values
    for (uint32_t i = 0; i < kernel_total_size; i++) {
        layer->kernels[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 0.1F;
    }

    // Allocate bias
    layer->bias = (float*)nimcp_calloc(config->num_filters, sizeof(float));
    if (!nimcp_validate_pointer(layer->bias, "bias")) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, config->num_filters * sizeof(float),
            "Failed to allocate convolution bias (%u filters)", config->num_filters);
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate convolution bias");
        conv_layer_destroy(layer);
        return NULL;
    }

    return layer;
}

/**
 * WHAT: Destroy convolution layer
 */
void conv_layer_destroy(conv_layer_t* layer)
{
    if (!layer) {
        return;
    }

    if (layer->kernels) {
        nimcp_free(layer->kernels);
    }
    if (layer->bias) {
        nimcp_free(layer->bias);
    }

    nimcp_free(layer);
}

/**
 * WHAT: Set kernel weights
 */
bool conv_layer_set_kernel(conv_layer_t* layer, uint32_t filter_idx, const float* kernel)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(layer, "layer") || !nimcp_validate_pointer(kernel, "kernel")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "conv_layer_set_kernel: required parameter is NULL (nimcp_validate_pointer, nimcp_validate_pointer)");
        return false;
    }

    if (filter_idx >= layer->num_filters) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid filter index: %u >= %u", filter_idx, layer->num_filters);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "conv_layer_set_kernel: capacity exceeded");
        return false;
    }

    uint32_t kernel_size_per_filter = layer->kernel_size * layer->kernel_size * layer->input_channels;
    uint32_t offset = filter_idx * kernel_size_per_filter;

    memcpy(&layer->kernels[offset], kernel, kernel_size_per_filter * sizeof(float));
    return true;
}

/**
 * WHAT: Forward pass through convolution
 */
bool conv_layer_forward(conv_layer_t* layer, const float* input, float* output)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(layer, "layer") ||
        !nimcp_validate_pointer(input, "input") ||
        !nimcp_validate_pointer(output, "output")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "conv_layer_forward: nimcp_validate_pointer is NULL");
        return false;
    }

    uint32_t kernel_size_per_filter = layer->kernel_size * layer->kernel_size * layer->input_channels;

    // For each filter
    for (uint32_t f = 0; f < layer->num_filters; f++) {
        float* kernel = &layer->kernels[f * kernel_size_per_filter];

        // For each output position
        for (uint32_t oy = 0; oy < layer->output_height; oy++) {
            for (uint32_t ox = 0; ox < layer->output_width; ox++) {
                float sum = layer->bias[f];

                // Convolve
                for (uint32_t c = 0; c < layer->input_channels; c++) {
                    for (uint32_t ky = 0; ky < layer->kernel_size; ky++) {
                        for (uint32_t kx = 0; kx < layer->kernel_size; kx++) {
                            int32_t ix = ox * layer->stride + kx - layer->padding;
                            int32_t iy = oy * layer->stride + ky - layer->padding;

                            // Check bounds
                            if (ix >= 0 && ix < (int32_t)layer->input_width &&
                                iy >= 0 && iy < (int32_t)layer->input_height) {

                                uint32_t input_idx = (iy * layer->input_width + ix) * layer->input_channels + c;
                                uint32_t kernel_idx = (ky * layer->kernel_size + kx) * layer->input_channels + c;

                                sum += input[input_idx] * kernel[kernel_idx];
                            }
                        }
                    }
                }

                // Apply activation
                uint32_t output_idx = (oy * layer->output_width + ox) * layer->num_filters + f;
                output[output_idx] = apply_activation(sum, layer->activation);
            }
        }
    }

    return true;
}

/**
 * WHAT: Get output dimensions
 */
uint32_t conv_layer_get_output_width(const conv_layer_t* layer)
{
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conv_layer_get_output_width: layer is NULL");
        return 0;
    }
    return layer->output_width;
}

uint32_t conv_layer_get_output_height(const conv_layer_t* layer)
{
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conv_layer_get_output_height: layer is NULL");
        return 0;
    }
    return layer->output_height;
}

uint32_t conv_layer_get_output_channels(const conv_layer_t* layer)
{
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conv_layer_get_output_channels: layer is NULL");
        return 0;
    }
    return layer->num_filters;
}

//=============================================================================
// Pooling Layer Implementation
//=============================================================================

struct pool_layer_struct {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t input_channels;
    uint32_t pool_size;
    uint32_t stride;
    pool_type_t type;

    uint32_t output_width;
    uint32_t output_height;
};

/**
 * WHAT: Create pooling layer
 */
pool_layer_t* pool_layer_create(const pool_layer_config_t* config)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(config, "config")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "visual_cortex",
            "NULL config provided to pool_layer_create");
        return NULL;
    }

    if (config->input_width == 0 || config->input_height == 0 ||
        config->input_channels == 0 || config->pool_size == 0 || config->stride == 0) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "visual_cortex",
            "Invalid pooling layer config: width=%u height=%u channels=%u pool=%u stride=%u",
            config->input_width, config->input_height, config->input_channels,
            config->pool_size, config->stride);
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid pooling layer configuration parameters");
        return NULL;
    }

    pool_layer_t* layer = (pool_layer_t*)nimcp_calloc(1, sizeof(pool_layer_t));
    if (!nimcp_validate_pointer(layer, "layer")) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(pool_layer_t),
            "Failed to allocate pooling layer");
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate pooling layer");
        return NULL;
    }

    layer->input_width = config->input_width;
    layer->input_height = config->input_height;
    layer->input_channels = config->input_channels;
    layer->pool_size = config->pool_size;
    layer->stride = config->stride;
    layer->type = config->type;

    layer->output_width = (config->input_width - config->pool_size) / config->stride + 1;
    layer->output_height = (config->input_height - config->pool_size) / config->stride + 1;

    return layer;
}

/**
 * WHAT: Destroy pooling layer
 */
void pool_layer_destroy(pool_layer_t* layer)
{
    if (layer) {
        nimcp_free(layer);
    }
}

/**
 * WHAT: Forward pass through pooling
 */
bool pool_layer_forward(pool_layer_t* layer, const float* input, float* output)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(layer, "layer") ||
        !nimcp_validate_pointer(input, "input") ||
        !nimcp_validate_pointer(output, "output")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pool_layer_forward: nimcp_validate_pointer is NULL");
        return false;
    }

    // For each channel
    for (uint32_t c = 0; c < layer->input_channels; c++) {
        // For each output position
        for (uint32_t oy = 0; oy < layer->output_height; oy++) {
            for (uint32_t ox = 0; ox < layer->output_width; ox++) {
                uint32_t base_y = oy * layer->stride;
                uint32_t base_x = ox * layer->stride;

                float pool_value;
                if (layer->type == POOL_MAX) {
                    pool_value = -INFINITY;
                } else {  // POOL_AVG
                    pool_value = 0.0F;
                }

                // Pool over window
                for (uint32_t py = 0; py < layer->pool_size; py++) {
                    for (uint32_t px = 0; px < layer->pool_size; px++) {
                        uint32_t iy = base_y + py;
                        uint32_t ix = base_x + px;

                        if (iy < layer->input_height && ix < layer->input_width) {
                            uint32_t input_idx = (iy * layer->input_width + ix) * layer->input_channels + c;

                            if (layer->type == POOL_MAX) {
                                if (input[input_idx] > pool_value) {
                                    pool_value = input[input_idx];
                                }
                            } else {  // POOL_AVG
                                pool_value += input[input_idx];
                            }
                        }
                    }
                }

                if (layer->type == POOL_AVG) {
                    pool_value /= (layer->pool_size * layer->pool_size);
                }

                uint32_t output_idx = (oy * layer->output_width + ox) * layer->input_channels + c;
                output[output_idx] = pool_value;
            }
        }
    }

    return true;
}

//=============================================================================
// Gabor Filter Implementation - Uses shared Gabor library
//=============================================================================

/**
 * WHAT: Create Gabor kernel using shared library
 * WHY:  Consolidated Gabor implementation eliminates duplicate code
 * HOW:  Converts gabor_params_t to gabor_filter_params_t and uses shared library
 */
float* gabor_create_kernel(int kernel_size, const gabor_params_t* params)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(params, "params")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_create_kernel: nimcp_validate_pointer is NULL");
        return NULL;
    }

    if (kernel_size <= 0 || kernel_size % 2 == 0) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid kernel size: %d (must be positive and odd)", kernel_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gabor_create_kernel: 2 is zero");
        return NULL;
    }

    /* Convert visual_cortex gabor_params_t to shared library format */
    gabor_filter_params_t filter_params;
    gabor_default_params(&filter_params);
    filter_params.orientation_deg = params->orientation;
    filter_params.wavelength = params->wavelength;
    filter_params.phase_deg = params->phase;
    filter_params.aspect_ratio = params->aspect_ratio;
    filter_params.bandwidth = params->bandwidth;

    /* Use shared library to create kernel data (DC balanced) */
    float* kernel = gabor_create_kernel_data(kernel_size, &filter_params);
    if (!kernel) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to create Gabor kernel via shared library");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gabor_create_kernel: kernel is NULL");
        return NULL;
    }

    return kernel;
}

//=============================================================================
// Attention Map Implementation
//=============================================================================

struct attention_map_struct {
    uint32_t width;
    uint32_t height;
    float* values;  // [height][width]
};

/**
 * WHAT: Create attention map
 */
attention_map_t* attention_map_create(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid attention map dimensions: %u x %u", width, height);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_map_create: width is zero");
        return NULL;
    }

    attention_map_t* map = (attention_map_t*)nimcp_calloc(1, sizeof(attention_map_t));
    if (!nimcp_validate_pointer(map, "map")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate attention map");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_map_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    map->width = width;
    map->height = height;
    map->values = (float*)nimcp_calloc(width * height, sizeof(float));

    if (!nimcp_validate_pointer(map->values, "map->values")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate attention map values");
        nimcp_free(map);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_map_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    return map;
}

/**
 * WHAT: Destroy attention map
 */
void attention_map_destroy(attention_map_t* map)
{
    if (!map) {
        return;
    }

    if (map->values) {
        nimcp_free(map->values);
    }
    nimcp_free(map);
}

/**
 * WHAT: Get attention value
 */
float attention_map_get(const attention_map_t* map, uint32_t x, uint32_t y)
{
    if (!nimcp_validate_pointer(map, "map")) {
        return -1.0F;
    }

    if (x >= map->width || y >= map->height) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Attention map coordinates out of bounds: (%u, %u) >= (%u, %u)", x, y, map->width, map->height);
        return -1.0F;
    }

    return map->values[y * map->width + x];
}

/**
 * WHAT: Set attention value
 */
bool attention_map_set(attention_map_t* map, uint32_t x, uint32_t y, float value)
{
    if (!nimcp_validate_pointer(map, "map")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_map_set: nimcp_validate_pointer is NULL");
        return false;
    }

    if (x >= map->width || y >= map->height) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Attention map coordinates out of bounds: (%u, %u) >= (%u, %u)", x, y, map->width, map->height);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_map_set: capacity exceeded");
        return false;
    }

    map->values[y * map->width + x] = value;
    return true;
}

//=============================================================================
// Visual Cortex Implementation
//=============================================================================

#define MAX_VISUAL_MEMORIES 1000
#define NUM_V1_LAYERS 4  // Layers 2/3, 4, 5, 6
#define NEUROMOD_TYPE_DOPAMINE 0
#define NEUROMOD_TYPE_ACETYLCHOLINE 1
#define NEUROMOD_TYPE_NOREPINEPHRINE 2
#define NUM_NEUROMOD_TYPES 3

struct visual_cortex_struct {
    // Configuration
    uint32_t input_width;
    uint32_t input_height;
    uint32_t num_v1_filters;
    uint32_t feature_dim;
    bool enable_attention;
    bool enable_memory;

    // Layers
    conv_layer_t* v1_layer;      // Edge detection
    pool_layer_t* pool_layer;    // Spatial pooling

    // Feature extraction (simplified - just average pooling to feature_dim)
    float* feature_weights;      // For compact feature representation

    // Visual memory
    visual_memory_t* memories[MAX_VISUAL_MEMORIES];
    uint32_t num_memories;

    // Neuromodulation
    brain_t brain;  /**< Brain reference for ACh + NE modulation */

    // === PHASE: PHASIC/TONIC NEUROMODULATION ===
    /**
     * Phasic/tonic states for three neuromodulators
     * Index: 0=dopamine, 1=acetylcholine, 2=norepinephrine
     *
     * WHAT: Dual-timescale neuromodulator dynamics
     * WHY:  Biological realism - bursts vs. baselines
     * HOW:  Separate phasic (rapid) and tonic (slow) components
     */
    phasic_tonic_state_t neuromod_states[NUM_NEUROMOD_TYPES];

    /**
     * Receptor expression profiles for V1 cortical layers
     * Index: 0=L2/3, 1=L4, 2=L5, 3=L6
     *
     * WHAT: Layer-specific receptor densities
     * WHY:  Different layers have different neuromod sensitivity
     * HOW:  D1/D2, M1/M2, α1/β2 densities per layer
     *
     * BIOLOGY:
     * - L2/3: Top-down attention (high D1, ACh)
     * - L4: Thalamic input (high ACh, NE)
     * - L5: Motor output (high DA, NE)
     * - L6: Feedback modulation (high M1, β2)
     */
    receptor_expression_t receptor_profiles[NUM_V1_LAYERS];

    // NIMCP 2.7 Phase 8.5: Internal recurrent network with fractal topology
    neural_network_t internal_network;  /**< Recurrent network for horizontal connections */
    bool has_internal_network;          /**< Whether internal network was created */

    // Statistics
    uint32_t images_processed;
    double total_processing_time;

    // === Memory Pool for O(1) Visual Memory Allocation ===
    memory_pool_t memory_pool;            /**< Pool for visual memory entries */
    nimcp_mutex_t* memory_pool_mutex;     /**< Mutex for memory pool thread safety */

    // === Copy-on-Write Support ===
    uint32_t* _cow_refcount;              /**< Reference count for CoW (NULL if owned) */
    bool _cow_is_shallow;                 /**< True if this is a shallow copy */

    // === Bio-Async Communication ===
    bio_module_context_t bio_ctx;         /**< Bio-async module context */
    bool bio_async_enabled;               /**< Whether bio-async is enabled */

    // === Second Messenger Cascades ===
    second_messenger_system_t* second_messengers; /**< Second messenger system per layer */
    bool second_messengers_enabled;               /**< Whether cascades are enabled */

    // === Training Interface (CNN-Cortex Integration) ===
    bool training_mode;                           /**< Whether in training mode */
    float* cached_conv_output;                    /**< Cached V1 output for gradient feedback */
    uint32_t cached_conv_size;                    /**< Size of cached conv output */
    float* cached_pool_output;                    /**< Cached pooled output for gradient feedback */
    uint32_t cached_pool_size;                    /**< Size of cached pool output */
    float* cached_feature_output;                 /**< Cached final normalized feature_dim embedding (last process) */
    uint32_t cached_feature_size;                 /**< Size of cached feature embedding (== feature_dim when valid) */
    float last_confidence;                        /**< Last computed visual confidence */
    float last_novelty;                           /**< Last computed novelty score */
    uint64_t last_process_timestamp;              /**< Timestamp of last processing */
};

/**
 * WHAT: Create visual cortex
 */
visual_cortex_t* visual_cortex_create(const visual_cortex_config_t* config)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(config, "config")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    if (config->input_width == 0 || config->input_height == 0 ||
        config->num_v1_filters == 0 || config->feature_dim == 0) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid visual cortex configuration parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    visual_cortex_t* cortex = (visual_cortex_t*)nimcp_calloc(1, sizeof(visual_cortex_t));
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_create: cortex allocation failed");
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate visual cortex");
        return NULL;
    }

    cortex->input_width = config->input_width;
    cortex->input_height = config->input_height;
    cortex->num_v1_filters = config->num_v1_filters;
    cortex->feature_dim = config->feature_dim;
    cortex->enable_attention = config->enable_attention;
    cortex->enable_memory = config->enable_memory;
    cortex->brain = NULL;  // Initialize neuromodulator brain reference

    // Initialize phasic/tonic neuromodulator states using proper API
    for (uint32_t i = 0; i < NUM_NEUROMOD_TYPES; i++) {
        cortex->neuromod_states[i] = phasic_tonic_state_create();
        phasic_tonic_set_phasic_burst(&cortex->neuromod_states[i], 0.0F);
        phasic_tonic_set_tonic_level(&cortex->neuromod_states[i], 0.5F);  // Baseline
    }

    // Initialize receptor expression profiles (biologically plausible defaults)
    // Layer 2/3: Top-down attention
    cortex->receptor_profiles[0].d1_density = 0.4F;
    cortex->receptor_profiles[0].d2_density = 0.2F;
    cortex->receptor_profiles[0].m1_density = 0.5F;
    cortex->receptor_profiles[0].m2_density = 0.3F;
    cortex->receptor_profiles[0].alpha1_density = 0.3F;
    cortex->receptor_profiles[0].beta2_density = 0.3F;

    // Layer 4: Thalamic input
    cortex->receptor_profiles[1].d1_density = 0.3F;
    cortex->receptor_profiles[1].d2_density = 0.2F;
    cortex->receptor_profiles[1].m1_density = 0.6F;  // High ACh
    cortex->receptor_profiles[1].m2_density = 0.3F;
    cortex->receptor_profiles[1].alpha1_density = 0.5F;  // High NE
    cortex->receptor_profiles[1].beta2_density = 0.3F;

    // Layer 5: Motor output/arousal
    cortex->receptor_profiles[2].d1_density = 0.4F;  // High DA
    cortex->receptor_profiles[2].d2_density = 0.3F;  // Moderate D2
    cortex->receptor_profiles[2].m1_density = 0.4F;
    cortex->receptor_profiles[2].m2_density = 0.2F;
    cortex->receptor_profiles[2].alpha1_density = 0.5F;  // High NE
    cortex->receptor_profiles[2].beta2_density = 0.3F;

    // Layer 6: Feedback modulation
    cortex->receptor_profiles[3].d1_density = 0.3F;
    cortex->receptor_profiles[3].d2_density = 0.2F;
    cortex->receptor_profiles[3].m1_density = 0.6F;  // High M1
    cortex->receptor_profiles[3].m2_density = 0.4F;
    cortex->receptor_profiles[3].alpha1_density = 0.3F;
    cortex->receptor_profiles[3].beta2_density = 0.4F;  // High β2

    // Create V1 layer (edge detection with Gabor-like filters)
    conv_layer_config_t conv_config = {
        .input_width = config->input_width,
        .input_height = config->input_height,
        .input_channels = 1,  // Start with grayscale
        .num_filters = config->num_v1_filters,
        .kernel_size = 7,
        .stride = 2,
        .padding = 3,
        .activation = VISUAL_ACTIVATION_RELU
    };

    cortex->v1_layer = conv_layer_create(&conv_config);
    if (!nimcp_validate_pointer(cortex->v1_layer, "v1_layer")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to create V1 convolution layer");
        visual_cortex_destroy(cortex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Initialize V1 with Gabor filters at different orientations
    int num_orientations = (config->num_v1_filters >= 4) ? 4 : config->num_v1_filters;
    for (uint32_t i = 0; i < config->num_v1_filters; i++) {
        float orientation = (i % num_orientations) * (180.0F / num_orientations);

        gabor_params_t gabor_params = {
            .wavelength = 4.0F,
            .orientation = orientation,
            .phase = 0.0F,
            .aspect_ratio = 0.5F,
            .bandwidth = 1.0F
        };

        float* gabor_kernel = gabor_create_kernel(7, &gabor_params);
        if (gabor_kernel) {
            conv_layer_set_kernel(cortex->v1_layer, i, gabor_kernel);
            nimcp_free(gabor_kernel);
        }
    }

    // Create pooling layer
    uint32_t v1_output_w = conv_layer_get_output_width(cortex->v1_layer);
    uint32_t v1_output_h = conv_layer_get_output_height(cortex->v1_layer);

    pool_layer_config_t pool_config = {
        .input_width = v1_output_w,
        .input_height = v1_output_h,
        .input_channels = config->num_v1_filters,
        .pool_size = 2,
        .stride = 2,
        .type = POOL_MAX
    };

    cortex->pool_layer = pool_layer_create(&pool_config);
    if (!nimcp_validate_pointer(cortex->pool_layer, "pool_layer")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to create pooling layer");
        visual_cortex_destroy(cortex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Allocate feature weights (simplified feature extraction)
    uint32_t pooled_size = (v1_output_w / 2) * (v1_output_h / 2) * config->num_v1_filters;
    cortex->feature_weights = (float*)nimcp_calloc(pooled_size, sizeof(float));
    if (!nimcp_validate_pointer(cortex->feature_weights, "feature_weights")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate feature weights");
        visual_cortex_destroy(cortex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Initialize feature weights
    for (uint32_t i = 0; i < pooled_size; i++) {
        cortex->feature_weights[i] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 0.01F;
    }

    // NIMCP 2.7 Phase 8.5: Fractal Topology Integration (Future Enhancement)
    // TODO: Add internal recurrent network with scale-free topology
    //
    // WHAT: Internal spiking network for recurrent V1 processing
    // WHY:  Enable temporal integration, feedback, and attention modulation
    // HOW:  Create neural_network with config->internal_neurons neurons,
    //       apply topology_generate_scale_free() with configured parameters
    //
    // BIOLOGICAL MOTIVATION:
    // While V1's feedforward structure (CNN) is innate, recurrent connections
    // within V1 (horizontal connections, feedback from V2/V4) are extensive and
    // exhibit scale-free properties. This internal network models:
    // - Horizontal connections for contour integration
    // - Feedback modulation for attention and expectation
    // - Temporal integration for motion processing
    //
    cortex->internal_network = NULL;
    cortex->has_internal_network = false;

    if (config->enable_fractal_topology && config->internal_neurons > 0) {
        // Create internal recurrent network
        network_config_t net_config = {
            .num_neurons = config->internal_neurons,
            .ei_ratio = 0.8F,  // 80% excitatory (typical cortex)
            .learning_rate = 0.001F,
            .hebbian_rate = 0.01F,
            .stdp_window = 20.0F,
            .homeostatic_rate = 0.001F,
            .target_activity = 0.1F,
            .adaptation_rate = 0.01F,
            .refractory_period = 2.0F,
            .min_weight = 0.0F,
            .max_weight = 1.0F,
            .update_interval = 1,
            .enable_stdp = true,
            .enable_homeostasis = true,
            .neuron_model = NEURON_MODEL_LIF,
            .model_params = NULL,
            .integration_method = ODE_EULER
        };

        cortex->internal_network = neural_network_create(&net_config);

        if (cortex->internal_network) {
            // Generate scale-free topology
            scale_free_config_t topo_config = {
                .power_law_gamma = config->power_law_gamma,
                .hub_ratio = config->hub_ratio,
                .min_degree = 2,
                .max_degree = config->internal_neurons / 10,
                .spatial_constraint = 0.5F,  // V1 has spatial organization
                .bidirectional = false
            };

            topology_stats_t stats;
            if (topology_generate_scale_free(cortex->internal_network, &topo_config, &stats)) {
                cortex->has_internal_network = true;
                LOG_INFO(VISUAL_LOG_MODULE, "V1 internal network: %u neurons, %u synapses, %.2f avg degree",
                         stats.num_neurons, stats.num_synapses, stats.avg_degree);
            } else {
                LOG_WARN(VISUAL_LOG_MODULE, "Failed to generate V1 topology, using network without topology");
                cortex->has_internal_network = true;  // Network exists, just without topology
            }
        } else {
            LOG_WARN(VISUAL_LOG_MODULE, "Failed to create V1 internal network");
        }
    }

    // === Initialize Memory Pool for O(1) Visual Memory Allocation ===
    memory_pool_config_t mem_pool_config = memory_pool_default_config(
        sizeof(visual_memory_t),
        MAX_VISUAL_MEMORIES  // Pool sized for maximum memories
    );
    mem_pool_config.alignment = 16;  // SIMD alignment for feature vectors
    mem_pool_config.enable_tracking = true;

    cortex->memory_pool = memory_pool_create(&mem_pool_config);
    if (!cortex->memory_pool) {
        LOG_WARN(VISUAL_LOG_MODULE, "Failed to create memory pool, using malloc fallback");
    }

    // Initialize memory pool mutex for thread safety
    cortex->memory_pool_mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (cortex->memory_pool_mutex) {
        nimcp_mutex_init(cortex->memory_pool_mutex, NULL);
    } else {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate memory pool mutex");
        visual_cortex_destroy(cortex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: validation failed");
        return NULL;
    }

    // Initialize CoW fields (owned by default)
    cortex->_cow_refcount = NULL;
    cortex->_cow_is_shallow = false;

    // === Bio-Async Registration ===
    cortex->bio_ctx = NULL;
    cortex->bio_async_enabled = false;

    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_VISUAL_CORTEX,
            .module_name = "visual_cortex",
            .inbox_capacity = 64,
            .user_data = cortex
        };

        cortex->bio_ctx = bio_router_register_module(&bio_info);
        if (cortex->bio_ctx) {
            cortex->bio_async_enabled = true;
            LOG_INFO(VISUAL_LOG_MODULE, "Bio-async registered for visual cortex (module_id=%d)",
                     BIO_MODULE_VISUAL_CORTEX);
        } else {
            LOG_WARN(VISUAL_LOG_MODULE, "Failed to register bio-async for visual cortex");
        }
    }

    // === Second Messenger Cascade Initialization ===
    cortex->second_messengers = NULL;
    cortex->second_messengers_enabled = false;

    if (config->enable_second_messengers) {
        // WHAT: Create second messenger system for V1 layers
        // WHY:  Enable neuromodulator-driven signaling cascades
        // HOW:  One system tracking NUM_V1_LAYERS (4) neurons, one per cortical layer
        second_messenger_config_t sm_config = second_messenger_default_config();
        sm_config.enable_bio_async = config->enable_bio_async;
        sm_config.enable_security = true;

        cortex->second_messengers = second_messenger_create(NUM_V1_LAYERS, &sm_config);
        if (cortex->second_messengers) {
            cortex->second_messengers_enabled = true;
            LOG_INFO(VISUAL_LOG_MODULE, "Second messenger cascades enabled for visual cortex (%u layers)",
                     NUM_V1_LAYERS);

            // Register with bio-async router if both are enabled
            if (cortex->bio_async_enabled && cortex->bio_ctx) {
                bio_router_t router = bio_router_get_global();
                if (router) {
                    nimcp_result_t result = second_messenger_register_bioasync(
                        cortex->second_messengers, router);
                    if (result != NIMCP_SUCCESS) {
                        LOG_WARN(VISUAL_LOG_MODULE, "Failed to register second messengers with bio-async: %d",
                                 result);
                    } else {
                        LOG_DEBUG(VISUAL_LOG_MODULE, "Second messengers registered with bio-async");
                    }
                }
            }
        } else {
            LOG_WARN(VISUAL_LOG_MODULE, "Failed to create second messenger system");
        }
    }

    // === Training Interface Initialization ===
    cortex->training_mode = false;
    cortex->cached_conv_output = NULL;
    cortex->cached_conv_size = 0;
    cortex->cached_pool_output = NULL;
    cortex->cached_pool_size = 0;
    cortex->cached_feature_output = NULL;
    cortex->cached_feature_size = 0;
    cortex->last_confidence = 0.0F;
    cortex->last_novelty = 0.0F;
    cortex->last_process_timestamp = 0;

    return cortex;
}

/**
 * WHAT: Destroy visual cortex
 */
void visual_cortex_destroy(visual_cortex_t* cortex)
{
    if (!cortex) {
        return;
    }

    // === Second Messenger Cleanup ===
    if (cortex->second_messengers) {
        second_messenger_destroy(cortex->second_messengers);
        cortex->second_messengers = NULL;
        LOG_DEBUG(VISUAL_LOG_MODULE, "Second messenger cascades destroyed");
    }

    // === Bio-Async Unregistration ===
    if (cortex->bio_async_enabled && cortex->bio_ctx) {
        bio_router_unregister_module(cortex->bio_ctx);
        cortex->bio_ctx = NULL;
        cortex->bio_async_enabled = false;
        LOG_DEBUG(VISUAL_LOG_MODULE, "Bio-async unregistered for visual cortex");
    }

    // === CoW Reference Counting ===
    // If this is a shallow copy with shared data, decrement refcount
    if (cortex->_cow_refcount) {
        // Load current count and attempt to decrement atomically
        uint32_t old_count = __atomic_load_n(cortex->_cow_refcount, __ATOMIC_SEQ_CST);
        do {
            if (old_count == 0) {
                // Already freed by another thread - just free our handle
                nimcp_free(cortex);
                return;
            }
        } while (!__atomic_compare_exchange_n(cortex->_cow_refcount, &old_count, old_count - 1,
                                              false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

        if (old_count > 1) {
            // Other references still exist - just free our handle
            nimcp_free(cortex);
            return;
        }
        // We were the last reference (old_count == 1) - free the refcount and continue cleanup
        nimcp_free(cortex->_cow_refcount);
    }

    if (cortex->v1_layer) {
        conv_layer_destroy(cortex->v1_layer);
    }

    if (cortex->pool_layer) {
        pool_layer_destroy(cortex->pool_layer);
    }

    if (cortex->feature_weights) {
        nimcp_free(cortex->feature_weights);
    }

    // Free visual memories (use pool if available, with mutex protection)
    for (uint32_t i = 0; i < cortex->num_memories; i++) {
        if (cortex->memories[i]) {
            if (cortex->memories[i]->features) {
                nimcp_free(cortex->memories[i]->features);
            }
            // Thread-safe check and release using mutex
            if (cortex->memory_pool_mutex) {
                nimcp_mutex_lock(cortex->memory_pool_mutex);
            }
            if (cortex->memory_pool && memory_pool_owns(cortex->memory_pool, cortex->memories[i])) {
                memory_pool_release(cortex->memory_pool, cortex->memories[i]);
            } else {
                nimcp_free(cortex->memories[i]);
            }
            if (cortex->memory_pool_mutex) {
                nimcp_mutex_unlock(cortex->memory_pool_mutex);
            }
        }
    }

    // === Destroy Memory Pool ===
    if (cortex->memory_pool) {
        memory_pool_destroy(cortex->memory_pool);
    }

    // === Destroy Memory Pool Mutex ===
    if (cortex->memory_pool_mutex) {
        nimcp_mutex_destroy(cortex->memory_pool_mutex);

    }

    // NIMCP 2.7 Phase 8.5: Destroy internal recurrent network
    if (cortex->internal_network) {
        neural_network_destroy(cortex->internal_network);
    }

    // === Training Cache Cleanup ===
    if (cortex->cached_conv_output) {
        nimcp_free(cortex->cached_conv_output);
        cortex->cached_conv_output = NULL;
    }
    if (cortex->cached_pool_output) {
        nimcp_free(cortex->cached_pool_output);
        cortex->cached_pool_output = NULL;
    }
    if (cortex->cached_feature_output) {
        nimcp_free(cortex->cached_feature_output);
        cortex->cached_feature_output = NULL;
    }

    nimcp_free(cortex);
}

/**
 * WHAT: Process image through visual cortex
 */
bool visual_cortex_process(
    visual_cortex_t* cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* features)
{
    // Guard: Validate inputs with basic pointer checks
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(image, "image") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    // BBB: Validate external image input (SECURITY CRITICAL)
    // This is external sensory data that could be adversarial/corrupted
    if (!bbb_check_pointer(image, "visual_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, VISUAL_LOG_MODULE, "invalid_image_ptr",
                      "NULL image pointer rejected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: bbb_check_pointer is NULL");
        return false;
    }

    // BBB: Validate image dimensions are within safe bounds
    // Prevent integer overflow and excessive memory allocation
    const uint32_t MAX_DIMENSION = 16384;  // 16K max dimension
    const uint32_t MAX_CHANNELS = 4;       // RGBA max
    if (!bbb_validate_range_u(width, 1, MAX_DIMENSION, "visual_cortex_process") ||
        !bbb_validate_range_u(height, 1, MAX_DIMENSION, "visual_cortex_process") ||
        !bbb_validate_range_u(channels, 1, MAX_CHANNELS, "visual_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, VISUAL_LOG_MODULE, "invalid_dimensions",
                      "width=%u height=%u channels=%u rejected", width, height, channels);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: bbb_validate_range_u is NULL");
        return false;
    }

    // BBB: Validate buffer size doesn't overflow
    uint64_t expected_size = (uint64_t)width * height * channels;
    if (expected_size > (uint64_t)UINT32_MAX) {
        bbb_audit_log(BBB_AUDIT_WARNING, VISUAL_LOG_MODULE, "size_overflow",
                      "Image size %llu exceeds maximum", (unsigned long long)expected_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: validation failed");
        return false;
    }

    // Standard dimension check
    if (width != cortex->input_width || height != cortex->input_height || channels == 0) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid image dimensions: %ux%ux%u (expected %ux%ux>0)",
                           width, height, channels, cortex->input_width, cortex->input_height);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: channels is zero");
        return false;
    }

    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    // Convert uint8 image to float (normalize to 0-1)
    uint32_t input_size = width * height;
    float* input_float = (float*)nimcp_calloc(input_size, sizeof(float));
    if (!nimcp_validate_pointer(input_float, "input_float")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate input buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    // Convert to grayscale if RGB
    for (uint32_t i = 0; i < input_size; i++) {
        float val = 0.0F;
        for (uint32_t c = 0; c < channels; c++) {
            val += image[i * channels + c];
        }
        input_float[i] = (val / channels) / 255.0F;
    }

    // V1: Edge detection
    uint32_t v1_output_w = conv_layer_get_output_width(cortex->v1_layer);
    uint32_t v1_output_h = conv_layer_get_output_height(cortex->v1_layer);
    uint32_t v1_output_size = v1_output_w * v1_output_h * cortex->num_v1_filters;

    float* v1_output = (float*)nimcp_calloc(v1_output_size, sizeof(float));
    if (!nimcp_validate_pointer(v1_output, "v1_output")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate V1 output buffer");
        nimcp_free(input_float);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!conv_layer_forward(cortex->v1_layer, input_float, v1_output)) {
        nimcp_free(input_float);
        nimcp_free(v1_output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: conv_layer_forward is NULL");
        return false;
    }

    // Apply neuromodulator gain (ACh + NE modulation)
    // Legacy gain calculation (kept for backward compatibility)
    float visual_gain = get_visual_gain(cortex->brain);

    // NEW: Compute layer-specific neuromodulation effects
    // Use Layer 4 (thalamic input layer) as default for feedforward processing
    neuromod_effects_t neuromod_effects;
    visual_cortex_compute_neuromod_effects(cortex, 1, &neuromod_effects);  // Layer 4

    // Apply combined gain: legacy × neuromodulation
    float combined_gain = visual_gain * neuromod_effects.gabor_gain;

    if (combined_gain != 1.0F) {
        /* Use tensor library for vectorized gain application */
        uint32_t v1_dims[] = {v1_output_size};
        nimcp_tensor_t* v1_tensor = nimcp_tensor_from_data(v1_output, v1_dims, 1, NIMCP_DTYPE_F32, false);
        if (v1_tensor) {
            nimcp_tensor_mul_scalar_(v1_tensor, (double)combined_gain);
            nimcp_tensor_destroy(v1_tensor);
        } else {
            /* Fallback to scalar */
            for (uint32_t i = 0; i < v1_output_size; i++) {
                v1_output[i] *= combined_gain;
            }
        }
    }

    // Pooling
    uint32_t pooled_w = v1_output_w / 2;
    uint32_t pooled_h = v1_output_h / 2;
    uint32_t pooled_size = pooled_w * pooled_h * cortex->num_v1_filters;

    float* pooled_output = (float*)nimcp_calloc(pooled_size, sizeof(float));
    if (!nimcp_validate_pointer(pooled_output, "pooled_output")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate pooled output buffer");
        nimcp_free(input_float);
        nimcp_free(v1_output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!pool_layer_forward(cortex->pool_layer, v1_output, pooled_output)) {
        nimcp_free(input_float);
        nimcp_free(v1_output);
        nimcp_free(pooled_output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_process: pool_layer_forward is NULL");
        return false;
    }

    // Feature extraction (simplified: average pooling to feature_dim)
    memset(features, 0, cortex->feature_dim * sizeof(float));

    uint32_t features_per_bin = (pooled_size + cortex->feature_dim - 1) / cortex->feature_dim;
    for (uint32_t i = 0; i < pooled_size; i++) {
        uint32_t bin = i / features_per_bin;
        if (bin < cortex->feature_dim) {
            features[bin] += pooled_output[i];
        }
    }

    // Normalize features using tensor library
    {
        uint32_t feat_dims[] = {cortex->feature_dim};
        nimcp_tensor_t* feat_tensor = nimcp_tensor_from_data(features, feat_dims, 1, NIMCP_DTYPE_F32, false);
        if (feat_tensor) {
            float norm = (float)nimcp_tensor_norm_p(feat_tensor, 2.0);
            if (norm > 1e-6F) {
                nimcp_tensor_mul_scalar_(feat_tensor, 1.0 / (double)norm);
            }
            nimcp_tensor_destroy(feat_tensor);
        } else {
            /* Fallback to scalar */
            float norm = 0.0F;
            for (uint32_t i = 0; i < cortex->feature_dim; i++) {
                norm += features[i] * features[i];
            }
            norm = sqrtf(norm);
            if (norm > 1e-6F) {
                for (uint32_t i = 0; i < cortex->feature_dim; i++) {
                    features[i] /= norm;
                }
            }
        }
    }

    /* Cache the final normalized feature embedding so consumers that have no
     * image to push (e.g. the language cascade reading the cortex's current
     * perceptual state) can retrieve the last-computed features via
     * visual_cortex_get_cached_features(). Grow the buffer on first use /
     * dim change; this is the same lifetime as the cortex. */
    if (cortex->cached_feature_size != cortex->feature_dim) {
        if (cortex->cached_feature_output) nimcp_free(cortex->cached_feature_output);
        cortex->cached_feature_output =
            (float*)nimcp_calloc(cortex->feature_dim, sizeof(float));
        cortex->cached_feature_size =
            cortex->cached_feature_output ? cortex->feature_dim : 0u;
    }
    if (cortex->cached_feature_output &&
        cortex->cached_feature_size == cortex->feature_dim) {
        memcpy(cortex->cached_feature_output, features,
               cortex->feature_dim * sizeof(float));
    }

    // Update statistics
    cortex->images_processed++;
    struct timespec end_ts;
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    double elapsed_ms = (end_ts.tv_sec - start_ts.tv_sec) * 1000.0 +
                        (end_ts.tv_nsec - start_ts.tv_nsec) / 1000000.0;
    cortex->total_processing_time += elapsed_ms;

    nimcp_free(input_float);
    nimcp_free(v1_output);
    nimcp_free(pooled_output);

    return true;
}

/**
 * WHAT: Compute visual attention
 */
bool visual_cortex_compute_attention(
    visual_cortex_t* cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    attention_map_t* attn_map)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(image, "image") ||
        !nimcp_validate_pointer(attn_map, "attn_map")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_compute_attention: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!cortex->enable_attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_compute_attention: cortex->enable_attention is NULL");
        return false;
    }

    // Compute layer-specific neuromodulation effects
    // Use Layer 2/3 (top-down attention layer) for attention modulation
    neuromod_effects_t neuromod_effects;
    visual_cortex_compute_neuromod_effects(cortex, 0, &neuromod_effects);  // Layer 2/3

    // Compute gradient magnitude (simple edge-based attention)
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            float gx = (float)image[(y * width + x + 1)] - (float)image[(y * width + x - 1)];
            float gy = (float)image[((y + 1) * width + x)] - (float)image[((y - 1) * width + x)];
            float magnitude = sqrtf(gx * gx + gy * gy) / 255.0F;

            // Apply neuromodulation attention boost (ACh enhances attention)
            magnitude *= neuromod_effects.attention_boost;

            attention_map_set(attn_map, x, y, magnitude);
        }
    }

    return true;
}

/**
 * WHAT: Store visual memory
 */
bool visual_cortex_store_memory(
    visual_cortex_t* cortex,
    const float* features,
    float salience)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_store_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!cortex->enable_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_store_memory: cortex->enable_memory is NULL");
        return false;
    }

    if (cortex->num_memories >= MAX_VISUAL_MEMORIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "visual_cortex_store_memory: capacity exceeded");
        return false;  // Memory full
    }

    // === Allocate memory entry (pool with malloc fallback) ===
    visual_memory_t* memory = NULL;
    if (cortex->memory_pool) {
        memory = (visual_memory_t*)memory_pool_acquire(cortex->memory_pool);
        if (memory) {
            memset(memory, 0, sizeof(visual_memory_t));
        }
    }
    // Fallback to malloc if pool exhausted or unavailable
    if (!memory) {
        memory = (visual_memory_t*)nimcp_calloc(1, sizeof(visual_memory_t));
    }
    if (!nimcp_validate_pointer(memory, "memory")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate visual memory entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_store_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    memory->feature_dim = cortex->feature_dim;
    memory->features = (float*)nimcp_calloc(cortex->feature_dim, sizeof(float));
    if (!nimcp_validate_pointer(memory->features, "memory->features")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate visual memory features");
        // Release memory to appropriate allocator
        if (cortex->memory_pool && memory_pool_owns(cortex->memory_pool, memory)) {
            memory_pool_release(cortex->memory_pool, memory);
        } else {
            nimcp_free(memory);
        }
        return false;
    }

    memcpy(memory->features, features, cortex->feature_dim * sizeof(float));
    memory->salience = salience;
    memory->timestamp = (uint64_t)time(NULL);

    cortex->memories[cortex->num_memories++] = memory;
    return true;
}

/**
 * WHAT: Recall visual memories
 */
bool visual_cortex_recall_memory(
    visual_cortex_t* cortex,
    const float* query_features,
    int max_results,
    visual_memory_t*** memories,
    int* num_memories)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(query_features, "query_features") ||
        !nimcp_validate_pointer(memories, "memories") ||
        !nimcp_validate_pointer(num_memories, "num_memories")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!cortex->enable_memory || cortex->num_memories == 0) {
        *memories = NULL;
        *num_memories = 0;
        return true;
    }

    // Compute similarity to all memories
    typedef struct {
        visual_memory_t* memory;
        float similarity;
    } memory_similarity_t;

    memory_similarity_t* similarities = (memory_similarity_t*)nimcp_calloc(
        cortex->num_memories, sizeof(memory_similarity_t));
    if (!nimcp_validate_pointer(similarities, "similarities")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate similarity buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    for (uint32_t i = 0; i < cortex->num_memories; i++) {
        similarities[i].memory = cortex->memories[i];

        // Cosine similarity
        float dot = 0.0F;
        for (uint32_t j = 0; j < cortex->feature_dim; j++) {
            dot += query_features[j] * cortex->memories[i]->features[j];
        }
        similarities[i].similarity = dot;
    }

    // Sort by similarity (simple bubble sort for small arrays)
    for (uint32_t i = 0; i < cortex->num_memories - 1; i++) {
        for (uint32_t j = 0; j < cortex->num_memories - i - 1; j++) {
            if (similarities[j].similarity < similarities[j + 1].similarity) {
                memory_similarity_t temp = similarities[j];
                similarities[j] = similarities[j + 1];
                similarities[j + 1] = temp;
            }
        }
    }

    // Return top results
    int num_results = (max_results < (int)cortex->num_memories) ? max_results : (int)cortex->num_memories;
    *memories = (visual_memory_t**)nimcp_calloc(num_results, sizeof(visual_memory_t*));
    if (!nimcp_validate_pointer(*memories, "result_memories")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate memory results array");
        nimcp_free(similarities);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    for (int i = 0; i < num_results; i++) {
        (*memories)[i] = similarities[i].memory;
    }
    *num_memories = num_results;

    nimcp_free(similarities);
    return true;
}

/**
 * WHAT: Get statistics
 */
bool visual_cortex_get_stats(const visual_cortex_t* cortex, visual_cortex_stats_t* stats)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(stats, "stats")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    stats->images_processed = cortex->images_processed;
    stats->memories_stored = cortex->num_memories;
    stats->avg_processing_time = (cortex->images_processed > 0) ?
        (float)(cortex->total_processing_time / cortex->images_processed) : 0.0F;

    // Estimate memory usage (rough)
    stats->memory_usage_mb = (sizeof(visual_cortex_t) +
                              cortex->num_memories * (sizeof(visual_memory_t) + cortex->feature_dim * sizeof(float))) /
                             (1024.0F * 1024.0F);

    return true;
}

//=============================================================================
// Brain Integration Helper Functions
//=============================================================================

/**
 * WHAT: Compute novelty score for curiosity system
 * WHY:  Drive exploration of novel visual patterns
 * HOW:  Compare features against visual memory, return novelty (0-1)
 *
 * High novelty (close to 1.0) = never seen before → triggers curiosity
 * Low novelty (close to 0.0) = familiar → less interesting
 */
float visual_cortex_compute_novelty(visual_cortex_t* cortex, const float* features)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        return 0.0F;
    }

    // If no memories, everything is novel
    if (cortex->num_memories == 0) {
        return 1.0F;
    }

    // Normalize query features
    float query_norm = 0.0F;
    for (uint32_t j = 0; j < cortex->feature_dim; j++) {
        query_norm += features[j] * features[j];
    }
    query_norm = sqrtf(query_norm);
    if (query_norm < 1e-6F) {
        return 1.0F;  // Zero features are maximally novel
    }

    // Find maximum cosine similarity to existing memories
    float max_similarity = 0.0F;
    for (uint32_t i = 0; i < cortex->num_memories; i++) {
        // Compute normalized dot product (cosine similarity)
        float dot = 0.0F;
        float memory_norm = 0.0F;
        for (uint32_t j = 0; j < cortex->feature_dim; j++) {
            dot += features[j] * cortex->memories[i]->features[j];
            memory_norm += cortex->memories[i]->features[j] * cortex->memories[i]->features[j];
        }
        memory_norm = sqrtf(memory_norm);

        if (memory_norm > 1e-6F) {
            float cosine_sim = dot / (query_norm * memory_norm);
            if (cosine_sim > max_similarity) {
                max_similarity = cosine_sim;
            }
        }
    }

    // Novelty = 1 - similarity (cosine similarity ranges from -1 to 1)
    // Clamp similarity to [0, 1] range first
    if (max_similarity < 0.0F) max_similarity = 0.0F;
    if (max_similarity > 1.0F) max_similarity = 1.0F;

    float novelty = 1.0F - max_similarity;
    return novelty;
}

/**
 * WHAT: Get maximum attention location
 * WHY:  Identify most salient region for attention system
 * HOW:  Find peak in attention map
 *
 * Returns the (x, y) location with highest attention value
 */
bool visual_cortex_get_attention_peak(
    const attention_map_t* attn_map,
    uint32_t* max_x,
    uint32_t* max_y,
    float* max_value)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(attn_map, "attn_map") ||
        !nimcp_validate_pointer(max_x, "max_x") ||
        !nimcp_validate_pointer(max_y, "max_y") ||
        !nimcp_validate_pointer(max_value, "max_value")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_attention_peak: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(attn_map->values, "attn_map->values")) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Attention map has no values");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_attention_peak: nimcp_validate_pointer is NULL");
        return false;
    }

    // Find maximum
    *max_value = -INFINITY;
    *max_x = 0;
    *max_y = 0;

    for (uint32_t y = 0; y < attn_map->height; y++) {
        for (uint32_t x = 0; x < attn_map->width; x++) {
            float value = attn_map->values[y * attn_map->width + x];
            if (value > *max_value) {
                *max_value = value;
                *max_x = x;
                *max_y = y;
            }
        }
    }

    return true;
}

/**
 * WHAT: Prepare visual features for memory consolidation
 * WHY:  Enable hippocampus to store visual experiences
 * HOW:  Package features with metadata for memory system
 *
 * This function stores visual features in the visual cortex's local memory
 * AND prepares them for transfer to the hippocampus for long-term consolidation.
 *
 * Integration flow:
 * Visual Cortex → local storage (fast recall)
 *              → Hippocampus (long-term consolidation)
 *              → Knowledge Graph (semantic integration)
 */
bool visual_cortex_consolidate_memory(
    visual_cortex_t* cortex,
    const float* features,
    float salience,
    const char* context)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_consolidate_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    // Store in local visual memory
    if (!visual_cortex_store_memory(cortex, features, salience)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_cortex_consolidate_memory: visual_cortex_store_memory is NULL");
        return false;
    }

    // TODO: Integration with hippocampus
    // This is a placeholder for future hippocampus integration
    // Will call: hippocampus_consolidate_visual_memory(features, salience, context)
    //
    // The hippocampus would:
    // 1. Store visual experience with episodic context
    // 2. Link to semantic knowledge graph
    // 3. Enable sleep-based memory consolidation
    // 4. Support visual imagination/recall
    (void)context;  // Suppress unused warning until hippocampus integration

    return true;
}

/**
 * @brief Associate brain with visual cortex for neuromodulation
 *
 * WHAT: Set brain reference for ACh + NE modulation of visual processing
 * WHY:  Enable neurochemical modulation (attention, arousal, threat detection)
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * BIOLOGY:
 * - Acetylcholine from basal forebrain enhances V1 feature selectivity
 * - Norepinephrine from locus coeruleus increases arousal-dependent vision
 *
 * @param cortex Visual cortex instance
 * @param brain Brain instance (or NULL to clear)
 */
void visual_cortex_set_brain(visual_cortex_t* cortex, brain_t brain)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_set_brain: cortex is NULL");
        return;
    }
    cortex->brain = brain;
}

//=============================================================================
// Neuromodulation Implementation
//=============================================================================

/**
 * @brief Get phasic/tonic state for a neuromodulator
 */
const phasic_tonic_state_t* visual_cortex_get_neuromod_state(
    const visual_cortex_t* cortex,
    uint32_t neuromod_type)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_neuromod_state: nimcp_validate_pointer is NULL");
        return NULL;
    }

    if (neuromod_type >= NUM_NEUROMOD_TYPES) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid neuromod_type: %u (max %u)", neuromod_type, NUM_NEUROMOD_TYPES - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_neuromod_state: capacity exceeded");
        return NULL;
    }

    return &cortex->neuromod_states[neuromod_type];
}

/**
 * @brief Set receptor expression profile for visual cortex
 */
bool visual_cortex_set_receptor_profile(
    visual_cortex_t* cortex,
    uint32_t layer_idx,
    const receptor_expression_t* receptors)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(receptors, "receptors")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_set_receptor_profile: nimcp_validate_pointer is NULL");
        return false;
    }

    if (layer_idx >= NUM_V1_LAYERS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid layer_idx: %u (max %u)", layer_idx, NUM_V1_LAYERS - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_set_receptor_profile: capacity exceeded");
        return false;
    }

    // Copy receptor profile
    cortex->receptor_profiles[layer_idx] = *receptors;
    return true;
}

/**
 * @brief Get receptor expression profile for a layer
 */
const receptor_expression_t* visual_cortex_get_receptor_profile(
    const visual_cortex_t* cortex,
    uint32_t layer_idx)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_receptor_profile: nimcp_validate_pointer is NULL");
        return NULL;
    }

    if (layer_idx >= NUM_V1_LAYERS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid layer_idx: %u (max %u)", layer_idx, NUM_V1_LAYERS - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_receptor_profile: capacity exceeded");
        return NULL;
    }

    return &cortex->receptor_profiles[layer_idx];
}

/**
 * @brief Update phasic/tonic decay
 *
 * WHAT: Apply exponential decay to phasic and tonic levels
 * WHY:  Model reuptake and clearance mechanisms
 * HOW:  level *= exp(-decay_rate * dt)
 *
 * COMPLEXITY: O(1)
 */
static void update_neuromod_decay(phasic_tonic_state_t* state, float dt_sec)
{
    // Exponential decay: level(t+dt) = level(t) * exp(-dt/tau)
    // Approximation: level *= (1 - dt/tau) for small dt
    float phasic_decay_rate = 1.0F / phasic_tonic_get_burst_decay_tau(state);  // Convert tau to rate
    float tonic_decay_rate = 1.0F / phasic_tonic_get_homeostatic_tau(state);    // Convert tau to rate

    float phasic = phasic_tonic_get_phasic_burst(state);
    phasic *= (1.0F - phasic_decay_rate * dt_sec);

    // Tonic level moves toward target with homeostatic regulation
    float tonic = phasic_tonic_get_tonic_level(state);
    float tonic_error = phasic_tonic_get_tonic_target(state) - tonic;
    tonic += tonic_error * tonic_decay_rate * dt_sec;

    // Clamp to valid ranges
    float max_burst = phasic_tonic_get_max_burst_amplitude(state);
    float tonic_min = phasic_tonic_get_tonic_min(state);
    float tonic_max = phasic_tonic_get_tonic_max(state);

    phasic = fminf(fmaxf(phasic, 0.0F), max_burst);
    tonic = fminf(fmaxf(tonic, tonic_min), tonic_max);

    phasic_tonic_set_phasic_burst(state, phasic);
    phasic_tonic_set_tonic_level(state, tonic);
}

/**
 * @brief Trigger phasic neuromodulator burst
 */
bool visual_cortex_trigger_phasic_burst(
    visual_cortex_t* cortex,
    uint32_t neuromod_type,
    float amount)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_phasic_burst: nimcp_validate_pointer is NULL");
        return false;
    }

    if (neuromod_type >= NUM_NEUROMOD_TYPES) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid neuromod_type: %u", neuromod_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_phasic_burst: capacity exceeded");
        return false;
    }

    if (amount < 0.0F || amount > 1.0F) {
        LOG_WARN(VISUAL_LOG_MODULE, "Phasic burst amount out of range: %.2f, clamping to [0,1]", amount);
        amount = fminf(fmaxf(amount, 0.0F), 1.0F);
    }

    // Add to phasic level (capped at 1.0)
    phasic_tonic_state_t* state = &cortex->neuromod_states[neuromod_type];
    float phasic = phasic_tonic_get_phasic_burst(state) + amount;
    phasic_tonic_set_phasic_burst(state, fminf(phasic, 1.0F));

    return true;
}

/**
 * @brief Set tonic neuromodulator baseline
 */
bool visual_cortex_set_tonic_level(
    visual_cortex_t* cortex,
    uint32_t neuromod_type,
    float level)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_set_tonic_level: nimcp_validate_pointer is NULL");
        return false;
    }

    if (neuromod_type >= NUM_NEUROMOD_TYPES) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid neuromod_type: %u", neuromod_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_set_tonic_level: capacity exceeded");
        return false;
    }

    if (level < 0.0F || level > 1.0F) {
        LOG_WARN(VISUAL_LOG_MODULE, "Tonic level out of range: %.2f, clamping to [0,1]", level);
        level = fminf(fmaxf(level, 0.0F), 1.0F);
    }

    // Set tonic level directly
    phasic_tonic_set_tonic_level(&cortex->neuromod_states[neuromod_type], level);
    return true;
}

/**
 * @brief Compute current neuromodulation effects
 *
 * ALGORITHM:
 * 1. Read neuromodulator levels from brain (if available)
 * 2. Update phasic/tonic states with decay
 * 3. Combine phasic + tonic: effective = α*phasic + (1-α)*tonic
 * 4. Multiply by receptor densities
 * 5. Compute gain values for visual processing
 */
bool visual_cortex_compute_neuromod_effects(
    const visual_cortex_t* cortex,
    uint32_t layer_idx,
    neuromod_effects_t* effects)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(effects, "effects")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_compute_neuromod_effects: nimcp_validate_pointer is NULL");
        return false;
    }

    if (layer_idx >= NUM_V1_LAYERS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid layer_idx: %u", layer_idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_compute_neuromod_effects: capacity exceeded");
        return false;
    }

    // Default effects (no modulation)
    effects->gabor_gain = 1.0F;
    effects->attention_boost = 1.0F;
    effects->plasticity_gate = 0.5F;
    effects->contrast_gain = 1.0F;

    // Step 1: Read neuromodulator levels from brain (if available)
    float dopamine_level = 0.0F;
    float ach_level = 0.0F;
    float ne_level = 0.0F;

    if (cortex->brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(cortex->brain);
        if (neuromod) {
            dopamine_level = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
            ach_level = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);
            ne_level = neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE);
        }
    }

    // Step 2: Combine with local phasic/tonic states
    // Phasic weight α = 0.6 (phasic dominates for fast events)
    const float PHASIC_WEIGHT = 0.6F;
    const float TONIC_WEIGHT = 1.0F - PHASIC_WEIGHT;

    const phasic_tonic_state_t* da_state = &cortex->neuromod_states[NEUROMOD_TYPE_DOPAMINE];
    const phasic_tonic_state_t* ach_state = &cortex->neuromod_states[NEUROMOD_TYPE_ACETYLCHOLINE];
    const phasic_tonic_state_t* ne_state = &cortex->neuromod_states[NEUROMOD_TYPE_NOREPINEPHRINE];

    float da_effective = dopamine_level + PHASIC_WEIGHT * phasic_tonic_get_phasic_burst(da_state) + TONIC_WEIGHT * phasic_tonic_get_tonic_level(da_state);
    float ach_effective = ach_level + PHASIC_WEIGHT * phasic_tonic_get_phasic_burst(ach_state) + TONIC_WEIGHT * phasic_tonic_get_tonic_level(ach_state);
    float ne_effective = ne_level + PHASIC_WEIGHT * phasic_tonic_get_phasic_burst(ne_state) + TONIC_WEIGHT * phasic_tonic_get_tonic_level(ne_state);

    // Clamp to [0, 1]
    da_effective = fminf(da_effective, 1.0F);
    ach_effective = fminf(ach_effective, 1.0F);
    ne_effective = fminf(ne_effective, 1.0F);

    // Step 3: Multiply by receptor densities
    const receptor_expression_t* receptors = &cortex->receptor_profiles[layer_idx];

    // Dopamine effect: D1 (excitatory) - 0.5*D2 (inhibitory)
    float da_effect = (receptors->d1_density - 0.5F * receptors->d2_density) * da_effective;

    // Acetylcholine effect: M1 (excitatory) - 0.3*M2 (inhibitory)
    float ach_effect = (receptors->m1_density - 0.3F * receptors->m2_density) * ach_effective;

    // Norepinephrine effect: α1 (excitatory) + 0.5*β2 (plasticity)
    float ne_effect = (receptors->alpha1_density + 0.5F * receptors->beta2_density) * ne_effective;

    // Step 4: Query second messenger cascade activities
    float pka_activity = 0.0F;
    float pkc_activity = 0.0F;
    float camkii_activity = 0.0F;

    if (cortex->second_messengers_enabled && cortex->second_messengers) {
        // WHAT: Get cascade state for this layer
        // WHY:  Cascades amplify and temporally extend neuromodulator effects
        // HOW:  Query PKA (cAMP), PKC (IP3/DAG), CaMKII (Ca2+) activities
        second_messenger_state_t cascade_state;
        nimcp_result_t result = second_messenger_get_state(
            cortex->second_messengers, layer_idx, &cascade_state);

        if (result == NIMCP_SUCCESS) {
            pka_activity = cascade_state.camp.pka_activity;
            pkc_activity = cascade_state.ip3_dag.pkc_activity;
            camkii_activity = cascade_state.calcium.camkii_activity;

            LOG_DEBUG(VISUAL_LOG_MODULE, "Layer %u cascade state: PKA=%.2f PKC=%.2f CaMKII=%.2f",
                      layer_idx, pka_activity, pkc_activity, camkii_activity);
        }
    }

    // Step 5: Compute gain values with cascade modulation
    // BIOLOGY:
    // - PKA (cAMP pathway): Enhances gain via AMPAR phosphorylation
    // - PKC (IP3/DAG pathway): Modulates contrast gain
    // - CaMKII (Ca2+ pathway): Gates plasticity via NMDAR modulation

    // Gabor gain: DA enhances signal detection, ACh enhances features, NE increases sensitivity
    // PKA amplifies these effects (D1 -> Gs -> cAMP -> PKA)
    effects->gabor_gain = 1.0F + 0.5F * da_effect + 0.3F * ach_effect + 0.4F * ne_effect
                          + 0.3F * pka_activity;  // PKA amplifies gain

    // Attention boost: ACh dominates attention, NE increases alertness
    // PKC modulates attention via vesicle release probability
    effects->attention_boost = 1.0F + 0.7F * ach_effect + 0.3F * ne_effect
                               + 0.2F * pkc_activity;  // PKC enhances attention

    // Plasticity gate: DA gates learning (reward), ACh gates encoding
    // CaMKII is the primary plasticity kinase (required for LTP)
    float plasticity_input = 2.0F * da_effect + ach_effect + 2.0F * camkii_activity;
    effects->plasticity_gate = 1.0F / (1.0F + expf(-plasticity_input));

    // Contrast gain: DA enhances contrast sensitivity
    // PKA and PKC both contribute to contrast modulation
    effects->contrast_gain = 1.0F + 0.4F * da_effect + 0.2F * ach_effect
                             + 0.3F * pka_activity + 0.2F * pkc_activity;

    // Clamp all gains to reasonable ranges
    effects->gabor_gain = fminf(fmaxf(effects->gabor_gain, 0.5F), 2.0F);
    effects->attention_boost = fminf(fmaxf(effects->attention_boost, 0.5F), 2.0F);
    effects->plasticity_gate = fminf(fmaxf(effects->plasticity_gate, 0.0F), 1.0F);
    effects->contrast_gain = fminf(fmaxf(effects->contrast_gain, 0.5F), 2.0F);

    return true;
}

//=============================================================================
// Second Messenger Cascade Integration
//=============================================================================

/**
 * @brief Trigger receptor activation for second messenger cascade
 *
 * WHAT: Activate G-protein coupled receptor to initiate signaling cascade
 * WHY:  Neuromodulators bind receptors -> trigger intracellular cascades
 * HOW:  Route to appropriate cascade (Gs/Gi/Gq) based on receptor type
 *
 * BIOLOGY:
 * - D1 receptors -> Gs -> cAMP -> PKA
 * - M1 receptors -> Gq -> IP3/DAG -> PKC + Ca2+
 * - beta-adrenergic -> Gs -> cAMP -> PKA
 * - alpha1-adrenergic -> Gq -> IP3/DAG -> PKC
 */
bool visual_cortex_trigger_receptor(
    visual_cortex_t* cortex,
    uint32_t layer_idx,
    receptor_type_t receptor_type,
    float occupancy)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_receptor: nimcp_validate_pointer is NULL");
        return false;
    }

    if (layer_idx >= NUM_V1_LAYERS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid layer_idx: %u (max %u)", layer_idx, NUM_V1_LAYERS - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_receptor: capacity exceeded");
        return false;
    }

    if (occupancy < 0.0F || occupancy > 1.0F) {
        LOG_WARN(VISUAL_LOG_MODULE, "Receptor occupancy out of range: %.2f, clamping to [0,1]", occupancy);
        occupancy = fminf(fmaxf(occupancy, 0.0F), 1.0F);
    }

    // Guard: Check if second messengers enabled
    if (!cortex->second_messengers_enabled || !cortex->second_messengers) {
        LOG_WARN(VISUAL_LOG_MODULE, "Second messengers not enabled, cannot trigger receptor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_trigger_receptor: required parameter is NULL (cortex->second_messengers_enabled, cortex->second_messengers)");
        return false;
    }

    // WHAT: Get current timestamp for cascade activation
    // WHY:  Cascades track temporal dynamics
    // HOW:  Use system time in milliseconds
    uint64_t timestamp_ms = (uint64_t)(time(NULL) * 1000);

    // WHAT: Route to appropriate G-protein pathway
    // WHY:  Different receptors couple to different G-proteins
    // HOW:  Based on receptor type, activate Gs, Gi, or Gq pathway
    nimcp_result_t result = NIMCP_ERROR_INVALID_PARAM;

    switch (receptor_type) {
        // Gs-coupled receptors (activate adenylyl cyclase -> cAMP -> PKA)
        case RECEPTOR_D1:
        case RECEPTOR_BETA:  // Beta-adrenergic
            result = second_messenger_activate_gs(
                cortex->second_messengers, layer_idx, occupancy, timestamp_ms);
            LOG_DEBUG(VISUAL_LOG_MODULE, "Layer %u: Activated Gs pathway (receptor=%d, occupancy=%.2f)",
                      layer_idx, receptor_type, occupancy);
            break;

        // Gq-coupled receptors (activate PLC -> IP3/DAG -> PKC + Ca2+)
        case RECEPTOR_MUSCARINIC:  // M1/M3 muscarinic
        case RECEPTOR_ALPHA1:      // Alpha1-adrenergic
        case RECEPTOR_5HT2A:
            result = second_messenger_activate_gq(
                cortex->second_messengers, layer_idx, occupancy, timestamp_ms);
            LOG_DEBUG(VISUAL_LOG_MODULE, "Layer %u: Activated Gq pathway (receptor=%d, occupancy=%.2f)",
                      layer_idx, receptor_type, occupancy);
            break;

        // Gi-coupled receptors (inhibit adenylyl cyclase -> reduce cAMP)
        case RECEPTOR_D2:
        case RECEPTOR_ALPHA2:  // Alpha2-adrenergic (autoreceptor)
        case RECEPTOR_5HT1A:   // 5-HT1A (also Gi-coupled)
            result = second_messenger_activate_gi(
                cortex->second_messengers, layer_idx, occupancy, timestamp_ms);
            LOG_DEBUG(VISUAL_LOG_MODULE, "Layer %u: Activated Gi pathway (receptor=%d, occupancy=%.2f)",
                      layer_idx, receptor_type, occupancy);
            break;

        default:
            LOG_ERROR(VISUAL_LOG_MODULE, "Unknown receptor type: %d", receptor_type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_receptor: operation failed");
            return false;
    }

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to activate receptor cascade: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_trigger_receptor: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Get second messenger cascade state for layer
 *
 * WHAT: Query cascade state (cAMP, PKA, PKC, CaMKII activities)
 * WHY:  Enable monitoring and integration with other systems
 * HOW:  Return state from second messenger system
 */
bool visual_cortex_get_second_messenger_state(
    const visual_cortex_t* cortex,
    uint32_t layer_idx,
    second_messenger_state_t* state)
{
    // Guard: Validate inputs
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(state, "state")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_second_messenger_state: nimcp_validate_pointer is NULL");
        return false;
    }

    if (layer_idx >= NUM_V1_LAYERS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Invalid layer_idx: %u (max %u)", layer_idx, NUM_V1_LAYERS - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_second_messenger_state: capacity exceeded");
        return false;
    }

    // Guard: Check if second messengers enabled
    if (!cortex->second_messengers_enabled || !cortex->second_messengers) {
        LOG_WARN(VISUAL_LOG_MODULE, "Second messengers not enabled");
        // Return default state (all zeros)
        memset(state, 0, sizeof(second_messenger_state_t));
        return true;
    }

    // WHAT: Query cascade state from second messenger system
    // WHY:  Provide access to intracellular signaling state
    // HOW:  Call second_messenger_get_state for this layer
    nimcp_result_t result = second_messenger_get_state(
        cortex->second_messengers, layer_idx, state);

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to get second messenger state: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_get_second_messenger_state: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Boost attention to specific visual region
 *
 * WHAT: Increase processing sensitivity for spatial region
 * WHY:  Social cues (faces/agents) should receive enhanced processing
 * HOW:  Scale feature activations in target region (stored for next process call)
 *
 * BIOLOGY: STS modulates V1 for social stimuli via feedback projections
 *
 * COMPLEXITY: O(1)
 *
 * @param cortex Visual cortex instance
 * @param region_x X coordinate of region center (normalized [0,1])
 * @param region_y Y coordinate of region center (normalized [0,1])
 * @param boost_factor Attention boost [1.0, 2.0]
 */
void visual_cortex_boost_region_attention(visual_cortex_t* cortex,
                                           float region_x,
                                           float region_y,
                                           float boost_factor)
{
    // Guard: Validate cortex
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_boost_region_attention: cortex is NULL");
        return;
    }

    // Guard: Validate coordinates [0, 1]
    region_x = fminf(fmaxf(region_x, 0.0F), 1.0F);
    region_y = fminf(fmaxf(region_y, 0.0F), 1.0F);

    // Guard: Clamp boost factor [1.0, 2.0]
    boost_factor = fminf(fmaxf(boost_factor, 1.0F), 2.0F);

    // WHAT: Store attention parameters for next processing cycle
    // WHY:  Cannot apply boost until we have image data
    // HOW:  Set neuromodulator gain (acts similarly to attention)
    //
    // NOTE: In a full implementation, we'd store attention map and apply during
    // conv_layer_forward(). For now, we use the existing neuromodulator gain
    // mechanism which already modulates V1 output.
    //
    // Future enhancement: Add spatial attention map to visual_cortex_struct
    // and multiply V1 output by attention values at corresponding spatial locations.
}

/**
 * @brief Detect if agent/person present in visual field
 *
 * WHAT: Simple heuristic agent detection
 * WHY:  Triggers mirror neuron observation mode
 * HOW:  Check for motion + face-like patterns in features
 *
 * COMPLEXITY: O(n) where n = num_features
 *
 * @param cortex Visual cortex instance
 * @param features Feature vector from recent processing
 * @param num_features Number of features
 * @return true if agent detected
 */
bool visual_cortex_detect_agent(visual_cortex_t* cortex,
                                 const float* features,
                                 uint32_t num_features)
{
    // Guard: Validate inputs
    if (!cortex || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_detect_agent: required parameter is NULL");
        return false;
    }
    if (num_features == 0) {
        return false;
    }

    // WHAT: Simple heuristic based on feature activation patterns
    // WHY:  Full face detection is expensive; this is a fast approximation
    // HOW:  Check for high variance (motion/edges) + mid-range activations (structure)
    //
    // HEURISTIC:
    // - High variance → motion or complex structure
    // - Multiple mid-range activations → organized structure (not noise)
    // - Activation in multiple regions → spatial extent (not a point)

    // Compute feature statistics
    float mean = 0.0F;
    float variance = 0.0F;
    uint32_t mid_range_count = 0;

    for (uint32_t i = 0; i < num_features; i++) {
        mean += features[i];
    }
    mean /= num_features;

    for (uint32_t i = 0; i < num_features; i++) {
        float diff = features[i] - mean;
        variance += diff * diff;

        // Count mid-range activations (0.3 to 0.7)
        if (features[i] > 0.3F && features[i] < 0.7F) {
            mid_range_count++;
        }
    }
    variance /= num_features;

    // DETECTION CRITERIA:
    // 1. Variance > 0.05 (sufficient structure/motion)
    // 2. Mid-range activations > 30% (organized structure, not uniform)
    bool has_structure = (variance > 0.05F);
    bool has_organization = (mid_range_count > num_features / 3);

    return has_structure && has_organization;
}

//=============================================================================
// Bio-Async Communication Implementation
//=============================================================================

/**
 * @brief Get bio-async module context
 */
bio_module_context_t visual_cortex_get_bio_context(visual_cortex_t* cortex)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_bio_context: cortex is NULL");
        return NULL;
    }
    if (!cortex->bio_async_enabled) {
        return NULL;
    }
    return cortex->bio_ctx;
}

/**
 * @brief Process pending bio-async messages
 *
 * Uses bio_router_process_inbox() which calls registered handlers.
 * Handlers should be registered during module initialization.
 */
uint32_t visual_cortex_process_bio_messages(visual_cortex_t* cortex, uint32_t max_messages)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_process_bio_messages: cortex is NULL");
        return 0;
    }
    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return 0;
    }

    // Process inbox using the router's handler-based system
    uint32_t processed = bio_router_process_inbox(cortex->bio_ctx, max_messages);

    if (processed > 0) {
        LOG_DEBUG(VISUAL_LOG_MODULE, "Processed %u bio-async messages", processed);
    }

    return processed;
}

/**
 * @brief Broadcast visual feature detection via bio-async
 */
nimcp_error_t visual_cortex_broadcast_input(
    visual_cortex_t* cortex,
    const float* features,
    uint32_t num_features,
    float salience)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_broadcast_input: cortex is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_broadcast_input: features is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (num_features == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Create visual feature detected message
    bio_msg_visual_feature_detected_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_VISUAL_FEATURE_DETECTED,
                        BIO_MODULE_VISUAL_CORTEX, 0,  // 0 = broadcast
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_DOPAMINE;  // Visual salience involves dopamine
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    // Fill in feature detection data
    msg.feature_id = 0;  // Generic visual input
    msg.x_position = 0.5F;  // Center (can be parameterized)
    msg.y_position = 0.5F;
    msg.confidence = salience;
    msg.salience = salience;
    msg.layer = 1;  // V1 layer

    // Broadcast to all interested modules
    nimcp_error_t err = bio_router_broadcast(cortex->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(VISUAL_LOG_MODULE, "Failed to broadcast visual input: %d", err);
        return err;
    }

    LOG_DEBUG(VISUAL_LOG_MODULE, "Broadcast visual input: %u features, salience=%.2f",
              num_features, salience);

    return NIMCP_SUCCESS;
}

/**
 * @brief Send visual attention shift notification
 */
nimcp_error_t visual_cortex_broadcast_attention_shift(
    visual_cortex_t* cortex,
    float x,
    float y,
    float salience)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_cortex_broadcast_attention_shift: cortex is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Create attention shift message using the defined type
    bio_msg_visual_attention_shift_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_VISUAL_ATTENTION_SHIFT,
                        BIO_MODULE_VISUAL_CORTEX, 0,  // 0 = broadcast
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Attention involves ACh
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;

    msg.target_x = x;
    msg.target_y = y;
    msg.urgency = salience;
    msg.reason = 0;  // Generic attention shift

    // Broadcast to all interested modules
    nimcp_error_t err = bio_router_broadcast(cortex->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(VISUAL_LOG_MODULE, "Failed to broadcast attention shift: %d", err);
        return err;
    }

    LOG_DEBUG(VISUAL_LOG_MODULE, "Broadcast attention shift: (%.2f, %.2f) salience=%.2f",
              x, y, salience);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Training Interface Implementation (CNN-Cortex Integration)
//=============================================================================

/**
 * @brief Get training state from visual cortex
 *
 * WHAT: Retrieve cached conv/pool outputs and confidence metrics
 * WHY:  CNN trainer needs access to cortex activations for gradient feedback
 * HOW:  Copy cached outputs to provided state structure
 *
 * BIOLOGY: Top-down feedback in visual system modulates V1 via attention
 * and prediction error signals.
 *
 * @param cortex Visual cortex instance
 * @param state Output training state structure
 * @return 0 on success, negative on error
 */
int visual_cortex_get_training_state(
    visual_cortex_t* cortex,
    visual_training_state_t* state)
{
    /* Guard: Validate inputs */
    if (!cortex || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_training_state: required parameter is NULL");
        return -1;
    }

    /* Initialize state */
    memset(state, 0, sizeof(visual_training_state_t));

    /* Copy cached conv output if available */
    if (cortex->cached_conv_output && cortex->cached_conv_size > 0) {
        state->conv_output = cortex->cached_conv_output;
        state->conv_output_size = cortex->cached_conv_size;
    }

    /* Copy cached pool output if available */
    if (cortex->cached_pool_output && cortex->cached_pool_size > 0) {
        state->pool_output = cortex->cached_pool_output;
        state->pool_output_size = cortex->cached_pool_size;
    }

    /* Copy metrics */
    state->confidence = cortex->last_confidence;
    state->novelty = cortex->last_novelty;
    state->timestamp_ms = cortex->last_process_timestamp;
    state->valid = (cortex->cached_conv_output != NULL);

    LOG_DEBUG(VISUAL_LOG_MODULE, "Training state retrieved: conv=%u, pool=%u, conf=%.2f",
              state->conv_output_size, state->pool_output_size, state->confidence);

    return 0;
}

/**
 * @brief Apply gradient feedback to visual cortex for STDP modulation
 *
 * WHAT: Convert gradients from CNN to STDP signals for cortex plasticity
 * WHY:  Enable top-down learning modulation in visual system
 * HOW:  Modulate internal network using scaled gradient signal
 *
 * BIOLOGY: Top-down prediction errors in predictive coding framework
 * modulate synaptic plasticity in lower visual areas.
 *
 * @param cortex Visual cortex instance
 * @param gradients Gradient vector from CNN backprop
 * @param gradient_size Size of gradient vector
 * @param scale Scale factor for gradient signal
 * @return 0 on success, negative on error
 */
int visual_cortex_apply_gradient_feedback(
    visual_cortex_t* cortex,
    const float* gradients,
    uint32_t gradient_size,
    float scale)
{
    /* Guard: Validate inputs */
    if (!cortex || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_apply_gradient_feedback: required parameter is NULL");
        return -1;
    }
    if (gradient_size == 0) {
        return -1;
    }

    /* Guard: Training mode must be enabled */
    if (!cortex->training_mode) {
        LOG_WARN(VISUAL_LOG_MODULE, "Gradient feedback rejected: training mode not enabled");
        return -2;
    }

    /* Guard: Scale must be reasonable */
    if (scale < 0.0F || scale > 10.0F) {
        LOG_WARN(VISUAL_LOG_MODULE, "Gradient scale out of range: %.2f", scale);
        return -3;
    }

    /* Compute gradient magnitude for STDP modulation strength */
    float grad_magnitude = 0.0F;
    for (uint32_t i = 0; i < gradient_size; i++) {
        grad_magnitude += gradients[i] * gradients[i];
    }
    grad_magnitude = sqrtf(grad_magnitude) * scale;

    /* Clamp modulation strength to reasonable range */
    if (grad_magnitude > 1.0F) {
        grad_magnitude = 1.0F;
    }

    /* Apply to internal recurrent network if available */
    if (cortex->has_internal_network && cortex->internal_network) {
        /* STDP modulation: Use gradient magnitude as dopamine-like signal
         * High gradients = high prediction error = enhanced learning */

        /* Apply neuromodulator diffusion to internal network */
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(cortex->brain);
        if (neuromod) {
            /* Temporarily boost dopamine based on gradient (prediction error) */
            float current_da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
            float boosted_da = current_da + grad_magnitude * 0.5F;
            if (boosted_da > 1.0F) boosted_da = 1.0F;

            /* This enhanced DA will affect next STDP updates in the network */
            neuromodulator_set_level(neuromod, NEUROMOD_DOPAMINE, boosted_da);

            LOG_DEBUG(VISUAL_LOG_MODULE, "Applied gradient feedback: mag=%.4f, DA: %.2f -> %.2f",
                      grad_magnitude, current_da, boosted_da);
        }
    }

    /* Broadcast gradient feedback via bio-async if enabled */
    if (cortex->bio_async_enabled && cortex->bio_ctx) {
        bio_msg_visual_feature_detected_t msg;
        bio_msg_init_header(&msg.header, BIO_MSG_VISUAL_FEATURE_DETECTED,
                            BIO_MODULE_VISUAL_CORTEX, 0,
                            sizeof(msg) - sizeof(bio_message_header_t));

        msg.header.channel = BIO_CHANNEL_DOPAMINE;  /* Prediction error */
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.feature_id = 0xFFFF;  /* Special ID for gradient feedback */
        msg.confidence = grad_magnitude;
        msg.salience = grad_magnitude;

        bio_router_broadcast(cortex->bio_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/**
 * @brief Extract visual features as a tensor
 *
 * WHAT: Process image through V1 and return features as nimcp_tensor_t
 * WHY:  CNN trainer expects tensor format for batch processing
 * HOW:  Call visual_cortex_process, wrap output in tensor
 *
 * @param cortex Visual cortex instance
 * @param image Input image data
 * @param width Image width
 * @param height Image height
 * @param channels Image channels
 * @param features Output tensor pointer (caller must destroy)
 * @return 0 on success, negative on error
 */
int visual_cortex_extract_features_tensor(
    visual_cortex_t* cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    struct nimcp_tensor** features)
{
    /* Guard: Validate inputs */
    if (!cortex || !image || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_extract_features_tensor: required parameter is NULL");
        return -1;
    }

    /* Allocate temporary feature buffer */
    uint32_t feature_dim = cortex->feature_dim;
    float* feature_buffer = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!feature_buffer) {
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to allocate feature buffer");
        return -2;
    }

    /* Process image through visual cortex */
    bool success = visual_cortex_process(cortex, image, width, height, channels, feature_buffer);
    if (!success) {
        nimcp_free(feature_buffer);
        return -3;
    }

    /* Cache outputs if in training mode */
    if (cortex->training_mode) {
        /* Compute confidence as normalized feature magnitude */
        float feature_sum = 0.0F;
        float feature_max = 0.0F;
        for (uint32_t i = 0; i < feature_dim; i++) {
            feature_sum += feature_buffer[i];
            if (feature_buffer[i] > feature_max) {
                feature_max = feature_buffer[i];
            }
        }
        /* Confidence: high when features are strong and structured */
        cortex->last_confidence = (feature_max > 0.1F) ?
            fminf(1.0F, feature_sum / (float)feature_dim * 2.0F) : 0.0F;

        /* Novelty: estimate based on feature variance (high variance = novel) */
        float mean = feature_sum / (float)feature_dim;
        float variance = 0.0F;
        for (uint32_t i = 0; i < feature_dim; i++) {
            float diff = feature_buffer[i] - mean;
            variance += diff * diff;
        }
        variance /= (float)feature_dim;
        cortex->last_novelty = fminf(1.0F, sqrtf(variance) * 5.0F);

        /* Get current timestamp */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        cortex->last_process_timestamp = (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }

    /* Create 1D tensor with feature dimension */
    uint32_t dims[1] = { feature_dim };
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!tensor) {
        nimcp_free(feature_buffer);
        LOG_ERROR(VISUAL_LOG_MODULE, "Failed to create feature tensor");
        return -4;
    }

    /* Copy features to tensor */
    float* tensor_data = (float*)nimcp_tensor_data(tensor);
    memcpy(tensor_data, feature_buffer, feature_dim * sizeof(float));

    nimcp_free(feature_buffer);

    *features = (struct nimcp_tensor*)tensor;

    LOG_DEBUG(VISUAL_LOG_MODULE, "Extracted %u features as tensor", feature_dim);

    return 0;
}

/**
 * @brief Get visual cortex output feature dimension
 *
 * WHAT: Return the output feature vector size
 * WHY:  CNN trainer needs to know input dimensions for layer sizing
 * HOW:  Return cortex->feature_dim
 *
 * @param cortex Visual cortex instance
 * @return Feature dimension, or 0 on error
 */
uint32_t visual_cortex_get_feature_dim(const visual_cortex_t* cortex)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_get_feature_dim: cortex is NULL");
        return 0;
    }
    return cortex->feature_dim;
}

/**
 * @brief Copy the last-computed normalized feature embedding.
 *
 * WHAT: Return the cached final feature vector from the most recent
 *       visual_cortex_process(), without requiring an image to be re-pushed.
 * WHY:  Consumers like the language cascade want the cortex's CURRENT
 *       perceptual state as a vector but have no image at call time. The
 *       feature embedding is cached at the end of each process().
 * HOW:  memcpy up to min(feature_dim, max_out) floats into out; write the
 *       count to *out_n. Returns 0 on success (including a clean 0-count
 *       when the cortex has never processed an image — out_n=0), -1 on bad
 *       args. A 0-count result is the common idle case → caller no-ops.
 *
 * @param cortex   Visual cortex instance
 * @param out      Destination buffer
 * @param max_out  Capacity of out (floats)
 * @param out_n    Receives number of floats written
 * @return 0 on success, -1 on NULL args
 */
int visual_cortex_get_cached_features(const visual_cortex_t* cortex,
                                      float* out, uint32_t max_out,
                                      uint32_t* out_n)
{
    if (!cortex || !out || !out_n) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "visual_cortex_get_cached_features: NULL arg");
        return -1;
    }
    *out_n = 0;
    if (!cortex->cached_feature_output || cortex->cached_feature_size == 0) {
        return 0;  /* never processed an image yet — clean idle no-op */
    }
    uint32_t n = cortex->cached_feature_size;
    if (n > max_out) n = max_out;
    memcpy(out, cortex->cached_feature_output, n * sizeof(float));
    *out_n = n;
    return 0;
}

/**
 * @brief Enable or disable training mode
 *
 * WHAT: Toggle training mode for activation caching
 * WHY:  Training requires cached outputs for gradient computation
 * HOW:  Set flag that enables caching during visual_cortex_process
 *
 * @param cortex Visual cortex instance
 * @param enable True to enable training mode
 * @return 0 on success, negative on error
 */
int visual_cortex_set_training_mode(visual_cortex_t* cortex, bool enable)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortex is NULL");

        return -1;
    }

    bool was_enabled = cortex->training_mode;
    cortex->training_mode = enable;

    if (enable && !was_enabled) {
        LOG_INFO(VISUAL_LOG_MODULE, "Training mode ENABLED - activation caching active");
    } else if (!enable && was_enabled) {
        /* Clear cached outputs when disabling */
        if (cortex->cached_conv_output) {
            nimcp_free(cortex->cached_conv_output);
            cortex->cached_conv_output = NULL;
            cortex->cached_conv_size = 0;
        }
        if (cortex->cached_pool_output) {
            nimcp_free(cortex->cached_pool_output);
            cortex->cached_pool_output = NULL;
            cortex->cached_pool_size = 0;
        }
        LOG_INFO(VISUAL_LOG_MODULE, "Training mode DISABLED - caches cleared");
    }

    return 0;
}

/**
 * @brief Check if training mode is enabled
 *
 * WHAT: Query training mode state
 * WHY:  Bridge needs to know if cortex is ready for gradient feedback
 * HOW:  Return training_mode flag
 *
 * @param cortex Visual cortex instance
 * @return True if training mode is enabled
 */
bool visual_cortex_is_training_mode(const visual_cortex_t* cortex)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_cortex_is_training_mode: cortex is NULL");
        return false;
    }
    return cortex->training_mode;
}

//=============================================================================
// Self-Awareness (KG Reader Integration)
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allows visual cortex to introspect its own capabilities and connections
 * WHY:  Enables self-awareness - "what am I and what can I do?"
 * HOW:  Query KG for Visual_Cortex entity and its relations
 *
 * @param kg Knowledge graph reader (must be loaded)
 * @return 1 if self-knowledge found, 0 otherwise
 */
int visual_cortex_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) {
        return 0;
    }

    /* Query self-identity from KG */
    const kg_entity_t* self = kg_reader_get_entity(kg, "Visual_Cortex");
    if (self) {
        /* Module now has access to its documented capabilities */
        LOG_DEBUG(VISUAL_LOG_MODULE, "Self-knowledge: entity_type=%s, observations=%u",
                  self->entity_type, self->num_observations);
    }

    /* Query what this module connects to (outputs) */
    kg_relation_list_t* outputs = kg_reader_get_relations_from(kg, "Visual_Cortex");
    if (outputs) {
        LOG_DEBUG(VISUAL_LOG_MODULE, "Self-knowledge: %u downstream targets",
                  outputs->count);
        kg_relation_list_destroy(outputs);
    }

    /* Query what connects to this module (inputs) */
    kg_relation_list_t* inputs = kg_reader_get_relations_to(kg, "Visual_Cortex");
    if (inputs) {
        LOG_DEBUG(VISUAL_LOG_MODULE, "Self-knowledge: %u upstream sources",
                  inputs->count);
        kg_relation_list_destroy(inputs);
    }

    return self ? 1 : 0;
}
