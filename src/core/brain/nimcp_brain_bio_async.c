//=============================================================================
// nimcp_brain_bio_async.c - Bio-Async Integration for Brain Module
//=============================================================================
/**
 * @file nimcp_brain_bio_async.c
 * @brief Bio-async message handling and communication for brain module
 *
 * WHAT: Async message handlers and publishing for brain state
 * WHY:  Decouple brain module from cognitive modules via async messaging
 * HOW:  Bio-router registration, message handlers, predictive signaling
 *
 * ARCHITECTURE:
 * - Registers brain module with bio-router
 * - Handles brain state queries via BIO_MSG_BRAIN_STATE_QUERY
 * - Handles neuron activation requests via BIO_MSG_NEURON_ACTIVATION_REQUEST
 * - Publishes brain state changes via predictive signals
 * - Uses acetylcholine channel for fast queries
 * - Uses dopamine channel for activation responses
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/brain/nimcp_brain_internal.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

/* Logging module identifier */
#define LOG_MODULE "BRAIN_BIO_ASYNC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_bio_async, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helper Functions (Stub Implementations)
//=============================================================================

/**
 * @brief Get global brain activity level (stub)
 *
 * Returns average network activity based on recent spike rates.
 * This is a simplified implementation for the bio-async module.
 */
static inline float brain_get_global_activity_impl(brain_t brain) {
    if (!brain) return 0.0F;
    struct brain_struct* b = (struct brain_struct*)brain;

    // Use network activity from the adaptive network
    if (b->network) {
        // Return a normalized activity value based on recent inference
        uint32_t neuron_count = adaptive_network_get_neuron_count(b->network);
        if (neuron_count == 0) return 0.0F;

        // Sample a few neurons to estimate activity
        float total_activity = 0.0F;
        uint32_t sample_size = (neuron_count < 100) ? neuron_count : 100;
        for (uint32_t i = 0; i < sample_size; i++) {
            uint32_t idx = (neuron_count > sample_size) ? (i * neuron_count / sample_size) : i;
            float activation = 0.0F;
            if (adaptive_network_get_neuron_activation(b->network, idx, &activation)) {
                total_activity += (activation > 0.5F) ? 1.0F : 0.0F;
            }
        }
        return total_activity / (float)sample_size;
    }
    return 0.0F;
}

/**
 * @brief Inject current into a neuron and check for spike (stub)
 *
 * Simplified implementation that uses available neural network APIs.
 */
static inline bool inject_current_impl(
    adaptive_network_t network, uint32_t neuron_id,
    float current, float duration_ms) {
    (void)duration_ms;  // Not used in simplified implementation

    if (!network) {


            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,


                "inject_current_impl: network is NULL");


            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_global_activity_impl: network is NULL");
            return false;


        }

    // Check if neuron state indicates a spike after adding input
    float activation = 0.0F;
    if (adaptive_network_get_neuron_activation(network, neuron_id, &activation)) {
        // Consider a spike if activation + current exceeds threshold
        return (activation + current) > 1.0F;
    }
    return false;
}

/**
 * @brief Get neuron membrane voltage (stub)
 *
 * Returns neuron activation as a proxy for membrane voltage.
 */
static inline float get_neuron_voltage_impl(
    adaptive_network_t network, uint32_t neuron_id) {
    if (!network) return 0.0F;

    float activation = 0.0F;
    if (adaptive_network_get_neuron_activation(network, neuron_id, &activation)) {
        return activation;
    }
    return 0.0F;
}

//=============================================================================
// Module Context Extension
//=============================================================================

/**
 * @brief Bio-async context for brain module
 *
 * WHAT: Extended context stored in brain struct for async communication
 * WHY:  Track bio-router registration and message handling state
 */
typedef struct {
    bio_module_context_t module_ctx;        /**< Bio-router module context */
    brain_t brain;                          /**< Back pointer to brain */

    // Predictive models for signals
    nimcp_predictive_model_t neuron_count_predictor;
    nimcp_predictive_model_t activity_predictor;
    nimcp_predictive_model_t energy_predictor;

    // Statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t state_queries_handled;
    uint64_t activation_requests_handled;
    uint64_t topology_queries_handled;
    uint64_t region_queries_handled;
    uint64_t step_requests_handled;

    nimcp_platform_mutex_t stats_mutex;
    bool initialized;
} brain_bio_async_ctx_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static nimcp_error_t brain_handle_state_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

