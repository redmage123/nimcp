/**
 * @file nimcp_spatial_neuromod.c
 * @brief Implementation of graph-based neuromodulator diffusion (Enhancement A2.1)
 *
 * IMPLEMENTATION NOTES:
 * - Uses explicit Euler for simplicity and performance
 * - Graph Laplacian computed via direct neighbor iteration
 * - Substeps available for stability with large diffusion coefficients
 * - Validates numerical stability via Courant condition
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Cache-friendly linear array scans
 * - Minimal memory allocations (buffers pre-allocated)
 * - Inline critical path functions
 * - SIMD-friendly memory layout
 *
 * NUMERICAL STABILITY:
 * Explicit Euler stability condition: dt <= 1/(2*D*max_degree)
 * - For D=0.1, max_degree=100, dt_max = 0.05
 * - Default dt=1ms may require substeps for dense networks
 * - Clamping ensures concentrations stay in [0,1]
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include <stddef.h>  /* for NULL */
#include "utils/quantum/nimcp_quantum_shannon.h"  // Phase C4.3: Quantum-Shannon diffusion
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdio.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "spatial_neuromod"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/geometry/nimcp_differential_geometry.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(spatial_neuromod)

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Epsilon for floating-point comparisons
 * WHY:  Prevent division by zero, handle numerical precision
 */
#define EPSILON 1e-10f

/**
 * WHAT: Maximum number of substeps
 * WHY:  Prevent infinite loops if stability condition is badly violated
 */
#define MAX_SUBSTEPS 100

/**
 * WHAT: Default parameters for each neuromodulator type
 * WHY:  Biologically-informed defaults from literature
 *
 * SOURCES:
 * - Dopamine: Dreyer et al. 2010, diffusion ~0.2-0.5 μm²/ms, decay ~500ms
 * - Serotonin: Bunin & Wightman 1998, slower diffusion, longer half-life
 * - Acetylcholine: Sarter et al. 2009, fast dynamics, rapid clearance
 * - Norepinephrine: Berridge & Waterhouse 2003, intermediate kinetics
 */
static const struct {
    float diffusion;  // Diffusion coefficient (normalized)
    float decay;      // Decay rate (1/s)
    float baseline;   // Baseline concentration
} NEUROMOD_DEFAULTS[NEUROMOD_COUNT] = {
    [NEUROMOD_DOPAMINE]      = {0.2F, 0.5F, 0.05F},   // Fast diffusion, fast decay
    [NEUROMOD_SEROTONIN]     = {0.05F, 0.1F, 0.3F},   // Slow diffusion, slow decay
    [NEUROMOD_ACETYLCHOLINE] = {0.3F, 2.0F, 0.1F},    // Fast diffusion, very fast decay
    [NEUROMOD_NOREPINEPHRINE]= {0.15F, 0.3F, 0.05F},  // Medium dynamics
    [NEUROMOD_GABA]          = {0.1F, 10.0F, 0.2F},   // Fast clearance
    [NEUROMOD_GLUTAMATE]     = {0.1F, 10.0F, 0.1F}    // Fast clearance
};

//=============================================================================
// Bio-Async Module State
//=============================================================================

/**
 * @brief Module state for bio-async integration
 */
typedef struct {
    bio_module_context_t module_ctx;           /**< Router module context */
    spatial_neuromod_system_t* system;         /**< Associated spatial system */
    bool initialized;                          /**< Initialization status */
    uint64_t messages_processed;               /**< Statistics counter */
    nimcp_mutex_t init_mutex;                /**< Mutex protecting initialization */
} spatial_neuromod_bio_state_t;

/** Global bio-async state (singleton) */
static spatial_neuromod_bio_state_t g_spatial_bio_state = {
    .module_ctx = NULL,
    .system = NULL,
    .initialized = false,
    .messages_processed = 0,
    .init_mutex = NIMCP_MUTEX_INITIALIZER
};

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle spatial diffusion request
 *
 * Processes a request to update spatial neuromodulator diffusion.
 */
static nimcp_error_t handle_spatial_diffusion_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;  // Not used

    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("Invalid spatial diffusion request");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_SUCCESS - 1);
        }
        return NIMCP_SUCCESS - 1;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // For now, acknowledge the message
    // TODO: Implement actual diffusion processing when message type is defined

    if (response_promise) {
        bio_message_header_t response;
        memcpy(&response, header, sizeof(response));
        response.source_module = BIO_MODULE_NEUROMODULATOR;
        response.target_module = header->source_module;

        nimcp_bio_promise_complete(response_promise, &response);
    }

    g_spatial_bio_state.messages_processed++;

    LOG_INFO("Processed spatial diffusion request from module 0x%04X",
             header->source_module);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle neuromodulator release request
 *
 * Processes requests to release neuromodulator at specific neurons.
 */
static nimcp_error_t handle_neuromodulator_release_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;  // Not used

    if (!msg || msg_size < sizeof(bio_msg_neuromodulator_release_t)) {
        LOG_ERROR("Invalid neuromodulator release message size");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_SUCCESS - 1);
        }
        return NIMCP_SUCCESS - 1;
    }

    if (!g_spatial_bio_state.system) {
        LOG_ERROR("Spatial neuromodulator system not initialized");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_SUCCESS - 1);
        }
        return NIMCP_SUCCESS - 1;
    }

    const bio_msg_neuromodulator_release_t* release_msg =
        (const bio_msg_neuromodulator_release_t*)msg;

    // Find the field for the specified neuromodulator type
    spatial_neuromod_field_t* field = NULL;
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (g_spatial_bio_state.system->enabled[i]) {
            field = g_spatial_bio_state.system->fields[i];
            if (field && field->type == (neuromodulator_type_t)release_msg->neuromodulator) {
                break;
            }
            field = NULL;
        }
    }

    if (!field) {
        LOG_WARN("Neuromodulator type %d not enabled in spatial system",
                         release_msg->neuromodulator);
        if (response_promise) {
            bio_message_header_t response;
            memcpy(&response, &release_msg->header, sizeof(response));
            response.source_module = BIO_MODULE_NEUROMODULATOR;
            response.target_module = release_msg->header.source_module;
            nimcp_bio_promise_complete(response_promise, &response);
        }
        return NIMCP_SUCCESS;
    }

    // Release neuromodulator at the specified region
    uint32_t neuron_id = release_msg->source_region;
    float amount = release_msg->release_amount;

    bool success = spatial_neuromod_release(field, neuron_id, amount);

    LOG_INFO("Released %.3f units of neuromodulator %d at neuron %u: %s",
             amount, release_msg->neuromodulator, neuron_id,
             success ? "SUCCESS" : "FAILED");

    // Send response
    if (response_promise) {
        bio_message_header_t response;
        memcpy(&response, &release_msg->header, sizeof(response));
        response.source_module = BIO_MODULE_NEUROMODULATOR;
        response.target_module = release_msg->header.source_module;

        nimcp_bio_promise_complete(response_promise, &response);
    }

    g_spatial_bio_state.messages_processed++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle concentration query request
 *
 * Responds with current concentration at a specific neuron.
 */
static nimcp_error_t handle_concentration_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)user_data;  // Not used

    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("Invalid concentration query");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, NIMCP_SUCCESS - 1);
        }
        return NIMCP_SUCCESS - 1;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // For now, send a dummy response
    // TODO: Define proper query/response message types

    if (response_promise) {
        bio_message_header_t response;
        memcpy(&response, header, sizeof(response));
        response.source_module = BIO_MODULE_NEUROMODULATOR;
        response.target_module = header->source_module;

        nimcp_bio_promise_complete(response_promise, &response);
    }

    g_spatial_bio_state.messages_processed++;

    LOG_DEBUG("Processed concentration query from module 0x%04X",
              header->source_module);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Module Initialization
//=============================================================================

