/**
 * @file nimcp_lnn_layer.c
 * @brief LNN Layer Implementation - Vectorized Continuous-Time Dynamics
 * @version 1.0.0
 * @date 2025-12-20
 *
 * @author NIMCP Development Team
 */

#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_neuron.h"
#include "lnn/nimcp_lnn_wiring.h"
#include "lnn/nimcp_lnn_ode.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_layer)

/*=============================================================================
 * Thread-Local RNG State
 *===========================================================================*/

/* Thread-local RNG state */
static __thread unsigned int tls_seed = 0;
static __thread bool tls_seed_initialized = false;

/**
 * @brief Initialize thread-local RNG seed
 *
 * WHAT: One-time initialization of thread-local random seed
 * WHY:  rand_r() requires per-thread seed for thread safety
 * HOW:  Combine timestamp and thread ID for uniqueness
 */
static void ensure_rng_initialized(void) {
    if (!tls_seed_initialized) {
        tls_seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
        tls_seed_initialized = true;
    }
}

/**
 * @brief Thread-safe random integer generator
 *
 * WHAT: Reentrant random number generator
 * WHY:  rand() is not thread-safe, rand_r() is
 * HOW:  Uses thread-local seed with rand_r()
 */
static int thread_safe_rand(void) {
    ensure_rng_initialized();
    return rand_r(&tls_seed);
}

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Apply activation function element-wise
 *
 * WHAT: Apply f(x) to all elements of tensor
 * WHY:  Nonlinearity in LTC dynamics
 * HOW:  Switch on activation type, apply function
 */
static int apply_activation(
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    lnn_activation_t activation
)
{
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null input or output tensor in apply_activation");
        return LNN_ERROR_NULL_POINTER;
    }

    switch (activation) {
        case LNN_ACTIVATION_TANH:
            {
                nimcp_tensor_t* result = nimcp_tensor_tanh(input);
                if (!result) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_FORWARD_PASS,
                                         "Tanh activation failed in apply_activation");
                    return LNN_ERROR_OPERATION_FAILED;
                }
                memcpy(nimcp_tensor_data(output), nimcp_tensor_data_const(result),
                       nimcp_tensor_numel(result) * sizeof(float));
                nimcp_tensor_destroy(result);
            }
            break;

        case LNN_ACTIVATION_SIGMOID:
            {
                nimcp_tensor_t* result = nimcp_tensor_sigmoid(input);
                if (!result) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_FORWARD_PASS,
                                         "Sigmoid activation failed in apply_activation");
                    return LNN_ERROR_OPERATION_FAILED;
                }
                memcpy(nimcp_tensor_data(output), nimcp_tensor_data_const(result),
                       nimcp_tensor_numel(result) * sizeof(float));
                nimcp_tensor_destroy(result);
            }
            break;

        case LNN_ACTIVATION_RELU:
            {
                nimcp_tensor_t* result = nimcp_tensor_relu(input);
                if (!result) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_FORWARD_PASS,
                                         "ReLU activation failed in apply_activation");
                    return LNN_ERROR_OPERATION_FAILED;
                }
                memcpy(nimcp_tensor_data(output), nimcp_tensor_data_const(result),
                       nimcp_tensor_numel(result) * sizeof(float));
                nimcp_tensor_destroy(result);
            }
            break;

        default:
            NIMCP_LOGGING_ERROR("Unsupported activation type: %d", activation);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER,
                                 "Unsupported activation type %d in apply_activation",
                                 activation);
            return LNN_ERROR_INVALID_PARAM;
    }

    return LNN_SUCCESS;
}

/**
 * @brief Generate random normal value
 *
 * WHAT: Thread-safe Box-Muller transform for Gaussian samples
 * WHY:  Weight initialization requires random normal values
 * HOW:  Uses thread-local storage for cached spare value
 *
 * THREAD SAFETY: Thread-safe via thread-local storage
 */
