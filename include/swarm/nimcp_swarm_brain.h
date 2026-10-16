/**
 * @file nimcp_swarm_brain.h
 * @brief NIMCP Swarm Brain Coordinator - Main Integration Point
 *
 * WHAT: Main coordinator that integrates all swarm components into a cohesive
 *       swarm intelligence system for drone/robot collectives.
 *
 * WHY:  Enables distributed cognitive processing across multiple physical agents,
 *       creating emergent swarm behaviors from individual constrained brains.
 *
 * HOW:  Integrates:
 *       - Local constrained NIMCP brain (resource-limited processing)
 *       - Swarm signal adapter (radio communication abstraction)
 *       - Collective workspace (shared attention/goals)
 *       - Emergence detection (tier tracking)
 *       - Consensus mechanisms (voting, decision-making)
 *       - Neuromodulator synchronization (emotional state sharing)
 *       - Bio-async message passing (internal coordination)
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │             NIMCP Swarm Brain Coordinator                   │
 *   │                                                             │
 *   │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────┐   │
 *   │  │ Local Brain │  │   Collective  │  │   Consensus     │   │
 *   │  │ (Constrained│  │   Workspace   │  │   Engine        │   │
 *   │  │   NIMCP)    │  │ (Shared Attn) │  │ (Voting/Quorum) │   │
 *   │  └──────┬──────┘  └───────┬──────┘  └────────┬────────┘   │
 *   │         │                 │                   │            │
 *   │         └─────────┬───────┴───────────────────┘            │
 *   │                   │                                        │
 *   │         ┌─────────▼──────────┐                            │
 *   │         │  Bio-Async Router  │                            │
 *   │         └─────────┬──────────┘                            │
 *   │                   │                                        │
 *   │         ┌─────────▼──────────┐                            │
 *   │         │  Signal Adapter    │                            │
 *   │         │    (Radio I/O)     │                            │
 *   │         └─────────┬──────────┘                            │
 *   └───────────────────┼────────────────────────────────────────┘
 *                       │
 *                 ┌─────▼──────┐
 *                 │   Radio    │
 *                 │  (LoRa/    │
 *                 │   WiFi/    │
 *                 │   etc.)    │
 *                 └────────────┘
 *
 * MESSAGE TYPES:
 * - HEARTBEAT: Periodic presence announcement
 * - PERCEPTION: Shared sensor observations
 * - THREAT: Urgent warning broadcasts
 * - VOTE_PROPOSE: Start consensus vote
 * - VOTE_CAST: Individual vote submission
 * - NEUROMOD_SYNC: Chemical state synchronization
 * - WORKSPACE_UPDATE: Collective attention update
 *
 * EMERGENCE TIERS:
 * - TIER_0_DISCONNECTED: No peers, solo operation
 * - TIER_1_PAIRED: 2-3 drones, basic coordination
 * - TIER_2_CLUSTER: 4-7 drones, group behaviors
 * - TIER_3_SWARM: 8+ drones, emergent intelligence
 * - TIER_4_SUPERORGANISM: 16+ drones with high coherence
 *
 * USAGE EXAMPLE:
 * ```c
 * // Configure and create swarm brain
 * swarm_brain_config_t config = {
 *     .drone_id = 1,
 *     .swarm_name = "alpha_squadron",
 *     .heartbeat_ms = 100,
 *     .coherence_threshold = 0.5,
 *     .enable_bio_async = true
 * };
 * swarm_brain_t* swarm = swarm_brain_create(&config);
 *
 * // Join swarm network
 * swarm_brain_join(swarm);
 *
 * // Main loop
 * while (running) {
 *     swarm_brain_process(swarm);  // Handle messages, votes, sync
 *
 *     // Share perception
 *     perception_data_t data = {...};
 *     swarm_brain_broadcast_perception(swarm, &data);
 *
 *     // Check emergence level
 *     swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(swarm);
 *
 *     sleep_ms(10);
 * }
 *
 * swarm_brain_leave(swarm);
 * swarm_brain_destroy(swarm);
 * ```
 *
 * THREAD SAFETY:
 * - All public APIs are thread-safe
 * - Internal state protected by mutex
 * - Bio-async inbox provides lock-free message passing
 *
 * PERFORMANCE:
 * - Heartbeat overhead: ~100 bytes/100ms = 1 KB/s
 * - Message latency: Radio RTT + processing (typically <50ms)
 * - Vote consensus: O(N) where N = peer count
 * - Memory footprint: ~10-20KB per swarm brain instance
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_BRAIN_H
#define NIMCP_SWARM_BRAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "swarm/nimcp_swarm_emergence.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Import from existing headers
struct brain_struct;                    // Forward declare brain_t
typedef struct brain_struct* brain_t;   // brain_t type
struct nimcp_swarm_signal_adapter;      // From nimcp_swarm_signal.h
struct nimcp_bio_async_module_ctx;      // Bio-async module context

//=============================================================================
// Constants
//=============================================================================

/** Maximum swarm name length */
#define SWARM_MAX_NAME_LEN 32

