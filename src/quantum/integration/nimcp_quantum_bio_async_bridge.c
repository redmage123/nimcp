/**
 * @file nimcp_quantum_bio_async_bridge.c
 * @brief Implementation of Quantum Module Bio-Async Integration Bridge
 * @version 1.0.2
 * @date 2026-01-15
 *
 * DEADLOCK PREVENTION STRATEGY
 * ============================
 *
 * This module uses a single mutex (bridge->base.mutex) to protect internal state.
 * The locking strategy follows these rules to prevent deadlocks:
 *
 * 1. LOCK ORDERING: The bridge mutex is always acquired AFTER any external
 *    router operations. This prevents circular wait conditions with the
 *    bio_router's internal locks.
 *
 * 2. MINIMAL LOCK SCOPE: Locks are held only for the minimum time necessary
 *    to update state/statistics. I/O operations (broadcasting) are done
 *    BEFORE acquiring the lock.
 *
 * 3. NO NESTED LOCKS: Public functions do NOT call other public functions
 *    while holding the mutex. Each function manages its own lock lifecycle.
 *
 * 4. GOTO CLEANUP PATTERN: All error paths use explicit unlock before return,
 *    ensuring the mutex is never left locked on failure.
 *
 * 5. EXTERNAL CALLS UNLOCKED: Calls to bio_router_broadcast() are made
 *    without holding bridge->base.mutex to prevent lock inversion deadlocks.
 *
 * Lock acquisition pattern in broadcast functions:
 *   1. Validate parameters (no lock)
 *   2. Build message (no lock)
 *   3. Call bio_router_broadcast() (no lock - router has its own locks)
 *   4. Acquire bridge->base.mutex
 *   5. Update internal state/statistics
 *   6. Release bridge->base.mutex
 *   7. Return result
 *
 * This pattern ensures that even if bio_router internally tries to call
 * back into this module, no deadlock can occur because we don't hold
 * the bridge mutex during the router call.
 */

#include "quantum/integration/nimcp_quantum_bio_async_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_bio_async_bridge)

#define LOG_MODULE "QUANTUM_BIO_ASYNC_BRIDGE"


/* ============================================================================
 * Internal State Cache
 * ============================================================================ */

typedef struct {
    float coherence_level;
    float min_coherence;
    uint32_t num_qubits;
    uint32_t errors_detected;
    uint32_t entangled_pairs;
} quantum_internal_state_t;

/* ============================================================================
 * Internal Bridge Structure
 * ============================================================================ */

