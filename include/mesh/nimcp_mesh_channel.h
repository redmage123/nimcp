/**
 * @file nimcp_mesh_channel.h
 * @brief Mesh Network Channel - Isolated Ledger Domain
 *
 * WHAT: Channel structure with independent world state, coordinator pool, and gossip
 * WHY:  Enable Hyperledger-inspired channel isolation for different brain regions
 * HOW:  Each channel has its own CRDT-based world state and knowledge graph
 *
 * BIOLOGICAL INSPIRATION:
 * Channels map to major brain functional divisions:
 * - Left Hemisphere: Analytical, sequential, language processing
 * - Right Hemisphere: Holistic, spatial, creative processing
 * - Subcortical: Emotion, reward, motor control
 * - GPU Compute: Accelerated batch processing (analogous to cerebellum)
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                           MESH CHANNEL                                   │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Channel: LEFT_HEMISPHERE                                               │
 * │  ├── World State (collective_workspace_t) ──── CRDT-based              │
 * │  ├── Knowledge Graph (kg_module_wiring_t) ──── Module topology         │
 * │  ├── Private Data Collections ──────────────── Sensitive data          │
 * │  ├── Gossip Context ────────────────────────── Belief propagation      │
 * │  ├── Coordinator Pool ──────────────────────── Leader/Worker/Standby   │
 * │  └── Authorized Participants ───────────────── Channel membership      │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_CHANNEL_H
#define NIMCP_MESH_CHANNEL_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_channel mesh_channel_t;
typedef struct mesh_channel_manager mesh_channel_manager_t;
typedef struct collective_workspace collective_workspace_t;
typedef struct kg_module_wiring kg_module_wiring_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum private data collections per channel */
#define MESH_MAX_PRIVATE_COLLECTIONS    16

/** @brief Maximum private collection name length */
#define MESH_MAX_COLLECTION_NAME_LEN    64

/** @brief Maximum private data key length */
#define MESH_MAX_PRIVATE_KEY_LEN        128

/** @brief Maximum private data value length */
#define MESH_MAX_PRIVATE_VALUE_LEN      4096

/** @brief Default gossip rounds per update */
#define MESH_DEFAULT_GOSSIP_ROUNDS      3

/** @brief Default convergence threshold for FEP */
#define MESH_DEFAULT_CONVERGENCE_THRESHOLD  0.01f

/* ============================================================================
 * Private Data Collection
 * ============================================================================ */

/**
 * @brief Private data entry in a collection
 */
typedef struct private_data_entry {
    char key[MESH_MAX_PRIVATE_KEY_LEN];     /**< Entry key */
    uint8_t* value;                          /**< Entry value */
    size_t value_len;                        /**< Value length */
    uint64_t version;                        /**< Version for MVCC */
    mesh_participant_id_t owner;             /**< Data owner */
    uint64_t created_at_ns;                  /**< Creation timestamp */
    uint64_t modified_at_ns;                 /**< Last modification */
} private_data_entry_t;

/**
 * @brief Private data collection
 *
 * WHAT: Isolated data storage within a channel
 * WHY:  Some data should not be visible to all channel members
 * HOW:  Access controlled by authorized participant list
 */
typedef struct private_data_collection {
    char name[MESH_MAX_COLLECTION_NAME_LEN]; /**< Collection name */
    mesh_participant_id_t* authorized;       /**< Authorized participants */
    size_t authorized_count;                 /**< Number of authorized */
    size_t authorized_capacity;              /**< Array capacity */
    private_data_entry_t* entries;           /**< Data entries */
    size_t entry_count;                      /**< Number of entries */
    size_t entry_capacity;                   /**< Entry array capacity */
} private_data_collection_t;

/* ============================================================================
 * Channel Configuration
 * ============================================================================ */

/**
 * @brief Channel configuration
 */
typedef struct mesh_channel_config {
    const char* channel_name;                /**< Human-readable name */
    mesh_channel_id_t channel_id;            /**< Channel ID */

    /* World state configuration */
    size_t world_state_capacity;             /**< Max items in world state */
    float broadcast_threshold;               /**< Gossip broadcast threshold */

    /* Gossip configuration */
    uint32_t gossip_rounds_per_update;       /**< Gossip rounds per update */
    float belief_decay_rate;                 /**< Belief decay rate */

    /* Convergence configuration */
    float convergence_threshold;             /**< FEP convergence threshold */
    uint32_t max_convergence_iterations;     /**< Max iterations for convergence */

    /* Timing */
    float base_interval_ms;                  /**< Base timing interval */
    float jitter_amplitude_ms;               /**< Pink noise jitter */

    /* Logging */
    bool enable_logging;                     /**< Enable channel logging */
} mesh_channel_config_t;

