/**
 * @file nimcp_dendrite_orchestrator_bridge.c
 * @brief Dendrite-Plasticity Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional bridge between dendritic spines and plasticity orchestrator
 * WHY:  Spines are the physical substrate of synapses - state must sync
 * HOW:  Maps synapse_id to spine, syncs weight/structural changes bidirectionally
 *
 * @author NIMCP Development Team
 */

#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
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

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendrite_orchestrator_bridge)

/* Security integration */

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default mapping capacity */
#define DEFAULT_MAPPING_CAPACITY 1024

/** Microseconds to milliseconds */
#define US_TO_MS 1000

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct dendrite_orchestrator_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    dendrite_orchestrator_config_t config;

    /* Connected systems */
    plasticity_orchestrator_t* orchestrator;
    dendrite_network_t* dendrite_network;

    /* Spine-synapse mappings (hash table) */
    spine_synapse_mapping_t* mappings;
    size_t mapping_capacity;
    size_t mapping_count;

    /* Statistics */
    dendrite_orchestrator_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;
};

BRIDGE_DEFINE_SECURITY_SETTERS(dendrite_orchestrator_bridge)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static uint32_t hash_synapse_id(uint32_t synapse_id, size_t capacity);
static spine_synapse_mapping_t* find_mapping(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);
static int grow_mappings(dendrite_orchestrator_bridge_t* bridge);
static void send_sync_message(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    dendrite_sync_direction_t direction
);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int dendrite_orchestrator_default_config(dendrite_orchestrator_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_weight_to_spine_sync = true;
    config->enable_spine_to_orchestrator_sync = true;
    config->enable_pre_spike_forwarding = true;
    config->enable_bio_async = true;
    config->weight_to_volume_scale = DENDRITE_ORCH_DEFAULT_WEIGHT_VOLUME_SCALE;
    config->weight_to_ampa_scale = DENDRITE_ORCH_DEFAULT_AMPA_SCALE;
    config->min_weight_delta_for_sync = 0.001f;
    config->initial_mapping_capacity = DEFAULT_MAPPING_CAPACITY;

    return 0;
}

dendrite_orchestrator_bridge_t* dendrite_orchestrator_bridge_create(
    const dendrite_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    dendrite_network_t* dendrite_network
) {
    /* Guard clauses */
    if (!orchestrator) {
        NIMCP_LOGGING_ERROR("dendrite_orchestrator_bridge_create: NULL orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_orchestrator_bridge_create: orchestrator is NULL");
        return NULL;
    }
    /* Allow NULL dendrite_network for testing - will operate with reduced functionality */
    if (!dendrite_network) {
        NIMCP_LOGGING_WARN("dendrite_orchestrator_bridge_create: NULL dendrite_network (reduced functionality)");
    }

    /* Allocate bridge */
    dendrite_orchestrator_bridge_t* bridge = (dendrite_orchestrator_bridge_t*)nimcp_calloc(
        1, sizeof(dendrite_orchestrator_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("dendrite_orchestrator_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_orchestrator_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        dendrite_orchestrator_default_config(&bridge->config);
    }

    /* Store handles */
    bridge->orchestrator = orchestrator;
    bridge->dendrite_network = dendrite_network;

    /* Initialize mapping table */
    size_t capacity = bridge->config.initial_mapping_capacity;
    if (capacity == 0) {
        capacity = DEFAULT_MAPPING_CAPACITY;
    }

    bridge->mappings = (spine_synapse_mapping_t*)nimcp_calloc(
        capacity, sizeof(spine_synapse_mapping_t)
    );
    if (!bridge->mappings) {
        NIMCP_LOGGING_ERROR("dendrite_orchestrator_bridge_create: mapping allocation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_orchestrator_bridge_create: bridge->mappings is NULL");
        return NULL;
    }
    bridge->mapping_capacity = capacity;
    bridge->mapping_count = 0;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "dendrite_orchestrator") != 0) {
        NIMCP_LOGGING_WARN("dendrite_orchestrator_bridge_create: bridge_base_init failed");
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(dendrite_orchestrator_stats_t));

    NIMCP_LOGGING_INFO("dendrite_orchestrator_bridge: created with capacity %zu", capacity);

    return bridge;
}