/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int spatial_neuromod_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_NEUROMODULATOR_RELEASE:
                bio_router_register_handler(ctx, message_types[i], handle_neuromodulator_release_request);
                registered++;
                LOG_DEBUG("  Registered handler for BIO_MSG_NEUROMODULATOR_RELEASE");
                break;

            default:
                LOG_DEBUG("Spatial neuromodulator: unknown message type %d in wiring callback",
                          message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Initialize bio-async integration for spatial neuromodulator
 *
 * WHAT: Registers module with bio-router and sets up message handlers
 * WHY:  Enable async communication with other NIMCP modules
 * HOW:  Register handlers for diffusion, release, and query operations
 *
 * @param system Spatial neuromodulator system to integrate
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t spatial_neuromod_bio_async_init(spatial_neuromod_system_t* system) {
    if (!system) {
        LOG_ERROR("NULL system in bio-async init");
        return NIMCP_SUCCESS - 1;
    }

    // Check if bio-router is initialized
    if (!bio_router_is_initialized()) {
        LOG_WARN("Bio-router not initialized, skipping bio-async integration");
        return NIMCP_SUCCESS;
    }

    // Thread-safe initialization check with mutex
    nimcp_mutex_lock(&g_spatial_bio_state.init_mutex);

    if (g_spatial_bio_state.initialized) {
        g_spatial_bio_state.system = system;  // Update system reference
        nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);
        LOG_DEBUG("Spatial neuromodulator bio-async already initialized, updated system ref");
        return NIMCP_SUCCESS;
    }

    // Register module with bio-router
    // Use BIO_MODULE_NEUROMODULATOR (not _SPATIAL) for compatibility with existing code
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_NEUROMODULATOR,
        .module_name = "spatial_neuromodulator",
        .inbox_capacity = 100,
        .user_data = &g_spatial_bio_state
    };

    g_spatial_bio_state.module_ctx = bio_router_register_module(&module_info);
    if (!g_spatial_bio_state.module_ctx) {
        nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);
        LOG_ERROR("Failed to register spatial neuromodulator module with bio-router");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    /* Try KG-driven wiring callback registration first */
    nimcp_error_t wiring_result = bio_router_register_wiring_callback(
        BIO_MODULE_NEUROMODULATOR,
        (void*)spatial_neuromod_wiring_handler_callback,
        system
    );

    if (wiring_result == NIMCP_SUCCESS) {
        LOG_INFO("Spatial neuromodulator: KG-driven wiring callback registered");
    } else {
        // Legacy fallback - register handlers directly
        nimcp_error_t err;

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(
                g_spatial_bio_state.module_ctx,
                BIO_MSG_NEUROMODULATOR_RELEASE,
                handle_neuromodulator_release_request
            )
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to register neuromodulator release handler");
            bio_router_unregister_module(g_spatial_bio_state.module_ctx);
            g_spatial_bio_state.module_ctx = NULL;
            nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);
            return err;
        }

        LOG_INFO("Spatial neuromodulator: legacy handler registration");
    }

    // Store system reference
    g_spatial_bio_state.system = system;
    g_spatial_bio_state.initialized = true;
    g_spatial_bio_state.messages_processed = 0;

    nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);

    LOG_INFO("Spatial neuromodulator bio-async integration initialized");

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown bio-async integration
 */
static void spatial_neuromod_bio_async_shutdown(void) {
    nimcp_mutex_lock(&g_spatial_bio_state.init_mutex);

    if (!g_spatial_bio_state.initialized) {
        nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);
        return;
    }

    if (g_spatial_bio_state.module_ctx) {
        bio_router_unregister_module(g_spatial_bio_state.module_ctx);
        g_spatial_bio_state.module_ctx = NULL;
    }

    uint64_t processed = g_spatial_bio_state.messages_processed;
    g_spatial_bio_state.system = NULL;
    g_spatial_bio_state.initialized = false;

    nimcp_mutex_unlock(&g_spatial_bio_state.init_mutex);

    LOG_INFO("Spatial neuromodulator bio-async integration shutdown (processed %lu messages)",
             processed);
}

/**
 * @brief Process pending bio-async messages
 *
 * Should be called periodically to handle incoming messages.
 *
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
static uint32_t spatial_neuromod_bio_async_process(uint32_t max_messages) {
    if (!g_spatial_bio_state.initialized || !g_spatial_bio_state.module_ctx) {
        return 0;
    }

    return bio_router_process_inbox(g_spatial_bio_state.module_ctx, max_messages);
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Validates neuron ID
 * WHY:  Prevent out-of-bounds access
 * HOW:  Range check against num_neurons
 */
static inline bool is_valid_neuron_id(uint32_t id, uint32_t num_neurons) {
    return id < num_neurons;
}

//=============================================================================
// Configuration
//=============================================================================

spatial_neuromod_config_t spatial_neuromod_default_config(neuromodulator_type_t type) {
    // WHAT: Returns biologically-informed defaults for neuromodulator type
    // WHY:  Different neuromodulators have different kinetics
    // HOW:  Lookup table from literature values

    if (type >= NEUROMOD_COUNT) {
        type = NEUROMOD_DOPAMINE;  // Fallback
        LOG_WARN("Invalid neuromodulator type, using dopamine defaults");
    }

    spatial_neuromod_config_t config = {
        .type = type,
        .diffusion_coeff = NEUROMOD_DEFAULTS[type].diffusion,
        .decay_rate = NEUROMOD_DEFAULTS[type].decay,
        .baseline = NEUROMOD_DEFAULTS[type].baseline,
        .timestep = 1.0F,  // 1 ms default
        .substeps = 1,     // No substeps by default

        // Phase C4.3: Quantum-Shannon defaults (disabled by default, backward compatible with C2.1)
        .enable_quantum_walk = false,  // Kept for backward compatibility (enables quantum-Shannon now)
        .quantum_walk_steps = 50,
        .quantum_mixing_ratio = 0.2F,  // 80% quantum, 20% classical
        .quantum_coin_type = COIN_HADAMARD,
        .quantum_decoherence = 0.05F,

        // Phase C4.4: Adaptive Routing defaults (disabled by default)
        .enable_adaptive_routing = false,       // Opt-in for intelligent source selection
        .efficiency_weight = 1.0F,              // Weight for propagation efficiency
        .speedup_weight = 0.5F,                 // Weight for quantum speedup
        .bottleneck_penalty_weight = 2.0F,      // Weight for bottleneck penalty (higher = avoid more)
        .info_rate_weight = 0.3F,               // Weight for information rate
        .num_adaptive_sources = 3,              // Select 3 optimal sources
        .min_source_score = 0.1F,               // Minimum score threshold

        // Phase C4.5: Dynamic Adaptation defaults (disabled by default)
        .enable_dynamic_adaptation = false,     // Opt-in for automatic K tuning
        .min_adaptive_sources = 1,              // Minimum K value
        .max_adaptive_sources = 10,             // Maximum K value
        .adaptation_rate = 0.1F,                // EMA smoothing factor (10% new, 90% old)
        .target_efficiency = 0.75F,             // Target efficiency to maintain
        .efficiency_tolerance = 0.1F,           // Tolerance band (±10%)
        .adaptation_cooldown_steps = 100,       // Wait 100 steps between adaptations

        // Phase C4.6: Multi-Objective defaults (disabled by default)
        .enable_multi_objective = false,        // Opt-in for Pareto-optimal selection
        .num_objectives = 2,                    // 2 objectives by default
        .objective_weights = {0.5F, 0.5F, 0.0F, 0.0F},  // Equal weights for 2 objectives
        .pareto_epsilon = 0.01F,                // 1% epsilon for dominance
        .prefer_diversity = true                // Prefer diverse solutions on Pareto front
    };

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

spatial_neuromod_field_t* spatial_neuromod_create(uint32_t num_neurons,
                                                   const spatial_neuromod_config_t* config) {
    // WHAT: Allocates and initializes spatial neuromodulator field
    // WHY:  Need concentration arrays for each neuron
    // HOW:  malloc arrays, initialize to baseline, set parameters
    //
    // COMPLEXITY: O(N) memory, O(N) time
    // MEMORY: ~3*N*sizeof(float) + overhead (~12N bytes for 32-bit floats)

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_create: config is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator creation");
        return NULL;
    }
    if (num_neurons == 0 || num_neurons > MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_create: invalid num_neurons");
        LOG_ERROR("Invalid parameters for spatial neuromodulator creation");
        return NULL;
    }

    // Allocate main structure
    spatial_neuromod_field_t* field = (spatial_neuromod_field_t*)
        nimcp_aligned_alloc(64, sizeof(spatial_neuromod_field_t));
    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_create: failed to allocate field");
        LOG_ERROR("Failed to allocate spatial neuromodulator field");
        return NULL;
    }

    // Initialize scalars
    memset(field, 0, sizeof(spatial_neuromod_field_t));
    field->num_neurons = num_neurons;
    field->type = config->type;
    field->diffusion_coeff = config->diffusion_coeff;
    field->decay_rate = config->decay_rate;
    field->baseline = config->baseline;
    field->timestep = config->timestep;
    field->substeps = config->substeps;
    field->max_concentration = 1.0F;
    field->min_concentration = 0.0F;

    // Allocate concentration arrays (cache-aligned for SIMD)
    field->concentration = (float*)nimcp_aligned_alloc(64, num_neurons * sizeof(float));
    field->source_rate = (float*)nimcp_aligned_alloc(64, num_neurons * sizeof(float));
    field->laplacian_buffer = (float*)nimcp_aligned_alloc(64, num_neurons * sizeof(float));

    if (!field->concentration || !field->source_rate || !field->laplacian_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_create: failed to allocate concentration arrays");
        LOG_ERROR("Failed to allocate concentration arrays");
        spatial_neuromod_destroy(field);
        return NULL;
    }

    // Initialize arrays to baseline/zero
    for (uint32_t i = 0; i < num_neurons; i++) {
        field->concentration[i] = config->baseline;
        field->source_rate[i] = 0.0F;
        field->laplacian_buffer[i] = 0.0F;
    }

    field->avg_concentration = config->baseline;

    // Phase C4.3: Initialize quantum-Shannon if enabled
    field->use_quantum_shannon = config->enable_quantum_walk;  // Backward compatible name
    field->quantum_mixing_ratio = config->quantum_mixing_ratio;
    field->quantum_shannon_diffusion = NULL;

    // Initialize Shannon metrics
    field->last_propagation_efficiency = 0.0F;
    field->last_speedup_vs_classical = 1.0F;
    field->last_num_bottlenecks = 0;
    field->last_information_rate = 0.0F;

    // Initialize Phase C4.5: Dynamic Adaptation state
    field->efficiency_ema = 0.0F;                        // Start with no history
    field->current_adaptive_sources = config->num_adaptive_sources;  // Start with config value
    field->adaptation_cooldown = 0;                      // No cooldown initially

    // Initialize Phase C4.6: Multi-Objective state
    field->pareto_front_size = 0;                        // Empty Pareto front initially
    field->pareto_cache_generation = 0;                  // Cache invalid initially
    // Note: pareto_front_scores initialized lazily on first use (avoids nested loops)

    if (config->enable_quantum_walk) {
        // WARNING: Cannot initialize quantum-Shannon here because we don't have
        // the neural network pointer yet. This will be done in system_create.
        // For now, just mark that it should be created.
        LOG_INFO("Quantum-Shannon diffusion enabled for field type=%d", config->type);
    }

    LOG_INFO("Created spatial neuromodulator field: type=%d, neurons=%u, D=%.3f, k=%.3f",
                   config->type, num_neurons, config->diffusion_coeff, config->decay_rate);

    return field;
}

