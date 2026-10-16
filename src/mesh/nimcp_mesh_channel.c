/**
 * @file nimcp_mesh_channel.c
 * @brief Mesh Network Channel Implementation
 *
 * WHAT: Implementation of channel structure with world state and gossip
 * WHY:  Enable isolated ledger domains for brain regions
 * HOW:  Integrates collective_workspace (CRDT) and kg_module_wiring (KG)
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_channel.h"
#include "swarm/nimcp_collective_workspace.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

/* P3-D: Named constant instead of magic number for belief array capacity */
#define MESH_DEFAULT_BELIEF_CAPACITY 64

/* ============================================================================
 * BBB Integration for Mesh Channel
 * ============================================================================ */

/** Global BBB system for mesh channel module - thread-safe access */
static _Atomic(bbb_system_t) g_mesh_channel_bbb = NULL;

/* Health agent boilerplate - thread-safe for mesh module */
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(mesh_channel, MESH_ADAPTER_CATEGORY_SYSTEM)


/**
 * @brief Set BBB system for mesh channel validation
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_channel_set_bbb(bbb_system_t bbb) {
    atomic_store(&g_mesh_channel_bbb, bbb);
}

/**
 * @brief Get current BBB system for mesh channel
 * @return BBB system or NULL
 */
bbb_system_t mesh_channel_get_bbb(void) {
    return atomic_load(&g_mesh_channel_bbb);
}

/**
 * @brief Validate belief data using BBB before processing
 *
 * WHAT: Validate incoming belief data for security threats
 * WHY:  Prevent malicious data injection via beliefs
 * HOW:  Use BBB input validation on belief content
 *
 * @param belief Belief to validate
 * @return true if valid, false if threat detected
 */
static bool validate_belief_bbb(const mesh_belief_t* belief) {
    if (!belief) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_belief_bbb: belief is NULL");
        return false;
    }

    bbb_system_t bbb = atomic_load(&g_mesh_channel_bbb);
    if (!bbb) return true;  /* BBB not configured, allow */

    bbb_validation_result_t result;

    /* Validate belief vector data */
    bool valid = bbb_validate_input(
        bbb,
        belief->belief_vector,
        belief->vector_dim * sizeof(float),
        &result
    );

    if (!valid) {
        LOG_WARN("BBB rejected belief %u from source 0x%lx: %s (threat=%s, severity=%s)",
                 belief->belief_id,
                 (unsigned long)belief->source,
                 result.reason,
                 bbb_threat_type_name(result.threat),
                 bbb_severity_name(result.severity));

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected incoming belief data");
        return false;
    }

    return true;
}

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Mesh channel structure
 */
struct mesh_channel {
    uint32_t magic;                          /**< Magic for validation */
    char name[MESH_MAX_NAME_LEN];            /**< Channel name */
    mesh_channel_id_t id;                    /**< Channel ID */

    /* World state (CRDT-based) */
    collective_workspace_t* world_state;

    /* Knowledge graph */
    kg_module_wiring_t** module_wirings;     /**< Array of module wirings */
    size_t wiring_count;                     /**< Number of wirings */
    size_t wiring_capacity;                  /**< Array capacity */

    /* Private data collections */
    private_data_collection_t* collections;  /**< Private collections */
    size_t collection_count;                 /**< Number of collections */

    /* Participants */
    mesh_participant_id_t* participants;     /**< Authorized participants */
    size_t participant_count;                /**< Participant count */
    size_t participant_capacity;             /**< Array capacity */

    /* Beliefs for gossip */
    mesh_belief_t* beliefs;                  /**< Belief array */
    size_t belief_count;                     /**< Belief count */
    size_t belief_capacity;                  /**< Array capacity */

    /* Configuration */
    mesh_channel_config_t config;            /**< Channel configuration */

    /* Convergence state */
    float current_free_energy;               /**< Current free energy */
    float previous_free_energy;              /**< Previous free energy */
    float delta_free_energy;                 /**< Change in free energy */
    bool has_converged;                      /**< Convergence flag */
    uint32_t convergence_iterations;         /**< Iterations since start */

    /* Statistics */
    mesh_channel_stats_t stats;              /**< Channel statistics */

    /* Registry reference */
    mesh_participant_registry_t* registry;   /**< Participant registry */

    /* Thread safety */
    nimcp_mutex_t* mutex;                    /**< Channel mutex */

    /* Timing */
    uint64_t last_update_ns;                 /**< Last update timestamp */
    uint64_t creation_time_ns;               /**< Creation timestamp */
};

/**
 * @brief Channel manager structure
 */
struct mesh_channel_manager {
    uint32_t magic;                          /**< Magic for validation */
    mesh_channel_t** channels;               /**< Array of channels */
    size_t channel_count;                    /**< Number of channels */
    size_t channel_capacity;                 /**< Array capacity */
    mesh_participant_registry_t* registry;   /**< Participant registry */
    mesh_channel_manager_config_t config;    /**< Configuration */
    nimcp_mutex_t* mutex;                    /**< Manager mutex */
};

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Validate channel handle
 */
static bool validate_channel(const mesh_channel_t* channel) {
    return channel && channel->magic == NIMCP_MESH_MAGIC;
}

/**
 * @brief Validate channel manager handle
 */
