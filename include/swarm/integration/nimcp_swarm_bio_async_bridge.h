/**
 * @file nimcp_swarm_bio_async_bridge.h
 * @brief Bio-Async Integration Bridge for Swarm Intelligence Coordination
 *
 * WHAT: Registers swarm modules with bio-async for biological-inspired messaging
 * WHY:  Enable swarm agents to communicate via neuromodulator-based async patterns
 * HOW:  Message types for agent state, consensus, pheromones, stigmergy coordination
 *
 * BIOLOGICAL BASIS:
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                     SWARM BIO-ASYNC INTEGRATION                             │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                             │
 * │  ┌──────────┐     BIO_CHANNEL_DOPAMINE      ┌──────────┐                   │
 * │  │ Agent A  │ ─────── Consensus ──────────▶ │ Agent B  │                   │
 * │  └──────────┘                               └──────────┘                   │
 * │       │                                           │                         │
 * │       │ BIO_CHANNEL_ACETYLCHOLINE                 │                         │
 * │       │ (Fast state sync)                         │                         │
 * │       ▼                                           ▼                         │
 * │  ┌──────────────────────────────────────────────────┐                      │
 * │  │              PHEROMONE FIELD                      │                      │
 * │  │    BIO_CHANNEL_SEROTONIN (Stigmergy trails)      │                      │
 * │  └──────────────────────────────────────────────────┘                      │
 * │                                                                             │
 * │  CHANNEL SEMANTICS FOR SWARM:                                               │
 * │  • DOPAMINE:     Consensus achievement, goal completion signals             │
 * │  • SEROTONIN:    Slow stigmergic coordination, pheromone persistence        │
 * │  • NOREPINEPHRINE: Threat alerts, priority escalation, conflict detection   │
 * │  • ACETYLCHOLINE: Fast agent state sync, attention coordination             │
 * │                                                                             │
 * └─────────────────────────────────────────────────────────────────────────────┘
 *
 * USAGE EXAMPLE:
 * @code
 * // Create swarm bio-async bridge
 * swarm_bio_bridge_config_t config;
 * swarm_bio_bridge_default_config(&config);
 * config.max_agents = 64;
 * config.enable_stigmergy = true;
 *
 * swarm_bio_bridge_t* bridge = swarm_bio_bridge_create(&config);
 * if (!bridge) { return; }
 *
 * // Register an agent
 * swarm_agent_id_t agent_id = swarm_bio_bridge_register_agent(bridge, "agent_1");
 *
 * // Broadcast pheromone via bio-async
 * swarm_pheromone_msg_t pheromone = {
 *     .type = SWARM_PHEROMONE_RESOURCE,
 *     .position = { 10.0f, 20.0f, 0.0f },
 *     .intensity = 0.8f
 * };
 * swarm_bio_bridge_broadcast_pheromone(bridge, agent_id, &pheromone);
 *
 * // Request consensus
 * swarm_bio_bridge_request_consensus(bridge, SWARM_CONSENSUS_FORMATION);
 *
 * // Cleanup
 * swarm_bio_bridge_destroy(bridge);
 * @endcode
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_BIO_ASYNC_BRIDGE_H
#define NIMCP_SWARM_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS AND LIMITS
 *============================================================================*/

#define SWARM_BIO_BRIDGE_MAX_AGENTS          256  /**< Maximum agents per bridge */
#define SWARM_BIO_BRIDGE_MAX_MSG_SIZE        4096 /**< Maximum message payload */
#define SWARM_BIO_BRIDGE_MAX_SUBSCRIPTIONS   32   /**< Max subscriptions per agent */
#define SWARM_BIO_BRIDGE_AGENT_NAME_MAX      64   /**< Maximum agent name length */

/*=============================================================================
 * BIO-MODULE IDs FOR SWARM INTEGRATION
 *============================================================================*/

/** @brief Bio-async module ID for swarm bio-async bridge */
#define BIO_MODULE_SWARM_BIO_ASYNC_BRIDGE    0x0B30

/** @brief Bio-async module ID for swarm agent coordinator */
#define BIO_MODULE_SWARM_AGENT_COORDINATOR   0x0B31

/** @brief Bio-async module ID for swarm stigmergy system */
#define BIO_MODULE_SWARM_STIGMERGY           0x0B32

/*=============================================================================
 * ERROR CODES
 *============================================================================*/