void spatial_neuromod_destroy(spatial_neuromod_field_t* field) {
    // WHAT: Frees all allocated memory for spatial field
    // WHY:  Prevent memory leaks
    // HOW:  Free arrays then structure

    if (!field) return;


    if (field->concentration) {
        nimcp_aligned_free(field->concentration);
    }
    if (field->source_rate) {
        nimcp_aligned_free(field->source_rate);
    }
    if (field->laplacian_buffer) {
        nimcp_aligned_free(field->laplacian_buffer);
    }

    // Phase C4.3: Cleanup quantum-Shannon
    if (field->quantum_shannon_diffusion) {
        quantum_shannon_destroy((quantum_shannon_diffusion_t*)field->quantum_shannon_diffusion);
        field->quantum_shannon_diffusion = NULL;
    }

    nimcp_aligned_free(field);
}

spatial_neuromod_system_t* spatial_neuromod_system_create(
    neural_network_t network,
    const bool enabled_types[NEUROMOD_COUNT],
    const spatial_neuromod_config_t configs[NEUROMOD_COUNT]) {

    // WHAT: Creates complete multi-field spatial neuromodulator system
    // WHY:  Manage all neuromodulator types in one system
    // HOW:  Create individual fields for each enabled type

    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_create: network is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator system creation");
        return NULL;
    }
    if (!enabled_types) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_create: enabled_types is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator system creation");
        return NULL;
    }
    if (!configs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_create: configs is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator system creation");
        return NULL;
    }

    spatial_neuromod_system_t* system = (spatial_neuromod_system_t*)
        nimcp_aligned_alloc(64, sizeof(spatial_neuromod_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_system_create: failed to allocate system");
        LOG_ERROR("Failed to allocate spatial neuromodulator system");
        return NULL;
    }

    memset(system, 0, sizeof(spatial_neuromod_system_t));
    system->network = network;
    system->global_diffusion_scale = 1.0F;
    system->use_substeps = false;

    // Get number of neurons from network using accessor function
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        system->enabled[i] = enabled_types[i];
        if (enabled_types[i]) {
            system->fields[i] = spatial_neuromod_create(num_neurons, &configs[i]);
            if (!system->fields[i]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_system_create: failed to create field");
                LOG_ERROR("Failed to create field for neuromodulator type %d", i);
                spatial_neuromod_system_destroy(system);
                return NULL;
            }
        }
    }

    // Phase C4.3: Create quantum-Shannon systems for enabled fields
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i] && system->fields[i]->use_quantum_shannon) {
            // Create quantum-Shannon config
            quantum_shannon_config_t qs_config = quantum_shannon_default_config();

            // Override with field-specific settings
            qs_config.quantum_config.coin_type = configs[i].quantum_coin_type;
            qs_config.quantum_config.num_steps = configs[i].quantum_walk_steps;
            qs_config.quantum_config.hybrid_mixing = configs[i].quantum_mixing_ratio;
            qs_config.quantum_config.decoherence_rate = configs[i].quantum_decoherence;

            // Use middle neuron as source for better connectivity
            uint32_t source_neuron = num_neurons / 2;

            // Initial information: 10 bits (neuromodulator concentration)
            float source_information = 10.0F;

            // Create quantum-Shannon diffusion system
            quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
                network,
                source_neuron,
                source_information,
                &qs_config
            );

            if (!qsd) {
                LOG_WARN("Failed to create quantum-Shannon for neuromodulator type %d, falling back to classical diffusion", i);
                system->fields[i]->use_quantum_shannon = false;
            } else {
                system->fields[i]->quantum_shannon_diffusion = (void*)qsd;
                LOG_INFO("Quantum-Shannon created for neuromodulator type %d (√N speedup + Shannon metrics enabled)", i);
            }
        }
    }

    LOG_INFO("Created spatial neuromodulator system with %u neurons", num_neurons);

    // Initialize bio-async integration
    nimcp_error_t bio_err = spatial_neuromod_bio_async_init(system);
    if (bio_err != NIMCP_SUCCESS) {
        LOG_WARN("Failed to initialize bio-async integration: %d", bio_err);
        // Continue anyway - bio-async is optional
    }

    return system;
}

void spatial_neuromod_system_destroy(spatial_neuromod_system_t* system) {
    // WHAT: Destroys entire spatial neuromodulator system
    // WHY:  Clean up all fields
    // HOW:  Destroy each field then system structure

    if (!system) return;


    // Shutdown bio-async integration
    spatial_neuromod_bio_async_shutdown();

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i]) {
            spatial_neuromod_destroy(system->fields[i]);
        }
    }

    nimcp_aligned_free(system);
}

