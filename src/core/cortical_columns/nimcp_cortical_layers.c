/**
 * @file nimcp_cortical_layers.c
 * @brief Implementation of cortical layer organization and processing
 *
 * WHAT: Six-layer cortical architecture with canonical microcircuit dynamics
 * WHY:  Biological cortex uses laminar organization for hierarchical processing
 * HOW:  Layer-specific computations, inter-layer connectivity, recurrent dynamics
 *
 * IMPLEMENTATION NOTES:
 * - Thread-safe using nimcp_platform_mutex
 * - Memory managed with nimcp_malloc/nimcp_free
 * - Guard clauses on all public functions
 * - Logging via nimcp_log
 * - All functions under 50 lines
 *
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include "core/cortical_columns/nimcp_cortical_layers.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#define LOG_MODULE "cortical_layers"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_layers)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static _Atomic bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void cortical_layers_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_CORTICAL_LAYERS,
        .module_name = "cortical_layers",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for cortical_layers module");
    }
}

__attribute__((constructor))
static void cortical_layers_bio_init(void) {
    pthread_once(&bio_init_once, cortical_layers_bio_init_impl);
}

__attribute__((destructor))
static void cortical_layers_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for cortical_layers module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Constants
//=============================================================================

#define MAX_NEURONS_PER_LAYER 10000
#define MIN_NEURONS_PER_LAYER 100
#define DEFAULT_TIME_CONSTANT 0.010f  // 10ms
#define SIGMOID_STEEPNESS 1.0f
#define NORMALIZATION_SIGMA 0.1f
#define NORMALIZATION_EXPONENT 2.0f
#define BURST_THRESHOLD 0.8f
#define PREDICTION_LEARNING_RATE NIMCP_DEFAULT_LEARNING_RATE

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Internal state for a single cortical layer
 * WHY:  Each layer has distinct dynamics and connectivity
 * HOW:  Stores neurons, connections, and processing parameters
 */
typedef struct layer_state {
    cortical_layer_config_t config;      /**< Layer configuration */
    float* neurons;                       /**< Neuron activation states */
    float* inputs;                        /**< Input accumulator */
    float* outputs;                       /**< Output buffer */
    uint32_t neuron_count;               /**< Number of neurons */
    float mean_activation;               /**< Running mean */
    float variance_activation;           /**< Running variance */
} layer_state_t;

/**
 * WHAT: Connection between two layers
 * WHY:  Represent synaptic projections with adjustable strength
 * HOW:  Stores source, target, and weight
 */
typedef struct layer_connection {
    cc_cortical_layer_t from;               /**< Source layer */
    cc_cortical_layer_t to;                 /**< Target layer */
    float strength;                      /**< Synaptic weight */
    bool is_feedback;                    /**< Feedback vs feedforward */
} layer_connection_t;

/**
 * WHAT: Complete laminar structure implementation
 * WHY:  Encapsulate all layers and their interconnections
 * HOW:  Array of layer states plus connection matrix
 */
struct laminar_structure {
    layer_state_t layers[CC_LAYER_COUNT];           /**< Layer states */
    layer_connection_t connections[CC_LAYER_COUNT * CC_LAYER_COUNT]; /**< Connectivity */
    uint32_t connection_count;                            /**< Active connections */
    float prediction_buffer[MAX_NEURONS_PER_LAYER];      /**< Layer VI predictions */
    uint64_t update_count;                                /**< Total updates */
    nimcp_platform_mutex_t mutex;                         /**< Thread safety */
};

//=============================================================================
// Static Helper Functions
//=============================================================================

/**
 * WHAT: Sigmoid activation function
 * WHY:  Non-linear transform for neural activation
 * HOW:  f(x) = 1 / (1 + exp(-k*x))
 */
static float sigmoid(float x, float steepness) {
    return 1.0F / (1.0F + expf(-steepness * x));
}

/**
 * WHAT: Divisive normalization (Heeger 1992)
 * WHY:  Layer IV uses normalization for contrast invariance
 * HOW:  r_i = c_i^n / (σ^n + Σ c_j^n)
 */
static float divisive_normalize(float value, float sum_squares, float sigma) {
    float numerator = powf(fabsf(value), NORMALIZATION_EXPONENT);
    float denominator = powf(sigma, NORMALIZATION_EXPONENT) + sum_squares;
    return numerator / (denominator + 1e-6F);  // Avoid division by zero
}