/**
 * @brief Channel statistics
 */
typedef struct mesh_channel_stats {
    mesh_channel_id_t channel_id;            /**< Channel ID */
    size_t participant_count;                /**< Current participants */
    size_t coordinator_count;                /**< Coordinators in channel */
    size_t world_state_items;                /**< Items in world state */
    size_t private_collections;              /**< Number of private collections */
    uint64_t transactions_processed;         /**< Total transactions */
    uint64_t gossip_rounds;                  /**< Total gossip rounds */
    uint64_t beliefs_propagated;             /**< Total beliefs propagated */
    float avg_convergence_time_ms;           /**< Average convergence time */
    float current_coherence;                 /**< Current coherence [0,1] */
    float current_free_energy;               /**< Current free energy */
} mesh_channel_stats_t;

/* ============================================================================
 * Channel Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default channel configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_default_config(mesh_channel_config_t* config);

/**
 * @brief Create a mesh channel
 *
 * WHAT: Allocate and initialize channel with world state and gossip
 * WHY:  Channels are isolated ledger domains for brain regions
 *
 * @param config Channel configuration
 * @param registry Participant registry
 * @return Channel handle or NULL on failure
 */
mesh_channel_t* mesh_channel_create(
    const mesh_channel_config_t* config,
    mesh_participant_registry_t* registry
);

/**
 * @brief Destroy a mesh channel
 *
 * @param channel Channel to destroy (NULL-safe)
 */
void mesh_channel_destroy(mesh_channel_t* channel);

/**
 * @brief Get channel ID
 *
 * @param channel Channel handle
 * @return Channel ID
 */
mesh_channel_id_t mesh_channel_get_id(const mesh_channel_t* channel);

/**
 * @brief Get channel name
 *
 * @param channel Channel handle
 * @return Channel name or NULL
 */
const char* mesh_channel_get_name(const mesh_channel_t* channel);

/* ============================================================================
 * Participant Management API
 * ============================================================================ */

/**
 * @brief Add participant to channel
 *
 * WHAT: Authorize participant for channel membership
 * WHY:  Control who can access channel resources
 *
 * @param channel Channel handle
 * @param participant_id Participant to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_add_participant(
    mesh_channel_t* channel,
    mesh_participant_id_t participant_id
);

/**
 * @brief Remove participant from channel
 *
 * @param channel Channel handle
 * @param participant_id Participant to remove
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_remove_participant(
    mesh_channel_t* channel,
    mesh_participant_id_t participant_id
);

/**
 * @brief Check if participant is in channel
 *
 * @param channel Channel handle
 * @param participant_id Participant to check
 * @return true if participant is channel member
 */
bool mesh_channel_has_participant(
    const mesh_channel_t* channel,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get all participants in channel
 *
 * @param channel Channel handle
 * @param ids_out Output array (caller allocates)
 * @param max_ids Maximum IDs to return
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_get_participants(
    const mesh_channel_t* channel,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
);

/**
 * @brief Get participant count in channel
 *
 * @param channel Channel handle
 * @return Participant count
 */
size_t mesh_channel_get_participant_count(const mesh_channel_t* channel);

/* ============================================================================
 * World State API
 * ============================================================================ */

/**
 * @brief Get channel world state
 *
 * WHAT: Access channel's CRDT-based world state
 * WHY:  World state stores channel-wide consensus data
 *
 * @param channel Channel handle
 * @return World state handle or NULL
 */
collective_workspace_t* mesh_channel_get_world_state(mesh_channel_t* channel);

/**
 * @brief Add item to world state
 *
 * @param channel Channel handle
 * @param contributor Contributing participant
 * @param item_type Item type
 * @param content Content vector
 * @param content_dim Content dimension
 * @param salience Item salience [0,1]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_add_world_state_item(
    mesh_channel_t* channel,
    mesh_participant_id_t contributor,
    uint32_t item_type,
    const float* content,
    size_t content_dim,
    float salience
);

/**
 * @brief Get top items from world state
 *
 * @param channel Channel handle
 * @param items_out Output array (caller allocates)
 * @param max_items Maximum items
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_get_top_world_state_items(
    const mesh_channel_t* channel,
    void* items_out,  /* collective_workspace_item_t* */
    size_t max_items,
    size_t* count_out
);

