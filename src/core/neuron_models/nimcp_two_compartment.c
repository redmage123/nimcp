#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_two_compartment.c - Two-Compartment Neuron Implementation
//=============================================================================
/**
 * @file nimcp_two_compartment.c
 * @brief Implementation of two-compartment neuron model
 *
 * IMPLEMENTATION NOTES:
 * - Uses RK4 integration from nimcp_integration.h for accuracy
 * - Coupled ODEs solved simultaneously
 * - Spike detection only at soma
 * - Both compartments reset after spike (with different reset values)
 *
 * PERFORMANCE:
 * - RK4: 4 derivative evaluations per update (~2x slower than point neuron)
 * - Euler: 1 derivative evaluation per update (faster but less accurate)
 * - Memory: 6 floats per neuron (V_soma, V_dend, I_soma, I_dend, t_last_spike, params*)
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include "core/neuron_models/nimcp_two_compartment.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/neuron_models/nimcp_neuron_model_internal.h"
#include "utils/numerical/nimcp_integration.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "two_compartment"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(two_compartment)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void two_compartment_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_NEURON_MODEL_TWO_COMPARTMENT,
        .module_name = "two_compartment",
        .inbox_capacity = 256,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for two_compartment module");
    }
}

__attribute__((constructor))
static void two_compartment_bio_init(void) {
    pthread_once(&bio_init_once, two_compartment_bio_init_impl);
}

__attribute__((destructor))
static void two_compartment_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for two_compartment module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Internal State Structure
//=============================================================================

/**
 * @brief Internal state for two-compartment neuron
 *
 * WHAT: Complete neuron state including voltages, currents, and parameters
 * WHY:  Encapsulates all information needed for simulation
 * HOW:  Stored in flexible array member of neuron_model_state_struct
 *
 * MEMORY LAYOUT:
 * - Fixed size: sizeof(two_compartment_state_t) = ~72 bytes
 * - Cache-friendly: frequently accessed fields first
 *
 * NOTE: This structure is stored in the model_state[] flexible array
 *       of neuron_model_state_struct, NOT directly allocated.
 */
typedef struct {
    /** Current state variables */
    float V_soma;              /**< Somatic membrane voltage (mV) */
    float V_dend;              /**< Dendritic membrane voltage (mV) */

    /** Current accumulators (reset after each integration step) */
    float I_soma;              /**< Current injected into soma (pA) */
    float I_dend;              /**< Current injected into dendrite (pA) */

    /** Spike tracking */
    float t_last_spike;        /**< Time of last spike (ms) */
    bool in_refractory;        /**< True if in refractory period */

    /** Neuron parameters */
    two_compartment_params_t params;
} two_compartment_state_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static void two_comp_init(neuron_model_state_t state, const void* params);
static void two_comp_destroy(neuron_model_state_t state);
static void two_comp_update(neuron_model_state_t state, float dt, float input_current);
static bool two_comp_check_spike(const neuron_model_state_t state);
static void two_comp_post_spike(neuron_model_state_t state);
static float two_comp_get_voltage(const neuron_model_state_t state);
static void two_comp_set_voltage(neuron_model_state_t state, float voltage);
static void two_comp_reset(neuron_model_state_t state);
static void two_comp_copy(neuron_model_state_t dst, const neuron_model_state_t src);

//=============================================================================
// Virtual Function Table
//=============================================================================

static const neuron_model_vtable_t two_compartment_vtable = {
    .name = "Two-Compartment (Soma+Dendrite)",
    .type = NEURON_MODEL_ADEX,  // Reuse ADEX enum for now (or add new type)
    .state_size = sizeof(two_compartment_state_t),

    .init = two_comp_init,
    .destroy = two_comp_destroy,
    .update = two_comp_update,
    .check_spike = two_comp_check_spike,
    .post_spike = two_comp_post_spike,
    .get_voltage = two_comp_get_voltage,
    .set_voltage = two_comp_set_voltage,
    .reset = two_comp_reset,
    .copy = two_comp_copy
};

//=============================================================================
// Derivative Function for ODE Integration
//=============================================================================

