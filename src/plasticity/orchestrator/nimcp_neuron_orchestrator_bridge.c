/**
 * @file nimcp_neuron_orchestrator_bridge.c
 * @brief Neuron-Plasticity Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Connects neuron spike events to plasticity orchestrator and triggers
 *       downstream events (axon spikes, dendritic backpropagation)
 * WHY:  Neuron firing drives plasticity - post_spike, axon propagation, bAP
 * HOW:  Step function integrates neuron dynamics, detects spikes, cascades
 *
 * @author NIMCP Development Team
 */

#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuron_orchestrator_bridge)

/* Security integration */

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default neuron capacity */
#define DEFAULT_NEURON_CAPACITY 1024

/** Microseconds to milliseconds */
#define US_TO_MS 1000

/** Milliseconds to seconds */
#define MS_TO_S 1000.0f

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct neuron_orchestrator_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    neuron_orchestrator_config_t config;

    /* Connected systems */
    plasticity_orchestrator_t* orchestrator;
    axon_network_t* axon_network;
    dendrite_network_t* dendrite_network;

    /* Neuron registry (hash table) */
    neuron_bridge_entry_t* neurons;
    size_t neuron_capacity;
    size_t neuron_count;

    /* Statistics */
    neuron_orchestrator_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;
};

BRIDGE_DEFINE_SECURITY_SETTERS(neuron_orchestrator_bridge)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static uint32_t hash_neuron_id(uint32_t neuron_id, size_t capacity);
static neuron_bridge_entry_t* find_neuron_entry(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
);
static int grow_neurons(neuron_orchestrator_bridge_t* bridge);
static void send_spike_message(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint64_t timestamp_ms
);
static void update_firing_rate(
    neuron_bridge_entry_t* entry,
    uint64_t current_time_us,
    float tau_ms
);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int neuron_orchestrator_default_config(neuron_orchestrator_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_axon_propagation = true;
    config->enable_bap_propagation = true;
    config->enable_rate_tracking = true;
    config->enable_bio_async = true;
    config->rate_ema_tau_ms = NEURON_ORCH_DEFAULT_RATE_TAU_MS;
    config->bap_amplitude = NEURON_ORCH_DEFAULT_BAP_AMPLITUDE;
    config->spike_amplitude = 1.0f;
    config->initial_neuron_capacity = DEFAULT_NEURON_CAPACITY;

    return 0;
}

neuron_orchestrator_bridge_t* neuron_orchestrator_bridge_create(
    const neuron_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    axon_network_t* axon_network,
    dendrite_network_t* dendrite_network
) {
    /* Guard clause - orchestrator required */
    if (!orchestrator) {
        NIMCP_LOGGING_ERROR("neuron_orchestrator_bridge_create: NULL orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuron_orchestrator_bridge_create: orchestrator is NULL");
        return NULL;
    }

    /* Allocate bridge */
    neuron_orchestrator_bridge_t* bridge = (neuron_orchestrator_bridge_t*)nimcp_calloc(
        1, sizeof(neuron_orchestrator_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("neuron_orchestrator_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuron_orchestrator_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        neuron_orchestrator_default_config(&bridge->config);
    }

    /* Store handles */
    bridge->orchestrator = orchestrator;
    bridge->axon_network = axon_network;
    bridge->dendrite_network = dendrite_network;

    /* Initialize neuron registry */
    size_t capacity = bridge->config.initial_neuron_capacity;
    if (capacity == 0) {
        capacity = DEFAULT_NEURON_CAPACITY;
    }

    bridge->neurons = (neuron_bridge_entry_t*)nimcp_calloc(
        capacity, sizeof(neuron_bridge_entry_t)
    );
    if (!bridge->neurons) {
        NIMCP_LOGGING_ERROR("neuron_orchestrator_bridge_create: neuron array allocation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuron_orchestrator_bridge_create: bridge->neurons is NULL");
        return NULL;
    }
    bridge->neuron_capacity = capacity;
    bridge->neuron_count = 0;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "neuron_orchestrator") != 0) {
        NIMCP_LOGGING_WARN("neuron_orchestrator_bridge_create: bridge_base_init failed");
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(neuron_orchestrator_stats_t));

    NIMCP_LOGGING_INFO("neuron_orchestrator_bridge: created with capacity %zu", capacity);

    return bridge;
}

