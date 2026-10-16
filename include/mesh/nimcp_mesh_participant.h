/**
 * @file nimcp_mesh_participant.h
 * @brief Mesh Network Participant Interface
 *
 * WHAT: Interface every NIMCP module implements to participate in mesh network
 * WHY:  Unified participation enabling endorsement, gossip, and consensus
 * HOW:  Callback-based interface with credential management and state accessors
 *
 * BIOLOGICAL INSPIRATION:
 * Like neurons in a brain network, each participant:
 * - Has an identity (credential)
 * - Receives and propagates beliefs (gossip)
 * - Participates in collective decisions (endorsement)
 * - Maintains local state that contributes to global cognition
 *
 * INTEGRATION:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      MESH PARTICIPANT INTERFACE                         │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Module (any NIMCP component)                                           │
 * │     │                                                                   │
 * │     ├── mesh_participant_interface_t                                    │
 * │     │      ├── Identity: id, type, credential                          │
 * │     │      ├── Callbacks: on_proposal, on_endorse, on_commit           │
 * │     │      ├── Gossip: on_belief_received, on_consensus_reached        │
 * │     │      └── State: get_free_energy, get_beliefs, get_health         │
 * │     │                                                                   │
 * │     └── Registered with mesh_participant_registry_t                     │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_PARTICIPANT_H
#define NIMCP_MESH_PARTICIPANT_H

#include "mesh/nimcp_mesh_types.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_transaction mesh_transaction_t;
typedef struct mesh_participant_registry mesh_participant_registry_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum channel memberships per participant */
#define MESH_MAX_CHANNEL_MEMBERSHIPS    8

/** @brief Default participant capacity in registry */
#define MESH_DEFAULT_REGISTRY_CAPACITY  256

/* ============================================================================
 * Participant Interface
 * ============================================================================ */

/**
 * @brief Mesh participant interface
 *
 * WHAT: Complete interface for mesh participation
 * WHY:  Every module that participates in mesh implements this
 * HOW:  Callback functions + state accessors
 *
 * IMPLEMENTATION NOTES:
 * - All callbacks are optional (NULL = not supported)
 * - user_context is passed to all callbacks
 * - Thread safety is the participant's responsibility
 */
typedef struct mesh_participant_interface {
    /* ---- Identity & Membership ---- */

    /** Module name (human-readable) */
    char module_name[MESH_MAX_NAME_LEN];

    /** Unique participant ID */
    mesh_participant_id_t id;

    /** Participant type */
    mesh_participant_type_t type;

    /** Home channel */
    mesh_channel_id_t home_channel;

    /** MSP-issued credential */
    credential_t* credential;

    /** Additional channel memberships */
    mesh_channel_id_t channel_memberships[MESH_MAX_CHANNEL_MEMBERSHIPS];
    size_t channel_membership_count;

    /* ---- Transaction Callbacks ---- */

    /**
     * @brief Called when a transaction is proposed
     *
     * @param ctx User context
     * @param tx Transaction being proposed
     * @return NIMCP_SUCCESS to accept, error to reject
     */
    nimcp_error_t (*on_proposal)(void* ctx, const mesh_transaction_t* tx);

    /**
     * @brief Called when endorsement is requested
     *
     * @param ctx User context
     * @param tx Transaction to endorse
     * @param endorsement Output: endorsement to fill
     * @return NIMCP_SUCCESS if endorsement provided
     */
    nimcp_error_t (*on_endorse_request)(
        void* ctx,
        const mesh_transaction_t* tx,
        mesh_endorsement_t* endorsement
    );

    /**
     * @brief Called when transaction is committed
     *
     * @param ctx User context
     * @param tx Committed transaction
     * @return NIMCP_SUCCESS on successful processing
     */
    nimcp_error_t (*on_commit)(void* ctx, const mesh_transaction_t* tx);

    /* ---- Gossip Callbacks ---- */

    /**
     * @brief Called when a belief is received via gossip
     *
     * @param ctx User context
     * @param belief Received belief
     * @return NIMCP_SUCCESS if belief accepted
     */
    nimcp_error_t (*on_belief_received)(void* ctx, const mesh_belief_t* belief);

    /**
     * @brief Called when channel reaches consensus
     *
     * @param ctx User context
     * @param consensus Consensus result
     * @return NIMCP_SUCCESS if processed
     */
    nimcp_error_t (*on_consensus_reached)(void* ctx, const mesh_consensus_t* consensus);

    /* ---- State Accessors ---- */

    /**
     * @brief Get current free energy (FEP)
     *
     * @param ctx User context
     * @return Free energy value (lower = better)
     */
    float (*get_free_energy)(void* ctx);

    /**
     * @brief Get current beliefs
     *
     * @param ctx User context
     * @param beliefs Output: belief set to populate
     */
    void (*get_beliefs)(void* ctx, mesh_belief_set_t* beliefs);

    /**
     * @brief Get world state contribution
     *
     * @param ctx User context
     * @param delta Output: state delta to populate
     */
    void (*get_world_state_contribution)(void* ctx, world_state_delta_t* delta);

    /**
     * @brief Get health metrics
     *
     * @param ctx User context
     * @param metrics Output: health metrics to populate
     */
    void (*get_health_metrics)(void* ctx, health_metrics_t* metrics);

    /* ---- GPU Capability (Optional) ---- */

    /** Whether participant has GPU acceleration */
    bool has_gpu_acceleration;

    /**
     * @brief Process transaction on GPU
     *
     * @param ctx User context
     * @param tx Transaction to process
     * @param result Output: processing result
     * @return NIMCP_SUCCESS if processed
     */
    nimcp_error_t (*gpu_process)(
        void* ctx,
        const mesh_transaction_t* tx,
        mesh_result_t* result
    );

    /* ---- Context ---- */

    /** User-provided context passed to all callbacks */
    void* user_context;

} mesh_participant_interface_t;

