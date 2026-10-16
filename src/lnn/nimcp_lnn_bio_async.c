/**
 * @file nimcp_lnn_bio_async.c
 * @brief Bio-async integration implementation for Liquid Neural Networks
 *
 * WHAT: Implements bio-async messaging for LNN networks
 * WHY:  Enable inter-module communication for LNN state and training
 * HOW:  Bio-router registration, message handlers, phase synchronization
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_wiring_helpers.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_bio_async)

/*=============================================================================
 * KG-Driven Wiring Infrastructure
 *===========================================================================*/

/* Forward declaration of bio-router handler */
static nimcp_error_t lnn_bio_router_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * Handler map for LNN bio-async module.
 * Message type 0 is used as a category handler for all LNN messages.
 */
DEFINE_HANDLER_MAP_BEGIN(lnn_bio_async)
    HANDLER_MAP_ENTRY(0, lnn_bio_router_handler)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(lnn_bio_async, lnn_network_t, network)

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Handler entry for message type
 */
typedef struct {
    lnn_bio_msg_handler_t handler;
    void* user_data;
} lnn_bio_handler_entry_t;

/**
 * @brief Bio-async context stored in network
 *
 * NOTE: This is stored in network->bio_ctx as opaque pointer
 */
typedef struct {
    bio_module_context_t bio_module;    /**< Bio-async module context */
    uint16_t module_id;                 /**< Module ID */
    bool connected;                      /**< Connection status */

    /* Message handlers (array indexed by lnn_bio_msg_type_t) */
    lnn_bio_handler_entry_t* handlers[LNN_BIO_MSG_COUNT];
    uint32_t handler_counts[LNN_BIO_MSG_COUNT];

    /* Phase synchronization state */
    nimcp_phase_sync_t phase_sync;      /**< Phase sync context */
    bool sync_pending;                   /**< Sync request pending */

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;

    /* Mutex for thread safety */
    void* mutex;
} lnn_bio_async_ctx_t;

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * @brief Get bio-async context from network
 */
static inline lnn_bio_async_ctx_t* get_bio_ctx(lnn_network_t* network) {
    if (!network || !network->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_bio_ctx: required parameter is NULL (network, network->bio_ctx)");
        return NULL;
    }
    return (lnn_bio_async_ctx_t*)network->bio_ctx;
}

/**
 * @brief Get bio-async context from network (const version)
 */
static inline const lnn_bio_async_ctx_t* get_bio_ctx_const(const lnn_network_t* network) {
    if (!network || !network->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_bio_ctx_const: required parameter is NULL (network, network->bio_ctx)");
        return NULL;
    }
    return (const lnn_bio_async_ctx_t*)network->bio_ctx;
}

/**
 * @brief Create bio-async context
 */
static lnn_bio_async_ctx_t* create_bio_ctx(void) {
    lnn_bio_async_ctx_t* ctx = (lnn_bio_async_ctx_t*)nimcp_malloc(sizeof(lnn_bio_async_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(lnn_bio_async_ctx_t),
                          "Failed to allocate bio-async context");
        return NULL;
    }

    memset(ctx, 0, sizeof(lnn_bio_async_ctx_t));
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                             "Failed to create mutex for bio-async context");
        nimcp_free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * @brief Destroy bio-async context
 */
static void destroy_bio_ctx(lnn_bio_async_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Free handler arrays */
    for (int i = 0; i < LNN_BIO_MSG_COUNT; i++) {
        if (ctx->handlers[i]) {
            nimcp_free(ctx->handlers[i]);
            ctx->handlers[i] = NULL;
        }
    }

    /* Destroy phase sync if active */
    if (ctx->phase_sync) {
        nimcp_phase_sync_destroy(ctx->phase_sync);
        ctx->phase_sync = NULL;
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
        ctx->mutex = NULL;
    }

    nimcp_free(ctx);
}

/**
 * @brief Bio-router message handler (invoked by bio-router)
 *
 * WHAT: Handles incoming bio-async messages for LNN
 * WHY:  Route messages to registered LNN handlers
 * HOW:  Extract message type, invoke handlers
 */