/**
 * @brief Derivative function for two-compartment system
 *
 * WHAT: Computes dV/dt for both compartments
 * WHY:  Required by nimcp_integration.h interface
 * HOW:  Coupled ODEs from cable theory
 *
 * STATE VECTOR:
 *   state[0] = V_soma
 *   state[1] = V_dend
 *
 * DERIVATIVES:
 *   dV_soma/dt = (-g_leak*(V_soma - E_leak) + I_soma + g_couple*(V_dend - V_soma)) / C_soma
 *   dV_dend/dt = (-g_leak*(V_dend - E_leak) + I_dend + g_couple*(V_soma - V_dend)) / C_dend
 *
 * COMPLEXITY: O(1) - just arithmetic operations
 *
 * @param state State vector [V_soma, V_dend]
 * @param t Current time (unused for time-invariant system)
 * @param params Pointer to two_compartment_state_t (contains currents and params)
 * @param derivatives Output: [dV_soma/dt, dV_dend/dt]
 */
static void two_compartment_derivatives(
    const float* state,
    float t,
    void* params,
    float* derivatives)
{
    (void)t;  // Time-invariant system

    two_compartment_state_t* neuron = (two_compartment_state_t*)params;

    // Extract state
    float V_soma = state[0];
    float V_dend = state[1];

    // Extract parameters
    float C_soma = neuron->params.C_soma;
    float C_dend = neuron->params.C_dend;
    float g_leak = neuron->params.g_leak;
    float g_couple = neuron->params.g_couple;
    float E_leak = neuron->params.E_leak;

    // Extract currents (injected by synapses or external stimulation)
    float I_soma = neuron->I_soma;
    float I_dend = neuron->I_dend;

    // Compute derivatives using coupled cable equations
    // Soma: leak + injected current + coupling from dendrite
    float I_leak_soma = -g_leak * (V_soma - E_leak);
    float I_coupling_to_soma = g_couple * (V_dend - V_soma);
    derivatives[0] = (I_leak_soma + I_soma + I_coupling_to_soma) / C_soma;

    // Dendrite: leak + injected current + coupling from soma
    float I_leak_dend = -g_leak * (V_dend - E_leak);
    float I_coupling_to_dend = g_couple * (V_soma - V_dend);
    derivatives[1] = (I_leak_dend + I_dend + I_coupling_to_dend) / C_dend;
}

//=============================================================================
// Vtable Function Implementations
//=============================================================================

/**
 * @brief Initialize two-compartment neuron state
 *
 * WHAT: Allocates and initializes neuron state
 * WHY:  Required by neuron model interface
 * HOW:  Sets voltages to resting potential, clears currents
 *
 * COMPLEXITY: O(1)
 */
static void two_comp_init(neuron_model_state_t state_opaque, const void* params_in)
{
    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    // Copy parameters (or use defaults if NULL)
    if (params_in != NULL) {
        memcpy(&state->params, params_in, sizeof(two_compartment_params_t));
    } else {
        state->params = two_compartment_default_params();
    }

    // Initialize state to resting potential
    state->V_soma = state->params.E_leak;
    state->V_dend = state->params.E_leak;

    // Clear currents
    state->I_soma = 0.0F;
    state->I_dend = 0.0F;

    // Clear spike tracking
    state->t_last_spike = -1000.0F;  // Large negative value
    state->in_refractory = false;
}

/**
 * @brief Destroy two-compartment neuron state
 *
 * WHAT: Cleanup function (currently no-op)
 * WHY:  Required by interface, future-proofing
 * HOW:  No dynamic allocations to free in this model
 */
static void two_comp_destroy(neuron_model_state_t state_opaque)
{
    (void)state_opaque;  // No cleanup needed
}

/**
 * @brief Update neuron dynamics for one timestep
 *
 * WHAT: Integrates coupled ODEs forward by dt
 * WHY:  Core simulation step
 * HOW:  Uses RK4 or Euler integration from nimcp_integration.h
 *
 * ALGORITHM:
 * 1. Check refractory period
 * 2. Accumulate input_current to appropriate compartment
 * 3. Integrate coupled ODEs
 * 4. Clear current accumulators
 * 5. Update refractory timer
 *
 * COMPLEXITY: O(1) for Euler, O(4) for RK4
 *
 * @param state Neuron state
 * @param dt Timestep (ms)
 * @param input_current Total synaptic input current (pA)
 */
