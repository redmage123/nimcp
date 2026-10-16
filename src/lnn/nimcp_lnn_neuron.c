/**
 * @file nimcp_lnn_neuron.c
 * @brief Implementation of LTC (Liquid Time-Constant) neuron operations
 *
 * WHAT: Single neuron dynamics with learnable time constants
 * WHY:  LTC neurons are the building blocks of Liquid Neural Networks
 * HOW:  Continuous-time state evolution via ODE integration
 */

#include "lnn/nimcp_lnn_neuron.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_neuron)

/*=============================================================================
 * Thread-Local RNG State
 *===========================================================================*/

/* Thread-local RNG state */
static __thread unsigned int tls_seed = 0;
static __thread bool tls_seed_initialized = false;

/* Forward declaration for randn_reset (used in set_rng_seed) */
static void randn_reset(void);

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
 * @brief Set the thread-local RNG seed for reproducibility
 *
 * WHAT: Override the thread-local seed
 * WHY:  Enable deterministic weight initialization with user-provided seed
 * HOW:  Set tls_seed directly and mark as initialized
 */
static void set_rng_seed(unsigned int seed) {
    tls_seed = seed;
    tls_seed_initialized = true;
    /* Clear any cached Box-Muller spare value from previous seed */
    randn_reset();
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
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Sigmoid activation function
 *
 * WHAT: σ(x) = 1 / (1 + exp(-x))
 * WHY:  Bounds output to [0, 1]
 * HOW:  Standard logistic function
 */
static inline float sigmoid(float x) {
    /* Guard against overflow */
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Derivative of sigmoid
 *
 * WHAT: σ'(x) = σ(x) * (1 - σ(x))
 */
static inline float sigmoid_derivative(float x) {
    float s = sigmoid(x);
    return s * (1.0f - s);
}

/**
 * @brief Apply activation function
 *
 * WHAT: Compute f(x) based on configured activation
 * WHY:  Nonlinearity needed for network expressiveness
 * HOW:  Switch on activation type
 */
static float apply_activation(float x, lnn_activation_t activation) {
    switch (activation) {
        case LNN_ACTIVATION_TANH:
            return tanhf(x);

        case LNN_ACTIVATION_SIGMOID:
            return sigmoid(x);

        case LNN_ACTIVATION_RELU:
            return x > 0.0f ? x : 0.0f;

        case LNN_ACTIVATION_GELU:
            /* GELU(x) ≈ x * Φ(x) where Φ is Gaussian CDF */
            return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));

        case LNN_ACTIVATION_SILU:
            /* SiLU(x) = x * sigmoid(x) */
            return x * sigmoid(x);

        case LNN_ACTIVATION_SOFTPLUS:
            /* softplus(x) = log(1 + exp(x)) */
            if (x > 20.0f) return x;  /* Avoid overflow */
            return logf(1.0f + expf(x));

        default:
            return tanhf(x);  /* Default to tanh */
    }
}

/**
 * @brief Derivative of activation function
 *
 * WHAT: Compute f'(x) for backpropagation
 * WHY:  Needed for gradient computation
 */
static float activation_derivative(float x, lnn_activation_t activation) {
    float fx;

    switch (activation) {
        case LNN_ACTIVATION_TANH:
            fx = tanhf(x);
            return 1.0f - fx * fx;

        case LNN_ACTIVATION_SIGMOID:
            return sigmoid_derivative(x);

        case LNN_ACTIVATION_RELU:
            return x > 0.0f ? 1.0f : 0.0f;

        case LNN_ACTIVATION_GELU:
            /* Approximation of GELU derivative */
            return 0.5f * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x))) +
                   0.5f * x * (1.0f - powf(tanhf(0.7978845608f * (x + 0.044715f * x * x * x)), 2.0f)) *
                   0.7978845608f * (1.0f + 3.0f * 0.044715f * x * x);

        case LNN_ACTIVATION_SILU:
            return sigmoid(x) + x * sigmoid_derivative(x);

        case LNN_ACTIVATION_SOFTPLUS:
            return sigmoid(x);

        default:
            fx = tanhf(x);
            return 1.0f - fx * fx;
    }
}