/** @brief Error code base for swarm bio-async bridge */
#define SWARM_BIO_ERROR_BASE                 15000

/** @brief Bridge not initialized */
#define SWARM_BIO_ERROR_NOT_INITIALIZED      (SWARM_BIO_ERROR_BASE + 1)

/** @brief Agent registration failed */
#define SWARM_BIO_ERROR_AGENT_REGISTRATION   (SWARM_BIO_ERROR_BASE + 2)

/** @brief Agent not found */
#define SWARM_BIO_ERROR_AGENT_NOT_FOUND      (SWARM_BIO_ERROR_BASE + 3)

/** @brief Maximum agents reached */
#define SWARM_BIO_ERROR_MAX_AGENTS           (SWARM_BIO_ERROR_BASE + 4)

/** @brief Message send failed */
#define SWARM_BIO_ERROR_SEND_FAILED          (SWARM_BIO_ERROR_BASE + 5)

/** @brief Invalid message type */
#define SWARM_BIO_ERROR_INVALID_MSG_TYPE     (SWARM_BIO_ERROR_BASE + 6)

/** @brief Consensus timeout */
#define SWARM_BIO_ERROR_CONSENSUS_TIMEOUT    (SWARM_BIO_ERROR_BASE + 7)

/** @brief Pheromone broadcast failed */
#define SWARM_BIO_ERROR_PHEROMONE_FAILED     (SWARM_BIO_ERROR_BASE + 8)

/** @brief Bio-async not connected */
#define SWARM_BIO_ERROR_NOT_CONNECTED        (SWARM_BIO_ERROR_BASE + 9)

/*=============================================================================
 * SWARM MESSAGE TYPES (Extend BIO_MSG_SWARM_* range 0x0B00)
 *============================================================================*/

/**
 * @brief Swarm bio-async specific message types
 *
 * These extend the base swarm message types for bio-async integration
 */
typedef enum {
    /** Agent state synchronization messages */
    SWARM_MSG_AGENT_STATE = 0x0BC0,          /**< Agent state broadcast */
    SWARM_MSG_AGENT_HEARTBEAT,               /**< Agent heartbeat/presence */
    SWARM_MSG_AGENT_CAPABILITY,              /**< Agent capability announcement */
    SWARM_MSG_AGENT_JOINED,                  /**< Agent joined swarm */
    SWARM_MSG_AGENT_LEFT,                    /**< Agent left swarm */
    SWARM_MSG_AGENT_POSITION,                /**< Agent position update */

    /** Consensus coordination messages */
    SWARM_MSG_CONSENSUS_REQUEST = 0x0BD0,    /**< Request consensus on topic */
    SWARM_MSG_CONSENSUS_VOTE,                /**< Cast consensus vote */
    SWARM_MSG_CONSENSUS_RESULT,              /**< Consensus result broadcast */
    SWARM_MSG_CONSENSUS_ABORT,               /**< Abort consensus process */
    SWARM_MSG_CONSENSUS_QUORUM_CHECK,        /**< Check quorum status */

    /** Pheromone/stigmergy messages */
    SWARM_MSG_PHEROMONE = 0x0BE0,            /**< Pheromone deposit/update */
    SWARM_MSG_PHEROMONE_GRADIENT,            /**< Pheromone gradient query/response */
    SWARM_MSG_PHEROMONE_DECAY,               /**< Pheromone decay notification */
    SWARM_MSG_STIGMERGY_MARK,                /**< Place stigmergic marker */
    SWARM_MSG_STIGMERGY_QUERY,               /**< Query stigmergic environment */

    /** Coordination messages */
    SWARM_MSG_COORDINATION_REQUEST = 0x0BF0, /**< Request coordination action */
    SWARM_MSG_COORDINATION_ACK,              /**< Acknowledge coordination */
    SWARM_MSG_FORMATION_UPDATE,              /**< Formation update */
    SWARM_MSG_LEADER_ANNOUNCE,               /**< Leader announcement */
    SWARM_MSG_TASK_DELEGATION,               /**< Task delegation to agent */

    SWARM_MSG_TYPE_COUNT                     /**< Total message type count */
} swarm_bio_msg_type_t;

/*=============================================================================
 * CONSENSUS TOPIC TYPES
 *============================================================================*/

/**
 * @brief Types of consensus topics for swarm decision-making
 */
