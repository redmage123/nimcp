/**
 * @file nimcp_snn_bio_async.c
 * @brief Bio-async integration implementation for Spiking Neural Networks
 *
 * WHAT: Implements bio-async messaging for SNN networks
 * WHY:  Enable inter-module communication for SNN state and spikes
 * HOW:  Bio-router registration, message handlers, spike broadcast
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/nimcp_snn_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_bio_async)

/*=============================================================================
 * KG-Driven Wiring Infrastructure
 *===========================================================================*/

/* Forward declaration of bio-router handler */
static nimcp_error_t snn_bio_router_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * Handler map for SNN bio-async module.
 * Message type 0 is used as a category handler for all SNN messages.
 */
DEFINE_HANDLER_MAP_BEGIN(snn_bio_async)
    HANDLER_MAP_ENTRY(0, snn_bio_router_handler)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(snn_bio_async, snn_network_t, network)

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Handler entry for message type
 */
typedef struct {
    snn_bio_msg_handler_t handler;
    void* user_data;
} snn_bio_handler_entry_t;

/**
 * @brief Bio-async context stored in network
 */
typedef struct {
    bio_module_context_t bio_module;    /**< Bio-async module context */
    uint16_t module_id;                 /**< Module ID */
    bool connected;                     /**< Connection status */

    /* Message handlers */
    snn_bio_handler_entry_t* handlers[SNN_BIO_MSG_COUNT];
    uint32_t handler_counts[SNN_BIO_MSG_COUNT];

    /* Phase synchronization state */
    nimcp_phase_sync_t phase_sync;      /**< Phase sync context */
    bool sync_pending;                  /**< Sync request pending */

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;

    /* Mutex for thread safety */
    nimcp_mutex_t mutex;
} snn_bio_async_ctx_t;

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * @brief Get bio-async context from network
 */
static inline snn_bio_async_ctx_t* get_bio_ctx(snn_network_t* network) {
    if (!network || !network->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_bio_ctx: required parameter is NULL (network, network->bio_ctx)");
        return NULL;
    }
    return (snn_bio_async_ctx_t*)network->bio_ctx;
}

/**
 * @brief Get bio-async context from network (const version)
 */
static inline const snn_bio_async_ctx_t* get_bio_ctx_const(const snn_network_t* network) {
    if (!network || !network->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_bio_ctx_const: required parameter is NULL (network, network->bio_ctx)");
        return NULL;
    }
    return (const snn_bio_async_ctx_t*)network->bio_ctx;
}

/**
 * @brief Create bio-async context
 */
static snn_bio_async_ctx_t* create_bio_ctx(void) {
    snn_bio_async_ctx_t* ctx = nimcp_malloc(sizeof(snn_bio_async_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_bio_async_ctx_t),
            "Failed to allocate bio-async context");
        return NULL;
    }

    memset(ctx, 0, sizeof(snn_bio_async_ctx_t));

    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "Failed to initialize bio-async mutex");
        nimcp_free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * @brief Destroy bio-async context
 */
static void destroy_bio_ctx(snn_bio_async_ctx_t* ctx) {
    if (!ctx) return;

    /* Free handler arrays */
    for (int i = 0; i < SNN_BIO_MSG_COUNT; i++) {
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

    nimcp_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);
}

/**
 * @brief Bio-router message handler
 */