/**
 * @brief Generate random normal value using Box-Muller transform
 *
 * WHAT: Sample from Normal(0, 1)
 * WHY:  Weight initialization requires Gaussian samples
 * HOW:  Box-Muller transform of uniform samples
 *
 * THREAD SAFETY: Uses thread-local storage for cached spare value and RNG
 */
/* Thread-local Box-Muller state */
static __thread bool g_randn_has_spare = false;
static __thread float g_randn_spare = 0.0f;

/**
 * @brief Reset the Box-Muller state (call when reseeding)
 */
static void randn_reset(void) {
    g_randn_has_spare = false;
    g_randn_spare = 0.0f;
}

static float randn(void) {
    if (g_randn_has_spare) {
        g_randn_has_spare = false;
        return g_randn_spare;
    }

    /* Box-Muller transform with thread-safe RNG */
    float u1 = (float)thread_safe_rand() / (float)RAND_MAX;
    float u2 = (float)thread_safe_rand() / (float)RAND_MAX;

    /* Avoid log(0) */
    if (u1 < 1e-10f) u1 = 1e-10f;

    float r = sqrtf(-2.0f * logf(u1));
    float theta = 2.0f * M_PI * u2;

    g_randn_spare = r * sinf(theta);
    g_randn_has_spare = true;

    return r * cosf(theta);
}

/*=============================================================================
 * Neuron Lifecycle
 *===========================================================================*/

lnn_neuron_t* lnn_neuron_create(const lnn_neuron_config_t* config,
                                uint32_t n_inputs,
                                uint32_t n_recurrent) {
    /* Guard: validate inputs */
    if (!config) {
        NIMCP_LOGGING_ERROR("Neuron config is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_neuron_create: config is NULL");
        return NULL;
    }
    if (n_inputs == 0) {
        NIMCP_LOGGING_ERROR("Number of inputs must be > 0");
        return NULL;
    }

    /* Allocate neuron structure */
    lnn_neuron_t* neuron = (lnn_neuron_t*)nimcp_calloc(1, sizeof(lnn_neuron_t));
    if (!neuron) {
        NIMCP_LOGGING_ERROR("Failed to allocate neuron");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_neuron_create: failed to allocate neuron");
        return NULL;
    }

    /* Set dimensions */
    neuron->n_inputs = n_inputs;
    neuron->n_recurrent = n_recurrent;

    /* Allocate input weights */
    neuron->w_in = (float*)nimcp_calloc(n_inputs, sizeof(float));
    if (!neuron->w_in) {
        NIMCP_LOGGING_ERROR("Failed to allocate input weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_neuron_create: failed to allocate input weights");
        lnn_neuron_destroy(neuron);
        return NULL;
    }

    /* Allocate recurrent weights */
    if (n_recurrent > 0) {
        neuron->w_rec = (float*)nimcp_calloc(n_recurrent, sizeof(float));
        if (!neuron->w_rec) {
            NIMCP_LOGGING_ERROR("Failed to allocate recurrent weights");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_neuron_create: failed to allocate recurrent weights");
            lnn_neuron_destroy(neuron);
            return NULL;
        }
    }

    /* Allocate tau weights (bias + input + recurrent) */
    uint32_t n_tau = 1 + n_inputs + n_recurrent;  // +1 for bias term
    neuron->w_tau = (float*)nimcp_calloc(n_tau, sizeof(float));
    if (!neuron->w_tau) {
        NIMCP_LOGGING_ERROR("Failed to allocate tau weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_neuron_create: failed to allocate tau weights");
        lnn_neuron_destroy(neuron);
        return NULL;
    }

    /* Allocate gradients */
    neuron->grad_w_in = (float*)nimcp_calloc(n_inputs, sizeof(float));
    neuron->grad_w_rec = (float*)nimcp_calloc(n_recurrent, sizeof(float));
    neuron->grad_w_tau = (float*)nimcp_calloc(n_tau, sizeof(float));

    if (!neuron->grad_w_in || (!neuron->grad_w_rec && n_recurrent > 0) || !neuron->grad_w_tau) {
        NIMCP_LOGGING_ERROR("Failed to allocate gradient arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_neuron_create: failed to allocate gradient arrays");
        lnn_neuron_destroy(neuron);
        return NULL;
    }

    /* Initialize configuration */
    neuron->activation = config->activation;
    neuron->tau_base = config->tau_base_init;
    neuron->tau_current = config->tau_base_init;
    neuron->tau_min = config->tau_min;
    neuron->tau_max = config->tau_max;

    /* Initialize state */
    neuron->x = 0.0f;
    neuron->x_prev = 0.0f;
    neuron->dx_dt = 0.0f;
    neuron->b_in = 0.0f;
    neuron->b_tau = 0.0f;

    /* Set default role */
    neuron->role = LNN_ROLE_INTER;
    neuron->id = 0;

    return neuron;
}