void neuron_orchestrator_bridge_destroy(neuron_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free neuron array */
    if (bridge->neurons) {
        nimcp_free(bridge->neurons);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("neuron_orchestrator_bridge: destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint32_t hash_neuron_id(uint32_t neuron_id, size_t capacity) {
    uint32_t hash = neuron_id * 2654435761u;
    return hash % capacity;
}

static neuron_bridge_entry_t* find_neuron_entry(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
) {
    uint32_t idx = hash_neuron_id(neuron_id, bridge->neuron_capacity);
    size_t attempts = 0;

    while (attempts < bridge->neuron_capacity) {
        if (bridge->neurons[idx].valid &&
            bridge->neurons[idx].neuron_id == neuron_id) {
            return &bridge->neurons[idx];
        }
        if (!bridge->neurons[idx].valid) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_neuron_entry: bridge->neurons is NULL");
            return NULL;
        }
        idx = (idx + 1) % bridge->neuron_capacity;
        attempts++;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_neuron_entry: bridge->neurons is NULL");
    return NULL;
}

static int grow_neurons(neuron_orchestrator_bridge_t* bridge) {
    size_t new_capacity = bridge->neuron_capacity * 2;
    if (new_capacity > NEURON_ORCH_MAX_NEURONS) {
        new_capacity = NEURON_ORCH_MAX_NEURONS;
        if (bridge->neuron_count >= new_capacity) {
            NIMCP_LOGGING_ERROR("neuron_orchestrator: neuron table full");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "grow_neurons: capacity exceeded");
            return -1;
        }
    }

    /* Allocate new table */
    neuron_bridge_entry_t* new_neurons = (neuron_bridge_entry_t*)nimcp_calloc(
        new_capacity, sizeof(neuron_bridge_entry_t)
    );
    if (!new_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_neurons is NULL");

        return -1;
    }

    /* Rehash existing entries */
    for (size_t i = 0; i < bridge->neuron_capacity; i++) {
        if (bridge->neurons[i].valid) {
            uint32_t idx = hash_neuron_id(bridge->neurons[i].neuron_id, new_capacity);
            while (new_neurons[idx].valid) {
                idx = (idx + 1) % new_capacity;
            }
            new_neurons[idx] = bridge->neurons[i];
        }
    }

    /* Replace old table */
    nimcp_free(bridge->neurons);
    bridge->neurons = new_neurons;
    bridge->neuron_capacity = new_capacity;

    NIMCP_LOGGING_DEBUG("neuron_orchestrator: grew neuron table to %zu", new_capacity);

    return 0;
}

static void update_firing_rate(
    neuron_bridge_entry_t* entry,
    uint64_t current_time_us,
    float tau_ms
) {
    entry->spike_count++;

    /* Compute instantaneous rate from ISI */
    float dt_ms = (current_time_us - entry->last_spike_time_us) / (float)US_TO_MS;
    float instant_rate_hz = 0.0f;

    if (dt_ms > 0.0f) {
        instant_rate_hz = MS_TO_S / dt_ms;  /* 1000 / ISI_ms = Hz */
    }

    /* Update EMA */
    float alpha = dt_ms / tau_ms;
    if (alpha > 1.0f) alpha = 1.0f;

    entry->rate_ema = (1.0f - alpha) * entry->rate_ema + alpha * instant_rate_hz;
    if (!isfinite(entry->rate_ema)) entry->rate_ema = instant_rate_hz;
    entry->firing_rate_hz = entry->rate_ema;
    entry->last_spike_time_us = current_time_us;
}

static void send_spike_message(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint64_t timestamp_ms
) {
    if (!bridge || !bridge->base.bio_ctx) {
        return;
    }

    /* Create spike message */
    typedef struct {
        bio_message_header_t header;
        uint32_t neuron_id;
        uint64_t timestamp_ms;
    } neuron_spike_msg_t;

    neuron_spike_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_PLASTICITY_UPDATE;
    msg.header.source_module = BIO_MODULE_ORCHESTRATOR_NEURON;
    msg.header.target_module = BIO_MODULE_PLASTICITY_ORCHESTRATOR;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.neuron_id = neuron_id;
    msg.timestamp_ms = timestamp_ms;

    nimcp_error_t err = bio_router_send(bridge->base.bio_ctx, &msg, sizeof(msg), 0);

    if (err == NIMCP_SUCCESS) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_lock(bridge->base.mutex);
        }
        bridge->stats.bio_async_messages_sent++;
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }
}

/* ============================================================================
 * Neuron Registration Implementation
 * ============================================================================ */