static nimcp_error_t lnn_bio_router_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    lnn_network_t* network = (lnn_network_t*)user_data;
    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);

    if (!ctx || !ctx->connected) {
        return LNN_ERROR_INVALID_STATE;
    }

    /* Extract message type from first uint32_t */
    if (msg_size < sizeof(uint32_t)) {
        NIMCP_LOGGING_ERROR("LNN bio-async: message too small");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "LNN bio-async message too small: %zu bytes", msg_size);
        return LNN_ERROR_INVALID_PARAM;
    }

    uint32_t msg_type_raw = *(const uint32_t*)msg;

    /* Map to lnn_bio_msg_type_t (assuming direct mapping) */
    if (msg_type_raw >= LNN_BIO_MSG_COUNT) {
        NIMCP_LOGGING_WARN("LNN bio-async: unknown message type %u", msg_type_raw);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "LNN bio-async: unknown message type %u", msg_type_raw);
        return LNN_ERROR_INVALID_PARAM;
    }

    lnn_bio_msg_type_t msg_type = (lnn_bio_msg_type_t)msg_type_raw;

    nimcp_mutex_lock(ctx->mutex);
    ctx->messages_received++;

    /* Invoke registered handlers */
    if (ctx->handler_counts[msg_type] > 0 && ctx->handlers[msg_type]) {
        for (uint32_t i = 0; i < ctx->handler_counts[msg_type]; i++) {
            lnn_bio_handler_entry_t* entry = &ctx->handlers[msg_type][i];
            if (entry->handler) {
                int result = entry->handler(network, msg_type, msg, msg_size, entry->user_data);
                if (result != 0) {
                    NIMCP_LOGGING_WARN("LNN bio-async: handler failed with code %d", result);
                }
            }
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    /* Complete promise if provided (for request/response pattern) */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, NULL);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Connection API Implementation
 *===========================================================================*/

int lnn_bio_async_connect(lnn_network_t* network, uint16_t module_id) {
    /* Validate inputs */
    if (!network) {
        NIMCP_LOGGING_ERROR("LNN bio-async connect: NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_connect");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Check if bio-async is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping LNN registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "Bio-async router not initialized for LNN connection");
        return LNN_ERROR_NOT_INITIALIZED;
    }

    /* Check if already connected */
    if (network->bio_ctx) {
        lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
        if (ctx && ctx->connected) {
            NIMCP_LOGGING_WARN("LNN bio-async: already connected");
            return 0;  /* Not an error */
        }
    }

    /* Create bio-async context */
    lnn_bio_async_ctx_t* ctx = create_bio_ctx();
    if (!ctx) {
        NIMCP_LOGGING_ERROR("LNN bio-async: failed to create context");
        /* Exception already thrown in create_bio_ctx */
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    /* Register with bio-router */
    char module_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(module_name, sizeof(module_name), "lnn_network_%u", network->id);

    bio_module_info_t info = {
        .module_id = module_id,
        .module_name = module_name,
        .inbox_capacity = 32,  /* Default inbox size */
        .user_data = network
    };

    ctx->bio_module = bio_router_register_module(&info);
    if (!ctx->bio_module) {
        NIMCP_LOGGING_ERROR("LNN bio-async: failed to register module");
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, network->id, "LNN",
                         "Failed to register LNN module 0x%04X with bio-router", module_id);
        destroy_bio_ctx(ctx);
        return LNN_ERROR_OPERATION_FAILED;
    }

    /* Register handlers via KG-driven wiring callback */
    nimcp_error_t err = bio_router_register_wiring_callback(
        module_id,
        (void*)lnn_bio_async_handler_callback,
        network
    );

    if (err != NIMCP_SUCCESS) {
        /* Legacy fallback: direct handler registration */
        NIMCP_LOGGING_DEBUG("LNN bio-async: KG wiring unavailable, using legacy registration");
        err = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            ctx->bio_module,
            0,  /* Message type 0 - we'll handle routing internally */
            lnn_bio_router_handler
        ));

        if (err != NIMCP_SUCCESS) {
            NIMCP_LOGGING_ERROR("LNN bio-async: failed to register handler");
            NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, network->id, "LNN",
                             "Failed to register bio-async handler for module 0x%04X", module_id);
            bio_router_unregister_module(ctx->bio_module);
            destroy_bio_ctx(ctx);
            return err;
        }
    }

    /* Update context */
    ctx->module_id = module_id;
    ctx->connected = true;
    network->bio_ctx = ctx;

    NIMCP_LOGGING_INFO("LNN network %u connected to bio-async as module 0x%04X",
                       network->id, module_id);

    return 0;
}