struct quantum_bio_async_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    quantum_bio_async_config_t config;
    bio_router_t router;
    bio_module_context_t module_ctx;    /**< Module context for router API */

    quantum_bio_subscription_t* subscriptions;
    uint32_t subscription_count;
    uint32_t subscription_capacity;

    bool connected;
    uint64_t last_broadcast_us;
    uint32_t time_since_broadcast_ms;

    /* Cached state */
    quantum_internal_state_t state;

    quantum_bio_async_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t quantum_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static quantum_bio_subscription_t* quantum_find_subscription(
    quantum_bio_async_bridge_t* b, uint32_t module_id
) {
    for (uint32_t i = 0; i < b->subscription_count; i++) {
        if (b->subscriptions[i].module_id == module_id && b->subscriptions[i].active) {
            return &b->subscriptions[i];
        }
    }
    /* P2 fix: Remove false positive NIMCP_THROW_TO_IMMUNE. "Not found" is normal
     * behavior for a search function - the caller handles NULL return. */
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int quantum_bio_async_default_config(quantum_bio_async_config_t* config) {
    if (!config) {
        LOG_ERROR("NULL config pointer in quantum_bio_async_default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config pointer");
        return -1;
    }

    config->state_broadcast_interval_ms = QUANTUM_BIO_DEFAULT_BROADCAST_INTERVAL_MS;
    config->enable_auto_broadcast = true;
    config->max_inbox_process_per_update = 32;
    config->message_ttl_ms = QUANTUM_BIO_MESSAGE_TTL_MS;
    config->default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    config->error_channel = BIO_CHANNEL_NOREPINEPHRINE;
    config->coherence_warning_threshold = QUANTUM_BIO_COHERENCE_WARNING;
    config->coherence_critical_threshold = QUANTUM_BIO_COHERENCE_CRITICAL;
    config->max_subscriptions = QUANTUM_BIO_MAX_SUBSCRIPTIONS;
    config->enable_coherence_routing = true;
    config->enable_entanglement_routing = true;
    config->enable_measurement_routing = true;
    config->enable_error_routing = true;
    config->enable_logging = false;

    return 0;
}

quantum_bio_async_bridge_t* quantum_bio_async_bridge_create(
    const quantum_bio_async_config_t* config
) {
    quantum_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(quantum_bio_async_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate quantum bio-async bridge");

    if (config) {
        bridge->config = *config;
    } else {
        quantum_bio_async_default_config(&bridge->config);
    }

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "quantum_bio_async") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_ERROR("Failed to create mutex for quantum bio-async bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create quantum bio-async bridge mutex");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->subscription_capacity = bridge->config.max_subscriptions;
    bridge->subscriptions = nimcp_calloc(
        bridge->subscription_capacity, sizeof(quantum_bio_subscription_t)
    );
    if (!bridge->subscriptions) {
        LOG_ERROR("Failed to allocate subscriptions for quantum bio-async bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate quantum bio-async bridge subscriptions");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->last_broadcast_us = quantum_get_timestamp_us();
    bridge->state.coherence_level = 1.0f;
    bridge->state.min_coherence = 1.0f;
    bridge->stats.avg_coherence = 1.0f;
    bridge->stats.min_coherence = 1.0f;

    return bridge;
}

void quantum_bio_async_bridge_destroy(quantum_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "quantum_bio_async");

    if (bridge->connected) {
        quantum_bio_async_disconnect(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->subscriptions);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int quantum_bio_async_connect(
    quantum_bio_async_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_connect");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }

    bridge->router = router;

    /* Register with the bio-router */
    if (router && bio_router_is_initialized()) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_ELIGIBILITY_QUANTUM,
            .module_name = "quantum_bio_async",
            .inbox_capacity = 0,  /* Use default */
            .user_data = bridge
        };
        bridge->module_ctx = bio_router_register_module(&info);
    }

    bridge->connected = true;

    return 0;
}

int quantum_bio_async_disconnect(quantum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_disconnect");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }

    /* Unregister from router */
    if (bridge->module_ctx) {
        bio_router_unregister_module(bridge->module_ctx);
        bridge->module_ctx = NULL;
    }

    bridge->router = NULL;
    bridge->connected = false;

    return 0;
}

bool quantum_bio_async_is_connected(const quantum_bio_async_bridge_t* bridge) {
    return bridge ? bridge->connected : false;
}

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

int quantum_bio_async_process_inbox(
    quantum_bio_async_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_process_inbox");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        LOG_ERROR("Bridge not connected in quantum_bio_async_process_inbox");
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected");
        return -1;
    }
    if (!bridge->module_ctx) return 0;

    /* Use the router to process inbox messages */
    uint32_t limit = max_messages > 0 ? max_messages : bridge->config.max_inbox_process_per_update;
    uint32_t processed = bio_router_process_inbox(bridge->module_ctx, limit);

    /* Update statistics with mutex protection */
    if (bridge->base.mutex) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.messages_received += processed;
    if (bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return (int)processed;
}

int quantum_bio_async_update(
    quantum_bio_async_bridge_t* bridge,
    uint32_t delta_ms
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        LOG_ERROR("Bridge not connected in quantum_bio_async_update");
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected");
        return -1;
    }

    bridge->time_since_broadcast_ms += delta_ms;

    if (bridge->config.enable_auto_broadcast &&
        bridge->time_since_broadcast_ms >= bridge->config.state_broadcast_interval_ms) {
        /* Broadcast coherence as periodic update */
        quantum_bio_async_broadcast_coherence(
            bridge,
            bridge->state.coherence_level,
            bridge->state.num_qubits
        );
        bridge->time_since_broadcast_ms = 0;
    }

    return 0;
}

/* ============================================================================
 * Broadcast API
 * ============================================================================ */

/**
 * @brief Helper to broadcast message through router with mutex protection
 */
static int quantum_broadcast_via_router(
    quantum_bio_async_bridge_t* bridge,
    const void* msg,
    size_t msg_size
) {
    if (!bridge->module_ctx) {
        /* Router not available, just update stats */
        return 0;
    }

    nimcp_error_t err = bio_router_broadcast(bridge->module_ctx, msg, msg_size);
    if (err != NIMCP_SUCCESS) {
        if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.routing_errors++;
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW(err, "Bio-router broadcast failed");
        return -1;
    }
    return 0;
}