/* ============================================================================
 * Participant Registration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for participant registration
 */
typedef struct mesh_participant_config {
    /** Module name */
    const char* module_name;

    /** Participant type */
    mesh_participant_type_t type;

    /** Home channel */
    mesh_channel_id_t home_channel;

    /** User context for callbacks */
    void* user_context;

    /** Request GPU acceleration support */
    bool request_gpu;

} mesh_participant_config_t;

/* ============================================================================
 * Participant Registry
 * ============================================================================ */

/**
 * @brief Registry configuration
 */
typedef struct mesh_registry_config {
    size_t initial_capacity;            /**< Initial participant capacity */
    bool enable_logging;                /**< Enable registry logging */
} mesh_registry_config_t;

/**
 * @brief Participant registry statistics
 */
typedef struct mesh_registry_stats {
    size_t total_participants;          /**< Total registered */
    size_t active_participants;         /**< Currently active */
    size_t coordinators;                /**< Coordinator participants */
    size_t by_channel[MESH_MAX_CHANNELS]; /**< Count per channel */
    uint64_t registrations;             /**< Total registrations */
    uint64_t unregistrations;           /**< Total unregistrations */
} mesh_registry_stats_t;

/* ============================================================================
 * Registry Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default registry configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_registry_default_config(mesh_registry_config_t* config);

/**
 * @brief Create participant registry
 *
 * WHAT: Allocate and initialize participant registry
 * WHY:  Central repository of all mesh participants
 *
 * @param config Configuration (NULL for defaults)
 * @return Registry handle or NULL on failure
 */
mesh_participant_registry_t* mesh_registry_create(
    const mesh_registry_config_t* config
);

/**
 * @brief Destroy participant registry
 *
 * @param registry Registry to destroy (NULL-safe)
 */
void mesh_registry_destroy(mesh_participant_registry_t* registry);

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

/**
 * @brief Register a participant in the mesh
 *
 * WHAT: Add participant to registry, assign ID
 * WHY:  Participants must register to participate in mesh
 *
 * @param registry Participant registry
 * @param interface Participant interface (copied)
 * @param config Registration configuration
 * @param id_out Output: assigned participant ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_register(
    mesh_participant_registry_t* registry,
    const mesh_participant_interface_t* interface,
    const mesh_participant_config_t* config,
    mesh_participant_id_t* id_out
);

/**
 * @brief Unregister a participant
 *
 * @param registry Participant registry
 * @param id Participant to unregister
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_unregister(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/**
 * @brief Get participant interface by ID
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @return Participant interface or NULL if not found
 */