/** Maximum number of tracked peers */
#define SWARM_MAX_PEERS 32

/** Maximum message size for swarm communication (max supported by signal adapter) */
#define SWARM_MAX_MESSAGE_SIZE 255

/** Default heartbeat interval (ms) */
#define SWARM_DEFAULT_HEARTBEAT_MS 100

/** Default sync interval (ms) */
#define SWARM_DEFAULT_SYNC_MS 50

/** Default vote timeout (ms) */
#define SWARM_DEFAULT_VOTE_TIMEOUT_MS 100

/** Default coherence threshold */
#define SWARM_DEFAULT_COHERENCE_THRESHOLD 0.5f

/** Default critical mass for swarm emergence */
#define SWARM_DEFAULT_CRITICAL_MASS 8

/** Default workspace size */
#define SWARM_DEFAULT_WORKSPACE_SIZE 32

/** Default broadcast threshold */
#define SWARM_DEFAULT_BROADCAST_THRESHOLD 0.7f

/** Default neuromodulator diffusion rate */
#define SWARM_DEFAULT_NEUROMOD_DIFFUSION 0.3f

//=============================================================================
// Enumerations
//=============================================================================

// swarm_emergence_tier_t is now defined in nimcp_swarm_emergence.h

/**
 * @brief Swarm message types
 * Note: If swarm_protocol.h is included, use its enum instead
 */
#ifndef SWARM_MESSAGE_TYPE_DEFINED
#define SWARM_MESSAGE_TYPE_DEFINED
typedef enum {
    SWARM_MSG_HEARTBEAT = 0x01,       /**< Periodic presence announcement */
    SWARM_MSG_PERCEPTION = 0x02,      /**< Shared sensor observation */
    SWARM_MSG_THREAT = 0x03,          /**< Urgent warning broadcast */
    SWARM_MSG_VOTE_PROPOSE = 0x04,    /**< Start consensus vote */
    SWARM_MSG_VOTE_CAST = 0x05,       /**< Individual vote submission */
    SWARM_MSG_NEUROMOD_SYNC = 0x06,   /**< Chemical state sync */
    SWARM_MSG_WORKSPACE_UPDATE = 0x07,/**< Collective attention update */
    SWARM_MSG_GOODBYE = 0x08          /**< Graceful disconnect */
} swarm_message_type_t;
#endif

/**
 * @brief Vote decision types
 */
typedef enum {
    VOTE_ABSTAIN = 0,                 /**< No preference */
    VOTE_APPROVE = 1,                 /**< Support proposal */
    VOTE_REJECT = 2                   /**< Oppose proposal */
} vote_decision_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Swarm brain configuration
 */
typedef struct {
    uint16_t drone_id;                /**< Unique drone identifier */
    char swarm_name[SWARM_MAX_NAME_LEN]; /**< Swarm network name */
    uint32_t heartbeat_ms;            /**< Heartbeat interval (default: 100ms) */
    uint32_t sync_ms;                 /**< Sync interval (default: 50ms) */
    uint32_t vote_timeout_ms;         /**< Vote timeout (default: 100ms) */
    float coherence_threshold;        /**< Coherence threshold (default: 0.5) */
    uint32_t critical_mass;           /**< Swarm critical mass (default: 8) */
    uint32_t workspace_size;          /**< Workspace size (default: 32) */
    float broadcast_threshold;        /**< Broadcast threshold (default: 0.7) */
    float neuromod_diffusion;         /**< Neuromod diffusion rate (default: 0.3) */
    bool enable_reward_sharing;       /**< Enable reward sharing (default: true) */
    bool enable_bio_async;            /**< Enable bio-async (default: true) */
} swarm_brain_config_t;

/**
 * @brief Peer information
 */
typedef struct {
    uint16_t drone_id;                /**< Peer drone ID */
    uint64_t last_seen_ms;            /**< Last heartbeat timestamp */
    float coherence;                  /**< Coherence with this peer (0-1) */
    uint32_t message_count;           /**< Messages received from peer */
    bool active;                      /**< Peer is active */
} swarm_peer_info_t;