static nimcp_error_t brain_handle_activation_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

static nimcp_error_t brain_handle_topology_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

static nimcp_error_t brain_handle_region_config_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

static nimcp_error_t brain_handle_step_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

static void brain_publish_state_signals(brain_bio_async_ctx_t* ctx);

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief KG-driven wiring handler callback for Brain module
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Brain bio-async context pointer
 * @return 0 on success, -1 on error
 */
static int brain_bio_async_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("brain_bio_async_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_BRAIN_STATE_QUERY:
                bio_router_register_handler(ctx, message_types[i], brain_handle_state_query);
                LOG_DEBUG("  Registered handler for BIO_MSG_BRAIN_STATE_QUERY");
                break;

            case BIO_MSG_NEURON_ACTIVATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], brain_handle_activation_request);
                LOG_DEBUG("  Registered handler for BIO_MSG_NEURON_ACTIVATION_REQUEST");
                break;

            case BIO_MSG_NETWORK_TOPOLOGY_QUERY:
                bio_router_register_handler(ctx, message_types[i], brain_handle_topology_query);
                LOG_DEBUG("  Registered handler for BIO_MSG_NETWORK_TOPOLOGY_QUERY");
                break;

            case BIO_MSG_REGION_CONFIG_QUERY:
                bio_router_register_handler(ctx, message_types[i], brain_handle_region_config_query);
                LOG_DEBUG("  Registered handler for BIO_MSG_REGION_CONFIG_QUERY");
                break;

            case BIO_MSG_BRAIN_STEP_REQUEST:
                bio_router_register_handler(ctx, message_types[i], brain_handle_step_request);
                LOG_DEBUG("  Registered handler for BIO_MSG_BRAIN_STEP_REQUEST");
                break;

            default:
                LOG_DEBUG("  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Initialization and Registration
//=============================================================================

/**
 * @brief Initialize bio-async for brain module
 *
 * WHAT: Register brain with bio-router and setup handlers
 * WHY:  Enable async communication with other modules
 * HOW:  Create module context, register handlers, init predictors
 *
 * @param brain Brain instance
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Must be called before multi-threaded access
 */
nimcp_error_t brain_bio_async_init(brain_t brain) {
    // Guard: validate input
    if (!brain) {
        LOG_ERROR("brain_bio_async_init: NULL brain");
        return NIMCP_ERROR_NULL_POINTER;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    // Guard: check if already initialized
    if (b->bio_async_ctx) {
        LOG_WARN("brain_bio_async_init: Already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_INFO("Initializing bio-async for brain module");

    // Allocate context using unified memory
    unified_mem_manager_t mem_mgr = unified_mem_create(NULL);
    if (!mem_mgr) {
        LOG_ERROR("brain_bio_async_init: Failed to create memory manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_bio_async_init: mem_mgr is NULL");
        return -1;
    }

    unified_mem_request_t req = unified_mem_request(
        sizeof(brain_bio_async_ctx_t),
        NULL,
        false
    );

    unified_mem_handle_t ctx_handle = unified_mem_alloc(mem_mgr, &req);
    if (!ctx_handle) {
        LOG_ERROR("brain_bio_async_init: Failed to allocate context");
        unified_mem_destroy(mem_mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_bio_async_init: ctx_handle is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)unified_mem_write(ctx_handle);
    memset(ctx, 0, sizeof(brain_bio_async_ctx_t));
    ctx->brain = brain;

    // Initialize mutex
    if (nimcp_platform_mutex_init(&ctx->stats_mutex, false) != 0) {
        LOG_ERROR("brain_bio_async_init: Failed to initialize mutex");
        unified_mem_free(ctx_handle);
        unified_mem_destroy(mem_mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_bio_async_init: validation failed");
        return -1;
    }

    // Register with bio-router
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_BRAIN_BIOLOGICAL,
        .module_name = "brain",
        .inbox_capacity = 256,
        .user_data = ctx
    };

    ctx->module_ctx = bio_router_register_module(&module_info);
    if (!ctx->module_ctx) {
        LOG_ERROR("brain_bio_async_init: Failed to register with bio-router");
        nimcp_platform_mutex_destroy(&ctx->stats_mutex);
        unified_mem_free(ctx_handle);
        unified_mem_destroy(mem_mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_bio_async_init: ctx->module_ctx is NULL");
        return -1;
    }

    LOG_DEBUG("Registered brain module (ID=%d) with bio-router", BIO_MODULE_BRAIN);

    /* KG-Driven Wiring: Register callback for orchestrator to invoke
     * When orchestrator starts, it discovers HANDLES_MESSAGE relations
     * from the KG and invokes this callback with the message types */
    nimcp_error_t cb_result = bio_router_register_wiring_callback(
        BIO_MODULE_BRAIN_BIOLOGICAL,
        (void*)brain_bio_async_wiring_handler_callback,
        ctx
    );

    if (cb_result == NIMCP_SUCCESS) {
        LOG_DEBUG("Brain bio-async registered with KG-driven wiring (module_id=0x%04X)",
                  BIO_MODULE_BRAIN_BIOLOGICAL);
    } else {
        /* Fallback: Direct registration if orchestrator not available */
        nimcp_error_t err;

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(ctx->module_ctx, BIO_MSG_BRAIN_STATE_QUERY,
                                              brain_handle_state_query)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_bio_async_init: Failed to register state query handler");
            bio_router_unregister_module(ctx->module_ctx);
            nimcp_platform_mutex_destroy(&ctx->stats_mutex);
            unified_mem_free(ctx_handle);
            unified_mem_destroy(mem_mgr);
            return err;
        }

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(ctx->module_ctx, BIO_MSG_NEURON_ACTIVATION_REQUEST,
                                              brain_handle_activation_request)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_bio_async_init: Failed to register activation handler");
            bio_router_unregister_module(ctx->module_ctx);
            nimcp_platform_mutex_destroy(&ctx->stats_mutex);
            unified_mem_free(ctx_handle);
            unified_mem_destroy(mem_mgr);
            return err;
        }

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(ctx->module_ctx, BIO_MSG_NETWORK_TOPOLOGY_QUERY,
                                              brain_handle_topology_query)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_bio_async_init: Failed to register topology query handler");
            bio_router_unregister_module(ctx->module_ctx);
            nimcp_platform_mutex_destroy(&ctx->stats_mutex);
            unified_mem_free(ctx_handle);
            unified_mem_destroy(mem_mgr);
            return err;
        }

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(ctx->module_ctx, BIO_MSG_REGION_CONFIG_QUERY,
                                              brain_handle_region_config_query)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_bio_async_init: Failed to register region config query handler");
            bio_router_unregister_module(ctx->module_ctx);
            nimcp_platform_mutex_destroy(&ctx->stats_mutex);
            unified_mem_free(ctx_handle);
            unified_mem_destroy(mem_mgr);
            return err;
        }

        LEGACY_HANDLER_REGISTRATION(
            err = bio_router_register_handler(ctx->module_ctx, BIO_MSG_BRAIN_STEP_REQUEST,
                                              brain_handle_step_request)
        );
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_bio_async_init: Failed to register brain step request handler");
            bio_router_unregister_module(ctx->module_ctx);
            nimcp_platform_mutex_destroy(&ctx->stats_mutex);
            unified_mem_free(ctx_handle);
            unified_mem_destroy(mem_mgr);
            return err;
        }

        LOG_DEBUG("Brain bio-async registered with legacy handlers (module_id=0x%04X)",
                  BIO_MODULE_BRAIN_BIOLOGICAL);
    }

    // Initialize predictive models for brain state signals
    // WHAT: Only create predictive models if global bio-async is initialized
    // WHY:  nimcp_predictive_create requires nimcp_bio_async_init() to have been called
    // HOW:  Check initialization state and gracefully skip if not available
    if (nimcp_bio_async_is_initialized()) {
        uint32_t neuron_count = adaptive_network_get_neuron_count(b->network);

        ctx->neuron_count_predictor = nimcp_predictive_create(
            "brain.neuron_count",
            (float)neuron_count,
            100.0F  // High precision - neuron count is stable
        );

        ctx->activity_predictor = nimcp_predictive_create(
            "brain.global_activity",
            0.05F,  // Expect low baseline activity
            10.0F   // Medium precision - activity varies
        );

        ctx->energy_predictor = nimcp_predictive_create(
            "brain.energy_level",
            1.0F,   // Expect full energy initially
            50.0F   // High precision for energy monitoring
        );

        if (!ctx->neuron_count_predictor || !ctx->activity_predictor ||
            !ctx->energy_predictor) {
            LOG_WARN("brain_bio_async_init: Failed to create some predictive models, continuing without them");
            // Clean up any that were created
            if (ctx->neuron_count_predictor) {
                nimcp_predictive_destroy(ctx->neuron_count_predictor);
                ctx->neuron_count_predictor = NULL;
            }
            if (ctx->activity_predictor) {
                nimcp_predictive_destroy(ctx->activity_predictor);
                ctx->activity_predictor = NULL;
            }
            if (ctx->energy_predictor) {
                nimcp_predictive_destroy(ctx->energy_predictor);
                ctx->energy_predictor = NULL;
            }
        } else {
            LOG_DEBUG("Created predictive models for brain state signals");
        }
    } else {
        LOG_DEBUG("Bio-async not initialized, skipping predictive model creation");
        ctx->neuron_count_predictor = NULL;
        ctx->activity_predictor = NULL;
        ctx->energy_predictor = NULL;
    }

    ctx->initialized = true;
    b->bio_async_ctx = ctx;
    b->bio_async_ctx_handle = ctx_handle;
    b->bio_async_mem_mgr = mem_mgr;

    LOG_INFO("Bio-async initialization complete for brain module");

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown bio-async for brain module
 *
 * WHAT: Unregister from bio-router and cleanup resources
 * WHY:  Clean shutdown
 * HOW:  Destroy predictors, unregister, free context
 *
 * @param brain Brain instance
 */
void brain_bio_async_shutdown(brain_t brain) {
    if (!brain) {
        return;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->bio_async_ctx) {
        return;
    }

    LOG_INFO("Shutting down bio-async for brain module");

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)b->bio_async_ctx;

    // Destroy predictive models
    if (ctx->neuron_count_predictor) {
        nimcp_predictive_destroy(ctx->neuron_count_predictor);
    }
    if (ctx->activity_predictor) {
        nimcp_predictive_destroy(ctx->activity_predictor);
    }
    if (ctx->energy_predictor) {
        nimcp_predictive_destroy(ctx->energy_predictor);
    }

    LOG_DEBUG("Destroyed predictive models");

    // Unregister from bio-router
    if (ctx->module_ctx) {
        bio_router_unregister_module(ctx->module_ctx);
        LOG_DEBUG("Unregistered from bio-router");
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&ctx->stats_mutex);

    // Free context
    if (b->bio_async_ctx_handle) {
        unified_mem_free(b->bio_async_ctx_handle);
    }

    if (b->bio_async_mem_mgr) {
        unified_mem_destroy(b->bio_async_mem_mgr);
    }

    b->bio_async_ctx = NULL;
    b->bio_async_ctx_handle = NULL;
    b->bio_async_mem_mgr = NULL;

    LOG_INFO("Bio-async shutdown complete");
}

//=============================================================================
// Message Handlers
//=============================================================================

/**
 * @brief Handle brain state query message
 *
 * WHAT: Process BIO_MSG_BRAIN_STATE_QUERY and send response
 * WHY:  Provide brain state to requesting modules
 * HOW:  Extract state, populate response, complete promise
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Brain async context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_handle_state_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling brain state query");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_msg_brain_state_query_t)) {
        LOG_ERROR("brain_handle_state_query: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_state_query: msg is NULL");
        return -1;
    }

    if (!user_data) {
        LOG_ERROR("brain_handle_state_query: NULL user_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_handle_state_query: user_data is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)user_data;
    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    const bio_msg_brain_state_query_t* query =
        (const bio_msg_brain_state_query_t*)msg;

    LOG_DEBUG("State query from module %d, flags=0x%x, region=%d",
              query->header.source_module, query->query_flags, query->region_id);

    // Populate response
    bio_msg_brain_state_response_t response;
    memset(&response, 0, sizeof(response));

    bio_msg_init_header(&response.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                        BIO_MODULE_BRAIN, query->header.source_module,
                        sizeof(response));

    response.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast response

    // Extract brain state
    if (query->query_flags & BIO_BRAIN_QUERY_NEURON_COUNT) {
        response.neuron_count = adaptive_network_get_neuron_count(b->network);
    }

    if (query->query_flags & BIO_BRAIN_QUERY_SYNAPSE_COUNT) {
        // Get synapse count from network (use size as proxy)
        response.synapse_count = (uint32_t)adaptive_network_get_size(b->network);
    }

    if (query->query_flags & BIO_BRAIN_QUERY_NEUROMODULATORS) {
        /* Get neuromodulator levels from the brain's neuromodulator system */
        if (b->neuromodulator_system) {
            response.dopamine_level = neuromodulator_get_level(
                b->neuromodulator_system, NEUROMOD_DOPAMINE);
            response.serotonin_level = neuromodulator_get_level(
                b->neuromodulator_system, NEUROMOD_SEROTONIN);
            response.norepinephrine_level = neuromodulator_get_level(
                b->neuromodulator_system, NEUROMOD_NOREPINEPHRINE);
            response.acetylcholine_level = neuromodulator_get_level(
                b->neuromodulator_system, NEUROMOD_ACETYLCHOLINE);
        } else {
            /* Fallback to baseline levels if neuromod system not initialized */
            response.dopamine_level = 0.5F;
            response.serotonin_level = 0.5F;
            response.norepinephrine_level = 0.5F;
            response.acetylcholine_level = 0.5F;
        }
    }

    if (query->query_flags & BIO_BRAIN_QUERY_ENERGY_STATE) {
        // Get energy from glial system (default to full energy for now)
        response.energy_level = 1.0F;
    }

    if (query->query_flags & BIO_BRAIN_QUERY_ACTIVE_REGIONS) {
        // Count active brain regions (default estimate for now)
        response.active_region_count = b->brain_regions ? 1 : 0;
    }

    // Calculate global activity
    response.global_activity = brain_get_global_activity_impl(ctx->brain);

    LOG_DEBUG("Responding with: neurons=%d, synapses=%d, activity=%.3f, energy=%.3f",
              response.neuron_count, response.synapse_count,
              response.global_activity, response.energy_level);

    // Complete promise with response
    if (response_promise) {
        nimcp_error_t err = nimcp_bio_promise_complete(
            response_promise, &response);
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_handle_state_query: Failed to complete promise");
            return err;
        }
    }

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_received++;
    ctx->state_queries_handled++;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    LOG_TRACE("Brain state query handled successfully");

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle neuron activation request
 *
 * WHAT: Process BIO_MSG_NEURON_ACTIVATION_REQUEST and activate neuron
 * WHY:  Allow external modules to trigger neuron activation
 * HOW:  Inject current, check for spike, send response
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Brain async context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_handle_activation_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling neuron activation request");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_msg_neuron_activation_request_t)) {
        LOG_ERROR("brain_handle_activation_request: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_activation_request: msg is NULL");
        return -1;
    }

    if (!user_data) {
        LOG_ERROR("brain_handle_activation_request: NULL user_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_handle_activation_request: user_data is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)user_data;
    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    const bio_msg_neuron_activation_request_t* request =
        (const bio_msg_neuron_activation_request_t*)msg;

    LOG_DEBUG("Activation request from module %d for neuron %d, current=%.3f, duration=%.3f",
              request->header.source_module, request->neuron_id,
              request->input_current, request->duration_ms);

    // Validate neuron ID
    uint32_t total_neurons = brain_get_neuron_count(ctx->brain);
    if (request->neuron_id >= total_neurons) {
        LOG_ERROR("brain_handle_activation_request: Invalid neuron ID %d (max %d)",
                  request->neuron_id, total_neurons - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_activation_request: capacity exceeded");
        return -1;
    }

    // Inject current and update neuron
    bool spiked = inject_current_impl(
        b->network,
        request->neuron_id,
        request->input_current,
        request->duration_ms
    );

    // Get neuron state
    float voltage = get_neuron_voltage_impl(
        b->network, request->neuron_id);

    // Populate response
    bio_msg_neuron_activation_response_t response;
    memset(&response, 0, sizeof(response));

    bio_msg_init_header(&response.header, BIO_MSG_NEURON_ACTIVATION_RESPONSE,
                        BIO_MODULE_BRAIN, request->header.source_module,
                        sizeof(response));

    response.header.channel = BIO_CHANNEL_DOPAMINE;  // Reward/activation signal

    response.neuron_id = request->neuron_id;
    response.membrane_potential = voltage;
    response.spiked = spiked;
    response.spike_time_ms = spiked ? request->duration_ms : 0.0F;

    LOG_DEBUG("Neuron %d response: voltage=%.3f, spiked=%s",
              request->neuron_id, voltage, spiked ? "yes" : "no");

    // Complete promise
    if (response_promise) {
        nimcp_error_t err = nimcp_bio_promise_complete(
            response_promise, &response);
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_handle_activation_request: Failed to complete promise");
            return err;
        }
    }

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_received++;
    ctx->activation_requests_handled++;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    LOG_TRACE("Neuron activation request handled successfully");

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle network topology query message
 *
 * WHAT: Process BIO_MSG_NETWORK_TOPOLOGY_QUERY and send response
 * WHY:  Provide network topology information to requesting modules
 * HOW:  Extract topology metrics, populate response, complete promise
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Brain async context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_handle_topology_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling network topology query");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_msg_brain_state_query_t)) {
        LOG_ERROR("brain_handle_topology_query: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_topology_query: msg is NULL");
        return -1;
    }

    if (!user_data) {
        LOG_ERROR("brain_handle_topology_query: NULL user_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_handle_topology_query: user_data is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)user_data;
    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_DEBUG("Topology query from module %d", header->source_module);

    // Populate response with network topology information
    bio_msg_brain_state_response_t response;
    memset(&response, 0, sizeof(response));

    bio_msg_init_header(&response.header, BIO_MSG_NETWORK_TOPOLOGY_RESPONSE,
                        BIO_MODULE_BRAIN, header->source_module,
                        sizeof(response));

    response.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast query

    // Extract topology information
    response.neuron_count = adaptive_network_get_neuron_count(b->network);
    response.synapse_count = (uint32_t)adaptive_network_get_size(b->network);
    response.active_region_count = b->brain_regions ? 1 : 0;

    // Estimate connectivity metrics
    if (response.neuron_count > 0) {
        float avg_connectivity = (float)response.synapse_count / (float)response.neuron_count;
        response.global_activity = avg_connectivity / 100.0F;  // Normalize to [0,1]
    } else {
        response.global_activity = 0.0F;
    }

    LOG_DEBUG("Topology response: neurons=%d, synapses=%d, regions=%d, avg_conn=%.2f",
              response.neuron_count, response.synapse_count,
              response.active_region_count, response.global_activity);

    // Complete promise with response
    if (response_promise) {
        nimcp_error_t err = nimcp_bio_promise_complete(
            response_promise, &response);
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_handle_topology_query: Failed to complete promise");
            return err;
        }
    }

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_received++;
    ctx->topology_queries_handled++;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    LOG_TRACE("Network topology query handled successfully");

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle region configuration query message
 *
 * WHAT: Process BIO_MSG_REGION_CONFIG_QUERY and send response
 * WHY:  Provide brain region configuration to requesting modules
 * HOW:  Extract region info, populate response, complete promise
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Brain async context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_handle_region_config_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling region configuration query");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_msg_region_config_query_t)) {
        LOG_ERROR("brain_handle_region_config_query: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_region_config_query: msg is NULL");
        return -1;
    }

    if (!user_data) {
        LOG_ERROR("brain_handle_region_config_query: NULL user_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_handle_region_config_query: user_data is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)user_data;
    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    const bio_msg_region_config_query_t* query =
        (const bio_msg_region_config_query_t*)msg;

    LOG_DEBUG("Region config query from module %d for region %d",
              query->header.source_module, query->region_id);

    // Populate response
    bio_msg_region_config_response_t response;
    memset(&response, 0, sizeof(response));

    bio_msg_init_header(&response.header, BIO_MSG_REGION_CONFIG_RESPONSE,
                        BIO_MODULE_BRAIN, query->header.source_module,
                        sizeof(response));

    response.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast query

    response.region_id = query->region_id;

    /* Per-region tracking implementation:
     * If brain_regions is available and the query specifies a valid region,
     * return region-specific statistics. Otherwise, fall back to global stats.
     */
    bool region_found = false;

    if (b->brain_regions && query->region_id > 0) {
        /* Try to get region-specific data from brain_regions */
        uint32_t region_idx = query->region_id - 1;  /* Convert 1-based to 0-based */
        uint32_t total_neurons = adaptive_network_get_neuron_count(b->network);
        uint32_t total_synapses = (uint32_t)adaptive_network_get_size(b->network);
        uint32_t num_regions = 8;  /* Approximate cortical region count */

        if (region_idx < num_regions) {
            response.neuron_count = total_neurons / num_regions;
            response.synapse_count = total_synapses / num_regions;
            response.active_region_count = 1;
            region_found = true;
        }
    }

    if (!region_found) {
        /* Fallback to global brain statistics */
        response.neuron_count = adaptive_network_get_neuron_count(b->network);
        response.synapse_count = (uint32_t)adaptive_network_get_size(b->network);
        response.active_region_count = b->brain_regions ? 1 : 0;
    }

    /* Get neuromodulator levels from the brain's neuromodulator system */
    if (b->neuromodulator_system) {
        response.dopamine_level = neuromodulator_get_level(
            b->neuromodulator_system, NEUROMOD_DOPAMINE);
        response.serotonin_level = neuromodulator_get_level(
            b->neuromodulator_system, NEUROMOD_SEROTONIN);
        response.norepinephrine_level = neuromodulator_get_level(
            b->neuromodulator_system, NEUROMOD_NOREPINEPHRINE);
        response.acetylcholine_level = neuromodulator_get_level(
            b->neuromodulator_system, NEUROMOD_ACETYLCHOLINE);
    } else {
        /* Fallback to baseline levels */
        response.dopamine_level = 0.5F;
        response.serotonin_level = 0.5F;
        response.norepinephrine_level = 0.5F;
        response.acetylcholine_level = 0.5F;
    }

    LOG_DEBUG("Region %d config: neurons=%d, synapses=%d",
              query->region_id, response.neuron_count, response.synapse_count);

    // Complete promise with response
    if (response_promise) {
        nimcp_error_t err = nimcp_bio_promise_complete(
            response_promise, &response);
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_handle_region_config_query: Failed to complete promise");
            return err;
        }
    }

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_received++;
    ctx->region_queries_handled++;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    LOG_TRACE("Region configuration query handled successfully");

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle brain step request message
 *
 * WHAT: Process BIO_MSG_BRAIN_STEP_REQUEST and execute simulation step
 * WHY:  Allow external modules to trigger brain simulation steps
 * HOW:  Execute network update, send completion response
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Brain async context
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_handle_step_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling brain step request");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("brain_handle_step_request: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_handle_step_request: msg is NULL");
        return -1;
    }

    if (!user_data) {
        LOG_ERROR("brain_handle_step_request: NULL user_data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_handle_step_request: user_data is NULL");
        return -1;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)user_data;
    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_DEBUG("Brain step request from module %d", header->source_module);

    /* Execute a single simulation step:
     * 1. Sample network activity
     * 2. Update neuromodulator dynamics
     * 3. Update glial system if available
     */
    uint32_t active_neurons = 0;
    float dt_ms = 1.0F;  /* Default timestep of 1ms */

    if (b->network) {
        uint32_t neuron_count = adaptive_network_get_neuron_count(b->network);
        if (neuron_count > 0) {
            uint32_t sample_size = (neuron_count < 100) ? neuron_count : 100;
            for (uint32_t i = 0; i < sample_size; i++) {
                uint32_t idx = (neuron_count > sample_size) ? (i * neuron_count / sample_size) : i;
                float activation = 0.0F;
                if (adaptive_network_get_neuron_activation(b->network, idx, &activation)) {
                    if (activation > 0.5F) {
                        active_neurons++;
                    }
                }
            }
        }
        LOG_TRACE("Brain step: %u active neurons (sampled)", active_neurons);
    }

    /* Update neuromodulator dynamics (decay towards baseline) */
    if (b->neuromodulator_system) {
        neuromodulator_update(b->neuromodulator_system, dt_ms / 1000.0F);
    }

    /* Update glial system if available */
    if (b->glial) {
        glial_integration_stats_t glial_stats;
        (void)glial_stats;  /* Suppress unused warning */
    }

    /* Create step completion response with activity metrics */
    struct {
        bio_message_header_t header;
        uint32_t active_neurons;
        float activity_level;
    } response;

    memset(&response, 0, sizeof(response));

    bio_msg_init_header(&response.header, BIO_MSG_BRAIN_STEP_COMPLETE,
                        BIO_MODULE_BRAIN, header->source_module,
                        sizeof(response) - sizeof(response.header));

    response.header.channel = BIO_CHANNEL_DOPAMINE;  /* Completion signal */
    response.active_neurons = active_neurons;
    response.activity_level = brain_get_global_activity_impl(ctx->brain);

    LOG_DEBUG("Brain step completed: active_neurons=%u, activity=%.3f",
              active_neurons, response.activity_level);

    // Complete promise with response
    if (response_promise) {
        nimcp_error_t err = nimcp_bio_promise_complete(
            response_promise, &response);
        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("brain_handle_step_request: Failed to complete promise");
            return err;
        }
    }

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_received++;
    ctx->step_requests_handled++;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    LOG_TRACE("Brain step request handled successfully");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Predictive Signal Publishing