int lnn_bio_async_disconnect(lnn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_disconnect");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        return 0;  /* Already disconnected */
    }

    /* Unregister from bio-router */
    if (ctx->bio_module) {
        bio_router_unregister_module(ctx->bio_module);
        ctx->bio_module = NULL;
    }

    /* Clean up context */
    ctx->connected = false;
    destroy_bio_ctx(ctx);
    network->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("LNN network %u disconnected from bio-async", network->id);

    return 0;
}

bool lnn_bio_async_is_connected(const lnn_network_t* network) {
    if (!network) {
        return false;
    }

    const lnn_bio_async_ctx_t* ctx = get_bio_ctx_const(network);
    return (ctx != NULL && ctx->connected);
}

/*=============================================================================
 * Broadcasting API Implementation
 *===========================================================================*/

int lnn_bio_async_broadcast_state(lnn_network_t* network, uint16_t target_module) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_broadcast_state");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_LOGGING_WARN("LNN bio-async: not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for state broadcast");
        return LNN_ERROR_INVALID_STATE;
    }

    /* Build state message (simplified - just header for now) */
    /* In a full implementation, would collect state from all layers */
    lnn_bio_state_msg_t state_msg = {
        .network_id = network->id,
        .layer_id = 0,  /* Would iterate over layers */
        .state = NULL,  /* Would allocate and fill */
        .state_size = 0,
        .tau = NULL,
        .tau_size = 0,
        .t = 0.0f,  /* Would get from network time */
        .health = LNN_STATE_VALID
    };

    /* Send via bio-router */
    nimcp_mutex_lock(ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(
        ctx->bio_module,
        &state_msg,
        sizeof(state_msg)
    );

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
        NIMCP_LOGGING_ERROR("LNN bio-async: failed to broadcast state");
    }
    nimcp_mutex_unlock(ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : LNN_ERROR_OPERATION_FAILED;
}

int lnn_bio_async_broadcast_tau(lnn_network_t* network, uint16_t target_module) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_broadcast_tau");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for tau broadcast");
        return LNN_ERROR_INVALID_STATE;
    }

    /* Build tau update message */
    lnn_bio_state_msg_t tau_msg = {
        .network_id = network->id,
        .layer_id = 0,
        .state = NULL,
        .state_size = 0,
        .tau = NULL,  /* Would allocate and fill */
        .tau_size = 0,
        .t = 0.0f,
        .health = LNN_STATE_VALID
    };

    nimcp_mutex_lock(ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(
        ctx->bio_module,
        &tau_msg,
        sizeof(tau_msg)
    );

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : LNN_ERROR_OPERATION_FAILED;
}

int lnn_bio_async_broadcast_training_event(
    lnn_network_t* network,
    const lnn_bio_training_msg_t* event
) {
    if (!network || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_bio_async_broadcast_training_event: network=%p, event=%p",
                             (void*)network, (void*)event);
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for training event broadcast");
        return LNN_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(
        ctx->bio_module,
        event,
        sizeof(lnn_bio_training_msg_t)
    );

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : LNN_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * Phase Synchronization API Implementation
 *===========================================================================*/

int lnn_bio_async_request_sync(
    lnn_network_t* network,
    nimcp_oscillation_band_t band,
    float coherence_target
) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_request_sync");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for sync request");
        return LNN_ERROR_INVALID_STATE;
    }

    /* Create phase sync context if not exists */
    if (!ctx->phase_sync) {
        ctx->phase_sync = nimcp_phase_sync_create(band);
        if (!ctx->phase_sync) {
            NIMCP_LOGGING_ERROR("LNN bio-async: failed to create phase sync");
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                              "Failed to create phase sync context for LNN");
            return LNN_ERROR_OUT_OF_MEMORY;
        }
    }

    /* Build sync request message */
    lnn_bio_sync_msg_t sync_msg = {
        .network_id = network->id,
        .band = band,
        .phase = 0.0f,  /* Would compute from network oscillations */
        .frequency = nimcp_oscillation_center_freq(band),
        .coherence_target = coherence_target
    };

    nimcp_mutex_lock(ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(
        ctx->bio_module,
        &sync_msg,
        sizeof(sync_msg)
    );

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
        ctx->sync_pending = true;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : LNN_ERROR_OPERATION_FAILED;
}