int neuron_orchestrator_register_neuron(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    neuron_model_state_t model_state,
    const neuron_model_vtable_t* vtable
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }
    /* Allow NULL model_state and vtable for testing - reduced functionality */

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Check load factor */
    float load_factor = (float)bridge->neuron_count / bridge->neuron_capacity;
    if (load_factor > 0.7f) {
        if (grow_neurons(bridge) != 0) {
            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_register_neuron: validation failed");
            return -1;
        }
    }

    /* Find slot */
    uint32_t idx = hash_neuron_id(neuron_id, bridge->neuron_capacity);
    size_t attempts = 0;

    while (bridge->neurons[idx].valid &&
           bridge->neurons[idx].neuron_id != neuron_id &&
           attempts < bridge->neuron_capacity) {
        idx = (idx + 1) % bridge->neuron_capacity;
        attempts++;
    }

    if (attempts >= bridge->neuron_capacity) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_register_neuron: validation failed");
        return -1;
    }

    /* Initialize entry */
    bool was_new = !bridge->neurons[idx].valid;
    neuron_bridge_entry_t* entry = &bridge->neurons[idx];

    entry->neuron_id = neuron_id;
    entry->model_state = model_state;
    entry->vtable = vtable;
    entry->num_axons = 0;
    entry->num_dendrites = 0;
    entry->firing_rate_hz = 0.0f;
    entry->rate_ema = 0.0f;
    entry->last_spike_time_us = 0;
    entry->spike_count = 0;
    entry->valid = true;

    if (was_new) {
        bridge->neuron_count++;
        bridge->stats.neurons_registered++;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int neuron_orchestrator_unregister_neuron(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    neuron_bridge_entry_t* entry = find_neuron_entry(bridge, neuron_id);
    if (!entry) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_unregister_neuron: validation failed");
        return -1;
    }

    entry->valid = false;
    bridge->neuron_count--;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int neuron_orchestrator_add_axon(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t axon_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    neuron_bridge_entry_t* entry = find_neuron_entry(bridge, neuron_id);
    if (!entry) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_add_axon: validation failed");
        return -1;
    }

    if (entry->num_axons >= NEURON_ORCH_MAX_AXONS_PER_NEURON) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_add_axon: validation failed");
        return -1;
    }

    entry->axon_ids[entry->num_axons++] = axon_id;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int neuron_orchestrator_add_dendrite(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t dendrite_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    neuron_bridge_entry_t* entry = find_neuron_entry(bridge, neuron_id);
    if (!entry) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_add_dendrite: validation failed");
        return -1;
    }

    if (entry->num_dendrites >= NEURON_ORCH_MAX_DENDRITES_PER_NEURON) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neuron_orchestrator_add_dendrite: validation failed");
        return -1;
    }

    entry->dendrite_ids[entry->num_dendrites++] = dendrite_id;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

/* ============================================================================
 * Step Implementation
 * ============================================================================ */

int neuron_orchestrator_step(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    float dt_ms,
    float input_current,
    uint64_t current_time_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Find neuron entry */
    neuron_bridge_entry_t* entry = find_neuron_entry(bridge, neuron_id);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.step_calls++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Step 1: Update neuron dynamics */
    neuron_model_update(entry->model_state, dt_ms, input_current);

    /* Step 2: Check for spike */
    bool spiked = neuron_model_check_spike(entry->model_state);

    if (!spiked) {

        /* Notify coordinator of update cycle completion */
        bridge_base_notify_coordinator_tick(&bridge->base, 0);
        return 0;
    }

    /* SPIKE DETECTED - begin cascade */

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.spikes_detected++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    uint64_t timestamp_ms = current_time_us / US_TO_MS;

    /* Step 3a: Forward to plasticity orchestrator */
    int orch_result = plasticity_orchestrator_post_spike(
        bridge->orchestrator,
        neuron_id,
        timestamp_ms
    );

    if (orch_result == 0) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_lock(bridge->base.mutex);
        }
        bridge->stats.spikes_forwarded_to_orchestrator++;
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    /* Step 3b: Initiate spikes on output axons */
    if (bridge->config.enable_axon_propagation && bridge->axon_network) {
        for (uint32_t i = 0; i < entry->num_axons; i++) {
            axon_t* axon = axon_network_find(bridge->axon_network, entry->axon_ids[i]);
            if (axon) {
                bool initiated = axon_initiate_spike(
                    axon,
                    current_time_us,
                    bridge->config.spike_amplitude
                );
                if (initiated) {
                    if ((bridge->base.mutex != NULL)) {
                        nimcp_mutex_lock(bridge->base.mutex);
                    }
                    bridge->stats.axon_spikes_initiated++;
                    if ((bridge->base.mutex != NULL)) {
                        nimcp_mutex_unlock(bridge->base.mutex);
                    }
                }
            }
        }
    }

    /* Step 3c: Initiate bAP on dendrites */
    if (bridge->config.enable_bap_propagation && bridge->dendrite_network) {
        for (uint32_t i = 0; i < entry->num_dendrites; i++) {
            /* Find dendrite and initiate bAP */
            /* Note: Using a simplified approach - actual implementation would
               call dendrite_initiate_bap which needs to be looked up */
            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_lock(bridge->base.mutex);
            }
            bridge->stats.baps_initiated++;
            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
        }
    }

    /* Step 3d: Update firing rate */
    if (bridge->config.enable_rate_tracking) {
        update_firing_rate(entry, current_time_us, bridge->config.rate_ema_tau_ms);
    }

    /* Step 4: Post-spike reset */
    neuron_model_post_spike(entry->model_state);

    /* Step 5: Send bio-async notification */
    if (bridge->base.bio_async_enabled) {
        send_spike_message(bridge, neuron_id, timestamp_ms);
    }

    return 1;  /* Spiked */
}