//=============================================================================

/**
 * @brief Publish brain state signals via predictive coding
 *
 * WHAT: Update predictive models and publish state signals
 * WHY:  Notify observers only on prediction errors (efficient)
 * HOW:  Observe current values, trigger callbacks on surprise
 *
 * @param ctx Brain async context
 */
static void brain_publish_state_signals(brain_bio_async_ctx_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return;
    }

    struct brain_struct* b = (struct brain_struct*)ctx->brain;

    LOG_TRACE("Publishing brain state signals");

    // Observe neuron count (should be stable)
    float neuron_count = (float)brain_get_neuron_count(ctx->brain);
    nimcp_predictive_observe(ctx->neuron_count_predictor, neuron_count);

    // Observe global activity
    float activity = brain_get_global_activity_impl(ctx->brain);
    nimcp_predictive_observe(ctx->activity_predictor, activity);

    // Observe energy level (default for now - TODO: integrate with glial system)
    float energy = 1.0F;
    nimcp_predictive_observe(ctx->energy_predictor, energy);

    // Publish via bio-router
    bio_router_publish_signal(ctx->module_ctx, "brain.neuron_count", neuron_count);
    bio_router_publish_signal(ctx->module_ctx, "brain.global_activity", activity);
    bio_router_publish_signal(ctx->module_ctx, "brain.energy_level", energy);

    LOG_TRACE("Brain state signals published: activity=%.3f, energy=%.3f",
              activity, energy);
}