typedef enum {
    SWARM_CONSENSUS_FORMATION = 0,           /**< Formation change consensus */
    SWARM_CONSENSUS_TARGET,                  /**< Target selection consensus */
    SWARM_CONSENSUS_RETREAT,                 /**< Retreat decision consensus */
    SWARM_CONSENSUS_RESOURCE,                /**< Resource allocation consensus */
    SWARM_CONSENSUS_LEADER,                  /**< Leader election consensus */
    SWARM_CONSENSUS_TASK,                    /**< Task assignment consensus */
    SWARM_CONSENSUS_EMERGENCY,               /**< Emergency response consensus */
    SWARM_CONSENSUS_CUSTOM,                  /**< Custom topic consensus */
    SWARM_CONSENSUS_TYPE_COUNT
} swarm_consensus_topic_t;

/*=============================================================================
 * PHEROMONE TYPES FOR STIGMERGY
 *============================================================================*/

/**
 * @brief Pheromone types for stigmergic communication
 */
typedef enum {
    SWARM_PHEROMONE_PATH = 0,                /**< Path/trail marker */
    SWARM_PHEROMONE_RESOURCE,                /**< Resource location marker */
    SWARM_PHEROMONE_DANGER,                  /**< Danger/threat warning */
    SWARM_PHEROMONE_RALLY,                   /**< Rally/gathering point */
    SWARM_PHEROMONE_TERRITORY,               /**< Territory boundary marker */
    SWARM_PHEROMONE_SIGNAL,                  /**< General signal marker */
    SWARM_PHEROMONE_TYPE_COUNT
} swarm_bio_pheromone_type_t;

/*=============================================================================
 * AGENT STATE
 *============================================================================*/

/**
 * @brief Agent operational states
 */
typedef enum {
    SWARM_AGENT_STATE_IDLE = 0,              /**< Agent idle/ready */
    SWARM_AGENT_STATE_ACTIVE,                /**< Agent actively processing */
    SWARM_AGENT_STATE_BUSY,                  /**< Agent busy with task */
    SWARM_AGENT_STATE_COORDINATING,          /**< Agent in coordination */
    SWARM_AGENT_STATE_WAITING,               /**< Agent waiting for response */
    SWARM_AGENT_STATE_ERROR,                 /**< Agent in error state */
    SWARM_AGENT_STATE_DISCONNECTED           /**< Agent disconnected */
} swarm_agent_state_t;

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct swarm_bio_bridge_struct swarm_bio_bridge_t;
typedef uint32_t swarm_agent_id_t;

/*=============================================================================
 * MESSAGE STRUCTURES
 *============================================================================*/

/**
 * @brief 3D position for swarm agents
 */
typedef struct {
    float x;                                 /**< X coordinate */
    float y;                                 /**< Y coordinate */
    float z;                                 /**< Z coordinate */
} swarm_bio_position_t;

/**
 * @brief Agent state message payload
 */
typedef struct {
    swarm_agent_id_t agent_id;               /**< Source agent ID */
    swarm_agent_state_t state;               /**< Current agent state */
    swarm_bio_position_t position;           /**< Agent position */
    float energy_level;                      /**< Agent energy [0.0-1.0] */
    float confidence;                        /**< Agent confidence [0.0-1.0] */
    uint64_t timestamp;                      /**< Message timestamp */
    uint32_t sequence;                       /**< Sequence number */
} swarm_agent_state_msg_t;

/**
 * @brief Consensus request/vote message
 */
typedef struct {
    uint32_t consensus_id;                   /**< Unique consensus session ID */
    swarm_consensus_topic_t topic;           /**< Consensus topic type */
    swarm_agent_id_t initiator;              /**< Agent that initiated consensus */
    float confidence;                        /**< Vote confidence [0.0-1.0] */
    uint32_t option;                         /**< Vote option/choice */
    uint64_t deadline_ms;                    /**< Consensus deadline */
    uint8_t payload[256];                    /**< Optional custom payload */
    size_t payload_size;                     /**< Payload size in bytes */
} swarm_consensus_msg_t;

/**
 * @brief Pheromone message for stigmergy
 */
typedef struct {
    swarm_bio_pheromone_type_t type;         /**< Pheromone type */
    swarm_bio_position_t position;           /**< Deposit position */
    float intensity;                         /**< Pheromone intensity [0.0-1.0] */
    float decay_rate;                        /**< Decay rate per second */
    swarm_agent_id_t depositor;              /**< Agent that deposited */
    uint64_t timestamp;                      /**< Deposit timestamp */
    float radius;                            /**< Effect radius */
} swarm_pheromone_msg_t;

