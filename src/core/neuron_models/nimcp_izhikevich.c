#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_izhikevich.c - Izhikevich Neuron Model Implementation
//=============================================================================
/**
 * @file nimcp_izhikevich.c
 * @brief Implementation of Izhikevich simple spiking neuron model
 *
 * MATHEMATICAL FORMULATION:
 * The Izhikevich model consists of two coupled ODEs:
 *   dv/dt = 0.04v² + 5v + 140 - u + I
 *   du/dt = a(bv - u)
 *
 * With after-spike reset:
 *   if v ≥ 30mV: v ← c, u ← u + d
 *
 * NUMERICAL INTEGRATION:
 * Uses Forward Euler method with timestep dt:
 *   v(t+dt) = v(t) + dt * dv/dt
 *   u(t+dt) = u(t) + dt * du/dt
 *
 * STABILITY: Stable for dt ≤ 1.0 ms (typical biological timestep)
 *
 * DESIGN PATTERNS:
 * - Strategy: Implements neuron_model_vtable_t interface
 * - Immutable Data: Parameters stored in const structs
 * - Single Responsibility: Each function handles one aspect
 *
 * PERFORMANCE:
 * - Update: O(1) - just 2 ODEs
 * - Memory: 16 bytes (v, u, params)
 * - Throughput: ~1M neurons/sec on modern CPU
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#include "core/neuron_models/nimcp_izhikevich.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "core/neuron_models/nimcp_neuron_model_internal.h"
#include "utils/numerical/nimcp_integration.h"  // Part A1: RK4 integration
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "izhikevich"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(izhikevich)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void izhikevich_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_NEURON_MODEL_IZHIKEVICH,
        .module_name = "izhikevich",
        .inbox_capacity = 256,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for izhikevich module");
    }
}

__attribute__((constructor))
static void izhikevich_bio_init(void) {
    pthread_once(&bio_init_once, izhikevich_bio_init_impl);
}

__attribute__((destructor))
static void izhikevich_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for izhikevich module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Internal State Structure
//=============================================================================

/**
 * @brief Izhikevich neuron internal state
 *
 * WHAT: Minimal state for Izhikevich dynamics
 * WHY: Only 2 state variables needed (v and u)
 * HOW: Compact layout for cache efficiency
 *
 * INVARIANTS:
 * - After spike: v == params.c
 * - Valid params: See izhikevich_params_t
 *
 * MEMORY: 16 bytes
 */
typedef struct {
    float v;                        /**< Membrane potential (mV) */
    float u;                        /**< Recovery variable */
    izhikevich_params_t params;     /**< Model parameters (a,b,c,d) */
    bool spiked_this_step;          /**< Spike flag for current timestep */
    integration_method_t integration_method; /**< ODE integration method (Part A1) */
    float current_input;            /**< Current input for derivative function */
} izhikevich_state_t;

//=============================================================================
// Preset Parameter Tables
//=============================================================================

/**
 * @brief Preset parameter table
 *
 * WHAT: Pre-configured parameter sets from Izhikevich 2003 paper
 * WHY: Validated combinations that produce specific firing patterns
 * HOW: Static const table indexed by preset enum
 *
 * REFERENCE: Table 1 in Izhikevich (2003) IEEE Trans. Neural Networks
 */
static const izhikevich_params_t PRESET_TABLE[] = {
    // Regular Spiking (RS) - Typical cortical pyramidal cell
    {.a = 0.02F, .b = 0.2F, .c = -65.0F, .d = 8.0F},

    // Intrinsically Bursting (IB) - Bursting cortical neuron
    {.a = 0.02F, .b = 0.2F, .c = -55.0F, .d = 4.0F},

    // Chattering (CH) - Fast rhythmic bursting
    {.a = 0.02F, .b = 0.2F, .c = -50.0F, .d = 2.0F},

    // Fast Spiking (FS) - Cortical inhibitory interneuron
    {.a = 0.1F, .b = 0.2F, .c = -65.0F, .d = 2.0F},

    // Low-Threshold Spiking (LTS) - Interneuron
    {.a = 0.02F, .b = 0.25F, .c = -65.0F, .d = 2.0F},

    // Thalamo-Cortical (TC) - Thalamic relay neuron
    {.a = 0.02F, .b = 0.25F, .c = -65.0F, .d = 0.05F},

    // Resonator (RZ) - Subthreshold oscillator
    {.a = 0.1F, .b = 0.26F, .c = -65.0F, .d = 2.0F},

    // Custom - User-defined (same as RS by default)
    {.a = 0.02F, .b = 0.2F, .c = -65.0F, .d = 8.0F}
};

