/**
 * @file nimcp_lnn_neuron.h
 * @brief Single LTC (Liquid Time-Constant) neuron API
 *
 * WHAT: Operations on individual LTC neurons
 * WHY:  LTC neurons are the fundamental building block of LNNs
 * HOW:  State evolution via dx/dt = -x/τ(x,I) + f(W_in*I + W_rec*x + b)
 *
 * BIOLOGICAL BASIS:
 * LTC neurons model adaptive neuronal dynamics where the membrane time constant
 * τ depends on the current state and inputs, similar to voltage-gated channels
 * modulating neuronal excitability.
 */

#ifndef NIMCP_LNN_NEURON_H
#define NIMCP_LNN_NEURON_H

#include "nimcp_lnn_types.h"
#include "nimcp_lnn_ode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Neuron Lifecycle
 *===========================================================================*/

/**
 * @brief Create and initialize a new LTC neuron
 *
 * WHAT: Allocate and initialize neuron structure with specified configuration
 * WHY:  Each neuron needs dedicated state, weights, and configuration
 * HOW:  Allocate memory, initialize weights to zero, set default state
 *
 * @param config Neuron configuration (activation, tau limits, etc.)
 * @param n_inputs Number of input connections
 * @param n_recurrent Number of recurrent connections
 * @return Pointer to new neuron, NULL on failure
 */
lnn_neuron_t* lnn_neuron_create(const lnn_neuron_config_t* config,
                                uint32_t n_inputs,
                                uint32_t n_recurrent);

/**
 * @brief Destroy neuron and free all resources
 *
 * WHAT: Free all allocated memory for neuron
 * WHY:  Prevent memory leaks
 * HOW:  Free weight arrays, gradient arrays, then neuron struct
 *
 * @param neuron Neuron to destroy
 */
void lnn_neuron_destroy(lnn_neuron_t* neuron);

/**
 * @brief Initialize neuron weights with random values
 *
 * WHAT: Set weights to random values from Normal(0, std)
 * WHY:  Random initialization breaks symmetry for learning
 * HOW:  Use Box-Muller transform for normal distribution
 *
 * @param neuron Neuron to initialize
 * @param std Standard deviation (default: 1/sqrt(n_inputs))
 * @param seed Random seed (0 = use current time)
 * @return 0 on success, negative on error
 */
int lnn_neuron_init_weights(lnn_neuron_t* neuron, float std, uint64_t seed);

/*=============================================================================
 * Forward Computation
 *===========================================================================*/

/**
 * @brief Compute liquid time constant τ(x, I)
 *
 * WHAT: Calculate input-dependent time constant
 * WHY:  LTC neurons adapt their temporal dynamics based on input
 * HOW:  τ = τ_base * sigmoid(W_τ · [x; I] + b_τ), clamped to [tau_min, tau_max]
 *
 * BIOLOGICAL BASIS:
 * Voltage-gated channels modulate membrane time constant based on activity.
 * High activity can speed up (decrease τ) or slow down (increase τ) dynamics.
 *
 * @param neuron LTC neuron
 * @param input Input vector [n_inputs]
 * @param n_inputs Input dimension
 * @param recurrent Recurrent state vector [n_recurrent]
 * @param n_recurrent Recurrent dimension
 * @return τ(x, I) time constant in milliseconds
 */
float lnn_neuron_compute_tau(const lnn_neuron_t* neuron,
                             const float* input, uint32_t n_inputs,
                             const float* recurrent, uint32_t n_recurrent);

/**
 * @brief Compute total weighted input to neuron
 *
 * WHAT: Calculate W_in·I + W_rec·x + b
 * WHY:  Input computation needed for both state update and τ calculation
 * HOW:  Dot products of weights with inputs and recurrent state
 *
 * @param neuron LTC neuron
 * @param input Input vector [n_inputs]
 * @param n_inputs Input dimension
 * @param recurrent Recurrent state vector [n_recurrent]
 * @param n_recurrent Recurrent dimension
 * @return total_input weighted sum
 */
float lnn_neuron_compute_input(const lnn_neuron_t* neuron,
                               const float* input, uint32_t n_inputs,
                               const float* recurrent, uint32_t n_recurrent);

/**
 * @brief Compute state derivative dx/dt
 *
 * WHAT: Calculate time derivative of neuron state
 * WHY:  Needed for ODE integration
 * HOW:  dx/dt = -x/τ + f(total_input) where f is activation function
 *
 * BIOLOGICAL BASIS:
 * Leaky integration: neuron state decays with time constant τ while being
 * driven by synaptic input processed through nonlinearity.
 *
 * @param neuron LTC neuron
 * @param tau Current time constant
 * @param total_input Weighted input sum (from lnn_neuron_compute_input)
 * @return dx/dt derivative
 */