void lnn_neuron_destroy(lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return;
    }

    /* Free weight arrays */
    if (neuron->w_in) nimcp_free(neuron->w_in);
    if (neuron->w_rec) nimcp_free(neuron->w_rec);
    if (neuron->w_tau) nimcp_free(neuron->w_tau);

    /* Free gradient arrays */
    if (neuron->grad_w_in) nimcp_free(neuron->grad_w_in);
    if (neuron->grad_w_rec) nimcp_free(neuron->grad_w_rec);
    if (neuron->grad_w_tau) nimcp_free(neuron->grad_w_tau);

    /* Free neuron structure */
    nimcp_free(neuron);
}

int lnn_neuron_init_weights(lnn_neuron_t* neuron, float std, uint64_t seed) {
    /* Guard: validate neuron */
    if (!neuron) {
        NIMCP_LOGGING_ERROR("Neuron is NULL");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Set random seed and reset Box-Muller state for reproducibility */
    if (seed == 0) {
        /* Use time-based seed */
        set_rng_seed((unsigned int)time(NULL) ^ (unsigned int)pthread_self());
    } else {
        /* Use user-provided seed for deterministic results */
        set_rng_seed((unsigned int)seed);
    }
    randn_reset();  /* Clear any cached spare value from previous calls */

    /* Use default std if not provided: 1/sqrt(n_inputs) */
    if (std <= 0.0f) {
        std = 1.0f / sqrtf((float)neuron->n_inputs);
    }

    /* Initialize input weights */
    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        neuron->w_in[i] = randn() * std;
    }

    /* Initialize recurrent weights with separate variance
     * Use 1/sqrt(n_recurrent) for proper scaling of recurrent connections
     */
    float rec_std = (neuron->n_recurrent > 0) ?
                    1.0f / sqrtf((float)neuron->n_recurrent) : std;
    for (uint32_t i = 0; i < neuron->n_recurrent; i++) {
        neuron->w_rec[i] = randn() * rec_std;
    }

    /* Initialize tau weights (smaller std for stability) */
    float tau_std = std * 0.1f;
    uint32_t n_tau = neuron->n_inputs + neuron->n_recurrent;
    for (uint32_t i = 0; i < n_tau; i++) {
        neuron->w_tau[i] = randn() * tau_std;
    }

    /* Initialize biases to small values */
    neuron->b_in = randn() * 0.01f;
    neuron->b_tau = randn() * 0.01f;

    return LNN_SUCCESS;
}

/*=============================================================================
 * Forward Computation
 *===========================================================================*/

float lnn_neuron_compute_tau(const lnn_neuron_t* neuron,
                             const float* input, uint32_t n_inputs,
                             const float* recurrent, uint32_t n_recurrent) {
    /* Guard: validate inputs */
    if (!neuron || !input) {
        return neuron ? neuron->tau_base : 1.0f;
    }
    if (n_inputs != neuron->n_inputs || n_recurrent != neuron->n_recurrent) {
        NIMCP_LOGGING_ERROR("Input dimension mismatch");
        return neuron->tau_base;
    }

    /* Compute W_τ · [x; I] + b_τ */
    float tau_input = neuron->b_tau;

    /* Add contribution from current state */
    tau_input += neuron->w_tau[0] * neuron->x;

    /* Add contribution from inputs */
    uint32_t offset = 1;
    for (uint32_t i = 0; i < n_inputs; i++) {
        tau_input += neuron->w_tau[offset + i] * input[i];
    }

    /* Add contribution from recurrent connections */
    offset += n_inputs;
    if (recurrent && n_recurrent > 0) {
        for (uint32_t i = 0; i < n_recurrent; i++) {
            tau_input += neuron->w_tau[offset + i] * recurrent[i];
        }
    }

    /* Apply sigmoid modulation: τ = τ_base * σ(tau_input) */
    float tau = neuron->tau_base * sigmoid(tau_input);

    /* Clamp to valid range [tau_min, tau_max] */
    if (tau < neuron->tau_min) tau = neuron->tau_min;
    if (tau > neuron->tau_max) tau = neuron->tau_max;

    return tau;
}

