#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neuron_model_bio_async.c - Bio-Async Integration for Neuron Model
//=============================================================================
/**
 * @file nimcp_neuron_model_bio_async.c
 * @brief Bio-async spike event publishing for neuron models
 *
 * WHAT: Async spike event publishing and activation request handling
 * WHY:  Notify observers of neuron spikes without tight coupling
 * HOW:  Publish BIO_MSG_NEURON_ACTIVATION_RESPONSE on spike events
 *
 * ARCHITECTURE:
 * - Registers neuron model module with bio-router
 * - Publishes spike events via dopamine channel (reward/activation)
 * - Handles activation requests from external modules
 * - Logs all state changes comprehensively
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "core/neuron_models/nimcp_neuron_model.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/neuron_models/nimcp_neuron_model_internal.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuron_model_bio_async)

//=============================================================================
// Global Bio-Async Context
//=============================================================================

/**
 * @brief Global bio-async context for neuron model module
 *
 * WHAT: Singleton context for neuron model async communication
 * WHY:  Neuron models are stateless vtable dispatchers, need global context
 */
typedef struct {
    bio_module_context_t module_ctx;        /**< Bio-router module context */
    unified_mem_manager_t mem_mgr;          /**< Memory manager */

    // Statistics
    uint64_t spikes_published;
    uint64_t activation_requests_received;
    uint64_t messages_sent;
    uint64_t messages_received;

    nimcp_platform_mutex_t stats_mutex;
    nimcp_platform_mutex_t init_mutex;
    bool initialized;
} neuron_model_bio_async_ctx_t;

static neuron_model_bio_async_ctx_t g_neuron_async_ctx = {0};

//=============================================================================
// Forward Declarations
//=============================================================================

static nimcp_error_t neuron_handle_activation_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief KG-driven wiring handler callback for Neuron Model module
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Neuron model async context pointer
 * @return 0 on success, -1 on error
 */