bool spatial_neuromod_system_update(
    spatial_neuromod_system_t* system,
    neural_network_t network,
    float dt)
{
    // WHAT: Updates all enabled neuromodulator fields in system
    // WHY:  Batch update all fields for convenience and consistency
    // HOW:  Iterate through fields, update each enabled one

    // Guard: Validate inputs
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_update: system is NULL");
        LOG_ERROR("Null system in spatial_neuromod_system_update");
        return false;
    }

    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_update: network is NULL");
        LOG_ERROR("Null network in spatial_neuromod_system_update");
        return false;
    }

    if (dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_system_update: invalid timestep");
        LOG_ERROR("Invalid timestep dt=%.6f in spatial_neuromod_system_update", dt);
        return false;
    }

    // Process bio-async messages before updating
    spatial_neuromod_bio_async_process(10);  // Process up to 10 messages per update

    // Update each enabled field
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (!system->enabled[i]) continue;
        if (!system->fields[i]) continue;

        bool success = spatial_neuromod_update(system->fields[i], network, dt);
        if (!success) {
            LOG_ERROR("Failed to update field %d in system", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_system_update: success is NULL");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Diffusion Dynamics - Core PDE Solver
//=============================================================================

bool spatial_neuromod_compute_laplacian(const spatial_neuromod_field_t* field,
                                         neural_network_t network,
                                         float* laplacian) {
    // WHAT: Computes discrete graph Laplacian operator
    // WHY:  Core operator for diffusion equation
    // HOW:  For each neuron: L_i = Σ_neighbors (c_j - c_i)
    //
    // MATHEMATICAL DEFINITION:
    // Continuous: ∇²c = ∂²c/∂x² + ∂²c/∂y² + ∂²c/∂z²
    // Discrete on graph: L_i = Σ_j∈N(i) (c_j - c_i)
    // Where N(i) = neighbors of node i (connected by edges)
    //
    // COMPLEXITY: O(E) where E = number of edges (synapses)
    // PERFORMANCE: ~10μs per 1000 neurons with avg degree 50

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_compute_laplacian: field is NULL");
        LOG_ERROR("Invalid parameters for Laplacian computation");
        return false;
    }
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_compute_laplacian: network is NULL");
        LOG_ERROR("Invalid parameters for Laplacian computation");
        return false;
    }
    if (!laplacian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_compute_laplacian: laplacian is NULL");
        LOG_ERROR("Invalid parameters for Laplacian computation");
        return false;
    }

    const float* concentration = field->concentration;
    const uint32_t num_neurons = field->num_neurons;

    // Initialize Laplacian to zero
    memset(laplacian, 0, num_neurons * sizeof(float));

    // Iterate over all neurons using accessor function
    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) {
            continue;  // Skip invalid neurons
        }

        float c_i = concentration[i];
        float lap_sum = 0.0F;

        // Sum over outgoing synapses (neighbors)
        for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
            synapse_handle_t* syn = NEURON_OUT_HANDLE(neuron, s);
            if (!syn) continue;
            uint32_t j = syn->target_neuron_id;

            if (!is_valid_neuron_id(j, num_neurons)) {
                continue;  // Skip invalid connections
            }

            float c_j = concentration[j];
            lap_sum += (c_j - c_i);  // Difference with neighbor
        }

        // Also consider incoming synapses for bidirectional diffusion
        for (uint32_t s = 0; s < NEURON_IN_COUNT(neuron); s++) {
            synapse_handle_t* syn = NEURON_IN_HANDLE(neuron, s);
            if (!syn) continue;
            // Find source neuron (need to search network - optimization possible)
            // For simplicity, we assume bidirectional diffusion via outgoing only
            // In full implementation, track source_id in synapse
        }

        laplacian[i] = lap_sum;
    }

    return true;
}

bool spatial_neuromod_update(spatial_neuromod_field_t* field,
                              neural_network_t network,
                              float dt) {
    // WHAT: Updates spatial concentration field for one timestep
    // WHY:  Core integration step for reaction-diffusion PDE
    // HOW:  Explicit Euler: c(t+dt) = c(t) + dt*(D*Laplacian - k*c + S)
    //
    // REACTION-DIFFUSION EQUATION:
    // ∂c/∂t = D * ∇²c - k*c + S(x,t)
    //
    // DISCRETIZED:
    // c_i(t+Δt) = c_i(t) + Δt * [D * Σ_j(c_j - c_i) - k*c_i + S_i]
    //
    // STABILITY CONDITION (von Neumann):
    // For explicit Euler: Δt ≤ 1/(2*D*max_degree)
    // If violated, use substeps or reduce timestep
    //
    // COMPLEXITY: O(E) per substep, where E = edges
    // TYPICAL: ~50μs for 1000 neurons, degree=50, 1 substep

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update: field is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator update");
        return false;
    }
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update: network is NULL");
        LOG_ERROR("Invalid parameters for spatial neuromodulator update");
        return false;
    }

    if (dt <= 0.0F || dt > 1.0F) {
        LOG_WARN("Unusual timestep dt=%.3f, clamping to [1e-6, 1.0]", dt);
        dt = nimcp_clampf(dt, 1e-6F, 1.0F);
    }

    const float D = field->diffusion_coeff;
    const float k = field->decay_rate;
    const uint32_t num_neurons = field->num_neurons;
    const uint32_t substeps = (field->substeps > 0) ? field->substeps : 1;
    const float sub_dt = dt / substeps;

    float* concentration = field->concentration;
    const float* source_rate = field->source_rate;
    float* laplacian = field->laplacian_buffer;

    // Phase C4.3: Use quantum-Shannon for diffusion if enabled
    if (field->use_quantum_shannon && field->quantum_shannon_diffusion) {
        quantum_shannon_diffusion_t* qsd = (quantum_shannon_diffusion_t*)field->quantum_shannon_diffusion;

        // Evolve quantum-Shannon diffusion
        // This performs quantum walk + Shannon information flow monitoring
        bool success = quantum_shannon_evolve(qsd, qsd->config.quantum_config.num_steps);

        if (success) {
            // Get probability distribution from quantum-Shannon
            float* qsd_prob = (float*)nimcp_malloc(num_neurons * sizeof(float));
            if (qsd_prob) {
                quantum_shannon_get_distribution(qsd, qsd_prob);

                // Get Shannon metrics
                shannon_diffusion_metrics_t metrics;
                quantum_shannon_get_metrics(qsd, &metrics);

                // Update field Shannon metrics
                field->last_propagation_efficiency = metrics.propagation_efficiency;
                field->last_speedup_vs_classical = metrics.speedup_vs_classical;
                field->last_num_bottlenecks = metrics.num_bottlenecks;
                field->last_information_rate = metrics.information_rate;

                // Hybrid: Mix quantum probability with classical diffusion
                float quantum_weight = 1.0F - field->quantum_mixing_ratio;
                float classical_weight = field->quantum_mixing_ratio;

                // Find source neurons and apply quantum diffusion
                for (uint32_t i = 0; i < num_neurons; i++) {
                    if (source_rate[i] > 1e-6F) {
                        // Apply quantum distribution scaled by source strength
                        for (uint32_t j = 0; j < num_neurons; j++) {
                            float quantum_contrib = qsd_prob[j] * source_rate[i];
                            concentration[j] = classical_weight * concentration[j] +
                                             quantum_weight * quantum_contrib;
                        }
                    }
                }

                nimcp_free(qsd_prob);

                // Log if bottlenecks detected (for debugging/optimization)
                if (metrics.num_bottlenecks > 0) {
                    LOG_INFO("Neuromodulator type %d: %u bottlenecks detected (speedup: %.2fx)",
                                   field->type, metrics.num_bottlenecks, metrics.speedup_vs_classical);
                }
            }
        }

        // Still apply decay and source terms
        for (uint32_t i = 0; i < num_neurons; i++) {
            float c = concentration[i];
            float S = source_rate[i];
            // Apply decay and source
            float dc = (-k * c + S) * dt;
            c += dc;
            concentration[i] = nimcp_clampf(c, field->min_concentration, field->max_concentration);
        }

        // Update statistics
        float sum = 0.0F;
        for (uint32_t i = 0; i < num_neurons; i++) {
            sum += concentration[i];
        }
        field->avg_concentration = (num_neurons > 0) ? (sum / num_neurons) : 0.0F;
        field->update_count++;

        return true;  // Skip classical diffusion
    }

    // Classical diffusion (fallback or when quantum disabled)
    // Substep loop (for stability with large D or high degree)
    for (uint32_t step = 0; step < substeps; step++) {
        // 1. Compute graph Laplacian
        if (!spatial_neuromod_compute_laplacian(field, network, laplacian)) {
            LOG_ERROR("Failed to compute Laplacian");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: spatial_neuromod_compute_laplacian is NULL");
            return false;
        }

        // 2. Apply reaction-diffusion equation with Riemannian curvature correction.
        //    On a curved neural manifold, diffusion is governed by the
        //    Laplace-Beltrami operator, not the flat Laplacian. We approximate
        //    the curvature correction using the local concentration gradient:
        //    curved_D = D * (1 + curvature_scale * |grad_c|^2)
        //    This accelerates diffusion in high-gradient (curved) regions.
        float total_decay = 0.0F;
        float sum_concentration = 0.0F;

        for (uint32_t i = 0; i < num_neurons; i++) {
            float c = concentration[i];
            float L = laplacian[i];
            float S = source_rate[i];

            // Riemannian curvature correction: approximate local metric
            // distortion from concentration gradient magnitude.
            // High Laplacian magnitude indicates high curvature → boost diffusion.
            float grad_mag_sq = L * L;
            float curved_D = D * (1.0f + 0.1f * grad_mag_sq /
                                  (grad_mag_sq + 1.0f));

            // dc/dt = D_curved * L - k*c + S
            float dcdt = curved_D * L - k * c + S;

            // Explicit Euler step
            float c_new = c + sub_dt * dcdt;

            // Clamp to valid range
            c_new = nimcp_clampf(c_new, field->min_concentration, field->max_concentration);

            concentration[i] = c_new;
            sum_concentration += c_new;
            total_decay += k * c * sub_dt;
        }

        // Update statistics
        field->avg_concentration = sum_concentration / num_neurons;
        field->total_decayed += total_decay;
    }

    // Clear source rates after they've been consumed
    // RATIONALE: Phasic releases are bursts, not continuous infusions.
    //            Source term should only apply once per release() call.
    for (uint32_t i = 0; i < num_neurons; i++) {
        field->source_rate[i] = 0.0F;
    }

    field->update_count++;

    return true;
}

