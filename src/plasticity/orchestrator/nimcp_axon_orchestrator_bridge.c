/**
 * @file nimcp_axon_orchestrator_bridge.c
 * @brief Axon-Plasticity Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Connects axon spike arrivals to plasticity orchestrator pre_spike events
 * WHY:  Axon conduction delays critically affect STDP timing
 * HOW:  Registers callback with axon network, forwards arrivals to orchestrator
 *
 * @author NIMCP Development Team
 */

#include "plasticity/orchestrator/nimcp_axon_orchestrator_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/math/nimcp_math_helpers.h"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(axon_orchestrator_bridge)

/* Security integration */

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default mapping capacity */
#define DEFAULT_MAPPING_CAPACITY 1024

/** Activity EMA decay factor per ms */
#define ACTIVITY_EMA_DECAY_FACTOR 0.99f

/** Microseconds to milliseconds */
#define US_TO_MS 1000

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Synapse-to-axon mapping entry
 */
typedef struct {
    uint32_t synapse_id;
    uint32_t axon_id;
    bool valid;
} synapse_axon_mapping_t;

/**
 * @brief Activity tracking per axon
 */
typedef struct {
    uint32_t axon_id;
    float activity_ema;
    uint64_t last_spike_time_us;
    uint32_t spike_count;
} axon_activity_entry_t;

/**
 * @brief Internal bridge structure
 */
struct axon_orchestrator_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    axon_orchestrator_config_t config;

    /* Orchestrator and axon network handles */
    plasticity_orchestrator_t* orchestrator;
    axon_network_t* axon_network;

    /* Synapse-to-axon mappings (hash table) */
    synapse_axon_mapping_t* mappings;
    size_t mapping_capacity;
    size_t mapping_count;

    /* Activity tracking (optional) */
    axon_activity_entry_t* activity_entries;
    size_t activity_capacity;

    /* Statistics */
    axon_orchestrator_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Memory management */
    unified_mem_manager_t* mem_manager;
    bool owns_mem_manager;
};

BRIDGE_DEFINE_SECURITY_SETTERS(axon_orchestrator_bridge)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static uint32_t hash_synapse_id(uint32_t synapse_id, size_t capacity);
static int grow_mappings(axon_orchestrator_bridge_t* bridge);
static void send_bio_async_spike_message(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_ms
);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int axon_orchestrator_default_config(axon_orchestrator_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_delay_compensation = false;  /* Use arrival time by default */
    config->enable_activity_tracking = true;
    config->enable_bio_async = true;
    config->activity_ema_tau_ms = AXON_ORCH_DEFAULT_ACTIVITY_TAU_MS;
    config->initial_mapping_capacity = DEFAULT_MAPPING_CAPACITY;

    return 0;
}