/**
 * @brief Collective workspace entry
 */
typedef struct {
    uint32_t concept_id;              /**< Shared concept identifier */
    float attention;                  /**< Collective attention weight (0-1) */
    uint32_t contributor_count;       /**< Number of drones attending */
    uint64_t last_update_ms;          /**< Last update timestamp */
} workspace_entry_t;

/**
 * @brief Perception data (sensor observations)
 */
typedef struct {
    uint32_t sensor_type;             /**< Sensor type ID */
    float values[8];                  /**< Sensor values */
    uint32_t value_count;             /**< Number of values */
    uint64_t timestamp_ms;            /**< Observation timestamp */
    float confidence;                 /**< Observation confidence (0-1) */
} perception_data_t;

/**
 * @brief Threat data (urgent warnings)
 */
typedef struct {
    uint32_t threat_type;             /**< Threat classification */
    float position[3];                /**< Threat position (x, y, z) */
    float severity;                   /**< Threat severity (0-1) */
    uint64_t timestamp_ms;            /**< Detection timestamp */
    char description[64];             /**< Human-readable description */
} threat_data_t;

/**
 * @brief Vote proposal
 */
typedef struct {
    uint32_t proposal_id;             /**< Unique proposal identifier */
    uint32_t action_type;             /**< Proposed action type */
    float parameters[8];              /**< Action parameters */
    uint32_t parameter_count;         /**< Number of parameters */
    uint16_t proposer_id;             /**< Proposing drone ID */
    uint64_t expiry_ms;               /**< Proposal expiry time */
} vote_proposal_t;

/**
 * @brief Neuromodulator state (for synchronization)
 */
typedef struct {
    float dopamine;                   /**< Dopamine level (0-1) */
    float serotonin;                  /**< Serotonin level (0-1) */
    float norepinephrine;             /**< Norepinephrine level (0-1) */
    float acetylcholine;              /**< Acetylcholine level (0-1) */
} neuromod_state_t;

/**
 * @brief Swarm statistics
 */
typedef struct {
    uint64_t messages_sent;           /**< Total messages sent */
    uint64_t messages_received;       /**< Total messages received */
    uint32_t peers_connected;         /**< Currently connected peers */
    uint32_t emergence_tier_changes;  /**< Tier transition count */
    uint32_t votes_completed;         /**< Completed consensus votes */
    double avg_latency_ms;            /**< Average message latency */
    float workspace_coherence;        /**< Current workspace coherence (0-1) */
    uint64_t uptime_ms;               /**< Time since creation */
} swarm_stats_t;

/**
 * @brief Opaque swarm brain handle
 */
typedef struct nimcp_swarm_brain swarm_brain_t;

//=============================================================================
// Core API Functions
//=============================================================================

/**
 * @brief Create a swarm brain coordinator
 *
 * Initializes all swarm components including local brain, signal adapter,
 * collective workspace, consensus engine, and bio-async context.
 *
 * @param config Configuration for the swarm brain
 * @return Pointer to swarm brain on success, NULL on failure
 */
swarm_brain_t* swarm_brain_create(const swarm_brain_config_t* config);

/**
 * @brief Destroy a swarm brain coordinator
 *
 * Gracefully disconnects from swarm, cleans up all resources.
 *
 * @param swarm Swarm brain to destroy
 */
void swarm_brain_destroy(swarm_brain_t* swarm);

/**
 * @brief Join swarm network
 *
 * Announces presence to swarm and begins listening for peers.
 *
 * @param swarm Swarm brain instance
 * @return true on success, false on failure
 */
bool swarm_brain_join(swarm_brain_t* swarm);

/**
 * @brief Leave swarm network gracefully
 *
 * Sends goodbye message and disconnects from peers.
 *
 * @param swarm Swarm brain instance
 * @return true on success, false on failure
 */
bool swarm_brain_leave(swarm_brain_t* swarm);

/**
 * @brief Process swarm brain (call frequently in main loop)
 *
 * Handles:
 * - Receiving and decoding radio messages
 * - Processing bio-async inbox
 * - Updating collective workspace
 * - Processing active votes
 * - Checking peer timeouts
 * - Sending periodic heartbeats
 * - Updating emergence tier
 *
 * @param swarm Swarm brain instance
 * @return true on success, false on error
 */
bool swarm_brain_process(swarm_brain_t* swarm);

//=============================================================================
// Communication API
//=============================================================================