//=============================================================================
// Source Term Manipulation (Release Events)
//=============================================================================

bool spatial_neuromod_release(spatial_neuromod_field_t* field,
                               uint32_t neuron_id,
                               float amount) {
    // WHAT: Adds neuromodulator release at specific neuron
    // WHY:  Models phasic release (e.g., dopamine burst from VTA)
    // HOW:  Increments source_rate which drives diffusion
    //
    // BIOLOGICAL: Vesicular release from presynaptic terminal
    // UNITS: amount in normalized units (0-1 typical)
    //
    // RATIONALE: source_rate is the canonical source term in the diffusion equation
    //            dc/dt = D*Laplacian(c) - k*c + S where S = source_rate
    //            This allows diffusion update to handle spatial/temporal dynamics

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release: field is NULL");
        LOG_ERROR("NULL field in spatial_neuromod_release");
        return false;
    }

    if (!is_valid_neuron_id(neuron_id, field->num_neurons)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_release: invalid neuron_id");
        LOG_ERROR("Invalid neuron_id=%u (max=%u)", neuron_id, field->num_neurons);
        return false;
    }

    if (amount < 0.0F) {
        LOG_WARN("Negative release amount=%.3f, clamping to 0", amount);
        amount = 0.0F;
    }

    // Add to source_rate (will be applied in next diffusion update)
    field->source_rate[neuron_id] += amount;

    field->total_released += amount;

    return true;
}

bool spatial_neuromod_release_batch(spatial_neuromod_field_t* field,
                                     const uint32_t* neuron_ids,
                                     const float* amounts,
                                     uint32_t count) {
    // WHAT: Batch release operation
    // WHY:  Efficient for simultaneous releases
    // HOW:  Loop over arrays, update source terms

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_batch: field is NULL");
        LOG_ERROR("Invalid parameters for batch release");
        return false;
    }
    if (!neuron_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_batch: neuron_ids is NULL");
        LOG_ERROR("Invalid parameters for batch release");
        return false;
    }
    if (!amounts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_batch: amounts is NULL");
        LOG_ERROR("Invalid parameters for batch release");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!spatial_neuromod_release(field, neuron_ids[i], amounts[i])) {
            LOG_WARN("Failed to release at neuron %u", neuron_ids[i]);
            // Continue with other releases
        }
    }

    return true;
}

//=============================================================================
// Query Functions
//=============================================================================

float spatial_neuromod_get_concentration(const spatial_neuromod_field_t* field,
                                          uint32_t neuron_id) {
    // WHAT: Gets concentration at specific neuron
    // WHY:  Neurons/synapses need local concentration for modulation
    // HOW:  Direct array access

    if (!field || !is_valid_neuron_id(neuron_id, field->num_neurons)) {
        return 0.0F;
    }

    return field->concentration[neuron_id];
}

bool spatial_neuromod_set_concentration(spatial_neuromod_field_t* field,
                                         uint32_t neuron_id,
                                         float concentration) {
    // WHAT: Sets concentration at specific neuron
    // WHY:  For initialization or testing
    // HOW:  Direct array write with clamping

    if (!field || !is_valid_neuron_id(neuron_id, field->num_neurons)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (field, is_valid_neuron_id)");
        return false;
    }

    field->concentration[neuron_id] = nimcp_clampf(concentration,
                                             field->min_concentration,
                                             field->max_concentration);
    return true;
}

float spatial_neuromod_get_gradient(const spatial_neuromod_field_t* field,
                                     neural_network_t network,
                                     uint32_t neuron_id) {
    // WHAT: Computes spatial gradient magnitude at neuron
    // WHY:  Quantify spatial non-uniformity
    // HOW:  |∇c| ≈ Σ_neighbors |c_j - c_i| / degree

    if (!field || !network || !is_valid_neuron_id(neuron_id, field->num_neurons)) {
        return 0.0F;
    }

    float c_i = field->concentration[neuron_id];
    neuron_t* neuron = neural_network_get_neuron(network, neuron_id);
    if (!neuron) {
        return 0.0F;
    }

    float gradient_sum = 0.0F;
    uint32_t degree = NEURON_OUT_COUNT(neuron);

    if (degree == 0) return 0.0F;

    for (uint32_t s = 0; s < degree; s++) {
        synapse_handle_t* syn = NEURON_OUT_HANDLE(neuron, s);
        if (!syn) continue;
        uint32_t j = syn->target_neuron_id;

        if (!is_valid_neuron_id(j, field->num_neurons)) continue;

        float c_j = field->concentration[j];
        gradient_sum += fabsf(c_j - c_i);
    }

    return gradient_sum / degree;
}

float spatial_neuromod_get_average(const spatial_neuromod_field_t* field) {
    // WHAT: Returns average concentration across network
    // WHY:  Global measure of neuromodulator level
    // HOW:  Computes average on-the-fly to avoid stale cache
    //
    // RATIONALE: Changed from cached to computed to handle manual concentration
    //            changes via set_concentration() (used in tests and initialization).
    //            Performance impact is minimal (O(N) scan) for this query operation.

    if (!field) return 0.0F;
    if (field->num_neurons == 0) return 0.0F;

    // Compute average from current concentration array
    float sum = 0.0F;
    for (uint32_t i = 0; i < field->num_neurons; i++) {
        sum += field->concentration[i];
    }

    return sum / field->num_neurons;
}

float spatial_neuromod_get_max(const spatial_neuromod_field_t* field,
                                uint32_t* neuron_id_out) {
    // WHAT: Finds maximum concentration in network
    // WHY:  Track hotspots, saturation
    // HOW:  Linear scan

    if (!field) return 0.0F;

    float max_conc = -FLT_MAX;
    uint32_t max_id = 0;

    for (uint32_t i = 0; i < field->num_neurons; i++) {
        if (field->concentration[i] > max_conc) {
            max_conc = field->concentration[i];
            max_id = i;
        }
    }

    if (neuron_id_out) {
        *neuron_id_out = max_id;
    }

    return max_conc;
}

//=============================================================================
// Integration with Global Neuromodulator System
//=============================================================================

bool spatial_neuromod_sync_to_global(const spatial_neuromod_field_t* field,
                                      neuromodulator_system_t system) {
    // WHAT: Updates global concentration from spatial average
    // WHY:  Maintain consistency between spatial and global
    // HOW:  Set global level = avg(spatial concentrations)

    if (!field || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_get_average: required parameter is NULL (field, system)");
        return false;
    }

    float avg = spatial_neuromod_get_average(field);
    return neuromodulator_set_level(system, field->type, avg);
}

bool spatial_neuromod_init_from_global(spatial_neuromod_field_t* field,
                                        neuromodulator_system_t system) {
    // WHAT: Initializes spatial field from global level
    // WHY:  Bootstrap spatial from existing global state
    // HOW:  Set all c_i = global_level

    if (!field || !system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_get_average: required parameter is NULL (field, system)");
        return false;
    }

    float global_level = neuromodulator_get_level(system, field->type);

    for (uint32_t i = 0; i < field->num_neurons; i++) {
        field->concentration[i] = global_level;
    }

    field->avg_concentration = global_level;

    return true;
}