axon_orchestrator_bridge_t* axon_orchestrator_bridge_create(
    const axon_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    axon_network_t* axon_network
) {
    /* Guard clauses */
    if (!orchestrator) {
        NIMCP_LOGGING_ERROR("axon_orchestrator_bridge_create: NULL orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_orchestrator_bridge_create: orchestrator is NULL");
        return NULL;
    }
    /* Allow NULL axon_network for testing - will operate with reduced functionality */
    if (!axon_network) {
        NIMCP_LOGGING_WARN("axon_orchestrator_bridge_create: NULL axon_network (reduced functionality)");
    }

    /* Allocate bridge structure */
    axon_orchestrator_bridge_t* bridge = (axon_orchestrator_bridge_t*)nimcp_calloc(
        1, sizeof(axon_orchestrator_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("axon_orchestrator_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_orchestrator_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        axon_orchestrator_default_config(&bridge->config);
    }

    /* Store handles */
    bridge->orchestrator = orchestrator;
    bridge->axon_network = axon_network;

    /* Initialize mapping table */
    size_t capacity = bridge->config.initial_mapping_capacity;
    if (capacity == 0) {
        capacity = DEFAULT_MAPPING_CAPACITY;
    }

    bridge->mappings = (synapse_axon_mapping_t*)nimcp_calloc(
        capacity, sizeof(synapse_axon_mapping_t)
    );
    if (!bridge->mappings) {
        NIMCP_LOGGING_ERROR("axon_orchestrator_bridge_create: mapping allocation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "axon_orchestrator_bridge_create: bridge->mappings is NULL");
        return NULL;
    }
    bridge->mapping_capacity = capacity;
    bridge->mapping_count = 0;

    /* Initialize activity tracking if enabled */
    if (bridge->config.enable_activity_tracking) {
        bridge->activity_entries = (axon_activity_entry_t*)nimcp_calloc(
            capacity, sizeof(axon_activity_entry_t)
        );
        bridge->activity_capacity = capacity;
    }

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "axon_orchestrator") != 0) {
        NIMCP_LOGGING_WARN("axon_orchestrator_bridge_create: bridge_base_init failed");
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(axon_orchestrator_stats_t));

    NIMCP_LOGGING_INFO("axon_orchestrator_bridge: created with capacity %zu", capacity);

    return bridge;
}

void axon_orchestrator_bridge_destroy(axon_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free activity entries */
    if (bridge->activity_entries) {
        nimcp_free(bridge->activity_entries);
    }

    /* Free mappings */
    if (bridge->mappings) {
        nimcp_free(bridge->mappings);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("axon_orchestrator_bridge: destroyed");
}

/* ============================================================================
 * Mapping Implementation
 * ============================================================================ */

/**
 * @brief Simple hash function for synapse IDs
 */
static uint32_t hash_synapse_id(uint32_t synapse_id, size_t capacity) {
    /* Multiplicative hash */
    uint32_t hash = synapse_id * 2654435761u;
    return hash % capacity;
}

/**
 * @brief Grow the mapping table when load factor exceeds threshold
 */
static int grow_mappings(axon_orchestrator_bridge_t* bridge) {
    size_t new_capacity = bridge->mapping_capacity * 2;
    if (new_capacity > AXON_ORCH_MAX_MAPPINGS) {
        new_capacity = AXON_ORCH_MAX_MAPPINGS;
        if (bridge->mapping_count >= new_capacity) {
            NIMCP_LOGGING_ERROR("axon_orchestrator: mapping table full");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "grow_mappings: capacity exceeded");
            return -1;
        }
    }

    /* Allocate new table */
    synapse_axon_mapping_t* new_mappings = (synapse_axon_mapping_t*)nimcp_calloc(
        new_capacity, sizeof(synapse_axon_mapping_t)
    );
    if (!new_mappings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_mappings is NULL");

        return -1;
    }

    /* Rehash existing entries */
    for (size_t i = 0; i < bridge->mapping_capacity; i++) {
        if (bridge->mappings[i].valid) {
            uint32_t idx = hash_synapse_id(bridge->mappings[i].synapse_id, new_capacity);
            while (new_mappings[idx].valid) {
                idx = (idx + 1) % new_capacity;
            }
            new_mappings[idx] = bridge->mappings[i];
        }
    }

    /* Replace old table */
    nimcp_free(bridge->mappings);
    bridge->mappings = new_mappings;
    bridge->mapping_capacity = new_capacity;

    NIMCP_LOGGING_DEBUG("axon_orchestrator: grew mapping table to %zu", new_capacity);

    return 0;
}

int axon_orchestrator_map_synapse(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t axon_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Check if we need to grow */
    float load_factor = (bridge->mapping_capacity > 0) ? (float)bridge->mapping_count / bridge->mapping_capacity : 1.0f;
    if (load_factor > 0.7f) {
        if (grow_mappings(bridge) != 0) {
            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "axon_orchestrator_map_synapse: validation failed");
            return -1;
        }
    }

    /* Find slot (linear probing) */
    uint32_t idx = hash_synapse_id(synapse_id, bridge->mapping_capacity);
    size_t attempts = 0;

    while (bridge->mappings[idx].valid &&
           bridge->mappings[idx].synapse_id != synapse_id &&
           attempts < bridge->mapping_capacity) {
        idx = (idx + 1) % bridge->mapping_capacity;
        attempts++;
    }

    if (attempts >= bridge->mapping_capacity) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "axon_orchestrator_map_synapse: validation failed");
        return -1;
    }

    /* Insert or update */
    bool was_new = !bridge->mappings[idx].valid;
    bridge->mappings[idx].synapse_id = synapse_id;
    bridge->mappings[idx].axon_id = axon_id;
    bridge->mappings[idx].valid = true;

    if (was_new) {
        bridge->mapping_count++;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int axon_orchestrator_unmap_synapse(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Find the entry */
    uint32_t idx = hash_synapse_id(synapse_id, bridge->mapping_capacity);
    size_t attempts = 0;

    while (bridge->mappings[idx].valid && attempts < bridge->mapping_capacity) {
        if (bridge->mappings[idx].synapse_id == synapse_id) {
            /* Mark as invalid (tombstone) */
            bridge->mappings[idx].valid = false;
            bridge->mapping_count--;

            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            return 0;
        }
        idx = (idx + 1) % bridge->mapping_capacity;
        attempts++;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "axon_orchestrator_unmap_synapse: validation failed");
    return -1;  /* Not found */
}

uint32_t axon_orchestrator_get_axon_for_synapse(
    const axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        return UINT32_MAX;
    }

    uint32_t idx = hash_synapse_id(synapse_id, bridge->mapping_capacity);
    size_t attempts = 0;

    while (bridge->mappings[idx].valid && attempts < bridge->mapping_capacity) {
        if (bridge->mappings[idx].synapse_id == synapse_id) {
            return bridge->mappings[idx].axon_id;
        }
        idx = (idx + 1) % bridge->mapping_capacity;
        attempts++;
    }

    return UINT32_MAX;  /* Not found */
}