static void two_comp_update(neuron_model_state_t state_opaque, float dt, float input_current)
{
    // Process pending bio-async messages
    if (bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    // Add input current to soma by default (or could be split)
    // In full implementation, synapse system would call two_compartment_add_current()
    // to target specific compartments
    state->I_soma += input_current;

    // Check refractory period
    if (state->in_refractory) {
        state->t_last_spike += dt;
        if (state->t_last_spike >= state->params.refractory_period) {
            state->in_refractory = false;
        }
        // During refractory, voltage is clamped - don't integrate
        state->I_soma = 0.0F;
        state->I_dend = 0.0F;
        return;
    }

    // Integrate coupled ODEs
    float voltage_state[2] = { state->V_soma, state->V_dend };

    // Select integration method
    integration_method_t method;
    switch (state->params.integration_method) {
        case ODE_EULER:
            method = INTEGRATION_EULER;
            break;
        case ODE_RK4:
            method = INTEGRATION_RK4;
            break;
        default:
            method = INTEGRATION_RK4;  // Default to RK4
            break;
    }

    // Perform integration step
    // Note: two_compartment_derivatives reads I_soma and I_dend from state
    bool success = integration_step(
        method,
        two_compartment_derivatives,
        voltage_state,
        0.0F,  // t (time-invariant system)
        dt,
        2,     // dimension (V_soma, V_dend)
        state  // params (passed to derivative function)
    );

    if (success) {
        state->V_soma = voltage_state[0];
        state->V_dend = voltage_state[1];
    }
    // If integration fails, voltages remain unchanged (safe fallback)

    // Clear current accumulators for next step
    state->I_soma = 0.0F;
    state->I_dend = 0.0F;
}

/**
 * @brief Check if neuron has spiked
 *
 * WHAT: Tests if somatic voltage crossed threshold
 * WHY:  Spike detection for event-driven processing
 * HOW:  Simple threshold crossing at soma only
 *
 * COMPLEXITY: O(1)
 *
 * @return true if V_soma >= threshold, false otherwise
 */
static bool two_comp_check_spike(const neuron_model_state_t state_opaque)
{
    const two_compartment_state_t* state = (const two_compartment_state_t*)state_opaque->model_state;

    // Spike only detected at soma
    return (state->V_soma >= state->params.V_threshold) && !state->in_refractory;
}

/**
 * @brief Perform post-spike reset
 *
 * WHAT: Resets voltages and enters refractory period
 * WHY:  Implements spike aftermath and refractory dynamics
 * HOW:  Sets V_soma to reset value, attenuates V_dend
 *
 * RESET STRATEGY:
 * - Soma: Reset to V_reset (strong hyperpolarization)
 * - Dendrite: Partial reset (50% of soma reset magnitude)
 *   - Reflects back-propagating action potential
 *   - Biologically realistic (bAP attenuates in dendrites)
 *
 * COMPLEXITY: O(1)
 */
static void two_comp_post_spike(neuron_model_state_t state_opaque)
{
    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    // Reset soma to reset voltage
    state->V_soma = state->params.V_reset;

    // Partially reset dendrite (back-propagating action potential)
    // Dendrite reset is attenuated (50% of reset magnitude)
    float reset_magnitude = state->params.V_threshold - state->params.V_reset;
    state->V_dend -= 0.5F * reset_magnitude;

    // Clamp dendrite to reasonable range
    if (state->V_dend < state->params.V_reset - 10.0F) {
        state->V_dend = state->params.V_reset - 10.0F;
    }

    // Enter refractory period
    state->in_refractory = true;
    state->t_last_spike = 0.0F;
}

/**
 * @brief Get somatic voltage
 *
 * WHAT: Returns current soma voltage
 * WHY:  Standard interface for monitoring
 * HOW:  Returns V_soma field
 *
 * @return Somatic membrane voltage (mV)
 */
static float two_comp_get_voltage(const neuron_model_state_t state_opaque)
{
    const two_compartment_state_t* state = (const two_compartment_state_t*)state_opaque->model_state;
    return state->V_soma;
}

/**
 * @brief Set somatic voltage
 *
 * WHAT: Manually sets soma voltage (dendrite unchanged)
 * WHY:  External stimulation or initialization
 * HOW:  Directly modifies V_soma
 */
static void two_comp_set_voltage(neuron_model_state_t state_opaque, float voltage)
{
    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;
    state->V_soma = voltage;
}

/**
 * @brief Reset neuron to resting state
 *
 * WHAT: Returns neuron to initial conditions
 * WHY:  Network reset and testing
 * HOW:  Sets all state to resting values
 */
static void two_comp_reset(neuron_model_state_t state_opaque)
{
    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    state->V_soma = state->params.E_leak;
    state->V_dend = state->params.E_leak;
    state->I_soma = 0.0F;
    state->I_dend = 0.0F;
    state->t_last_spike = -1000.0F;
    state->in_refractory = false;
}

/**
 * @brief Copy neuron state
 *
 * WHAT: Deep copy of neuron state
 * WHY:  Snapshotting and cloning
 * HOW:  memcpy entire state structure
 */
static void two_comp_copy(neuron_model_state_t dst_opaque, const neuron_model_state_t src_opaque)
{
    // Guard: NULL pointer check
    if (!dst_opaque || !src_opaque) {
        return;
    }

    // Access flexible array member, not the wrapper struct
    two_compartment_state_t* dst = (two_compartment_state_t*)dst_opaque->model_state;
    const two_compartment_state_t* src = (const two_compartment_state_t*)src_opaque->model_state;

    memcpy(dst, src, sizeof(two_compartment_state_t));
}

//=============================================================================
// Public API Implementation
//=============================================================================

const neuron_model_vtable_t* two_compartment_get_vtable(void)
{
    return &two_compartment_vtable;
}

two_compartment_params_t two_compartment_default_params(void)
{
    two_compartment_params_t params = {
        .C_soma = TWO_COMP_DEFAULT_C_SOMA,
        .C_dend = TWO_COMP_DEFAULT_C_DEND,
        .g_leak = TWO_COMP_DEFAULT_G_LEAK,
        .g_couple = TWO_COMP_DEFAULT_G_COUPLE,
        .E_leak = TWO_COMP_LEAK_POTENTIAL,
        .V_threshold = TWO_COMP_SPIKE_THRESHOLD,
        .V_reset = TWO_COMP_RESET_VOLTAGE,
        .refractory_period = TWO_COMP_REFRACTORY_PERIOD,
        .integration_method = ODE_RK4  // Default to RK4 for accuracy
    };
    return params;
}

two_compartment_params_t two_compartment_create_params(
    float C_soma,
    float C_dend,
    float g_leak,
    float g_couple,
    float E_leak,
    ode_integration_method_t integration_method)
{
    two_compartment_params_t params = {
        .C_soma = C_soma,
        .C_dend = C_dend,
        .g_leak = g_leak,
        .g_couple = g_couple,
        .E_leak = E_leak,
        .V_threshold = TWO_COMP_SPIKE_THRESHOLD,
        .V_reset = TWO_COMP_RESET_VOLTAGE,
        .refractory_period = TWO_COMP_REFRACTORY_PERIOD,
        .integration_method = integration_method
    };
    return params;
}

float two_compartment_calculate_attenuation(const two_compartment_params_t* params)
{
    if (params == NULL) {
        return 0.0F;
    }

    // Steady-state transfer coefficient from dendrite to soma
    // transfer = g_couple / (g_leak + g_couple)
    // attenuation = 1 - transfer
    float transfer = params->g_couple / (params->g_leak + params->g_couple);
    float attenuation = 1.0F - transfer;

    return attenuation;
}

void two_compartment_calculate_time_constants(
    const two_compartment_params_t* params,
    float* tau_soma,
    float* tau_dend)
{
    if (params == NULL) {
        return;
    }

    // Membrane time constant: τ = C / g_leak
    if (tau_soma != NULL) {
        *tau_soma = params->C_soma / params->g_leak;
    }

    if (tau_dend != NULL) {
        *tau_dend = params->C_dend / params->g_leak;
    }
}

void two_compartment_get_compartment_voltages(
    const neuron_model_state_t state_opaque,
    float* V_soma,
    float* V_dend)
{
    if (state_opaque == NULL) {
        return;
    }

    const two_compartment_state_t* state = (const two_compartment_state_t*)state_opaque->model_state;

    if (V_soma != NULL) {
        *V_soma = state->V_soma;
    }

    if (V_dend != NULL) {
        *V_dend = state->V_dend;
    }
}

void two_compartment_set_compartment_voltages(
    neuron_model_state_t state_opaque,
    float V_soma,
    float V_dend)
{
    if (state_opaque == NULL) {
        return;
    }

    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    state->V_soma = V_soma;
    state->V_dend = V_dend;
}

void two_compartment_add_current(
    neuron_model_state_t state_opaque,
    float current,
    compartment_target_t target)
{
    if (state_opaque == NULL) {
        return;
    }

    two_compartment_state_t* state = (two_compartment_state_t*)state_opaque->model_state;

    switch (target) {
        case COMPARTMENT_SOMA:
            state->I_soma += current;
            break;

        case COMPARTMENT_DENDRITE:
            state->I_dend += current;
            break;

        case COMPARTMENT_AUTO:
        default:
            // Default: split 70% soma, 30% dendrite
            state->I_soma += 0.7F * current;
            state->I_dend += 0.3F * current;
            break;
    }
}