/**
 * WHAT: Compute sum of squares for normalization
 * WHY:  Divisive normalization denominator term
 * HOW:  Σ c_j^n
 */
static float compute_sum_squares(const float* values, uint32_t count) {
    float sum = 0.0F;
    for (uint32_t i = 0; i < count; i++) {
        sum += powf(fabsf(values[i]), NORMALIZATION_EXPONENT);
    }
    return sum;
}

/**
 * WHAT: Initialize a single layer state
 * WHY:  Set up memory and initial conditions for layer
 * HOW:  Allocate arrays, zero initialize
 */
static bool init_layer_state(layer_state_t* layer,
                             const cortical_layer_config_t* config) {
    // Guard clause
    if (!layer || !config) {
        LOG_ERROR("Invalid parameters to init_layer_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_sum_squares: required parameter is NULL (layer, config)");
        return false;
    }

    // Copy configuration
    layer->config = *config;

    // Determine neuron count based on density
    layer->neuron_count = config->neuron_density;
    if (layer->neuron_count > MAX_NEURONS_PER_LAYER) {
        layer->neuron_count = MAX_NEURONS_PER_LAYER;
        LOG_WARNING("Layer %d clamped to %u neurons",
                   config->layer, MAX_NEURONS_PER_LAYER);
    }

    // Allocate neuron arrays
    layer->neurons = nimcp_calloc(layer->neuron_count, sizeof(float));
    layer->inputs = nimcp_calloc(layer->neuron_count, sizeof(float));
    layer->outputs = nimcp_calloc(layer->neuron_count, sizeof(float));

    if (!layer->neurons || !layer->inputs || !layer->outputs) {
        LOG_ERROR("Failed to allocate layer arrays");
        nimcp_free(layer->neurons);
        nimcp_free(layer->inputs);
        nimcp_free(layer->outputs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_sum_squares: required parameter is NULL (layer->neurons, layer->inputs, layer->outputs)");
        return false;
    }

    // Initialize statistics
    layer->mean_activation = 0.0F;
    layer->variance_activation = 0.0F;

    return true;
}

/**
 * WHAT: Clean up a single layer state
 * WHY:  Free allocated memory for layer
 * HOW:  Release all arrays
 */
static void cleanup_layer_state(layer_state_t* layer) {
    if (!layer) return;

    nimcp_free(layer->neurons);
    nimcp_free(layer->inputs);
    nimcp_free(layer->outputs);

    layer->neurons = NULL;
    layer->inputs = NULL;
    layer->outputs = NULL;
    layer->neuron_count = 0;
}

/**
 * WHAT: Update layer statistics (mean and variance)
 * WHY:  Track layer health and activity levels
 * HOW:  Online algorithm for running statistics
 */
static void update_layer_stats(layer_state_t* layer) {
    if (!layer || !layer->neurons || layer->neuron_count == 0) return;

    // Compute mean
    float sum = 0.0F;
    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        sum += layer->neurons[i];
    }
    layer->mean_activation = sum / (float)layer->neuron_count;

    // Compute variance
    float var_sum = 0.0F;
    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        float diff = layer->neurons[i] - layer->mean_activation;
        var_sum += diff * diff;
    }
    layer->variance_activation = var_sum / (float)layer->neuron_count;
}

//=============================================================================
// Layer-Specific Processing Functions
//=============================================================================

/**
 * WHAT: Layer I processing (multiplicative modulation)
 * WHY:  Top-down attention acts as gain control
 * HOW:  Multiply inputs by modulation signal
 */
static void process_layer_I(layer_state_t* layer) {
    if (!layer) return;

    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        // Simple gain modulation
        layer->neurons[i] = layer->inputs[i] * 0.5F;  // Attenuate
        layer->outputs[i] = layer->neurons[i];
        layer->inputs[i] = 0.0F;  // Clear for next cycle
    }
}

/**
 * WHAT: Layer II/III processing (recurrent dynamics)
 * WHY:  Lateral integration and feature binding
 * HOW:  τ·dx/dt = -x + f(Wx + I), Euler integration
 */
