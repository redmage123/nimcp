/**
 * @file nimcp_rcog_collective_bridge.h
 * @brief Collective Consciousness/Swarm Integration Bridge for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting recursive cognition with swarm/collective systems
 * WHY:  Distribute recursive processing across swarm members for scalability
 * HOW:  Full bridge pattern with CRDT-based state merging and stigmergy
 *
 * BIOLOGICAL BASIS:
 * Collective intelligence emerges from distributed processing:
 * - Stigmergy: Indirect communication through shared environment (like ants)
 * - Swarm consensus: Byzantine-tolerant agreement on answers
 * - Load distribution: Share processing across available drones
 * - Collective memory: Shared context across swarm members
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * | RECURSIVE COGNITION  |                    |    SWARM SYSTEMS     |
 * |                      |                    |                      |
 * | - Context Store      |<-- stigmergy ----->| - Collective WS      |
 * |   (shared vars)      |    (pheromones)    |   (CRDT state)       |
 * | - Delegation Pool    |                    | - Swarm Conscious    |
 * |   (broadcast tasks)  |<-- volunteer ----->|   (IIT coherence)    |
 * | - Answer Refiner     |    subtasks        | - Consensus Engine   |
 * |   (collective)       |                    |   (Byzantine vote)   |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (distributed processing)
 * ```
 *
 * SWARM MESSAGE TYPES (0x1406xx):
 * - SUBTASK_BROADCAST: Broadcast subtask to swarm
 * - SUBTASK_VOLUNTEER: Volunteer to process subtask
 * - RESULT_SHARE: Share result with swarm
 * - CONTEXT_SHARE: Share context variable
 * - ANSWER_STATE_SYNC: Sync answer state (CRDT)
 */

#ifndef NIMCP_RCOG_COLLECTIVE_BRIDGE_H
#define NIMCP_RCOG_COLLECTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_engine;
struct rcog_context_store;
struct rcog_delegation_pool;
struct rcog_answer_refiner;
struct rcog_subtask;
struct rcog_answer_state;
struct collective_workspace;
struct swarm_consciousness;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum swarm members to coordinate with */
#define RCOG_COLLECTIVE_MAX_SWARM_MEMBERS       64

/** Maximum shared context variables */
#define RCOG_COLLECTIVE_MAX_SHARED_VARS         16

/** Default broadcast salience threshold */
#define RCOG_COLLECTIVE_DEFAULT_BROADCAST_THRESHOLD 0.6f

/** Default consensus coherence threshold */
#define RCOG_COLLECTIVE_DEFAULT_CONSENSUS_THRESHOLD 0.8f

/** Maximum concurrent collective subtasks */
#define RCOG_COLLECTIVE_MAX_CONCURRENT_SUBTASKS 16

/*=============================================================================
 * MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Collective-specific bio-async message types
 */
typedef enum {
    RCOG_MSG_SUBTASK_BROADCAST       = 0x140601,  /**< Broadcast subtask to swarm */
    RCOG_MSG_SUBTASK_VOLUNTEER       = 0x140602,  /**< Volunteer to process */
    RCOG_MSG_SUBTASK_RESULT_SHARE    = 0x140603,  /**< Share result with swarm */
    RCOG_MSG_CONTEXT_SHARE           = 0x140604,  /**< Share context variable */
    RCOG_MSG_ANSWER_STATE_SYNC       = 0x140605,  /**< Sync answer state (CRDT) */
    RCOG_MSG_DEPTH_LIMIT_WARNING     = 0x140606,  /**< Warn swarm of depth limit */
    RCOG_MSG_COLLECTIVE_READY        = 0x140607,  /**< Answer ready signal */
    RCOG_MSG_VOLUNTEER_ACK           = 0x140608,  /**< Acknowledge volunteer */
    RCOG_MSG_HANDOFF_REQUEST         = 0x140609,  /**< Request task handoff */
    RCOG_MSG_HANDOFF_ACCEPT          = 0x14060A   /**< Accept task handoff */
} rcog_collective_message_type_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Information about a swarm member
 */