int quantum_bio_async_broadcast_coherence(
    quantum_bio_async_bridge_t* bridge,
    float coherence_level,
    uint32_t num_qubits
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_coherence");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for coherence broadcast");
        return -1;
    }
    if (!bridge->config.enable_coherence_routing) return 0;

    quantum_bio_coherence_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_UQ_COHERENCE_UPDATE;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_coherence_msg_t) - sizeof(bio_message_header_t);

    msg.coherence_level = coherence_level;
    msg.previous_coherence = bridge->state.coherence_level;
    msg.decoherence_rate = (bridge->state.coherence_level - coherence_level) * 1000.0f;
    msg.num_qubits = num_qubits;
    msg.avg_t1_time_us = 50.0f;
    msg.avg_t2_time_us = 30.0f;
    msg.is_warning = (coherence_level < bridge->config.coherence_warning_threshold);
    msg.is_critical = (coherence_level < bridge->config.coherence_critical_threshold);
    msg.timestamp_us = msg.header.timestamp_us;

    if (msg.is_critical) {
        msg.header.channel = bridge->config.error_channel;
        msg.header.flags |= BIO_MSG_FLAG_URGENT;
    }

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update internal state with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.coherence_level = coherence_level;
    bridge->state.num_qubits = num_qubits;
    if (coherence_level < bridge->state.min_coherence) {
        bridge->state.min_coherence = coherence_level;
    }

    bridge->stats.avg_coherence = (bridge->stats.avg_coherence * 0.99f) + (coherence_level * 0.01f);
    if (!isfinite(bridge->stats.avg_coherence)) bridge->stats.avg_coherence = coherence_level;
    bridge->stats.min_coherence = bridge->state.min_coherence;

    if (msg.is_critical || msg.is_warning) {
        bridge->stats.decoherence_warnings++;
    }

    bridge->stats.coherence_updates_sent++;
    bridge->stats.broadcasts_sent++;
    bridge->last_broadcast_us = msg.timestamp_us;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_entanglement(
    quantum_bio_async_bridge_t* bridge,
    uint32_t qubit_a,
    uint32_t qubit_b,
    float fidelity,
    bool is_creation
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_entanglement");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for entanglement broadcast");
        return -1;
    }
    if (!bridge->config.enable_entanglement_routing) return 0;

    quantum_bio_entanglement_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_ENTANGLEMENT;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_entanglement_msg_t) - sizeof(bio_message_header_t);

    msg.qubit_a = qubit_a;
    msg.qubit_b = qubit_b;
    msg.entanglement_fidelity = fidelity;
    msg.bell_state = 0; /* Phi+ by default */
    msg.concurrence = fidelity;
    msg.is_creation = is_creation;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update state with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    if (is_creation) {
        bridge->state.entangled_pairs++;
    } else {
        if (bridge->state.entangled_pairs > 0) {
            bridge->state.entangled_pairs--;
        }
    }

    bridge->stats.entanglement_events_sent++;
    bridge->stats.broadcasts_sent++;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_measurement(
    quantum_bio_async_bridge_t* bridge,
    uint32_t qubit_id,
    int32_t outcome,
    float probability
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_measurement");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for measurement broadcast");
        return -1;
    }
    if (!bridge->config.enable_measurement_routing) return 0;

    quantum_bio_measurement_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_MEASUREMENT;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_measurement_msg_t) - sizeof(bio_message_header_t);

    msg.qubit_id = qubit_id;
    msg.basis = 0; /* Z basis by default */
    msg.outcome = outcome;
    msg.probability = probability;
    msg.confidence = 0.95f;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update stats with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.measurements_sent++;
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_walk_update(
    quantum_bio_async_bridge_t* bridge,
    uint32_t walk_id,
    uint32_t step_number,
    float amplitude_at_target
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_walk_update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for walk update broadcast");
        return -1;
    }

    quantum_bio_walk_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_WALK_DIFFUSION;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_walk_msg_t) - sizeof(bio_message_header_t);

    msg.walk_id = walk_id;
    msg.step_number = step_number;
    msg.amplitude_at_target = amplitude_at_target;
    msg.target_found = (amplitude_at_target > 0.5f);
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update stats with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.walk_updates_sent++;
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_annealing(
    quantum_bio_async_bridge_t* bridge,
    uint32_t annealing_id,
    uint32_t step_number,
    float temperature,
    float current_energy
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_annealing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for annealing broadcast");
        return -1;
    }

    quantum_bio_annealing_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_ANNEAL_STATE;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_annealing_msg_t) - sizeof(bio_message_header_t);

    msg.annealing_id = annealing_id;
    msg.step_number = step_number;
    msg.temperature = temperature;
    msg.current_energy = current_energy;
    msg.is_complete = (temperature < 0.01f);
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update stats with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_gate(
    quantum_bio_async_bridge_t* bridge,
    uint32_t gate_type,
    uint32_t target_qubit,
    float gate_fidelity
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_gate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for gate broadcast");
        return -1;
    }

    quantum_bio_gate_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_GATE_APPLIED;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_gate_msg_t) - sizeof(bio_message_header_t);

    msg.gate_type = gate_type;
    msg.target_qubit = target_qubit;
    msg.gate_fidelity = gate_fidelity;
    msg.gate_time_us = 0.1f; /* Typical single-qubit gate */
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update stats with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_error(
    quantum_bio_async_bridge_t* bridge,
    uint32_t error_type,
    uint32_t qubit_id,
    bool is_correctable
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for error broadcast");
        return -1;
    }
    if (!bridge->config.enable_error_routing) return 0;

    quantum_bio_error_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_ERROR_DETECTED;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.error_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_error_msg_t) - sizeof(bio_message_header_t);

    msg.error_type = error_type;
    msg.qubit_id = qubit_id;
    msg.is_correctable = is_correctable;
    msg.was_corrected = false;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update state with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.errors_detected++;
    msg.error_count = bridge->state.errors_detected;
    bridge->stats.errors_detected++;
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int quantum_bio_async_broadcast_amplitude(
    quantum_bio_async_bridge_t* bridge,
    uint32_t target_state,
    float estimated_amplitude,
    float variance
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_broadcast_amplitude");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "Bridge not connected for amplitude broadcast");
        return -1;
    }

    quantum_bio_amplitude_msg_t msg = {0};
    msg.header.type = BIO_MSG_ELIG_QUANTUM_AMPLITUDE_ESTIMATE;
    msg.header.source_module = BIO_MODULE_ELIGIBILITY_QUANTUM;
    msg.header.channel = bridge->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = quantum_get_timestamp_us();
    msg.header.payload_size = sizeof(quantum_bio_amplitude_msg_t) - sizeof(bio_message_header_t);

    msg.target_state = target_state;
    msg.estimated_amplitude = estimated_amplitude;
    msg.estimated_probability = estimated_amplitude * estimated_amplitude;
    msg.variance = variance;
    msg.std_error = (variance > 0.0f) ? sqrtf(variance) : 0.0f;
    msg.timestamp_us = msg.header.timestamp_us;

    /* Route through bio_router */
    int result = quantum_broadcast_via_router(bridge, &msg, sizeof(msg));

    /* Update stats with mutex protection */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.broadcasts_sent++;
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

