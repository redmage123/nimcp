#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neuralnet_activation.c - Activation Functions for Neural Networks
//=============================================================================

#include "core/neuralnet/nimcp_neuralnet_activation.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>

#define LOG_MODULE "neuralnet_activation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuralnet_activation)

// Constants from original file
#define EPSILON 1e-10f
#define MAX_ACTIVATION 1.0f
#define MIN_ACTIVATION -1.0f

/* Single authoritative definition of activation_strategy_table_t and neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

//=============================================================================
// Forward Declarations
//=============================================================================

static float sigmoid(float x);
static float fast_tanh(float x);
static float activate_sigmoid(float input, float threshold);
static float activate_tanh(float input, float threshold);
static float activate_relu(float input, float threshold);
static float activate_leaky_relu(float input, float threshold);
static float activate_adaptive(float input, float threshold);
static void init_activation_strategies(activation_strategy_table_t* table);
static float clamp_activation(float value);

//=============================================================================
// Helper Functions
//=============================================================================

static float sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

static float fast_tanh(float x)
{
    if (fabsf(x) > 4.0f) return tanhf(x);
    float x2 = x * x;
    float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / (b + 1e-10f);
}

static float clamp_activation(float value)
{
    return fmaxf(MIN_ACTIVATION, fminf(MAX_ACTIVATION, value));
}

//=============================================================================
// Activation Function Strategies
//=============================================================================

static float activate_sigmoid(float input, float threshold)
{
    (void)threshold;
    return sigmoid(input);
}

static float activate_tanh(float input, float threshold)
{
    (void)threshold;
    return fast_tanh(input);
}

static float activate_relu(float input, float threshold)
{
    (void)threshold;
    return (input > 0.0f) ? input : 0.0f;
}

static float activate_leaky_relu(float input, float threshold)
{
    (void)threshold;
    return (input > 0.0f) ? input : 0.01f * input;
}

static float activate_adaptive(float input, float threshold)
{
    (void)threshold;
    return input;
}

static void init_activation_strategies(activation_strategy_table_t* table)
{
    if (!table)
        return;

    table->functions[ACTIVATION_SIGMOID] = activate_sigmoid;
    table->functions[ACTIVATION_TANH] = activate_tanh;
    table->functions[ACTIVATION_RELU] = activate_relu;
    table->functions[ACTIVATION_LEAKY_RELU] = activate_leaky_relu;
    table->functions[ACTIVATION_ADAPTIVE] = activate_adaptive;
}

//=============================================================================
// Public API Implementation
//=============================================================================

float neural_network_compute_activation(neuron_t* neuron, float input)
{
    if (!neuron) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_network_compute_activation: neuron is NULL");
        return 0.0f;
    }

    // Strategy table initialized on stack each call (small, O(1))
    // Avoids TOCTOU race on static initialization with _Atomic bool
    activation_strategy_table_t table;
    init_activation_strategies(&table);

    // Validate activation type
    if (neuron->activation_type < 0 || neuron->activation_type >= 8) {
        LOG_WARN(LOG_MODULE, "Invalid activation type %d for neuron %u, using pass-through",
                 neuron->activation_type, neuron->id);
        return clamp_activation(input);
    }

    // Strategy dispatch
    activation_fn_t activation_fn = table.functions[neuron->activation_type];
    if (!activation_fn) {
        LOG_ERROR(LOG_MODULE, "NULL activation function for type %d", neuron->activation_type);
        return clamp_activation(input);
    }

    // Execute activation function
    float result = activation_fn(input, neuron->threshold);

    // Update adaptive threshold if using adaptive activation
    if (neuron->activation_type == ACTIVATION_ADAPTIVE) {
        if (input > neuron->threshold) {
            neuron->threshold += neuron->adaptation;
        } else {
            neuron->threshold = fmaxf(neuron->threshold - neuron->adaptation * 0.1f,
                                      neuron->rest_potential + 10.0f);
        }
    }

    return clamp_activation(result);
}

float neural_network_clamp_activation(float value)
{
    return clamp_activation(value);
}