static float randn(float mean, float std)
{
    /* Box-Muller transform with thread-local storage */
    static __thread bool has_spare = false;
    static __thread float spare;

    if (has_spare) {
        has_spare = false;
        return mean + std * spare;
    }

    has_spare = true;
    float u, v, s;
    do {
        u = (thread_safe_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        v = (thread_safe_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    return mean + std * u * s;
}

/*=============================================================================
 * Configuration
 *===========================================================================*/

void lnn_layer_config_default(lnn_layer_config_t* config)
{
    if (!config) return;

    /* Architecture */
    config->n_neurons = 64;

    /* Activation */
    config->activation = LNN_ACTIVATION_TANH;

    /* Time constants */
    config->tau_base_init = 10.0f;    /* 10 ms baseline */
    config->tau_min = 1.0f;           /* 1 ms minimum */
    config->tau_max = 100.0f;         /* 100 ms maximum */
    config->learn_tau = true;

    /* Initialization */
    config->weight_init_std = 0.1f;   /* Xavier-style */
    config->seed = 0;                 /* Use time */

    /* Wiring */
    config->wiring_type = LNN_WIRING_FULL;
    config->sparsity = 0.0f;

    /* ODE solver */
    config->ode_method = LNN_ODE_RK4;
    config->dt = 1.0f;                /* 1 ms default step */

    /* Normalization */
    config->use_layer_norm = false;
    config->layer_norm_eps = 1e-5f;
}

/*=============================================================================
 * Layer Lifecycle
 *===========================================================================*/

lnn_layer_t* lnn_layer_create(const lnn_layer_config_t* config, uint32_t n_inputs)
{
    /* Validate inputs */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null configuration in lnn_layer_create");
        return NULL;
    }
    if (config->n_neurons == 0 || n_inputs == 0) {
        NIMCP_LOGGING_ERROR("Invalid dimensions: n_neurons=%u, n_inputs=%u",
                           config->n_neurons, n_inputs);
        NIMCP_THROW_BRAIN(NIMCP_ERROR_DIMENSION_MISMATCH, 0, "lnn_layer",
                         "Invalid dimensions: n_neurons=%u, n_inputs=%u",
                         config->n_neurons, n_inputs);
        return NULL;
    }

    /* Allocate layer structure */
    lnn_layer_t* layer = (lnn_layer_t*)nimcp_calloc(1, sizeof(lnn_layer_t));
    if (!layer) {
        NIMCP_LOGGING_ERROR("Failed to allocate layer structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_layer_create: layer is NULL");
        return NULL;
    }

    /* Initialize basic fields */
    layer->id = 0;  /* Set by network */
    snprintf(layer->name, sizeof(layer->name), "lnn_layer");
    layer->n_neurons = config->n_neurons;
    layer->ode_method = config->ode_method;
    layer->dt = config->dt;
    layer->step_count = 0;
    layer->use_layer_norm = config->use_layer_norm;
    layer->layer_norm_eps = config->layer_norm_eps;

    /* Allocate neuron array (for debugging/individual access) */
    layer->neurons = (lnn_neuron_t*)nimcp_calloc(config->n_neurons, sizeof(lnn_neuron_t));
    if (!layer->neurons) {
        NIMCP_LOGGING_ERROR("Failed to allocate neurons");
        lnn_layer_destroy(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_layer_create: layer->neurons is NULL");
        return NULL;
    }

    /* Initialize individual neurons with DIVERSE time constants.
     * Log-uniform in [0.5, 50.0] ms so different neurons capture different
     * timescales: fast neurons (0.5ms) for transient features, slow neurons
     * (50ms) for persistent context. Without diversity, all taus converge to
     * the same value (~5ms) and the LNN has no multi-timescale capability. */
    for (uint32_t i = 0; i < config->n_neurons; i++) {
        layer->neurons[i].id = i;
        layer->neurons[i].activation = config->activation;
        /* Log-uniform: tau = exp(log(0.5) + i/N * (log(50) - log(0.5))) */
        float t = (float)i / (float)(config->n_neurons > 1 ? config->n_neurons - 1 : 1);
        float log_min = logf(0.5f);
        float log_max = logf(50.0f);
        layer->neurons[i].tau_base = expf(log_min + t * (log_max - log_min));
        layer->neurons[i].n_inputs = n_inputs;
        layer->neurons[i].n_recurrent = config->n_neurons;
    }

    /* Create state tensors
     * MEMORY SAFETY: Allocate all tensors first, then check all at once.
     * lnn_layer_destroy() is NULL-safe for all tensor pointers.
     */
    uint32_t dims_state[1] = {config->n_neurons};
    layer->x = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    /* Initialize tau tensor with diverse values matching per-neuron tau_base */
    layer->tau = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    layer->dx_dt = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    layer->tau_base = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    if (layer->tau && layer->tau_base) {
        float* tau_data = (float*)nimcp_tensor_data(layer->tau);
        float* tau_base_data = (float*)nimcp_tensor_data(layer->tau_base);
        for (uint32_t i = 0; i < config->n_neurons; i++) {
            tau_data[i] = layer->neurons[i].tau_base;
            tau_base_data[i] = layer->neurons[i].tau_base;
        }
    }
    layer->b_in = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    layer->b_tau = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);

    if (!layer->x || !layer->tau || !layer->dx_dt || !layer->tau_base ||
        !layer->b_in || !layer->b_tau) {
        NIMCP_LOGGING_ERROR("Failed to allocate state tensors");
        /* lnn_layer_destroy() handles partial allocation - NULL-safe */
        lnn_layer_destroy(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_layer_create: operation failed");
        return NULL;
    }

    /* Create weight tensors
     * MEMORY SAFETY: lnn_layer_destroy() is NULL-safe for all pointers.
     */
    uint32_t dims_W_in[2] = {config->n_neurons, n_inputs};
    uint32_t dims_W_rec[2] = {config->n_neurons, config->n_neurons};
    uint32_t dims_W_tau[2] = {config->n_neurons, n_inputs + config->n_neurons};

    layer->W_in = nimcp_tensor_zeros(dims_W_in, 2, NIMCP_DTYPE_F32);
    layer->W_rec = nimcp_tensor_zeros(dims_W_rec, 2, NIMCP_DTYPE_F32);
    layer->W_tau = nimcp_tensor_zeros(dims_W_tau, 2, NIMCP_DTYPE_F32);

    if (!layer->W_in || !layer->W_rec || !layer->W_tau) {
        NIMCP_LOGGING_ERROR("Failed to allocate weight tensors");
        /* lnn_layer_destroy() handles partial allocation - NULL-safe */
        lnn_layer_destroy(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_layer_create: required parameter is NULL (layer->W_in, layer->W_rec, layer->W_tau)");
        return NULL;
    }

    /* Create gradient tensors
     * MEMORY SAFETY: lnn_layer_destroy() is NULL-safe for all pointers.
     */
    layer->grad_W_in = nimcp_tensor_zeros(dims_W_in, 2, NIMCP_DTYPE_F32);
    layer->grad_W_rec = nimcp_tensor_zeros(dims_W_rec, 2, NIMCP_DTYPE_F32);
    layer->grad_W_tau = nimcp_tensor_zeros(dims_W_tau, 2, NIMCP_DTYPE_F32);
    layer->grad_b_in = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    layer->grad_b_tau = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);
    layer->grad_tau_base = nimcp_tensor_zeros(dims_state, 1, NIMCP_DTYPE_F32);

    if (!layer->grad_W_in || !layer->grad_W_rec || !layer->grad_W_tau ||
        !layer->grad_b_in || !layer->grad_b_tau || !layer->grad_tau_base) {
        NIMCP_LOGGING_ERROR("Failed to allocate gradient tensors");
        /* lnn_layer_destroy() handles partial allocation - NULL-safe */
        lnn_layer_destroy(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_layer_create: operation failed");
        return NULL;
    }

    /* Create wiring pattern */
    layer->wiring = lnn_wiring_create(config->wiring_type, config->n_neurons, config->sparsity);
    if (!layer->wiring) {
        NIMCP_LOGGING_WARN("Failed to create wiring, using FULL connectivity");
        /* Continue with NULL wiring (will use full connectivity) */
    }

    /* Initialize weights */
    if (lnn_layer_init_weights(layer, config->weight_init_std, config->seed) != LNN_SUCCESS) {
        NIMCP_LOGGING_WARN("Weight initialization failed, using zeros");
    }

    NIMCP_LOGGING_INFO("Created LNN layer: %u neurons, %u inputs",
                      config->n_neurons, n_inputs);

    return layer;
}

void lnn_layer_destroy(lnn_layer_t* layer)
{
    if (!layer) return;

    /* Free GPU layer if still attached (normally freed by network destroy) */
    if (layer->gpu_lnn_layer) {
        nimcp_lnn_layer_gpu_destroy((nimcp_lnn_layer_gpu_t*)layer->gpu_lnn_layer);
        layer->gpu_lnn_layer = NULL;
    }

    /* Free neurons */
    if (layer->neurons) {
        for (uint32_t i = 0; i < layer->n_neurons; i++) {
            if (layer->neurons[i].w_in) nimcp_free(layer->neurons[i].w_in);
            if (layer->neurons[i].w_rec) nimcp_free(layer->neurons[i].w_rec);
            if (layer->neurons[i].w_tau) nimcp_free(layer->neurons[i].w_tau);
            if (layer->neurons[i].grad_w_in) nimcp_free(layer->neurons[i].grad_w_in);
            if (layer->neurons[i].grad_w_rec) nimcp_free(layer->neurons[i].grad_w_rec);
            if (layer->neurons[i].grad_w_tau) nimcp_free(layer->neurons[i].grad_w_tau);
        }
        nimcp_free(layer->neurons);
    }

    /* Free state tensors */
    nimcp_tensor_destroy(layer->x);
    nimcp_tensor_destroy(layer->tau);
    nimcp_tensor_destroy(layer->dx_dt);
    nimcp_tensor_destroy(layer->tau_base);
    nimcp_tensor_destroy(layer->b_in);
    nimcp_tensor_destroy(layer->b_tau);

    /* Free weight tensors */
    nimcp_tensor_destroy(layer->W_in);
    nimcp_tensor_destroy(layer->W_rec);
    nimcp_tensor_destroy(layer->W_tau);

    /* Free gradient tensors */
    nimcp_tensor_destroy(layer->grad_W_in);
    nimcp_tensor_destroy(layer->grad_W_rec);
    nimcp_tensor_destroy(layer->grad_W_tau);
    nimcp_tensor_destroy(layer->grad_b_in);
    nimcp_tensor_destroy(layer->grad_b_tau);
    nimcp_tensor_destroy(layer->grad_tau_base);

    /* Free wiring */
    if (layer->wiring) {
        lnn_wiring_destroy(layer->wiring);
    }

    nimcp_free(layer);
}

int lnn_layer_init_weights(lnn_layer_t* layer, float std, uint64_t seed)
{
    if (!layer) return LNN_ERROR_NULL_POINTER;

    /* Initialize thread-local random seed */
    if (seed == 0) {
        ensure_rng_initialized();
    } else {
        tls_seed = (unsigned int)seed;
        tls_seed_initialized = true;
    }

    /* Initialize W_in with random normal */
    float* W_in_data = (float*)nimcp_tensor_data(layer->W_in);
    size_t W_in_size = nimcp_tensor_numel(layer->W_in);
    for (size_t i = 0; i < W_in_size; i++) {
        W_in_data[i] = randn(0.0f, std);
    }

    /* Initialize W_rec with random normal (apply sparsity if wiring exists) */
    float* W_rec_data = (float*)nimcp_tensor_data(layer->W_rec);
    size_t W_rec_size = nimcp_tensor_numel(layer->W_rec);
    for (size_t i = 0; i < W_rec_size; i++) {
        W_rec_data[i] = randn(0.0f, std);
    }

    /* Apply sparsity mask from wiring */
    if (layer->wiring && layer->wiring->type != LNN_WIRING_FULL) {
        /* Zero out non-connected weights based on wiring pattern */
        for (uint32_t i = 0; i < layer->n_neurons; i++) {
            for (uint32_t j = 0; j < layer->n_neurons; j++) {
                if (!lnn_wiring_is_connected(layer->wiring, i, j)) {
                    W_rec_data[i * layer->n_neurons + j] = 0.0f;
                }
            }
        }
    }

    /* Spectral normalization of W_rec at initialization.
     * Ensures σ_max(W_rec) ≤ 1.0 from the start, preventing adjoint ODE
     * instability on the very first backward pass.
     * Power iteration: 10 iterations, O(n²) each. */
    {
        uint32_t n = layer->n_neurons;
        float* u = (float*)nimcp_calloc(n, sizeof(float));
        float* v = (float*)nimcp_calloc(n, sizeof(float));
        if (u && v && n > 0) {
            float init_val = 1.0f / sqrtf((float)n);
            for (uint32_t i = 0; i < n; i++) u[i] = init_val;

            float sigma = 0.0f;
            for (int iter = 0; iter < 10; iter++) {
                float v_norm = 0.0f;
                for (uint32_t j = 0; j < n; j++) {
                    float sum = 0.0f;
                    for (uint32_t ii = 0; ii < n; ii++) {
                        sum += W_rec_data[ii * n + j] * u[ii];
                    }
                    v[j] = sum;
                    v_norm += sum * sum;
                }
                v_norm = sqrtf(v_norm);
                if (v_norm < 1e-12f) break;
                for (uint32_t j = 0; j < n; j++) v[j] /= v_norm;

                float u_norm = 0.0f;
                for (uint32_t ii = 0; ii < n; ii++) {
                    float sum = 0.0f;
                    for (uint32_t j = 0; j < n; j++) {
                        sum += W_rec_data[ii * n + j] * v[j];
                    }
                    u[ii] = sum;
                    u_norm += sum * sum;
                }
                u_norm = sqrtf(u_norm);
                if (u_norm < 1e-12f) break;
                sigma = u_norm;
                for (uint32_t ii = 0; ii < n; ii++) u[ii] /= u_norm;
            }

            if (sigma > 1.0f) {
                float scale = 1.0f / sigma;
                for (size_t i = 0; i < W_rec_size; i++) {
                    W_rec_data[i] *= scale;
                }
                NIMCP_LOGGING_DEBUG("W_rec init spectral norm: %.2f → 1.0 (rescaled)", sigma);
            }
        }
        if (u) nimcp_free(u);
        if (v) nimcp_free(v);
    }

    /* Initialize W_tau with small random values */
    float* W_tau_data = (float*)nimcp_tensor_data(layer->W_tau);
    size_t W_tau_size = nimcp_tensor_numel(layer->W_tau);
    for (size_t i = 0; i < W_tau_size; i++) {
        W_tau_data[i] = randn(0.0f, std * 0.1f);  /* Smaller for tau modulation */
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Forward Computation
 *===========================================================================*/

int lnn_layer_compute_tau(lnn_layer_t* layer, const nimcp_tensor_t* input)
{
    if (!layer || !input) return LNN_ERROR_NULL_POINTER;

    /* Concatenate [input; x] for tau computation
     * W_tau is [n_neurons, n_inputs + n_neurons] so we need input vector
     * of size (n_inputs + n_neurons).
     */
    nimcp_tensor_t* tensors[2] = {(nimcp_tensor_t*)input, layer->x};
    nimcp_tensor_t* tau_input = nimcp_tensor_cat(tensors, 2, 0);
    if (!tau_input) return LNN_ERROR_OPERATION_FAILED;

    /* Compute tau modulation: tau_logits = W_tau @ [input; x] + b_tau */
    nimcp_tensor_t* tau_logits = nimcp_tensor_mv(layer->W_tau, tau_input);
    nimcp_tensor_destroy(tau_input);
    if (!tau_logits) return LNN_ERROR_OPERATION_FAILED;

    nimcp_tensor_t* tau_logits_biased = nimcp_tensor_add(tau_logits, layer->b_tau);
    nimcp_tensor_destroy(tau_logits);
    if (!tau_logits_biased) return LNN_ERROR_OPERATION_FAILED;

    /* Apply sigmoid to bound tau */
    nimcp_tensor_t* tau_sigmoid = nimcp_tensor_sigmoid(tau_logits_biased);
    nimcp_tensor_destroy(tau_logits_biased);
    if (!tau_sigmoid) return LNN_ERROR_OPERATION_FAILED;

    /* Scale by tau_base: tau = tau_base * sigmoid(...) */
    nimcp_tensor_t* tau_new = nimcp_tensor_mul(layer->tau_base, tau_sigmoid);
    nimcp_tensor_destroy(tau_sigmoid);
    if (!tau_new) return LNN_ERROR_OPERATION_FAILED;

    /* Update layer tau with floor enforcement.
     * tau = tau_base * sigmoid(...) can produce near-zero values when tau_base
     * is small. The decay term -x/tau then explodes → NaN.
     * Floor at 0.01 matches the backward pass tau_safe floor. */
    {
        float* tau_data = (float*)nimcp_tensor_data(tau_new);
        uint32_t tn = layer->n_neurons;
        for (uint32_t ti = 0; ti < tn; ti++) {
            if (!isfinite(tau_data[ti]) || tau_data[ti] < 0.01f) tau_data[ti] = 0.01f;
        }
    }
    nimcp_tensor_destroy(layer->tau);
    layer->tau = tau_new;

    return LNN_SUCCESS;
}

int lnn_layer_compute_derivatives(lnn_layer_t* layer, const nimcp_tensor_t* input)
{
    if (!layer || !input) return LNN_ERROR_NULL_POINTER;

    /* Compute h_in = W_in @ input + b_in */
    nimcp_tensor_t* h_in = nimcp_tensor_mv(layer->W_in, input);
    if (!h_in) return LNN_ERROR_OPERATION_FAILED;

    nimcp_tensor_t* h_in_biased = nimcp_tensor_add(h_in, layer->b_in);
    nimcp_tensor_destroy(h_in);
    if (!h_in_biased) return LNN_ERROR_OPERATION_FAILED;

    /* Compute h_rec = W_rec @ x */
    nimcp_tensor_t* h_rec = nimcp_tensor_mv(layer->W_rec, layer->x);
    if (!h_rec) {
        nimcp_tensor_destroy(h_in_biased);
        return LNN_ERROR_OPERATION_FAILED;
    }

    /* Compute h_total = h_in + h_rec */
    nimcp_tensor_t* h_total = nimcp_tensor_add(h_in_biased, h_rec);
    nimcp_tensor_destroy(h_in_biased);
    nimcp_tensor_destroy(h_rec);
    if (!h_total) return LNN_ERROR_OPERATION_FAILED;

    /* Apply activation: f(h_total) */
    nimcp_tensor_t* activated = nimcp_tensor_create((uint32_t[]){layer->n_neurons}, 1, NIMCP_DTYPE_F32);
    if (!activated) {
        nimcp_tensor_destroy(h_total);
        return LNN_ERROR_OPERATION_FAILED;
    }

    if (apply_activation(h_total, activated, layer->neurons[0].activation) != LNN_SUCCESS) {
        nimcp_tensor_destroy(h_total);
        nimcp_tensor_destroy(activated);
        return LNN_ERROR_OPERATION_FAILED;
    }
    nimcp_tensor_destroy(h_total);

    /* Compute decay term: -x / tau */
    nimcp_tensor_t* decay = nimcp_tensor_div(layer->x, layer->tau);
    if (!decay) {
        nimcp_tensor_destroy(activated);
        return LNN_ERROR_OPERATION_FAILED;
    }
    nimcp_tensor_t* neg_decay = nimcp_tensor_neg(decay);
    nimcp_tensor_destroy(decay);
    if (!neg_decay) {
        nimcp_tensor_destroy(activated);
        return LNN_ERROR_OPERATION_FAILED;
    }

    /* Compute dx/dt = -x/tau + f(h) */
    nimcp_tensor_t* dx_dt_new = nimcp_tensor_add(neg_decay, activated);
    nimcp_tensor_destroy(neg_decay);
    nimcp_tensor_destroy(activated);
    if (!dx_dt_new) return LNN_ERROR_OPERATION_FAILED;

    /* Update layer dx_dt */
    nimcp_tensor_destroy(layer->dx_dt);
    layer->dx_dt = dx_dt_new;

    return LNN_SUCCESS;
}

int lnn_layer_step(lnn_layer_t* layer, float dt, lnn_ode_method_t method)
{
    if (!layer) return LNN_ERROR_NULL_POINTER;

    /* Use provided dt or layer default */
    float step_dt = (dt > 0.0f) ? dt : layer->dt;

    /* Use provided method or layer default */
    lnn_ode_method_t solver = (method != LNN_ODE_METHOD_COUNT) ? method : layer->ode_method;

    /* Perform ODE step using specified method */
    nimcp_tensor_t* x_new = lnn_ode_step_tensor(layer->x, layer->dx_dt, step_dt, solver);
    if (!x_new) return LNN_ERROR_ODE_DIVERGENCE;

    /* Update state */
    nimcp_tensor_destroy(layer->x);
    layer->x = x_new;
    layer->step_count++;

    /* Stability guard: clamp state to [-10, 10] and replace NaN/Inf with 0.
     * The ODE integrator can diverge when W_rec eigenvalues exceed 1/tau,
     * producing NaN/Inf that propagates to every downstream consumer
     * (feature vectors, loss computation, gradient flow). */
    {
        float* x_data = (float*)nimcp_tensor_data(layer->x);
        uint32_t n = layer->n_neurons;
        for (uint32_t i = 0; i < n; i++) {
            if (!isfinite(x_data[i])) {
                x_data[i] = 0.0f;
            } else if (x_data[i] > 10.0f) {
                x_data[i] = 10.0f;
            } else if (x_data[i] < -10.0f) {
                x_data[i] = -10.0f;
            }
        }
    }

    return LNN_SUCCESS;
}

int lnn_layer_forward(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
)
{
    if (!layer || !input || !output) return LNN_ERROR_NULL_POINTER;

    /* Hamiltonian dynamics dispatch — energy-conserving alternative to LTC.
     * Falls through to GPU/CPU LTC path if Hamiltonian fails (e.g. p not allocated). */
    if (layer->use_hamiltonian && layer->H_net) {
        extern int lnn_layer_forward_hamiltonian(lnn_layer_t*, const nimcp_tensor_t*,
                                                  nimcp_tensor_t*, float);
        int hnn_rc = lnn_layer_forward_hamiltonian(layer, input, output, dt);
        if (hnn_rc == 0) return LNN_SUCCESS;
        /* Fall through to LTC on failure */
    }

    /* GPU LNN dispatch — upload input to GPU, run ODE step, sync back */
    if (layer->gpu_lnn_layer && layer->gpu_ctx) {
        nimcp_gpu_context_t* gpu_ctx = (nimcp_gpu_context_t*)layer->gpu_ctx;
        nimcp_lnn_layer_gpu_t* gpu_layer = (nimcp_lnn_layer_gpu_t*)layer->gpu_lnn_layer;

        nimcp_gpu_tensor_t* gpu_input = nimcp_gpu_tensor_from_cpu(gpu_ctx, input);
        if (gpu_input) {
            nimcp_lnn_ode_config_t ode_cfg = nimcp_lnn_ode_default_config();
            ode_cfg.method = layer->ode_method;
            ode_cfg.dt = (dt > 0.0f) ? dt : layer->dt;

            bool ok = nimcp_gpu_lnn_ode_step(gpu_ctx, gpu_layer, gpu_input, &ode_cfg);
            nimcp_gpu_tensor_destroy(gpu_input);

            if (ok) {
                /* Sync GPU state back to CPU layer->x for downstream consumption */
                if (nimcp_lnn_layer_gpu_to_cpu(gpu_layer, layer)) {
                    memcpy(nimcp_tensor_data(output),
                           nimcp_tensor_data_const(layer->x),
                           layer->n_neurons * sizeof(float));
                    return LNN_SUCCESS;
                }
            }
        }
        /* GPU path failed — fall through to CPU path */
        NIMCP_LOGGING_WARN("GPU LNN forward failed for layer %u, falling back to CPU",
                           layer->id);
    }

    /* 1. Compute time constants (produces layer->tau = tau_base * sigmoid(...)) */
    int ret = lnn_layer_compute_tau(layer, input);
    if (ret != LNN_SUCCESS) return ret;

    /* 1b. Substrate adapter (Phase 2 biological-substrate wiring).
     * When the owning network has attached a substrate AND the relevant
     * tunables are on, the network writes layer->substrate_dend_effects
     * with a borrowed pointer to its cached dendrite effect struct. We
     * multiply each element of layer->tau by the membrane_time_constant_mod
     * scalar (same floor as the compute-tau stability clamp).
     *
     * CRITICAL: We modulate the EPHEMERAL layer->tau tensor, not the
     * learned layer->tau_base or W_tau parameters. Gradients flowing
     * through backward still reach tau_base via the adjoint through the
     * full learned expression. Substrate composes, never replaces. */
    if (layer->substrate_dend_effects && layer->tau) {
        const float mod = layer->substrate_dend_effects->membrane_time_constant_mod;
        /* At baseline (healthy membrane) mod=1.0 → no-op, exactly matching
         * the no-substrate path. Mod only departs from 1.0 when membrane
         * integrity < 1.0. */
        if (mod != 1.0f) {
            float* tau_data = (float*)nimcp_tensor_data(layer->tau);
            uint32_t n = layer->n_neurons;
            for (uint32_t i = 0; i < n; i++) {
                float v = tau_data[i] * mod;
                /* Re-enforce the same 0.01 floor compute_tau uses to
                 * prevent -x/tau blowup when mod pushes tau sub-floor. */
                if (!isfinite(v) || v < 0.01f) v = 0.01f;
                tau_data[i] = v;
            }
        }
    }

    /* 2. Compute derivatives (consumes layer->tau in -x/tau decay) */
    ret = lnn_layer_compute_derivatives(layer, input);
    if (ret != LNN_SUCCESS) return ret;

    /* 3. Take ODE step */
    ret = lnn_layer_step(layer, dt, LNN_ODE_METHOD_COUNT);  /* Use layer default */
    if (ret != LNN_SUCCESS) return ret;

    /* 3b. Apply layer normalization to prevent activation collapse */
    if (layer->use_layer_norm && layer->n_neurons > 1) {
        float* x_data = (float*)nimcp_tensor_data(layer->x);
        uint32_t n = layer->n_neurons;

        /* Compute mean */
        float mean = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            mean += x_data[i];
        }
        mean /= (float)n;

        /* Compute variance */
        float var = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float diff = x_data[i] - mean;
            var += diff * diff;
        }
        var /= (float)n;

        /* Normalize: x = (x - mean) / sqrt(var + eps) */
        float inv_std = 1.0f / sqrtf(var + layer->layer_norm_eps);
        for (uint32_t i = 0; i < n; i++) {
            x_data[i] = (x_data[i] - mean) * inv_std;
        }
    }

    /* 4. Copy state to output */
    memcpy(nimcp_tensor_data(output), nimcp_tensor_data_const(layer->x),
           layer->n_neurons * sizeof(float));

    return LNN_SUCCESS;
}

/*=============================================================================
 * State Management
 *===========================================================================*/

int lnn_layer_get_state(const lnn_layer_t* layer, nimcp_tensor_t** state)
{
    if (!layer || !state) return LNN_ERROR_NULL_POINTER;
    if (!layer->x) return LNN_ERROR_NULL_POINTER;

    *state = nimcp_tensor_clone(layer->x);
    if (!*state) return LNN_ERROR_OUT_OF_MEMORY;

    return LNN_SUCCESS;
}

int lnn_layer_set_state(lnn_layer_t* layer, const nimcp_tensor_t* state)
{
    if (!layer || !state) return LNN_ERROR_NULL_POINTER;

    /* Validate shape */
    if (nimcp_tensor_numel(state) != layer->n_neurons) {
        NIMCP_LOGGING_ERROR("State size mismatch: got %zu, expected %u",
                           nimcp_tensor_numel(state), layer->n_neurons);
        return LNN_ERROR_INVALID_DIMENSION;
    }

    /* Copy state */
    memcpy(nimcp_tensor_data(layer->x), nimcp_tensor_data_const(state),
           layer->n_neurons * sizeof(float));

    return LNN_SUCCESS;
}

int lnn_layer_get_tau(const lnn_layer_t* layer, nimcp_tensor_t** tau)
{
    if (!layer || !tau) return LNN_ERROR_NULL_POINTER;

    *tau = nimcp_tensor_clone(layer->tau);
    if (!*tau) return LNN_ERROR_OUT_OF_MEMORY;

    return LNN_SUCCESS;
}

void lnn_layer_reset(lnn_layer_t* layer)
{
    if (!layer) return;

    /* Reset state to zero */
    float* x_data = (float*)nimcp_tensor_data(layer->x);
    float* dx_dt_data = (float*)nimcp_tensor_data(layer->dx_dt);

    memset(x_data, 0, layer->n_neurons * sizeof(float));
    memset(dx_dt_data, 0, layer->n_neurons * sizeof(float));

    layer->step_count = 0;
}

/*=============================================================================
 * Gradient Computation
 *===========================================================================*/

int lnn_layer_backward(lnn_layer_t* layer, const nimcp_tensor_t* upstream_grad)
{
    /* WHAT: Compute gradients w.r.t. layer parameters using adjoint method
     * WHY:  Enable training of LTC layers
     * HOW:  Chain rule through layer dynamics: upstream_grad → param gradients
     */

    if (!layer || !upstream_grad) return LNN_ERROR_NULL_POINTER;

    /* Validate gradient dimensions */
    if (nimcp_tensor_numel(upstream_grad) != layer->n_neurons) {
        NIMCP_LOGGING_ERROR("Upstream gradient size mismatch: got %zu, expected %u",
                           nimcp_tensor_numel(upstream_grad), layer->n_neurons);
        return LNN_ERROR_INVALID_DIMENSION;
    }

    const float* upstream_data = (const float*)nimcp_tensor_data_const(upstream_grad);
    const float* x_data = (const float*)nimcp_tensor_data_const(layer->x);
    const float* tau_data = (const float*)nimcp_tensor_data_const(layer->tau);

    /* Gradient accumulation for W_rec: ∂L/∂W_rec = ∂L/∂x * ∂x/∂W_rec
     * For LTC: ∂x/∂W_rec comes from activation term f(W_rec @ x)
     */
    float* grad_W_rec_data = (float*)nimcp_tensor_data(layer->grad_W_rec);
    uint32_t n = layer->n_neurons;

    /* Compute recurrent contribution h_rec = W_rec @ x */
    nimcp_tensor_t* h_rec = nimcp_tensor_mv(layer->W_rec, layer->x);
    if (!h_rec) {
        NIMCP_LOGGING_ERROR("Failed to compute recurrent contribution");
        return LNN_ERROR_OPERATION_FAILED;
    }

    float* h_rec_data = (float*)nimcp_tensor_data(h_rec);

    /* For each neuron j, accumulate gradient for its weights W_rec[j,:] */
    for (uint32_t j = 0; j < n; j++) {
        /* Compute activation derivative: f'(h_rec[j]) */
        float h_val = h_rec_data[j];
        float act_deriv;

        /* Use tanh derivative (matching layer activation) */
        float tanh_val = tanhf(h_val);
        act_deriv = 1.0f - tanh_val * tanh_val;

        /* Gradient w.r.t. W_rec[j,k] = upstream[j] * f'(h_rec[j]) * x[k] */
        for (uint32_t k = 0; k < n; k++) {
            grad_W_rec_data[j * n + k] += upstream_data[j] * act_deriv * x_data[k];
        }
    }

    nimcp_tensor_destroy(h_rec);

    /* Gradient w.r.t. tau_base: ∂L/∂tau_base
     * From LTC dynamics: dx/dt = -x/tau + f(...)
     * ∂L/∂tau = ∂L/∂x * ∂x/∂tau = upstream * (x / tau^2)
     * ∂L/∂tau_base = ∂L/∂tau * ∂tau/∂tau_base
     */
    float* grad_tau_base_data = (float*)nimcp_tensor_data(layer->grad_tau_base);

    for (uint32_t j = 0; j < n; j++) {
        float tau_j = tau_data[j];
        /* Guard against gradient explosion from small tau values (P0 fix)
         * Use conservative epsilon of 0.01 to prevent extreme gradients
         * This prevents tau_sq from becoming too small (e.g., tau=0.01 gives tau_sq=1e-4)
         */
        float tau_safe = fmaxf(tau_j, 0.01f);
        float tau_sq = tau_safe * tau_safe;

        /* ∂L/∂tau = upstream * x / tau^2 (decay term gradient) */
        float grad_tau = upstream_data[j] * x_data[j] / tau_sq;

        /* P0 fix: Check for NaN/Inf in gradient */
        if (isnan(grad_tau) || isinf(grad_tau)) {
            grad_tau = 0.0f;  /* Skip corrupted gradient */
            continue;
        }

        /* For simplified tau: tau = tau_base * sigmoid(...)
         * ∂tau/∂tau_base ≈ sigmoid(...) ≈ tau / tau_base
         * So ∂L/∂tau_base = grad_tau * (tau / tau_base)
         */
        float tau_base_j = ((float*)nimcp_tensor_data(layer->tau_base))[j];
        if (tau_base_j > 1e-6f) {
            float grad_update = grad_tau * (tau_j / tau_base_j);
            if (!isnan(grad_update) && !isinf(grad_update)) {
                grad_tau_base_data[j] += grad_update;
            }
        }
    }

    /* Gradient w.r.t. b_in: ∂L/∂b_in = upstream * f'(h_total)
     * where h_total = h_in + h_rec (input to activation)
     */
    float* grad_b_in_data = (float*)nimcp_tensor_data(layer->grad_b_in);

    /* Recompute h_rec for activation derivative */
    h_rec = nimcp_tensor_mv(layer->W_rec, layer->x);
    if (h_rec) {
        h_rec_data = (float*)nimcp_tensor_data(h_rec);

        for (uint32_t j = 0; j < n; j++) {
            float tanh_val = tanhf(h_rec_data[j]);
            float act_deriv = 1.0f - tanh_val * tanh_val;
            grad_b_in_data[j] += upstream_data[j] * act_deriv;
        }

        nimcp_tensor_destroy(h_rec);
    }

    /* Note: Gradient w.r.t. W_in requires input history (stored during forward pass)
     * This would be handled at network level with input caching or adjoint checkpointing
     * For now, we accumulate the computable gradients (W_rec, tau_base, b_in)
     */

    return LNN_SUCCESS;
}

void lnn_layer_reset_gradients(lnn_layer_t* layer)
{
    if (!layer) return;

    float* grad_W_in_data = (float*)nimcp_tensor_data(layer->grad_W_in);
    float* grad_W_rec_data = (float*)nimcp_tensor_data(layer->grad_W_rec);
    float* grad_W_tau_data = (float*)nimcp_tensor_data(layer->grad_W_tau);
    float* grad_b_in_data = (float*)nimcp_tensor_data(layer->grad_b_in);
    float* grad_b_tau_data = (float*)nimcp_tensor_data(layer->grad_b_tau);
    float* grad_tau_base_data = (float*)nimcp_tensor_data(layer->grad_tau_base);

    memset(grad_W_in_data, 0, nimcp_tensor_numel(layer->grad_W_in) * sizeof(float));
    memset(grad_W_rec_data, 0, nimcp_tensor_numel(layer->grad_W_rec) * sizeof(float));
    memset(grad_W_tau_data, 0, nimcp_tensor_numel(layer->grad_W_tau) * sizeof(float));
    memset(grad_b_in_data, 0, nimcp_tensor_numel(layer->grad_b_in) * sizeof(float));
    memset(grad_b_tau_data, 0, nimcp_tensor_numel(layer->grad_b_tau) * sizeof(float));
    memset(grad_tau_base_data, 0, nimcp_tensor_numel(layer->grad_tau_base) * sizeof(float));
}

int lnn_layer_get_gradients(
    const lnn_layer_t* layer,
    nimcp_tensor_t** grads,
    uint32_t* n_grads
)
{
    if (!layer || !grads || !n_grads) return LNN_ERROR_NULL_POINTER;

    /* Return pointers to all gradient tensors */
    grads[0] = layer->grad_W_in;
    grads[1] = layer->grad_W_rec;
    grads[2] = layer->grad_W_tau;
    grads[3] = layer->grad_b_in;
    grads[4] = layer->grad_b_tau;
    grads[5] = layer->grad_tau_base;

    *n_grads = 6;

    return LNN_SUCCESS;
}

int lnn_layer_apply_gradients(lnn_layer_t* layer, float learning_rate)
{
    if (!layer) return LNN_ERROR_NULL_POINTER;

    /* Simple SGD: θ = θ - lr * ∇θ */
    nimcp_tensor_t* lr_grad = NULL;

    /* Update W_in */
    lr_grad = nimcp_tensor_mul_scalar(layer->grad_W_in, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->W_in, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    /* Update W_rec */
    lr_grad = nimcp_tensor_mul_scalar(layer->grad_W_rec, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->W_rec, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    /* Update W_tau */
    lr_grad = nimcp_tensor_mul_scalar(layer->grad_W_tau, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->W_tau, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    /* Update biases */
    lr_grad = nimcp_tensor_mul_scalar(layer->grad_b_in, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->b_in, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    lr_grad = nimcp_tensor_mul_scalar(layer->grad_b_tau, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->b_tau, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    /* Update tau_base */
    lr_grad = nimcp_tensor_mul_scalar(layer->grad_tau_base, learning_rate);
    if (lr_grad) {
        nimcp_tensor_sub_(layer->tau_base, lr_grad);
        nimcp_tensor_destroy(lr_grad);
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Statistics and Introspection
 *===========================================================================*/

int lnn_layer_get_stats(
    const lnn_layer_t* layer,
    float* avg_tau,
    float* min_tau,
    float* max_tau,
    float* state_norm
)
{
    if (!layer) return LNN_ERROR_NULL_POINTER;

    /* Compute average tau */
    if (avg_tau) {
        nimcp_tensor_t* tau_sum = nimcp_tensor_sum(layer->tau);
        if (tau_sum) {
            *avg_tau = nimcp_tensor_get_flat(tau_sum, 0) / layer->n_neurons;
            nimcp_tensor_destroy(tau_sum);
        } else {
            *avg_tau = 0.0f;
        }
    }

    /* Compute min/max tau */
    if (min_tau) {
        nimcp_tensor_t* tau_min_tensor = nimcp_tensor_min(layer->tau);
        if (tau_min_tensor) {
            *min_tau = nimcp_tensor_get_flat(tau_min_tensor, 0);
            nimcp_tensor_destroy(tau_min_tensor);
        } else {
            *min_tau = 0.0f;
        }
    }

    if (max_tau) {
        nimcp_tensor_t* tau_max_tensor = nimcp_tensor_max(layer->tau);
        if (tau_max_tensor) {
            *max_tau = nimcp_tensor_get_flat(tau_max_tensor, 0);
            nimcp_tensor_destroy(tau_max_tensor);
        } else {
            *max_tau = 0.0f;
        }
    }

    /* Compute state L2 norm */
    if (state_norm) {
        *state_norm = nimcp_tensor_norm_p(layer->x, 2.0);
    }

    /* Update layer statistics */
    lnn_layer_t* mutable_layer = (lnn_layer_t*)layer;
    mutable_layer->avg_tau = avg_tau ? *avg_tau : 0.0f;
    mutable_layer->min_tau = min_tau ? *min_tau : 0.0f;
    mutable_layer->max_tau = max_tau ? *max_tau : 0.0f;

    return LNN_SUCCESS;
}

size_t lnn_layer_param_count(const lnn_layer_t* layer)
{
    if (!layer) return 0;

    size_t count = 0;

    /* Weight matrices */
    count += nimcp_tensor_numel(layer->W_in);
    count += nimcp_tensor_numel(layer->W_rec);
    count += nimcp_tensor_numel(layer->W_tau);

    /* Biases */
    count += nimcp_tensor_numel(layer->b_in);
    count += nimcp_tensor_numel(layer->b_tau);

    /* Base time constants */
    count += nimcp_tensor_numel(layer->tau_base);

    return count;
}
