#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neuron_model.c - Neuron Model Plugin Interface Implementation
//=============================================================================
/**
 * @file nimcp_neuron_model.c
 * @brief Implementation of generic neuron model interface
 *
 * ARCHITECTURAL OVERVIEW:
 * This file implements the plugin framework for neuron models using:
 * - Strategy Pattern: Dispatch via virtual function tables
 * - Handle/Body: Opaque state structure with vtable pointer
 * - Factory Pattern: Model-agnostic creation interface
 *
 * DESIGN PATTERNS:
 * - Strategy: Each model implements same interface differently
 * - Factory: neuron_model_create() dispatches to model constructors
 * - Handle/Body: State struct hidden behind opaque pointer
 *
 * COMPLEXITY:
 * - All operations: O(1) - simple function pointer dispatch
 * - Memory: sizeof(neuron_model_state_struct) + model-specific state
 *
 * DESIGN PRINCIPLES:
 * - No nested ifs: Guard clauses only
 * - Single responsibility: Each function does one thing
 * - Functions <50 lines: Keep interface layer thin
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#include "core/neuron_models/nimcp_neuron_model.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "core/neuron_models/nimcp_neuron_model_internal.h"
#include "core/neuron_models/nimcp_izhikevich.h"  // For izhikevich_set_integration_method
#include "core/neuron_models/nimcp_two_compartment.h"  // For two_compartment functions (Part A3.1)
#include "utils/numerical/nimcp_integration.h"  // For integration_method_t
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "neuron_model"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuron_model)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void neuron_model_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_NEURON_MODEL,
        .module_name = "neuron_model",
        .inbox_capacity = 256,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for neuron_model module");
    }
}

__attribute__((constructor))
static void neuron_model_bio_init(void) {
    pthread_once(&bio_init_once, neuron_model_bio_init_impl);
}

__attribute__((destructor))
static void neuron_model_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for neuron_model module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Factory Functions
//=============================================================================

/**
 * @brief Create neuron model state with vtable
 *
 * WHAT: Allocates state structure and initializes model
 * WHY: Single allocation improves cache locality
 * HOW: Allocates [vtable_ptr + model_state] in one block
 *
 * PATTERN: Factory Method
 * COMPLEXITY: O(1) + model init complexity
 * MEMORY: sizeof(struct) + vtable->state_size
 *
 * @param vtable Virtual function table
 * @param params Model-specific parameters (NULL = use defaults)
 * @return Initialized state or NULL on failure
 */
neuron_model_state_t neuron_model_create(const neuron_model_vtable_t* vtable, const void* params) {
    // Guard: Validate vtable
    NIMCP_API_CHECK_NULL_RET_NULL(vtable, "neuron_model_create: vtable is NULL");
    NIMCP_API_CHECK_NULL_RET_NULL(vtable->init, "neuron_model_create: vtable->init is NULL");
    if (vtable->state_size == 0) {
        LOG_ERROR("neuron_model_create: vtable->state_size is 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vtable->state_size is 0");
        return NULL;
    }

    // Allocate contiguous block for state + model data
    const size_t total_size = sizeof(struct neuron_model_state_struct) + vtable->state_size;
    neuron_model_state_t state = (neuron_model_state_t)nimcp_calloc(1, total_size);

    // Guard: Check allocation
    NIMCP_API_CHECK_ALLOC_SIZE(state, total_size, "neuron_model_create: failed to allocate state");

    // Initialize vtable pointer
    state->vtable = vtable;

    // Call model-specific initialization
    vtable->init(state, params);

    return state;
}

/**
 * @brief Destroy neuron model state
 *
 * WHAT: Calls model destructor and frees memory
 * WHY: Proper cleanup prevents memory leaks
 * HOW: Calls vtable->destroy() if present, then nimcp_free()
 *
 * COMPLEXITY: O(1) + model destroy complexity
 *
 * @param state State to destroy (NULL-safe)
 */
void neuron_model_destroy(neuron_model_state_t state) {
    // Guard: NULL check
    if (!state) {
        return;
    }

    // Call model-specific cleanup if provided
    if (state->vtable->destroy) {
        state->vtable->destroy(state);
    }

    // Free the state block
    nimcp_free(state);
}

//=============================================================================
// Dynamics Functions - Dispatch to Model Implementation
//=============================================================================

/**
 * @brief Update neuron model dynamics
 *
 * PATTERN: Strategy dispatch
 * COMPLEXITY: O(1) dispatch + model update complexity
 */
void neuron_model_update(neuron_model_state_t state, float dt, float input_current) {
    /* Bio-async inbox draining is NOT done here: it used to run on every
     * per-neuron update (and the Izhikevich update drained a second inbox on
     * top), a per-neuron-per-step cost. Call neuron_model_process_bio_inbox()
     * once per simulation step instead. */

    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->update) {
        return;
    }

    state->vtable->update(state, dt, input_current);
}

/**
 * @brief Drain all neuron-model bio-async inboxes (explicit, once per step)
 */