/**
 * @brief Coordination message
 */
typedef struct {
    uint32_t coordination_id;                /**< Coordination session ID */
    swarm_agent_id_t target_agent;           /**< Target agent (0 = broadcast) */
    uint32_t action_type;                    /**< Action type code */
    swarm_bio_position_t target_position;    /**< Target position if applicable */
    float priority;                          /**< Action priority [0.0-1.0] */
    uint8_t data[128];                       /**< Custom action data */
    size_t data_size;                        /**< Data size */
} swarm_coordination_msg_t;

/*=============================================================================
 * CALLBACK TYPES
 *============================================================================*/

/**
 * @brief Callback for agent state updates
 *
 * @param bridge Bridge handle
 * @param msg Agent state message
 * @param user_data User context
 */
typedef void (*swarm_agent_state_callback_t)(
    swarm_bio_bridge_t* bridge,
    const swarm_agent_state_msg_t* msg,
    void* user_data
);

/**
 * @brief Callback for consensus events
 *
 * @param bridge Bridge handle
 * @param msg Consensus message
 * @param user_data User context
 */
typedef void (*swarm_consensus_callback_t)(
    swarm_bio_bridge_t* bridge,
    const swarm_consensus_msg_t* msg,
    void* user_data
);

/**
 * @brief Callback for pheromone events
 *
 * @param bridge Bridge handle
 * @param msg Pheromone message
 * @param user_data User context
 */
typedef void (*swarm_pheromone_callback_t)(
    swarm_bio_bridge_t* bridge,
    const swarm_pheromone_msg_t* msg,
    void* user_data
);

/**
 * @brief Callback for coordination events
 *
 * @param bridge Bridge handle
 * @param msg Coordination message
 * @param user_data User context
 */
typedef void (*swarm_coordination_callback_t)(
    swarm_bio_bridge_t* bridge,
    const swarm_coordination_msg_t* msg,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Configuration for swarm bio-async bridge
 */
typedef struct {
    /* Capacity settings */
    uint32_t max_agents;                     /**< Maximum agents (default: 64) */
    uint32_t inbox_capacity;                 /**< Bio-async inbox size */

    /* Bio-async channel preferences */
    nimcp_bio_channel_type_t consensus_channel;   /**< Channel for consensus (default: DOPAMINE) */
    nimcp_bio_channel_type_t state_channel;       /**< Channel for state sync (default: ACETYLCHOLINE) */
    nimcp_bio_channel_type_t pheromone_channel;   /**< Channel for pheromones (default: SEROTONIN) */
    nimcp_bio_channel_type_t alert_channel;       /**< Channel for alerts (default: NOREPINEPHRINE) */

    /* Timing settings */
    uint32_t heartbeat_interval_ms;          /**< Agent heartbeat interval (default: 1000) */
    uint32_t consensus_timeout_ms;           /**< Consensus timeout (default: 5000) */
    uint32_t pheromone_broadcast_interval_ms; /**< Pheromone update interval (default: 500) */

    /* Feature flags */
    bool enable_stigmergy;                   /**< Enable stigmergic communication */
    bool enable_consensus;                   /**< Enable consensus coordination */
    bool enable_heartbeat;                   /**< Enable automatic heartbeats */
    bool enable_logging;                     /**< Enable debug logging */
    bool auto_connect_bio_async;             /**< Auto-connect to bio-async router */

    /* Consensus settings */
    float quorum_threshold;                  /**< Quorum threshold [0.5-1.0] (default: 0.66) */
    float consensus_confidence_weight;       /**< Confidence weighting (default: 0.8) */
} swarm_bio_bridge_config_t;

/*=============================================================================
 * AGENT INFO
 *============================================================================*/

/**
 * @brief Registered agent information
 */
typedef struct {
    swarm_agent_id_t id;                     /**< Agent ID */
    char name[SWARM_BIO_BRIDGE_AGENT_NAME_MAX]; /**< Agent name */
    swarm_agent_state_t state;               /**< Current state */
    swarm_bio_position_t position;           /**< Last known position */
    float energy;                            /**< Current energy level */
    uint64_t last_heartbeat;                 /**< Last heartbeat timestamp */
    bool is_local;                           /**< Is this a local agent */
    void* user_data;                         /**< Agent-specific user data */
} swarm_agent_info_t;

/*=============================================================================
 * STATISTICS
 *============================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t state_messages_sent;            /**< Agent state messages sent */
    uint64_t state_messages_received;        /**< Agent state messages received */
    uint64_t consensus_initiated;            /**< Consensus sessions initiated */
    uint64_t consensus_completed;            /**< Consensus sessions completed */
    uint64_t consensus_failed;               /**< Consensus sessions failed */
    uint64_t pheromone_broadcasts;           /**< Pheromone broadcasts sent */
    uint64_t pheromone_received;             /**< Pheromone messages received */
    uint64_t coordination_messages;          /**< Coordination messages total */

    /* Agent tracking */
    uint32_t active_agents;                  /**< Currently active agents */
    uint32_t total_agents_registered;        /**< Total agents ever registered */

    /* Timing stats */
    float avg_consensus_time_ms;             /**< Average consensus duration */
    float avg_message_latency_us;            /**< Average message latency */
    uint64_t last_activity_time;             /**< Last activity timestamp */

    /* Health metrics */
    uint64_t heartbeats_missed;              /**< Missed heartbeats */
    uint64_t bio_async_errors;               /**< Bio-async errors */
} swarm_bio_bridge_stats_t;