static void process_layer_II_III(layer_state_t* layer, float dt) {
    if (!layer) return;

    const float tau = DEFAULT_TIME_CONSTANT;
    const float decay = dt / tau;
    const float input_threshold = 1e-8F;  // Very low threshold - divisive normalization produces small values

    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        // Recurrent dynamics: decay old state, add new input
        float activation = layer->neurons[i];
        float input_current = layer->inputs[i];

        // Only drive activation when there's meaningful input
        // Otherwise, apply pure exponential decay
        if (fabsf(input_current) > input_threshold) {
            // Scale input for sigmoid (divisive normalization gives small values)
            // Use input directly in sigmoid, which handles the scaling
            float f_input = sigmoid(input_current * 10.0F, SIGMOID_STEEPNESS);
            activation += decay * (-activation + f_input);
        } else {
            // No input: pure decay toward zero
            activation *= (1.0F - decay);
        }

        layer->neurons[i] = activation;
        layer->outputs[i] = activation;
        layer->inputs[i] = 0.0F;
    }
}

/**
 * WHAT: Layer IV processing (divisive normalization with decay)
 * WHY:  Contrast-invariant feature detection with persistent activation
 * HOW:  Heeger normalization model with exponential decay when no input
 */
static void process_layer_IV(layer_state_t* layer) {
    if (!layer) return;

    const float decay_rate = 0.9F;  // Activation persistence (90% retention per step)

    // Compute normalization pool
    float sum_squares = compute_sum_squares(layer->inputs, layer->neuron_count);

    // Check if there's meaningful input
    bool has_input = sum_squares > 1e-6F;

    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        if (has_input) {
            // Normal processing with new input
            float normalized = divisive_normalize(
                layer->inputs[i],
                sum_squares,
                NORMALIZATION_SIGMA
            );
            layer->neurons[i] = normalized;
        } else {
            // No new input: decay existing activation
            layer->neurons[i] *= decay_rate;
        }
        layer->outputs[i] = layer->neurons[i];
        layer->inputs[i] = 0.0F;
    }
}

/**
 * WHAT: Layer V processing (bursting output neurons with integration)
 * WHY:  Generate strong output signals for motor commands
 * HOW:  Integrate inputs over time, threshold with burst amplification
 */
static void process_layer_V(layer_state_t* layer) {
    if (!layer) return;

    const float integration_rate = 0.3F;  // Rate of input integration
    const float decay_rate = 0.8F;        // Decay when no input
    const float input_scale = 20.0F;      // Scale small inputs up

    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        float input = layer->inputs[i];
        float current = layer->neurons[i];

        // Integrate new input with existing state (scale small normalized values)
        if (input > 1e-8F) {
            // Has input: scale up and integrate
            float scaled_input = input * input_scale;
            current = current * (1.0F - integration_rate) + scaled_input * integration_rate;

            // Threshold and burst
            if (current > BURST_THRESHOLD) {
                current = 1.0F;  // Burst spike
            } else {
                // Apply sigmoid to integrated value
                current = sigmoid(current, SIGMOID_STEEPNESS);
            }
        } else {
            // No input: decay
            current *= decay_rate;
        }

        layer->neurons[i] = current;
        layer->outputs[i] = current;
        layer->inputs[i] = 0.0F;
    }
}

/**
 * WHAT: Layer VI processing (predictive coding)
 * WHY:  Generate predictions for lower layers
 * HOW:  Compute prediction error, update predictions
 */
static void process_layer_VI(layer_state_t* layer,
                             const float* actual NIMCP_UNUSED,
                             float* prediction_buffer) {
    if (!layer) return;

    for (uint32_t i = 0; i < layer->neuron_count; i++) {
        // Compute prediction error
        float error = layer->inputs[i] - prediction_buffer[i];

        // Update prediction with learning
        prediction_buffer[i] += PREDICTION_LEARNING_RATE * error;

        // Store error signal
        layer->neurons[i] = error;
        layer->outputs[i] = prediction_buffer[i];  // Send prediction

        layer->inputs[i] = 0.0F;
    }
}

//=============================================================================
// Public API: Layer Configuration
//=============================================================================