uint32_t neuron_model_process_bio_inbox(uint32_t max_messages) {
    uint32_t processed = 0;
    bio_module_context_t ctx = bio_ctx;
    if (ctx) {
        processed += bio_router_process_inbox(ctx, max_messages);
    }
    processed += izhikevich_process_bio_inbox(max_messages);
    processed += two_compartment_process_bio_inbox(max_messages);
    return processed;
}

/**
 * @brief Check if neuron spiked
 *
 * PATTERN: Strategy dispatch
 * COMPLEXITY: O(1)
 */
bool neuron_model_check_spike(const neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->check_spike) {
        return false;
    }

    return state->vtable->check_spike(state);
}

/**
 * @brief Execute post-spike reset
 *
 * PATTERN: Strategy dispatch
 * COMPLEXITY: O(1)
 */
void neuron_model_post_spike(neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->post_spike) {
        return;
    }

    state->vtable->post_spike(state);
}

//=============================================================================
// State Access Functions
//=============================================================================

/**
 * @brief Get membrane voltage
 *
 * COMPLEXITY: O(1)
 */
float neuron_model_get_voltage(const neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->get_voltage) {
        return 0.0F;
    }

    return state->vtable->get_voltage(state);
}

/**
 * @brief Set membrane voltage
 *
 * COMPLEXITY: O(1)
 */
void neuron_model_set_voltage(neuron_model_state_t state, float voltage) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->set_voltage) {
        return;
    }

    state->vtable->set_voltage(state, voltage);
}

/**
 * @brief Reset neuron to resting state
 *
 * COMPLEXITY: O(1)
 */
void neuron_model_reset(neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->reset) {
        return;
    }

    state->vtable->reset(state);
}

//=============================================================================
// Introspection Functions
//=============================================================================

/**
 * @brief Get model name
 *
 * COMPLEXITY: O(1)
 */
const char* neuron_model_get_name(const neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->name) {
        return "unknown";
    }

    return state->vtable->name;
}

/**
 * @brief Get model type
 *
 * COMPLEXITY: O(1)
 */
neuron_model_type_t neuron_model_get_type(const neuron_model_state_t state) {
    // Guard: Validate state and vtable
    if (!state || !state->vtable) {
        return NEURON_MODEL_LIF;  // Default fallback
    }

    return state->vtable->type;
}

//=============================================================================
// Integration Method Configuration (Part A1.1: RK4 Support)
//=============================================================================

/**
 * @brief Set ODE integration method for neuron model
 *
 * PART A1.1: RK4 Integration Support
 * Maps ode_integration_method_t to integration_method_t and dispatches to model-specific setters
 */
void neuron_model_set_integration_method(neuron_model_state_t state, ode_integration_method_t method) {
    // Guard: Validate state
    if (!state || !state->vtable) {
        return;
    }

    // Map ode_integration_method_t to integration_method_t
    integration_method_t integration_method;
    switch (method) {
        case ODE_EULER:
            integration_method = INTEGRATION_EULER;
            break;
        case ODE_RK4:
            integration_method = INTEGRATION_RK4;
            break;
        case ODE_RK2:
            // RK2 not supported in integration system - fall back to Euler
            integration_method = INTEGRATION_EULER;
            break;
        case ODE_ADAPTIVE:
            // Part A1.2: Adaptive RK45 (Dormand-Prince)
            integration_method = INTEGRATION_ADAPTIVE;
            break;
        default:
            integration_method = INTEGRATION_EULER;
            break;
    }

    // Dispatch to model-specific setter based on type
    switch (state->vtable->type) {
        case NEURON_MODEL_IZHIKEVICH:
            izhikevich_set_integration_method(state, integration_method);
            break;
        case NEURON_MODEL_TWO_COMPARTMENT:
            // Two-compartment model has integration method set at creation time
            // Integration method is baked into the params structure
            // No runtime setter needed - already using RK4
            break;
        // Add other model types here as they gain RK4 support
        case NEURON_MODEL_LIF:
        case NEURON_MODEL_ADEX:
        case NEURON_MODEL_HODGKIN_HUXLEY:
            // These models don't support configurable integration yet
            // They will continue using their built-in methods
            break;
    }
}


//=============================================================================
// Leaky Integrate-and-Fire (LIF) Model
//=============================================================================
/*
 * tau_m dV/dt = -(V - v_rest) + R*I, spike at V >= v_thresh, reset to v_reset,
 * clamp at v_reset for t_ref ms. Integrated with the exact exponential
 * solution for constant I over the step (stable for any dt). The reset happens
 * inside update() (like the Izhikevich model), so post_spike() is a no-op and
 * check_spike() reports the spike of the most recent update().
 */

typedef struct {
    float v;                    /**< Membrane potential (mV) */
    float refractory_remaining; /**< Absolute refractory time left (ms) */
    bool spiked_this_step;      /**< Spike flag for the last update */
    lif_model_params_t params;  /**< Model parameters */
} lif_state_t;