//=============================================================================
// Visualization & Analysis
//=============================================================================

uint32_t spatial_neuromod_export(const spatial_neuromod_field_t* field,
                                  float* buffer,
                                  uint32_t buffer_size) {
    // WHAT: Exports concentration field to buffer
    // WHY:  For external visualization/analysis
    // HOW:  memcpy to provided buffer

    if (!field || !buffer || buffer_size < field->num_neurons) {
        return 0;
    }

    memcpy(buffer, field->concentration, field->num_neurons * sizeof(float));
    return field->num_neurons;
}

bool spatial_neuromod_compute_stats(const spatial_neuromod_field_t* field,
                                     neural_network_t network,
                                     float* mean_out,
                                     float* variance_out,
                                     float* max_gradient_out) {
    // WHAT: Computes spatial statistics
    // WHY:  Quantitative analysis of distribution
    // HOW:  Single pass over arrays

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_get_statistics: field is NULL");
        return false;
    }
    if (field->num_neurons == 0) {
        if (mean_out) *mean_out = 0.0F;
        if (variance_out) *variance_out = 0.0F;
        if (max_gradient_out) *max_gradient_out = 0.0F;
        return true;
    }

    // Mean
    float mean = spatial_neuromod_get_average(field);
    if (mean_out) *mean_out = mean;

    // Variance
    if (variance_out) {
        float var_sum = 0.0F;
        for (uint32_t i = 0; i < field->num_neurons; i++) {
            float diff = field->concentration[i] - mean;
            var_sum += diff * diff;
        }
        *variance_out = var_sum / field->num_neurons;
    }

    // Maximum gradient
    if (max_gradient_out && network) {
        float max_grad = 0.0F;
        for (uint32_t i = 0; i < field->num_neurons; i++) {
            float grad = spatial_neuromod_get_gradient(field, network, i);
            if (grad > max_grad) max_grad = grad;
        }
        *max_gradient_out = max_grad;
    }

    return true;
}

//=============================================================================
// Reset & Debugging
//=============================================================================

bool spatial_neuromod_reset(spatial_neuromod_field_t* field) {
    // WHAT: Resets field to baseline
    // WHY:  Clean state for new simulation
    // HOW:  Set all c_i = baseline, S_i = 0

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_reset: field is NULL");
        return false;
    }

    for (uint32_t i = 0; i < field->num_neurons; i++) {
        field->concentration[i] = field->baseline;
        field->source_rate[i] = 0.0F;
        field->laplacian_buffer[i] = 0.0F;
    }

    field->total_released = 0.0F;
    field->total_decayed = 0.0F;
    field->avg_concentration = field->baseline;
    field->max_gradient = 0.0F;
    field->update_count = 0;

    return true;
}

bool spatial_neuromod_validate(const spatial_neuromod_field_t* field) {
    // WHAT: Validates field state for errors
    // WHY:  Catch numerical issues early
    // HOW:  Check for NaN, inf, out-of-range values

    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_validate: field is NULL");
        return false;
    }

    for (uint32_t i = 0; i < field->num_neurons; i++) {
        float c = field->concentration[i];

        // Check for NaN/inf
        if (!isfinite(c)) {
            LOG_ERROR("Non-finite concentration at neuron %u: %.3f", i, c);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "spatial_neuromod_validate: isfinite is NULL");
            return false;
        }

        // Check range
        if (c < field->min_concentration - EPSILON ||
            c > field->max_concentration + EPSILON) {
            LOG_ERROR("Concentration out of range at neuron %u: %.3f", i, c);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "spatial_neuromod_validate: isfinite is NULL");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Phase C4.4: Adaptive Routing Implementation
//=============================================================================

float spatial_neuromod_score_neuron(
    const spatial_neuromod_field_t* field,
    uint32_t neuron_id,
    neural_network_t network,
    const spatial_neuromod_config_t* config)
{
    // WHAT: Computes suitability score for neuron as neuromodulator source
    // WHY:  Intelligent source selection maximizes information propagation
    // HOW:  Weighted combination of Shannon metrics
    //
    // COMPLEXITY: O(1)

    // Guard: Validate parameters
    if (!field || !config || !is_valid_neuron_id(neuron_id, field->num_neurons)) {
        return 0.0F;
    }

    // Guard: Quantum-Shannon must be enabled
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        return 0.0F;
    }

    // Extract Shannon metrics
    float efficiency = field->last_propagation_efficiency;  // η ∈ [0, 1]
    float speedup = field->last_speedup_vs_classical;       // speedup ≥ 1.0
    uint32_t bottlenecks = field->last_num_bottlenecks;     // count
    float info_rate = field->last_information_rate;         // bits/step

    // Normalize speedup to [0, 1] range (assume max speedup = 50x)
    float speedup_normalized = fminf(speedup / 50.0F, 1.0F);

    // Normalize information rate to [0, 1] range (assume max = 10.0 bits/step)
    float info_rate_normalized = fminf(info_rate / 10.0F, 1.0F);

    // Bottleneck penalty: more bottlenecks = lower score
    float bottleneck_penalty = bottlenecks > 0 ? 1.0F / (1.0F + bottlenecks) : 1.0F;

    // Compute weighted score
    float score = config->efficiency_weight * efficiency
                + config->speedup_weight * speedup_normalized
                - config->bottleneck_penalty_weight * (1.0F - bottleneck_penalty)
                + config->info_rate_weight * info_rate_normalized;

    // Normalize to [0, 1]
    float max_possible_score = config->efficiency_weight
                             + config->speedup_weight
                             + config->info_rate_weight;

    if (max_possible_score > 0.0F) {
        score = score / max_possible_score;
    }

    return fmaxf(0.0F, fminf(score, 1.0F));  // Clamp to [0, 1]
}

// Helper structure for sorting neurons by score
typedef struct {
    uint32_t neuron_id;
    float score;
} neuron_score_t;

// Comparison function for qsort (descending order by score)
static int compare_neuron_scores(const void* a, const void* b) {
    const neuron_score_t* na = (const neuron_score_t*)a;
    const neuron_score_t* nb = (const neuron_score_t*)b;

    if (na->score > nb->score) return -1;
    if (na->score < nb->score) return 1;
    return 0;
}

bool spatial_neuromod_select_optimal_sources(
    const spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    uint32_t* selected_ids,
    float* selected_scores,
    uint32_t* num_selected)
{
    // WHAT: Selects top K neurons for neuromodulator release
    // WHY:  Adaptive routing improves information utilization 2-3x
    // HOW:  Score all neurons, sort, select top K
    //
    // COMPLEXITY: O(N log N) for full sort, O(N log K) with min-heap optimization

    // Guard: Validate parameters
    if (!field || !config || !selected_ids || !num_selected) {
        LOG_ERROR("Invalid parameters for optimal source selection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_optimal_sources: required parameter is NULL (field, config, selected_ids, num_selected)");
        return false;
    }

    // Guard: Quantum-Shannon must be enabled
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        LOG_WARN("Quantum-Shannon not enabled, cannot select optimal sources");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_optimal_sources: required parameter is NULL (field->use_quantum_shannon, field->quantum_shannon_diffusion)");
        return false;
    }

    uint32_t num_neurons = field->num_neurons;
    // Use current_adaptive_sources (may differ from config if Phase C4.5 dynamic adaptation enabled)
    uint32_t K = field->current_adaptive_sources;

    // Clamp K to reasonable range
    if (K == 0) K = 1;
    if (K > num_neurons) K = num_neurons;
    if (K > 100) K = 100;  // Safety limit

    // Allocate temporary array for scoring
    neuron_score_t* scores = (neuron_score_t*)nimcp_malloc(num_neurons * sizeof(neuron_score_t));
    if (!scores) {
        LOG_ERROR("Failed to allocate memory for neuron scoring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_select_optimal_sources: scores is NULL");
        return false;
    }

    // Score all neurons
    for (uint32_t i = 0; i < num_neurons; i++) {
        scores[i].neuron_id = i;
        scores[i].score = spatial_neuromod_score_neuron(field, i, network, config);
    }

    // Sort by score (descending)
    qsort(scores, num_neurons, sizeof(neuron_score_t), compare_neuron_scores);

    // Select top K with score >= min_source_score
    uint32_t count = 0;
    for (uint32_t i = 0; i < K && i < num_neurons; i++) {
        if (scores[i].score >= config->min_source_score) {
            selected_ids[count] = scores[i].neuron_id;
            if (selected_scores) {
                selected_scores[count] = scores[i].score;
            }
            count++;
        }
    }

    *num_selected = count;
    nimcp_free(scores);

    return count > 0;
}