cortical_layer_config_t cortical_layer_get_default_config(cc_cortical_layer_t layer) {
    cortical_layer_config_t config = {0};
    config.layer = layer;

    switch (layer) {
        case CC_LAYER_I:
            config.thickness_ratio = 0.05F;
            config.neuron_density = 500;
            config.excitatory_ratio = 0.60F;
            config.default_connectivity = 0.10F;
            break;

        case CC_LAYER_II_III:
            config.thickness_ratio = 0.40F;
            config.neuron_density = 2000;
            config.excitatory_ratio = 0.80F;
            config.default_connectivity = 0.30F;
            break;

        case CC_LAYER_IV:
            config.thickness_ratio = 0.15F;
            config.neuron_density = 3000;
            config.excitatory_ratio = 0.85F;
            config.default_connectivity = 0.40F;
            break;

        case CC_LAYER_V:
            config.thickness_ratio = 0.20F;
            config.neuron_density = 1500;
            config.excitatory_ratio = 0.75F;
            config.default_connectivity = 0.25F;
            break;

        case CC_LAYER_VI:
            config.thickness_ratio = 0.20F;
            config.neuron_density = 1200;
            config.excitatory_ratio = 0.70F;
            config.default_connectivity = 0.20F;
            break;

        default:
            LOG_ERROR("Invalid layer type: %d", layer);
            break;
    }

    return config;
}

void cortical_layer_set_config(cortical_layer_config_t* config,
                               cc_cortical_layer_t layer) {
    // Guard clause
    if (!config) {
        LOG_ERROR("NULL config pointer");
        return;
    }

    // Validate and clamp parameters
    if (config->thickness_ratio <= 0.0F || config->thickness_ratio > 1.0F) {
        LOG_WARNING("Invalid thickness_ratio %f, clamping",
                   config->thickness_ratio);
        config->thickness_ratio = (config->thickness_ratio <= 0.0F) ? 0.01F : 1.0F;
    }

    if (config->excitatory_ratio <= 0.0F || config->excitatory_ratio > 1.0F) {
        LOG_WARNING("Invalid excitatory_ratio %f, clamping",
                   config->excitatory_ratio);
        config->excitatory_ratio = (config->excitatory_ratio <= 0.0F) ? 0.5F : 1.0F;
    }

    config->layer = layer;
    LOG_DEBUG("Set config for layer %d", layer);
}

const char* cortical_layer_get_name(cc_cortical_layer_t layer) {
    static const char* names[] = {
        "Layer I",
        "Layer II/III",
        "Layer IV",
        "Layer V",
        "Layer VI"
    };

    if (layer >= 0 && layer < CC_LAYER_COUNT) {
        return names[layer];
    }
    return "Unknown Layer";
}

const char* cortical_layer_get_description(cc_cortical_layer_t layer) {
    static const char* descriptions[] = {
        "Apical dendrites, top-down modulation, feedback processing",
        "Cortico-cortical projections, lateral association, feature binding",
        "Thalamic input processing, feedforward feature detection, granular layer",
        "Subcortical output, decision signals, action commands, pyramidal layer",
        "Corticothalamic feedback, predictive coding, error computation"
    };

    if (layer >= 0 && layer < CC_LAYER_COUNT) {
        return descriptions[layer];
    }
    return "Unknown layer type";
}

//=============================================================================
// Public API: Laminar Structure Lifecycle
//=============================================================================

laminar_structure_t* laminar_structure_create(
    const cortical_layer_config_t configs[CC_LAYER_COUNT]) {

    // Allocate main structure
    laminar_structure_t* ls = nimcp_calloc(1, sizeof(laminar_structure_t));
    if (!ls) {
        LOG_ERROR("Failed to allocate laminar structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ls is NULL");

        return NULL;
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&ls->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        nimcp_free(ls);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_layer_get_description: validation failed");
        return NULL;
    }

    // Initialize each layer
    bool success = true;
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        cortical_layer_config_t config;

        if (configs) {
            config = configs[i];
        } else {
            config = cortical_layer_get_default_config((cc_cortical_layer_t)i);
        }

        if (!init_layer_state(&ls->layers[i], &config)) {
            LOG_ERROR("Failed to initialize layer %d", i);
            success = false;
            break;
        }
    }

    if (!success) {
        laminar_structure_destroy(ls);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    // Initialize other fields
    ls->connection_count = 0;
    ls->update_count = 0;
    memset(ls->prediction_buffer, 0, sizeof(ls->prediction_buffer));

    LOG_INFO("Created laminar structure with %d layers", CC_LAYER_COUNT);
    return ls;
}