/**
 * @brief Get world state coherence
 *
 * @param channel Channel handle
 * @return Coherence value [0,1]
 */
float mesh_channel_get_world_state_coherence(const mesh_channel_t* channel);

/**
 * @brief Prune old/low-salience world state items
 *
 * @param channel Channel handle
 * @param current_time_ms Current time
 * @return Number of items pruned
 */
size_t mesh_channel_prune_world_state(
    mesh_channel_t* channel,
    uint64_t current_time_ms
);

/* ============================================================================
 * Knowledge Graph API
 * ============================================================================ */

/**
 * @brief Get channel knowledge graph
 *
 * WHAT: Access channel's module wiring knowledge graph
 * WHY:  KG tracks module topology and connections
 *
 * @param channel Channel handle
 * @return Knowledge graph handle or NULL
 */
kg_module_wiring_t* mesh_channel_get_knowledge_graph(mesh_channel_t* channel);

/**
 * @brief Register module wiring in channel KG
 *
 * @param channel Channel handle
 * @param wiring Module wiring to register
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_register_module_wiring(
    mesh_channel_t* channel,
    const kg_module_wiring_t* wiring
);

/* ============================================================================
 * Private Data Collection API
 * ============================================================================ */

/**
 * @brief Create private data collection
 *
 * WHAT: Create isolated data storage for sensitive information
 * WHY:  Some data should only be visible to specific participants
 *
 * @param channel Channel handle
 * @param name Collection name
 * @param creator Collection creator (automatically authorized)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_create_private_collection(
    mesh_channel_t* channel,
    const char* name,
    mesh_participant_id_t creator
);

/**
 * @brief Authorize participant for private collection
 *
 * @param channel Channel handle
 * @param collection_name Collection name
 * @param authorizer Must be existing authorized member
 * @param participant Participant to authorize
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_authorize_for_collection(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t authorizer,
    mesh_participant_id_t participant
);

/**
 * @brief Put data in private collection
 *
 * @param channel Channel handle
 * @param collection_name Collection name
 * @param participant Must be authorized
 * @param key Data key
 * @param value Data value
 * @param value_len Value length
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_put_private_data(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key,
    const void* value,
    size_t value_len
);

/**
 * @brief Get data from private collection
 *
 * @param channel Channel handle
 * @param collection_name Collection name
 * @param participant Must be authorized
 * @param key Data key
 * @param value_out Output buffer
 * @param value_len_out Output: actual length
 * @param max_len Maximum buffer size
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_get_private_data(
    const mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key,
    void* value_out,
    size_t* value_len_out,
    size_t max_len
);

/**
 * @brief Delete data from private collection
 *
 * @param channel Channel handle
 * @param collection_name Collection name
 * @param participant Must be authorized
 * @param key Data key
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_delete_private_data(
    mesh_channel_t* channel,
    const char* collection_name,
    mesh_participant_id_t participant,
    const char* key
);

/* ============================================================================
 * Gossip and Belief Propagation API
 * ============================================================================ */

/**
 * @brief Introduce belief into channel
 *
 * WHAT: Add belief for gossip propagation
 * WHY:  Beliefs spread through channel via gossip protocol
 *
 * @param channel Channel handle
 * @param belief Belief to introduce
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_introduce_belief(
    mesh_channel_t* channel,
    const mesh_belief_t* belief
);

/**
 * @brief Execute gossip round
 *
 * WHAT: Propagate beliefs through channel
 * WHY:  Gossip enables distributed consensus
 *
 * @param channel Channel handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_gossip_round(mesh_channel_t* channel);

/**
 * @brief Get consensus beliefs from channel
 *
 * @param channel Channel handle
 * @param beliefs_out Output array (caller allocates)
 * @param max_beliefs Maximum beliefs
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_get_consensus_beliefs(
    const mesh_channel_t* channel,
    mesh_belief_t* beliefs_out,
    size_t max_beliefs,
    size_t* count_out
);

/* ============================================================================
 * Update and Convergence API
 * ============================================================================ */