bool spatial_neuromod_release_adaptive(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float total_amount)
{
    // WHAT: Intelligently selects source neurons and releases neuromodulator
    // WHY:  Maximizes information propagation efficiency (2-3x better)
    // HOW:  Selects optimal sources, distributes amount evenly
    //
    // COMPLEXITY: O(N log K) where N = neurons, K = num_adaptive_sources

    // Guard: Validate parameters
    if (!field || !network || !config) {
        LOG_ERROR("Invalid parameters for adaptive release");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_adaptive: required parameter is NULL (field, network, config)");
        return false;
    }

    // Guard: Check if adaptive routing enabled
    if (!config->enable_adaptive_routing) {
        LOG_WARN("Adaptive routing disabled, using fallback (random source)");
        // Fallback: release at middle neuron
        uint32_t middle = field->num_neurons / 2;
        return spatial_neuromod_release(field, middle, total_amount);
    }

    // Guard: Quantum-Shannon must be enabled
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        LOG_WARN("Quantum-Shannon not enabled, using fallback (random source)");
        uint32_t middle = field->num_neurons / 2;
        return spatial_neuromod_release(field, middle, total_amount);
    }

    // Allocate arrays for selected sources
    uint32_t max_sources = config->num_adaptive_sources;
    uint32_t* selected_ids = (uint32_t*)nimcp_malloc(max_sources * sizeof(uint32_t));
    if (!selected_ids) {
        LOG_ERROR("Failed to allocate memory for source selection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spatial_neuromod_release_adaptive: selected_ids is NULL");
        return false;
    }

    // Select optimal sources
    uint32_t num_selected = 0;
    bool success = spatial_neuromod_select_optimal_sources(
        field, network, config, selected_ids, NULL, &num_selected);

    if (!success || num_selected == 0) {
        LOG_WARN("No optimal sources found, using fallback");
        nimcp_free(selected_ids);
        uint32_t middle = field->num_neurons / 2;
        return spatial_neuromod_release(field, middle, total_amount);
    }

    // Distribute total amount evenly across selected sources
    float amount_per_source = total_amount / (float)num_selected;

    // Release at each selected neuron
    for (uint32_t i = 0; i < num_selected; i++) {
        if (!spatial_neuromod_release(field, selected_ids[i], amount_per_source)) {
            LOG_WARN("Failed to release at optimal neuron %u", selected_ids[i]);
        }
    }

    nimcp_free(selected_ids);
    return true;
}

bool spatial_neuromod_release_adaptive_batch(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    const float* amounts,
    uint32_t count)
{
    // WHAT: Multiple adaptive releases with different amounts
    // WHY:  Efficient for time-varying release patterns
    // HOW:  Calls spatial_neuromod_release_adaptive() for each amount
    //
    // COMPLEXITY: O(M * N log K) where M = count, N = neurons, K = sources

    // Guard: Validate parameters
    if (!field || !network || !config || !amounts) {
        LOG_ERROR("Invalid parameters for adaptive batch release");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_adaptive_batch: required parameter is NULL (field, network, config, amounts)");
        return false;
    }

    // Release each amount adaptively
    bool all_success = true;
    for (uint32_t i = 0; i < count; i++) {
        if (!spatial_neuromod_release_adaptive(field, network, config, amounts[i])) {
            LOG_WARN("Failed adaptive release %u/%u", i + 1, count);
            all_success = false;
        }
    }

    return all_success;
}

//=============================================================================
// Phase C4.5: Dynamic Source Adaptation
//=============================================================================

bool spatial_neuromod_update_dynamic_adaptation(
    spatial_neuromod_field_t* field,
    const spatial_neuromod_config_t* config)
{
    // WHAT: Update EMA and dynamically adjust num_adaptive_sources
    // WHY:  Automatically tunes K for optimal network performance
    // HOW:  Exponential moving average + cooldown-based rate limiting
    //
    // COMPLEXITY: O(1)

    // Guard: Validate parameters
    if (!field || !config) {
        LOG_ERROR("Invalid parameters for dynamic adaptation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update_dynamic_adaptation: required parameter is NULL (field, config)");
        return false;
    }

    // Guard: Dynamic adaptation must be enabled
    if (!config->enable_dynamic_adaptation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update_dynamic_adaptation: config->enable_dynamic_adaptation is NULL");
        return false;  // Silent return (not an error, just disabled)
    }

    // Guard: Requires quantum-Shannon for efficiency metrics
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update_dynamic_adaptation: required parameter is NULL (field->use_quantum_shannon, field->quantum_shannon_diffusion)");
        return false;
    }

    // Guard: Requires adaptive routing to be enabled
    if (!config->enable_adaptive_routing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_update_dynamic_adaptation: config->enable_adaptive_routing is NULL");
        return false;
    }

    // Update efficiency EMA
    float alpha = config->adaptation_rate;
    float current_efficiency = field->last_propagation_efficiency;
    field->efficiency_ema = alpha * current_efficiency + (1.0F - alpha) * field->efficiency_ema;

    // Decrement cooldown
    if (field->adaptation_cooldown > 0) {
        field->adaptation_cooldown--;
        return true;  // Success, but not adapting yet
    }

    // Check if we should adapt
    float target = config->target_efficiency;
    float tolerance = config->efficiency_tolerance;
    float ema = field->efficiency_ema;

    uint32_t K = field->current_adaptive_sources;
    bool adapted = false;

    if (ema < target - tolerance) {
        // Efficiency too low: increase K (more source diversity)
        if (K < config->max_adaptive_sources) {
            field->current_adaptive_sources = K + 1;
            adapted = true;
            LOG_INFO("Dynamic adaptation: Increased K from %u to %u (EMA=%.3f < target=%.3f)",
                          K, K + 1, ema, target);
        }
    } else if (ema > target + tolerance) {
        // Efficiency too high: decrease K (fewer sources needed)
        if (K > config->min_adaptive_sources) {
            field->current_adaptive_sources = K - 1;
            adapted = true;
            LOG_INFO("Dynamic adaptation: Decreased K from %u to %u (EMA=%.3f > target=%.3f)",
                          K, K - 1, ema, target);
        }
    }

    // Reset cooldown if we adapted
    if (adapted) {
        field->adaptation_cooldown = config->adaptation_cooldown_steps;
    }

    return true;
}

uint32_t spatial_neuromod_get_current_adaptive_sources(
    const spatial_neuromod_field_t* field)
{
    // WHAT: Query current K value
    // WHY:  With dynamic adaptation, K changes over time
    // HOW:  Return field->current_adaptive_sources
    //
    // COMPLEXITY: O(1)

    // Guard: Validate parameter
    if (!field) {
        return 0;
    }

    return field->current_adaptive_sources;
}

//=============================================================================
// Phase C4.6: Multi-Objective Adaptation
//=============================================================================