static int neuron_model_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("neuron_model_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_NEURON_ACTIVATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], neuron_handle_activation_request);
                LOG_DEBUG("  Registered handler for BIO_MSG_NEURON_ACTIVATION_REQUEST");
                break;

            default:
                LOG_DEBUG("  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Initialization and Shutdown
//=============================================================================

/**
 * @brief Initialize bio-async for neuron model module
 *
 * WHAT: Register neuron model with bio-router
 * WHY:  Enable async spike event publishing
 * HOW:  Create global context, register handlers
 *
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
nimcp_error_t neuron_model_bio_async_init(void) {
    nimcp_platform_mutex_lock(&g_neuron_async_ctx.init_mutex);

    // Guard: check if already initialized
    if (g_neuron_async_ctx.initialized) {
        LOG_WARN("neuron_model_bio_async_init: Already initialized");
        nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
        return NIMCP_SUCCESS;
    }

    LOG_INFO("Initializing bio-async for neuron model module");

    // Initialize mutexes
    if (nimcp_platform_mutex_init(&g_neuron_async_ctx.stats_mutex, false) != 0) {
        LOG_ERROR("neuron_model_bio_async_init: Failed to initialize stats mutex");
        nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "neuron_model_bio_async_init: validation failed");
        return -1;
    }

    // Create memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    g_neuron_async_ctx.mem_mgr = unified_mem_create(&mem_config);

    if (!g_neuron_async_ctx.mem_mgr) {
        LOG_ERROR("neuron_model_bio_async_init: Failed to create memory manager");
        nimcp_platform_mutex_destroy(&g_neuron_async_ctx.stats_mutex);
        nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_model_bio_async_init: g_neuron_async_ctx is NULL");
        return -1;
    }

    LOG_DEBUG("Created unified memory manager for neuron model async");

    // Register with bio-router
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_NEURON_MODEL_BIO_ASYNC,
        .module_name = "neuron_model",
        .inbox_capacity = 128,
        .user_data = &g_neuron_async_ctx
    };

    g_neuron_async_ctx.module_ctx = bio_router_register_module(&module_info);

    if (!g_neuron_async_ctx.module_ctx) {
        LOG_ERROR("neuron_model_bio_async_init: Failed to register with bio-router");
        unified_mem_destroy(g_neuron_async_ctx.mem_mgr);
        nimcp_platform_mutex_destroy(&g_neuron_async_ctx.stats_mutex);
        nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_model_bio_async_init: g_neuron_async_ctx is NULL");
        return -1;
    }

    LOG_DEBUG("Registered neuron model module (ID=%d) with bio-router",
              BIO_MODULE_NEURON_MODEL);

    /* KG-Driven Wiring: Register callback for orchestrator to invoke
     * When orchestrator starts, it discovers HANDLES_MESSAGE relations
     * from the KG and invokes this callback with the message types */
    nimcp_error_t cb_result = bio_router_register_wiring_callback(
        BIO_MODULE_NEURON_MODEL_BIO_ASYNC,
        (void*)neuron_model_wiring_handler_callback,
        &g_neuron_async_ctx
    );

    if (cb_result == NIMCP_SUCCESS) {
        LOG_DEBUG("Neuron model bio-async registered with KG-driven wiring (module_id=0x%04X)",
                  BIO_MODULE_NEURON_MODEL_BIO_ASYNC);
    } else {
        /* Fallback: Direct registration if orchestrator not available */
        LEGACY_HANDLER_REGISTRATION(
            nimcp_error_t err = bio_router_register_handler(
                g_neuron_async_ctx.module_ctx,
                BIO_MSG_NEURON_ACTIVATION_REQUEST,
                neuron_handle_activation_request
            )
        );

        if (err != NIMCP_SUCCESS) {
            LOG_ERROR("neuron_model_bio_async_init: Failed to register handler");
            bio_router_unregister_module(g_neuron_async_ctx.module_ctx);
            unified_mem_destroy(g_neuron_async_ctx.mem_mgr);
            nimcp_platform_mutex_destroy(&g_neuron_async_ctx.stats_mutex);
            nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
            return err;
        }

        LOG_DEBUG("Neuron model bio-async registered with legacy handlers (module_id=0x%04X)",
                  BIO_MODULE_NEURON_MODEL_BIO_ASYNC);
    }

    g_neuron_async_ctx.initialized = true;

    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);

    LOG_INFO("Bio-async initialization complete for neuron model module");

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown bio-async for neuron model module
 *
 * WHAT: Unregister from bio-router and cleanup
 * WHY:  Clean shutdown
 * HOW:  Unregister, destroy memory manager
 */
void neuron_model_bio_async_shutdown(void) {
    nimcp_platform_mutex_lock(&g_neuron_async_ctx.init_mutex);

    if (!g_neuron_async_ctx.initialized) {
        nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);
        return;
    }

    LOG_INFO("Shutting down bio-async for neuron model module");

    // Unregister from bio-router
    if (g_neuron_async_ctx.module_ctx) {
        bio_router_unregister_module(g_neuron_async_ctx.module_ctx);
        g_neuron_async_ctx.module_ctx = NULL;
        LOG_DEBUG("Unregistered from bio-router");
    }

    // Destroy memory manager
    if (g_neuron_async_ctx.mem_mgr) {
        unified_mem_destroy(g_neuron_async_ctx.mem_mgr);
        g_neuron_async_ctx.mem_mgr = NULL;
    }

    // Destroy stats mutex
    nimcp_platform_mutex_destroy(&g_neuron_async_ctx.stats_mutex);

    g_neuron_async_ctx.initialized = false;

    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.init_mutex);

    LOG_INFO("Bio-async shutdown complete for neuron model module");
}

//=============================================================================
// Message Handler
//=============================================================================

/**
 * @brief Handle neuron activation request
 *
 * WHAT: Process BIO_MSG_NEURON_ACTIVATION_REQUEST
 * WHY:  Allow external modules to trigger neuron updates
 * HOW:  Update neuron state and send response
 *
 * @param msg Message pointer
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Context pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t neuron_handle_activation_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    LOG_TRACE("Handling neuron activation request in neuron model module");

    // Guard: validate inputs
    if (!msg || msg_size < sizeof(bio_msg_neuron_activation_request_t)) {
        LOG_ERROR("neuron_handle_activation_request: Invalid message");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_handle_activation_request: msg is NULL");
        return -1;
    }

    const bio_msg_neuron_activation_request_t* request =
        (const bio_msg_neuron_activation_request_t*)msg;

    LOG_DEBUG("Activation request for neuron %d: current=%.3f, duration=%.3f",
              request->neuron_id, request->input_current, request->duration_ms);

    // NOTE: This handler is for direct neuron model manipulation
    // The brain module should handle neuron activation requests for its network
    // This is a fallback/test handler

    LOG_WARN("neuron_handle_activation_request: Direct neuron model activation not implemented");
    LOG_WARN("  Neuron activation should be handled by brain module");

    // Update statistics
    nimcp_platform_mutex_lock(&g_neuron_async_ctx.stats_mutex);
    g_neuron_async_ctx.messages_received++;
    g_neuron_async_ctx.activation_requests_received++;
    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.stats_mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Spike Event Publishing
//=============================================================================

/**
 * @brief Publish spike event via bio-async
 *
 * WHAT: Broadcast neuron spike event to subscribers
 * WHY:  Notify observers of spike without tight coupling
 * HOW:  Send BIO_MSG_NEURON_ACTIVATION_RESPONSE via dopamine channel
 *
 * @param neuron_id Neuron identifier
 * @param voltage Membrane potential at spike
 * @param spike_time_ms Spike timestamp
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 * LOGGING: LOG_TRACE for each spike
 */