void laminar_structure_destroy(laminar_structure_t* ls) {
    // Guard clause
    if (!ls) return;

    // Clean up all layers
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        cleanup_layer_state(&ls->layers[i]);
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&ls->mutex);

    // Free main structure
    nimcp_free(ls);

    LOG_DEBUG("Destroyed laminar structure");
}

//=============================================================================
// Public API: Layer Input Processing
//=============================================================================

void laminar_process_input(laminar_structure_t* ls,
                          cc_cortical_layer_t target,
                          const float* input,
                          uint32_t size) {
    // Guard clauses
    if (!ls || !input) {
        LOG_ERROR("NULL parameter to laminar_process_input");
        return;
    }

    if (target < 0 || target >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid target layer: %d", target);
        return;
    }

    // Lock for thread safety
    nimcp_platform_mutex_lock(&ls->mutex);

    layer_state_t* layer = &ls->layers[target];
    uint32_t copy_size = (size < layer->neuron_count) ? size : layer->neuron_count;

    // Accumulate input
    for (uint32_t i = 0; i < copy_size; i++) {
        layer->inputs[i] += input[i];
    }

    nimcp_platform_mutex_unlock(&ls->mutex);

    LOG_DEBUG("Processed input to layer %d, size %u", target, copy_size);
}

//=============================================================================
// Public API: Inter-Layer Processing
//=============================================================================

void laminar_process_feedforward(laminar_structure_t* ls) {
    // Guard clause
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    const float dt = 0.001F;  // 1ms timestep

    // Process Layer IV (thalamic input)
    process_layer_IV(&ls->layers[CC_LAYER_IV]);

    // Layer IV → Layer II/III
    layer_state_t* layer4 = &ls->layers[CC_LAYER_IV];
    layer_state_t* layer23 = &ls->layers[CC_LAYER_II_III];
    uint32_t transfer_size = (layer4->neuron_count < layer23->neuron_count) ?
                            layer4->neuron_count : layer23->neuron_count;
    for (uint32_t i = 0; i < transfer_size; i++) {
        layer23->inputs[i] += layer4->outputs[i] * 1.0F;  // Full strength
    }

    // Process Layer II/III (recurrent integration)
    process_layer_II_III(&ls->layers[CC_LAYER_II_III], dt);

    // Layer II/III → Layer V
    layer_state_t* layer5 = &ls->layers[CC_LAYER_V];
    transfer_size = (layer23->neuron_count < layer5->neuron_count) ?
                   layer23->neuron_count : layer5->neuron_count;
    for (uint32_t i = 0; i < transfer_size; i++) {
        layer5->inputs[i] += layer23->outputs[i] * 0.8F;  // Moderate strength
    }

    // Process Layer V (output bursting)
    process_layer_V(&ls->layers[CC_LAYER_V]);

    ls->update_count++;

    nimcp_platform_mutex_unlock(&ls->mutex);
}

void laminar_process_feedback(laminar_structure_t* ls) {
    // Guard clause
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    // Layer VI → Layer IV (modulatory feedback)
    layer_state_t* layer6 = &ls->layers[CC_LAYER_VI];
    layer_state_t* layer4 = &ls->layers[CC_LAYER_IV];

    // Process Layer VI prediction
    process_layer_VI(&ls->layers[CC_LAYER_VI],
                    layer4->neurons,
                    ls->prediction_buffer);

    // Apply feedback modulation to Layer IV
    uint32_t transfer_size = (layer6->neuron_count < layer4->neuron_count) ?
                            layer6->neuron_count : layer4->neuron_count;
    for (uint32_t i = 0; i < transfer_size; i++) {
        layer4->inputs[i] += layer6->outputs[i] * 0.5F;  // Modulatory
    }

    // Layer I → Layer II/III (attentional modulation)
    layer_state_t* layer1 = &ls->layers[CC_LAYER_I];
    layer_state_t* layer23 = &ls->layers[CC_LAYER_II_III];

    process_layer_I(&ls->layers[CC_LAYER_I]);

    transfer_size = (layer1->neuron_count < layer23->neuron_count) ?
                   layer1->neuron_count : layer23->neuron_count;
    for (uint32_t i = 0; i < transfer_size; i++) {
        // Multiplicative modulation (gain control)
        layer23->inputs[i] *= (1.0F + layer1->outputs[i] * 0.4F);
    }

    // Process Layer II/III with the modulated inputs
    const float dt = 0.001F;  // 1ms timestep
    process_layer_II_III(layer23, dt);

    nimcp_platform_mutex_unlock(&ls->mutex);
}