float lnn_neuron_compute_input(const lnn_neuron_t* neuron,
                               const float* input, uint32_t n_inputs,
                               const float* recurrent, uint32_t n_recurrent) {
    /* Guard: validate inputs */
    if (!neuron || !input) {
        return 0.0f;
    }
    if (n_inputs != neuron->n_inputs || n_recurrent != neuron->n_recurrent) {
        NIMCP_LOGGING_ERROR("Input dimension mismatch");
        return 0.0f;
    }

    /* Compute W_in · I + b_in */
    float total_input = neuron->b_in;

    for (uint32_t i = 0; i < n_inputs; i++) {
        total_input += neuron->w_in[i] * input[i];
    }

    /* Add recurrent contribution W_rec · x_rec */
    if (recurrent && n_recurrent > 0) {
        for (uint32_t i = 0; i < n_recurrent; i++) {
            total_input += neuron->w_rec[i] * recurrent[i];
        }
    }

    return total_input;
}

float lnn_neuron_compute_derivative(const lnn_neuron_t* neuron,
                                    float tau,
                                    float total_input) {
    /* Guard: validate neuron */
    if (!neuron) {
        return 0.0f;
    }

    /* Guard: prevent division by zero */
    if (tau <= 0.0f) {
        tau = neuron->tau_min;
    }

    /* Compute dx/dt = -x/τ + f(total_input) */
    float decay = -neuron->x / tau;
    float activation = apply_activation(total_input, neuron->activation);

    return decay + activation;
}

/**
 * @brief ODE derivative function for neuron dynamics
 *
 * WHAT: Compute dx/dt for use with ODE solvers
 * WHY:  ODE solvers need this function signature
 * HOW:  Package neuron, input, recurrent into params struct
 */
typedef struct {
    const lnn_neuron_t* neuron;
    const float* input;
    const float* recurrent;
    uint32_t n_inputs;
    uint32_t n_recurrent;
} lnn_neuron_ode_params_t;

static float neuron_derivative(float t, float x, void* params) {
    lnn_neuron_ode_params_t* p = (lnn_neuron_ode_params_t*)params;

    /* Temporarily set neuron state for tau computation */
    lnn_neuron_t* neuron = (lnn_neuron_t*)p->neuron;
    float x_saved = neuron->x;
    neuron->x = x;

    /* Compute tau and total input */
    float tau = lnn_neuron_compute_tau(p->neuron, p->input, p->n_inputs,
                                       p->recurrent, p->n_recurrent);
    float total_input = lnn_neuron_compute_input(p->neuron, p->input, p->n_inputs,
                                                  p->recurrent, p->n_recurrent);

    /* Compute derivative */
    float dx_dt = lnn_neuron_compute_derivative(p->neuron, tau, total_input);

    /* Restore neuron state */
    neuron->x = x_saved;

    return dx_dt;
}

int lnn_neuron_step(lnn_neuron_t* neuron,
                    const float* input, uint32_t n_inputs,
                    const float* recurrent, uint32_t n_recurrent,
                    float dt,
                    lnn_ode_method_t method) {
    /* Guard: validate inputs */
    if (!neuron || !input) {
        NIMCP_LOGGING_ERROR("NULL pointer in neuron_step");
        return LNN_ERROR_NULL_POINTER;
    }
    if (n_inputs != neuron->n_inputs || n_recurrent != neuron->n_recurrent) {
        NIMCP_LOGGING_ERROR("Input dimension mismatch");
        return LNN_ERROR_INVALID_DIMENSION;
    }
    if (dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Time step must be positive");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    /* Save previous state */
    neuron->x_prev = neuron->x;

    /* Compute current tau */
    neuron->tau_current = lnn_neuron_compute_tau(neuron, input, n_inputs,
                                                  recurrent, n_recurrent);

    /* Compute total input */
    float total_input = lnn_neuron_compute_input(neuron, input, n_inputs,
                                                  recurrent, n_recurrent);

    /* Compute derivative */
    neuron->dx_dt = lnn_neuron_compute_derivative(neuron, neuron->tau_current,
                                                   total_input);

    /* Setup ODE parameters */
    lnn_neuron_ode_params_t params = {
        .neuron = neuron,
        .input = input,
        .recurrent = recurrent,
        .n_inputs = n_inputs,
        .n_recurrent = n_recurrent
    };

    /* Integrate using specified method */
    float x_new = lnn_ode_step(method, 0.0f, neuron->x, dt,
                               neuron_derivative, &params);

    /* Check for numerical issues BEFORE updating state
     * This preserves the previous valid state if NaN/Inf detected
     */
    if (isnan(x_new) || isinf(x_new)) {
        NIMCP_LOGGING_ERROR("Numerical instability detected (NaN/Inf), preserving previous state");
        return LNN_ERROR_ODE_DIVERGENCE;
    }

    /* Update state only if x_new is valid */
    neuron->x = x_new;

    return LNN_SUCCESS;
}

/*=============================================================================
 * State Management
 *===========================================================================*/

float lnn_neuron_get_state(const lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return 0.0f;
    }
    return neuron->x;
}