lif_model_params_t lif_model_default_params(void) {
    lif_model_params_t p = {
        .tau_m = 10.0F,
        .v_rest = -65.0F,
        .v_reset = -65.0F,
        .v_thresh = -50.0F,
        .t_ref = 2.0F,
        .resistance = 10.0F   /* == tau_m: dV/dt gets +I, as in Izhikevich */
    };
    return p;
}

lif_model_params_t lif_model_create_params(float tau_m, float v_rest, float v_reset,
                                           float v_thresh, float t_ref, float resistance) {
    lif_model_params_t p = {
        .tau_m = tau_m,
        .v_rest = v_rest,
        .v_reset = v_reset,
        .v_thresh = v_thresh,
        .t_ref = t_ref,
        .resistance = resistance
    };
    return p;
}

static bool lif_params_valid(const lif_model_params_t* p) {
    if (!isfinite(p->tau_m) || !isfinite(p->v_rest) || !isfinite(p->v_reset) ||
        !isfinite(p->v_thresh) || !isfinite(p->t_ref) || !isfinite(p->resistance)) {
        return false;
    }
    if (p->tau_m <= 0.0F || p->resistance <= 0.0F || p->t_ref < 0.0F) {
        return false;
    }
    return p->v_reset < p->v_thresh;
}

static void lif_init(neuron_model_state_t state, const void* params) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lif_init: state is NULL");
        return;
    }
    lif_state_t* lif = (lif_state_t*)state->model_state;
    lif->params = lif_model_default_params();
    if (params) {
        lif_model_params_t p;
        memcpy(&p, params, sizeof(p));
        if (lif_params_valid(&p)) {
            lif->params = p;
        } else {
            LOG_WARN("lif_init: invalid LIF params (tau_m=%.3f R=%.3f t_ref=%.3f "
                     "v_reset=%.2f v_thresh=%.2f) - using defaults",
                     (double)p.tau_m, (double)p.resistance, (double)p.t_ref,
                     (double)p.v_reset, (double)p.v_thresh);
        }
    }
    lif->v = lif->params.v_rest;
    lif->refractory_remaining = 0.0F;
    lif->spiked_this_step = false;
}

static void lif_destroy(neuron_model_state_t state) {
    (void)state;  /* fixed-size state, nothing to free */
}

static void lif_update(neuron_model_state_t state, float dt, float input_current) {
    if (!state) {
        return;
    }
    lif_state_t* lif = (lif_state_t*)state->model_state;
    const lif_model_params_t* p = &lif->params;

    lif->spiked_this_step = false;
    if (!(dt > 0.0F) || !isfinite(dt)) {
        return;
    }
    if (!isfinite(input_current)) {
        input_current = 0.0F;
    }

    float t = dt;
    if (lif->refractory_remaining > 0.0F) {
        if (lif->refractory_remaining >= t) {
            lif->refractory_remaining -= t;
            lif->v = p->v_reset;
            return;
        }
        /* refractory ends inside this step: integrate only the remainder */
        t -= lif->refractory_remaining;
        lif->refractory_remaining = 0.0F;
        lif->v = p->v_reset;
    }

    const float v_inf = p->v_rest + p->resistance * input_current;
    lif->v = v_inf + (lif->v - v_inf) * expf(-t / p->tau_m);

    if (lif->v >= p->v_thresh) {
        lif->spiked_this_step = true;
        lif->v = p->v_reset;
        lif->refractory_remaining = p->t_ref;
    }
}

static bool lif_check_spike(const neuron_model_state_t state) {
    if (!state) {
        return false;
    }
    return ((const lif_state_t*)state->model_state)->spiked_this_step;
}

static void lif_post_spike(neuron_model_state_t state) {
    (void)state;  /* reset + refractory are applied in lif_update() */
}

static float lif_get_voltage(const neuron_model_state_t state) {
    if (!state) {
        return 0.0F;
    }
    return ((const lif_state_t*)state->model_state)->v;
}

static void lif_set_voltage(neuron_model_state_t state, float voltage) {
    if (!state || !isfinite(voltage)) {
        return;
    }
    ((lif_state_t*)state->model_state)->v = voltage;
}

static void lif_reset(neuron_model_state_t state) {
    if (!state) {
        return;
    }
    lif_state_t* lif = (lif_state_t*)state->model_state;
    lif->v = lif->params.v_rest;
    lif->refractory_remaining = 0.0F;
    lif->spiked_this_step = false;
}

static void lif_copy(neuron_model_state_t dst, const neuron_model_state_t src) {
    if (!dst || !src) {
        return;
    }
    memcpy(dst->model_state, src->model_state, sizeof(lif_state_t));
}

static const neuron_model_vtable_t lif_vtable = {
    .name = "LIF",
    .type = NEURON_MODEL_LIF,
    .state_size = sizeof(lif_state_t),
    .init = lif_init,
    .destroy = lif_destroy,
    .update = lif_update,
    .check_spike = lif_check_spike,
    .post_spike = lif_post_spike,
    .get_voltage = lif_get_voltage,
    .set_voltage = lif_set_voltage,
    .reset = lif_reset,
    .copy = lif_copy
};

const neuron_model_vtable_t* neuron_model_get_lif_vtable(void) {
    return &lif_vtable;
}