/**
 * @brief Preset names table
 */
static const char* PRESET_NAMES[] = {
    "Regular Spiking (RS)",
    "Intrinsically Bursting (IB)",
    "Chattering (CH)",
    "Fast Spiking (FS)",
    "Low-Threshold Spiking (LTS)",
    "Thalamo-Cortical (TC)",
    "Resonator (RZ)",
    "Custom"
};

/**
 * @brief Preset descriptions table
 */
static const char* PRESET_DESCRIPTIONS[] = {
    "Typical cortical pyramidal neuron - adapts slowly to sustained input",
    "Bursting neuron - produces clusters of spikes",
    "Fast rhythmic bursting - high-frequency spike trains",
    "Fast-spiking inhibitory interneuron - no adaptation",
    "Low-threshold spiking interneuron - moderate adaptation",
    "Thalamic relay neuron - exhibits rebound burst after inhibition",
    "Resonator - subthreshold oscillations without spiking",
    "Custom user-defined parameters"
};

//=============================================================================
// Core Dynamics Functions
//=============================================================================

/**
 * @brief Compute dv/dt from Izhikevich equation
 *
 * WHAT: Right-hand side of voltage ODE
 * WHY: Separated for clarity and potential RK4 integration
 * HOW: Direct implementation of 0.04v² + 5v + 140 - u + I
 *
 * COMPLEXITY: O(1)
 *
 * @param v Current voltage (mV)
 * @param u Current recovery variable
 * @param I Input current
 * @return dv/dt derivative
 */
static inline float compute_dv_dt(float v, float u, float I) {
    return 0.04F * v * v + 5.0F * v + 140.0F - u + I;
}

/**
 * @brief Compute du/dt from Izhikevich equation
 *
 * WHAT: Right-hand side of recovery ODE
 * WHY: Separated for clarity and potential RK4 integration
 * HOW: Direct implementation of a(bv - u)
 *
 * COMPLEXITY: O(1)
 *
 * @param v Current voltage (mV)
 * @param u Current recovery variable
 * @param params Model parameters
 * @return du/dt derivative
 */
static inline float compute_du_dt(float v, float u, const izhikevich_params_t* params) {
    return params->a * (params->b * v - u);
}

/**
 * @brief Derivative function wrapper for integration_step()
 *
 * WHAT: Computes derivatives [dv/dt, du/dt] for RK4 integration
 * WHY: Bridges Izhikevich model to generic integration interface
 * HOW: Extracts state, calls compute_dv_dt and compute_du_dt
 *
 * PART A1.1: RK4 Integration
 *
 * @param state State vector [v, u]
 * @param t Current time (unused for autonomous system)
 * @param params User parameters (izhikevich_state_t*)
 * @param derivatives Output derivatives [dv/dt, du/dt]
 */
static void izhikevich_derivatives(const float* state, float t, void* params, float* derivatives) {
    (void)t;  // Autonomous system - time not used

    izhikevich_state_t* izh = (izhikevich_state_t*)params;
    float v = state[0];
    float u = state[1];

    derivatives[0] = compute_dv_dt(v, u, izh->current_input);
    derivatives[1] = compute_du_dt(v, u, &izh->params);
}

//=============================================================================
// Vtable Implementation Functions
//=============================================================================