void lnn_neuron_set_state(lnn_neuron_t* neuron, float x) {
    /* Guard: null check */
    if (!neuron) {
        return;
    }
    neuron->x = x;
}

float lnn_neuron_get_tau(const lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return 1.0f;
    }
    return neuron->tau_current;
}

void lnn_neuron_reset(lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return;
    }

    /* Reset state variables */
    neuron->x = 0.0f;
    neuron->x_prev = 0.0f;
    neuron->dx_dt = 0.0f;
    neuron->tau_current = neuron->tau_base;
}

/*=============================================================================
 * Gradient Computation
 *===========================================================================*/

int lnn_neuron_accumulate_gradients(lnn_neuron_t* neuron,
                                    float upstream_grad,
                                    const float* input,
                                    const float* recurrent,
                                    float tau) {
    /* Guard: validate inputs */
    if (!neuron || !input) {
        return LNN_ERROR_NULL_POINTER;
    }

    /* Compute total input for activation derivative */
    float total_input = lnn_neuron_compute_input(neuron, input, neuron->n_inputs,
                                                  recurrent, neuron->n_recurrent);

    /* Compute activation derivative */
    float act_deriv = activation_derivative(total_input, neuron->activation);

    /* Gradient w.r.t. input weights: ∂L/∂w_in = ∂L/∂x * f'(z) * I */
    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        neuron->grad_w_in[i] += upstream_grad * act_deriv * input[i];
    }

    /* Gradient w.r.t. recurrent weights */
    if (recurrent && neuron->n_recurrent > 0) {
        for (uint32_t i = 0; i < neuron->n_recurrent; i++) {
            neuron->grad_w_rec[i] += upstream_grad * act_deriv * recurrent[i];
        }
    }

    /* Gradient w.r.t. input bias */
    neuron->grad_b_in += upstream_grad * act_deriv;

    /* Gradient w.r.t. tau parameters (simplified) */
    /* ∂L/∂τ = ∂L/∂x * x/τ² */
    /* Guard: prevent gradient explosion from small tau values
     * Use adaptive epsilon based on tau_min configuration (P0 fix)
     * This prevents tau_sq from becoming too small relative to configured range
     */
    float tau_epsilon = fmaxf(1e-4f, neuron->tau_min * 0.01f);
    float tau_safe = fmaxf(tau, tau_epsilon);
    float tau_sq = tau_safe * tau_safe;
    float grad_tau = upstream_grad * neuron->x / tau_sq;

    /* ∂L/∂w_τ = ∂L/∂τ * ∂τ/∂w_τ = ∂L/∂τ * τ_base * σ'(z_τ) * [x; I] */
    float tau_input = neuron->b_tau;
    for (uint32_t i = 0; i < neuron->n_inputs + neuron->n_recurrent + 1; i++) {
        if (i == 0) {
            tau_input += neuron->w_tau[i] * neuron->x;
        } else if (i <= neuron->n_inputs) {
            tau_input += neuron->w_tau[i] * input[i - 1];
        } else if (recurrent) {
            tau_input += neuron->w_tau[i] * recurrent[i - neuron->n_inputs - 1];
        }
    }

    float sig_deriv = sigmoid_derivative(tau_input);
    float grad_tau_weight = grad_tau * neuron->tau_base * sig_deriv;

    /* Accumulate tau weight gradients */
    neuron->grad_w_tau[0] += grad_tau_weight * neuron->x;
    for (uint32_t i = 0; i < neuron->n_inputs; i++) {
        neuron->grad_w_tau[i + 1] += grad_tau_weight * input[i];
    }
    if (recurrent && neuron->n_recurrent > 0) {
        for (uint32_t i = 0; i < neuron->n_recurrent; i++) {
            neuron->grad_w_tau[neuron->n_inputs + 1 + i] += grad_tau_weight * recurrent[i];
        }
    }

    /* Gradient w.r.t. tau bias */
    neuron->grad_b_tau += grad_tau_weight;

    /* Gradient w.r.t. tau_base */
    neuron->grad_tau_base += grad_tau * sigmoid(tau_input);

    return LNN_SUCCESS;
}