void laminar_process_lateral(laminar_structure_t* ls) {
    // Guard clause
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    layer_state_t* layer23 = &ls->layers[CC_LAYER_II_III];

    // Simple lateral connections (neighbor coupling)
    // In full implementation, this would use a weight matrix
    for (uint32_t i = 1; i < layer23->neuron_count - 1; i++) {
        float left = layer23->neurons[i - 1];
        float right = layer23->neurons[i + 1];
        layer23->inputs[i] += (left + right) * 0.1F;  // Weak lateral
    }

    nimcp_platform_mutex_unlock(&ls->mutex);
}

//=============================================================================
// Public API: Layer Output
//=============================================================================

void laminar_get_output(laminar_structure_t* ls,
                       cc_cortical_layer_t layer,
                       float* output,
                       uint32_t size) {
    // Guard clauses
    if (!ls || !output) {
        LOG_ERROR("NULL parameter to laminar_get_output");
        return;
    }

    if (layer < 0 || layer >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid layer: %d", layer);
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    layer_state_t* layer_state = &ls->layers[layer];
    uint32_t copy_size = (size < layer_state->neuron_count) ?
                        size : layer_state->neuron_count;

    memcpy(output, layer_state->outputs, copy_size * sizeof(float));

    nimcp_platform_mutex_unlock(&ls->mutex);
}

float laminar_get_layer_activation(laminar_structure_t* ls,
                                   cc_cortical_layer_t layer) {
    // Guard clauses
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return 0.0F;
    }

    if (layer < 0 || layer >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid layer: %d", layer);
        return 0.0F;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    update_layer_stats(&ls->layers[layer]);
    float activation = ls->layers[layer].mean_activation;

    nimcp_platform_mutex_unlock(&ls->mutex);

    return activation;
}

uint32_t laminar_get_layer_neuron_count(const laminar_structure_t* ls,
                                        cc_cortical_layer_t layer) {
    // Guard clauses
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return 0;
    }

    if (layer < 0 || layer >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid layer: %d", layer);
        return 0;
    }

    return ls->layers[layer].neuron_count;
}

//=============================================================================
// Public API: Connectivity Configuration
//=============================================================================