float lnn_neuron_compute_derivative(const lnn_neuron_t* neuron,
                                    float tau,
                                    float total_input);

/**
 * @brief Execute one integration timestep
 *
 * WHAT: Advance neuron state by dt using specified ODE solver
 * WHY:  Core operation for continuous-time dynamics
 * HOW:  1. Compute τ(x,I) 2. Compute total_input 3. Solve ODE for new x
 *
 * @param neuron LTC neuron (state updated in place)
 * @param input Input vector [n_inputs]
 * @param n_inputs Input dimension
 * @param recurrent Recurrent state vector [n_recurrent]
 * @param n_recurrent Recurrent dimension
 * @param dt Time step (milliseconds)
 * @param method ODE integration method
 * @return 0 on success, negative on error
 */
int lnn_neuron_step(lnn_neuron_t* neuron,
                    const float* input, uint32_t n_inputs,
                    const float* recurrent, uint32_t n_recurrent,
                    float dt,
                    lnn_ode_method_t method);

/*=============================================================================
 * State Management
 *===========================================================================*/

/**
 * @brief Get current neuron state
 *
 * @param neuron LTC neuron
 * @return x current state value
 */
float lnn_neuron_get_state(const lnn_neuron_t* neuron);

/**
 * @brief Set neuron state
 *
 * @param neuron LTC neuron
 * @param x New state value
 */
void lnn_neuron_set_state(lnn_neuron_t* neuron, float x);

/**
 * @brief Get current time constant
 *
 * @param neuron LTC neuron
 * @return τ current time constant
 */
float lnn_neuron_get_tau(const lnn_neuron_t* neuron);

/**
 * @brief Reset neuron to initial state
 *
 * WHAT: Reset state to zero, clear history
 * WHY:  Needed between sequences or episodes
 * HOW:  Set x = 0, x_prev = 0, dx_dt = 0, tau_current = tau_base
 *
 * @param neuron LTC neuron
 */
void lnn_neuron_reset(lnn_neuron_t* neuron);

/*=============================================================================
 * Gradient Computation (for training)
 *===========================================================================*/

/**
 * @brief Accumulate gradients for backpropagation
 *
 * WHAT: Add gradient contributions to parameter gradients
 * WHY:  Enables learning via gradient descent
 * HOW:  Apply chain rule to compute ∂L/∂θ where θ are neuron parameters
 *
 * @param neuron LTC neuron
 * @param upstream_grad Gradient from upstream layers (∂L/∂x)
 * @param input Input vector at current timestep
 * @param recurrent Recurrent state at current timestep
 * @param tau Time constant at current timestep
 * @return 0 on success, negative on error
 */
int lnn_neuron_accumulate_gradients(lnn_neuron_t* neuron,
                                    float upstream_grad,
                                    const float* input,
                                    const float* recurrent,
                                    float tau);

/**
 * @brief Reset all gradient accumulators to zero
 *
 * WHAT: Clear gradient buffers
 * WHY:  Prepare for new gradient accumulation
 * HOW:  Set all grad_* arrays to zero
 *
 * @param neuron LTC neuron
 */
void lnn_neuron_reset_gradients(lnn_neuron_t* neuron);

/**
 * @brief Get accumulated gradients
 *
 * WHAT: Retrieve gradient values for external optimizer
 * WHY:  Optimizer needs gradients to update weights
 * HOW:  Copy gradient buffers to output arrays
 *
 * @param neuron LTC neuron
 * @param grad_w_in Output: input weight gradients [n_inputs]
 * @param grad_w_rec Output: recurrent weight gradients [n_recurrent]
 * @param grad_w_tau Output: tau weight gradients [n_inputs + n_recurrent]
 * @param grad_b_in Output: input bias gradient
 * @param grad_b_tau Output: tau bias gradient
 * @param grad_tau_base Output: base tau gradient
 * @return 0 on success, negative on error
 */
int lnn_neuron_get_gradients(const lnn_neuron_t* neuron,
                             float* grad_w_in,
                             float* grad_w_rec,
                             float* grad_w_tau,
                             float* grad_b_in,
                             float* grad_b_tau,
                             float* grad_tau_base);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Convert neuron role to string
 *
 * @param role Neuron role enum
 * @return String representation
 */
const char* lnn_neuron_role_to_string(lnn_neuron_role_t role);

/**
 * @brief Count total number of parameters in neuron
 *
 * WHAT: Calculate total learnable parameters
 * WHY:  Needed for memory allocation and optimizer setup
 * HOW:  Sum dimensions of all weight arrays plus biases
 *
 * @param neuron LTC neuron
 * @return Total parameter count
 */
size_t lnn_neuron_param_count(const lnn_neuron_t* neuron);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_NEURON_H */