static nimcp_error_t snn_bio_router_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    snn_network_t* network = (snn_network_t*)user_data;
    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);

    if (!ctx || !ctx->connected) {
        return SNN_ERROR_INVALID_STATE;
    }

    /* Extract message type */
    if (msg_size < sizeof(uint32_t)) {
        NIMCP_LOGGING_ERROR("SNN bio-async: message too small");
        return SNN_ERROR_INVALID_CONFIG;
    }

    uint32_t msg_type_raw = *(const uint32_t*)msg;
    if (msg_type_raw >= SNN_BIO_MSG_COUNT) {
        NIMCP_LOGGING_WARN("SNN bio-async: unknown message type %u", msg_type_raw);
        return SNN_ERROR_INVALID_CONFIG;
    }

    snn_bio_msg_type_t msg_type = (snn_bio_msg_type_t)msg_type_raw;

    nimcp_mutex_lock(&ctx->mutex);
    ctx->messages_received++;

    /* Invoke registered handlers */
    if (ctx->handler_counts[msg_type] > 0 && ctx->handlers[msg_type]) {
        for (uint32_t i = 0; i < ctx->handler_counts[msg_type]; i++) {
            snn_bio_handler_entry_t* entry = &ctx->handlers[msg_type][i];
            if (entry->handler) {
                int result = entry->handler(network, msg_type, msg, msg_size, entry->user_data);
                if (result != 0) {
                    NIMCP_LOGGING_WARN("SNN bio-async: handler failed with code %d", result);
                }
            }
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, NULL);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Connection API Implementation
 *===========================================================================*/

int snn_bio_async_connect(snn_network_t* network, uint16_t module_id) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async connect: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check if bio-async is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping SNN registration");
        return SNN_ERROR_NOT_INITIALIZED;
    }

    /* Check if already connected */
    if (network->bio_ctx) {
        snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
        if (ctx && ctx->connected) {
            NIMCP_LOGGING_WARN("SNN bio-async: already connected");
            return 0;
        }
    }

    /* Create bio-async context */
    snn_bio_async_ctx_t* ctx = create_bio_ctx();
    if (!ctx) {
        /* Exception already thrown by create_bio_ctx */
        return SNN_ERROR_OUT_OF_MEMORY;
    }

    /* Register with bio-router */
    char module_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(module_name, sizeof(module_name), "snn_network_%u", network->id);

    bio_module_info_t info = {
        .module_id = module_id,
        .module_name = module_name,
        .inbox_capacity = 64,  /* Higher capacity for spike events */
        .user_data = network
    };

    ctx->bio_module = bio_router_register_module(&info);
    if (!ctx->bio_module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "SNN bio-async: failed to register module with bio-router");
        destroy_bio_ctx(ctx);
        return SNN_ERROR_OPERATION_FAILED;
    }

    /* Register handlers via KG-driven wiring callback */
    nimcp_error_t err = bio_router_register_wiring_callback(
        module_id,
        (void*)snn_bio_async_handler_callback,
        network
    );

    if (err != NIMCP_SUCCESS) {
        /* Legacy fallback: direct handler registration */
        NIMCP_LOGGING_DEBUG("SNN bio-async: KG wiring unavailable, using legacy registration");
        err = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            ctx->bio_module,
            0,
            snn_bio_router_handler
        ));

        if (err != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
                "SNN bio-async: failed to register handler");
            bio_router_unregister_module(ctx->bio_module);
            destroy_bio_ctx(ctx);
            return err;
        }
    }

    ctx->module_id = module_id;
    ctx->connected = true;
    network->bio_ctx = ctx;

    NIMCP_LOGGING_INFO("SNN network %u connected to bio-async as module 0x%04X",
                       network->id, module_id);

    return 0;
}

int snn_bio_async_disconnect(snn_network_t* network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async disconnect: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return 0;

    if (ctx->bio_module) {
        bio_router_unregister_module(ctx->bio_module);
        ctx->bio_module = NULL;
    }

    ctx->connected = false;
    destroy_bio_ctx(ctx);
    network->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("SNN network %u disconnected from bio-async", network->id);

    return 0;
}

bool snn_bio_async_is_connected(const snn_network_t* network) {
    if (!network) {
        return false;
    }
    const snn_bio_async_ctx_t* ctx = get_bio_ctx_const(network);
    return (ctx != NULL && ctx->connected);
}

/*=============================================================================
 * Broadcasting API Implementation
 *===========================================================================*/