/**
 * @brief Broadcast perception data to swarm
 *
 * Shares sensor observations with all peers in swarm.
 *
 * @param swarm Swarm brain instance
 * @param perception Perception data to broadcast
 * @return true on success, false on failure
 */
bool swarm_brain_broadcast_perception(
    swarm_brain_t* swarm,
    const perception_data_t* perception
);

/**
 * @brief Broadcast urgent threat warning
 *
 * Immediately broadcasts threat to all peers with high priority.
 *
 * @param swarm Swarm brain instance
 * @param threat Threat data to broadcast
 * @return true on success, false on failure
 */
bool swarm_brain_broadcast_threat(
    swarm_brain_t* swarm,
    const threat_data_t* threat
);

/**
 * @brief Propose action for consensus vote
 *
 * Initiates a vote on proposed action, waits for peer responses.
 *
 * @param swarm Swarm brain instance
 * @param proposal Vote proposal
 * @return true if vote started, false on failure
 */
bool swarm_brain_propose_action(
    swarm_brain_t* swarm,
    const vote_proposal_t* proposal
);

/**
 * @brief Synchronize neuromodulator state with swarm
 *
 * Broadcasts local neuromodulator levels and integrates peer states.
 *
 * @param swarm Swarm brain instance
 * @param local_state Local neuromodulator state
 * @return true on success, false on failure
 */
