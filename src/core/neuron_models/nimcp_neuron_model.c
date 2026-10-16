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
    // Process pending bio-async messages
    if (bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Guard: Validate state and vtable
    if (!state || !state->vtable || !state->vtable->update) {
        return;
    }

    state->vtable->update(state, dt, input_current);
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