int quantum_bio_async_subscribe_module(
    quantum_bio_async_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_subscribe_module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    quantum_bio_subscription_t* existing = quantum_find_subscription(bridge, module_id);
    if (existing) {
        existing->msg_type_mask |= msg_types;
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    if (bridge->subscription_count >= bridge->subscription_capacity) {
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW(NIMCP_ERROR_BUFFER_FULL, "Subscription capacity exceeded");
        return -1;
    }

    quantum_bio_subscription_t* sub = &bridge->subscriptions[bridge->subscription_count++];
    sub->module_id = module_id;
    sub->msg_type_mask = msg_types;
    sub->active = true;
    sub->subscription_time = quantum_get_timestamp_us();
    sub->messages_sent = 0;

    bridge->stats.active_subscriptions = bridge->subscription_count;
    if (bridge->subscription_count > bridge->stats.peak_subscriptions) {
        bridge->stats.peak_subscriptions = bridge->subscription_count;
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int quantum_bio_async_unsubscribe_module(
    quantum_bio_async_bridge_t* bridge,
    uint32_t module_id
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_unsubscribe_module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].module_id == module_id) {
            bridge->subscriptions[i].active = false;
            bridge->stats.active_subscriptions--;
            if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_THROW(NIMCP_ERROR_NOT_FOUND, "Subscription not found for module");
    return -1;
}

uint32_t quantum_bio_async_get_subscriber_count(
    const quantum_bio_async_bridge_t* bridge,
    quantum_bio_msg_type_t msg_type
) {
    if (!bridge) return 0;

    /* Note: casting away const for mutex - safe since we're only reading */
    quantum_bio_async_bridge_t* mutable_bridge = (quantum_bio_async_bridge_t*)bridge;
    if (mutable_bridge->base.mutex) nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t count = 0;
    uint32_t mask = (1U << msg_type);

    for (uint32_t i = 0; i < bridge->subscription_count; i++) {
        if (bridge->subscriptions[i].active &&
            (bridge->subscriptions[i].msg_type_mask & mask)) {
            count++;
        }
    }

    if (mutable_bridge->base.mutex) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

int quantum_bio_async_get_stats(
    const quantum_bio_async_bridge_t* bridge,
    quantum_bio_async_stats_t* stats
) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }
    if (!stats) {
        LOG_ERROR("NULL stats pointer in quantum_bio_async_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL stats pointer");
        return -1;
    }

    /* Note: casting away const for mutex - safe since we're only reading */
    quantum_bio_async_bridge_t* mutable_bridge = (quantum_bio_async_bridge_t*)bridge;
    if (mutable_bridge->base.mutex) nimcp_mutex_lock(mutable_bridge->base.mutex);

    *stats = bridge->stats;

    if (mutable_bridge->base.mutex) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int quantum_bio_async_reset_stats(quantum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        LOG_ERROR("NULL bridge in quantum_bio_async_reset_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    uint32_t active = bridge->stats.active_subscriptions;
    uint32_t peak = bridge->stats.peak_subscriptions;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.active_subscriptions = active;
    bridge->stats.peak_subscriptions = peak;
    bridge->stats.avg_coherence = 1.0f;
    bridge->stats.min_coherence = 1.0f;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

static const char* quantum_bio_msg_type_names[] = {
    "COHERENCE_UPDATE", "ENTANGLEMENT", "MEASUREMENT",
    "WALK_UPDATE", "ANNEALING_STEP", "GATE_APPLIED",
    "ERROR_DETECTED", "ERROR_CORRECTED", "STATE_PREPARED",
    "AMPLITUDE_ESTIMATE", "REQUEST_QUBITS", "QUERY_STATE"
};

const char* quantum_bio_msg_type_name(quantum_bio_msg_type_t msg_type) {
    if (msg_type >= QUANTUM_BIO_MSG_COUNT) return "UNKNOWN";
    return quantum_bio_msg_type_names[msg_type];
}

void quantum_bio_async_print_summary(const quantum_bio_async_bridge_t* bridge) {
    if (!bridge) {
        printf("Quantum Bio-Async Bridge: NULL\n");
        return;
    }

    printf("Quantum Bio-Async Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Coherence: %.3f (min: %.3f, avg: %.3f)\n",
           bridge->state.coherence_level,
           bridge->stats.min_coherence,
           bridge->stats.avg_coherence);
    printf("  Qubits: %u, Entangled pairs: %u\n",
           bridge->state.num_qubits,
           bridge->state.entangled_pairs);
    printf("  Subscriptions: %u (peak: %u)\n",
           bridge->stats.active_subscriptions,
           bridge->stats.peak_subscriptions);
    printf("  Messages sent: %lu, received: %lu\n",
           (unsigned long)bridge->stats.messages_sent,
           (unsigned long)bridge->stats.messages_received);
    printf("  Broadcasts: %lu (coherence: %lu, entangle: %lu, measure: %lu)\n",
           (unsigned long)bridge->stats.broadcasts_sent,
           (unsigned long)bridge->stats.coherence_updates_sent,
           (unsigned long)bridge->stats.entanglement_events_sent,
           (unsigned long)bridge->stats.measurements_sent);
    printf("  Errors: detected=%lu, decoherence_warnings=%lu\n",
           (unsigned long)bridge->stats.errors_detected,
           (unsigned long)bridge->stats.decoherence_warnings);
    printf("  Routing errors: handler=%lu, routing=%lu\n",
           (unsigned long)bridge->stats.handler_errors,
           (unsigned long)bridge->stats.routing_errors);
}