/*=============================================================================
 * CONSENSUS RESULT
 *============================================================================*/

/**
 * @brief Result of a consensus session
 */
typedef struct {
    uint32_t consensus_id;                   /**< Consensus session ID */
    swarm_consensus_topic_t topic;           /**< Topic that was voted on */
    bool achieved;                           /**< Whether consensus was achieved */
    uint32_t winning_option;                 /**< Winning option (if achieved) */
    float confidence;                        /**< Aggregate confidence [0.0-1.0] */
    uint32_t votes_for;                      /**< Votes for winning option */
    uint32_t votes_against;                  /**< Votes against */
    uint32_t votes_total;                    /**< Total votes cast */
    uint64_t duration_ms;                    /**< Time to reach consensus */
} swarm_consensus_result_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *============================================================================*/

/**
 * @brief Swarm bio-async bridge main structure
 */
struct swarm_bio_bridge_struct {
    bridge_base_t base;                      /**< Base bridge (MUST be first) */

    /* Configuration */
    swarm_bio_bridge_config_t config;        /**< Bridge configuration */

    /* Agent management */
    swarm_agent_info_t* agents;              /**< Registered agents array */
    uint32_t agent_count;                    /**< Current agent count */
    swarm_agent_id_t next_agent_id;          /**< Next agent ID to assign */

    /* Active consensus */
    swarm_consensus_msg_t* active_consensus; /**< Currently active consensus */
    uint32_t* consensus_votes;               /**< Vote tallies */
    bool consensus_in_progress;              /**< Consensus is ongoing */

    /* Callbacks */
    swarm_agent_state_callback_t on_agent_state;
    swarm_consensus_callback_t on_consensus;
    swarm_pheromone_callback_t on_pheromone;
    swarm_coordination_callback_t on_coordination;
    void* callback_user_data;                /**< User data for callbacks */

    /* Statistics */
    swarm_bio_bridge_stats_t stats;          /**< Bridge statistics */

    /* State */
    bool initialized;                        /**< Initialization flag */
};

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns configuration with sensible defaults
 * WHY:  Simplify bridge creation
 * HOW:  Sets biologically-inspired defaults
 *
 * @param config Output configuration structure
 */
void swarm_bio_bridge_default_config(swarm_bio_bridge_config_t* config);

/**
 * @brief Create swarm bio-async bridge
 *
 * WHAT: Allocates and initializes a swarm bio-async bridge
 * WHY:  Enable swarm coordination via bio-async messaging
 * HOW:  Allocates memory, initializes state, optionally connects to router
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * THREAD SAFETY: Must be called from single thread
 */
swarm_bio_bridge_t* swarm_bio_bridge_create(const swarm_bio_bridge_config_t* config);