int snn_bio_async_broadcast_spike(snn_network_t* network, const snn_bio_spike_msg_t* event) {
    if (!network || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async broadcast spike: NULL %s", !network ? "network" : "event");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, event, sizeof(snn_bio_spike_msg_t));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

int snn_bio_async_broadcast_state(snn_network_t* network, uint16_t target_module) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async broadcast state: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    (void)target_module;

    /* Build state message */
    snn_bio_state_msg_t state_msg = {
        .network_id = network->id,
        .n_populations = network->n_populations,
        .rates = NULL,
        .rates_size = 0,
        .t = 0.0f,
        .total_spikes = 0,
        .mean_rate = 0.0f
    };

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, &state_msg, sizeof(state_msg));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

int snn_bio_async_broadcast_stdp(snn_network_t* network, const snn_bio_stdp_msg_t* event) {
    if (!network || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async broadcast STDP: NULL %s", !network ? "network" : "event");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, event, sizeof(snn_bio_stdp_msg_t));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

int snn_bio_async_broadcast_training(snn_network_t* network, const snn_bio_training_msg_t* event) {
    if (!network || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async broadcast training: NULL %s", !network ? "network" : "event");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, event, sizeof(snn_bio_training_msg_t));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

int snn_bio_async_broadcast_population(snn_network_t* network, const snn_bio_population_msg_t* event) {
    if (!network || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async broadcast population: NULL %s", !network ? "network" : "event");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, event, sizeof(snn_bio_population_msg_t));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * Phase Synchronization API Implementation
 *===========================================================================*/

int snn_bio_async_request_sync(snn_network_t* network, nimcp_oscillation_band_t band, float coherence_target) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async request sync: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    if (!ctx->phase_sync) {
        ctx->phase_sync = nimcp_phase_sync_create(band);
        if (!ctx->phase_sync) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_phase_sync_t),
                "SNN bio-async: failed to create phase sync");
            return SNN_ERROR_OUT_OF_MEMORY;
        }
    }

    snn_bio_sync_msg_t sync_msg = {
        .network_id = network->id,
        .band = band,
        .phase = 0.0f,
        .frequency = nimcp_oscillation_center_freq(band),
        .coherence_target = coherence_target
    };

    nimcp_mutex_lock(&ctx->mutex);
    nimcp_error_t err = bio_router_broadcast(ctx->bio_module, &sync_msg, sizeof(sync_msg));

    if (err == NIMCP_SUCCESS) {
        ctx->messages_sent++;
        ctx->sync_pending = true;
    } else {
        ctx->messages_dropped++;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

int snn_bio_async_wait_sync(snn_network_t* network, int timeout_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async wait sync: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    if (!ctx->phase_sync || !ctx->sync_pending) {
        NIMCP_LOGGING_WARN("SNN bio-async: no sync pending");
        return SNN_ERROR_INVALID_STATE;
    }

    nimcp_error_t err = nimcp_phase_sync_wait_all(ctx->phase_sync, (timeout_ms > 0) ? timeout_ms : 0);

    nimcp_mutex_lock(&ctx->mutex);
    if (err == NIMCP_SUCCESS) {
        ctx->sync_pending = false;
    }
    nimcp_mutex_unlock(&ctx->mutex);

    return (err == NIMCP_SUCCESS) ? 0 : SNN_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * Message Handler API Implementation
 *===========================================================================*/

int snn_bio_async_register_handler(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    snn_bio_msg_handler_t handler,
    void* user_data
) {
    if (!network || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async register handler: NULL %s", !network ? "network" : "handler");
        return SNN_ERROR_NULL_POINTER;
    }
    if (type >= SNN_BIO_MSG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "SNN bio-async register handler: invalid message type %d", type);
        return SNN_ERROR_INVALID_CONFIG;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = ctx->handler_counts[type];
    snn_bio_handler_entry_t* new_handlers = nimcp_realloc(
        ctx->handlers[type],
        (count + 1) * sizeof(snn_bio_handler_entry_t)
    );

    if (!new_handlers) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, (count + 1) * sizeof(snn_bio_handler_entry_t),
            "SNN bio-async register handler: failed to allocate handler array");
        nimcp_mutex_unlock(&ctx->mutex);
        return SNN_ERROR_OUT_OF_MEMORY;
    }

    new_handlers[count].handler = handler;
    new_handlers[count].user_data = user_data;

    ctx->handlers[type] = new_handlers;
    ctx->handler_counts[type] = count + 1;

    nimcp_mutex_unlock(&ctx->mutex);

    return 0;
}

int snn_bio_async_process(snn_network_t* network, int timeout_ms) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async process: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_bio_async_ctx_t* ctx = get_bio_ctx(network);
    if (!ctx || !ctx->connected) return SNN_ERROR_INVALID_STATE;

    uint32_t max_messages = (timeout_ms == 0) ? 1 : 100;
    uint32_t processed = bio_router_process_inbox(ctx->bio_module, max_messages);

    return (int)processed;
}

/*=============================================================================
 * Utility Functions Implementation
 *===========================================================================*/

const char* snn_bio_msg_type_to_string(snn_bio_msg_type_t type) {
    switch (type) {
        case SNN_BIO_MSG_SPIKE_EVENT:
            return "SPIKE_EVENT";
        case SNN_BIO_MSG_STATE_BROADCAST:
            return "STATE_BROADCAST";
        case SNN_BIO_MSG_RATE_UPDATE:
            return "RATE_UPDATE";
        case SNN_BIO_MSG_SYNC_REQUEST:
            return "SYNC_REQUEST";
        case SNN_BIO_MSG_SYNC_RESPONSE:
            return "SYNC_RESPONSE";
        case SNN_BIO_MSG_TRAINING_EVENT:
            return "TRAINING_EVENT";
        case SNN_BIO_MSG_STDP_EVENT:
            return "STDP_EVENT";
        case SNN_BIO_MSG_POPULATION_ACTIVITY:
            return "POPULATION_ACTIVITY";
        case SNN_BIO_MSG_INSTABILITY:
            return "INSTABILITY";
        case SNN_BIO_MSG_IMMUNE_ALERT:
            return "IMMUNE_ALERT";
        default:
            return "UNKNOWN";
    }
}

int snn_bio_async_get_stats(
    const snn_network_t* network,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* messages_dropped
) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "SNN bio-async get stats: NULL network");
        return SNN_ERROR_NULL_POINTER;
    }

    const snn_bio_async_ctx_t* ctx = get_bio_ctx_const(network);
    if (!ctx) return SNN_ERROR_INVALID_STATE;

    if (messages_sent) *messages_sent = ctx->messages_sent;
    if (messages_received) *messages_received = ctx->messages_received;
    if (messages_dropped) *messages_dropped = ctx->messages_dropped;

    return 0;
}