/**
 * @brief Update channel state
 *
 * WHAT: Perform periodic channel update
 * WHY:  Process pending operations, run gossip, check convergence
 *
 * @param channel Channel handle
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_update(
    mesh_channel_t* channel,
    uint64_t delta_ms
);

/**
 * @brief Check if channel has converged
 *
 * WHAT: Test if channel reached FEP convergence
 * WHY:  Convergence indicates consensus reached
 *
 * @param channel Channel handle
 * @return true if converged (ΔF < threshold)
 */
bool mesh_channel_has_converged(const mesh_channel_t* channel);

/**
 * @brief Get channel free energy
 *
 * @param channel Channel handle
 * @return Current free energy
 */
float mesh_channel_get_free_energy(const mesh_channel_t* channel);

/**
 * @brief Get channel convergence progress
 *
 * @param channel Channel handle
 * @return Convergence progress [0,1] where 1 = converged
 */
float mesh_channel_get_convergence_progress(const mesh_channel_t* channel);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get channel statistics
 *
 * @param channel Channel handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_get_stats(
    const mesh_channel_t* channel,
    mesh_channel_stats_t* stats
);

/**
 * @brief Reset channel statistics
 *
 * @param channel Channel handle
 */
void mesh_channel_reset_stats(mesh_channel_t* channel);

/* ============================================================================
 * Channel Manager API
 * ============================================================================ */

/**
 * @brief Channel manager configuration
 */
typedef struct mesh_channel_manager_config {
    size_t max_channels;                     /**< Maximum channels */
    bool enable_logging;                     /**< Enable logging */
} mesh_channel_manager_config_t;

/**
 * @brief Create channel manager
 *
 * WHAT: Manager for multiple channels
 * WHY:  Centralized channel lifecycle management
 *
 * @param config Configuration
 * @param registry Participant registry
 * @return Manager handle or NULL
 */
mesh_channel_manager_t* mesh_channel_manager_create(
    const mesh_channel_manager_config_t* config,
    mesh_participant_registry_t* registry
);

/**
 * @brief Destroy channel manager
 *
 * @param manager Manager to destroy (NULL-safe)
 */
void mesh_channel_manager_destroy(mesh_channel_manager_t* manager);

/**
 * @brief Create channel via manager
 *
 * @param manager Channel manager
 * @param config Channel configuration
 * @return Channel handle or NULL
 */
mesh_channel_t* mesh_channel_manager_create_channel(
    mesh_channel_manager_t* manager,
    const mesh_channel_config_t* config
);

/**
 * @brief Get channel by ID
 *
 * @param manager Channel manager
 * @param channel_id Channel ID
 * @return Channel handle or NULL
 */
mesh_channel_t* mesh_channel_manager_get_channel(
    mesh_channel_manager_t* manager,
    mesh_channel_id_t channel_id
);

/**
 * @brief Get channel by name
 *
 * @param manager Channel manager
 * @param name Channel name
 * @return Channel handle or NULL
 */
mesh_channel_t* mesh_channel_manager_get_channel_by_name(
    mesh_channel_manager_t* manager,
    const char* name
);

/**
 * @brief Update all channels
 *
 * @param manager Channel manager
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_manager_update(
    mesh_channel_manager_t* manager,
    uint64_t delta_ms
);

/**
 * @brief Create well-known channels
 *
 * WHAT: Create standard brain region channels
 * WHY:  Initialize Left/Right Hemisphere, Subcortical, GPU channels
 *
 * @param manager Channel manager
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_channel_manager_create_standard_channels(
    mesh_channel_manager_t* manager
);

/* ============================================================================
 * BBB Integration API
 * ============================================================================ */

/**
 * Forward declaration for BBB system
 */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Set BBB system for mesh channel validation
 *
 * WHAT: Configure BBB for belief validation
 * WHY:  Prevent malicious data injection via beliefs
 *
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_channel_set_bbb(bbb_system_t bbb);

/**
 * @brief Get current BBB system for mesh channel
 *
 * @return BBB system or NULL
 */
bbb_system_t mesh_channel_get_bbb(void);

/**
 * @brief Set health agent for mesh channel heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void mesh_channel_set_health_agent(nimcp_health_agent_t* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Print channel info for debugging
 *
 * @param channel Channel handle
 */
void mesh_channel_print_info(const mesh_channel_t* channel);

/**
 * @brief Print channel manager status
 *
 * @param manager Channel manager
 */
void mesh_channel_manager_print_status(const mesh_channel_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_CHANNEL_H */