/**
 * @brief Destroy swarm bio-async bridge
 *
 * WHAT: Cleans up and frees bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnects bio-async, frees memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void swarm_bio_bridge_destroy(swarm_bio_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_reset(swarm_bio_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC CONNECTION
 *============================================================================*/

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_connect_bio_async(swarm_bio_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_disconnect_bio_async(swarm_bio_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool swarm_bio_bridge_is_bio_async_connected(const swarm_bio_bridge_t* bridge);

/*=============================================================================
 * AGENT MANAGEMENT
 *============================================================================*/

/**
 * @brief Register a local agent with the bridge
 *
 * WHAT: Registers a new agent for coordination
 * WHY:  Track agents participating in swarm
 * HOW:  Assigns ID, stores info, broadcasts join message
 *
 * @param bridge Bridge handle
 * @param name Agent name (max 63 chars)
 * @return Agent ID or 0 on failure
 */
swarm_agent_id_t swarm_bio_bridge_register_agent(
    swarm_bio_bridge_t* bridge,
    const char* name
);

/**
 * @brief Register agent with custom user data
 *
 * @param bridge Bridge handle
 * @param name Agent name
 * @param user_data Agent-specific user data
 * @return Agent ID or 0 on failure
 */
swarm_agent_id_t swarm_bio_bridge_register_agent_ex(
    swarm_bio_bridge_t* bridge,
    const char* name,
    void* user_data
);

/**
 * @brief Unregister an agent
 *
 * @param bridge Bridge handle
 * @param agent_id Agent to unregister
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_unregister_agent(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
);

/**
 * @brief Get agent information
 *
 * @param bridge Bridge handle
 * @param agent_id Agent ID
 * @param out_info Output agent info
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_get_agent_info(
    const swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    swarm_agent_info_t* out_info
);

/**
 * @brief Update agent state
 *
 * @param bridge Bridge handle
 * @param agent_id Agent ID
 * @param state New state
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_update_agent_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    swarm_agent_state_t state
);

/**
 * @brief Update agent position
 *
 * @param bridge Bridge handle
 * @param agent_id Agent ID
 * @param position New position
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_update_agent_position(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id,
    const swarm_bio_position_t* position
);

/**
 * @brief Get number of active agents
 *
 * @param bridge Bridge handle
 * @return Number of active agents
 */
uint32_t swarm_bio_bridge_get_agent_count(const swarm_bio_bridge_t* bridge);

/*=============================================================================
 * AGENT STATE MESSAGING
 *============================================================================*/

/**
 * @brief Broadcast agent state to swarm
 *
 * WHAT: Sends agent state via bio-async
 * WHY:  Synchronize agent states across swarm
 * HOW:  Uses ACETYLCHOLINE channel for fast propagation
 *
 * @param bridge Bridge handle
 * @param agent_id Agent ID
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_broadcast_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
);

/**
 * @brief Send heartbeat for agent
 *
 * @param bridge Bridge handle
 * @param agent_id Agent ID
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_send_heartbeat(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t agent_id
);

/*=============================================================================
 * CONSENSUS COORDINATION
 *============================================================================*/

/**
 * @brief Initiate consensus on a topic
 *
 * WHAT: Start a consensus session among swarm agents
 * WHY:  Collective decision-making
 * HOW:  Broadcasts request via DOPAMINE channel
 *
 * @param bridge Bridge handle
 * @param topic Consensus topic
 * @param initiator Initiating agent ID
 * @param options Number of vote options
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @return Consensus session ID or 0 on failure
 */
uint32_t swarm_bio_bridge_initiate_consensus(
    swarm_bio_bridge_t* bridge,
    swarm_consensus_topic_t topic,
    swarm_agent_id_t initiator,
    uint32_t options,
    uint32_t timeout_ms
);

/**
 * @brief Cast vote in active consensus
 *
 * @param bridge Bridge handle
 * @param consensus_id Consensus session ID
 * @param agent_id Voting agent ID
 * @param option Vote option
 * @param confidence Vote confidence [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_cast_vote(
    swarm_bio_bridge_t* bridge,
    uint32_t consensus_id,
    swarm_agent_id_t agent_id,
    uint32_t option,
    float confidence
);

/**
 * @brief Get consensus result
 *
 * @param bridge Bridge handle
 * @param consensus_id Consensus session ID
 * @param out_result Output result structure
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_get_consensus_result(
    const swarm_bio_bridge_t* bridge,
    uint32_t consensus_id,
    swarm_consensus_result_t* out_result
);

/**
 * @brief Abort active consensus
 *
 * @param bridge Bridge handle
 * @param consensus_id Consensus session ID
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_abort_consensus(
    swarm_bio_bridge_t* bridge,
    uint32_t consensus_id
);

/*=============================================================================
 * PHEROMONE/STIGMERGY
 *============================================================================*/

/**
 * @brief Broadcast pheromone deposit
 *
 * WHAT: Broadcast pheromone to swarm via bio-async
 * WHY:  Stigmergic indirect coordination
 * HOW:  Uses SEROTONIN channel for slow, persistent signaling
 *
 * @param bridge Bridge handle
 * @param depositor Agent depositing pheromone
 * @param pheromone Pheromone message
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_broadcast_pheromone(
    swarm_bio_bridge_t* bridge,
    swarm_agent_id_t depositor,
    const swarm_pheromone_msg_t* pheromone
);

/**
 * @brief Query pheromone gradient at position
 *
 * @param bridge Bridge handle
 * @param position Query position
 * @param type Pheromone type to query
 * @param out_direction Output gradient direction
 * @param out_intensity Output intensity
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_query_pheromone_gradient(
    swarm_bio_bridge_t* bridge,
    const swarm_bio_position_t* position,
    swarm_bio_pheromone_type_t type,
    swarm_bio_position_t* out_direction,
    float* out_intensity
);

/*=============================================================================
 * COORDINATION
 *============================================================================*/

/**
 * @brief Send coordination request
 *
 * @param bridge Bridge handle
 * @param msg Coordination message
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_send_coordination(
    swarm_bio_bridge_t* bridge,
    const swarm_coordination_msg_t* msg
);

/**
 * @brief Broadcast alert to swarm
 *
 * WHAT: Send high-priority alert to all agents
 * WHY:  Emergency/threat notification
 * HOW:  Uses NOREPINEPHRINE channel for priority
 *
 * @param bridge Bridge handle
 * @param alert_type Alert type code
 * @param position Position of alert (optional)
 * @param intensity Alert intensity [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_broadcast_alert(
    swarm_bio_bridge_t* bridge,
    uint32_t alert_type,
    const swarm_bio_position_t* position,
    float intensity
);

/*=============================================================================
 * CALLBACK REGISTRATION
 *============================================================================*/

/**
 * @brief Register agent state callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_on_agent_state(
    swarm_bio_bridge_t* bridge,
    swarm_agent_state_callback_t callback,
    void* user_data
);

/**
 * @brief Register consensus callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_on_consensus(
    swarm_bio_bridge_t* bridge,
    swarm_consensus_callback_t callback,
    void* user_data
);

/**
 * @brief Register pheromone callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_on_pheromone(
    swarm_bio_bridge_t* bridge,
    swarm_pheromone_callback_t callback,
    void* user_data
);

/**
 * @brief Register coordination callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_on_coordination(
    swarm_bio_bridge_t* bridge,
    swarm_coordination_callback_t callback,
    void* user_data
);

/*=============================================================================
 * UPDATE AND PROCESSING
 *============================================================================*/

/**
 * @brief Process incoming bio-async messages
 *
 * WHAT: Process pending messages from bio-async router
 * WHY:  Handle incoming swarm communications
 * HOW:  Polls inbox, dispatches to handlers
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int swarm_bio_bridge_process_messages(
    swarm_bio_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update tick
 * WHY:  Handle timeouts, heartbeats, cleanup
 * HOW:  Check consensus timeouts, send heartbeats, prune stale agents
 *
 * @param bridge Bridge handle
 * @param dt_ms Delta time since last update
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_update(
    swarm_bio_bridge_t* bridge,
    uint32_t dt_ms
);

/*=============================================================================
 * STATISTICS AND DEBUG
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param out_stats Output statistics
 * @return 0 on success, error code on failure
 */
int swarm_bio_bridge_get_stats(
    const swarm_bio_bridge_t* bridge,
    swarm_bio_bridge_stats_t* out_stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 */
void swarm_bio_bridge_reset_stats(swarm_bio_bridge_t* bridge);

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return String name
 */
const char* swarm_bio_msg_type_name(swarm_bio_msg_type_t msg_type);

/**
 * @brief Get consensus topic name string
 *
 * @param topic Consensus topic
 * @return String name
 */
const char* swarm_consensus_topic_name(swarm_consensus_topic_t topic);

/**
 * @brief Get pheromone type name string
 *
 * @param type Pheromone type
 * @return String name
 */
const char* swarm_bio_pheromone_type_name(swarm_bio_pheromone_type_t type);

/**
 * @brief Get agent state name string
 *
 * @param state Agent state
 * @return String name
 */
const char* swarm_agent_state_name(swarm_agent_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_BIO_ASYNC_BRIDGE_H */