static bool validate_manager(const mesh_channel_manager_t* manager) {
    return manager && manager->magic == NIMCP_MESH_MAGIC;
}

/**
 * @brief Find private collection by name
 */
static private_data_collection_t* find_collection(
    mesh_channel_t* channel,
    const char* name
) {
    if (!channel || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_collection: required parameter is NULL (channel, name)");
        return NULL;
    }

    for (size_t i = 0; i < channel->collection_count; i++) {
        if (strcmp(channel->collections[i].name, name) == 0) {
            return &channel->collections[i];
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Check if participant is authorized for collection
 */
static bool is_authorized_for_collection(
    const private_data_collection_t* collection,
    mesh_participant_id_t participant
) {
    if (!collection) {
        return false;
    }

    for (size_t i = 0; i < collection->authorized_count; i++) {
        if (collection->authorized[i] == participant) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Find private data entry by key
 */
static private_data_entry_t* find_entry(
    private_data_collection_t* collection,
    const char* key
) {
    if (!collection || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry: required parameter is NULL (collection, key)");
        return NULL;
    }

    for (size_t i = 0; i < collection->entry_count; i++) {
        if (strcmp(collection->entries[i].key, key) == 0) {
            return &collection->entries[i];
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Compute channel free energy from world state
 */
static float compute_free_energy(mesh_channel_t* channel) {
    if (!channel || !channel->world_state) return 1.0f;

    /* Free energy approximated from world state coherence */
    float coherence = collective_workspace_get_coherence(channel->world_state);

    /* F ≈ 1 - coherence (simplified FEP) */
    /* Higher coherence = lower free energy = more consensus */
    return 1.0f - coherence;
}

/**
 * @brief Destroy private collection
 */
static void destroy_collection(private_data_collection_t* collection) {
    if (!collection) return;

    if (collection->authorized) {
        nimcp_free(collection->authorized);
    }

    if (collection->entries) {
        for (size_t i = 0; i < collection->entry_count; i++) {
            if (collection->entries[i].value) {
                nimcp_free(collection->entries[i].value);
            }
        }
        nimcp_free(collection->entries);
    }
}

/* ============================================================================
 * Channel Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_channel_default_config(mesh_channel_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->channel_name = "unnamed";
    config->channel_id = 0;
    config->world_state_capacity = COLLECTIVE_WORKSPACE_MAX_ITEMS;
    config->broadcast_threshold = COLLECTIVE_WORKSPACE_DEFAULT_BROADCAST_THRESHOLD;
    config->gossip_rounds_per_update = MESH_DEFAULT_GOSSIP_ROUNDS;
    config->belief_decay_rate = 0.1f;
    config->convergence_threshold = MESH_DEFAULT_CONVERGENCE_THRESHOLD;
    config->max_convergence_iterations = 100;
    config->base_interval_ms = 50.0f;
    config->jitter_amplitude_ms = 10.0f;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_channel_t* mesh_channel_create(
    const mesh_channel_config_t* config,
    mesh_participant_registry_t* registry
) {
    mesh_channel_config_t default_config;
    if (!config) {
        mesh_channel_default_config(&default_config);
        config = &default_config;
    }

    mesh_channel_t* channel = nimcp_calloc(1, sizeof(*channel));
    if (!channel) {
        LOG_ERROR("Failed to allocate channel");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_create: channel is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&channel->config, config, sizeof(*config));
    channel->id = config->channel_id;
    if (config->channel_name) {
        strncpy(channel->name, config->channel_name, MESH_MAX_NAME_LEN - 1);
    }

    /* Create world state */
    collective_workspace_config_t ws_config = collective_workspace_default_config(0, 16);
    ws_config.broadcast_threshold = config->broadcast_threshold;
    channel->world_state = collective_workspace_create(&ws_config);
    if (!channel->world_state) {
        LOG_ERROR("Failed to create world state for channel %s", channel->name);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_create: channel->world_state is NULL");
        return NULL;
    }

    /* Allocate participant array */
    channel->participant_capacity = MESH_MAX_PARTICIPANTS_PER_CHANNEL;
    channel->participants = nimcp_calloc(channel->participant_capacity,
                                          sizeof(mesh_participant_id_t));
    if (!channel->participants) {
        LOG_ERROR("Failed to allocate participant array");
        collective_workspace_destroy(channel->world_state);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_create: channel->participants is NULL");
        return NULL;
    }

    /* Allocate beliefs array */
    channel->belief_capacity = MESH_DEFAULT_BELIEF_CAPACITY;
    channel->beliefs = nimcp_calloc(channel->belief_capacity, sizeof(mesh_belief_t));
    if (!channel->beliefs) {
        LOG_ERROR("Failed to allocate beliefs array");
        nimcp_free(channel->participants);
        collective_workspace_destroy(channel->world_state);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_create: channel->beliefs is NULL");
        return NULL;
    }

    /* Allocate collections array */
    channel->collections = nimcp_calloc(MESH_MAX_PRIVATE_COLLECTIONS,
                                         sizeof(private_data_collection_t));
    if (!channel->collections) {
        LOG_ERROR("Failed to allocate collections array");
        nimcp_free(channel->beliefs);
        nimcp_free(channel->participants);
        collective_workspace_destroy(channel->world_state);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_create: channel->collections is NULL");
        return NULL;
    }

    /* Allocate module wirings array */
    channel->wiring_capacity = 64;
    channel->module_wirings = nimcp_calloc(channel->wiring_capacity,
                                            sizeof(kg_module_wiring_t*));
    if (!channel->module_wirings) {
        LOG_ERROR("Failed to allocate module wirings array");
        nimcp_free(channel->collections);
        nimcp_free(channel->beliefs);
        nimcp_free(channel->participants);
        collective_workspace_destroy(channel->world_state);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_channel_create: channel->module_wirings is NULL");
        return NULL;
    }

    /* Create mutex */
    channel->mutex = nimcp_mutex_create(NULL);
    if (!channel->mutex) {
        LOG_ERROR("Failed to create channel mutex");
        nimcp_free(channel->module_wirings);
        nimcp_free(channel->collections);
        nimcp_free(channel->beliefs);
        nimcp_free(channel->participants);
        collective_workspace_destroy(channel->world_state);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_channel_create: channel->mutex is NULL");
        return NULL;
    }

    /* Initialize state */
    channel->magic = NIMCP_MESH_MAGIC;
    channel->registry = registry;
    channel->current_free_energy = 1.0f;
    channel->previous_free_energy = 1.0f;
    channel->delta_free_energy = 0.0f;
    channel->has_converged = false;
    channel->creation_time_ns = nimcp_time_now_ns();
    channel->last_update_ns = channel->creation_time_ns;

    /* Initialize statistics */
    memset(&channel->stats, 0, sizeof(channel->stats));
    channel->stats.channel_id = channel->id;

    LOG_INFO("Created channel '%s' (id=%u)", channel->name, channel->id);

    return channel;
}

void mesh_channel_destroy(mesh_channel_t* channel) {
    if (!channel) return;

    /* Destroy world state */
    if (channel->world_state) {
        collective_workspace_destroy(channel->world_state);
    }

    /* Destroy collections */
    if (channel->collections) {
        for (size_t i = 0; i < channel->collection_count; i++) {
            destroy_collection(&channel->collections[i]);
        }
        nimcp_free(channel->collections);
    }

    /* Free arrays */
    if (channel->participants) {
        nimcp_free(channel->participants);
    }
    if (channel->beliefs) {
        nimcp_free(channel->beliefs);
    }
    if (channel->module_wirings) {
        /* Note: We don't destroy the wirings themselves, just our references */
        nimcp_free(channel->module_wirings);
    }

    /* Destroy mutex */
    if (channel->mutex) {
        nimcp_mutex_destroy(channel->mutex);
    }

    LOG_INFO("Destroyed channel '%s'", channel->name);

    channel->magic = 0;
    nimcp_free(channel);
}

mesh_channel_id_t mesh_channel_get_id(const mesh_channel_t* channel) {
    return channel ? channel->id : 0;
}

const char* mesh_channel_get_name(const mesh_channel_t* channel) {
    return channel ? channel->name : NULL;
}

/* ============================================================================
 * Participant Management
 * ============================================================================ */

nimcp_error_t mesh_channel_add_participant(
    mesh_channel_t* channel,
    mesh_participant_id_t participant_id
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(channel->mutex);

    /* Check if already member */
    for (size_t i = 0; i < channel->participant_count; i++) {
        if (channel->participants[i] == participant_id) {
            nimcp_mutex_unlock(channel->mutex);
            return NIMCP_SUCCESS; /* Already member */
        }
    }

    /* Check capacity */
    if (channel->participant_count >= channel->participant_capacity) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Add participant */
    channel->participants[channel->participant_count++] = participant_id;
    channel->stats.participant_count = channel->participant_count;

    nimcp_mutex_unlock(channel->mutex);

    LOG_DEBUG("Added participant 0x%016lx to channel %s",
              (unsigned long)participant_id, channel->name);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_remove_participant(
    mesh_channel_t* channel,
    mesh_participant_id_t participant_id
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(channel->mutex);

    bool found = false;
    for (size_t i = 0; i < channel->participant_count; i++) {
        if (channel->participants[i] == participant_id) {
            /* Shift remaining */
            for (size_t j = i; j < channel->participant_count - 1; j++) {
                channel->participants[j] = channel->participants[j + 1];
            }
            channel->participant_count--;
            found = true;
            break;
        }
    }

    channel->stats.participant_count = channel->participant_count;

    nimcp_mutex_unlock(channel->mutex);

    return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;
}

bool mesh_channel_has_participant(
    const mesh_channel_t* channel,
    mesh_participant_id_t participant_id
) {
    if (!validate_channel(channel)) {
        return false;
    }

    /* P1-33: Must lock mutex - concurrent add/remove can modify array during iteration */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    for (size_t i = 0; i < channel->participant_count; i++) {
        if (channel->participants[i] == participant_id) {
            nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
            return true;
        }
    }
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return false;
}

nimcp_error_t mesh_channel_get_participants(
    const mesh_channel_t* channel,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!ids_out || !count_out) return NIMCP_ERROR_NULL_POINTER;

    /* P2-152: Mutex protection for participant array access */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);

    size_t count = channel->participant_count < max_ids ?
                   channel->participant_count : max_ids;

    memcpy(ids_out, channel->participants, count * sizeof(mesh_participant_id_t));
    *count_out = count;

    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return NIMCP_SUCCESS;
}

size_t mesh_channel_get_participant_count(const mesh_channel_t* channel) {
    if (!channel) return 0;
    /* P1: Lock mutex to prevent torn read while add/remove modifies count */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    size_t count = channel->participant_count;
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return count;
}

/* ============================================================================
 * World State
 * ============================================================================ */

collective_workspace_t* mesh_channel_get_world_state(mesh_channel_t* channel) {
    return channel ? channel->world_state : NULL;
}

nimcp_error_t mesh_channel_add_world_state_item(
    mesh_channel_t* channel,
    mesh_participant_id_t contributor,
    uint32_t item_type,
    const float* content,
    size_t content_dim,
    float salience
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!channel->world_state) return NIMCP_ERROR_INVALID_STATE;
    if (!content || content_dim == 0) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(channel->mutex);

    collective_workspace_item_t item;
    memset(&item, 0, sizeof(item));

    /* Generate item ID from contributor */
    item.item_id = (uint32_t)((contributor >> 32) ^ (contributor & 0xFFFFFFFF));
    item.salience = salience;
    item.type = (workspace_item_type_t)item_type;
    item.source_drone = (uint16_t)(contributor & 0xFFFF);
    item.timestamp_ms = nimcp_time_now_ns() / 1000000;

    /* Copy content */
    size_t copy_dim = content_dim < COLLECTIVE_WORKSPACE_CONTENT_DIM ?
                      content_dim : COLLECTIVE_WORKSPACE_CONTENT_DIM;
    memcpy(item.content, content, copy_dim * sizeof(float));

    bool success = collective_workspace_add_item(channel->world_state, &item);

    if (success) {
        channel->stats.world_state_items = collective_workspace_get_item_count(
            channel->world_state);
    }

    nimcp_mutex_unlock(channel->mutex);

    return success ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_error_t mesh_channel_get_top_world_state_items(
    const mesh_channel_t* channel,
    void* items_out,
    size_t max_items,
    size_t* count_out
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!items_out || !count_out) return NIMCP_ERROR_NULL_POINTER;
    if (!channel->world_state) return NIMCP_ERROR_INVALID_STATE;

    uint32_t actual_count = 0;
    bool success = collective_workspace_get_top_items(
        channel->world_state,
        (collective_workspace_item_t*)items_out,
        (uint32_t)max_items,
        &actual_count
    );

    *count_out = actual_count;

    return success ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/**
 * @brief Internal unlocked version of get_world_state_coherence
 *
 * WHAT: Get coherence without acquiring the mutex
 * WHY:  Called from mesh_channel_update() which already holds channel->mutex,
 *       preventing self-deadlock on non-recursive mutex
 * PRECONDITION: Caller must hold channel->mutex
 */
static float get_world_state_coherence_unlocked(const mesh_channel_t* channel) {
    if (!channel->world_state) return 0.0f;
    return collective_workspace_get_coherence(channel->world_state);
}

float mesh_channel_get_world_state_coherence(const mesh_channel_t* channel) {
    if (!validate_channel(channel) || !channel->world_state) return 0.0f;
    /* P1: Lock mutex to prevent torn read while update modifies world_state */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    float coherence = get_world_state_coherence_unlocked(channel);
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return coherence;
}

size_t mesh_channel_prune_world_state(
    mesh_channel_t* channel,
    uint64_t current_time_ms
) {
    if (!validate_channel(channel) || !channel->world_state) return 0;

    nimcp_mutex_lock(channel->mutex);
    uint32_t pruned = collective_workspace_prune(channel->world_state, current_time_ms);
    channel->stats.world_state_items = collective_workspace_get_item_count(
        channel->world_state);
    nimcp_mutex_unlock(channel->mutex);

    return pruned;
}

/* ============================================================================
 * Knowledge Graph
 * ============================================================================ */

kg_module_wiring_t* mesh_channel_get_knowledge_graph(mesh_channel_t* channel) {
    if (!channel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_channel_get_knowledge_graph: channel is NULL");
        return NULL;
    }
    if (channel->wiring_count == 0) {
        /* Empty wiring is valid state, not an error (P2: false positive removal) */
        return NULL;
    }
    /* Return first wiring as representative */
    return channel->module_wirings[0];
}

nimcp_error_t mesh_channel_register_module_wiring(
    mesh_channel_t* channel,
    const kg_module_wiring_t* wiring
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!wiring) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(channel->mutex);

    if (channel->wiring_count >= channel->wiring_capacity) {
        /* Expand array */
        size_t new_capacity = channel->wiring_capacity * 2;
        kg_module_wiring_t** new_arr = nimcp_realloc(
            channel->module_wirings,
            new_capacity * sizeof(kg_module_wiring_t*)
        );
        if (!new_arr) {
            nimcp_mutex_unlock(channel->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }
        channel->module_wirings = new_arr;
        channel->wiring_capacity = new_capacity;
    }

    /* Store reference (not copy) */
    channel->module_wirings[channel->wiring_count++] = (kg_module_wiring_t*)wiring;

    nimcp_mutex_unlock(channel->mutex);

    LOG_DEBUG("Registered module wiring in channel %s", channel->name);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Private Data Collections
 * ============================================================================ */

nimcp_error_t mesh_channel_create_private_collection(
    mesh_channel_t* channel,
    const char* name,
    mesh_participant_id_t creator
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!name) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(channel->mutex);

    /* Check if exists */
    if (find_collection(channel, name)) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Check capacity */
    if (channel->collection_count >= MESH_MAX_PRIVATE_COLLECTIONS) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Initialize collection */
    private_data_collection_t* collection = &channel->collections[channel->collection_count];
    memset(collection, 0, sizeof(*collection));
    strncpy(collection->name, name, MESH_MAX_COLLECTION_NAME_LEN - 1);

    /* Allocate authorized array */
    collection->authorized_capacity = 16;
    collection->authorized = nimcp_calloc(collection->authorized_capacity,
                                           sizeof(mesh_participant_id_t));
    if (!collection->authorized) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Add creator as authorized */
    collection->authorized[0] = creator;
    collection->authorized_count = 1;

    /* Allocate entries array */
    collection->entry_capacity = 32;
    collection->entries = nimcp_calloc(collection->entry_capacity,
                                        sizeof(private_data_entry_t));
    if (!collection->entries) {
        nimcp_free(collection->authorized);
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    channel->collection_count++;
    channel->stats.private_collections = channel->collection_count;

    nimcp_mutex_unlock(channel->mutex);

    LOG_DEBUG("Created private collection '%s' in channel %s", name, channel->name);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_authorize_for_collection(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t authorizer,
    mesh_participant_id_t participant
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!collection_name) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(channel->mutex);

    private_data_collection_t* collection = find_collection(channel, collection_name);
    if (!collection) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Check authorizer is authorized */
    if (!is_authorized_for_collection(collection, authorizer)) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Check if already authorized */
    if (is_authorized_for_collection(collection, participant)) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_SUCCESS;
    }

    /* Check capacity */
    if (collection->authorized_count >= collection->authorized_capacity) {
        size_t new_capacity = collection->authorized_capacity * 2;
        mesh_participant_id_t* new_arr = nimcp_realloc(
            collection->authorized,
            new_capacity * sizeof(mesh_participant_id_t)
        );
        if (!new_arr) {
            nimcp_mutex_unlock(channel->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }
        collection->authorized = new_arr;
        collection->authorized_capacity = new_capacity;
    }

    collection->authorized[collection->authorized_count++] = participant;

    nimcp_mutex_unlock(channel->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_put_private_data(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key,
    const void* value,
    size_t value_len
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!collection_name || !key || !value) return NIMCP_ERROR_NULL_POINTER;
    if (value_len > MESH_MAX_PRIVATE_VALUE_LEN) return NIMCP_ERROR_OUT_OF_RANGE;

    nimcp_mutex_lock(channel->mutex);

    private_data_collection_t* collection = find_collection(channel, collection_name);
    if (!collection) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!is_authorized_for_collection(collection, participant)) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Find or create entry */
    private_data_entry_t* entry = find_entry(collection, key);
    if (!entry) {
        if (collection->entry_count >= collection->entry_capacity) {
            size_t new_capacity = collection->entry_capacity * 2;
            private_data_entry_t* new_arr = nimcp_realloc(
                collection->entries,
                new_capacity * sizeof(private_data_entry_t)
            );
            if (!new_arr) {
                nimcp_mutex_unlock(channel->mutex);
                return NIMCP_ERROR_NO_MEMORY;
            }
            collection->entries = new_arr;
            collection->entry_capacity = new_capacity;
        }
        entry = &collection->entries[collection->entry_count++];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->key, key, MESH_MAX_PRIVATE_KEY_LEN - 1);
        entry->created_at_ns = nimcp_time_now_ns();
        entry->owner = participant;
    }

    /* Update value */
    if (entry->value) {
        nimcp_free(entry->value);
    }
    entry->value = nimcp_malloc(value_len);
    if (!entry->value) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(entry->value, value, value_len);
    entry->value_len = value_len;
    entry->version++;
    entry->modified_at_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(channel->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_get_private_data(
    const mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key,
    void* value_out,
    size_t* value_len_out,
    size_t max_len
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!collection_name || !key || !value_out || !value_len_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Cast away const for mutex (safe, we're only reading) */
    mesh_channel_t* ch = (mesh_channel_t*)channel;
    nimcp_mutex_lock(ch->mutex);

    private_data_collection_t* collection = find_collection(ch, collection_name);
    if (!collection) {
        nimcp_mutex_unlock(ch->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!is_authorized_for_collection(collection, participant)) {
        nimcp_mutex_unlock(ch->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    private_data_entry_t* entry = find_entry(collection, key);
    if (!entry || !entry->value) {
        nimcp_mutex_unlock(ch->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    size_t copy_len = entry->value_len < max_len ? entry->value_len : max_len;
    memcpy(value_out, entry->value, copy_len);
    *value_len_out = entry->value_len;

    nimcp_mutex_unlock(ch->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_delete_private_data(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!collection_name || !key) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(channel->mutex);

    private_data_collection_t* collection = find_collection(channel, collection_name);
    if (!collection) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!is_authorized_for_collection(collection, participant)) {
        nimcp_mutex_unlock(channel->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Find and remove entry */
    for (size_t i = 0; i < collection->entry_count; i++) {
        if (strcmp(collection->entries[i].key, key) == 0) {
            if (collection->entries[i].value) {
                nimcp_free(collection->entries[i].value);
            }
            /* Shift remaining */
            for (size_t j = i; j < collection->entry_count - 1; j++) {
                collection->entries[j] = collection->entries[j + 1];
            }
            collection->entry_count--;
            nimcp_mutex_unlock(channel->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(channel->mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Gossip and Belief Propagation
 * ============================================================================ */

nimcp_error_t mesh_channel_introduce_belief(
    mesh_channel_t* channel,
    const mesh_belief_t* belief
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!belief) return NIMCP_ERROR_NULL_POINTER;

    /* BBB validation of incoming belief data */
    if (!validate_belief_bbb(belief)) {
        LOG_ERROR("Rejecting belief %u from source 0x%lx - BBB validation failed",
                  belief->belief_id, (unsigned long)belief->source);
        return NIMCP_ERROR_BBB_VALIDATION;
    }

    nimcp_mutex_lock(channel->mutex);

    /* Check capacity */
    if (channel->belief_count >= channel->belief_capacity) {
        size_t new_capacity = channel->belief_capacity * 2;
        mesh_belief_t* new_arr = nimcp_realloc(
            channel->beliefs,
            new_capacity * sizeof(mesh_belief_t)
        );
        if (!new_arr) {
            nimcp_mutex_unlock(channel->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }
        channel->beliefs = new_arr;
        channel->belief_capacity = new_capacity;
    }

    /* Copy belief */
    memcpy(&channel->beliefs[channel->belief_count], belief, sizeof(mesh_belief_t));
    channel->belief_count++;

    nimcp_mutex_unlock(channel->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_gossip_round(mesh_channel_t* channel) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(channel->mutex);

    /* Send heartbeat at start of gossip round */
    mesh_channel_heartbeat("gossip_round", 0.0f);

    /* Simplified gossip: propagate beliefs to world state */
    for (size_t i = 0; i < channel->belief_count; i++) {
        mesh_belief_t* belief = &channel->beliefs[i];

        /* Periodic heartbeat during long gossip iterations */
        if (i > 0 && (i % 100) == 0) {
            float progress = (float)i / (float)channel->belief_count;
            mesh_channel_heartbeat("gossip_propagate", progress);
        }

        /* Add belief to world state as item */
        collective_workspace_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = belief->belief_id;
        item.salience = belief->certainty;
        item.type = WORKSPACE_ITEM_STATE;
        item.source_drone = (uint16_t)(belief->source & 0xFFFF);
        item.timestamp_ms = belief->timestamp_ns / 1000000;

        /* Copy belief vector to content */
        size_t copy_dim = belief->vector_dim < COLLECTIVE_WORKSPACE_CONTENT_DIM ?
                          belief->vector_dim : COLLECTIVE_WORKSPACE_CONTENT_DIM;
        memcpy(item.content, belief->belief_vector, copy_dim * sizeof(float));

        collective_workspace_merge_item(channel->world_state, &item);

        /* Decay belief certainty */
        belief->certainty *= (1.0f - channel->config.belief_decay_rate);
        belief->propagation_count++;
    }

    /* Remove low-certainty beliefs */
    size_t write_idx = 0;
    for (size_t i = 0; i < channel->belief_count; i++) {
        if (channel->beliefs[i].certainty > 0.1f) {
            if (write_idx != i) {
                channel->beliefs[write_idx] = channel->beliefs[i];
            }
            write_idx++;
        }
    }
    channel->belief_count = write_idx;

    channel->stats.gossip_rounds++;
    channel->stats.beliefs_propagated += channel->belief_count;

    /* Update world state item count after gossip merge */
    channel->stats.world_state_items = collective_workspace_get_item_count(
        channel->world_state);

    nimcp_mutex_unlock(channel->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_get_consensus_beliefs(
    const mesh_channel_t* channel,
    mesh_belief_t* beliefs_out,
    size_t max_beliefs,
    size_t* count_out
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!beliefs_out || !count_out) return NIMCP_ERROR_NULL_POINTER;

    /* P1: Lock mutex to prevent torn reads of beliefs array while update modifies it */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);

    size_t count = channel->belief_count < max_beliefs ?
                   channel->belief_count : max_beliefs;

    /* Return beliefs sorted by certainty (already high certainty = consensus) */
    memcpy(beliefs_out, channel->beliefs, count * sizeof(mesh_belief_t));
    *count_out = count;

    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update and Convergence
 * ============================================================================ */

nimcp_error_t mesh_channel_update(
    mesh_channel_t* channel,
    uint64_t delta_ms
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(channel->mutex);

    /* Run gossip rounds - P2-153: Intentional unlock-relock pattern.
     * Gossip rounds involve network I/O and must not hold the channel lock.
     * State is re-validated after each round when lock is reacquired. */
    for (uint32_t i = 0; i < channel->config.gossip_rounds_per_update; i++) {
        nimcp_mutex_unlock(channel->mutex);
        mesh_channel_gossip_round(channel);
        nimcp_mutex_lock(channel->mutex);
    }

    /* Update free energy */
    channel->previous_free_energy = channel->current_free_energy;
    channel->current_free_energy = compute_free_energy(channel);
    channel->delta_free_energy = fabsf(channel->current_free_energy -
                                        channel->previous_free_energy);

    /* Check convergence */
    channel->convergence_iterations++;
    if (channel->delta_free_energy < channel->config.convergence_threshold) {
        channel->has_converged = true;
    } else {
        channel->has_converged = false;
    }

    /* Update statistics */
    channel->stats.current_coherence = get_world_state_coherence_unlocked(channel);
    channel->stats.current_free_energy = channel->current_free_energy;
    channel->stats.transactions_processed++;

    /* Prune world state periodically */
    uint64_t now_ms = nimcp_time_now_ns() / 1000000;
    if (now_ms % 1000 < delta_ms) {
        nimcp_mutex_unlock(channel->mutex);
        mesh_channel_prune_world_state(channel, now_ms);
        nimcp_mutex_lock(channel->mutex);
    }

    channel->last_update_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(channel->mutex);

    return NIMCP_SUCCESS;
}

bool mesh_channel_has_converged(const mesh_channel_t* channel) {
    if (!channel) return false;
    /* P1: Lock mutex to prevent torn read while update modifies convergence state */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    bool val = channel->has_converged;
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return val;
}

float mesh_channel_get_free_energy(const mesh_channel_t* channel) {
    if (!channel) return 1.0f;
    /* P1: Lock mutex to prevent torn read while update modifies free energy */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    float val = channel->current_free_energy;
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return val;
}

float mesh_channel_get_convergence_progress(const mesh_channel_t* channel) {
    if (!channel) return 0.0f;

    /* P1: Lock mutex to prevent torn read while update modifies free energy */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    float fe = channel->current_free_energy;
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);

    /* Progress = 1 - (current_fe / initial_fe) clamped to [0,1] */
    float progress = 1.0f - fe;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_channel_get_stats(
    const mesh_channel_t* channel,
    mesh_channel_stats_t* stats
) {
    if (!validate_channel(channel)) return NIMCP_ERROR_INVALID_PARAM;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    /* P1: Lock mutex to get consistent snapshot of stats */
    nimcp_mutex_lock(((mesh_channel_t*)channel)->mutex);
    memcpy(stats, &channel->stats, sizeof(mesh_channel_stats_t));
    nimcp_mutex_unlock(((mesh_channel_t*)channel)->mutex);
    return NIMCP_SUCCESS;
}

void mesh_channel_reset_stats(mesh_channel_t* channel) {
    if (!validate_channel(channel)) return;

    nimcp_mutex_lock(channel->mutex);
    memset(&channel->stats, 0, sizeof(channel->stats));
    channel->stats.channel_id = channel->id;
    channel->stats.participant_count = channel->participant_count;
    channel->stats.private_collections = channel->collection_count;
    nimcp_mutex_unlock(channel->mutex);
}

/* ============================================================================
 * Channel Manager
 * ============================================================================ */

mesh_channel_manager_t* mesh_channel_manager_create(
    const mesh_channel_manager_config_t* config,
    mesh_participant_registry_t* registry
) {
    mesh_channel_manager_config_t default_config = {
        .max_channels = MESH_MAX_CHANNELS,
        .enable_logging = true
    };
    if (!config) {
        config = &default_config;
    }

    mesh_channel_manager_t* manager = nimcp_calloc(1, sizeof(*manager));
    if (!manager) {
        LOG_ERROR("Failed to allocate channel manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_manager_create: manager is NULL");
        return NULL;
    }

    manager->channel_capacity = config->max_channels;
    manager->channels = nimcp_calloc(manager->channel_capacity, sizeof(mesh_channel_t*));
    if (!manager->channels) {
        LOG_ERROR("Failed to allocate channels array");
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_manager_create: manager->channels is NULL");
        return NULL;
    }

    manager->mutex = nimcp_mutex_create(NULL);
    if (!manager->mutex) {
        LOG_ERROR("Failed to create manager mutex");
        nimcp_free(manager->channels);
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_manager_create: manager->mutex is NULL");
        return NULL;
    }

    manager->magic = NIMCP_MESH_MAGIC;
    memcpy(&manager->config, config, sizeof(*config));
    manager->registry = registry;

    LOG_INFO("Created channel manager with capacity %zu", manager->channel_capacity);

    return manager;
}

void mesh_channel_manager_destroy(mesh_channel_manager_t* manager) {
    if (!manager) return;

    if (manager->channels) {
        for (size_t i = 0; i < manager->channel_count; i++) {
            if (manager->channels[i]) {
                mesh_channel_destroy(manager->channels[i]);
            }
        }
        nimcp_free(manager->channels);
    }

    if (manager->mutex) {
        nimcp_mutex_destroy(manager->mutex);
    }

    manager->magic = 0;
    nimcp_free(manager);
    LOG_INFO("Destroyed channel manager");
}

mesh_channel_t* mesh_channel_manager_create_channel(
    mesh_channel_manager_t* manager,
    const mesh_channel_config_t* config
) {
    if (!validate_manager(manager) || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_manager_create_channel: required parameter is NULL (validate_manager, config)");
        return NULL;
    }

    nimcp_mutex_lock(manager->mutex);

    if (manager->channel_count >= manager->channel_capacity) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR("Channel manager full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mesh_channel_manager_create_channel: capacity exceeded");
        return NULL;
    }

    mesh_channel_t* channel = mesh_channel_create(config, manager->registry);
    if (!channel) {
        nimcp_mutex_unlock(manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_channel_manager_create_channel: channel is NULL");
        return NULL;
    }

    manager->channels[manager->channel_count++] = channel;

    nimcp_mutex_unlock(manager->mutex);

    return channel;
}

mesh_channel_t* mesh_channel_manager_get_channel(
    mesh_channel_manager_t* manager,
    mesh_channel_id_t channel_id
) {
    if (!validate_manager(manager)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_channel_manager_get_channel: validate_manager is NULL");
        return NULL;
    }

    /* P1: Lock mutex to prevent torn read while create/destroy modifies channel array */
    nimcp_mutex_lock(manager->mutex);
    mesh_channel_t* result = NULL;
    for (size_t i = 0; i < manager->channel_count; i++) {
        if (manager->channels[i] &&
            mesh_channel_get_id(manager->channels[i]) == channel_id) {
            result = manager->channels[i];
            break;
        }
    }
    nimcp_mutex_unlock(manager->mutex);
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return result;
}

mesh_channel_t* mesh_channel_manager_get_channel_by_name(
    mesh_channel_manager_t* manager,
    const char* name
) {
    if (!validate_manager(manager) || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_channel_manager_get_channel_by_name: required parameter is NULL (validate_manager, name)");
        return NULL;
    }

    /* P1: Lock mutex to prevent torn read while create/destroy modifies channel array */
    nimcp_mutex_lock(manager->mutex);
    mesh_channel_t* result = NULL;
    for (size_t i = 0; i < manager->channel_count; i++) {
        if (manager->channels[i]) {
            const char* ch_name = mesh_channel_get_name(manager->channels[i]);
            if (ch_name && strcmp(ch_name, name) == 0) {
                result = manager->channels[i];
                break;
            }
        }
    }
    nimcp_mutex_unlock(manager->mutex);
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return result;
}

nimcp_error_t mesh_channel_manager_update(
    mesh_channel_manager_t* manager,
    uint64_t delta_ms
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;

    for (size_t i = 0; i < manager->channel_count; i++) {
        if (manager->channels[i]) {
            mesh_channel_update(manager->channels[i], delta_ms);
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_channel_manager_create_standard_channels(
    mesh_channel_manager_t* manager
) {
    if (!validate_manager(manager)) return NIMCP_ERROR_INVALID_PARAM;

    /* Create System channel */
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_name = "SYSTEM";
    config.channel_id = MESH_CHANNEL_SYSTEM;
    if (!mesh_channel_manager_create_channel(manager, &config)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Create Left Hemisphere channel */
    mesh_channel_default_config(&config);
    config.channel_name = "LEFT_HEMISPHERE";
    config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
    if (!mesh_channel_manager_create_channel(manager, &config)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Create Right Hemisphere channel */
    mesh_channel_default_config(&config);
    config.channel_name = "RIGHT_HEMISPHERE";
    config.channel_id = MESH_CHANNEL_RIGHT_HEMISPHERE;
    if (!mesh_channel_manager_create_channel(manager, &config)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Create Subcortical channel */
    mesh_channel_default_config(&config);
    config.channel_name = "SUBCORTICAL";
    config.channel_id = MESH_CHANNEL_SUBCORTICAL;
    if (!mesh_channel_manager_create_channel(manager, &config)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Create GPU Compute channel */
    mesh_channel_default_config(&config);
    config.channel_name = "GPU_COMPUTE";
    config.channel_id = MESH_CHANNEL_GPU_COMPUTE;
    config.base_interval_ms = 5.0f;  /* Faster for GPU */
    config.jitter_amplitude_ms = 1.0f;
    if (!mesh_channel_manager_create_channel(manager, &config)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    LOG_INFO("Created 5 standard channels");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void mesh_channel_print_info(const mesh_channel_t* channel) {
    if (!channel) {
        printf("Channel: NULL\n");
        return;
    }

    printf("Channel: %s (ID=%u)\n", channel->name, channel->id);
    printf("  Participants: %zu\n", channel->participant_count);
    printf("  Beliefs:      %zu\n", channel->belief_count);
    printf("  Collections:  %zu\n", channel->collection_count);
    printf("  Wirings:      %zu\n", channel->wiring_count);
    printf("  Free Energy:  %.4f\n", channel->current_free_energy);
    printf("  Converged:    %s\n", channel->has_converged ? "yes" : "no");
    printf("  Coherence:    %.4f\n", mesh_channel_get_world_state_coherence(channel));
}

void mesh_channel_manager_print_status(const mesh_channel_manager_t* manager) {
    if (!manager) {
        printf("Channel Manager: NULL\n");
        return;
    }

    printf("Channel Manager:\n");
    printf("  Channels: %zu / %zu\n", manager->channel_count, manager->channel_capacity);

    for (size_t i = 0; i < manager->channel_count; i++) {
        if (manager->channels[i]) {
            printf("  [%zu] ", i);
            mesh_channel_print_info(manager->channels[i]);
        }
    }
}