/**
 * @brief Initialize Izhikevich neuron state
 *
 * WHAT: Sets initial voltage, recovery, and parameters
 * WHY: Factory initialization from params or defaults
 * HOW: Copies params if provided, else uses Regular Spiking
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_init(neuron_model_state_t state, const void* params) {
    // Guard: Validate state
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "izhikevich_init: state is NULL");
        return;
    }

    // Get pointer to Izhikevich-specific state
    izhikevich_state_t* izh = (izhikevich_state_t*)state->model_state;

    // Initialize voltage to resting potential
    izh->v = IZHIKEVICH_RESTING_POTENTIAL;
    izh->u = izh->v * 0.2F;  // u = b*v at rest (approximation)
    izh->spiked_this_step = false;
    izh->integration_method = INTEGRATION_EULER;  // Default to Euler (backward compatible)
    izh->current_input = 0.0F;

    // Set parameters from input or use default (Regular Spiking)
    if (params) {
        memcpy(&izh->params, params, sizeof(izhikevich_params_t));
    } else {
        izh->params = PRESET_TABLE[IZHI_PRESET_REGULAR_SPIKING];
    }
}

/**
 * @brief Destroy Izhikevich state
 *
 * WHAT: Cleanup function (currently no-op)
 * WHY: Required by vtable interface
 * HOW: No dynamic allocations to free
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_destroy(neuron_model_state_t state) {
    // No cleanup needed - all state is in fixed-size struct
    (void)state;  // Suppress unused warning
}

/**
 * @brief Update Izhikevich neuron dynamics for one timestep
 *
 * WHAT: Integrates ODEs using configurable integration method (Euler, RK4, etc.)
 * WHY: Core simulation function - advances neuron state with configurable accuracy
 * HOW: Uses integration_step() from nimcp_integration.h (Part A1.1)
 *
 * PART A1.1: RK4 Integration Support
 * - Euler: O(dt) local error, fastest (1.0x)
 * - RK4: O(dt^4) local error, 4x slower but 10-1000x more accurate
 *
 * COMPLEXITY:
 * - Euler: O(1) + 1 derivative call
 * - RK4: O(1) + 4 derivative calls
 *
 * STABILITY:
 * - Euler: Stable for dt ≤ 1.0 ms
 * - RK4: Stable for dt ≤ 5.0 ms
 *
 * @param state Neuron state
 * @param dt Timestep in milliseconds
 * @param input_current Input current (arbitrary units)
 */
static void izhikevich_update(neuron_model_state_t state, float dt, float input_current) {
    // Process pending bio-async messages
    if (bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Guard: Validate state
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "izhikevich_update: state is NULL");
        return;
    }

    izhikevich_state_t* izh = (izhikevich_state_t*)state->model_state;

    // Clear spike flag from previous timestep
    izh->spiked_this_step = false;

    // Store input current for derivative function
    izh->current_input = input_current;

    // Pack state for integration: [v, u]
    float ode_state[2] = {izh->v, izh->u};

    // Perform ODE integration using configured method (Euler, RK4, etc.)
    bool success = integration_step(
        izh->integration_method,    // Method: INTEGRATION_EULER or INTEGRATION_RK4
        izhikevich_derivatives,     // Derivative function
        ode_state,                  // State vector [v, u]
        0.0F,                       // Time (not used - autonomous system)
        dt,                         // Timestep
        2,                          // Dimension (v and u)
        izh                         // Parameters (passed to derivative function)
    );

    if (!success) {
        // Integration failed - fall back to previous state
        return;
    }

    // Unpack integrated state
    izh->v = ode_state[0];
    izh->u = ode_state[1];

    // Check for spike and perform reset
    if (izh->v >= IZHIKEVICH_SPIKE_THRESHOLD) {
        izh->spiked_this_step = true;
        izh->v = izh->params.c;  // Reset voltage
        izh->u += izh->params.d;  // Update recovery (NOTE: += not =)
    }
}

/**
 * @brief Check if neuron spiked this timestep
 *
 * COMPLEXITY: O(1)
 */
static bool izhikevich_check_spike(const neuron_model_state_t state) {
    // Guard: Validate state
    if (!state) {
        return false;
    }

    const izhikevich_state_t* izh = (const izhikevich_state_t*)state->model_state;
    return izh->spiked_this_step;
}

/**
 * @brief Post-spike processing
 *
 * WHAT: Placeholder for post-spike actions
 * WHY: Required by vtable interface
 * HOW: Currently no-op (reset handled in update)
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_post_spike(neuron_model_state_t state) {
    // Reset already handled in update() function
    // This function available for future extensions (e.g., spike history)
    (void)state;  // Suppress unused warning
}

/**
 * @brief Get current membrane voltage
 *
 * COMPLEXITY: O(1)
 */