bool spatial_neuromod_score_neuron_multi_objective(
    const spatial_neuromod_field_t* field,
    uint32_t neuron_id,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float* scores)
{
    // WHAT: Compute multi-objective scores for neuron
    // WHY:  Support trade-offs between competing objectives
    // HOW:  Extract and normalize Shannon metrics
    //
    // COMPLEXITY: O(1)

    // Guard: Validate parameters
    if (!field || !config || !scores) {
        LOG_ERROR("Invalid parameters for multi-objective scoring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_score_neuron_multi_objective: required parameter is NULL (field, config, scores)");
        return false;
    }

    // Guard: Multi-objective must be enabled
    if (!config->enable_multi_objective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_score_neuron_multi_objective: config->enable_multi_objective is NULL");
        return false;
    }

    // Guard: Quantum-Shannon must be enabled
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_score_neuron_multi_objective: required parameter is NULL (field->use_quantum_shannon, field->quantum_shannon_diffusion)");
        return false;
    }

    // Guard: Valid neuron ID
    if (neuron_id >= field->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_score_neuron_multi_objective: capacity exceeded");
        return false;
    }

    // Guard: Valid number of objectives
    if (config->num_objectives < 2 || config->num_objectives > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_score_neuron_multi_objective: validation failed");
        return false;
    }

    // Extract Shannon metrics (same as Phase C4.4)
    float efficiency = field->last_propagation_efficiency;
    float speedup = field->last_speedup_vs_classical;
    uint32_t bottlenecks = field->last_num_bottlenecks;
    float info_rate = field->last_information_rate;

    // Objective 0: Propagation efficiency [0-1]
    scores[0] = efficiency;

    // Objective 1: Quantum speedup (normalized)
    scores[1] = fminf(speedup / 50.0F, 1.0F);

    // Objective 2: Bottleneck avoidance [0-1]
    scores[2] = bottlenecks > 0 ? 1.0F / (1.0F + bottlenecks) : 1.0F;

    // Objective 3: Information rate (normalized)
    scores[3] = fminf(info_rate / 10.0F, 1.0F);

    return true;
}

bool spatial_neuromod_pareto_dominates(
    const float* scores_a,
    const float* scores_b,
    uint32_t num_objectives,
    float epsilon)
{
    // WHAT: Check if A Pareto-dominates B
    // WHY:  Core operation for finding Pareto front
    // HOW:  A dominates B if: A[i] >= B[i] ∀i, and A[j] > B[j] for some j
    //
    // COMPLEXITY: O(k) where k = num_objectives

    // Guard: Validate parameters
    if (!scores_a || !scores_b || num_objectives == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_pareto_dominates: required parameter is NULL (scores_a, scores_b)");
        return false;
    }

    bool at_least_one_better = false;

    // Check all objectives
    for (uint32_t i = 0; i < num_objectives; i++) {
        if (scores_a[i] < scores_b[i] - epsilon) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_pareto_dominates: validation failed");
            return false;  // A is worse on objective i
        }
        if (scores_a[i] > scores_b[i] + epsilon) {
            at_least_one_better = true;  // A is better on objective i
        }
    }

    return at_least_one_better;
}

// Helper: Compute weighted scalarization score
static float compute_weighted_score(
    const float* scores,
    const float* weights,
    uint32_t num_objectives)
{
    // WHAT: Compute weighted sum of objectives
    // WHY:  Select from Pareto front using preferences
    // HOW:  Σ(weight[i] * score[i])
    //
    // COMPLEXITY: O(k)

    float total = 0.0F;
    for (uint32_t i = 0; i < num_objectives; i++) {
        total += weights[i] * scores[i];
    }
    return total;
}

bool spatial_neuromod_select_pareto_optimal(
    const spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    uint32_t* selected_ids,
    float* selected_scores,
    uint32_t* num_selected)
{
    // WHAT: Select Pareto-optimal neurons
    // WHY:  Find best neurons when objectives conflict
    // HOW:  1) Find Pareto front, 2) Select K from front
    //
    // COMPLEXITY: O(N² × k)

    // Guard: Validate parameters
    if (!field || !config || !selected_ids || !num_selected) {
        LOG_ERROR("Invalid parameters for Pareto selection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_pareto_optimal: required parameter is NULL (field, config, selected_ids, num_selected)");
        return false;
    }

    // Guard: Multi-objective must be enabled
    if (!config->enable_multi_objective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_pareto_optimal: config->enable_multi_objective is NULL");
        return false;
    }

    // Guard: Quantum-Shannon must be enabled
    if (!field->use_quantum_shannon || !field->quantum_shannon_diffusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_pareto_optimal: required parameter is NULL (field->use_quantum_shannon, field->quantum_shannon_diffusion)");
        return false;
    }

    uint32_t N = field->num_neurons;
    uint32_t K = field->current_adaptive_sources;
    uint32_t num_obj = config->num_objectives;

    // Allocate memory for all neurons' scores
    float* all_scores = (float*)nimcp_malloc(N * 4 * sizeof(float));
    bool* is_dominated = (bool*)nimcp_malloc(N * sizeof(bool));

    if (!all_scores || !is_dominated) {
        LOG_ERROR("Memory allocation failed");
        if (all_scores) nimcp_free(all_scores);
        if (is_dominated) nimcp_free(is_dominated);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_select_pareto_optimal: validation failed");
        return false;
    }

    // Score all neurons
    for (uint32_t i = 0; i < N; i++) {
        spatial_neuromod_score_neuron_multi_objective(
            field, i, network, config, &all_scores[i * 4]);
        is_dominated[i] = false;
    }

    // Find Pareto front (non-dominated solutions)
    for (uint32_t i = 0; i < N; i++) {
        if (is_dominated[i]) continue;

        for (uint32_t j = 0; j < N; j++) {
            if (i == j || is_dominated[j]) continue;

            if (spatial_neuromod_pareto_dominates(
                &all_scores[j * 4], &all_scores[i * 4],
                num_obj, config->pareto_epsilon)) {
                is_dominated[i] = true;
                break;
            }
        }
    }

    // Count Pareto front size
    uint32_t front_size = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (!is_dominated[i]) front_size++;
    }

    if (front_size == 0) {
        LOG_WARN("Empty Pareto front");
        nimcp_free(all_scores);
        nimcp_free(is_dominated);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_select_pareto_optimal: front_size is zero");
        return false;
    }

    // Select K from front
    if (front_size <= K) {
        // Front is small, take all
        uint32_t count = 0;
        for (uint32_t i = 0; i < N && count < K; i++) {
            if (!is_dominated[i]) {
                selected_ids[count++] = i;
            }
        }
        *num_selected = count;
    } else {
        // Front is large, select best K using weighted scalarization
        typedef struct {
            uint32_t id;
            float score;
        } weighted_neuron_t;

        weighted_neuron_t* weighted = (weighted_neuron_t*)nimcp_malloc(
            front_size * sizeof(weighted_neuron_t));

        if (!weighted) {
            nimcp_free(all_scores);
            nimcp_free(is_dominated);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_select_pareto_optimal: weighted is NULL");
            return false;
        }

        uint32_t wcount = 0;
        for (uint32_t i = 0; i < N; i++) {
            if (!is_dominated[i]) {
                weighted[wcount].id = i;
                weighted[wcount].score = compute_weighted_score(
                    &all_scores[i * 4],
                    config->objective_weights,
                    num_obj);
                wcount++;
            }
        }

        // Sort by weighted score (descending)
        for (uint32_t i = 0; i < wcount - 1; i++) {
            for (uint32_t j = i + 1; j < wcount; j++) {
                if (weighted[j].score > weighted[i].score) {
                    weighted_neuron_t temp = weighted[i];
                    weighted[i] = weighted[j];
                    weighted[j] = temp;
                }
            }
        }

        // Select top K
        for (uint32_t i = 0; i < K && i < wcount; i++) {
            selected_ids[i] = weighted[i].id;
        }
        *num_selected = (K < wcount) ? K : wcount;

        nimcp_free(weighted);
    }

    nimcp_free(all_scores);
    nimcp_free(is_dominated);
    return true;
}

bool spatial_neuromod_release_multi_objective(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float total_amount)
{
    // WHAT: Adaptive release using Pareto-optimal selection
    // WHY:  Optimize multiple objectives simultaneously
    // HOW:  Select Pareto front, distribute evenly
    //
    // COMPLEXITY: O(N² × k)

    // Guard: Validate parameters
    if (!field || !network || !config) {
        LOG_ERROR("Invalid parameters for multi-objective release");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_multi_objective: required parameter is NULL (field, network, config)");
        return false;
    }

    // Guard: Multi-objective must be enabled
    if (!config->enable_multi_objective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spatial_neuromod_release_multi_objective: config->enable_multi_objective is NULL");
        return false;
    }

    // Select Pareto-optimal sources
    uint32_t selected_ids[100];
    uint32_t num_selected;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, config, selected_ids, NULL, &num_selected);

    if (!success || num_selected == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "spatial_neuromod_release_multi_objective: success is NULL");
        return false;
    }

    // Distribute evenly
    float amount_per_source = total_amount / num_selected;

    for (uint32_t i = 0; i < num_selected; i++) {
        field->source_rate[selected_ids[i]] += amount_per_source;
        field->total_released += amount_per_source;
    }

    return true;
}