bool swarm_brain_sync_neuromodulators(
    swarm_brain_t* swarm,
    const neuromod_state_t* local_state
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current emergence tier
 *
 * @param swarm Swarm brain instance
 * @return Current emergence tier
 */
swarm_emergence_tier_t swarm_brain_get_emergence_tier(
    const swarm_brain_t* swarm
);

/**
 * @brief Get collective workspace
 *
 * Returns pointer to workspace entries (array of workspace_size elements).
 *
 * @param swarm Swarm brain instance
 * @param workspace_size Pointer to store workspace size
 * @return Pointer to workspace entries, NULL on error
 */
const workspace_entry_t* swarm_brain_get_workspace(
    const swarm_brain_t* swarm,
    uint32_t* workspace_size
);

/**
 * @brief Get connected peers
 *
 * Returns array of active peer information.
 *
 * @param swarm Swarm brain instance
 * @param peer_count Pointer to store peer count
 * @return Pointer to peer info array, NULL on error
 */
const swarm_peer_info_t* swarm_brain_get_peers(
    const swarm_brain_t* swarm,
    uint32_t* peer_count
);

/**
 * @brief Get swarm statistics
 *
 * @param swarm Swarm brain instance
 * @param stats Pointer to store statistics
 * @return true on success, false on failure
 */
bool swarm_brain_get_stats(
    const swarm_brain_t* swarm,
    swarm_stats_t* stats
);

/**
 * @brief Get local brain instance
 *
 * Provides access to underlying constrained NIMCP brain for
 * direct interaction if needed.
 *
 * @param swarm Swarm brain instance
 * @return Pointer to brain, NULL on error
 */
brain_t swarm_brain_get_local_brain(swarm_brain_t* swarm);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default swarm brain configuration
 *
 * Returns configuration with sensible defaults.
 *
 * @return Default configuration
 */
swarm_brain_config_t swarm_brain_default_config(void);

/**
 * @brief Get emergence tier name as string
 *
 * @param tier Emergence tier
 * @return String representation of tier
 */
const char* swarm_emergence_tier_string(swarm_emergence_tier_t tier);

/**
 * @brief Get message type name as string
 *
 * @param msg_type Message type
 * @return String representation of message type
 */
const char* swarm_message_type_string(swarm_message_type_t msg_type);

/**
 * @brief Check if swarm brain is operational
 *
 * @param swarm Swarm brain instance
 * @return true if operational, false otherwise
 */
bool swarm_brain_is_operational(const swarm_brain_t* swarm);

/**
 * @brief Reset swarm statistics
 *
 * @param swarm Swarm brain instance
 * @return true on success, false on failure
 */
bool swarm_brain_reset_stats(swarm_brain_t* swarm);

//=============================================================================
// Local Brain Instantiation API (Feature 1-4)
//=============================================================================

/**
 * @brief Brain configuration for local instances (swarm-specific)
 */
typedef struct {
    uint32_t neuron_count;            /**< Number of neurons in local brain */
    uint32_t synapse_count;           /**< Number of synapses */
    float learning_rate;              /**< Local learning rate */
    bool share_structure;             /**< Share common structures (true = memory efficient) */
    bool enable_local_learning;       /**< Enable local learning (false = inference only) */
} swarm_local_brain_config_t;

/**
 * @brief Brain synchronization configuration
 */
typedef struct {
    uint32_t* layer_indices;          /**< Array of layer indices to sync (NULL = all) */
    uint32_t layer_count;             /**< Number of layers to sync (0 = all) */
    float sync_threshold;             /**< Minimum weight change to sync (0 = always) */
    bool bidirectional;               /**< Bidirectional sync (false = one-way) */
} brain_sync_config_t;

/**
 * @brief Collective learning experience
 */
typedef struct {
    uint32_t agent_id;                /**< Agent that had experience */
    float* input_data;                /**< Input sensory data */
    uint32_t input_size;              /**< Size of input */
    float* target_output;             /**< Target/reward signal */
    uint32_t target_size;             /**< Size of target */
    float importance;                 /**< Experience importance weight (0-1) */
    uint64_t timestamp_ms;            /**< When experience occurred */
} learning_experience_t;

/**
 * @brief Brain migration checkpoint
 */
typedef struct {
    uint32_t checkpoint_size;         /**< Size of checkpoint data */
    uint8_t* checkpoint_data;         /**< Serialized brain state */
    uint32_t source_agent;            /**< Source agent ID */
    uint32_t target_agent;            /**< Target agent ID */
    uint64_t migration_time_ms;       /**< When migration started */
} brain_migration_checkpoint_t;

/**
 * @brief Feature 1: Create local brain instance for agent
 *
 * WHAT: Creates lightweight brain instance for each swarm agent
 * WHY:  Enable distributed cognition with local processing
 * HOW:  Shares common structures, maintains local state per agent
 *
 * @param swarm Swarm brain coordinator
 * @param agent_id Unique agent identifier
 * @param config Brain configuration
 * @return Local brain handle or NULL on failure
 */
brain_t swarm_brain_create_local(
    swarm_brain_t* swarm,
    uint16_t agent_id,
    const swarm_local_brain_config_t* config
);

/**
 * @brief Feature 2: Synchronize neural weights across agents
 *
 * WHAT: Sync neural weights from source to target agents
 * WHY:  Enable knowledge sharing and collective learning
 * HOW:  Partial or full weight transfer with configurable layers
 *
 * @param swarm Swarm brain coordinator
 * @param source_agent Source agent ID
 * @param target_agents Array of target agent IDs
 * @param target_count Number of target agents
 * @param sync_config Sync configuration (NULL = full sync)
 * @return true on success, false on failure
 */
bool swarm_brain_sync_weights(
    swarm_brain_t* swarm,
    uint16_t source_agent,
    const uint16_t* target_agents,
    uint32_t target_count,
    const brain_sync_config_t* sync_config
);

/**
 * @brief Feature 3: Collective learning from distributed experiences
 *
 * WHAT: Aggregate learning across all agents using federated approach
 * WHY:  Learn from collective experience without centralizing data
 * HOW:  Weight experiences by importance, apply federated averaging
 *
 * @param swarm Swarm brain coordinator
 * @param experiences Array of learning experiences
 * @param experience_count Number of experiences
 * @return true on success, false on failure
 */
bool swarm_brain_collective_learn(
    swarm_brain_t* swarm,
    const learning_experience_t* experiences,
    uint32_t experience_count
);

/**
 * @brief Feature 4: Migrate brain state to different host
 *
 * WHAT: Move brain state from one agent to another
 * WHY:  Enable agent hot-swapping and fault tolerance
 * HOW:  Checkpoint state, transfer, restore on new host
 *
 * @param swarm Swarm brain coordinator
 * @param agent_id Source agent ID
 * @param new_host Target agent ID for migration
 * @return Migration checkpoint on success, NULL on failure
 */
brain_migration_checkpoint_t* swarm_brain_migrate(
    swarm_brain_t* swarm,
    uint16_t agent_id,
    uint16_t new_host
);

/**
 * @brief Restore brain from migration checkpoint
 *
 * @param swarm Swarm brain coordinator
 * @param checkpoint Migration checkpoint
 * @return true on success, false on failure
 */
bool swarm_brain_restore_migration(
    swarm_brain_t* swarm,
    const brain_migration_checkpoint_t* checkpoint
);

/**
 * @brief Destroy migration checkpoint
 *
 * @param checkpoint Migration checkpoint to free
 */
void swarm_brain_migration_checkpoint_destroy(
    brain_migration_checkpoint_t* checkpoint
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_BRAIN_H */