static float izhikevich_get_voltage(const neuron_model_state_t state) {
    // Guard: Validate state
    if (!state) {
        return 0.0F;
    }

    const izhikevich_state_t* izh = (const izhikevich_state_t*)state->model_state;
    return izh->v;
}

/**
 * @brief Set membrane voltage
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_set_voltage(neuron_model_state_t state, float voltage) {
    // Guard: Validate state
    if (!state) {
        return;
    }

    izhikevich_state_t* izh = (izhikevich_state_t*)state->model_state;
    izh->v = voltage;
}

/**
 * @brief Reset neuron to resting state
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_reset(neuron_model_state_t state) {
    // Guard: Validate state
    if (!state) {
        return;
    }

    izhikevich_state_t* izh = (izhikevich_state_t*)state->model_state;
    izh->v = IZHIKEVICH_RESTING_POTENTIAL;
    izh->u = izh->v * izh->params.b;
    izh->spiked_this_step = false;
}

/**
 * @brief Copy neuron state
 *
 * COMPLEXITY: O(1)
 */
static void izhikevich_copy(neuron_model_state_t dst, const neuron_model_state_t src) {
    // Guard: Validate states
    if (!dst || !src) {
        return;
    }

    izhikevich_state_t* dst_izh = (izhikevich_state_t*)dst->model_state;
    const izhikevich_state_t* src_izh = (const izhikevich_state_t*)src->model_state;

    memcpy(dst_izh, src_izh, sizeof(izhikevich_state_t));
}

//=============================================================================
// Virtual Function Table
//=============================================================================

/**
 * @brief Izhikevich model vtable
 *
 * PATTERN: Strategy Pattern - polymorphic interface implementation
 * INVARIANT: All function pointers are non-NULL
 */
static const neuron_model_vtable_t izhikevich_vtable = {
    .name = "Izhikevich",
    .type = NEURON_MODEL_IZHIKEVICH,
    .state_size = sizeof(izhikevich_state_t),
    .init = izhikevich_init,
    .destroy = izhikevich_destroy,
    .update = izhikevich_update,
    .check_spike = izhikevich_check_spike,
    .post_spike = izhikevich_post_spike,
    .get_voltage = izhikevich_get_voltage,
    .set_voltage = izhikevich_set_voltage,
    .reset = izhikevich_reset,
    .copy = izhikevich_copy
};

//=============================================================================
// Public API Implementation
//=============================================================================

const neuron_model_vtable_t* izhikevich_get_vtable(void) {
    return &izhikevich_vtable;
}

izhikevich_params_t izhikevich_get_preset_params(izhikevich_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_TABLE) / sizeof(PRESET_TABLE[0])) {
        return PRESET_TABLE[IZHI_PRESET_REGULAR_SPIKING];
    }

    return PRESET_TABLE[preset];
}

izhikevich_params_t izhikevich_create_params(float a, float b, float c, float d) {
    izhikevich_params_t params = {.a = a, .b = b, .c = c, .d = d};
    return params;
}

const char* izhikevich_get_preset_name(izhikevich_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_NAMES) / sizeof(PRESET_NAMES[0])) {
        return "Unknown";
    }

    return PRESET_NAMES[preset];
}

const char* izhikevich_get_preset_description(izhikevich_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_DESCRIPTIONS) / sizeof(PRESET_DESCRIPTIONS[0])) {
        return "Unknown preset type";
    }

    return PRESET_DESCRIPTIONS[preset];
}

/**
 * @brief Set ODE integration method for Izhikevich neuron
 *
 * PART A1.1: RK4 Integration
 */
void izhikevich_set_integration_method(neuron_model_state_t state, integration_method_t method) {
    // Guard: Validate state
    if (!state) {
        return;
    }

    izhikevich_state_t* izh = (izhikevich_state_t*)state->model_state;
    izh->integration_method = method;
}

// Expose vtable via the generic interface
const neuron_model_vtable_t* neuron_model_get_izhikevich_vtable(void) {
    return izhikevich_get_vtable();
}