int lnn_bio_async_wait_sync(lnn_network_t* network, int timeout_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_wait_sync");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for wait_sync");
        return LNN_ERROR_INVALID_STATE;
    }

    if (!ctx->phase_sync || !ctx->sync_pending) {
        NIMCP_LOGGING_WARN("LNN bio-async: no sync pending");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "No sync pending for LNN bio-async wait_sync");
        return LNN_ERROR_INVALID_STATE;
    }

    /* Wait for phase sync (using bio-async phase sync API) */
    nimcp_error_t err = nimcp_phase_sync_wait_all(
        ctx->phase_sync,
        (timeout_ms > 0) ? timeout_ms : 0
    );

    nimcp_mutex_lock(ctx->mutex);
    if (err == NIMCP_SUCCESS) {
        ctx->sync_pending = false;
    }
    nimcp_mutex_unlock(ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : LNN_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * Message Handler API Implementation
 *===========================================================================*/

int lnn_bio_async_register_handler(
    lnn_network_t* network,
    lnn_bio_msg_type_t type,
    lnn_bio_msg_handler_t handler,
    void* user_data
) {
    if (!network || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null pointer in lnn_bio_async_register_handler: network=%p, handler=%p",
                             (void*)network, (void*)handler);
        return LNN_ERROR_NULL_POINTER;
    }

    if (type >= LNN_BIO_MSG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid bio-async message type: %d", type);
        return LNN_ERROR_INVALID_PARAM;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for handler registration");
        return LNN_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Allocate or expand handler array */
    uint32_t count = ctx->handler_counts[type];
    lnn_bio_handler_entry_t* new_handlers = (lnn_bio_handler_entry_t*)nimcp_realloc(
        ctx->handlers[type],
        (count + 1) * sizeof(lnn_bio_handler_entry_t)
    );

    if (!new_handlers) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          (count + 1) * sizeof(lnn_bio_handler_entry_t),
                          "Failed to expand handler array for type %d", type);
        nimcp_mutex_unlock(ctx->mutex);
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    /* Add new handler */
    new_handlers[count].handler = handler;
    new_handlers[count].user_data = user_data;

    ctx->handlers[type] = new_handlers;
    ctx->handler_counts[type] = count + 1;

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int lnn_bio_async_process(lnn_network_t* network, int timeout_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_process");
        return LNN_ERROR_NULL_POINTER;
    }

    lnn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "LNN bio-async not connected for message processing");
        return LNN_ERROR_INVALID_STATE;
    }

    /* Process messages from inbox using bio-router */
    uint32_t max_messages = (timeout_ms == 0) ? 1 : 100;  /* Process up to 100 messages */
    uint32_t processed = bio_router_process_inbox(ctx->bio_module, max_messages);

    return (int)processed;
}

/*=============================================================================
 * Utility Functions Implementation
 *===========================================================================*/

const char* lnn_bio_msg_type_to_string(lnn_bio_msg_type_t type) {
    switch (type) {
        case LNN_BIO_MSG_STATE_BROADCAST:
            return "STATE_BROADCAST";
        case LNN_BIO_MSG_TAU_UPDATE:
            return "TAU_UPDATE";
        case LNN_BIO_MSG_GRADIENT_READY:
            return "GRADIENT_READY";
        case LNN_BIO_MSG_INSTABILITY:
            return "INSTABILITY";
        case LNN_BIO_MSG_SYNC_REQUEST:
            return "SYNC_REQUEST";
        case LNN_BIO_MSG_SYNC_RESPONSE:
            return "SYNC_RESPONSE";
        case LNN_BIO_MSG_TRAINING_EVENT:
            return "TRAINING_EVENT";
        default:
            return "UNKNOWN";
    }
}

int lnn_bio_async_get_stats(
    const lnn_network_t* network,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* messages_dropped
) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_bio_async_get_stats");
        return LNN_ERROR_NULL_POINTER;
    }

    const lnn_bio_async_ctx_t* ctx = get_bio_ctx_const(network);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "No bio-async context for stats retrieval");
        return LNN_ERROR_INVALID_STATE;
    }

    if (messages_sent) {
        *messages_sent = ctx->messages_sent;
    }
    if (messages_received) {
        *messages_received = ctx->messages_received;
    }
    if (messages_dropped) {
        *messages_dropped = ctx->messages_dropped;
    }

    return 0;
}