void laminar_connect_feedforward(laminar_structure_t* ls,
                                 cc_cortical_layer_t from,
                                 cc_cortical_layer_t to,
                                 float strength) {
    // Guard clauses
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    if (from < 0 || from >= CC_LAYER_COUNT ||
        to < 0 || to >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid layer indices: %d -> %d", from, to);
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    // Find or create connection
    bool found = false;
    for (uint32_t i = 0; i < ls->connection_count; i++) {
        if (ls->connections[i].from == from &&
            ls->connections[i].to == to &&
            !ls->connections[i].is_feedback) {
            ls->connections[i].strength = strength;
            found = true;
            break;
        }
    }

    if (!found && ls->connection_count < CC_LAYER_COUNT * CC_LAYER_COUNT) {
        ls->connections[ls->connection_count].from = from;
        ls->connections[ls->connection_count].to = to;
        ls->connections[ls->connection_count].strength = strength;
        ls->connections[ls->connection_count].is_feedback = false;
        ls->connection_count++;
    }

    nimcp_platform_mutex_unlock(&ls->mutex);

    LOG_DEBUG("Set feedforward connection %d -> %d, strength %.2f",
             from, to, strength);
}

void laminar_connect_feedback(laminar_structure_t* ls,
                              cc_cortical_layer_t from,
                              cc_cortical_layer_t to,
                              float strength) {
    // Guard clauses
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    if (from < 0 || from >= CC_LAYER_COUNT ||
        to < 0 || to >= CC_LAYER_COUNT) {
        LOG_ERROR("Invalid layer indices: %d -> %d", from, to);
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    // Find or create connection
    bool found = false;
    for (uint32_t i = 0; i < ls->connection_count; i++) {
        if (ls->connections[i].from == from &&
            ls->connections[i].to == to &&
            ls->connections[i].is_feedback) {
            ls->connections[i].strength = strength;
            found = true;
            break;
        }
    }

    if (!found && ls->connection_count < CC_LAYER_COUNT * CC_LAYER_COUNT) {
        ls->connections[ls->connection_count].from = from;
        ls->connections[ls->connection_count].to = to;
        ls->connections[ls->connection_count].strength = strength;
        ls->connections[ls->connection_count].is_feedback = true;
        ls->connection_count++;
    }

    nimcp_platform_mutex_unlock(&ls->mutex);

    LOG_DEBUG("Set feedback connection %d -> %d, strength %.2f",
             from, to, strength);
}

//=============================================================================
// Public API: Canonical Microcircuit
//=============================================================================

void laminar_apply_canonical_circuit(laminar_structure_t* ls) {
    // Guard clause
    if (!ls) {
        LOG_ERROR("NULL laminar structure");
        return;
    }

    LOG_INFO("Applying canonical microcircuit (Douglas & Martin 1991)");

    // Feedforward connections
    laminar_connect_feedforward(ls, CC_LAYER_IV, CC_LAYER_II_III, 1.0F);
    laminar_connect_feedforward(ls, CC_LAYER_II_III, CC_LAYER_V, 0.8F);
    laminar_connect_feedforward(ls, CC_LAYER_V, CC_LAYER_VI, 0.7F);

    // Feedback connections
    laminar_connect_feedback(ls, CC_LAYER_VI, CC_LAYER_IV, 0.5F);
    laminar_connect_feedback(ls, CC_LAYER_I, CC_LAYER_II_III, 0.4F);

    // Recurrent connections within Layer II/III
    laminar_connect_feedforward(ls, CC_LAYER_II_III, CC_LAYER_II_III, 0.6F);

    LOG_INFO("Canonical circuit applied: %u connections", ls->connection_count);
}

//=============================================================================
// Public API: Statistics
//=============================================================================

void laminar_get_profile(laminar_structure_t* ls,
                        laminar_profile_t* profile) {
    // Guard clauses
    if (!ls || !profile) {
        LOG_ERROR("NULL parameter to laminar_get_profile");
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        update_layer_stats(&ls->layers[i]);
        profile->layer_activations[i] = ls->layers[i].mean_activation;

        // Compute mean input
        float input_sum = 0.0F;
        for (uint32_t j = 0; j < ls->layers[i].neuron_count; j++) {
            input_sum += ls->layers[i].inputs[j];
        }
        profile->layer_inputs[i] = (ls->layers[i].neuron_count > 0)
            ? input_sum / (float)ls->layers[i].neuron_count : 0.0F;

        // Compute mean output
        float output_sum = 0.0F;
        for (uint32_t j = 0; j < ls->layers[i].neuron_count; j++) {
            output_sum += ls->layers[i].outputs[j];
        }
        profile->layer_outputs[i] = (ls->layers[i].neuron_count > 0)
            ? output_sum / (float)ls->layers[i].neuron_count : 0.0F;
    }

    profile->timestamp = (uint64_t)time(NULL);

    nimcp_platform_mutex_unlock(&ls->mutex);
}

void laminar_get_stats(laminar_structure_t* ls,
                      laminar_stats_t* stats) {
    // Guard clauses
    if (!ls || !stats) {
        LOG_ERROR("NULL parameter to laminar_get_stats");
        return;
    }

    nimcp_platform_mutex_lock(&ls->mutex);

    // Update and copy layer statistics
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        update_layer_stats(&ls->layers[i]);
        stats->mean_activation[i] = ls->layers[i].mean_activation;
        stats->variance_activation[i] = ls->layers[i].variance_activation;
    }

    // Compute information flow metrics
    stats->total_feedforward_flow =
        ls->layers[CC_LAYER_IV].mean_activation +
        ls->layers[CC_LAYER_II_III].mean_activation +
        ls->layers[CC_LAYER_V].mean_activation;

    stats->total_feedback_flow =
        ls->layers[CC_LAYER_VI].mean_activation +
        ls->layers[CC_LAYER_I].mean_activation;

    // Prediction error from Layer VI
    stats->prediction_error = fabsf(ls->layers[CC_LAYER_VI].mean_activation);

    stats->update_count = ls->update_count;

    nimcp_platform_mutex_unlock(&ls->mutex);
}