const mesh_participant_interface_t* mesh_participant_get(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/**
 * @brief Get participant interface by name
 *
 * @param registry Participant registry
 * @param name Module name
 * @return Participant interface or NULL if not found
 */
const mesh_participant_interface_t* mesh_participant_get_by_name(
    mesh_participant_registry_t* registry,
    const char* name
);

/**
 * @brief Check if participant is registered
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @return true if registered
 */
bool mesh_participant_is_registered(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/* ============================================================================
 * Channel Membership API
 * ============================================================================ */

/**
 * @brief Add participant to channel
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param channel Channel to join
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_join_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
);

/**
 * @brief Remove participant from channel
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param channel Channel to leave
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_leave_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
);

/**
 * @brief Check if participant is in channel
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param channel Channel to check
 * @return true if member
 */
bool mesh_participant_is_in_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
);

/**
 * @brief Get all participants in a channel
 *
 * @param registry Participant registry
 * @param channel Channel ID
 * @param ids_out Output: array of participant IDs (caller allocates)
 * @param max_ids Maximum IDs to return
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_get_channel_members(
    mesh_participant_registry_t* registry,
    mesh_channel_id_t channel,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
);

/* ============================================================================
 * Credential Management API
 * ============================================================================ */

/**
 * @brief Set participant credential
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param credential Credential to set (copied)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_set_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const credential_t* credential
);

/**
 * @brief Get participant credential
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @return Credential or NULL if not found
 */
const credential_t* mesh_participant_get_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/**
 * @brief Validate participant credential
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @return true if credential is valid
 */
bool mesh_participant_validate_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/**
 * @brief Suspend participant credential
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param reason Suspension reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_suspend_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const char* reason
);

/**
 * @brief Revoke participant credential
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param reason Revocation reason
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_revoke_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const char* reason
);

/* ============================================================================
 * Callback Invocation API
 * ============================================================================ */

/**
 * @brief Invoke on_proposal callback for participant
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param tx Transaction being proposed
 * @return Callback result
 */
nimcp_error_t mesh_participant_invoke_on_proposal(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx
);

/**
 * @brief Invoke on_endorse_request callback
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param tx Transaction to endorse
 * @param endorsement Output: endorsement
 * @return Callback result
 */
nimcp_error_t mesh_participant_invoke_on_endorse(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
);

/**
 * @brief Invoke on_commit callback
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param tx Committed transaction
 * @return Callback result
 */
nimcp_error_t mesh_participant_invoke_on_commit(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx
);

/**
 * @brief Invoke on_belief_received callback
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param belief Received belief
 * @return Callback result
 */
nimcp_error_t mesh_participant_invoke_on_belief(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_belief_t* belief
);

/**
 * @brief Invoke on_consensus_reached callback
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param consensus Consensus result
 * @return Callback result
 */
nimcp_error_t mesh_participant_invoke_on_consensus(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_consensus_t* consensus
);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get participant's free energy
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @return Free energy or -1.0 if not available
 */
float mesh_participant_get_free_energy(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
);

/**
 * @brief Get participant's beliefs
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param beliefs Output: belief set
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_get_beliefs(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_belief_set_t* beliefs
);

/**
 * @brief Get participant's health metrics
 *
 * @param registry Participant registry
 * @param id Participant ID
 * @param metrics Output: health metrics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_participant_get_health(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    health_metrics_t* metrics
);

/* ============================================================================
 * Statistics and Iteration API
 * ============================================================================ */

/**
 * @brief Get registry statistics
 *
 * @param registry Participant registry
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_registry_get_stats(
    mesh_participant_registry_t* registry,
    mesh_registry_stats_t* stats
);

/**
 * @brief Iterate over all participants
 *
 * @param registry Participant registry
 * @param callback Called for each participant
 * @param user_ctx Context passed to callback
 * @return NIMCP_SUCCESS on success
 *
 * Callback signature: bool (*)(const mesh_participant_interface_t*, void*)
 * Return false from callback to stop iteration.
 */
nimcp_error_t mesh_registry_iterate(
    mesh_participant_registry_t* registry,
    bool (*callback)(const mesh_participant_interface_t*, void*),
    void* user_ctx
);

/**
 * @brief Iterate over participants in a channel
 *
 * @param registry Participant registry
 * @param channel Channel to iterate
 * @param callback Called for each participant
 * @param user_ctx Context passed to callback
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_registry_iterate_channel(
    mesh_participant_registry_t* registry,
    mesh_channel_id_t channel,
    bool (*callback)(const mesh_participant_interface_t*, void*),
    void* user_ctx
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
 * @brief Set BBB system for mesh participant validation
 *
 * WHAT: Configure BBB for registration validation
 * WHY:  Prevent malicious module registration
 *
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_participant_set_bbb(bbb_system_t bbb);

/**
 * @brief Get current BBB system for mesh participant
 *
 * @return BBB system or NULL
 */
bbb_system_t mesh_participant_get_bbb(void);

/**
 * @brief Set health agent for mesh participant heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void mesh_participant_set_health_agent(nimcp_health_agent_t* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Initialize participant interface with defaults
 *
 * @param interface Interface to initialize
 */
void mesh_participant_interface_init(mesh_participant_interface_t* interface);

/**
 * @brief Initialize participant config with defaults
 *
 * @param config Config to initialize
 */
void mesh_participant_config_init(mesh_participant_config_t* config);

/**
 * @brief Print participant info for debugging
 *
 * @param interface Participant interface
 */
void mesh_participant_print_info(const mesh_participant_interface_t* interface);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_PARTICIPANT_H */