/* ============================================================================
 * Spike Handling Implementation
 * ============================================================================ */

void axon_orchestrator_spike_callback(
    axon_t* axon,
    const axon_spike_event_t* spike,
    void* user_data
) {
    axon_orchestrator_bridge_t* bridge = (axon_orchestrator_bridge_t*)user_data;

    /* Guard clauses */
    if (!bridge || !spike) {
        return;
    }

    /* Get effective time for STDP */
    uint64_t effective_time_ms = axon_orchestrator_get_effective_time_ms(bridge, spike);

    /* Forward to plasticity orchestrator */
    int result = plasticity_orchestrator_pre_spike(
        bridge->orchestrator,
        spike->target_synapse_id,
        effective_time_ms
    );

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->stats.spikes_forwarded++;

    if (bridge->config.enable_delay_compensation) {
        bridge->stats.spikes_delay_compensated++;
    }

    if (result != 0) {
        bridge->stats.spikes_unmapped++;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Send bio-async message if enabled */
    if (bridge->base.bio_async_enabled) {
        send_bio_async_spike_message(bridge, spike->target_synapse_id, effective_time_ms);
    }

    /* Update activity tracking if enabled */
    if (bridge->config.enable_activity_tracking && bridge->activity_entries) {
        /* Simple activity EMA update */
        uint32_t axon_idx = spike->axon_id % bridge->activity_capacity;
        axon_activity_entry_t* entry = &bridge->activity_entries[axon_idx];

        if (entry->axon_id != spike->axon_id) {
            /* New axon - initialize */
            entry->axon_id = spike->axon_id;
            entry->activity_ema = 0.0f;
            entry->spike_count = 0;
        }

        /* Update EMA: instantaneous spike (value 1.0) mixed with decay */
        float dt_ms = (spike->arrival_time - entry->last_spike_time_us) / (float)US_TO_MS;
        float decay = nimcp_safe_expf(-dt_ms / bridge->config.activity_ema_tau_ms);
        entry->activity_ema = decay * entry->activity_ema + (1.0f - decay) * 1.0f;
        if (!isfinite(entry->activity_ema)) entry->activity_ema = 1.0f;
        entry->last_spike_time_us = spike->arrival_time;
        entry->spike_count++;
    }
}

uint64_t axon_orchestrator_get_effective_time_ms(
    const axon_orchestrator_bridge_t* bridge,
    const axon_spike_event_t* spike
) {
    if (!bridge || !spike) {
        return 0;
    }

    /* Choose initiation or arrival time based on config */
    uint64_t time_us;
    if (bridge->config.enable_delay_compensation) {
        /* Use initiation time (compensate for axon delay) */
        time_us = spike->initiation_time;
    } else {
        /* Use arrival time (default - more biologically accurate for STDP) */
        time_us = spike->arrival_time;
    }

    /* Convert to milliseconds */
    return time_us / US_TO_MS;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int axon_orchestrator_bridge_update(
    axon_orchestrator_bridge_t* bridge,
    uint64_t current_time_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    uint32_t spikes_processed = 0;

    /* Process arriving spikes through axon network (if available) */
    if (bridge->axon_network) {
        spikes_processed = axon_network_process_arrivals(
            bridge->axon_network,
            current_time_us,
            axon_orchestrator_spike_callback,
            bridge
        );
    }

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->stats.update_calls++;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return (int)spikes_processed;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int axon_orchestrator_get_stats(
    const axon_orchestrator_bridge_t* bridge,
    axon_orchestrator_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_orchestrator_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int axon_orchestrator_reset_stats(axon_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    memset(&bridge->stats, 0, sizeof(axon_orchestrator_stats_t));

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

size_t axon_orchestrator_get_mapping_count(
    const axon_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }
    return bridge->mapping_count;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Send bio-async message for spike arrival
 */
static void send_bio_async_spike_message(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_ms
) {
    if (!bridge || !bridge->base.bio_ctx) {
        return;
    }

    /* Create spike arrival message */
    typedef struct {
        bio_message_header_t header;
        uint32_t synapse_id;
        uint64_t timestamp_ms;
    } spike_arrival_msg_t;

    spike_arrival_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Initialize header */
    msg.header.type = BIO_MSG_PLASTICITY_UPDATE;  /* Using existing message type */
    msg.header.source_module = BIO_MODULE_ORCHESTRATOR_AXON;
    msg.header.target_module = BIO_MODULE_PLASTICITY_ORCHESTRATOR;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.synapse_id = synapse_id;
    msg.timestamp_ms = timestamp_ms;

    /* Send message */
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

int axon_orchestrator_connect_bio_async(axon_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Check if already connected */
    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("axon_orchestrator: bio-async router not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "axon_orchestrator_connect_bio_async: bio_router_is_initialized is NULL");
        return -1;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_ORCHESTRATOR_AXON,
        .module_name = "axon_orchestrator_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);

    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("axon_orchestrator: connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_ERROR("axon_orchestrator: failed to register with bio-async");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "axon_orchestrator_connect_bio_async: validation failed");
    return -1;
}

int axon_orchestrator_disconnect_bio_async(axon_orchestrator_bridge_t* bridge) {
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
    NIMCP_LOGGING_INFO("axon_orchestrator: disconnected from bio-async router");

    return 0;
}

bool axon_orchestrator_is_bio_async_connected(
    const axon_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Accessor Implementation
 * ============================================================================ */

plasticity_orchestrator_t* axon_orchestrator_get_orchestrator(
    axon_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->orchestrator;
}

axon_network_t* axon_orchestrator_get_axon_network(
    axon_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->axon_network;
}