void neuron_model_publish_spike(
    uint32_t neuron_id,
    float voltage,
    float spike_time_ms
) {
    if (!g_neuron_async_ctx.initialized) {
        return;
    }

    LOG_TRACE("Publishing spike event for neuron %d at %.3f ms (voltage=%.3f)",
              neuron_id, spike_time_ms, voltage);

    // Create spike event message
    bio_msg_neuron_activation_response_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_NEURON_ACTIVATION_RESPONSE,
                        BIO_MODULE_NEURON_MODEL, 0,  // Broadcast (target=0)
                        sizeof(msg));

    msg.header.channel = BIO_CHANNEL_DOPAMINE;  // Reward/activation signal
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.neuron_id = neuron_id;
    msg.membrane_potential = voltage;
    msg.spiked = true;
    msg.spike_time_ms = spike_time_ms;

    // Broadcast via bio-router
    nimcp_error_t err = bio_router_broadcast(
        g_neuron_async_ctx.module_ctx,
        &msg,
        sizeof(msg)
    );

    if (err != NIMCP_SUCCESS) {
        LOG_ERROR("neuron_model_publish_spike: Failed to broadcast spike event");
        return;
    }

    LOG_TRACE("Spike event published successfully");

    // Update statistics
    nimcp_platform_mutex_lock(&g_neuron_async_ctx.stats_mutex);
    g_neuron_async_ctx.spikes_published++;
    g_neuron_async_ctx.messages_sent++;
    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.stats_mutex);
}

/**
 * @brief Publish neuron state change
 *
 * WHAT: Broadcast neuron voltage change
 * WHY:  Notify observers of voltage dynamics
 * HOW:  Send activation response without spike flag
 *
 * @param neuron_id Neuron identifier
 * @param voltage Current membrane potential
 */
void neuron_model_publish_state(uint32_t neuron_id, float voltage) {
    if (!g_neuron_async_ctx.initialized) {
        return;
    }

    LOG_TRACE("Publishing state update for neuron %d (voltage=%.3f)",
              neuron_id, voltage);

    // Create state update message
    bio_msg_neuron_activation_response_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_NEURON_ACTIVATION_RESPONSE,
                        BIO_MODULE_NEURON_MODEL, 0,
                        sizeof(msg));

    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast state update
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.neuron_id = neuron_id;
    msg.membrane_potential = voltage;
    msg.spiked = false;
    msg.spike_time_ms = 0.0F;

    // Broadcast
    bio_router_broadcast(g_neuron_async_ctx.module_ctx, &msg, sizeof(msg));

    // Update statistics
    nimcp_platform_mutex_lock(&g_neuron_async_ctx.stats_mutex);
    g_neuron_async_ctx.messages_sent++;
    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.stats_mutex);
}

//=============================================================================
// Message Processing
//=============================================================================

/**
 * @brief Process incoming messages
 *
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t neuron_model_bio_async_process_messages(uint32_t max_messages) {
    if (!g_neuron_async_ctx.initialized) {
        return 0;
    }

    uint32_t processed = bio_router_process_inbox(
        g_neuron_async_ctx.module_ctx, max_messages);

    if (processed > 0) {
        LOG_TRACE("Processed %d neuron model messages", processed);
    }

    return processed;
}

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bio-async statistics for neuron model
 *
 * @param spikes_published Output: number of spikes published
 * @param activation_requests Output: activation requests received
 * @param messages_sent Output: total messages sent
 * @param messages_received Output: total messages received
 * @return true if stats available, false otherwise
 */
bool neuron_model_bio_async_get_stats(
    uint64_t* spikes_published,
    uint64_t* activation_requests,
    uint64_t* messages_sent,
    uint64_t* messages_received
) {
    if (!g_neuron_async_ctx.initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "neuron_model_bio_async_get_stats: g_neuron_async_ctx is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&g_neuron_async_ctx.stats_mutex);

    if (spikes_published) *spikes_published = g_neuron_async_ctx.spikes_published;
    if (activation_requests) *activation_requests = g_neuron_async_ctx.activation_requests_received;
    if (messages_sent) *messages_sent = g_neuron_async_ctx.messages_sent;
    if (messages_received) *messages_received = g_neuron_async_ctx.messages_received;

    nimcp_platform_mutex_unlock(&g_neuron_async_ctx.stats_mutex);

    return true;
}