typedef struct {
    uint16_t drone_id;               /**< Drone identifier */
    float capacity;                  /**< Available capacity [0.0-1.0] */
    float coherence;                 /**< Coherence with swarm [0.0-1.0] */
    uint32_t active_subtasks;        /**< Currently processing subtasks */
    uint64_t last_seen_ms;           /**< Last communication timestamp */
    bool is_available;               /**< Available for delegation */
} rcog_swarm_member_info_t;

/**
 * @brief Shared context variable info
 */
typedef struct {
    char name[64];                   /**< Variable name */
    uint16_t source_drone;           /**< Drone that shared it */
    float salience;                  /**< Importance level */
    uint64_t shared_at_ms;           /**< When it was shared */
    size_t size;                     /**< Size in bytes */
    bool is_local;                   /**< Whether we have local copy */
} rcog_shared_variable_info_t;

/**
 * @brief Collective subtask tracking
 */
typedef struct {
    uint64_t subtask_id;             /**< Subtask identifier */
    uint16_t assigned_drone;         /**< Drone processing it (0 = unassigned) */
    uint16_t volunteers[4];          /**< Drones that volunteered */
    uint8_t num_volunteers;          /**< Number of volunteers */
    float completion_progress;       /**< Progress [0.0-1.0] */
    bool result_received;            /**< Whether result was received */
} rcog_collective_subtask_t;

/**
 * @brief Effects flowing from recursive cognition to collective
 *
 * WHAT: Subtask broadcasts, context sharing, and answer synchronization
 * WHY:  Distribute work and share state across swarm
 */
typedef struct {
    /* Subtask broadcasting */
    bool broadcast_subtask;          /**< Broadcast a subtask */
    uint64_t subtask_to_broadcast;   /**< Subtask ID to broadcast */
    float subtask_priority;          /**< Priority of subtask */
    float required_capacity;         /**< Required capacity to process */

    /* Context sharing (stigmergy) */
    bool share_context_variable;     /**< Share a context variable */
    const char* variable_to_share;   /**< Variable name to share */
    float variable_salience;         /**< Salience for sharing */

    /* Answer synchronization */
    bool sync_answer_state;          /**< Sync answer state with swarm */
    float current_confidence;        /**< Current answer confidence */
    uint32_t refinement_step;        /**< Current refinement step */

    /* Handoff request */
    bool request_handoff;            /**< Request task handoff to another drone */
    uint16_t preferred_target;       /**< Preferred target drone (0 = any) */
    const char* handoff_reason;      /**< Reason for handoff */

    /* Status */
    float local_load;                /**< Local processing load [0.0-1.0] */
    uint32_t active_local_subtasks;  /**< Active local subtasks */
} rcog_to_collective_effects_t;

/**
 * @brief Effects flowing from collective to recursive cognition
 *
 * WHAT: Volunteer offers, shared results, and consensus status
 * WHY:  Coordinate distributed processing
 */
typedef struct {
    /* Swarm status */
    uint16_t swarm_size;             /**< Total swarm members */
    uint16_t available_members;      /**< Members available for work */
    float swarm_coherence;           /**< Overall swarm coherence [0.0-1.0] */
    float swarm_load;                /**< Average swarm load [0.0-1.0] */

    /* Volunteer offers */
    bool has_volunteers;             /**< Volunteers available */
    uint8_t num_volunteers;          /**< Number of volunteers */
    uint16_t volunteer_drones[8];    /**< Volunteer drone IDs */
    float volunteer_capacities[8];   /**< Volunteer capacities */

    /* Shared results */
    bool result_received;            /**< Result received from swarm */
    uint64_t result_subtask_id;      /**< Subtask ID of result */
    uint16_t result_source_drone;    /**< Drone that provided result */
    float result_confidence;         /**< Confidence of result */

    /* Context imports available */
    uint32_t num_shared_variables;   /**< Number of shared variables available */
    rcog_shared_variable_info_t shared_vars[RCOG_COLLECTIVE_MAX_SHARED_VARS];

    /* Consensus status */
    bool consensus_reached;          /**< Swarm consensus on answer */
    float consensus_confidence;      /**< Confidence of consensus */
    uint32_t agreeing_drones;        /**< Number agreeing on answer */

    /* Handoff status */
    bool handoff_accepted;           /**< Handoff was accepted */
    uint16_t handoff_drone;          /**< Drone that accepted handoff */
} collective_to_rcog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Collective bridge configuration
 */