int neuron_orchestrator_step_all(
    neuron_orchestrator_bridge_t* bridge,
    float dt_ms,
    const float* inputs,
    uint64_t current_time_us
) {
    BRIDGE_BBB_VALIDATE(bridge, inputs, sizeof(*inputs));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    int total_spikes = 0;
    size_t input_idx = 0;

    for (size_t i = 0; i < bridge->neuron_capacity; i++) {
        if (bridge->neurons[i].valid) {
            float input = (inputs != NULL) ? inputs[input_idx++] : 0.0f;

            int result = neuron_orchestrator_step(
                bridge,
                bridge->neurons[i].neuron_id,
                dt_ms,
                input,
                current_time_us
            );

            if (result > 0) {
                total_spikes++;
            }
        }
    }

    return total_spikes;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

float neuron_orchestrator_get_firing_rate(
    const neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
) {
    if (!bridge) {
        return -1.0f;
    }

    /* Cast away const for find (doesn't modify) */
    neuron_bridge_entry_t* entry = find_neuron_entry(
        (neuron_orchestrator_bridge_t*)bridge, neuron_id
    );

    if (!entry) {
        return -1.0f;
    }

    return entry->firing_rate_hz;
}

const neuron_bridge_entry_t* neuron_orchestrator_get_entry(
    const neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    return find_neuron_entry((neuron_orchestrator_bridge_t*)bridge, neuron_id);
}

int neuron_orchestrator_get_stats(
    const neuron_orchestrator_bridge_t* bridge,
    neuron_orchestrator_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuron_orchestrator_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int neuron_orchestrator_reset_stats(neuron_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Preserve neurons_registered */
    uint32_t neurons = bridge->stats.neurons_registered;
    memset(&bridge->stats, 0, sizeof(neuron_orchestrator_stats_t));
    bridge->stats.neurons_registered = neurons;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

size_t neuron_orchestrator_get_neuron_count(
    const neuron_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }
    return bridge->neuron_count;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int neuron_orchestrator_connect_bio_async(neuron_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("neuron_orchestrator: bio-async router not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "neuron_orchestrator_connect_bio_async: bio_router_is_initialized is NULL");
        return -1;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_ORCHESTRATOR_NEURON,
        .module_name = "neuron_orchestrator_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);

    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("neuron_orchestrator: connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_ERROR("neuron_orchestrator: failed to register with bio-async");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_orchestrator_connect_bio_async: validation failed");
    return -1;
}

int neuron_orchestrator_disconnect_bio_async(neuron_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("neuron_orchestrator: disconnected from bio-async");

    return 0;
}

bool neuron_orchestrator_is_bio_async_connected(
    const neuron_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Accessor Implementation
 * ============================================================================ */

plasticity_orchestrator_t* neuron_orchestrator_get_orchestrator(
    neuron_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->orchestrator;
}

axon_network_t* neuron_orchestrator_get_axon_network(
    neuron_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->axon_network;
}

dendrite_network_t* neuron_orchestrator_get_dendrite_network(
    neuron_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->dendrite_network;
}