void dendrite_orchestrator_bridge_destroy(dendrite_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free mappings */
    if (bridge->mappings) {
        nimcp_free(bridge->mappings);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("dendrite_orchestrator_bridge: destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint32_t hash_synapse_id(uint32_t synapse_id, size_t capacity) {
    uint32_t hash = synapse_id * 2654435761u;
    return hash % capacity;
}

static spine_synapse_mapping_t* find_mapping(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    uint32_t idx = hash_synapse_id(synapse_id, bridge->mapping_capacity);
    size_t attempts = 0;

    while (attempts < bridge->mapping_capacity) {
        if (bridge->mappings[idx].valid &&
            bridge->mappings[idx].synapse_id == synapse_id) {
            return &bridge->mappings[idx];
        }
        if (!bridge->mappings[idx].valid) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_mapping: bridge->mappings is NULL");
            return NULL;
        }
        idx = (idx + 1) % bridge->mapping_capacity;
        attempts++;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_mapping: bridge->mappings is NULL");
    return NULL;
}

static int grow_mappings(dendrite_orchestrator_bridge_t* bridge) {
    size_t new_capacity = bridge->mapping_capacity * 2;
    if (new_capacity > DENDRITE_ORCH_MAX_MAPPINGS) {
        new_capacity = DENDRITE_ORCH_MAX_MAPPINGS;
        if (bridge->mapping_count >= new_capacity) {
            NIMCP_LOGGING_ERROR("dendrite_orchestrator: mapping table full");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "grow_mappings: capacity exceeded");
            return -1;
        }
    }

    /* Allocate new table */
    spine_synapse_mapping_t* new_mappings = (spine_synapse_mapping_t*)nimcp_calloc(
        new_capacity, sizeof(spine_synapse_mapping_t)
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

    NIMCP_LOGGING_DEBUG("dendrite_orchestrator: grew mapping table to %zu", new_capacity);

    return 0;
}

static void send_sync_message(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    dendrite_sync_direction_t direction
) {
    if (!bridge || !bridge->base.bio_ctx) {
        return;
    }

    /* Create sync message */
    typedef struct {
        bio_message_header_t header;
        uint32_t synapse_id;
        uint8_t direction;
    } spine_sync_msg_t;

    spine_sync_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_PLASTICITY_UPDATE;
    msg.header.source_module = BIO_MODULE_ORCHESTRATOR_DENDRITE;
    msg.header.target_module = BIO_MODULE_PLASTICITY_ORCHESTRATOR;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.synapse_id = synapse_id;
    msg.direction = (uint8_t)direction;

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
 * Mapping Implementation
 * ============================================================================ */

int dendrite_orchestrator_map_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t dendrite_id,
    uint32_t spine_index
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Check load factor */
    float load_factor = (bridge->mapping_capacity > 0) ? (float)bridge->mapping_count / bridge->mapping_capacity : 1.0f;
    if (load_factor > 0.7f) {
        if (grow_mappings(bridge) != 0) {
            if ((bridge->base.mutex != NULL)) {
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dendrite_orchestrator_map_spine: validation failed");
            return -1;
        }
    }

    /* Find slot */
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dendrite_orchestrator_map_spine: validation failed");
        return -1;
    }

    /* Insert or update */
    bool was_new = !bridge->mappings[idx].valid;

    bridge->mappings[idx].synapse_id = synapse_id;
    bridge->mappings[idx].dendrite_id = dendrite_id;
    bridge->mappings[idx].spine_index = spine_index;
    bridge->mappings[idx].valid = true;

    if (was_new) {
        bridge->mapping_count++;
        bridge->stats.spines_registered++;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int dendrite_orchestrator_unmap_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    spine_synapse_mapping_t* mapping = find_mapping(bridge, synapse_id);
    if (!mapping) {
        if ((bridge->base.mutex != NULL)) {
            nimcp_mutex_unlock(bridge->base.mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dendrite_orchestrator_unmap_spine: validation failed");
        return -1;
    }

    mapping->valid = false;
    bridge->mapping_count--;
    bridge->stats.spines_eliminated++;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int dendrite_orchestrator_get_spine_mapping(
    const dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t* dendrite_id_out,
    uint32_t* spine_index_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    spine_synapse_mapping_t* mapping = find_mapping(
        (dendrite_orchestrator_bridge_t*)bridge, synapse_id
    );

    if (!mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mapping is NULL");


        return -1;
    }

    if (dendrite_id_out) {
        *dendrite_id_out = mapping->dendrite_id;
    }
    if (spine_index_out) {
        *spine_index_out = mapping->spine_index;
    }

    return 0;
}

/* ============================================================================
 * Synchronization Implementation
 * ============================================================================ */

int dendrite_orchestrator_sync_weight_to_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Find mapping */
    spine_synapse_mapping_t* mapping = find_mapping(bridge, synapse_id);
    if (!mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mapping is NULL");

        return -1;
    }

    /* Get weight from orchestrator */
    float weight = plasticity_orchestrator_get_weight(bridge->orchestrator, synapse_id);
    if (isnan(weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_orchestrator_sync_weight_to_spine: validation failed");
        return -1;
    }

    /* TODO: Find dendrite and spine, update morphology
     * This requires dendrite_network_find() and spine access functions
     * which would update:
     * - spine->synaptic_weight = weight
     * - spine->head_diameter += weight_delta * volume_scale
     * - spine->ampa_receptors += weight_delta * ampa_scale
     */

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.weight_to_spine_syncs++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Send bio-async notification */
    if (bridge->base.bio_async_enabled) {
        send_sync_message(bridge, synapse_id, SYNC_ORCHESTRATOR_TO_SPINE);
    }

    return 0;
}

int dendrite_orchestrator_sync_spine_to_orchestrator(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Find mapping */
    spine_synapse_mapping_t* mapping = find_mapping(bridge, synapse_id);
    if (!mapping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mapping is NULL");

        return -1;
    }

    /* TODO: Get spine state and update orchestrator
     * This requires:
     * 1. Find dendrite via dendrite_network
     * 2. Get spine at spine_index
     * 3. Check spine state:
     *    - SPINE_STATE_POTENTIATED → increase weight
     *    - SPINE_STATE_DEPRESSED → decrease weight
     *    - SPINE_STATE_ELIMINATED → remove synapse
     * 4. Update orchestrator via plasticity_orchestrator_set_weight()
     */

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.spine_to_orchestrator_syncs++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Send bio-async notification */
    if (bridge->base.bio_async_enabled) {
        send_sync_message(bridge, synapse_id, SYNC_SPINE_TO_ORCHESTRATOR);
    }

    return 0;
}

int dendrite_orchestrator_sync_all(
    dendrite_orchestrator_bridge_t* bridge,
    dendrite_sync_direction_t direction
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    int synced = 0;

    for (size_t i = 0; i < bridge->mapping_capacity; i++) {
        if (bridge->mappings[i].valid) {
            int result;
            if (direction == SYNC_ORCHESTRATOR_TO_SPINE) {
                result = dendrite_orchestrator_sync_weight_to_spine(
                    bridge, bridge->mappings[i].synapse_id
                );
            } else {
                result = dendrite_orchestrator_sync_spine_to_orchestrator(
                    bridge, bridge->mappings[i].synapse_id
                );
            }

            if (result == 0) {
                synced++;
            }
        }
    }

    return synced;
}

/* ============================================================================
 * Event Handling Implementation
 * ============================================================================ */

int dendrite_orchestrator_pre_spike(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_pre_spike_forwarding) {
        return 0;
    }

    /* Forward to orchestrator */
    int result = plasticity_orchestrator_pre_spike(
        bridge->orchestrator,
        synapse_id,
        timestamp_ms
    );

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.pre_spikes_forwarded++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return result;
}

int dendrite_orchestrator_spine_formed(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t dendrite_id,
    uint32_t spine_index,
    uint32_t synapse_id,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Map the new spine */
    int result = dendrite_orchestrator_map_spine(
        bridge, synapse_id, dendrite_id, spine_index
    );

    if (result != 0) {
        return result;
    }

    /* Initialize weight in orchestrator */
    result = plasticity_orchestrator_set_weight(
        bridge->orchestrator,
        synapse_id,
        initial_weight
    );

    return result;
}

int dendrite_orchestrator_spine_eliminated(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Remove mapping */
    int result = dendrite_orchestrator_unmap_spine(bridge, synapse_id);

    if (result != 0) {
        return result;
    }

    /* Set weight to 0 in orchestrator (or remove synapse if supported) */
    result = plasticity_orchestrator_set_weight(
        bridge->orchestrator,
        synapse_id,
        0.0f
    );

    return result;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int dendrite_orchestrator_bridge_update(
    dendrite_orchestrator_bridge_t* bridge,
    uint64_t current_time_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    (void)current_time_us;  /* May be used for periodic sync */

    /* Update statistics */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    bridge->stats.update_calls++;
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int dendrite_orchestrator_get_stats(
    const dendrite_orchestrator_bridge_t* bridge,
    dendrite_orchestrator_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_orchestrator_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int dendrite_orchestrator_reset_stats(dendrite_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }

    /* Preserve spines_registered */
    uint64_t registered = bridge->stats.spines_registered;
    memset(&bridge->stats, 0, sizeof(dendrite_orchestrator_stats_t));
    bridge->stats.spines_registered = registered;

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

size_t dendrite_orchestrator_get_mapping_count(
    const dendrite_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }
    return bridge->mapping_count;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int dendrite_orchestrator_connect_bio_async(dendrite_orchestrator_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("dendrite_orchestrator: bio-async router not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "dendrite_orchestrator_connect_bio_async: bio_router_is_initialized is NULL");
        return -1;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_ORCHESTRATOR_DENDRITE,
        .module_name = "dendrite_orchestrator_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);

    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("dendrite_orchestrator: connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_ERROR("dendrite_orchestrator: failed to register with bio-async");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_orchestrator_connect_bio_async: validation failed");
    return -1;
}

int dendrite_orchestrator_disconnect_bio_async(dendrite_orchestrator_bridge_t* bridge) {
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
    NIMCP_LOGGING_INFO("dendrite_orchestrator: disconnected from bio-async");

    return 0;
}

bool dendrite_orchestrator_is_bio_async_connected(
    const dendrite_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Accessor Implementation
 * ============================================================================ */

plasticity_orchestrator_t* dendrite_orchestrator_get_orchestrator(
    dendrite_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->orchestrator;
}

dendrite_network_t* dendrite_orchestrator_get_dendrite_network(
    dendrite_orchestrator_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }
    return bridge->dendrite_network;
}