typedef struct {
    /* Identity */
    uint16_t local_drone_id;         /**< This drone's ID */

    /* Broadcasting */
    float broadcast_threshold;       /**< Salience threshold to broadcast */
    bool auto_broadcast_failures;    /**< Auto-broadcast on failure */
    uint32_t broadcast_timeout_ms;   /**< Timeout for broadcast responses */

    /* Volunteering */
    bool enable_volunteering;        /**< Enable volunteering for subtasks */
    float volunteer_threshold;       /**< Load threshold to volunteer */
    uint32_t max_volunteered_tasks;  /**< Max tasks to volunteer for */

    /* Context sharing (stigmergy) */
    bool enable_stigmergy;           /**< Enable pheromone-like sharing */
    float stigmergy_decay_rate;      /**< Decay rate of shared salience */
    bool auto_import_high_salience;  /**< Auto-import high salience vars */

    /* Consensus */
    float consensus_threshold;       /**< Coherence for consensus */
    uint32_t min_consensus_drones;   /**< Minimum drones for consensus */
} rcog_collective_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Collective bridge opaque handle
 */
typedef struct rcog_collective_bridge rcog_collective_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create collective bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
rcog_collective_bridge_t* rcog_collective_bridge_create(
    const rcog_collective_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
rcog_collective_bridge_t* rcog_collective_bridge_create_default(void);

/**
 * @brief Destroy collective bridge
 * @param bridge Bridge handle (NULL safe)
 */
void rcog_collective_bridge_destroy(rcog_collective_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
rcog_collective_bridge_config_t rcog_collective_bridge_default_config(void);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to collective workspace
 * @param bridge Bridge handle
 * @param workspace Collective workspace handle
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_connect_workspace(
    rcog_collective_bridge_t* bridge,
    struct collective_workspace* workspace
);

/**
 * @brief Connect bridge to swarm consciousness
 * @param bridge Bridge handle
 * @param consciousness Swarm consciousness handle
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_connect_consciousness(
    rcog_collective_bridge_t* bridge,
    struct swarm_consciousness* consciousness
);

/**
 * @brief Connect bridge to recursive cognition engine
 * @param bridge Bridge handle
 * @param engine Recursive cognition engine handle
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_connect_engine(
    rcog_collective_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected to all systems
 */
bool rcog_collective_bridge_is_connected(const rcog_collective_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_update(
    rcog_collective_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * SUBTASK DISTRIBUTION
 *===========================================================================*/

/**
 * @brief Broadcast subtask to swarm
 * @param bridge Bridge handle
 * @param subtask Subtask to broadcast
 * @param handle Output collective handle for tracking
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_broadcast_subtask(
    rcog_collective_bridge_t* bridge,
    const struct rcog_subtask* subtask,
    rcog_collective_handle_t** handle
);

/**
 * @brief Collect results from swarm members
 * @param bridge Bridge handle
 * @param handle Collective handle
 * @param results Output array of results
 * @param max_results Maximum results to collect
 * @param num_results Output number of results
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_collect_results(
    rcog_collective_bridge_t* bridge,
    rcog_collective_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t max_results,
    size_t* num_results
);

/**
 * @brief Accept volunteered subtask from swarm
 * @param bridge Bridge handle
 * @param subtask_id Subtask to accept
 * @param source_drone Drone that broadcasted
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_volunteer_for_subtask(
    rcog_collective_bridge_t* bridge,
    uint64_t subtask_id,
    uint16_t source_drone
);

/*=============================================================================
 * CONTEXT SHARING (STIGMERGY)
 *===========================================================================*/

/**
 * @brief Share context variable with swarm
 * @param bridge Bridge handle
 * @param variable_name Variable to share
 * @param salience Importance level
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_share_context(
    rcog_collective_bridge_t* bridge,
    const char* variable_name,
    float salience
);

/**
 * @brief Import shared context from swarm member
 * @param bridge Bridge handle
 * @param source_drone Source drone ID
 * @param variable_name Variable to import
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_import_context(
    rcog_collective_bridge_t* bridge,
    uint16_t source_drone,
    const char* variable_name
);

/**
 * @brief Get list of available shared variables
 * @param bridge Bridge handle
 * @param vars Output array of variable info
 * @param max_vars Maximum to return
 * @param num_vars Output number of variables
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_list_shared_context(
    const rcog_collective_bridge_t* bridge,
    rcog_shared_variable_info_t* vars,
    size_t max_vars,
    size_t* num_vars
);

/*=============================================================================
 * ANSWER CONSENSUS
 *===========================================================================*/

/**
 * @brief Distributed answer refinement across swarm
 * @param bridge Bridge handle
 * @param state Answer state to refine collectively
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_refine_answer(
    rcog_collective_bridge_t* bridge,
    struct rcog_answer_state* state
);

/**
 * @brief Check if swarm has reached consensus on answer
 * @param bridge Bridge handle
 * @param state Answer state to check
 * @param coherence_threshold Required coherence
 * @return true if consensus reached
 */
bool rcog_collective_bridge_consensus_reached(
    const rcog_collective_bridge_t* bridge,
    const struct rcog_answer_state* state,
    float coherence_threshold
);

/**
 * @brief Get consensus confidence level
 * @param bridge Bridge handle
 * @return Consensus confidence [0.0-1.0]
 */
float rcog_collective_bridge_get_consensus_confidence(
    const rcog_collective_bridge_t* bridge
);

/*=============================================================================
 * SWARM STATUS
 *===========================================================================*/

/**
 * @brief Get information about swarm members
 * @param bridge Bridge handle
 * @param members Output array of member info
 * @param max_members Maximum to return
 * @param num_members Output number of members
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_get_swarm_members(
    const rcog_collective_bridge_t* bridge,
    rcog_swarm_member_info_t* members,
    size_t max_members,
    size_t* num_members
);

/**
 * @brief Get overall swarm coherence
 * @param bridge Bridge handle
 * @return Swarm coherence [0.0-1.0]
 */
float rcog_collective_bridge_get_swarm_coherence(
    const rcog_collective_bridge_t* bridge
);

/**
 * @brief Get number of active collective subtasks
 * @param bridge Bridge handle
 * @return Number of active collective subtasks
 */
uint32_t rcog_collective_bridge_get_active_subtasks(
    const rcog_collective_bridge_t* bridge
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from rcog to collective
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_get_outgoing_effects(
    const rcog_collective_bridge_t* bridge,
    rcog_to_collective_effects_t* effects
);

/**
 * @brief Get current effects from collective to rcog
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_get_incoming_effects(
    const rcog_collective_bridge_t* bridge,
    collective_to_rcog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t subtasks_broadcast;
    uint64_t subtasks_volunteered;
    uint64_t results_shared;
    uint64_t context_vars_shared;
    uint64_t context_vars_imported;
    uint64_t consensus_reached_count;
    float avg_consensus_time_ms;
    float avg_swarm_coherence;
    uint64_t handoffs_requested;
    uint64_t handoffs_accepted;
} rcog_collective_bridge_stats_t;

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_collective_bridge_get_stats(
    const rcog_collective_bridge_t* bridge,
    rcog_collective_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void rcog_collective_bridge_reset_stats(rcog_collective_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_COLLECTIVE_BRIDGE_H */