void lnn_neuron_reset_gradients(lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return;
    }

    /* Reset input weight gradients */
    memset(neuron->grad_w_in, 0, neuron->n_inputs * sizeof(float));

    /* Reset recurrent weight gradients */
    if (neuron->n_recurrent > 0) {
        memset(neuron->grad_w_rec, 0, neuron->n_recurrent * sizeof(float));
    }

    /* Reset tau weight gradients (+1 for bias term, matching allocation) */
    uint32_t n_tau = 1 + neuron->n_inputs + neuron->n_recurrent;
    memset(neuron->grad_w_tau, 0, n_tau * sizeof(float));

    /* Reset bias gradients */
    neuron->grad_b_in = 0.0f;
    neuron->grad_b_tau = 0.0f;
    neuron->grad_tau_base = 0.0f;
}

int lnn_neuron_get_gradients(const lnn_neuron_t* neuron,
                             float* grad_w_in,
                             float* grad_w_rec,
                             float* grad_w_tau,
                             float* grad_b_in,
                             float* grad_b_tau,
                             float* grad_tau_base) {
    /* Guard: validate inputs */
    if (!neuron) {
        return LNN_ERROR_NULL_POINTER;
    }

    /* Copy input weight gradients */
    if (grad_w_in) {
        memcpy(grad_w_in, neuron->grad_w_in, neuron->n_inputs * sizeof(float));
    }

    /* Copy recurrent weight gradients */
    if (grad_w_rec && neuron->n_recurrent > 0) {
        memcpy(grad_w_rec, neuron->grad_w_rec, neuron->n_recurrent * sizeof(float));
    }

    /* Copy tau weight gradients (W9: +1 for bias term to match allocation) */
    if (grad_w_tau) {
        uint32_t n_tau = 1 + neuron->n_inputs + neuron->n_recurrent;
        memcpy(grad_w_tau, neuron->grad_w_tau, n_tau * sizeof(float));
    }

    /* Copy bias gradients */
    if (grad_b_in) *grad_b_in = neuron->grad_b_in;
    if (grad_b_tau) *grad_b_tau = neuron->grad_b_tau;
    if (grad_tau_base) *grad_tau_base = neuron->grad_tau_base;

    return LNN_SUCCESS;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

const char* lnn_neuron_role_to_string(lnn_neuron_role_t role) {
    switch (role) {
        case LNN_ROLE_SENSORY:  return "SENSORY";
        case LNN_ROLE_INTER:    return "INTER";
        case LNN_ROLE_COMMAND:  return "COMMAND";
        case LNN_ROLE_MOTOR:    return "MOTOR";
        default:                return "UNKNOWN";
    }
}

size_t lnn_neuron_param_count(const lnn_neuron_t* neuron) {
    /* Guard: null check */
    if (!neuron) {
        return 0;
    }

    /* Count parameters:
     * - w_in: n_inputs
     * - w_rec: n_recurrent
     * - w_tau: n_inputs + n_recurrent
     * - b_in: 1
     * - b_tau: 1
     * - tau_base: 1
     */
    size_t count = neuron->n_inputs +          /* w_in */
                   neuron->n_recurrent +        /* w_rec */
                   neuron->n_inputs +           /* w_tau (input part) */
                   neuron->n_recurrent +        /* w_tau (recurrent part) */
                   3;                           /* b_in, b_tau, tau_base */

    return count;
}