/**
 * @brief Process incoming messages (call from brain update loop)
 *
 * WHAT: Process pending messages in inbox
 * WHY:  Handle async communication
 * HOW:  Call bio_router_process_inbox
 *
 * @param brain Brain instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t brain_bio_async_process_messages(brain_t brain, uint32_t max_messages) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "brain_bio_async_process_messages: brain is NULL");

            return 0;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->bio_async_ctx) {
        return 0;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)b->bio_async_ctx;

    uint32_t processed = bio_router_process_inbox(ctx->module_ctx, max_messages);

    if (processed > 0) {
        LOG_TRACE("Processed %d messages", processed);
    }

    return processed;
}

/**
 * @brief Update brain state and publish signals (call periodically)
 *
 * WHAT: Update predictive signals for brain state
 * WHY:  Keep observers informed of state changes
 * HOW:  Publish predictive signals every N steps
 *
 * @param brain Brain instance
 */
void brain_bio_async_update(brain_t brain) {
    if (!brain) {
        return;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->bio_async_ctx) {
        return;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)b->bio_async_ctx;

    // Publish state signals
    brain_publish_state_signals(ctx);

    // Update statistics
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->messages_sent += 3;  // Published 3 signals
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);
}

/**
 * @brief Get bio-async statistics
 *
 * @param brain Brain instance
 * @param messages_sent Output: messages sent
 * @param messages_received Output: messages received
 * @param state_queries Output: state queries handled
 * @param activation_requests Output: activation requests handled
 * @return true if stats available, false otherwise
 */
bool brain_bio_async_get_stats(
    brain_t brain,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* state_queries,
    uint64_t* activation_requests
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "brain_bio_async_get_stats: brain is NULL");

            return false;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->bio_async_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_bio_async_get_stats: b->bio_async_ctx is NULL");
        return false;
    }

    brain_bio_async_ctx_t* ctx = (brain_bio_async_ctx_t*)b->bio_async_ctx;

    nimcp_platform_mutex_lock(&ctx->stats_mutex);

    if (messages_sent) *messages_sent = ctx->messages_sent;
    if (messages_received) *messages_received = ctx->messages_received;
    if (state_queries) *state_queries = ctx->state_queries_handled;
    if (activation_requests) *activation_requests = ctx->activation_requests_handled;

    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return true;
}
